# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch
from helpers.format_config import DataFormat, InputOutputFormat
from helpers.golden_generators import (
    BroadcastGolden,
    EltwiseBinaryGolden,
    TransposeGolden,
    get_golden_generator,
)
from helpers.llk_params import (
    BlocksCalculationAlgorithm,
    BroadcastType,
    DestAccumulation,
    DestSync,
    EltwiseBinaryReuseDestType,
    MathFidelity,
    MathOperation,
    Transpose,
    format_dict,
)
from helpers.param_config import (
    get_num_blocks_and_num_tiles_in_block,
    input_output_formats,
    parametrize,
)
from helpers.stimuli_config import StimuliConfig
from helpers.stimuli_generator import generate_stimuli_w_tile_dimensions
from helpers.test_config import TestConfig
from helpers.test_variant_parameters import (
    BROADCAST_TYPE,
    DEST_SYNC,
    EN_DEST_REUSE,
    MATH_FIDELITY,
    MATH_OP,
    NUM_BLOCKS,
    NUM_FACES_C_DIM,
    NUM_FACES_R_DIM,
    NUM_TILES_IN_BLOCK,
    REUSE_DEST_TYPE,
    TEST_FACE_DIMS,
    UNPACK_TRANS_FACES,
    UNPACK_TRANS_WITHIN_FACE,
)
from helpers.tile_constants import FACE_C_DIM, SUPPORTED_TILE_SIZES, get_tile_params
from helpers.tilize_untilize import tilize_block
from helpers.utils import passed_test

ALL_TILE_DIMENSIONS = [list(td) for td in SUPPORTED_TILE_SIZES]


def _get_valid_formats(dest_acc):
    """
    Filter formats based on dest accumulation:
    - If dest accumulation is enabled, input must be Float32
    """
    all_formats = input_output_formats(
        [
            DataFormat.Bfp8_b,
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],
        same=False,
    )
    if dest_acc == DestAccumulation.Yes:
        return [f for f in all_formats if f.input_format == DataFormat.Float32]
    return all_formats


def _get_valid_formats_include_bfp4_b(dest_acc):
    """
    Filter formats based on dest accumulation:
    - If dest accumulation is enabled, input must be Float32
    """
    all_formats = input_output_formats(
        [
            DataFormat.Bfp4_b,
            DataFormat.Bfp8_b,
            DataFormat.Float16_b,
            DataFormat.Float32,
        ],
        same=False,
    )
    if dest_acc == DestAccumulation.Yes:
        return [f for f in all_formats if f.input_format == DataFormat.Float32]
    return all_formats


def _get_valid_math_fidelity(formats, math_op=None):
    """
    Filter math fidelity based on input data format:
    - Bfp8_b: LoFi only
    - Float16_b: LoFi or HiFi2
    - Float32: HiFi3 and HiFi4

    Math fidelity > LoFi is only supported for Elwmul (hardware constraint),
    so non-multiply ops are restricted to LoFi regardless of format.
    """
    if math_op is not None and math_op != MathOperation.Elwmul:
        return [MathFidelity.LoFi]
    input_format = formats.input_format
    if input_format in [DataFormat.Bfp8_b, DataFormat.Bfp4_b]:
        return [MathFidelity.LoFi]
    elif input_format == DataFormat.Float16_b:
        return [MathFidelity.LoFi, MathFidelity.HiFi2]
    elif input_format == DataFormat.Float32:
        return [MathFidelity.HiFi3, MathFidelity.HiFi4]
    return [
        MathFidelity.LoFi,
        MathFidelity.HiFi2,
        MathFidelity.HiFi3,
        MathFidelity.HiFi4,
    ]


def _get_valid_math_ops(math_fidelity):
    """High fidelity operations are only supported for Elwmul."""
    if math_fidelity != MathFidelity.LoFi:
        return [MathOperation.Elwmul]
    return [MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul]


def _get_valid_transpose(broadcast_type):
    """Transpose does not work for Scalar broadcast."""
    if broadcast_type == BroadcastType.Scalar:
        return [Transpose.No]
    return [Transpose.No, Transpose.Yes]


def _get_valid_tile_dimensions(transpose_srca, broadcast_type):
    """
    Filter tile dimensions based on transpose and broadcast constraints:
    - Transpose only works for 32x32 tiles
    - 32x16 tiles are not supported for Column or Row broadcast
    """
    if transpose_srca == Transpose.Yes:
        return [[32, 32]]

    if broadcast_type in (BroadcastType.Column, BroadcastType.Row):
        return [td for td in ALL_TILE_DIMENSIONS if td != [32, 16]]

    return ALL_TILE_DIMENSIONS


