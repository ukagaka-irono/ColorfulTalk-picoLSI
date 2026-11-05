#!/usr/bin/env bash
# pico LSI の試験: ファームウェアをビルドし、avr8js で UART からの操作を再現して、
# 返事・BUSY・PWM の値（PC 版 nano と一致するか）・合成の遅れ（アンダーラン）を確かめる。
#   AVR_GCC=<avr-gcc の bin> AVR8JS=<avr8js を npm install したフォルダ> NANO_CTALK=<PC 版 nano の ctalk> tests/check.sh
# SPDX-License-Identifier: MIT
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/build"
: "${AVR_GCC:?}" "${AVR8JS:?}" "${NANO_CTALK:?}"
mkdir -p "$OUT"
PATH="$AVR_GCC:$PATH" make -s -C "$ROOT/firmware" pico-lsi.hex >/dev/null
cp "$ROOT/firmware/pico-lsi.hex" "$OUT/"
cp "$ROOT/tests/sim.mjs" "$AVR8JS/sim.mjs"

S1="konnichiwa,seka'i."
S2="hoNtouni/so'udesuka?"
S3="kyo'uwa/i'i/teNkidesu."
cat > "$OUT/script.json" <<EOF
[
 {"send": "#?"}, {"wait": "prompt"},
 {"send": "$S1"}, {"wait": "prompt"},
 {"send": "#V 1"}, {"wait": "prompt"},
 {"send": "#S 150"}, {"wait": "prompt"},
 {"send": "$S2"}, {"wait": "prompt"},
 {"send": "#V 0"}, {"wait": "prompt"},
 {"send": "#S 100"}, {"wait": "prompt"},
 {"send": "konnichiwa@"}, {"wait": "prompt"},
 {"send": "#V 9"}, {"wait": "prompt"},
 {"send": "$S3"},
 {"send": "#X", "at": "busy", "afterMs": 300}, {"wait": "prompt"}
]
EOF
RESULT=$(cd "$AVR8JS" && node sim.mjs "$OUT/pico-lsi.hex" "$OUT/script.json")
echo "$RESULT"

# PC 版 nano の 8bit 出力と比べる（ctn_sample8 = clamp((s >> 7) + 128)）
"$NANO_CTALK" -v F -s 100 -o "$OUT/ref1.wav" "$S1"
"$NANO_CTALK" -v M -s 150 -o "$OUT/ref2.wav" "$S2"
python - "$OUT" <<'PY'
import json, struct, sys, os
d = sys.argv[1]
utt = json.load(open(os.path.join(d, "script.out.json")))
def ref(p):
    b = open(os.path.join(d, p), "rb").read()[44:]
    s = struct.unpack("<%dh" % (len(b) // 2), b)
    return [max(0, min(255, (v >> 7) + 128)) for v in s]
ok = True
for i, name in enumerate(["ref1.wav", "ref2.wav"]):
    r = ref(name)
    same = utt[i] == r
    ok &= same
    print("utterance %d: %d samples on chip, %d on PC -> %s" % (i + 1, len(utt[i]), len(r), "identical" if same else "DIFFERENT"))
print("stopped utterance: %d samples (stopped by #X)" % len(utt[2]))
sys.exit(0 if ok else 1)
PY
