#!/usr/bin/env python3
# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0
"""
Analyze input_output_formats combinations vs Wormhole and Blackhole
is_unpacker_to_register_conversion_supported and is_packer_conversion_supported.
Finds any (input, output, dest_acc) that would hit the asserts.
Run from repo root: python tests/python_tests/analyze_format_combinations.py
"""
# Use test helpers (run from llk/tt-llk or set PYTHONPATH)
import sys
from itertools import product
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from helpers.chip_architecture import ChipArchitecture
from helpers.data_format_inference import infer_data_formats
from helpers.format_config import DataFormat
from helpers.llk_params import DestAccumulation

# ---------------------------------------------------------------------------
# Wormhole is_unpacker_to_register_conversion_supported (cunpack_common.h)
# (in_l1, out_reg) -> supported
# ---------------------------------------------------------------------------
WH_UNPACK = {
    (DataFormat.Float32, DataFormat.Float32),
    (DataFormat.Float32, DataFormat.Tf32),
    (DataFormat.Float32, DataFormat.Float16),
    (DataFormat.Float32, DataFormat.Float16_b),
    (DataFormat.Tf32, DataFormat.Tf32),
    (DataFormat.Tf32, DataFormat.Float32),
    (DataFormat.Float16, DataFormat.Float16),
    (DataFormat.Float16_b, DataFormat.Float16_b),
    (DataFormat.Bfp8, DataFormat.Float32),
    (DataFormat.Bfp8, DataFormat.Float16_b),
    (DataFormat.Bfp8, DataFormat.Float16),
    (DataFormat.Bfp8, DataFormat.Bfp8),
    (DataFormat.Bfp8_b, DataFormat.Float32),
    (DataFormat.Bfp8_b, DataFormat.Float16),
    (DataFormat.Bfp8_b, DataFormat.Float16_b),
    (DataFormat.Bfp8_b, DataFormat.Bfp8_b),
    (DataFormat.Int32, DataFormat.Int32),
    (DataFormat.UInt16, DataFormat.UInt16),
    (DataFormat.Int8, DataFormat.Int8),
    (DataFormat.UInt8, DataFormat.Int8),
}


def wormhole_unpack_supported(in_l1: DataFormat, out_reg: DataFormat) -> bool:
    return (in_l1, out_reg) in WH_UNPACK


