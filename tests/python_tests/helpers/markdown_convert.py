import csv
from pathlib import Path

def csv_to_markdown_table(csv_path):
    with open(csv_path, newline='') as f:
        reader = list(csv.reader(f))
    
    header = reader[0]
    rows = reader[1:]

    # Build markdown table
    md_lines = []

    # Header
    md_lines.append('| ' + ' | '.join(header) + ' |')

    # Separator
    md_lines.append('| ' + ' | '.join(['---'] * len(header)) + ' |')

    # Data rows
    for row in rows:
        md_lines.append('| ' + ' | '.join(row) + ' |')

    return '\n'.join(md_lines)

# Example usage
md_table = csv_to_markdown_table('/localdev/skrsmanovic/gitrepos/tt-metal/tt_metal/third_party/tt_llk/perf/eltwise_binary_fpu_perf.csv')
print(md_table)