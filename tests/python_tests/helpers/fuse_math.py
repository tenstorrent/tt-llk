# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from typing import Dict, List, Type


class Fpu:
    def exec(self, config: Dict) -> str:
        return ""


class MatmulFpu(Fpu):
    def exec(self, config: Dict) -> str:
        CT_DIM = config["ct_dim"]
        RT_DIM = config["rt_dim"]
        KT_DIM = config["kt_dim"]

        math_format = config["math_format"]
        MATH_FORMAT = f"static_cast<std::underlying_type_t<DataFormat>>(DataFormat::{math_format.name})"

        fidelity = config["math_fidelity"]
        MATH_FIDELITY = fidelity.value

        dest_acc = config["dest_acc"].value

        code = f"""
    _llk_math_matmul_init_<{MATH_FIDELITY}, DstTileFaceLayout::RowMajor>(TILE_R_DIM, TILE_C_DIM, TILE_R_DIM, TILE_C_DIM, false, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    _llk_math_pack_sync_init_<DstSync::SyncHalf, {dest_acc}>();
    _llk_math_hw_configure_<false, false>({MATH_FORMAT}, {MATH_FORMAT});
    _llk_math_wait_for_dest_available_<DstSync::SyncHalf>();
    for (uint32_t j = 0; j < {KT_DIM}; j++)
    {{
        _llk_math_matmul_<{MATH_FIDELITY}, DstTileFaceLayout::RowMajor>(0, 0, {CT_DIM}, {RT_DIM}, {KT_DIM});
    }}
"""
        return code


class Sfpu:
    operation: str

    def __init__(self, operation: str):
        self.operation = operation

    def exec(self, config: Dict) -> str:
        return ""


class UnarySfpu(Sfpu):
    def __init__(self, operation: str):
        super().__init__(operation)

    def exec(self, config: Dict) -> str:
        code = f"""
    _llk_math_eltwise_unary_sfpu_init_<SfpuType::{self.operation}>();
    _llk_math_eltwise_unary_sfpu_start_<DstSync::SyncHalf>(0);
    test_utils::call_sfpu_operation_32(SfpuType::{self.operation});
    _llk_math_eltwise_unary_sfpu_done_();
"""
        return code


class BinarySfpu(Sfpu):
    def __init__(self, operation: str):
        super().__init__(operation)

    def exec(self, config: Dict) -> str:
        code = f"""

    _llk_math_eltwise_binary_sfpu_init_<ckernel::BinaryOp::{self.operation}>();
    _llk_math_eltwise_binary_sfpu_start_<DstSync::SyncHalf>(0);

    switch (ckernel::BinaryOp::{self.operation})
    {{
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::XLOGY:
            _sfpu_binary_init_<false, ckernel::BinaryOp::{self.operation}>();
            _calculate_sfpu_binary_<false, ckernel::BinaryOp::{self.operation}, 32>(0, 1, 0);
            break;
        case BinaryOp::RSHFT:
            _calculate_binary_right_shift_<false, 32, INT32, false>(0, 1, 0);
            break;
        case BinaryOp::LSHFT:
            _calculate_binary_left_shift_<false, 32, INT32, false>(0, 1, 0);
            break;
        case BinaryOp::LOGICAL_RSHFT:
            _calculate_logical_right_shift_<false, 32, INT32, false>(0, 1, 0);
            break;
        case BinaryOp::ADD_TOP_ROW:
            _init_add_top_row_();
            // Use actual format when compiling for ADD_TOP_ROW tests, otherwise use Float32 as safe default for static assert
            {{
                constexpr DataFormat add_top_row_format =
                    (ckernel::BinaryOp::{self.operation} == BinaryOp::ADD_TOP_ROW) ? static_cast<DataFormat>(formats.math) : DataFormat::Float32;
                _calculate_add_top_row_<add_top_row_format>(0, 1, 0);
            }}
            break;
        default:
            return;
    }}
    _llk_math_eltwise_binary_sfpu_done_();
"""
        return code


class Math:
    fpu: Type[Fpu]
    sfpu: List[Sfpu]

    def __init__(self, fpu: Type[Fpu], sfpu: List[Sfpu]):
        self.fpu = fpu
        self.sfpu = sfpu

    def exec(self, config: Dict) -> str:
        fpu_instance = self.fpu()
        code = fpu_instance.exec(config)

        for sfpu in self.sfpu:
            code += sfpu.exec(config)

        dest_acc = config["dest_acc"].value
        code += f"""
    _llk_math_dest_section_done_<DstSync::SyncHalf, {dest_acc}>();
"""
        return code
