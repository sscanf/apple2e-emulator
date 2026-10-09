#!/bin/sh
# Render the Disk II drive pictures (assets/drives/src/*.svg) to the PNGs the
# side panel loads (assets/drives/*.png), using headless Google Chrome: the
# drawings use text, filters and clip paths that SDL's SVG loader cannot draw.
#
# The texture filter is confined to each shape's bounding box while
# rendering; with the default filter region it leaves a light haze around them.
set -e
cd "$(dirname "$0")/.."
CHROME=${CHROME:-"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"}
SCALE=${SCALE:-0.48}   # 1000x620 drawings -> about 500x310
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

for svg in assets/drives/src/*.svg; do
    name=$(basename "$svg" .svg)
    sed 's/<filter id="texture">/<filter id="texture" x="0" y="0" width="1" height="1">/' "$svg" > "$TMP/$name.svg"
    "$CHROME" --headless=new --disable-gpu --hide-scrollbars \
        --force-device-scale-factor="$SCALE" --default-background-color=00000000 \
        --window-size=1000,620 --screenshot="$TMP/$name.png" "file://$TMP/$name.svg" 2>/dev/null
    cp "$TMP/$name.png" "assets/drives/$name.png"
    echo "assets/drives/$name.png"
done
