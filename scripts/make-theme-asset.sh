#!/bin/bash
node ulight/scripts/theme-to-css.js \
  -i ulight/themes/ulight.json \
  -o assets/code.css \
  --no-block-background \
  --no-block-foreground \
  --mmml
