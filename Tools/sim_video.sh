#!/usr/bin/env bash
# Record a whole one-round workout in the simulator for the demo video:
# get ready, a work interval, a rest with the next exercise, pause/resume,
# then on to the side plank's switch and the finish (about 9 minutes).
# Writes OUTDIR/demo.mp4, OUTDIR/sim.log and OUTDIR/marks.txt; then
# Tools/make_video.py OUTDIR keeps the start and the end, in real time, with
# the beeps rebuilt from the log.
#
# Needs Xvfb, xdotool and ffmpeg. Usage: Tools/sim_video.sh OUTDIR
set -euo pipefail
OUT=${1:-/tmp/sim-video}; mkdir -p "$OUT"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/Software/Apps/LVGL-GUI/simulator/build/bin"
export DISPLAY=${DISPLAY_NUM:-:97} UNA_SIM_MUTE=1   # beeps are rebuilt from the log
Xvfb "$DISPLAY" -screen 0 640x480x24 >/dev/null 2>&1 & XPID=$!
sleep 1
rm -f "$ROOT/Output/sevenminute.cfg" "$OUT/marks.txt"
cd "$BIN"; stdbuf -oL -eL ./SevenMinuteSimulator > "$OUT/sim.log" 2>&1 & SPID=$!
VPID=""
trap 'kill $SPID $XPID $VPID 2>/dev/null || true' EXIT
sleep 3
W=$(xdotool search --name "7-Minute" | head -1); xdotool windowmove "$W" 0 0; sleep 0.5
ffmpeg -loglevel error -y -f x11grab -draw_mouse 0 -framerate 25 -video_size 480x480 -i "$DISPLAY+0,0" \
    -c:v libx264 -pix_fmt yuv420p -crf 20 "$OUT/demo.mp4" & VPID=$!
T0=$(date +%s.%N)
mark() { echo "$(echo "$(date +%s.%N) - $T0" | bc) $1" >> "$OUT/marks.txt"; }
key()  { xdotool key --window "$W" "$1"; sleep "${2:-0.5}"; }

waitlog() { until [ "$(grep -c -- "$1" "$OUT/sim.log" || true)" -ge "${2:-1}" ]; do sleep 0.2; done; }
sleep 1.5
mark home;     sleep 1.5
mark settings; key 1 0.9; key 2 0.9; key 1 0.6; key 1 0.6; key 2 0.6; key 2 0.9
mark start;    key 3
waitlog "CUE GO" 2; sleep 3
key 3 2.5; key 3                     # pause, resume
waitlog "CUE FINISHED"; sleep 6       # the whole round: no skipping
mark end
kill -INT $VPID; wait $VPID 2>/dev/null || true; VPID=""
echo "done: $OUT"
