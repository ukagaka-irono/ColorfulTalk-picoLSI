// pico LSI のファームウェアを avr8js（ATmega328P, 16MHz）で動かす試験。
//   node sim.mjs pico-lsi.hex script.json
// script.json: [{ "send": "konnichiwa." }, { "wait": "prompt" }, { "send": "#X", "at": "busy" }, ...]
// UART に行を送り、返事（TX）、BUSY ピン（PD2）、PWM のデューティ（OCR2B）を記録して JSON で出す。
import fs from 'node:fs';
import { CPU, avrInstruction, AVRTimer, timer0Config, timer1Config, timer2Config, AVRUSART, usart0Config } from 'avr8js';

const [hexPath, scriptPath] = process.argv.slice(2);
const FREQ = 16e6;
const PORTD = 0x2b, OCR2B = 0xb4, GPIOR0 = 0x3e;

function loadHex(text) {
  const bytes = new Uint8Array(32768);
  for (const line of text.split(/\r?\n/)) {
    if (!line.startsWith(':')) continue;
    const n = parseInt(line.substr(1, 2), 16), addr = parseInt(line.substr(3, 4), 16), type = parseInt(line.substr(7, 2), 16);
    if (type === 0) for (let i = 0; i < n; i++) bytes[addr + i] = parseInt(line.substr(9 + 2 * i, 2), 16);
  }
  return new Uint16Array(bytes.buffer);
}

const cpu = new CPU(loadHex(fs.readFileSync(hexPath, 'utf8')), 2048);
new AVRTimer(cpu, timer0Config);
new AVRTimer(cpu, timer1Config);
new AVRTimer(cpu, timer2Config);
const usart = new AVRUSART(cpu, usart0Config, FREQ);

let tx = '';
usart.onByteTransmit = (b) => { tx += String.fromCharCode(b); };
let busy = false, underruns = 0, minSP = 0xffff;
const utterances = [];            // BUSY が High の間に書かれた OCR2B の値
const busyEdges = [];
cpu.writeHooks[PORTD] = (v) => {
  const b = (v & 4) !== 0;
  if (b !== busy) {
    busy = b;
    busyEdges.push({ busy: b, ms: cpu.cycles / FREQ * 1000 });
    if (b) utterances.push([]);
  }
  return false;
};
cpu.writeHooks[OCR2B] = (v) => { if (busy) utterances[utterances.length - 1].push(v); return false; };
cpu.writeHooks[GPIOR0] = (v) => { underruns = v; return false; };

const script = JSON.parse(fs.readFileSync(scriptPath, 'utf8'));
let step = 0, sendBuf = [], promptsSeen = 0, promptsNeeded = 1;

function run(cycles) {
  const end = cpu.cycles + cycles;
  while (cpu.cycles < end) {
    avrInstruction(cpu);
    cpu.tick();
    const sp = cpu.dataView.getUint16(0x5d, true);
    if (sp < minSP) minSP = sp;
    if (sendBuf.length && !usart.rxBusy) { if (usart.writeByte(sendBuf[0])) sendBuf.shift(); }
  }
}

const prompts = () => (tx.match(/>/g) || []).length;
run(FREQ * 0.01);                                  // 起動の '>' を待つ
for (const s of script) {
  if (s.send !== undefined) {
    if (s.at === 'busy') { const t0 = cpu.cycles; while (!busy && cpu.cycles - t0 < FREQ * 2) run(1000); run(FREQ * (s.afterMs || 0) / 1000); }
    sendBuf.push(...Buffer.from(s.send + '\n', 'ascii'));
  }
  if (s.wait === 'prompt') {
    const target = prompts() + 1, t0 = cpu.cycles;
    while (prompts() < target && cpu.cycles - t0 < FREQ * 15) run(4000);
  }
}
run(FREQ * 0.02);
console.log(JSON.stringify({ tx, underruns, minSP, ramUsed: 0x8ff - minSP, busyEdges, utterances: utterances.map((u) => u.length) }));
fs.writeFileSync(scriptPath.replace(/\.json$/, '.out.json'), JSON.stringify(utterances));
