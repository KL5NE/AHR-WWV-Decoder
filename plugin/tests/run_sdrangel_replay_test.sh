#!/usr/bin/env bash
# End-to-end test of the plugin inside a running SDRangel (Linux dev build).
#
# Replays a synthetic WWV recording through SDRangel's File Input device, adds
# the AHR WWV channel through SDRangel's REST API, and checks the decoded
# minutes written to SDRangel's log against the recording's known start time.
#
# usage: run_sdrangel_replay_test.sh <sdrangel binary> <wwv_make_sdriq binary> [seconds] [carrier offset Hz]
# needs: curl, python3; a display (e.g. Xvfb :99 and DISPLAY=:99) or QT_QPA_PLATFORM=offscreen
# The REST API cannot set this channel's offset (no web API adapter), so the
# carrier is placed at the device centre unless an offset is given AND the
# channel offset is set by hand in the GUI.
set -u
SDR=${1:?sdrangel binary}
MKIQ=${2:?wwv_make_sdriq binary}
SECS=${3:-240}
OFFSET=${4:-0}
API=http://127.0.0.1:8091/sdrangel
WORK=$(mktemp -d)
LOG=$WORK/sdrangel.log
START=$(( $(date +%s) / 60 * 60 - 3600 + 17 ))   # an hour ago, 17 s into a minute: arbitrary but known

"$MKIQ" "$WORK/wwv.sdriq" --seconds $((SECS + 60)) --rate 48000 --centre 10000000 --offset "$OFFSET" --snr 20 --start "$START" || exit 2

"$SDR" > "$LOG" 2>&1 &
PID=$!
trap 'kill $PID 2>/dev/null; wait $PID 2>/dev/null' EXIT

for i in $(seq 1 60); do curl -s "$API" > /dev/null && break; sleep 1; done
grep -q "loaded plugin .*ahrwwvtimedecoder" "$LOG" && echo "PASS plugin loaded by SDRangel" || { echo "FAIL plugin not loaded"; exit 1; }

curl -s -X POST "$API/deviceset?direction=0" > /dev/null; sleep 3
curl -s -X PUT "$API/deviceset/0/device" -H 'Content-Type: application/json' -d '{"hwType":"FileInput","direction":0}' > /dev/null; sleep 3
curl -s -X PATCH "$API/deviceset/0/device/settings" -H 'Content-Type: application/json' \
  -d "{\"deviceHwType\":\"FileInput\",\"direction\":0,\"fileInputSettings\":{\"fileName\":\"$WORK/wwv.sdriq\",\"loop\":0,\"accelerationFactor\":1}}" > /dev/null
curl -s -X POST "$API/deviceset/0/channel" -H 'Content-Type: application/json' \
  -d '{"channelType":"AHRWWVTimeDecoder","direction":0,"originatorDeviceSetIndex":0}' > /dev/null; sleep 2
curl -s "$API/deviceset/0" | grep -q AHRWWVTimeDecoder && echo "PASS channel created" || { echo "FAIL channel not created"; exit 1; }
curl -s -X POST "$API/deviceset/0/device/run" > /dev/null
echo "replaying $SECS s of signal (real time)..."
sleep "$SECS"

python3 - "$LOG" "$START" <<'PY'
import re, sys, datetime
log, start = sys.argv[1], int(sys.argv[2])
acc = [l for l in open(log, errors="replace") if "AHRWWV: frame accepted" in l]
rej = [l for l in open(log, errors="replace") if "AHRWWV: frame rejected" in l]
print(f"frames accepted {len(acc)}, rejected {len(rej)}")
ok = True
valid_minutes = set()
for m in range(0, 20):
    t = datetime.datetime.fromtimestamp(start // 60 * 60 + 60 * m, datetime.timezone.utc)
    valid_minutes.add((t.year, t.timetuple().tm_yday, t.hour, t.minute))
states = []
for l in acc:
    g = re.search(r"accepted (\d{4}) day (\d{3}) (\d\d):(\d\d) UTC.*state (\w+)", l)
    key = tuple(int(x) for x in g.groups()[:4])
    states.append(g.group(5))
    if key not in valid_minutes:
        ok = False
        print("WRONG minute decoded:", l.strip())
print("states:", states)
if not acc:
    print("FAIL no frame decoded"); sys.exit(1)
if not ok:
    print("FAIL wrong time decoded"); sys.exit(1)
if "Verified" not in states:
    print("FAIL never verified"); sys.exit(1)
print("PASS decoded minutes match the recording, verified")
PY
