#!/bin/bash
node ulight/scripts/theme-to-css.js \
  -i ulight/themes/wg21.json \
  -o assets/code.css \
  --no-block-background \
  --no-block-foreground \
  --mmml
