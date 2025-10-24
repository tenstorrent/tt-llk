#!/bin/bash
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

# Script to dump all test parameters from pytest tests

# Check if pytest is available
if ! command -v pytest &> /dev/null; then
    echo "ERROR: pytest not found. Please install test dependencies first:"
    echo ""
    echo "  cd $(dirname "$0")/../.."
    echo "  pip install -r requirements.txt"
    echo ""
    echo "Or if using a virtual environment:"
    echo "  python3 -m venv venv"
    echo "  source venv/bin/activate"
    echo "  pip install -r requirements.txt"
    echo ""
    exit 1
fi

echo "Dumping test parameters from pytest tests..."
echo "============================================"

# Create output directory
OUTPUT_DIR="parameter_coverage_reports"
mkdir -p "$OUTPUT_DIR"

# Run pytest with the parameter dumper plugin
# --collect-only: Only collect tests, don't run them
# -p: Load the plugin module
# --param-dump: Output file for parameters
# --param-format: Output format (json, csv, excel, all)
# --param-summary: Generate summary statistics

echo ""
echo "1. Generating JSON dump with all parameter details..."
pytest --collect-only -p pytest_param_dumper \
    --param-dump="$OUTPUT_DIR/all_parameters.json" \
    --param-format=json \
    --param-summary

echo ""
echo "2. Generating CSV format for easy viewing in spreadsheets..."
pytest --collect-only -p pytest_param_dumper \
    --param-dump="$OUTPUT_DIR/all_parameters.csv" \
    --param-format=csv \
    -q  # Quiet mode to reduce output

echo ""
echo "3. Generating Excel workbook with multiple analysis sheets..."
pytest --collect-only -p pytest_param_dumper \
    --param-dump="$OUTPUT_DIR/all_parameters.xlsx" \
    --param-format=excel \
    -q

echo ""
echo "4. Generating all formats at once..."
pytest --collect-only -p pytest_param_dumper \
    --param-dump="$OUTPUT_DIR/complete_dump" \
    --param-format=all \
    --param-summary \
    -q

echo ""
echo "============================================"
echo "Parameter dumps have been generated in: $OUTPUT_DIR/"
echo ""
echo "Files generated:"
echo "  - all_parameters.json: Complete parameter data in JSON format"
echo "  - all_parameters.csv: Flattened view in CSV format"
echo "  - all_parameters.xlsx: Excel workbook with multiple analysis sheets"
echo "  - complete_dump.*: All formats including summary report"
echo ""
echo "To view a quick summary, check: $OUTPUT_DIR/complete_dump_summary.txt"

# Optional: Show a quick preview of the summary
if [ -f "$OUTPUT_DIR/complete_dump_summary.txt" ]; then
    echo ""
    echo "Quick Preview of Parameter Summary:"
    echo "-----------------------------------"
    head -n 20 "$OUTPUT_DIR/complete_dump_summary.txt"
    echo "..."
    echo "(See full summary in $OUTPUT_DIR/complete_dump_summary.txt)"
fi
