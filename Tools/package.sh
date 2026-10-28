#!/usr/bin/env bash
# Build the watch app and pack the community-app-store upload:
#   Output/SevenMinute_store.zip = *.uapp + app-manifest.json + icon.png
# Usage: UNA_SDK=/path/to/una-sdk Tools/package.sh
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${UNA_SDK:?Set UNA_SDK to the una-sdk checkout}"
BUILD="$ROOT/build"

cmake -G "Unix Makefiles" -S "$ROOT/Software/Apps/SevenMinute-CMake" -B "$BUILD"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

UAPP=$(ls -t "$BUILD"/SevenMinute_*.uapp | head -1)
NAME=$(basename "$UAPP")
VERSION=${NAME#SevenMinute_}; VERSION=${VERSION%.uapp}; VERSION=${VERSION%%-*}
APP_ID=$(sed -n 's/^set(APP_ID "\([0-9A-F]*\)").*/\1/p' "$ROOT/Software/Apps/SevenMinute-CMake/CMakeLists.txt")

STAGE=$(mktemp -d)
cp "$UAPP" "$STAGE/"
cp "$ROOT/Resources/icon.png" "$STAGE/icon.png"

cat > "$STAGE/app-manifest.json" <<JSON
{
  "manifest_version": 1,
  "type": ["activity"],
  "name": "7-Minute",
  "icon": "icon.png",
  "binary": "$NAME",
  "appVersion": "$VERSION",
  "minKernelVersion": "1.4.0",
  "requiredHardware": [],
  "description": "The scientific 7-minute workout (Klika & Jordan, 2013) on your wrist: 12 body-weight exercises, 30 s each with 10 s transitions, guided by distinct beeps and buzzes so you never need to look at the watch. A 3-2-1 countdown, a long GO, a falling REST, a triple chirp to switch sides on the side plank, and a fanfare at the finish. 1-3 rounds, beep, buzz or both, live heart rate, pause, skip and a workout summary.",
  "stravaExport": false,
  "id": "$APP_ID",
  "supportsLaps": false,
  "supportsDistance": false,
  "supportsTrack": false,
  "supportsHeartbeat": false,
  "supportsElevation": false,
  "supportsStep": false,
  "supportsSpeed": "none",
  "customMeasures": []
}
JSON
python3 "$UNA_SDK/Utilities/Scripts/app_packer/min_kernel_version.py" --stamp "$STAGE/app-manifest.json"
# Checked against the store's own errors (Oct 2026): it requires
# manifest_version, minKernelVersion and description, and rejects any key it
# does not know, including the "previews" the SDK docs show. Screenshots are
# added on the app's page in the portal.
python3 - "$STAGE/app-manifest.json" <<'PY'
import json, sys
allowed = {"manifest_version", "type", "name", "icon", "binary", "appVersion", "minKernelVersion",
           "requiredHardware", "description", "stravaExport", "id", "supportsLaps", "supportsDistance",
           "supportsTrack", "supportsHeartbeat", "supportsElevation", "supportsStep",
           "supportsSpeed", "customMeasures", "configFields", "configFile"}
required = {"manifest_version", "minKernelVersion", "description"}
m = json.load(open(sys.argv[1]))
extra, missing = sorted(set(m) - allowed), sorted(required - set(m))
if extra or missing:
    sys.exit(f"app-manifest.json: the store rejects {extra} and requires {missing}")
PY

ZIP="$ROOT/Output/SevenMinute_${VERSION}_store.zip"
rm -f "$ZIP"
(cd "$STAGE" && zip -qr "$ZIP" .)
cp "$UAPP" "$ROOT/Output/"
rm -rf "$STAGE"
echo "Store package: $ZIP"
unzip -l "$ZIP"