@parametrize(
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    formats=lambda dest_acc: _get_valid_formats(dest_acc),
    broadcast_type=[
        BroadcastType.None_,
        BroadcastType.Row,
        BroadcastType.Column,
        BroadcastType.Scalar,
    ],
    math_fidelity=lambda formats: _get_valid_math_fidelity(formats),
    transpose_srca=lambda broadcast_type: _get_valid_transpose(broadcast_type),
    math_op=lambda math_fidelity: _get_valid_math_ops(math_fidelity),
    input_dimensions=[[256, 32]],
    tile_dimensions=lambda transpose_srca, broadcast_type: _get_valid_tile_dimensions(
        transpose_srca, broadcast_type
    ),
)
def test_eltwise_binary(
    dest_acc,
    formats,
    broadcast_type,
    math_fidelity,
    transpose_srca,
    math_op,
    input_dimensions,
    tile_dimensions,
    workers_tensix_coordinates,
):

    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)
    num_faces = num_faces_r_dim * num_faces_c_dim

    # Calculate tile count based on tile_dimensions (not hardcoded 32x32)
    tile_rows, tile_cols = tile_dimensions
    tile_cnt_A = (input_dimensions[0] // tile_rows) * (input_dimensions[1] // tile_cols)
    tile_cnt_B = tile_cnt_A

    # Generate stimuli with correct face dimensions for smaller tiles
    # Uses generate_stimuli_w_tile_dimensions which computes face_r_dim and num_faces from tile_dimensions
    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format_B,  # Use different format for src_B
        input_dimensions_B=input_dimensions,
        tile_dimensions=tile_dimensions,
    )

    effective_dest_acc = (
        DestAccumulation.Yes
        if formats.output_format == DataFormat.Float32
        else dest_acc
    )
    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        effective_dest_acc,
        formats,
        input_dimensions,
        tile_dimensions,
        BlocksCalculationAlgorithm.Standard,
    )

    # Compute element-wise subtraction in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)

    # Tilize inputs for device and golden calculation
    src_A_tilized = tilize_block(
        src_A,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )
    src_B_tilized = tilize_block(
        src_B,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )

    # Flatten tilized tensors
    src_A_tilized_flat = src_A_tilized.flatten()
    src_B_tilized_flat = src_B_tilized.flatten()

    # Send tilized data to device (device handles transpose during unpack)
    stimuli_A = src_A_tilized_flat
    stimuli_B = src_B_tilized_flat

    # Prepare golden src_A: apply tile-level transpose if enabled
    # Hardware does transpose_faces then transpose_within_faces during unpack
    golden_src_A = src_A_tilized_flat
    if transpose_srca == Transpose.Yes:
        transpose_golden = get_golden_generator(TransposeGolden)
        # Apply face transpose — also quantizes BFP formats to float16_b
        golden_src_A = transpose_golden.transpose_faces_multi_tile(
            src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=True,
            untilize=False,
            input_dimensions=tuple(input_dimensions),
        )
        # Apply within-face transpose on already-quantized data.
        # Use Float32 to match hardware source register precision (TF32)
        # and avoid double-quantization — hardware only quantizes once
        # during unpack, then transposes at full precision.
        golden_src_A = transpose_golden.transpose_within_faces_multi_tile(
            golden_src_A,
            (
                DataFormat.Float16_b
                if formats.input_format in [DataFormat.Bfp4_b, DataFormat.Bfp8_b]
                else formats.input_format
            ),
            num_tiles=tile_cnt_A,
            tilize=False,
            untilize=False,
            input_dimensions=tuple(input_dimensions),
        )

    # Prepare golden src_B: apply broadcast if enabled
    golden_src_B = src_B_tilized_flat
    if broadcast_type != BroadcastType.None_:
        broadcast_golden = get_golden_generator(BroadcastGolden)
        golden_src_B = broadcast_golden(
            broadcast_type,
            src_B_tilized_flat,
            formats.input_format,
            num_faces=num_faces,
            tile_cnt=tile_cnt_A,
            face_r_dim=face_r_dim,
        )

    # When transpose/broadcast already quantized an operand (BFP -> float16_b),
    # pass None to skip re-quantization in EltwiseBinaryGolden.
    golden_input_format_A = (
        None if transpose_srca == Transpose.Yes else formats.input_format
    )
    golden_input_format_B = (
        None if broadcast_type != BroadcastType.None_ else formats.input_format
    )
    golden_tensor = binary_golden(
        math_op,
        golden_src_A,
        golden_src_B,
        formats.output_format,
        math_fidelity,
        input_format=golden_input_format_A,
        input_format_B=golden_input_format_B,
    )

    configuration = TestConfig(
        "sources/eltwise_binary_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(broadcast_type),
            MATH_OP(mathop=math_op),
            DEST_SYNC(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(transpose_srca),
            UNPACK_TRANS_WITHIN_FACE(transpose_srca),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
            NUM_BLOCKS(num_blocks),
            NUM_FACES_R_DIM(num_faces_r_dim),
            NUM_FACES_C_DIM(num_faces_c_dim),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
        ],
        variant_stimuli=StimuliConfig(
            stimuli_A,
            formats.input_format,
            stimuli_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=tile_dimensions,
            use_dense_tile_dimensions=True,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Compare in tilized format
    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)
    assert test_passed, "Assert against golden failed"


@parametrize(
    dest_acc=[DestAccumulation.No, DestAccumulation.Yes],
    formats=[
        fmt
        for fmt in input_output_formats(
            [
                DataFormat.Bfp4_b,
                DataFormat.Bfp8_b,
                DataFormat.Float16_b,
                DataFormat.Float32,
            ]
        )
        if fmt.input_format == DataFormat.Bfp4_b
        or fmt.output_format == DataFormat.Bfp4_b
    ],
    broadcast_type=[
        BroadcastType.None_,
        BroadcastType.Row,
        BroadcastType.Column,
        BroadcastType.Scalar,
    ],
    math_op=[MathOperation.Elwmul, MathOperation.Elwadd, MathOperation.Elwsub],
    math_fidelity=lambda formats, math_op: _get_valid_math_fidelity(formats, math_op),
    transpose_srca=[Transpose.Yes, Transpose.No],
    input_dimensions=[[32, 32], [64, 32], [32, 64], [256, 32]],
    tile_dimensions=lambda transpose_srca, broadcast_type: _get_valid_tile_dimensions(
        transpose_srca, broadcast_type
    ),
)
def test_eltwise_binary_bfp4_b(
    dest_acc,
    formats,
    broadcast_type,
    math_op,
    math_fidelity,
    transpose_srca,
    input_dimensions,
    tile_dimensions,
    workers_tensix_coordinates,
):
    if transpose_srca == Transpose.Yes and broadcast_type == BroadcastType.Scalar:
        pytest.skip("SrcA transpose is not supported with scalar broadcast")

    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)
    num_faces = num_faces_r_dim * num_faces_c_dim

    # Calculate tile count based on tile_dimensions (not hardcoded 32x32)
    tile_rows, tile_cols = tile_dimensions
    tile_cnt_A = (input_dimensions[0] // tile_rows) * (input_dimensions[1] // tile_cols)
    tile_cnt_B = tile_cnt_A

    # Generate stimuli with correct face dimensions for smaller tiles
    # Uses generate_stimuli_w_tile_dimensions which computes face_r_dim and num_faces from tile_dimensions
    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format_B,  # Use different format for src_B
        input_dimensions_B=input_dimensions,
        tile_dimensions=tile_dimensions,
    )

    effective_dest_acc = (
        DestAccumulation.Yes
        if formats.output_format == DataFormat.Float32
        else dest_acc
    )
    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        effective_dest_acc,
        formats,
        input_dimensions,
        tile_dimensions,
        BlocksCalculationAlgorithm.Standard,
    )

    # Compute element-wise subtraction in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)

    # Tilize inputs for device and golden calculation
    src_A_tilized = tilize_block(
        src_A,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )
    src_B_tilized = tilize_block(
        src_B,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )

    # Flatten tilized tensors
    src_A_tilized_flat = src_A_tilized.flatten()
    src_B_tilized_flat = src_B_tilized.flatten()

    # Send tilized data to device (device handles transpose during unpack)
    stimuli_A = src_A_tilized_flat
    stimuli_B = src_B_tilized_flat

    # Prepare golden src_A: apply tile-level transpose if enabled
    # Hardware does transpose_faces then transpose_within_faces during unpack
    golden_src_A = src_A_tilized_flat
    if transpose_srca == Transpose.Yes:
        transpose_golden = get_golden_generator(TransposeGolden)
        # Apply face transpose — also quantizes BFP formats to float16_b
        golden_src_A = transpose_golden.transpose_faces_multi_tile(
            src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=True,
            untilize=False,
            input_dimensions=tuple(input_dimensions),
        )
        # Apply within-face transpose on already-quantized data.
        # Use Float32 to match hardware source register precision (TF32)
        # and avoid double-quantization — hardware only quantizes once
        # during unpack, then transposes at full precision.
        golden_src_A = transpose_golden.transpose_within_faces_multi_tile(
            golden_src_A,
            (
                DataFormat.Float16_b
                if formats.input_format in [DataFormat.Bfp4_b, DataFormat.Bfp8_b]
                else formats.input_format
            ),
            num_tiles=tile_cnt_A,
            tilize=False,
            untilize=False,
            input_dimensions=tuple(input_dimensions),
        )

    # Prepare golden src_B: apply broadcast if enabled
    golden_src_B = src_B_tilized_flat
    if broadcast_type != BroadcastType.None_:
        broadcast_golden = get_golden_generator(BroadcastGolden)
        golden_src_B = broadcast_golden(
            broadcast_type,
            src_B_tilized_flat,
            formats.input_format,
            num_faces=num_faces,
            tile_cnt=tile_cnt_A,
            face_r_dim=face_r_dim,
        )

    # When transpose/broadcast already quantized an operand (BFP -> float16_b),
    # pass None to skip re-quantization in EltwiseBinaryGolden.
    golden_input_format_A = (
        None if transpose_srca == Transpose.Yes else formats.input_format
    )
    golden_input_format_B = (
        None if broadcast_type != BroadcastType.None_ else formats.input_format
    )
    golden_tensor = binary_golden(
        math_op,
        golden_src_A,
        golden_src_B,
        formats.output_format,
        math_fidelity,
        input_format=golden_input_format_A,
        input_format_B=golden_input_format_B,
    )

    configuration = TestConfig(
        "sources/eltwise_binary_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(broadcast_type),
            MATH_OP(mathop=math_op),
            DEST_SYNC(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(transpose_srca),
            UNPACK_TRANS_WITHIN_FACE(transpose_srca),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
            NUM_BLOCKS(num_blocks),
            NUM_FACES_R_DIM(num_faces_r_dim),
            NUM_FACES_C_DIM(num_faces_c_dim),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
        ],
        variant_stimuli=StimuliConfig(
            stimuli_A,
            formats.input_format,
            stimuli_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=tile_dimensions,
            use_dense_tile_dimensions=True,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result tensor ({len(res_from_L1)}) and golden tensor ({len(golden_tensor)}) are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)
    if not test_passed:
        print("\n=== DEBUG: test_eltwise_binary_bfp4_b FAILED ===")
        print(
            f"  broadcast_type={broadcast_type}  math_op={math_op}  math_fidelity={math_fidelity}"
        )
        print(
            f"  formats={formats.input_format}->{formats.output_format}  tile_dimensions={tile_dimensions}"
        )
        print(f"  input_dimensions={input_dimensions}  dest_acc={dest_acc}")
        torch.set_printoptions(linewidth=200, precision=9, sci_mode=True)
        print(f"\n--- src_A_tilized_flat (first input, first 32 elements) ---")
        print(src_A_tilized_flat[:32])
        if transpose_srca == Transpose.Yes:
            print(f"\n--- golden_src_A (after transpose, first 32 elements) ---")
            print(golden_src_A[:32])
        print(
            f"\n--- src_B_tilized_flat (second input, raw before broadcast, first 32 elements) ---"
        )
        print(src_B_tilized_flat[:32])
        if broadcast_type != BroadcastType.None_:
            print(f"\n--- golden_src_B (after broadcast, first 32 elements) ---")
            print(golden_src_B[:32])
        print(f"\n--- golden_tensor (expected output, first 32 elements) ---")
        print(golden_tensor[:32])
        print(f"\n--- res_tensor (device output, first 32 elements) ---")
        print(res_tensor[:32])
        diff = (golden_tensor.float() - res_tensor.float()).abs()
        rel_diff = diff / (golden_tensor.float().abs() + 1e-10)
        print(f"\n--- abs diff (max={diff.max():.6f}, mean={diff.mean():.6f}) ---")
        print(
            f"--- rel diff (max={rel_diff.max():.6f}, mean={rel_diff.mean():.6f}) ---"
        )
        # Show first differing block
        nonzero_idx = diff.nonzero(as_tuple=True)[0]
        if len(nonzero_idx) > 0:
            print(
                f"  first {len(nonzero_idx)} nonzero diffs at indices: {nonzero_idx[:20].tolist()}"
            )
            print(f"  golden at those: {golden_tensor[nonzero_idx[:20]].tolist()}")
            print(f"  result at those: {res_tensor[nonzero_idx[:20]].tolist()}")
        else:
            print(
                "  all diffs are zero -- failure is in _bfp4_block_aware_compare ULP logic"
            )
            # Rerun compare with verbose block-level output
            import math as _math

            g = golden_tensor.float().flatten()
            r = res_tensor.float().flatten()
            BLOCK = 16
            for blk in range(0, len(g), BLOCK):
                g_blk = g[blk : blk + BLOCK]
                r_blk = r[blk : blk + BLOCK]
                diff_blk = (g_blk - r_blk).abs()
                block_max = max(g_blk.abs().max().item(), r_blk.abs().max().item())
                if block_max == 0:
                    ok = torch.isclose(g_blk, r_blk, atol=1e-5, rtol=0.0).all().item()
                    if not ok:
                        print(
                            f"  FAIL block {blk//BLOCK} (block_max=0): g={g_blk.tolist()} r={r_blk.tolist()}"
                        )
                else:
                    block_exp = _math.floor(_math.log2(block_max))
                    one_ulp = 2.0 ** (block_exp - 3 + 1)
                    ulp_ok = (diff_blk <= one_ulp).all().item()
                    if not ulp_ok:
                        print(
                            f"  FAIL block {blk//BLOCK} (block_max={block_max:.4f}, 1ulp={one_ulp:.6f}): max_diff={diff_blk.max():.6f}"
                        )
                        print(f"    g={g_blk.tolist()}")
                        print(f"    r={r_blk.tolist()}")
        torch.set_printoptions(profile="default")
    assert test_passed, "Assert against golden failed"


@parametrize(
    reuse_dest_type=[
        EltwiseBinaryReuseDestType.DEST_TO_SRCA,
        EltwiseBinaryReuseDestType.DEST_TO_SRCB,
    ],
    formats=lambda: input_output_formats(
        [DataFormat.Float16_b, DataFormat.Float32, DataFormat.Bfp8_b],
        same=True,
    ),
    math_fidelity=[MathFidelity.LoFi],
    math_op=[MathOperation.Elwadd, MathOperation.Elwsub, MathOperation.Elwmul],
    input_dimensions=[[512, 32]],
    output_dimensions=[[128, 32]],
    tile_dimensions=[[32, 32], [16, 32]],
)
def test_eltwise_binary_dest_reuse(
    reuse_dest_type,
    formats,
    math_fidelity,
    math_op,
    input_dimensions,
    output_dimensions,
    tile_dimensions,
    workers_tensix_coordinates,
):
    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)
    num_faces = num_faces_r_dim * num_faces_c_dim

    tile_rows, tile_cols = tile_dimensions
    tile_cnt_input = (input_dimensions[0] // tile_rows) * (
        input_dimensions[1] // tile_cols
    )
    tile_cnt_output = (output_dimensions[0] // tile_rows) * (
        output_dimensions[1] // tile_cols
    )

    assert tile_cnt_input % tile_cnt_output == 0, (
        f"Input tile count ({tile_cnt_input}) must be divisible by "
        f"output tile count ({tile_cnt_output})"
    )

    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format,
        input_dimensions_B=input_dimensions,
        tile_dimensions=tile_dimensions,
    )

    # Compute block/tile counts for output (determines dest register blocking)
    effective_dest_acc = (
        DestAccumulation.Yes
        if formats.output_format == DataFormat.Float32
        else DestAccumulation.No
    )
    output_num_blocks, output_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        effective_dest_acc,
        formats,
        output_dimensions,
        tile_dimensions,
        BlocksCalculationAlgorithm.Standard,
    )

    # Input has the same block count, but more tiles per block
    inner_dim = tile_cnt_input // tile_cnt_output
    input_tiles_in_block = inner_dim * output_tiles_in_block
    input_num_blocks = output_num_blocks

    # Tilize inputs
    src_A_tilized = tilize_block(
        src_A,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )
    src_B_tilized = tilize_block(
        src_B,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )

    src_A_tilized_flat = src_A_tilized.flatten()
    src_B_tilized_flat = src_B_tilized.flatten()

    stimuli_A = src_A_tilized_flat
    stimuli_B = src_B_tilized_flat

    # Golden: simulate dest reuse.
    # move_d2a/move_d2b copies old_dest into srcA/srcB (replacing the unpacked value).
    # ELWADD/ELWSUB: new_dest = srcA op srcB  (overwrites dest)
    # ELWMUL:        new_dest = old_dest + srcA * srcB  (always accumulates)
    tile_elements = num_faces * face_r_dim * FACE_C_DIM
    torch_format = format_dict[formats.output_format]
    golden_tensor = torch.zeros(tile_cnt_output * tile_elements, dtype=torch_format)

    for out_t in range(tile_cnt_output):
        block_idx = out_t // output_tiles_in_block
        tile_in_block = out_t % output_tiles_in_block
        dest = torch.zeros(tile_elements, dtype=torch_format)

        for i in range(inner_dim):
            input_tile_idx = (
                block_idx * input_tiles_in_block
                + i * output_tiles_in_block
                + tile_in_block
            )
            start = input_tile_idx * tile_elements
            end = start + tile_elements

            a_tile = src_A_tilized_flat[start:end].to(torch_format)
            b_tile = src_B_tilized_flat[start:end].to(torch_format)

            if reuse_dest_type == EltwiseBinaryReuseDestType.DEST_TO_SRCA:
                srcA, srcB = dest.clone(), b_tile
            else:
                srcA, srcB = a_tile, dest.clone()

            if math_op == MathOperation.Elwadd:
                dest = srcA + srcB
            elif math_op == MathOperation.Elwsub:
                dest = srcA - srcB
            elif math_op == MathOperation.Elwmul:
                dest = dest + srcA * srcB

        out_start = out_t * tile_elements
        golden_tensor[out_start : out_start + tile_elements] = dest

    configuration = TestConfig(
        "sources/eltwise_binary_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(BroadcastType.None_),
            MATH_OP(mathop=math_op),
            DEST_SYNC(),
            EN_DEST_REUSE(),
            REUSE_DEST_TYPE(reuse_dest_type=reuse_dest_type),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(Transpose.No),
            UNPACK_TRANS_WITHIN_FACE(Transpose.No),
            NUM_TILES_IN_BLOCK(
                output_tiles_in_block,
                input_num_tiles_in_block=input_tiles_in_block,
                output_num_tiles_in_block=output_tiles_in_block,
            ),
            NUM_BLOCKS(
                output_num_blocks,
                input_num_blocks=input_num_blocks,
                output_num_blocks=output_num_blocks,
            ),
            NUM_FACES_R_DIM(num_faces_r_dim),
            NUM_FACES_C_DIM(num_faces_c_dim),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
        ],
        variant_stimuli=StimuliConfig(
            stimuli_A,
            formats.input_format,
            stimuli_B,
            formats.input_format,
            formats.output_format,
            tile_count_A=tile_cnt_input,
            tile_count_B=tile_cnt_input,
            tile_count_res=tile_cnt_output,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=tile_dimensions,
            use_dense_tile_dimensions=True,
        ),
        dest_acc=DestAccumulation.No,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    test_passed = passed_test(golden_tensor, res_tensor, formats.output_format)
    assert test_passed, "Assert against golden failed"


