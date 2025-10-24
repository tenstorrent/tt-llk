# Quick Start: Dumping Test Parameters

## Prerequisites

First, install the Python dependencies (if not already done):

```bash
cd /Users/fvranic/Work/tt-llk/tests
pip install -r requirements.txt
```

## Simplest Way to Dump All Parameters

```bash
cd /Users/fvranic/Work/tt-llk/tests/python_tests

# Run this single command to get all parameter data
pytest --collect-only -p pytest_param_dumper --param-dump=all_params --param-format=all --param-summary
```

This will create a `parameter_coverage_reports/` directory with:
- `all_params.json` - Complete parameter data
- `all_params.csv` - Spreadsheet-friendly format
- `all_params.xlsx` - Excel with multiple analysis sheets
- `all_params_summary.txt` - Human-readable summary

## What You'll Get

The summary will show you:
- Total number of tests and parametrized tests
- All unique parameters used across tests
- How many times each parameter is used
- Sample values for each parameter

Example snippet:
```
Total tests collected: 547
Parametrized tests: 423
Unique parameters: 12

Parameter Usage:
  formats: 156 tests (8 unique values)
  dest_acc: 142 tests (2 unique values)
  math_fidelity: 89 tests (4 unique values)
  ...
```

## Using the Convenience Script

Alternatively, use the provided script:

```bash
./dump_test_parameters.sh
```

This script will generate multiple output formats and show you where the files are saved.
