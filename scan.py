# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
import os
from collections import defaultdict

# List of folders to scan
ARCH = "quasar"
FOLDERS = [
    f"tt_llk_{'wormhole_b0' if ARCH == 'wormhole' else ARCH}",
    f"tests/hw_specific/{ARCH}/inc",
    "tests/firmware/riscv/common",
    "tests/helpers/include",
]


def list_files_with_paths(folders):
    file_paths = []
    for folder in folders:
        for root, dirs, files in os.walk(folder):
            for file in files:
                file_paths.append(os.path.join(root, file))
    return file_paths


def extract_includes(file_path):
    includes = []
    try:
        with open(file_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line.startswith("#include"):
                    # Extract filename from #include statement
                    if '"' in line:
                        # For #include "filename.h"
                        start = line.find('"') + 1
                        end = line.find('"', start)
                        if start > 0 and end > start:
                            includes.append(line[start:end])
                    elif "<" in line and ">" in line:
                        # For #include <filename.h>
                        start = line.find("<") + 1
                        end = line.find(">", start)
                        if start > 0 and end > start:
                            includes.append(line[start:end])
    except (UnicodeDecodeError, IOError):
        pass
    return includes


def build_include_graph(files):
    """Build an include graph from the files."""
    include_graph = defaultdict(list)
    file_to_path = {}
    path_to_file = {}

    # Create mappings for both filename-only and full relative paths
    for file_path in files:
        filename = os.path.basename(file_path)
        file_to_path[filename] = file_path

        # Also create mapping for relative paths within the folders
        for folder in FOLDERS:
            if file_path.startswith(folder + "/"):
                relative_path = file_path[
                    len(folder) + 1 :
                ]  # Remove folder prefix and slash
                path_to_file[relative_path] = file_path

    def find_matching_file(include_path):
        """Find a matching file for an include path, handling both exact matches and partial paths."""
        # First try exact filename match
        if include_path in file_to_path:
            return include_path, file_to_path[include_path]

        # Then try relative path match
        if include_path in path_to_file:
            return os.path.basename(include_path), path_to_file[include_path]

        # Try to find files that end with this path (for cases like "sfpu/file.h")
        for rel_path, full_path in path_to_file.items():
            if rel_path.endswith(include_path):
                return os.path.basename(include_path), full_path

        # If no match found, return the original include path
        return include_path, None

    # Build the graph
    for file_path in files:
        filename = os.path.basename(file_path)
        includes = extract_includes(file_path)

        for include in includes:
            matched_key, matched_path = find_matching_file(include)
            include_graph[filename].append(matched_key)
            if matched_path:
                file_to_path[matched_key] = matched_path

    return include_graph, file_to_path


def print_include_graph(include_graph, file_to_path):
    """Print the include graph in a readable format."""
    print("\n" + "=" * 60)
    print("INCLUDE GRAPH")
    print("=" * 60)

    for file, includes in sorted(include_graph.items()):
        if includes:
            print(f"\n{file}:")
            print(f"  Path: {file_to_path.get(file, 'Unknown')}")
            print(f"  Includes:")
            for include in sorted(includes):
                include_path = file_to_path.get(include)
                if include_path:
                    print(f"    -> {include}")
                    print(f"       ({include_path})")
                else:
                    print(f"    -> {include}")
                    print(f"       (Not found in scanned files)")


def find_circular_dependencies(include_graph):
    """Find circular dependencies in the include graph."""

    def dfs(node, visited, rec_stack, path):
        visited.add(node)
        rec_stack.add(node)
        path.append(node)

        for neighbor in include_graph.get(node, []):
            if neighbor not in visited:
                cycles = dfs(neighbor, visited, rec_stack, path.copy())
                if cycles:
                    return cycles
            elif neighbor in rec_stack:
                # Found a cycle
                cycle_start = path.index(neighbor)
                return [path[cycle_start:] + [neighbor]]

        rec_stack.remove(node)
        return []

    visited = set()
    all_cycles = []

    for node in include_graph:
        if node not in visited:
            cycles = dfs(node, visited, set(), [])
            all_cycles.extend(cycles)

    return all_cycles


def generate_dot_graph(include_graph, file_to_path, output_file="include_graph.dot"):
    """Generate a DOT file for graphviz visualization."""
    # Collect all files mentioned (both as includers and included)
    all_mentioned_files = set(include_graph.keys())
    for includes in include_graph.values():
        all_mentioned_files.update(includes)

    with open(output_file, "w") as f:
        f.write("digraph IncludeGraph {\n")
        f.write("  rankdir=TB;\n")
        f.write("  edge [color=blue];\n\n")

        # Add nodes with different shapes based on whether they exist in scanned files
        for file in sorted(all_mentioned_files):
            label = file.replace(".", "\\n.")  # Break long filenames

            if file in file_to_path:
                # File exists in our scanned files - use box shape
                f.write(
                    f'  "{file}" [label="{label}", shape=box, style="rounded,filled", '
                    f"fillcolor=lightblue, color=darkblue];\n"
                )
            else:
                # File is referenced but not found in scanned files - use ellipse shape
                f.write(
                    f'  "{file}" [label="{label}", shape=ellipse, style="filled", '
                    f"fillcolor=lightgray, color=red, fontcolor=red];\n"
                )

        f.write("\n")

        # Add edges
        for file, includes in include_graph.items():
            for include in includes:
                f.write(f'  "{file}" -> "{include}";\n')

        f.write("}\n")

    print(f"\nDOT file generated: {output_file}")
    print("Legend:")
    print("  - Blue rounded boxes: Files found in scanned directories")
    print(
        "  - Gray ellipses (red text): Files referenced but not found in scanned directories"
    )
    print("To visualize, install graphviz and run:")
    print(f"  dot -Tpng {output_file} -o include_graph.png")
    print(f"  dot -Tsvg {output_file} -o include_graph.svg")


print("\nListing files:")
files = list_files_with_paths(FOLDERS)
for file_path in files:
    print(file_path)


print("\nScanning for #include statements:")
for file_path in files:
    includes = extract_includes(file_path)
    if includes:
        print(f"\n{file_path}:")
        for include in includes:
            print(f"  {include}")

print("\nChecking for missing includes:")
missing_includes = []

# Create a set of just filenames for faster lookup
file_names = set()
for file_path in files:
    file_names.add(os.path.basename(file_path))

include_count = 0

for file_path in files:
    includes = extract_includes(file_path)
    include_count += len(includes)
    for include in includes:
        if include not in file_names:
            missing_includes.append((file_path, include))

print(f"\nTotal includes found: {include_count}\n")

if missing_includes:
    print("Missing include files:")
    for file_path, missing_include in missing_includes:
        print(f"  {file_path} -> {missing_include}")
else:
    print("All includes found in file list.")

print("\nDeduplicated missing includes:")
unique_missing = set()
for file_path, missing_include in missing_includes:
    unique_missing.add(missing_include)

if unique_missing:
    for missing_include in sorted(unique_missing):
        print(f"  {missing_include}")
else:
    print("No missing includes found.")


# Build and display include graph
print("\n" + "=" * 60)
print("BUILDING INCLUDE GRAPH")
print("=" * 60)

include_graph, file_to_path = build_include_graph(files)

# Print the include graph
print_include_graph(include_graph, file_to_path)

# Check for circular dependencies
print("\n" + "=" * 60)
print("CIRCULAR DEPENDENCY ANALYSIS")
print("=" * 60)

circular_deps = find_circular_dependencies(include_graph)
if circular_deps:
    print("WARNING: Circular dependencies found!")
    for i, cycle in enumerate(circular_deps, 1):
        print(f"\nCircular dependency #{i}:")
        cycle_str = " -> ".join(cycle)
        print(f"  {cycle_str}")
else:
    print("No circular dependencies found.")

# Generate DOT file for visualization
print("\n" + "=" * 60)
print("GRAPH VISUALIZATION")
print("=" * 60)

generate_dot_graph(include_graph, file_to_path)

# Print some statistics
print("\n" + "=" * 60)
print("INCLUDE GRAPH STATISTICS")
print("=" * 60)

total_files = len(file_to_path)
files_with_includes = len([f for f in include_graph if include_graph[f]])
total_include_relationships = sum(len(includes) for includes in include_graph.values())

print(f"Total files scanned: {total_files}")
print(f"Files with includes: {files_with_includes}")
print(f"Total include relationships: {total_include_relationships}")
if files_with_includes > 0:
    avg_includes = total_include_relationships / files_with_includes
    print(f"Average includes per file (with includes): {avg_includes:.2f}")

# Find files with most includes
if include_graph:
    most_includes = max(include_graph.items(), key=lambda x: len(x[1]))
    print(
        f"File with most includes: {most_includes[0]} ({len(most_includes[1])} includes)"
    )

# Find most included files
include_count = defaultdict(int)
for includes in include_graph.values():
    for include in includes:
        include_count[include] += 1

if include_count:
    most_included = max(include_count.items(), key=lambda x: x[1])
    print(f"Most included file: {most_included[0]} (included {most_included[1]} times)")