@parametrize(
    dest_acc=[DestAccumulation.Yes],  # Dest accumulation is required for int8.
    formats=InputOutputFormat(DataFormat.Int8, DataFormat.Int8),
    broadcast_type=[
        BroadcastType.None_,
    ],
    math_fidelity=MathFidelity.LoFi,
    transpose_srca=Transpose.No,
    math_op=[MathOperation.Elwadd, MathOperation.Elwsub],
    input_dimensions=[[32, 32], [512, 32]],
    tile_dimensions=lambda transpose_srca, broadcast_type: _get_valid_tile_dimensions(
        transpose_srca, broadcast_type
    ),
)
def test_eltwise_binary_int8_format(
    dest_acc,
    formats,
    broadcast_type,
    math_fidelity,
    transpose_srca,
    math_op,
    input_dimensions,
    tile_dimensions,
    workers_tensix_coordinates,
):
    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)
    num_faces = num_faces_r_dim * num_faces_c_dim

    # Calculate tile count based on tile_dimensions (not hardcoded 32x32)
    tile_rows, tile_cols = tile_dimensions
    tile_cnt_A = (input_dimensions[0] // tile_rows) * (input_dimensions[1] // tile_cols)
    tile_cnt_B = tile_cnt_A

    # Generate stimuli with correct face dimensions for smaller tiles
    # Uses generate_stimuli_w_tile_dimensions which computes face_r_dim and num_faces from tile_dimensions
    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format_B,  # Use different format for src_B
        input_dimensions_B=input_dimensions,
        tile_dimensions=tile_dimensions,
        negative_values=True,  # Test eltwadd with negative values,
    )

    # Use modulo to get even distribution in range -50 to +50 (avoids bunching at boundaries and avoid overflow so we can test exact results against golden)
    src_A = (src_A % 101) - 50
    src_B = (src_B % 101) - 50

    effective_dest_acc = (
        DestAccumulation.Yes
        if formats.output_format == DataFormat.Float32
        else dest_acc
    )
    num_blocks, num_tiles_in_block = get_num_blocks_and_num_tiles_in_block(
        DestSync.Half,
        effective_dest_acc,
        formats,
        input_dimensions,
        tile_dimensions,
        BlocksCalculationAlgorithm.Standard,
    )

    # Compute eltwise binary operation (add and sub) in tilized format
    binary_golden = get_golden_generator(EltwiseBinaryGolden)

    # Tilize inputs for device and golden calculation
    src_A_tilized = tilize_block(
        src_A,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )
    src_B_tilized = tilize_block(
        src_B,
        dimensions=input_dimensions,
        stimuli_format=formats.input_format,
        num_faces=num_faces,
        tile_dimensions=tile_dimensions,
        face_r_dim=face_r_dim,
    )

    # Flatten tilized tensors
    src_A_tilized_flat = src_A_tilized.flatten()
    src_B_tilized_flat = src_B_tilized.flatten()

    # Send tilized data to device (device handles transpose during unpack)
    stimuli_A = src_A_tilized_flat
    stimuli_B = src_B_tilized_flat

    # Prepare golden src_A: apply tile-level transpose if enabled
    # Hardware does transpose_faces then transpose_within_faces during unpack
    golden_src_A = src_A_tilized_flat

    # Prepare golden src_B: apply broadcast if enabled
    golden_src_B = src_B_tilized_flat

    # Compute golden on tilized data
    golden_tensor = binary_golden(
        math_op,
        golden_src_A,
        golden_src_B,
        formats.output_format,
        math_fidelity,
        input_format=formats.input_format,
    )

    configuration = TestConfig(
        "sources/eltwise_binary_test.cpp",
        formats,
        templates=[
            MATH_FIDELITY(math_fidelity),
            BROADCAST_TYPE(broadcast_type),
            MATH_OP(mathop=math_op),
            DEST_SYNC(),
        ],
        runtimes=[
            UNPACK_TRANS_FACES(transpose_srca),
            UNPACK_TRANS_WITHIN_FACE(transpose_srca),
            NUM_TILES_IN_BLOCK(num_tiles_in_block),
            NUM_BLOCKS(num_blocks),
            NUM_FACES_R_DIM(num_faces_r_dim),
            NUM_FACES_C_DIM(num_faces_c_dim),
            TEST_FACE_DIMS(face_r_dim=face_r_dim),
        ],
        variant_stimuli=StimuliConfig(
            stimuli_A,
            formats.input_format,
            stimuli_B,
            formats.input_format_B,
            formats.output_format,
            tile_count_A=tile_cnt_A,
            tile_count_B=tile_cnt_B,
            tile_count_res=tile_cnt_A,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
            tile_dimensions=tile_dimensions,
            use_dense_tile_dimensions=True,
        ),
        dest_acc=dest_acc,
        unpack_to_dest=False,
    )

    res_from_L1 = configuration.run(workers_tensix_coordinates).result

    assert len(res_from_L1) == len(
        golden_tensor
    ), "Result tensor and golden tensor are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # Compare in tilized format
    test_passed = passed_test(
        golden_tensor, res_tensor, formats.output_format, print_errors=False
    )
    assert test_passed, "Assert against golden failed"
