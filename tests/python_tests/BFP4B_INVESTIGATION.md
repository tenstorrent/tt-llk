# Bfp4_b Datacopy Test Hang -- Investigation Report

## Status

- Python test infrastructure: **complete** (pack, unpack, format enum, inference, stimuli, golden model, tolerances)
- Hardware test on Blackhole: **skipped** (Tensix hang -- see root cause below)
- Existing tests: **339 passed, 0 regressions**

## Summary

The `test_eltwise_unary_datacopy` hangs with `TENSIX TIMED OUT` (all three cores: Unpacker, Math, Packer) when run with `DataFormat.Bfp4_b` on Blackhole. The Python test infrastructure changes are correct; the hang is caused by incomplete Blackhole hardware/firmware support for Bfp4_b in the unpack-datacopy-pack pipeline.

## What was verified as correct

### Python test infrastructure

All changes mirror the existing Bfp8_b pattern:

- `format_config.py` -- Enum member, `is_exponent_B()`, `num_bytes_per_tile()`, enum value maps (Bfp4_b = 7)
- `pack.py` -- `float_to_bfp4_block()` and `pack_bfp4_b()` produce correct output: 16 exponents + N/2 mantissa bytes (two 4-bit datums per byte)
- `unpack.py` -- `bfp4_to_float_block()` and `unpack_bfp4_b()` correctly expand packed bytes into 4-bit datums
- `llk_params.py` -- `format_dict[Bfp4_b] = torch.bfloat16`, `format_tile_sizes[Bfp4_b] = 576`
- `tile_constants.py` -- `calculate_tile_size_bytes` handles half-byte mantissas
- `stimuli_config.py`, `stimuli_generator.py`, `golden_generators.py`, `utils.py`, `constraints.py` -- all updated consistently

### Tile size calculation

Full 32x32 Bfp4_b tile: 64 exponent bytes + 512 mantissa bytes = **576 bytes**. This matches the C++ `GET_L1_HEADERLESS_TILE_SIZE(Bfp4_b)` which returns `(512 >> 4) + (64 >> 4) = 36` (in 16-byte units = 576 bytes).

### Format inference

For `Bfp4_b -> Bfp4_b` with `dest_acc=No`, all `FormatConfig` fields resolve to `Bfp4_b` -- identical pattern to `Bfp8_b -> Bfp8_b`. The C++ enum value 7 is correctly mapped in both `WORMHOLE_DATA_FORMAT_ENUM_VALUES` and `BLACKHOLE_DATA_FORMAT_ENUM_VALUES`.

### Runtime struct layout

The datacopy test uses `RUNTIME_FORMATS`. The ELF is shared across format variants (format values excluded from variant hash). Format values are written to the `RuntimeParams` struct in L1 at runtime. The struct layout is identical regardless of format values.

### C++ hardware code coverage

`Bfp4_b` appears in all relevant Blackhole switch statements:

- `GET_L1_HEADERLESS_TILE_SIZE` -- returns correct tile size
- `IS_BFP_FORMAT` -- returns true
- `llk_defs.h` `get_load_store_mode` -- returns `InstrModLoadStore::DEFAULT`
- `configure_unpack_AB` -- stride and exp_width calculations produce same values as Bfp8_b

### Data layout written to L1

For `num_faces=1, face_r_dim=16`: 144 bytes (16 exponents + 128 mantissa bytes for 256 elements). For `num_faces=4`: 576 bytes (full tile). Both match hardware expectations.

## Root cause: incomplete Blackhole Bfp4_b support

### Evidence

1. **DeepWiki documentation** states the library "primarily supports Bfp8_b format" for BFP -- Bfp4_b has enum entries but no existing test coverage.

2. **Wormhole has Bfp4-specific packer GPR initialization that Blackhole lacks.** In `tt_llk_wormhole_b0/common/inc/cpack_common.h`, the packer explicitly initializes `EXP1_SEC_SIZE_BFP4`, `EXP2_SEC_SIZE_BFP4`, `EXP3_SEC_SIZE_BFP4` GPRs with format-specific exponent section sizes (lines 181-186) and uses them during pack (lines 461-465). The Blackhole `cpack_common.h` defines these GPR constants in `ckernel_gpr_map.h` but **never initializes or references them**.

3. **All three Tensix cores time out**, indicating the Unpacker hangs first (cannot process Bfp4_b data from L1), causing a cascade: Math waits for unpacked data that never arrives, Packer waits for Math output that never arrives.

4. **The `SCALE_DATUM_SIZE` function does not handle sub-byte formats.** For Bfp4_b it falls through to `default: return datum_count` (1 byte per datum), but the actual storage is 0.5 bytes per datum. While this function is only used in tilize/untilize paths (not triggered by this test), it indicates Bfp4_b was not fully considered in the codebase.

### Likely specific cause

The Blackhole packer's `set_packer_config` function at `cpack_common.h:230` sets `exp_section_size = num_faces` for all BFP formats uniformly. However, the per-packer exponent section size GPRs (`EXP1_SEC_SIZE_BFP4` etc.) that the pack micro-operations reference during actual packing are **never initialized on Blackhole**. When the packer runs with `Bfp4_b`, it reads uninitialized GPR values for exponent section offsets, causing the packer to compute incorrect L1 write addresses or loop counts, resulting in a hang.

## Current mitigation

Bfp4_b tests are skipped on Blackhole in `test_eltwise_unary_datacopy.py` with a descriptive message. The skip condition checks if either input or output format is Bfp4_b and the chip architecture is Blackhole. Remove the skip once the C++ fix is in place.

## Recommended next steps

1. **Verify on Wormhole** -- The Wormhole `cpack_common.h` has complete Bfp4 packer GPR initialization. Test whether Bfp4_b works on a Wormhole machine to confirm the Python infrastructure is correct.

2. **Check with HW team** -- Confirm whether Blackhole is expected to support Bfp4_b in the standard unpack-math-pack pipeline, and whether the missing `EXP*_SEC_SIZE_BFP4` GPR initialization is a known gap.

3. **Port Wormhole Bfp4 packer init to Blackhole** -- If HW supports it, the fix would be adding `EXP*_SEC_SIZE_BFP4` GPR initialization to `tt_llk_blackhole/common/inc/cpack_common.h` in both `set_packer_config` and `reconfig_packer_data_format`, mirroring the Wormhole implementation. This is a C++ hardware file change.

4. **Remove skip** -- Once the C++ fix is confirmed working, remove the `pytest.skip` in `test_eltwise_unary_datacopy.py`.
