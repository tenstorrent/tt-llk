# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""Debug utilities for test failure analysis."""

from datetime import datetime
from pathlib import Path

import torch


def dump_test_failure(
    test_name: str,
    golden_tensor: torch.Tensor,
    result_tensor: torch.Tensor,
    test_params: dict,
    output_dir: str = "test_failures",
):
    """
    Dump detailed information about a test failure to a file.

    Creates a file in the specified output directory containing:
    - All test parameters
    - Golden tensor values
    - Result tensor values
    - Differences between tensors
    - Statistics about mismatches

    Args:
        test_name: Name of the test that failed
        golden_tensor: Expected output tensor
        result_tensor: Actual output tensor from hardware
        test_params: Dictionary of test parameters
        output_dir: Directory to save failure dumps (default: "test_failures")

    Returns:
        str: Path to the created dump file
    """
    # Create output directory if it doesn't exist
    dump_dir = Path(output_dir)
    dump_dir.mkdir(parents=True, exist_ok=True)

    # Generate unique filename with timestamp
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    filename = f"{test_name}_{timestamp}.txt"
    filepath = dump_dir / filename

    with open(filepath, "w") as f:
        # Write header
        f.write("=" * 80 + "\n")
        f.write(f"TEST FAILURE DUMP: {test_name}\n")
        f.write(f"Timestamp: {datetime.now().isoformat()}\n")
        f.write("=" * 80 + "\n\n")

        # Write test parameters
        f.write("TEST PARAMETERS:\n")
        f.write("-" * 80 + "\n")
        for key, value in test_params.items():
            f.write(f"{key}: {value}\n")
        f.write("\n")

        # Write tensor shapes and dtypes
        f.write("TENSOR INFORMATION:\n")
        f.write("-" * 80 + "\n")
        f.write(f"Golden tensor shape: {golden_tensor.shape}\n")
        f.write(f"Golden tensor dtype: {golden_tensor.dtype}\n")
        f.write(f"Result tensor shape: {result_tensor.shape}\n")
        f.write(f"Result tensor dtype: {result_tensor.dtype}\n")
        f.write(f"Total elements: {golden_tensor.numel()}\n")
        f.write("\n")

        # Calculate differences
        abs_diff = torch.abs(golden_tensor - result_tensor)
        rel_diff = torch.where(
            golden_tensor != 0,
            abs_diff / torch.abs(golden_tensor),
            torch.zeros_like(abs_diff),
        )

        # Calculate mismatch statistics
        mismatches = ~torch.isclose(golden_tensor, result_tensor, rtol=0.05, atol=0.05)
        num_mismatches = mismatches.sum().item()
        mismatch_percentage = (num_mismatches / golden_tensor.numel()) * 100

        f.write("MISMATCH STATISTICS:\n")
        f.write("-" * 80 + "\n")
        f.write(f"Number of mismatches: {num_mismatches}\n")
        f.write(f"Mismatch percentage: {mismatch_percentage:.2f}%\n")
        f.write(f"Max absolute difference: {abs_diff.max().item():.10f}\n")
        f.write(f"Mean absolute difference: {abs_diff.mean().item():.10f}\n")
        f.write(f"Max relative difference: {rel_diff.max().item():.10f}\n")
        f.write(f"Mean relative difference: {rel_diff.mean().item():.10f}\n")
        f.write(f"relu threshold: {test_params.get('relu_threshold', 'N/A')}\n")
        f.write(f"intermediate type: {test_params.get('intermediate_type', 'N/A')}\n")
        f.write(test_params.get("data_formats", "Data formats: N/A") + "\n")
        f.write("\n")

        # Write detailed value comparison
        f.write("DETAILED VALUE COMPARISON:\n")
        f.write("-" * 80 + "\n")
        f.write(
            f"{'Index':<10} {'Golden':<20} {'Result':<20} {'Abs Diff':<20} {'Rel Diff':<20}\n"
        )
        f.write("-" * 80 + "\n")

        # Write all mismatches
        mismatch_indices = torch.where(mismatches)[0]
        for idx in mismatch_indices:
            idx_val = idx.item()
            golden_val = golden_tensor[idx].item()
            result_val = result_tensor[idx].item()
            abs_diff_val = abs_diff[idx].item()
            rel_diff_val = rel_diff[idx].item()

            f.write(
                f"{idx_val:<10} {golden_val:<20.10f} {result_val:<20.10f} "
                f"{abs_diff_val:<20.10f} {rel_diff_val:<20.10f}\n"
            )

        f.write("\n")

        # Write full tensor dumps with enhanced matrix visualization
        input_dims = test_params.get("input_dimensions", None)
        if input_dims and len(input_dims) == 2:
            height, width = input_dims

            try:
                golden_matrix = golden_tensor.view(height, width)
                result_matrix = result_tensor.view(height, width)
                mismatch_matrix = mismatches.view(height, width)

                # Visual pattern map for quick identification
                f.write("VISUAL PATTERN MAP (each symbol = one element):\n")
                f.write("-" * 80 + "\n")
                f.write(
                    "Legend: \033[92m●\033[0m=Match, \033[93m░\033[0m=Small diff, \033[91m▓\033[0m=Med diff, \033[91m█\033[0m=Large diff\n"
                )
                f.write(f"Matrix: {height} rows × {width} cols\n")

                # Column markers every 10 for reference
                col_header = "Col: "
                for col in range(0, width, 10):
                    col_header += f"{col:>10d}"
                f.write(col_header + "\n")
                f.write("-" * min(80, len(col_header)) + "\n")

                # Visual pattern map
                for i in range(height):
                    row_str = f"R{i:2d}: "
                    for j in range(width):
                        is_mismatch = mismatch_matrix[i, j].item()
                        if is_mismatch:
                            golden_val = golden_matrix[i, j].item()
                            result_val = result_matrix[i, j].item()
                            abs_diff = abs(golden_val - result_val)

                            if abs_diff > 0.5:
                                symbol = "\033[91m█\033[0m"  # Large error
                            elif abs_diff > 0.1:
                                symbol = "\033[91m▓\033[0m"  # Medium error
                            else:
                                symbol = "\033[93m░\033[0m"  # Small error
                        else:
                            symbol = "\033[92m●\033[0m"  # Match

                        row_str += symbol

                    f.write(row_str + "\n")

                f.write("\n")

                # Tile analysis for common 32x32 patterns
                if height == 32 and width % 32 == 0:
                    tiles_x = width // 32
                    f.write("TILE BOUNDARY ANALYSIS (32x32 tiles):\n")
                    f.write("-" * 80 + "\n")
                    f.write(f"Total tiles: {tiles_x}\n")

                    for tile_x in range(tiles_x):
                        start_col = tile_x * 32
                        end_col = (tile_x + 1) * 32

                        tile_errors = 0
                        for i in range(height):
                            for j in range(start_col, end_col):
                                if mismatch_matrix[i, j].item():
                                    tile_errors += 1

                        tile_total = height * 32
                        error_rate = (tile_errors / tile_total) * 100

                        if error_rate > 10:
                            color = "\033[91m"  # Red
                        elif error_rate > 5:
                            color = "\033[93m"  # Yellow
                        else:
                            color = "\033[92m"  # Green

                        f.write(
                            f"{color}Tile {tile_x}: cols {start_col:3d}-{end_col-1:3d} "
                            f"→ {tile_errors:3d}/{tile_total} errors ({error_rate:5.1f}%)\033[0m\n"
                        )
                    f.write("\n")

                # Numerical comparison - compact view
                f.write("NUMERICAL SAMPLE (first 16 cols, format: golden→result):\n")
                f.write("-" * 80 + "\n")
                display_cols = min(16, width)

                # Header
                header = "    "
                for j in range(display_cols):
                    header += f"{j:>8d}"
                f.write(header + "\n")

                # Show sample rows
                sample_rows = (
                    min(height, 16) if height <= 32 else [0, 1, 2, 15, 16, 17, 30, 31]
                )
                if height <= 32:
                    sample_rows = list(range(min(height, 16)))

                for i in sample_rows:
                    row_str = f"{i:2d}: "
                    for j in range(display_cols):
                        golden_val = golden_matrix[i, j].item()
                        result_val = result_matrix[i, j].item()
                        is_mismatch = mismatch_matrix[i, j].item()

                        if is_mismatch:
                            cell = f"\033[91m{golden_val:3.1f}→{result_val:3.1f}\033[0m"
                        else:
                            cell = f"\033[92m{golden_val:3.1f}→{result_val:3.1f}\033[0m"

                        row_str += f"{cell:>8s}"

                    f.write(row_str + "\n")

                if height > 16 and isinstance(sample_rows, list):
                    f.write("    ... (middle rows omitted) ...\n")

                f.write("\n")

            except Exception as e:
                f.write(f"Matrix visualization failed: {e}\n")
                f.write("Falling back to linear format...\n\n")
                _write_linear_tensors(
                    f, golden_tensor, result_tensor, golden_tensor - result_tensor
                )
        else:
            f.write("Input dimensions not 2D, using linear format:\n\n")
            _write_linear_tensors(
                f, golden_tensor, result_tensor, golden_tensor - result_tensor
            )

        # Write footer
        f.write("=" * 80 + "\n")
        f.write("END OF DUMP\n")
        f.write("=" * 80 + "\n")

    # Print tile-by-tile analysis to console
    console_output = print_tile_by_tile_analysis(
        golden_tensor, result_tensor, test_params
    )

    # Also save console output to a separate file
    console_filepath = dump_dir / f"{test_name}_{timestamp}_console.txt"
    with open(console_filepath, "w") as cf:
        cf.write(console_output)

    print(f"\nConsole analysis saved to: {console_filepath}")

    return str(filepath)