# ---------------------------------------------------------------------------
# Wormhole is_packer_conversion_supported (cpack_common.h)
# (dest_reg, out_l1) -> supported. Bfp4/2 handled via is_bfp4_2_a/b.
# ---------------------------------------------------------------------------
WH_PACK_TABLE = {
    DataFormat.Float32: {
        DataFormat.Float32,
        DataFormat.Tf32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Tf32: {
        DataFormat.Float32,
        DataFormat.Tf32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Float16: {
        DataFormat.Float32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8,
    },
    DataFormat.Float16_b: {
        DataFormat.Float32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
    },
    DataFormat.Bfp8: {
        DataFormat.Float32,
        DataFormat.Float16,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Bfp8_b: {
        DataFormat.Float32,
        DataFormat.Float16_b,
        DataFormat.Bfp8_b,
    },
    DataFormat.Int32: {DataFormat.Int32, DataFormat.Int8},
    DataFormat.UInt16: {DataFormat.UInt16},
    DataFormat.Int8: {DataFormat.Int8},
}


def wormhole_pack_supported(dest_reg: DataFormat, out_l1: DataFormat) -> bool:
    allowed = WH_PACK_TABLE.get(dest_reg)
    if allowed is None:
        return False
    return out_l1 in allowed


# ---------------------------------------------------------------------------
# Blackhole is_unpacker_to_register_conversion_supported (cunpack_common.h)
# Float32/Tf32 as above; Float16, Bfp8, Bfp8_b (and group) -> Float16, Tf32, Float16_b only.
# ---------------------------------------------------------------------------
BH_UNPACK = {
    (DataFormat.Float32, DataFormat.Float32),
    (DataFormat.Float32, DataFormat.Tf32),
    (DataFormat.Float32, DataFormat.Float16),
    (DataFormat.Float32, DataFormat.Float16_b),
    (DataFormat.Tf32, DataFormat.Tf32),
    (DataFormat.Tf32, DataFormat.Float16),
    (DataFormat.Tf32, DataFormat.Float16_b),
    (DataFormat.Float16, DataFormat.Float16),
    (DataFormat.Float16, DataFormat.Tf32),
    (DataFormat.Float16, DataFormat.Float16_b),
    (DataFormat.Float16_b, DataFormat.Float16),
    (DataFormat.Float16_b, DataFormat.Tf32),
    (DataFormat.Float16_b, DataFormat.Float16_b),
    (DataFormat.Bfp8, DataFormat.Float16),
    (DataFormat.Bfp8, DataFormat.Tf32),
    (DataFormat.Bfp8, DataFormat.Float16_b),
    (DataFormat.Bfp8, DataFormat.Bfp8),
    (DataFormat.Bfp8_b, DataFormat.Float16),
    (DataFormat.Bfp8_b, DataFormat.Tf32),
    (DataFormat.Bfp8_b, DataFormat.Float16_b),
    (DataFormat.Bfp8_b, DataFormat.Bfp8_b),
    (DataFormat.Int32, DataFormat.Int32),
    (DataFormat.UInt16, DataFormat.UInt16),
    (DataFormat.Int8, DataFormat.Int8),
    (DataFormat.UInt8, DataFormat.UInt8),
}


def blackhole_unpack_supported(in_l1: DataFormat, out_reg: DataFormat) -> bool:
    return (in_l1, out_reg) in BH_UNPACK


# ---------------------------------------------------------------------------
# Blackhole is_packer_conversion_supported (cpack_common.h)
# ---------------------------------------------------------------------------
BH_PACK_TABLE = {
    DataFormat.Float32: {
        DataFormat.Float32,
        DataFormat.Tf32,
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Float16: {
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Float16_b: {
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Bfp8: {
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Bfp8_b: {
        DataFormat.Float16,
        DataFormat.Float16_b,
        DataFormat.Float32,
        DataFormat.Bfp8,
        DataFormat.Bfp8_b,
    },
    DataFormat.Int32: {DataFormat.Int32, DataFormat.Int8, DataFormat.UInt8},
    DataFormat.UInt16: {DataFormat.UInt16},
    DataFormat.Int8: {DataFormat.Int8},
    DataFormat.UInt8: {DataFormat.UInt8},
}


def blackhole_pack_supported(dest_reg: DataFormat, out_l1: DataFormat) -> bool:
    allowed = BH_PACK_TABLE.get(dest_reg)
    if allowed is None:
        return False
    return out_l1 in allowed


# ---------------------------------------------------------------------------
# Format sets used in tests (input_output_formats with same=False => each x each)
# ---------------------------------------------------------------------------
# Eltwise unary SFPU and others
FORMAT_SETS = [
    (
        "eltwise_unary_sfpu etc",
        [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Bfp8_b],
    ),
    (
        "with Float32",
        [
            DataFormat.Float16,
            DataFormat.Float16_b,
            DataFormat.Bfp8_b,
            DataFormat.Float32,
        ],
    ),
]


def run_analysis(chip_arch: ChipArchitecture):
    """Run analysis for one architecture. Returns list of (in_f, out_f, acc, kind, detail)."""
    unpack_ok = (
        wormhole_unpack_supported
        if chip_arch == ChipArchitecture.WORMHOLE
        else blackhole_unpack_supported
    )
    pack_ok = (
        wormhole_pack_supported
        if chip_arch == ChipArchitecture.WORMHOLE
        else blackhole_pack_supported
    )
    failures = []
    for set_name, formats in FORMAT_SETS:
        for (in_fmt, out_fmt), dest_acc in product(
            product(formats, formats),
            [DestAccumulation.Yes, DestAccumulation.No],
        ):
            try:
                cfg = infer_data_formats(
                    in_fmt,
                    out_fmt,
                    dest_acc,
                    unpacking_to_dest=False,
                    chip_arch=chip_arch,
                )
            except Exception as e:
                failures.append(
                    (in_fmt.name, out_fmt.name, dest_acc.name, "inference", str(e))
                )
                continue
            if not unpack_ok(cfg.unpack_A_src, cfg.unpack_A_dst):
                failures.append(
                    (
                        in_fmt.name,
                        out_fmt.name,
                        dest_acc.name,
                        "unpack",
                        f"({cfg.unpack_A_src.name} -> {cfg.unpack_A_dst.name})",
                    )
                )
            if not pack_ok(cfg.pack_src, cfg.pack_dst):
                failures.append(
                    (
                        in_fmt.name,
                        out_fmt.name,
                        dest_acc.name,
                        "pack",
                        f"({cfg.pack_src.name} -> {cfg.pack_dst.name})",
                    )
                )
    return failures


def main():
    print("=" * 80)
    print("Format combination analysis (unpack + pack asserts)")
    print("=" * 80)
    all_failures = {}
    for chip_arch in (ChipArchitecture.WORMHOLE, ChipArchitecture.BLACKHOLE):
        arch_name = chip_arch.name
        failures = run_analysis(chip_arch)
        all_failures[arch_name] = failures
        print(f"\n--- {arch_name} ---")
        if failures:
            print(f"  FAIL: {len(failures)} combination(s) would hit asserts")
        else:
            print("  OK: all analyzed combinations supported")
    print("\n" + "=" * 80)
    any_fail = any(all_failures.values())
    if any_fail:
        print("COMBINATIONS THAT WOULD HIT ASSERTS:")
        for arch_name, failures in all_failures.items():
            if failures:
                print(f"\n  [{arch_name}]")
                for in_f, out_f, acc, kind, detail in failures:
                    print(
                        f"    input={in_f} output={out_f} dest_acc={acc}  [{kind}] {detail}"
                    )
        print(
            f"\nTotal: Wormhole {len(all_failures['WORMHOLE'])}, Blackhole {len(all_failures['BLACKHOLE'])}"
        )
    else:
        print(
            "All analyzed combinations are supported on both Wormhole and Blackhole (no assert would fire)."
        )
    print("=" * 80)
    return 0 if not any_fail else 1


if __name__ == "__main__":
    sys.exit(main())
