#!/bin/sh
# Render the SVG drawings used by the emulator window to the PNGs it loads,
# with headless Google Chrome (the drawings use text, filters, masks and clip
# paths that SDL's SVG loader cannot draw):
#   assets/drives/src/*.svg  -> assets/drives/*.png   (about 500x310)
#   assets/monitor/src/*.svg -> assets/monitor/*.png  (twice the SVG size)
#
# In the drive drawings the texture filter is confined to each shape's
# bounding box; with the default filter region it leaves a light haze.
set -e
cd "$(dirname "$0")/.."
CHROME=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# render SVG OUT_DIR SCALE WIDTH HEIGHT
render() {
    name=$(basename "$1" .svg)
    sed 's/<filter id="texture">/<filter id="texture" x="0" y="0" width="1" height="1">/' "$1" > "$TMP/$name.svg"
    "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
        --force-device-scale-factor="$3" --default-background-color=00000000 \
        --window-size="$4,$5" --screenshot="$TMP/$name.png" "file://$TMP/$name.svg" 2>/dev/null
    cp "$TMP/$name.png" "$2/$name.png"
    echo "$2/$name.png"
}

for svg in assets/drives/src/*.svg; do render "$svg" assets/drives 0.48 1000 620; done
for svg in assets/monitor/src/*.svg; do render "$svg" assets/monitor 2 690 512; done
