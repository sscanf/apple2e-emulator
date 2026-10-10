#!/bin/sh
# Copy what the emulator needs at run time (ROMs, pictures, sounds) into DIR.
# Used by the release workflow; run from the project root.
#   tools/package.sh DIR
set -eu
dest=$1
mkdir -p "$dest/roms" "$dest/assets/drives" "$dest/assets/monitor" "$dest/assets/sounds" "$dest/sounds"
cp roms/*.rom "$dest/roms/"
cp assets/drives/*.png "$dest/assets/drives/"
cp assets/monitor/*.png "$dest/assets/monitor/"
cp assets/sounds/* "$dest/assets/sounds/"
cp sounds/* "$dest/sounds/"
cp LICENSE README.md "$dest/"
