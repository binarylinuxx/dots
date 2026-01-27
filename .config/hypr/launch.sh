#!/bin/bash

# List of all widgets you've tried to open
widgets=(
  "Bar"
  "top-left"
  "top-right"
  "dock-bottom"
  "blox-left"
  "blox-right"
  "left-center-gap"
  "right-center-gap"
  "bottom-gap"
  "bottom-right-corner"
  "bottom-right-gap"
  "bottom-center-gap"
  "bottom-left-gap"
  "bottom-left-corner"
)

# Open each widget, ignoring errors for non-existent ones
for widget in "${widgets[@]}"; do
  eww open "$widget" 2>/dev/null
done

# Optional: add a small delay between opening widgets to reduce load
# for widget in "${widgets[@]}"; do
#   eww open "$widget" 2>/dev/null
#   sleep 0.1
# done

echo "All available EWW widgets have been opened"
