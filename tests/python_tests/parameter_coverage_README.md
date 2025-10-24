# Test Parameter Coverage Analysis

This directory contains a pytest plugin to dump all parameters used in pytest tests to create coverage tables.

## How It Works

The `pytest_param_dumper.py` plugin captures actual runtime parameters during pytest test collection. This shows exactly what pytest will test, including all parameter expansions from `@pytest.mark.parametrize` and custom `@parametrize` decorators.

## Quick Start
```bash
# Make the script executable (only needed once)
chmod +x dump_test_parameters.sh

# Run the parameter dump
./dump_test_parameters.sh
```

## Manual Usage
```bash
# Generate JSON dump
pytest --collect-only -p pytest_param_dumper --param-dump=params.json --param-format=json --param-summary

# Generate CSV for spreadsheets
pytest --collect-only -p pytest_param_dumper --param-dump=params.csv --param-format=csv

# Generate Excel with multiple analysis sheets
pytest --collect-only -p pytest_param_dumper --param-dump=params.xlsx --param-format=excel

# Generate all formats at once
pytest --collect-only -p pytest_param_dumper --param-dump=params --param-format=all --param-summary
```

## Output Files
- **JSON**: Complete parameter data with all details
- **CSV**: Flattened view for easy spreadsheet analysis
- **Excel**: Multi-sheet workbook with:
  - Tests sheet: All tests with their parameters
  - Parameter Summary: Unique values and usage counts
  - Coverage Matrix: Which tests use which parameters
- **Summary Text**: Human-readable summary statistics

## Examples of What You'll See

### Parameter Summary Example:
```
TEST PARAMETER SUMMARY
==================================================

Total tests collected: 547
Parametrized tests: 423
Unique parameters: 12

Parameter Usage:
------------------------------
  formats: 156 tests (8 unique values)
  dest_acc: 142 tests (2 unique values)
  math_fidelity: 89 tests (4 unique values)
  reduce_dim: 45 tests (3 unique values)
  ...
```

### Coverage Matrix Example:
| test_name | formats | dest_acc | math_fidelity | reduce_dim |
|-----------|---------|----------|---------------|------------|
| test_math_matmul | ✓ | ✓ | ✓ | |
| test_reduce | ✓ | ✓ | | ✓ |
| test_eltwise_unary | ✓ | ✓ | | |

## Notes

- The pytest plugin requires pytest to be properly configured and able to collect tests
- Respects the current pytest configuration and markers
- Large test suites may take a few moments to analyze
- The Excel format provides the most comprehensive analysis with multiple views
- Requires pandas and openpyxl packages (added to requirements.txt)
