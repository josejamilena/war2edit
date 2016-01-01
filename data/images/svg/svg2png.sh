#! /usr/bin/env sh

set -e

WIDTH=100
HEIGHT=100
OUT_DIR="$(dirname "$0")/../"
INKSCAPE="$(which inkscape)"

err() {
   echo "*** $@" 1>&2
}

inkscape_export() {
   file="$1"
   out="$OUT_DIR/$(echo "$file" | cut -d '.' -f 1)"

   "$INKSCAPE" \
      --export-width="$WIDTH" \
      --export-height="$HEIGHT" \
      --export-png="$out".png \
      "$file"
}

FILES="$(ls -1 *.svg)"

for f in $FILES; do
   inkscape_export "$f"
done

