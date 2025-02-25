#!/bin/bash

# Ensure the script runs from the correct directory (update path accordingly)
cd "$(dirname "$0")" || exit 1

# Run the Python parser on all JSON files
python skeleton_parser.py ebay_data/items-*.json
