# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
# SPDX-License-Identifier: Apache-2.0

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

BFP_BLOCK_SIZE = 16


def _print_bfp_mismatch_blocks(
    golden_tensor: "torch.Tensor",
    res_tensor: "torch.Tensor",
    src_a: "torch.Tensor | None" = None,
    golden_src_b: "torch.Tensor | None" = None,
):
    """Print BFP blocks (shared exponent + mantissas in binary) for every block
    that contains at least one mismatch between golden and result.

    Optionally also shows src_A, golden_src_B and their sum alongside each element.
    """
    import struct

    g_flat = golden_tensor.float().flatten()
    r_flat = res_tensor.float().flatten()
    n = g_flat.numel()

    a_flat = src_a.float().flatten() if src_a is not None else None
    b_flat = golden_src_b.float().flatten() if golden_src_b is not None else None
    has_inputs = a_flat is not None and b_flat is not None

    def float_to_bfp_parts(val: float):
        """Return (sign, exponent, mantissa_bits) from a float32 value."""
        packed = struct.pack(">f", val)
        bits = int.from_bytes(packed, "big")
        sign = (bits >> 31) & 1
        exp = (bits >> 23) & 0xFF
        mant = bits & 0x7FFFFF
        return sign, exp, mant

    mismatch_found = False
    for blk_start in range(0, n, BFP_BLOCK_SIZE):
        blk_end = min(blk_start + BFP_BLOCK_SIZE, n)
        g_blk = g_flat[blk_start:blk_end]
        r_blk = r_flat[blk_start:blk_end]

        if torch.equal(g_blk, r_blk):
            continue

        if not mismatch_found:
            print("\n" + "=" * 120)
            print("BFP MISMATCH BLOCKS (shared exponent + mantissas in binary)")
            print("=" * 120)
            mismatch_found = True

        # Compute shared exponent for golden and result blocks independently
        g_exps = [float_to_bfp_parts(v)[1] for v in g_blk.tolist()]
        r_exps = [float_to_bfp_parts(v)[1] for v in r_blk.tolist()]
        g_shared_exp = max(g_exps)
        r_shared_exp = max(r_exps)

        print(f"\nBlock [{blk_start}:{blk_end}]")
        print(f"  Golden shared exp : {g_shared_exp} (0b{g_shared_exp:08b})")
        print(f"  Result shared exp : {r_shared_exp} (0b{r_shared_exp:08b})")

        header = (
            f"  {'Idx':>4}  {'Golden value':>14}  {'Golden bits (s exp mant)':>34}  "
            f"{'Result value':>14}  {'Result bits (s exp mant)':>34}  {'Match':>8}"
        )
        if has_inputs:
            header += f"  {'src_A':>14}  {'golden_src_B':>14}  {'A+B':>14}"
        print(header)
        print("  " + "-" * (115 + (50 if has_inputs else 0)))

        a_blk = a_flat[blk_start:blk_end] if has_inputs else None
        b_blk = b_flat[blk_start:blk_end] if has_inputs else None

        for i, (gv, rv) in enumerate(zip(g_blk.tolist(), r_blk.tolist())):
            g_sign, g_exp, g_mant = float_to_bfp_parts(gv)
            r_sign, r_exp, r_mant = float_to_bfp_parts(rv)
            match = "OK" if gv == rv else "MISMATCH"
            g_bits = f"{'1' if g_sign else '0'} {g_exp:08b} {g_mant:023b}"
            r_bits = f"{'1' if r_sign else '0'} {r_exp:08b} {r_mant:023b}"
            line = (
                f"  [{blk_start + i:4d}]  {gv:>14.6f}  {g_bits}  "
                f"{rv:>14.6f}  {r_bits}  {match:>8}"
            )
            if has_inputs:
                av = a_blk[i].item()
                bv = b_blk[i].item()
                line += f"  {av:>14.6f}  {bv:>14.6f}  {av + bv:>14.6f}"
            print(line)

    if mismatch_found:
        print("=" * 120 + "\n")


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
        # Apply face transpose (f0,f1,f2,f3 -> f0,f2,f1,f3)
        golden_src_A = transpose_golden.transpose_faces_multi_tile(
            src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=True,
            untilize=False,  # Keep tilized
            input_dimensions=tuple(input_dimensions),
        )
        # Apply within-face transpose (transpose each 16x16 face)
        golden_src_A = transpose_golden.transpose_within_faces_multi_tile(
            golden_src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=False,  # Already tilized
            untilize=False,  # Keep tilized for golden comparison
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

    # Compute golden on tilized data
    # When broadcast is applied, BroadcastGolden already quantized golden_src_B
    # (Bfp4_b/Bfp8_b -> float16_b), so we must not re-quantize it here.
    golden_input_format_B = (
        None if broadcast_type != BroadcastType.None_ else formats.input_format
    )
    golden_tensor = binary_golden(
        math_op,
        golden_src_A,
        golden_src_B,
        formats.output_format,
        math_fidelity,
        input_format=formats.input_format,
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
    if not test_passed:
        _print_bfp_mismatch_blocks(
            golden_tensor, res_tensor, src_a=golden_src_A, golden_src_b=golden_src_B
        )
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
                # DataFormat.Float32,
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
    math_op=[MathOperation.Elwadd, MathOperation.Elwsub],
    math_fidelity=lambda formats, math_op: _get_valid_math_fidelity(formats, math_op),
    transpose_srca=Transpose.No,
    input_dimensions=[[32, 32], [64, 32], [32, 64], [256, 32]],
    # input_dimensions=[[32, 32]],
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
    # import os
    # from io import StringIO

    # DEBUG_BFP4 = os.environ.get("DEBUG_BFP4", "1") == "1"
    # debug_buffer = StringIO()

    # def debug_print(title, data=None, tensor=None, max_items=64):
    # output = f"\n{'='*80}\n"
    # output += f"DEBUG: {title}\n"
    # output += f"{'='*80}\n"
    # if data is not None:
    #     output += f"{data}\n"
    # if tensor is not None:
    #     if isinstance(tensor, torch.Tensor):
    #         flat = tensor.flatten()
    #         output += f"  Shape: {tensor.shape}, Dtype: {tensor.dtype}\n"
    #         output += f"  Min: {flat.min():.4f}, Max: {flat.max():.4f}, Mean: {flat.mean():.4f}\n"
    #         output += f"  First {min(max_items, len(flat))} values: {flat[:max_items].tolist()}\n"
    #     else:
    #         output += f"  Data: {tensor}\n"
    # output += f"{'='*80}\n"
    # debug_buffer.write(output)
    # if DEBUG_BFP4:
    #     print(output, end="")

    face_r_dim, num_faces_r_dim, num_faces_c_dim = get_tile_params(tile_dimensions)
    num_faces = num_faces_r_dim * num_faces_c_dim

    # Calculate tile count based on tile_dimensions (not hardcoded 32x32)
    tile_rows, tile_cols = tile_dimensions
    tile_cnt_A = (input_dimensions[0] // tile_rows) * (input_dimensions[1] // tile_cols)
    tile_cnt_B = tile_cnt_A

    # debug_print(
    #     "TEST CONFIGURATION",
    #     f"formats: {formats}, broadcast_type: {broadcast_type}, math_op: {math_op}\n"
    #     f"math_fidelity: {math_fidelity}, transpose_srca: {transpose_srca}\n"
    #     f"input_dimensions: {input_dimensions}, tile_dimensions: {tile_dimensions}\n"
    #     f"face_r_dim: {face_r_dim}, num_faces: {num_faces}, tile_cnt: {tile_cnt_A}",
    # )

    # Generate stimuli with correct face dimensions for smaller tiles
    # Uses generate_stimuli_w_tile_dimensions which computes face_r_dim and num_faces from tile_dimensions
    src_A, _, src_B, _ = generate_stimuli_w_tile_dimensions(
        stimuli_format_A=formats.input_format,
        input_dimensions_A=input_dimensions,
        stimuli_format_B=formats.input_format_B,  # Use different format for src_B
        input_dimensions_B=input_dimensions,
        tile_dimensions=tile_dimensions,
    )

    # debug_print("GENERATED STIMULI - src_A", tensor=src_A, max_items=128)
    # debug_print("GENERATED STIMULI - src_B", tensor=src_B, max_items=128)

    # Print some sample values from src_B for scalar broadcast debugging
    # if broadcast_type == BroadcastType.Scalar:
    #     print(
    #         f"\n>>> SCALAR BROADCAST CHECK: src_B first 16 values: {src_B[:16].tolist()}"
    #     )
    #     print(
    #         f">>> SCALAR BROADCAST CHECK: src_B tilized first 16 values will be computed next..."
    #     )

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

    # debug_print("TILIZED src_A", tensor=src_A_tilized, max_items=128)
    # debug_print("TILIZED src_B", tensor=src_B_tilized, max_items=128)

    # Flatten tilized tensors
    src_A_tilized_flat = src_A_tilized.flatten()
    src_B_tilized_flat = src_B_tilized.flatten()

    # debug_print("TILIZED FLAT src_A", tensor=src_A_tilized_flat, max_items=128)
    # debug_print("TILIZED FLAT src_B", tensor=src_B_tilized_flat, max_items=128)

    # if broadcast_type == BroadcastType.Scalar:
    #     print(
    #         f">>> SCALAR BROADCAST CHECK: src_B_tilized_flat first 16 values: {src_B_tilized_flat[:16].tolist()}"
    #     )

    # Send tilized data to device (device handles transpose during unpack)
    stimuli_A = src_A_tilized_flat
    stimuli_B = src_B_tilized_flat

    # Prepare golden src_A: apply tile-level transpose if enabled
    # Hardware does transpose_faces then transpose_within_faces during unpack
    golden_src_A = src_A_tilized_flat
    if transpose_srca == Transpose.Yes:
        transpose_golden = get_golden_generator(TransposeGolden)
        # Apply face transpose (f0,f1,f2,f3 -> f0,f2,f1,f3)
        golden_src_A = transpose_golden.transpose_faces_multi_tile(
            src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=True,
            untilize=False,  # Keep tilized
            input_dimensions=tuple(input_dimensions),
        )
        # Apply within-face transpose (transpose each 16x16 face)
        golden_src_A = transpose_golden.transpose_within_faces_multi_tile(
            golden_src_A,
            formats.input_format,
            num_tiles=tile_cnt_A,
            tilize=False,  # Already tilized
            untilize=False,  # Keep tilized for golden comparison
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

    # debug_print(
    #     "GOLDEN src_A (after transpose if any)", tensor=golden_src_A, max_items=128
    # )
    # debug_print(
    #     "GOLDEN src_B (after broadcast if any)", tensor=golden_src_B, max_items=128
    # )
    # debug_print("BROADCAST TYPE", data=f"{broadcast_type}")

    # Compute golden on tilized data
    # When broadcast is applied, BroadcastGolden already quantized golden_src_B
    # (Bfp4_b/Bfp8_b -> float16_b), so we must not re-quantize it here.
    # golden_input_format_B = None if broadcast_type != BroadcastType.None_ else formats.input_format
    golden_tensor = binary_golden(
        math_op,
        golden_src_A,
        golden_src_B,
        formats.output_format,
        math_fidelity,
        input_format=formats.input_format,
        input_format_B=formats.input_format,
    )

    # debug_print(
    #     "GOLDEN TENSOR (result of binary op)", tensor=golden_tensor, max_items=256
    # )

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

    # debug_print(
    #     "RESULT FROM L1 (raw)",
    #     data=f"Type: {type(res_from_L1)}, Length: {len(res_from_L1)}",
    # )
    # debug_print(
    #     "RESULT FROM L1 (first 256 values)",
    #     tensor=torch.tensor(res_from_L1),
    #     max_items=256,
    # )

    assert len(res_from_L1) == len(
        golden_tensor
    ), f"Result tensor ({len(res_from_L1)}) and golden tensor ({len(golden_tensor)}) are not of the same length"

    torch_format = format_dict[formats.output_format]
    res_tensor = torch.tensor(res_from_L1, dtype=torch_format)

    # debug_print("RESULT TENSOR (converted to torch)", tensor=res_tensor, max_items=256)

    # Compare in tilized format - print debug output only if test fails
    try:
        is_valid = passed_test(
            golden_tensor,
            res_tensor,
            formats.output_format,
            print_pcc=True,
        )
        assert is_valid, "Assert against golden failed"
    except AssertionError as e:
        # print("\n")
        # print("src_B")
        # print(src_B.view(32, 32))
        # print("\n")
        # print("src_A")
        # print(src_A.view(32, 32))
        # print("\n")
        # print("-" * 200)
        # print("golden_src_B")
        # print(golden_src_B.view(32, 32))
        # print("golden_src_A + golden_src_B (emulated addition)")
        # golden_added = (
        #     golden_src_A.to(torch.float32) + golden_src_B.to(torch.float32)
        # ).to(golden_src_A.dtype)
        # print(golden_added.view(32, 32))
        # print("\n")
        # print("golden_tensor")
        # print(golden_tensor.view(32, 32))
        # print("\n")
        # print("res_tensor")
        # print(res_tensor.view(32, 32))
        # print("\n")
        # print("-" * 200)

        _print_bfp_mismatch_blocks(
            golden_tensor, res_tensor, src_a=golden_src_A, golden_src_b=golden_src_B
        )
        raise


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
    if not test_passed:
        _print_bfp_mismatch_blocks(golden_tensor, res_tensor)
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

    if not test_passed:
        _print_bfp_mismatch_blocks(golden_tensor, res_tensor)
    assert test_passed, "Assert against golden failed"