def print_tile_by_tile_analysis(golden_tensor, result_tensor, test_params):
    """Print detailed tile-by-tile analysis to console and return the output as string."""
    input_dims = test_params.get("input_dimensions", None)
    if not input_dims or len(input_dims) != 2:
        msg = "Cannot perform tile analysis: input dimensions not available or not 2D"
        print(msg)
        return msg

    height, width = input_dims

    # Colors for console output
    RED = "\033[91m"
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    BLUE = "\033[94m"
    CYAN = "\033[96m"
    RESET = "\033[0m"
    BOLD = "\033[1m"

    output_lines = []  # Capture output for file saving

    try:
        golden_matrix = golden_tensor.view(height, width)
        result_matrix = result_tensor.view(height, width)

        # Calculate mismatches
        mismatches = ~torch.isclose(golden_tensor, result_tensor, rtol=0.05, atol=0.05)
        mismatch_matrix = mismatches.view(height, width)

        header = f"{CYAN}{'='*80}{RESET}"
        print(header)
        output_lines.append("=" * 80)

        title = f"{CYAN}TILE-BY-TILE ANALYSIS (32x32 tiles){RESET}"
        print(title)
        output_lines.append("TILE-BY-TILE ANALYSIS (32x32 tiles)")

        matrix_info = f"{CYAN}Matrix: {height}x{width}{RESET}"
        print(matrix_info)
        output_lines.append(f"Matrix: {height}x{width}")

        print(header)
        output_lines.append("=" * 80)

        # Calculate number of tiles
        tiles_y = (height + 31) // 32  # Round up
        tiles_x = (width + 31) // 32  # Round up

        for tile_y in range(tiles_y):
            for tile_x in range(tiles_x):
                # Calculate tile boundaries
                start_row = tile_y * 32
                end_row = min((tile_y + 1) * 32, height)
                start_col = tile_x * 32
                end_col = min((tile_x + 1) * 32, width)

                tile_height = end_row - start_row
                tile_width = end_col - start_col

                # Extract tile data
                tile_golden = golden_matrix[start_row:end_row, start_col:end_col]
                tile_result = result_matrix[start_row:end_row, start_col:end_col]
                tile_mismatches = mismatch_matrix[start_row:end_row, start_col:end_col]

                # Calculate tile statistics
                tile_total = tile_height * tile_width
                tile_errors = tile_mismatches.sum().item()
                error_rate = (tile_errors / tile_total) * 100

                # Color code based on error rate
                if error_rate > 10:
                    tile_color = RED
                    status = "HIGH ERROR"
                elif error_rate > 5:
                    tile_color = YELLOW
                    status = "MEDIUM ERROR"
                elif error_rate > 0:
                    tile_color = BLUE
                    status = "LOW ERROR"
                else:
                    tile_color = GREEN
                    status = "PERFECT"

                # Print tile header
                tile_header = (
                    f"\n{tile_color}{BOLD}TILE [{tile_y},{tile_x}]: rows {start_row:2d}-{end_row-1:2d}, cols {start_col:3d}-{end_col-1:3d} "
                    f"({tile_height}x{tile_width}) - {status} ({error_rate:5.1f}%){RESET}"
                )
                print(tile_header)
                output_lines.append(
                    f"\nTILE [{tile_y},{tile_x}]: rows {start_row:2d}-{end_row-1:2d}, cols {start_col:3d}-{end_col-1:3d} "
                    f"({tile_height}x{tile_width}) - {status} ({error_rate:5.1f}%)"
                )

                if error_rate > 0:  # Only show detailed view for tiles with errors
                    legend = f"Legend: {GREEN}●{RESET}=Match, {YELLOW}░{RESET}=Small diff, {RED}▓{RESET}=Med diff, {RED}█{RESET}=Large diff"
                    print(legend)
                    output_lines.append(
                        "Legend: ●=Match, ░=Small diff, ▓=Med diff, █=Large diff"
                    )

                    # Column header
                    col_header = "    "
                    col_header_plain = "    "
                    for j in range(0, tile_width, 4):  # Every 4th column
                        local_col = start_col + j
                        col_header += f"{local_col:>4d}"
                        col_header_plain += f"{local_col:>4d}"
                    print(col_header)
                    output_lines.append(col_header_plain)

                    # Print tile pattern map
                    for i in range(tile_height):
                        row_str = f"{start_row + i:2d}: "
                        row_str_plain = f"{start_row + i:2d}: "
                        for j in range(tile_width):
                            is_mismatch = tile_mismatches[i, j].item()
                            if is_mismatch:
                                golden_val = tile_golden[i, j].item()
                                result_val = tile_result[i, j].item()
                                abs_diff = abs(golden_val - result_val)

                                if abs_diff > 0.5:
                                    symbol = f"{RED}█{RESET}"
                                    symbol_plain = "█"
                                elif abs_diff > 0.1:
                                    symbol = f"{RED}▓{RESET}"
                                    symbol_plain = "▓"
                                else:
                                    symbol = f"{YELLOW}░{RESET}"
                                    symbol_plain = "░"
                            else:
                                symbol = f"{GREEN}●{RESET}"
                                symbol_plain = "●"

                            row_str += symbol
                            row_str_plain += symbol_plain
                        print(row_str)
                        output_lines.append(row_str_plain)

                    # Show some numerical examples for error tiles
                    if error_rate > 5:
                        samples_header = (
                            f"\n{YELLOW}Sample mismatches in this tile:{RESET}"
                        )
                        print(samples_header)
                        output_lines.append("\nSample mismatches in this tile:")

                        mismatch_count = 0
                        for i in range(tile_height):
                            for j in range(tile_width):
                                if tile_mismatches[i, j].item():
                                    golden_val = tile_golden[i, j].item()
                                    result_val = tile_result[i, j].item()
                                    global_row = start_row + i
                                    global_col = start_col + j

                                    sample_line = (
                                        f"  [{global_row:2d},{global_col:3d}]: {golden_val:6.3f} → {result_val:6.3f} "
                                        f"(diff: {abs(golden_val - result_val):6.3f})"
                                    )
                                    print(sample_line)
                                    output_lines.append(sample_line)

                                    mismatch_count += 1
                                    if (
                                        mismatch_count >= 5
                                    ):  # Limit to 5 examples per tile
                                        if tile_errors > 5:
                                            remaining_line = f"  ... and {tile_errors - 5} more in this tile"
                                            print(remaining_line)
                                            output_lines.append(remaining_line)
                                        break
                            if mismatch_count >= 5:
                                break
                else:
                    perfect_msg = f"  {GREEN}✓ Perfect tile - no errors{RESET}"
                    print(perfect_msg)
                    output_lines.append("  ✓ Perfect tile - no errors")

        footer = f"\n{CYAN}{'='*80}{RESET}"
        print(footer)
        output_lines.append("\n" + "=" * 80)

        return "\n".join(output_lines)

    except Exception as e:
        error_msg = f"{RED}Error in tile analysis: {e}{RESET}"
        print(error_msg)
        return f"Error in tile analysis: {e}"


def _write_linear_tensors(f, golden_tensor, result_tensor, diff_tensor):
    """Fallback linear format for tensor display."""
    f.write("FULL GOLDEN TENSOR (Linear):\n")
    f.write("-" * 80 + "\n")
    golden_list = golden_tensor.flatten().tolist()
    for i in range(0, len(golden_list), 8):
        line_vals = golden_list[i : i + 8]
        f.write(f"[{i:6d}] " + " ".join(f"{v:12.6f}" for v in line_vals) + "\n")
    f.write("\n")

    f.write("FULL RESULT TENSOR (Linear):\n")
    f.write("-" * 80 + "\n")
    result_list = result_tensor.flatten().tolist()
    for i in range(0, len(result_list), 8):
        line_vals = result_list[i : i + 8]
        f.write(f"[{i:6d}] " + " ".join(f"{v:12.6f}" for v in line_vals) + "\n")
    f.write("\n")
