1) Hey I was curious if there is anything to keep in mind when writing an SFPU initI saw this  

```cpp
inline void _llk_math_eltwise_unary_sfpu_init_()
{
    sfpu::_init_sfpu_config_reg();
    eltwise_unary_sfpu_configure_addrmod<sfpu_op>();
    math::reset_counters(p_setrwc::SET_ABD_F);
}
```

I was wondering if I can use this for any llk I want to initThis is an sfpu llk that calculates mean and variance at the same timeMy plan is to have dst0, dst1, and dst2 be inputs. And the llk writes out the outputs to dst1 and dst2.  
input:  
(dst_0 being the input data)  
(dst_1 being the past mean value)  
(dst_2 being the past variance value)Output  
(dst_1 updated mean value)  
(dst_2 updated variance value)

Someone can correct me if i am wrong, but i think eltwise is when you have two inputs, but it seems you are going to use three inputs, so you might need a specialized one. You need to check how the addrmods are configured and if they are applicable to your code.

his one was ripped directly from the exp_init(). So only one input in this case. Mine would be pretty unique indeed with 3. I haven't seen any op like thatCould you explain what are the addrmods or point me towards something to read? I'm not really familiar with them. (edited)
we just recently added a first ternary op support in LLK, for ttnn.where. The init part should be the same, and just a new *ternary_sfpu_params.h is used. Look at this [PR](https://github.com/tenstorrent/tt-llk/pull/520).

* PR in question:

# Add ttnn.where LLK #520

### Problem description

TT-NN operation where needs to be implemented in LLK

### What's changed

Added sfpu kerenls for BH and WH that implement ttnn.where functionality and cpp and python tests.

### Type of change

- [ ]  Bug fix (non-breaking change which fixes an issue)
- [x]  New feature (non-breaking change which adds functionality)
- [ ]  Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ]  Documentation update

## Pull Request Overview

This pull request implements the `ttnn.where` operation in the LLK (Low-Level Kernel) layer for both BlackHole and Wormhole B0 architectures. The implementation adds SFPU (Special Function Processing Unit) kernels that provide ternary conditional selection functionality, allowing selection between two tensors based on a condition tensor.

Key changes:

- Adds ternary SFPU infrastructure with new header files for both architectures
- Implements the `where` operation kernel with separate optimizations for FP16 and FP32 data formats
- Includes comprehensive C++ and Python test suites to validate functionality

### Reviewed Changes

Copilot reviewed 11 out of 11 changed files in this pull request and generated 4 comments.

Show a summary per fileComments suppressed due to low confidence (1)

### Files changed

```cpp
tests/firmware/riscv/common/llk_sfpu_types.h
|   |
|---|
|@@ -97,4 +97,5 @@ enum class SfpuType|
|prelu,|
|reshuffle_rows,|
|hardsigmoid,|
|where,|
|};|

tests/python_tests/helpers/format_arg_mapping.py
|   |
|---|
|@@ -26,6 +26,8 @@ class MathOpType(Enum):|
||
|SFPU_UNARY = auto()|
|SFPU_BINARY = auto()|
|SFPU_TERNARY = auto()|
||
|FPU_BINARY = auto()|
|REDUCE = auto()|
||
|[](https://github.com/tenstorrent/tt-llk/pull/520/files#diff-5114f7d71c3e54a8fd0870b6783201606f3937fc9c02da9fa8d785504500f371)[](https://github.com/tenstorrent/tt-llk/pull/520/files#diff-5114f7d71c3e54a8fd0870b6783201606f3937fc9c02da9fa8d785504500f371)|   |@@ -84,6 +86,13 @@ class MathOperation(Enum):|
|SfpuElwsub = OpSpec("SUB", MathOpType.SFPU_BINARY)|
|SfpuXlogy = OpSpec("XLOGY", MathOpType.SFPU_BINARY)|
||
|# =============================================================================|
|# SFPU TERNARY OPERATIONS|
|# =============================================================================|
|SfpuWhere = OpSpec("WHERE", MathOpType.SFPU_TERNARY)|
|# Alias maintained for backward compatibility with older test cases|
|TTNNWhere = SfpuWhere|
||
|# =============================================================================|
|# REDUCE OPERATIONS|
|# =============================================================================|

tests/python_tests/test_ttnn_where.py
|   |   |   |
|---|---|---|
||   |   |
||   |   |
||   |   |
||   |   |
||   |   |
|@@ -0,0 +1,339 @@|
|# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|# SPDX-License-Identifier: Apache-2.0|
||
||
|import pytest|
|import torch|
|from ttexalens.tt_exalens_lib import (|
|write_to_device,|
|)|
||
|from helpers.device import (|
|collect_results,|
|wait_for_tensix_operations_finished,|
|)|
|from helpers.format_arg_mapping import (|
|DestAccumulation,|
|MathOperation,|
|format_dict,|
|)|
|from helpers.format_config import DataFormat|
|from helpers.pack import pack_bfp8_b, pack_bfp16, pack_fp32|
|from helpers.param_config import (|
|clean_params,|
|generate_param_ids,|
|generate_params,|
|input_output_formats,|
|)|
|from helpers.test_config import run_test|
||
||
|# Helper function|
|def extend_tensor(condition, length=1024, dtype=torch.float32):|
|condition_extended = torch.zeros(length, dtype=dtype)|
|condition_extended[: condition.shape[0]] = condition|
|return condition_extended.flatten()|
||
||
|def generate_golden(operand1, true_value, false_value):|
|# operand1, true_value, and false_value are 1D tensors of floats|
|mask = operand1.view(32, 32) != 0|
|return torch.where(|
|mask, true_value.view(32, 32), false_value.view(32, 32)|
|).flatten()|
||
||
|# Helper check function|
|def torch_equal_nan(a, b):|
|return torch.all((a == b) \| (torch.isnan(a) & torch.isnan(b)))|
||
||
|# SUPPORTED FORMATS FOR TEST - allow sweeping through multiple formats|
|supported_formats = [|
|DataFormat.Float16_b,|
|DataFormat.Float32,|
|]|
||
||
|def get_dtype_for_format(data_format):|
|"""Get appropriate torch dtype for the given DataFormat"""|
|return format_dict[data_format]|
||
||
|def get_dest_acc_for_format(data_format):|
|"""Get appropriate dest_acc options for the given DataFormat"""|
|if data_format == DataFormat.Float32:|
|return [DestAccumulation.Yes] # Float32 requires dest_acc=Yes|
|elif data_format == DataFormat.Float16_b:|
|return [DestAccumulation.No] # Float16_b only allows dest_acc=No|
|else:|
|return [DestAccumulation.No, DestAccumulation.Yes] # Other formats can use both|
||
||
|def create_test_tensors(data_format):|
|"""Create test tensors with appropriate dtype for the given format"""|
|dtype = get_dtype_for_format(data_format)|
||
|condition = torch.tensor([1, 0, -2, 0, 5, 0, 0, 8, 0, -1], dtype=dtype)|
|condition_all_ones = torch.tensor([1, 1, 1, 1, 1, 1, 1, 1, 1, 1], dtype=dtype)|
|condition_all_zeros = torch.tensor([0, 0, 0, 0, 0, 0, 0, 0, 0, 0], dtype=dtype)|
||
|# true and false value tensors|
|true_values = torch.tensor(|
|[|
|1.0,|
|float("nan"),|
|3.0,|
|float("inf"),|
|-float("inf"),|
|-1.0,|
|0.0,|
|-0.0,|
|42.49,|
|-92.42,|
|],|
|dtype=dtype,|
|)|
|false_values = torch.tensor(|
|[|
|-1.0,|
|999.9,|
|float("nan"),|
|-float("inf"),|
|float("inf"),|
|1.0,|
|-0.0,|
|0.0,|
|-3.14,|
|7.84,|
|],|
|dtype=dtype,|
|)|
||
|return condition, condition_all_ones, condition_all_zeros, true_values, false_values|
||
||
|# Generate parameter combinations that dynamically include appropriate dest_acc for each format|
|# Use same=True to ensure input and output formats are identical (no mixing)|
|test_formats = input_output_formats(supported_formats, same=True)|
|all_params = []|
|for fmt in test_formats:|
|dest_acc_options = get_dest_acc_for_format(fmt.input_format)|
|# Use generate_params to create properly formatted parameter tuples|
|params = generate_params(|
|["ttnn_where_test"],|
|[fmt],|
|dest_acc=dest_acc_options,|
|mathop=[MathOperation.TTNNWhere],|
|)|
|all_params.extend(params)|
||
|param_ids = generate_param_ids(all_params)|
||
||
|@pytest.mark.parametrize(|
|"testname, formats, dest_acc, mathop",|
|clean_params(all_params),|
|ids=param_ids,|
|)|
|@pytest.mark.parametrize("test_case", ["mixed", "all_ones", "all_zeros"])|
|def test_ttnn_where(testname, formats, dest_acc, mathop, test_case):|
||
|# Generate tensors dynamically based on current input format|
|condition, condition_all_ones, condition_all_zeros, true_values, false_values = (|
|create_test_tensors(formats.input_format)|
|)|
|dtype = get_dtype_for_format(formats.input_format)|
||
|# Select test case|
|if test_case == "mixed":|
|test_condition = condition|
|elif test_case == "all_ones":|
|test_condition = condition_all_ones|
|else: # all_zeros|
|test_condition = condition_all_zeros|
||
|# Create test tensors with appropriate dtype for current format|
|src_A = extend_tensor(test_condition.bool(), length=1024, dtype=dtype)|
|src_B = extend_tensor(true_values, length=1024, dtype=dtype)|
|src_C = extend_tensor(false_values, length=1024, dtype=dtype)|
||
|# Skipping test combinations that are not supported|
|if (|
|formats.output_format == DataFormat.Float32|
|or formats.input_format == DataFormat.Float32|
|) and dest_acc == DestAccumulation.No:|
|pytest.skip(|
|"Skipping test for Float32 input format with NO dest_acc, as it is not supported."|
|)|
||
|core_loc = "0,0"|
|buffer_A_address = 0x1A000|
|buffer_B_address = 0x1B000|
|buffer_C_address = 0x1C000|
||
|if formats.input_format == DataFormat.Float32:|
|pack_function_A = pack_fp32|
|pack_function_B = pack_fp32|
|pack_function_C = pack_fp32|
|elif formats.input_format == DataFormat.Float16_b:|
|pack_function_A = pack_bfp16|
|pack_function_B = pack_bfp16|
|pack_function_C = pack_bfp16|
|elif formats.input_format == DataFormat.Bfp8_b:|
|pack_function_A = pack_bfp8_b|
|pack_function_B = pack_bfp8_b|
|pack_function_C = pack_bfp8_b|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|else:|
|raise ValueError(f"Unsupported input format: {formats.input_format}")|
||
|golden = generate_golden(src_A, src_B, src_C)|
|write_to_device(core_loc, buffer_A_address, pack_function_A(src_A))|
|write_to_device(core_loc, buffer_B_address, pack_function_B(src_B))|
|write_to_device(core_loc, buffer_C_address, pack_function_C(src_C))|
||
|unpack_to_dest = formats.input_format.is_32_bit()|
||
|test_config = {|
|"formats": formats,|
|"testname": testname,|
|"dest_acc": dest_acc,|
|"mathop": mathop,|
|"unpack_to_dest": unpack_to_dest,|
|}|
||
|run_test(test_config)|
||
|wait_for_tensix_operations_finished()|
|res_from_L1 = collect_results(formats, tile_count=1, address=0x1D000)|
|res_from_L1 = res_from_L1[:1024]|
|assert len(res_from_L1) == len(golden)|
||
|golden_tensor = torch.tensor(|
|golden,|
|dtype=(|
|format_dict[formats.output_format]|
|if formats.output_format|
|in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|else torch.bfloat16|
|),|
|)|
|res_tensor = torch.tensor(|
|res_from_L1,|
|dtype=(|
|format_dict[formats.output_format]|
|if formats.output_format|
|in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|else torch.bfloat16|
|),|
|)|
||
|assert torch_equal_nan(golden_tensor, res_tensor)|
||
||
|# MCW test with dynamic format sweeping like main test|
|# Use same input/output format - no mixing|
|test_formats_mcw = input_output_formats(supported_formats, same=True)|
|all_params_mcw = []|
|for fmt in test_formats_mcw:|
|dest_acc_options = get_dest_acc_for_format(fmt.input_format)|
|# Use generate_params to create properly formatted parameter tuples|
|params = generate_params(|
|["ttnn_where_test"],|
|[fmt],|
|dest_acc=dest_acc_options,|
|mathop=[MathOperation.TTNNWhere],|
|)|
|all_params_mcw.extend(params)|
||
|param_ids_mcw = generate_param_ids(all_params_mcw)|
||
||
|@pytest.mark.parametrize(|
|"testname, formats, dest_acc, mathop",|
|clean_params(all_params_mcw),|
|ids=param_ids_mcw,|
|)|
|@pytest.mark.parametrize("h", [32])|
|@pytest.mark.parametrize("w", [32])|
|def test_ttnn_where_mcw(testname, formats, dest_acc, mathop, h, w):|
|# Generate dtype dynamically based on current input format|
|dtype = get_dtype_for_format(formats.input_format)|
||
|C = torch.arange(h * w, dtype=dtype)|
|C = (C % 2).to(dtype) # Alternates 0, 1, 0, 1, ... with correct dtype|
|C = C.reshape(1, 1, h, w)|
|C = C.expand(1, 1, h, w) # Broadcast to (n, c, h, w)|
|T = torch.ones(1, 1, h, w, dtype=dtype) * 2|
|F = torch.ones(1, 1, h, w, dtype=dtype) * 11|
|golden = torch.where(C != 0, T, F)|
||
|C = C.flatten().to(format_dict[formats.input_format])|
|T = T.flatten().to(format_dict[formats.input_format])|
|F = F.flatten().to(format_dict[formats.input_format])|
||
|core_loc = "0,0"|
|buffer_A_address = 0x1A000|
|buffer_B_address = 0x1B000|
|buffer_C_address = 0x1C000|
||
|if formats.input_format == DataFormat.Float32:|
|pack_function_A = pack_fp32|
|pack_function_B = pack_fp32|
|pack_function_C = pack_fp32|
|elif formats.input_format == DataFormat.Float16_b:|
|pack_function_A = pack_bfp16|
|pack_function_B = pack_bfp16|
|pack_function_C = pack_bfp16|
|elif formats.input_format == DataFormat.Bfp8_b:|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|pack_function_A = pack_bfp8_b|
|pack_function_B = pack_bfp8_b|
|pack_function_C = pack_bfp8_b|
|else:|
|raise ValueError(f"Unsupported input format: {formats.input_format}")|
||
|golden = generate_golden(C, T, F)|
|write_to_device(core_loc, buffer_A_address, pack_function_A(C))|
|write_to_device(core_loc, buffer_B_address, pack_function_B(T))|
|write_to_device(core_loc, buffer_C_address, pack_function_C(F))|
||
|unpack_to_dest = formats.input_format.is_32_bit()|
||
|test_config = {|
|"formats": formats,|
|"testname": testname,|
|"dest_acc": dest_acc,|
|"unpack_to_dest": unpack_to_dest,|
|"mathop": mathop,|
|}|
||
|run_test(test_config)|
||
|wait_for_tensix_operations_finished()|
|res_from_L1 = collect_results(formats, tile_count=1, address=0x1D000)|
|res_from_L1 = res_from_L1[:1024]|
||
|golden_tensor = torch.tensor(|
|golden,|
|dtype=(|
|format_dict[formats.output_format]|
|if formats.output_format|
|in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|else torch.bfloat16|
|),|
|)|
||
|golden_tensor = golden_tensor.flatten()[:1024] # Ensure it matches the result size|
||
|res_tensor = torch.tensor(|
|res_from_L1,|
|dtype=(|
|format_dict[formats.output_format]|
|if formats.output_format|
|in [DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32]|
|else torch.bfloat16|
|),|
|)|
||
|assert len(res_tensor) == len(golden_tensor)|
|assert torch_equal_nan(golden_tensor, res_tensor)|

tests/sources/ttnn_where_test.cpp
|   |
|---|
|@@ -0,0 +1,165 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#include <algorithm>|
|#include <cstdint>|
||
|#include "ckernel.h"|
|#include "ckernel_debug.h"|
|#include "llk_defs.h"|
||
|// Globals|
|uint32_t unp_cfg_context = 0;|
|uint32_t pack_sync_tile_dst_ptr = 0;|
|uint32_t math_sync_tile_dst_index = 0;|
||
|constexpr bool disable_src_zero_flag = true;|
||
|#ifdef LLK_TRISC_UNPACK|
||
|#include "llk_unpack_A.h"|
|#include "llk_unpack_common.h"|
|#include "params.h"|
||
|void run_kernel()|
|{|
|using DataFormatUT = std::underlying_type_t<DataFormat>;|
|auto to_ufmt = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };|
||
|uint8_t UNPACK_FMT;|
|if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))|
|{|
|UNPACK_FMT = to_ufmt(DataFormat::Float32);|
|}|
|else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))|
|{|
|UNPACK_FMT = to_ufmt(DataFormat::Bfp8_b);|
|}|
|else|
|{|
|UNPACK_FMT = to_ufmt(DataFormat::UInt16);|
|}|
||
|volatile uint32_t* const buffer_condition = reinterpret_cast<volatile uint32_t*>(0x1a000);|
|volatile uint32_t* const buffer_true = reinterpret_cast<volatile uint32_t*>(0x1b000);|
|volatile uint32_t* const buffer_false = reinterpret_cast<volatile uint32_t*>(0x1c000);|
||
|_llk_unpack_A_hw_configure_<is_fp32_dest_acc_en, StochRndType::None, disable_src_zero_flag>(UNPACK_FMT, UNPACK_FMT, FACE_R_DIM, 0, 4);|
|_llk_unpack_A_init_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(0, 0, FACE_R_DIM, 4, UNPACK_FMT, UNPACK_FMT);|
|_llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_condition), 0, UNPACK_FMT, UNPACK_FMT);|
|_llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_true), 0, UNPACK_FMT, UNPACK_FMT);|
|_llk_unpack_A_<BroadcastType::NONE, false, EltwiseBinaryReuseDestType::NONE, unpack_to_dest>(L1_ADDRESS(buffer_false), 0, UNPACK_FMT, UNPACK_FMT);|
|}|
||
|#endif|
||
|#ifdef LLK_TRISC_MATH|
||
|#include "ckernel_sfpu.h"|
|#include "ckernel_sfpu_where.h"|
|#include "llk_math_common.h"|
|#include "llk_math_eltwise_ternary_sfpu.h"|
|#include "llk_math_eltwise_unary_datacopy.h"|
|#include "llk_math_eltwise_unary_sfpu.h"|
|#include "params.h"|
||
|using namespace ckernel;|
||
|// using namespace sfpu;|
||
|void run_kernel()|
|{|
|using DataFormatUT = std::underlying_type_t<DataFormat>;|
|auto to_ufmt = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };|
||
|uint8_t MATH_FMT;|
|if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))|
|{|
|MATH_FMT = to_ufmt(DataFormat::Float32);|
|}|
|else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))|
|{|
|MATH_FMT = to_ufmt(DataFormat::Bfp8_b);|
|}|
|else|
|{|
|MATH_FMT = to_ufmt(DataFormat::UInt16);|
|}|
||
|// copy srca to dest|
|#ifdef ARCH_BLACKHOLE|
|_llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false, false>(0, 0, 4, MATH_FMT);|
|#else|
|_llk_math_eltwise_unary_datacopy_init_<DataCopyType::A2D, is_fp32_dest_acc_en, BroadcastType::NONE, false>(0, 0, 4, MATH_FMT);|
|#endif|
|_llk_math_pack_sync_init_<DstSync::SyncHalf, is_fp32_dest_acc_en>();|
|_llk_math_hw_configure_<false, false>(MATH_FMT, MATH_FMT);|
|_llk_math_wait_for_dest_available_<DstSync::SyncHalf>();|
|_llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(|
|0, MATH_FMT, MATH_FMT); // buffer condition|
|_llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(|
|1, MATH_FMT, MATH_FMT); // buffer true|
|_llk_math_eltwise_unary_datacopy_<DataCopyType::A2D, DstSync::SyncHalf, is_fp32_dest_acc_en, BroadcastType::NONE, unpack_to_dest>(|
|2, MATH_FMT, MATH_FMT); // buffer false|
||
|// calculation of sfpu operation on dest|
|_llk_math_eltwise_ternary_sfpu_init_<SfpuType::where>();|
|_llk_math_eltwise_ternary_sfpu_start_<DstSync::SyncHalf>(0);|
||
|ckernel::sfpu::_calculate_where_<false, static_cast<DataFormat>(UNPACK_A_IN)>();|
||
|_llk_math_eltwise_ternary_sfpu_done_();|
||
|_llk_math_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();|
|}|
||
|#endif|
||
|#ifdef LLK_TRISC_PACK|
||
|#include "llk_pack.h"|
|#include "llk_pack_common.h"|
|#include "params.h"|
||
|void run_kernel()|
|{|
|using DataFormatUT = std::underlying_type_t<DataFormat>;|
|auto to_ufmt = [](DataFormat fmt) constexpr { return static_cast<DataFormatUT>(fmt); };|
||
|std::uint8_t PACK_FMT;|
|if (UNPACK_A_IN == to_ufmt(DataFormat::Float32))|
|{|
|PACK_FMT = to_ufmt(DataFormat::Float32);|
|}|
|else if (UNPACK_A_IN == to_ufmt(DataFormat::Bfp8_b))|
|{|
|PACK_FMT = to_ufmt(DataFormat::Bfp8_b);|
|}|
|else|
|{|
|PACK_FMT = to_ufmt(DataFormat::UInt16);|
|}|
||
|volatile uint32_t* const buffer_Dest = reinterpret_cast<volatile uint32_t*>(0x1d000);|
||
|#ifdef ARCH_BLACKHOLE|
|_llk_pack_hw_configure_<is_fp32_dest_acc_en, false, false>(PACK_FMT, PACK_FMT, 16 * 16 * 4);|
|#else|
|_llk_pack_hw_configure_<is_fp32_dest_acc_en, false>(PACK_FMT, PACK_FMT, 16 * 16 * 4);|
|#endif|
||
|_llk_pack_init_<false, false, DstTileFaceLayout::RowMajor, false>(PACK_FMT);|
||
|#ifdef ARCH_BLACKHOLE|
|_llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor>();|
|#else|
|_llk_pack_dest_init_<DstSync::SyncHalf, is_fp32_dest_acc_en, DstTileFaceLayout::RowMajor, false>();|
|#endif|
||
|_llk_packer_wait_for_math_done_();|
|_llk_pack_<DstSync::SyncHalf, false, is_fp32_dest_acc_en>(0, L1_ADDRESS(buffer_Dest));|
|_llk_pack_dest_section_done_<DstSync::SyncHalf, is_fp32_dest_acc_en>();|
|}|
||
|#endif|

tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_where.h
|   |
|---|
|@@ -0,0 +1,99 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
||
|#include "llk_defs.h"|
|#include "sfpi.h"|
||
|namespace ckernel::sfpu|
|{|
||
|/*|
||
|Expects following input in Dst register:|
|Index 0 ( Tile 0 ) -> condition tensor|
|Index 32 ( Tile 1 ) -> true tensor|
|Index 64 ( Tile 2 ) -> false tensor|
||
|*/|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS>|
|inline void _calculate_where_fp16_b_()|
|{|
|constexpr uint dst_tile_size_rows = 64;|
||
|sfpi::vFloat cond = sfpi::dst_reg[0];|
||
|for (int i = 0; i < ITERATIONS; i++)|
|{|
|cond = sfpi::dst_reg[0];|
||
|v_if (cond == 0.0f)|
|{|
|// output_tensor = false_tensor;|
|TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 2 * dst_tile_size_rows);|
|}|
|v_else|
|{|
|// output_tensor = true_tensor;|
|TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_tile_size_rows);|
|v_endif;|
|}|
|// sfpi::dst_reg[0] = output_tensor;|
|TTI_SFPSTORE(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 0);|
||
|sfpi::dst_reg++;|
|}|
|}|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS>|
|inline void _calculate_where_fp32_()|
|{|
|constexpr uint dst_tile_size = 32;|
||
|sfpi::vFloat output_tensor = 0;|
|sfpi::vFloat true_tensor = 0;|
|sfpi::vFloat false_tensor = 0;|
|sfpi::vFloat cond = sfpi::dst_reg[0];|
||
|for (int i = 0; i < ITERATIONS; i++)|
|{|
|cond = sfpi::dst_reg[0];|
|true_tensor = sfpi::dst_reg[dst_tile_size];|
|false_tensor = sfpi::dst_reg[dst_tile_size * 2];|
||
|v_if (cond != 0.0f)|
|{|
|output_tensor = true_tensor;|
|}|
|v_else|
|{|
|output_tensor = false_tensor;|
|}|
|v_endif;|
||
|sfpi::dst_reg[0] = output_tensor;|
|sfpi::dst_reg++;|
|}|
|}|
||
|template <bool APPROXIMATION_MODE, DataFormat data_format>|
|inline void _calculate_where_()|
|{|
|// Add a compile-time check to ensure only supported formats are used.|
|static_assert(|
|data_format == DataFormat::Float32 \| data_format == DataFormat::Float16_b,|
|"Unsupported data format for _calculate_where_(). Only Float32 and Float16_b are allowed.");|
|if constexpr (data_format == DataFormat::Float32)|
|{|
|_calculate_where_fp32_<APPROXIMATION_MODE, 32>();|
|}|
|else|
|{|
|_calculate_where_fp16_b_<APPROXIMATION_MODE, 32>();|
|}|
|}|
||
|} // namespace ckernel::sfpu|

tt_llk_blackhole/llk_lib/llk_math_eltwise_ternary_sfpu.h
|   |
|---|
|@@ -0,0 +1,64 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
|#include <type_traits>|
||
|#include "ckernel_globals.h"|
|#include "ckernel_include.h"|
|#include "ckernel_ops.h"|
|#include "ckernel_sfpu.h"|
|#include "ckernel_template.h"|
|#include "cmath_common.h"|
|#include "llk_math_common.h"|
|#include "llk_sfpu_types.h"|
||
|using namespace ckernel;|

|// local function declarations|
|template <SfpuType sfpu_op>|
|inline void eltwise_ternary_sfpu_configure_addrmod()|
|{|
|addr_mod_t {|
|.srca = {.incr = 0},|
|.srcb = {.incr = 0},|
|.dest = {.incr = 0},|
|}|
|.set(ADDR_MOD_7);|
|}|
||
|inline void eltwise_ternary_sfpu_configure_mop();|
||
|template <DstSync Dst>|
|inline void _llk_math_eltwise_ternary_sfpu_start_(const uint dst_index)|
|{|
|math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);|
|// math::set_addr_mod_base();|
|TTI_SETC16(2, 1); // set addr mod base (use addr mods 4..7) ADDR_MOD_SET_Base_ADDR32 = 2 for wormhole|
||
|TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);|
|}|
||
|inline void _llk_math_eltwise_ternary_sfpu_done_()|
|{|
|math::clear_dst_reg_addr();|
||
|TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::WAIT_SFPU);|
|// math::clear_addr_mod_base();|
|TTI_SETC16(2, 0); // clear addr mod base (use addr mods 0..3)|
|}|
||
|inline void _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_()|
|{|
|math::inc_dst_addr<8>();|
|math::inc_dst_addr<8>();|
|}|
||
|template <SfpuType sfpu_op>|
|inline void _llk_math_eltwise_ternary_sfpu_init_()|
|{|
|sfpu::_init_sfpu_config_reg();|
|eltwise_ternary_sfpu_configure_addrmod<sfpu_op>();|
|math::reset_counters(p_setrwc::SET_ABD_F);|
|}|
||   |   |

tt_llk_blackhole/llk_lib/llk_math_eltwise_ternary_sfpu_params.h

|   |
|---|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
|#include "ckernel_sfpu_where.h"|
|#include "llk_math_eltwise_ternary_sfpu.h"|
|#include "llk_sfpu_types.h"|
||
|template <bool APPROXIMATE, class F, class... ARGS>|
|   |
|---|
|inline void _llk_math_eltwise_ternary_sfpu_params_(|
|F&& sfpu_func, uint dst_index0, uint dst_index1, uint dst_index2, int vector_mode = (int)VectorMode::RC, ARGS&&... args)|
|   |   |   |
|---|---|---|
||   |   |
|{|
|// Compute minimum destination index|
|uint dst_index = std::min(std::min(dst_index0, dst_index1), dst_index2);|
||
|_llk_math_eltwise_ternary_sfpu_start_<DST_SYNC_MODE>(dst_index); // Reuse same sync primitive|
||
|if (vector_mode == (int)VectorMode::R)|
|{|
|// Row vector - Face0 + Face1|
|for (int face = 0; face < 2; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // repeat 2x|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|// Skip next 2 faces|
|for (int i = 0; i < 4; ++i)|
|{|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|else if (vector_mode == (int)VectorMode::C)|
|{|
|// Column vector - Face0 + Face2|
|for (int face = 0; face < 2; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
|for (int i = 0; i < 4; ++i)|
|{|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|}|
|else if (vector_mode == (int)VectorMode::RC)|
|{|
|// All 4 faces|
|for (int face = 0; face < 4; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|else|
|{|
|// Default: single face pass-through|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|}|
|_llk_math_eltwise_ternary_sfpu_done_(); // Finalize|
|}|

tt_llk_wormhole_b0/common/inc/ckernel_sfpu.h

|   |
|---|
|@@ -52,6 +52,7 @@|
|#include "sfpu/ckernel_sfpu_topk.h"|
|#include "sfpu/ckernel_sfpu_trigonometry.h"|
|#include "sfpu/ckernel_sfpu_typecast.h"|
|#include "sfpu/ckernel_sfpu_where.h"|
||
|// namespace ckernel|
|// {|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_where.h

|   |
|---|
|@@ -0,0 +1,99 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
||
|#include "llk_defs.h"|
|#include "sfpi.h"|
||
|namespace ckernel::sfpu|
|{|
||
|/*|
||
|Expects following input in Dst register:|
|Index 0 ( Tile 0 ) -> condition tensor|
|Index 32 ( Tile 1 ) -> true tensor|
|Index 64 ( Tile 2 ) -> false tensor|
||
|*/|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS>|
|inline void _calculate_where_fp16_b_()|
|{|
|constexpr uint dst_tile_size_rows = 64;|
||
|sfpi::vFloat cond = sfpi::dst_reg[0];|
||
|for (int i = 0; i < ITERATIONS; i++)|
|{|
|cond = sfpi::dst_reg[0];|
||
|v_if (cond == 0.0f)|
|{|
|// output_tensor = false_tensor;|
|TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 2 * dst_tile_size_rows);|
|}|
|v_else|
|{|
|// output_tensor = true_tensor;|
|TTI_SFPLOAD(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, dst_tile_size_rows);|
|v_endif;|
|}|
|// sfpi::dst_reg[0] = output_tensor;|
|TTI_SFPSTORE(p_sfpu::LREG3, InstrModLoadStore::LO16, ADDR_MOD_3, 0);|
||
|sfpi::dst_reg++;|
|}|
|}|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS>|
|inline void _calculate_where_fp32_()|
|{|
|constexpr uint dst_tile_size = 32;|
||
|sfpi::vFloat output_tensor = 0;|
|sfpi::vFloat true_tensor = 0;|
|sfpi::vFloat false_tensor = 0;|
|sfpi::vFloat cond = sfpi::dst_reg[0];|
||
|for (int i = 0; i < ITERATIONS; i++)|
|{|
|cond = sfpi::dst_reg[0];|
|true_tensor = sfpi::dst_reg[dst_tile_size];|
|false_tensor = sfpi::dst_reg[dst_tile_size * 2];|
||
|v_if (cond != 0.0f)|
|{|
|output_tensor = true_tensor;|
|}|
|v_else|
|{|
|output_tensor = false_tensor;|
|}|
|v_endif;|
||
|sfpi::dst_reg[0] = output_tensor;|
|sfpi::dst_reg++;|
|}|
|}|
||
|template <bool APPROXIMATION_MODE, DataFormat data_format>|
|inline void _calculate_where_()|
|{|
|// Add a compile-time check to ensure only supported formats are used.|
|static_assert(|
|data_format == DataFormat::Float32 \| data_format == DataFormat::Float16_b,|
|"Unsupported data format for _calculate_where_(). Only Float32 and Float16_b are allowed.");|
|if constexpr (data_format == DataFormat::Float32)|
|{|
|_calculate_where_fp32_<APPROXIMATION_MODE, 32>();|
|}|
|else|
|{|
|_calculate_where_fp16_b_<APPROXIMATION_MODE, 32>();|
|}|
|}|
||
|} // namespace ckernel::sfpu|

tt_llk_wormhole_b0/llk_lib/llk_math_eltwise_ternary_sfpu.h

|   |   |   |
|---|---|---|
||   |   |
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
|#include <type_traits>|
||
|#include "ckernel_globals.h"|
|#include "ckernel_include.h"|
|#include "ckernel_ops.h"|
|#include "ckernel_sfpu.h"|
|#include "ckernel_template.h"|
|#include "cmath_common.h"|
|#include "llk_math_common.h"|
|#include "llk_sfpu_types.h"|
||
|using namespace ckernel;|
|**ldjurovicTT** marked this conversation as resolved.|   |   |
||
|// local function declarations|
|template <SfpuType sfpu_op>|
|inline void eltwise_ternary_sfpu_configure_addrmod()|
|{|
|addr_mod_t {|
|.srca = {.incr = 0},|
|.srcb = {.incr = 0},|
|.dest = {.incr = 0},|
|}|
|.set(ADDR_MOD_7);|
|}|
||
|inline void eltwise_ternary_sfpu_configure_mop();|
||
|template <DstSync Dst>|
|inline void _llk_math_eltwise_ternary_sfpu_start_(const uint dst_index)|
|{|
|math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);|
||
|math::set_addr_mod_base();|
|TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);|
|}|
||
|inline void _llk_math_eltwise_ternary_sfpu_done_()|
|{|
|math::clear_dst_reg_addr();|
||
|TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::WAIT_SFPU);|
|math::clear_addr_mod_base();|
|}|
||
|inline void _llk_math_eltwise_ternary_sfpu_inc_dst_face_addr_()|
|{|
|math::inc_dst_addr<8>();|
|math::inc_dst_addr<8>();|
|}|
||
|template <SfpuType sfpu_op>|
|inline void _llk_math_eltwise_ternary_sfpu_init_()|
|{|
|sfpu::_init_sfpu_config_reg();|
|eltwise_ternary_sfpu_configure_addrmod<sfpu_op>();|
|math::reset_counters(p_setrwc::SET_ABD_F);|
|}|

tt_llk_wormhole_b0/llk_lib/llk_math_eltwise_ternary_sfpu_params.h

|   |
|---|
|@@ -0,0 +1,62 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|#pragma once|
|#include "ckernel_sfpu_where.h"|
|#include "llk_math_eltwise_ternary_sfpu.h"|
|#include "llk_sfpu_types.h"|
||
|template <bool APPROXIMATE, class F, class... ARGS>|
|inline void _llk_math_eltwise_ternary_sfpu_params_(|
|F&& sfpu_func, uint dst_index0, uint dst_index1, uint dst_index2, int vector_mode = (int)VectorMode::RC, ARGS&&... args)|
|{|
|// Compute minimum destination index|
|uint dst_index = std::min(std::min(dst_index0, dst_index1), dst_index2);|
||
|_llk_math_eltwise_ternary_sfpu_start_<DST_SYNC_MODE>(dst_index); // Reuse same sync primitive|
||
|if (vector_mode == (int)VectorMode::R)|
|{|
|// Row vector - Face0 + Face1|
|for (int face = 0; face < 2; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D); // repeat 2x|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|// Skip next 2 faces|
|for (int i = 0; i < 4; ++i)|
|{|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|else if (vector_mode == (int)VectorMode::C)|
|{|
|// Column vector - Face0 + Face2|
|for (int face = 0; face < 2; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|for (int i = 0; i < 4; ++i)|
|{|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|}|
|else if (vector_mode == (int)VectorMode::RC)|
|{|
|// All 4 faces|
|for (int face = 0; face < 4; face++)|
|{|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|TTI_SETRWC(p_setrwc::CLR_NONE, p_setrwc::CR_D, 8, 0, 0, p_setrwc::SET_D);|
|}|
|}|
|else|
|{|
|// Default: single face pass-through|
|std::forward<F>(sfpu_func)(std::forward<ARGS&&>(args)...); // Need to replace the above line with this|
|}|
|_llk_math_eltwise_ternary_sfpu_done_(); // Finalize|
|}|
```

* Awesome, thanks for the link to the PR. Super helpful!
* I was wondering if anyone knows why the following code is in the ternary_sfpu_params.h
```cpp
uint dst_index = std::min(std::min(dst_index0, dst_index1), dst_index2);

// Reuse same sync primitive
_llk_math_eltwise_ternary_sfpu_start_<DST_SYNC_MODE>(dst_index);

inline void _llk_math_eltwise_ternary_sfpu_start_(const uint dst_index) {
    math::set_dst_write_addr<DstTileLayout::Default, DstTileShape::Tile32x32>(dst_index);
    // math::set_addr_mod_base(); // Optional: comment explains potential use
    TTI_SETC16(2, 1); // Set addr mod base (use addr mods 4..7), ADDR_MOD_SET_Base_ADDR32 = 2 for wormhole
    TTI_STALLWAIT(p_stall::STALL_SFPU, p_stall::MATH);
}
```

* I was wondering why we would set the dst_write_addr to the minimum of the 3 dst registers?Does that mean if I pass dst0, dst1, and dst2 to a ternery operation. then dst_reg[0] is the starting vector for dst0([0-3][0-15] even). And then dst_reg[32] for dst1  \ dst_reg[64]  for dst2.This seems a bit odd, since it seems like the SFPU always chooses to write to the smallest dst_reg. I just want to double check I am not misunderstanding something
* Afaik it sets the address of the destination register when the swapping of register parts between math and pack is done
* I'm not sure of the exact reason for this choice as it is not documented explicitly, but the obvious reasons are simplicity (Dest space where input argument was placed is certainly available) and efficiency (it let's you start your pointer from the first available dst_reg pointer and increment).  
DeepWiki gave a similar answer -
### Why do SFPU implementations write result to the DEST register with the smallest index?

SFPU implementations write results to the DEST register with the smallest index because the SFPU uses a load/store architecture with sequential pointer-based addressing.

The SFPU (Special Function Processing Unit) cannot operate directly on data stored in the Destination register - it requires data to be loaded into its internal registers first, then stored back to the Destination register after processing top_level_overview.md:135-147 .

SFPU implementations follow a consistent pattern where they start writing to `dst_reg[0]` (the smallest index) and then increment the destination register pointer after each operation. This can be seen in the fill operation implementation ckernel_sfpu_fill.h:22-24 and typecast operations ckernel_sfpu_typecast.h:65-67 where `sfpi::dst_reg[0]` is written to, followed by `sfpi::dst_reg++` to advance to the next register location.

This sequential addressing pattern ensures orderly processing of multiple tiles or data elements. The SFPU operations process data iteratively, starting from the lowest register index and incrementing the pointer after each tile is processed ckernel_sfpu_fill.h:20-25 . The underlying SFPSTORE instruction supports addressing specific destination register locations assembly.yaml:2189-2196 , but the software implementations use the incremental pointer approach for efficient sequential processing.

## Notes 

This architectural design choice ensures that SFPU operations maintain a predictable memory access pattern and can efficiently process multiple tiles in sequence without requiring complex address calculations. The smallest index serves as the natural starting point for this sequential processing model.

```
docs/llk/l2/top_level_overview.md
SFPU provides specialized operations beyond FPU capabilities. As a SIMD engine, it executes identical operations on multiple data points in parallel. Unlike FPU, SFPU can only read from and write to Destination register. It is a load/store architecture, meaning that it cannot operate directly on data stored in the Destination register - it requires the data to be loaded in its internal registers, and stored back to Destination register. SFPU supports:
136
137- 32-bit input calculations.
138- Straightforward SW implementation of complex mathematical functions (sigmoid, exponential, reciprocal, etc.).
139
140SFPU data flow requirements:
141
142- Input operands must reside in the Destination register.
143- Data can be moved to Destination register through:
144  - FPU transfer from Source A/B registers.
145  - Direct unpacking from L1 memory via Unpacker 0.
146- Results are stored back in the Destination register.
147
148SFPU is instantiated within FPU, meaning that the same processor used for issuing FPU instructions (TRISC1) is in charge of issuing SFPU instructions.
149
150## Destination Operand Register file
```

```
tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_fill.h

17    // SFPU microcode
18    sfpi::vFloat fill_val = value;
19
20    for (int d = 0; d < ITERATIONS; d++)
21    {
22        sfpi::dst_reg[0] = fill_val;
23        sfpi::dst_reg++;
24    }
```


2) Hey all, I was looking into writing an SPFU LLK with SFPI. I was wondering if someone could help explain some SFPI code to me
```cpp
template <bool APPROXIMATION_MODE>
inline void calculate_sum_int_row() {
    for (unsigned i = 0; i < 8; i += 2) {
        vInt a = dst_reg[i];
        a = sfpu_twos_comp_to_sign_mag(a);

        int arr[] = {1, 8, 9};
        for (unsigned j = 0; j < sizeof(arr) / sizeof(arr[0]); ++j) {
            vInt b = dst_reg[i + arr[j]];
            b = sfpu_twos_comp_to_sign_mag(b);
            a += b;
        }

        a = sfpu_sign_mag_to_twos_comp(a);
        dst_reg[i] = a;
    }
}
```

So I assume that dst_reg[i] will give you a 32 length vector of inputs stores into vInt a. I was wondering if I increment i, how would I be moving through the dst reg? Does it grab the 32 elements, and then the i+2 is the 32 after that  
like

```cpp
dst_reg[i]=<0,1,2,3,4.., 31>
dst_regs[i+2]=<32,33,34...,63>
```

I have also been having a hard time visualizing the loops and where in the tile
` vInt a = dst_reg[i];`
and  
` vInt b = dst_reg[i + arr[j]];`
would be located?Any help would be greatly appreciated (edited)

* You are correct that dst_reg[i] will give you a 32 length vector of inputs stored into vInt a, but it will not return 32 consecutive elements as you think. dst_reg is organized as a bunch of 16-element rows, and each load/store instruction actually operates on 4 rows at a time, on either the even or odd columns. So dst_reg[0] loads the even elements of rows 0-3, and dst_reg[1] loads odd eleemnts of rows 0-3, dst_reg[2] loads even elements of rows 4-7, etc... So if you think of it as indices of elements it would be :  

```
dst_reg[i]=<i*4+0,i*4+2,i*4+4.., i*4+62>
dst_reg[i+1]=<i*4+1,i*4+3,i*4+5.., i*4+63>
```

This might make it difficult to perform row addition the way you have here.Also, note that when using sfpi (rather than TTI_ instructions directly) you dont need to increment the index "i" by 2, since sfpi will abstract the row stride away for you already. So you just need increase the index by 1 and it will be multiplied by 2 by sfpi.Lastly, it looks like the kernel you want to implement does row-wise addition between rows indexed by "arr". A similar kernel exists with `tt_metal/third_party/tt_llk/tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_reshuffle_rows.h` for fp formats. You'll notice it uses TTI_ instructions so the indices are x2 everywhere.

* Ahh I see, thanks for the breakdown of how dst_reg is parsing through the tile. That really helps a lotIs this access pattern of odd and even loads something that is set in stone? Or would I be able to change the way the hardware DMA accesses loads in values?I was hoping to load a column of values into a vfloat? I'm not too worried about the perf of this load.Could I weave in TTI_intructions with the SFPI to get an LLK that has a custom load for dst_reg and sfpi for doing the calculation side? I see that in `tt_metal/third_party/tt_llk/tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_reshuffle_rows.h` , this is a good example of custom loads, but I was wondering if I could use the vfloats along with the TTI

* unfortunately its the only access pattern possible
* Ahh, I see. I probably could do a lot of loads and do some math to get the data I want
* yeah if you want to load a column, then each load will take 4 rows of a column at a time so if you can work with that it should be ok. If you need pointers feel free to ask here again
* Right, I may be able to save some perf by loading multiple collumns at once. Since I will need all 32 collumns
* yeah, try to do work on cols in parallel if possible
* Yeah, thanks a ton for the explanations so far. I definitely will reach out when I have something more concrete to test with
3) hey guys I have been asked a few times now about how the unpack matmul is able to optimize for rt_dim and ct_dim, and I made a diagram here: for anyone who needs to see why there are less unpacks
```
subblock_r_dim = 2
subblock_c_dim = 2
subblock_k_dim = 2
num_subblocks_r_dim = 1
num_subblocks_c_dim = 1
num_subblocks_k_dim = 1

SrcA → In1
+-----+-----+
| A0  | A1  |
+-----+-----+
| A2  | A3  |
+-----+-----+

SrcB → In0
+-----+-----+
| B0  | B1  |
+-----+-----+
| B2  | B3  |
+-----+-----+

Results:
+-------------------+-------------------+
| B0A0 + B1A2       | B0A1 + B1A3       |
+-------------------+-------------------+
| B2A0 + B3A2       | B2A1 + B3A3       |
+-------------------+-------------------+

Unpack Algorithm:

Check if subblock_c_dim ≥ subblock_r_dim → true,
That means we will re-use srcB.

Time | Unpack0           | Unpack1           | Math–FPU Dest
---------------------------------------------------------------
  0  | bank 0 (A0)       | bank 0 (B0)       | B0A0 (tile index 0)
  1  | bank 1 (A1)       | –                 | B0A1 (tile index 1)
  2  | bank 0 (A2)       | bank 1 (B2)       | B2A0 (tile index 2)
  3  | bank 1 (A3)       | –                 | B2A1 (tile index 3)
  4  | bank 0 (A0)       | bank 0 (B1)       | B1A2 (tile index 0, so it is accumulated, final result = B0A0 + B1A2)
  5  | bank 1 (A1)       | –                 | B1A3 (tile index 1, so it is accumulated, final result = B0A1 + B1A3)
  6  | bank 0 (A2)       | bank 1 (B3)       | B3A2 (tile index 2, so it is accumulated, final result = B2A0 + B3A2)
  7  | bank 1 (A3)       | –                 | B3A3 (tile index 3, so it is accumulated, final result = B2A1 + B3A3)

Total num unpacks = 8 from Unpack0 + 4 from Unpack1 = 12
```

* would you be able to create this diagram for a non-square tensor? Something that has non 1 num_subblock size?

* if num_subblock_*_dim>1 , it depends on the op writer how to handle it, but sure I can do that to show how its handled currently for the bmm_.*cpp file, and the conv file
* the above for subblock_r_dim/subblock_c_dim is what the LLK function itself uses (edited)

4) Hey all, I had a bit of a silly conceptual question. But why do we have more than 1 dst register. Is it just so the sfpu and fpu can store intermediate results that it can access quickly?
* We have only one dst register, sometimes it is divided into two parts for double buffering, math writes to one, while packer packs the other one. In some cases you will read in the code that the dst has 16 registers, or something like that, the thing they are referring to is how many tiles of the particular datum type in the context could be fit into one dst register or say one part of the double buffered system.
* When you say double buffering, do you mean the partitions of the dst register get double buffered?
* like the first half might be currently written to by the math thread. And the second half is being packed the packer thread
* Also I'm not sure the double buffering works as intended, because the dst_register calls dont allow for both the math and packer thread to work on the data at the same time

```cpp
tile_regs_acquire();//math now owns dst, packer must not own
for(dst_location < 8){
  MATH(some_math_fpu_op(dst_location))
}
MATHtile_regs_commit());//math release all dst regs packer now owns dst
PACK(tile_regs_wait());//packer waits for all dst registers
for(dst_location < 8){
  PACK(pack_tile())
}
tile_regs_release();packer releases dst
```

With this current set of apis, it looks like the packer can't start working until all the math loops are finished.  
So I'm not sure it actually double buffers as intended
You would need something like
```cpp
tile_regs_acquire(); // Math now owns all dst
for (dst_location = 0; dst_location < 8; ++dst_location) {
    MATH(some_math_fpu_op(dst_location));
    MATH(tile_regs_commit(dst_location)); // Math releases specific dst_location
    // Packer now owns dst_location
    PACK(tile_regs_wait(dst_location));   // Packer waits for specific dst_location
    PACK(pack_tile(dst_location));
}
tile_regs_release(); // Packer releases all dst
```

* Also I use 8 in this case for num_dst_regs since I believe it can fit 8 bf16 tiles. 16 for bf8 and 4 for fp32 also are applicable
* Dst is 1024x16x16b or 512x16x32b  
[https://github.com/tenstorrent/tt-isa-documentation/blob/main/WormholeB0/TensixTile/TensixCoprocessor/README.md](https://github.com/tenstorrent/tt-isa-documentation/blob/main/WormholeB0/TensixTile/TensixCoprocessor/README.md)  
and gets sliced and diced in various different ways depending on setup (full or half dst and fp16 or fp32 data)
* You are looking at the sequence of code, but you need to keep in mind three threads are using that code, when the packer is working , math can go back to the top of the code and use the other half of the dest regiser.
* The 8 and 4 are views into how many tiles fit within dst
* thats true. But the packer gets blocked until the math thread hits the tile_regs_commit. So even though they are being executed on separate cores, you can consider the operations working in lock step and not parallel.
* think of your code in a loop, when the packer starts working math can go and work on the other half when packer starts working it will work on the part math worked on before, and when math starts working it will work on a different part
* To sum up the thread for anyone curious/reading. Anil and I hopped on a call, and he explained the concepts of full sync dst and half sync dst. I didn't realize half sync was an option
5) Hey guys, does anyone know what do `DstSync::Tile16` and `DstSync::Tile2` do and why would we use them instead of using `DstSync::SyncHalf` and `DstSync::SyncFull`?
* They refer to having dest sync over a tile or a face, those terms are safe to remove from the repo since they are fully deprecated and not supported at all
6) I'm working on a bit of a cleanup effort and I ran into the following:
```cpp
#ifdef PERF_DUMP
    #define DECOUPLINGS_EN (SKIP_UNP || MATH_PACK_DECOUPLE)
#else
    #define SKIP_UNP 0
    #define MATH_PACK_DECOUPLE 0
    #define DECOUPLINGS_EN 0
    #define OVERLAY_DECOUPLE 0
#endif

#if defined(EN_KERNEL_SLOWDOWN)
    #include "kernel_slowdown_config.h"
#endif

#ifndef INSERT_UNPACK_DELAY
    #define INSERT_UNPACK_DELAY 0
#endif

#ifndef INSERT_MATH_DELAY
    #define INSERT_MATH_DELAY 0
#endif

#ifndef INSERT_PACK_DELAY
    #define INSERT_PACK_DELAY 0
#endif

#define DELAY_EN (INSERT_UNPACK_DELAY || INSERT_PACK_DELAY || INSERT_MATH_DELAY)
```
* Are any of these still used? I seems that PERF_DUMP DECOUPLINGS_EN SKIP_UNP MATH_PACK_DECOUPLE OVERLAY_DECOUPLE are not used because there are many missing headers that are included when PERF_DUMP is defined, that I couldn't find mentions of in metal and llk (edited)
* Deep wiki is very good at answering these types of queries. Have you tried it ?
Looking at your code snippet, you're working with performance debugging and delay injection macros in the TT-Metal codebase . This appears to be part of the kernel compilation system that controls various debugging and performance measurement features .

## Code Analysis 

Your snippet defines several conditional compilation macros:

**Performance Dump Controls:**

- `PERF_DUMP` enables decoupling features for performance analysis 
- `DECOUPLINGS_EN` combines `SKIP_UNP` and `MATH_PACK_DECOUPLE`flags 
- `OVERLAY_DECOUPLE` is set to 0 when performance dumping is disabled 

**Kernel Slowdown System:**

- `EN_KERNEL_SLOWDOWN` conditionally includes `kernel_slowdown_config.h`

**Delay Injection Controls:**

- `INSERT_UNPACK_DELAY`, `INSERT_MATH_DELAY`, `INSERT_PACK_DELAY` default to 0 
- `DELAY_EN` combines all three delay flags 

## Connection to TT-Metal Debug System 

This code is directly related to TT-Metal's debug delay system. The delay injection functionality is implemented in the watcher/debug system: sanitize_noc.h:581-598

The debug delay system allows inserting artificial delays for debugging NOC transactions. The `debug_insert_delay()` function checks runtime configuration to determine if delays should be applied based on transaction type and RISC processor sanitize_noc.h:587-589

## Runtime Configuration 

The delay system is configured through the watcher server initialization, where delay masks are set up per core and RISC type: watcher_server.cpp:408-417

The system supports configuring delays for different transaction types (read, write, atomic) and different RISC processors through environment variables like `TT_METAL_WATCHER_DEBUG_DELAY`watcher.rst:236-245

## Usage in Kernel Compilation 

These macros are used during JIT compilation to conditionally enable debugging features. The kernel generation system processes defines and creates configuration headers: genfiles.cpp:171-173

**Notes**

The macros you're looking at control compile-time debugging features in TT-Metal kernels. The delay injection system is primarily used for debugging NOC (Network-on-Chip) transactions and can be configured at runtime through environment variables. The `PERF_DUMP`conditional compilation allows enabling performance analysis features that might impact normal operation.


7) FYI : After talking with Milos and Filip , it seems we can formalize a procedure kind of. If you have gone through the ideas for testing, and you work on a bug that is complex enough that you think it would not be caught in the normal testing methodology we follow, please open an issue describing such a bug so that we could adapt our testing mechanism. One such example is something like this. [https://github.com/tenstorrent/tt-llk/issues/482](https://github.com/tenstorrent/tt-llk/issues/482)TLDR: The idea is to set or unset all registers to true/false, then add perturbation and then run our test as usual. (edited)
8) Hey all, I was getting a weird bug in my ops kernel. And I think there is something wrong with the packer in this sectionRight now, when I print the destination register. The values all looks correct. But when printing the tile after it has been packed. After the 4th tile it seems to have gone bad?

```cpp
 56     for (uint32_t tile = 0; tile < cb_length; tile += num_dst_regs) {
 57         tile_regs_acquire();
 58         uint32_t blk = tile + num_dst_regs > cb_length ? cb_length - tile : num_dst_regs;
 59         cb_wait_front(cb_in, index + blk);
 60         for (uint32_t wtr = 0; wtr < blk; wtr++) {
 61             mul_tiles_bcast_scalar(cb_in, cb_scaler, index + wtr, 0, wtr);
 62         }
 63         if constexpr (pop_input) {
 64             cb_pop_front(cb_in, blk);
 65         } else {
 66             index += blk;
 67         }
 68         tile_regs_commit();
 69         tile_regs_wait();
 70         cb_reserve_back(cb_intermediate, blk);
 71         for (uint32_t wtr = 0; wtr < blk; wtr++) {
 72             if constexpr (pop_input) {
 73                 PACK(auto addr = get_local_cb_interface(cb_intermediate).fifo_wr_ptr + get_local_cb_interface(cb_intermediate).fifo_wr_tile_ptr - 1);
 74                 PACK(DPRINT << "TILE: "<< (tile + wtr) << ENDL());
 75                 PACK(DPRINT << "addr: "<< addr << ENDL());
 76                 PACK(DPRINT << "fifo_wr_ptr: "<<get_local_cb_interface(cb_intermediate).fifo_wr_ptr << ENDL());
 77                 PACK(DPRINT << "fifo_wr_tile_ptr: " <<get_local_cb_interface(cb_intermediate).fifo_wr_tile_ptr << ENDL() << ENDL() << ENDL());
 78             }
 79             pack_tile(wtr, cb_intermediate);
 80         }
 81         cb_push_back(cb_intermediate, blk);
 82         tile_regs_release();
 83     }
 84     if constexpr (pop_input) {
 85         cb_wait_front(cb_intermediate, cb_length);
 86         for (uint32_t tile = 0; tile < cb_length; tile++) {
 87                 DPRINT << "tile: "<< tile << ENDL();
 88                 UNPACK(tt::compute::common::print_full_tile(cb_intermediate, tile, true));
 89         }
 90     }
```

I added some code to print the pack_addr that pack_tile uses.And even though this cb should be completely empty and not need to loop around. I noticed that the fifo_wr_ptr decrements after the 4th tile.

  ```
1 TILE: 0
  2 addr: 33341
  3 fifo_wr_ptr: 33342
  4 fifo_wr_tile_ptr: 0
  5
  6
  7 TILE: 1
  8 addr: 33597
  9 fifo_wr_ptr: 33342
 10 fifo_wr_tile_ptr: 256
 11
 12
 13 TILE: 2
 14 addr: 33853
 15 fifo_wr_ptr: 33342
 16 fifo_wr_tile_ptr: 512
 17
 18
 19 TILE: 3
 20 addr: 34109
 21 fifo_wr_ptr: 33342
 22 fifo_wr_tile_ptr: 768
 23
 24
 25 TILE: 4
 26 addr: 24125
 27 fifo_wr_ptr: 24126
 28 fifo_wr_tile_ptr: 0
 29
 30
 31 TILE: 5
 32 addr: 24381
 33 fifo_wr_ptr: 24126
 34 fifo_wr_tile_ptr: 256
 35
 36
 37 TILE: 6
 38 addr: 24637
 39 fifo_wr_ptr: 24126
 40 fifo_wr_tile_ptr: 512
 41
 42
 43 TILE: 7
 44 addr: 24893
 45 fifo_wr_ptr: 24126
 46 fifo_wr_tile_ptr: 768
 47...

...
```

I was wondering if there any more ways I could investigate the state of the packer to make sure that it setup correctly.If anyone from the llk team could hop on a call to talk about this issue, it would be immensely appreciated (edited)
* `fifo_wr_ptr` gets decremented in `cb_push_back` if it exceeds the `fifo_limit` . If it does it will decrement by `fifo_size` . Maybe you can check how those two are being programmed as a first step?
* Ahh, I think I was able to find out why I was getting weird values. It seems to be a bug with the kernel debug tool print_full_tiles/tile_slice function.  

```cpp
inline void print_full_tile(uint32_t cb_id, uint32_t tile_id = 0, bool untilize = false) {
    DPRINT << "======" << ENDL();
    for (uint16_t r = 0; r < 32; ++r) {
        DPRINT << (uint)r << " : "
               << TileSlice(
                      cb_id,
                      tile_id,
                      SliceRange{
                          .h0 = (uint8_t)r,
                          .h1 = (uint8_t)(r + 1),
                          .hs = (uint8_t)1,
                          .w0 = (uint8_t)0,
                          .w1 = (uint8_t)32,
                          .ws = (uint8_t)1},
                      true,
                      untilize)
               << ENDL();
    }
    DPRINT << "++++++" << ENDL();
}
```

The function does not check to see if the tile_index passes the fifo_limit and instead moves into unowned memory. I added popfronts to try printing the cb right at the read pointer instead of letting the function try to calculate the index itself, the data looked good.Thanks for helping me with debugging if the rd and wr pointers were working correctly. Really appreciate it!
8) So quick question: I have to do an sfpu operation ( still working on that -0.0 case with fp16_b now ) and then treat that data as UInt16 . Basically I set all LLK formats to UInt16 and in that case I get some nonsense result, but when I run it with fp16_b and then immediately after that without result as UInt16 I get good results. Apparently I miss setting some option somewhere. Does anybody have any idea why this happens?
* I was planning to treat uint16 as bfloat16, Radomir and Syed said the only problem that could happen is that the hardware will treat -0 as 0 and the result of treating uint16 as bfloat16 would be different because of that. For that some zero flag needed to be disabled. Need to look at what flag they mentioned. Is your problem related to this ?
* I am trying to do reverse ![:smile:](https://a.slack-edge.com/production-standard-emoji-assets/14.0/apple-medium/1f604@2x.png) Since hw will treat it as 0 in some cases ( still trying to localize that issue, Milos sent me some registers to probe ). Since my op has no calculation just moving data around dest I wanted to preserve all bits since -0.0 is 0x8000 which is no special value for uint16 and because bits are bits should be normally packed out. I just need to find diff where some setting enables me to make test pass. Like everything becomes ok after running fp16_b and then uint16 but when I rest and run with uint16 seems like i hit wrong tile of dest. Maybe it has something to do with problem someone encountered when casting uint32 to uint16 and how ints are stored in dest
* Hope we can converge to common understanding of -0.0
* is the problem that -0.0 become 0.0 when trying to pack out from dest?
* It's similar but not exactly the same, for my issue when I tried to pack uint16 out of 32bit DEST, the packer would pack out the HI16 bits instead of the LO16. But it would look at the correct tiles
* can you give an example of a nonsense result you get? what is the value in Dest and then what does it become in L1?
* n/a
9) I am seeing a very weird behavior, for the bug i am working on, it reproduced an error at a certain iteration number on simulator, but not on the second run, it reproduced on another iteration on another run, ~~it seems there is a bug in the factory that would make the program read random data, but (fixed the bug) isnt the initialization for memory fixed to constant values when running the simulator ?~~
* I am not sure what could cause this seemingly random behaviour where different iterations fail on different runs. But, there were problems we encountered while running tests from the llk repo which would maybe be worth highlighting.  
* The simulator exposed a lack of sync between the brisc and the triscs, where the brisc would not start running, but the trics would, which caused brisc to never execute TTI_ZEROACC to zero out the values in the dest register. This was causing all the tests reading from dest to fail. This was solved by first taking the brisc out of reset and then in the brisc.cpp we would take the triscs out of reset.  
* The next problem was that the first iteration would pass but the following iterations would fail. This was happening because after trics complete their run, we read a 0x1 from the mailbox which indicates that trisc is complete. By the time the next test runs and the trisc.cpp sets the value in the mailbox to something other than 0x1 to indicate that the trisc kernel is not complete, the 0x1 would be read again and the test would essentially stop before it even ran. This was fixed by resetting the value in the mailbox inside conftest.py as a pytest fixture.
* Thnx for the detailed info ! If a python test runs two different kernels in succession, I assume that the timing between the two kernels is non-deterministic and might affect how the kernels are simulated right ? Could this be the case of non-determinism ?  Do you want me to create an issue for this ? Where should the issue be , in tt-metal ?
* If you think the issue is related to simulator functionality, it's best to create an issue at the following link: [https://yyz-gitlab.local.tenstorrent.com/tenstorrent/tt-umd-simulators/-/issues](https://yyz-gitlab.local.tenstorrent.com/tenstorrent/tt-umd-simulators/-/issues) and assign me.If it’s related to the tt-metal code itself, then it’s better to open a ticket and assign it to the appropriate code owner in the tt-metal repo.
* Issue created
8) Okay I modified the wormhole versim to use 2 cores, and when i simulate i see two cores and the dump also consists of two cores but one core does nothing. Then i did some printouts in the op factory and it turns out the number of cores the op sees when it queries using the function  

```cpp
    auto compute_with_storage_grid_size = device->compute_with_storage_grid_size();   
 auto [num_cores, all_cores, core_group_1, core_group_2, num_tiles_per_core_group_1, num_tiles_per_core_group_2] = split_work_to_cores(compute_with_storage_grid_size, num_tiles);
```

is still 1. What do i need to change to get this correct ?  
My branch is here [https://yyz-gitlab.local.tenstorrent.com/tenstorrent/tt-umd-simulators/-/commit/4b08eabd3730158d911c6c3289890518c9b55666](https://yyz-gitlab.local.tenstorrent.com/tenstorrent/tt-umd-simulators/-/commit/4b08eabd3730158d911c6c3289890518c9b55666)
* I will try different coordinates, maybe it's related to bad coordinates.
* test_multicore_kernel is failing for your coord:
```cpp
2015] dzivanovic:bgdepyc01-special-dzivanovic-for-reservation-6666 /localdev/dzivanovic/tt-metal >
TT_METAL_SLOW_DISPATCH_MODE=1 ./build/test/tt_metal/test_multi_core_kernel

2025-07-04 11:57:28.390 | info    | Device         | Opening user mode device driver (tt_cluster.cpp:190)
2025-07-04 11:57:28.395 | warning | SiliconDriver  | Chip 0 not found in cluster descriptor, creating mock cluster descriptor. (cluster.cpp:455)
2025-07-04 11:57:28.395 | info    | SiliconDriver  | Harvesting mask for chip 0 is 0x0 (NOC0: 0x0, simulated harvesting mask: 0x0). (cluster.cpp:282)
2025-07-04 11:57:28.400 | info    | EmulationDriver| Dialing: ipc:///tmp/dzivanovic_07-04-11:57:28_nng_ipc (tt_simulation_host.cpp:42)
2025-07-04 11:57:28.401 | info    | EmulationDriver| Instantiating simulation device (tt_simulation_device.cpp:56)
2025-07-04 11:57:28.403 | info    | EmulationDriver| Simulator process spawned with PID: 367808 (tt_simulation_device.cpp:78)
2025-07-04 11:57:28.410 | info    | EmulationDriver| Waiting for remote: Connection refused (tt_simulation_host.cpp:59)
2025-07-04 11:57:29.410 | info    | EmulationDriver| Waiting for ack msg from remote... (tt_simulation_device.cpp:91)
2025-07-04 11:57:29.499 | info    | Metal          | AI CLK for device 0 is: 0 MHz (metal_context.cpp:128)
2025-07-04 11:58:50.174 | info    | Metal          | Initializing device 0. Program cache is enabled (device.cpp:428)

2025-07-04 11:58:51.146 | error    | Test           | No core coordinate found at location: (0, 2, TENSIX, LOGICAL) (test_multi_core_kernel.cpp:377)
2025-07-04 11:58:51.146 | error    | Test           | System error message: Is a directory (test_multi_core_kernel.cpp:379)
2025-07-04 11:58:51.146 | critical | Always         | Test Failed (assert.hpp:107)

terminate called after throwing an instance of 'std::runtime_error'
  what():  TT_THROW @ /localdev/dzivanovic/tt-metal/tests/tt_metal/tt_metal/test_multi_core_kernel.cpp:385:
           tt::exception info: Test Failed

backtrace:
--- ./build/test/tt_metal/test_multi_core_kernel(+0x71a77) [0x55ac6602aa77]
--- ./build/test/tt_metal/test_multi_core_kernel(+0x6fde2) [0x55ac66028de2]
--- /lib/x86_64-linux-gnu/libc.so.6(+0x29d90) [0x7f0b24f5cd90]
--- /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0x80) [0x7f0b24f5ce40]
--- ./build/test/tt_metal/test_multi_core_kernel(+0x6d945) [0x55ac66026945]

[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] *** Process received signal ***
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] Signal: Aborted (6)
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] Signal code: (-6)

[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 0] /lib/x86_64-linux-gnu/libc.so.6(+0x42520) [0x7f0b24f75520]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 1] /lib/x86_64-linux-gnu/libc.so.6(pthread_kill+0x12c) [0x7f0b24fc99fc]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 2] /lib/x86_64-linux-gnu/libc.so.6(raise+0x16) [0x7f0b24f75476]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 3] /lib/x86_64-linux-gnu/libc.so.6(abort+0xd3) [0x7f0b24f5b7f3]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 4] /lib/x86_64-linux-gnu/libstdc++.so.6(+0xa2b9e) [0x7f0b25307b9e]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 5] /lib/x86_64-linux-gnu/libstdc++.so.6(+0xae20c) [0x7f0b2531320c]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 6] /lib/x86_64-linux-gnu/libstdc++.so.6(+0xae277) [0x7f0b25313277]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 7] /lib/x86_64-linux-gnu/libstdc++.so.6(+0xae4d8) [0x7f0b253134d8]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 8] ./build/test/tt_metal/test_multi_core_kernel(_ZN2tt6assert13tt_throw_implIJN3fmt3v117fstringIJEEEEEEvPKciS7_S7_DpRKT_+0x485) [0x55ac66090055]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [ 9] ./build/test/tt_metal/test_multi_core_kernel(+0x71a77) [0x55ac6602aa77]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [10] ./build/test/tt_metal/test_multi_core_kernel(+0x6fde2) [0x55ac66028de2]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [11] /lib/x86_64-linux-gnu/libc.so.6(+0x29d90) [0x7f0b24f5cd90]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [12] /lib/x86_64-linux-gnu/libc.so.6(__libc_start_main+0x80) [0x7f0b24f5ce40]
[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] [13] ./build/test/tt_metal/test_multi_core_kernel(+0x6d945) [0x55ac66026945]

[bgdepyc01-special-dzivanovic-for-reservation-6666:367777] *** End of error message ***
Aborted (core dumped)
```

* Ok
* Yes all the changes are pushed, how many cores is your test expecting ? Mine was expecting 2
* But the call to those functions returned 1, but the simulation dumped 2 files
* Ok, I understand now, this test was written with hardcoded coordinates, and that doesn't work for our setup.
* I have pushed changes on your branch for Simulator to use 3x3 grid size.  
Could you now run your test and let me know if it's working?
* The simulation hangs right at the start, i saw your changes, but from what little i understood about the topology before, the changes made seem to contradict my understanding.Like the noc cores and dram cores do not seem to be included in the 3x3 grid you created.
* Git branch in question:
### Add wormhole extra core dir
* Showing with 317 additions and 0 deletions
```cpp
versim/CMakeLists.txt

add_subdirectory(grayskull)
add_subdirectory(wormhole)
add_subdirectory(wormhole_b0)
add_subdirectory(wormhole_b0_3)

versim/wormhole_b0_3/CMakeLists.txt

set(VERSIM_ROOT "${PROJECT_SOURCE_DIR}/third_party/versim/versim")
set(WH_VERSIM_ROOT "${VERSIM_ROOT}/wormhole_b0")
if(EXISTS ${WH_VERSIM_ROOT})
    add_executable(versim-wormhole-b0_3 ../versim.cpp tt_versim_device.cpp)
    target_compile_options(versim-wormhole-b0_3 PRIVATE
        -Wno-deprecated-declarations
        -Wno-unused-value
        -Wno-expansion-to-defined
        -Wno-deprecated-builtins
        -Wno-non-c-typedef-for-linkage)
    set_target_properties(versim-wormhole-b0_3 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3"
    )
    target_link_libraries(versim-wormhole-b0_3 PRIVATE
        sanity_common
        c_verilated_hw
        ${WH_VERSIM_ROOT}/lib/versim-core.so
        ${VERSIM_ROOT}/required_libraries/libboost_filesystem.so.1.65.1     # this is not the right way to do this but oh well...
        ${VERSIM_ROOT}/required_libraries/libboost_system.so.1.65.1
        ${VERSIM_ROOT}/required_libraries/libboost_thread.so.1.65.1
        ${VERSIM_ROOT}/required_libraries/libboost_regex.so.1.65.1
        ${VERSIM_ROOT}/required_libraries/libicudata.so.60
        ${VERSIM_ROOT}/required_libraries/libicui18n.so.60
        ${VERSIM_ROOT}/required_libraries/libicuuc.so.60
        pthread # this is supposed to be 2.2.5 but it's picking up system glibc and nothing has broken down yet.
    )
    target_include_directories(versim-wormhole-b0_3 PRIVATE
        ${WH_VERSIM_ROOT}/headers/src/firmware/riscv/common
        ${WH_VERSIM_ROOT}/headers/vendor/tenstorrent-repositories/verilator/include
        ${WH_VERSIM_ROOT}/headers/vendor/tenstorrent-repositories/verilator/include/vltstd
        ${WH_VERSIM_ROOT}/headers/vendor/yaml-cpp/include
        ${WH_VERSIM_ROOT}/headers/vendor/fc4sc/includes
        ${WH_VERSIM_ROOT}/headers/vendor/tclap/include
        ${WH_VERSIM_ROOT}/headers/vendor/tenstorrent-repositories/range-v3/include
        ${WH_VERSIM_ROOT}/headers/src/hardware/tdma/tb/tile
        ${WH_VERSIM_ROOT}/headers/src/meta/wormhole_b0/instructions/inc
        ${WH_VERSIM_ROOT}/headers/src/meta/wormhole_b0/types/inc
        ${WH_VERSIM_ROOT}/headers/src/software/command_assembler/inc
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/common
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/versim-core
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/versim-core/common
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/versim-core/common/inc
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/versim-core/monitors
        ${WH_VERSIM_ROOT}/headers/src/t6ifc/versim-core/checkers
        ${WH_VERSIM_ROOT}/headers/src/tvm/inc
        #${WH_VERSIM_ROOT}/headers/usr_include
        ${WH_VERSIM_ROOT}/headers
        ${VERSIM_ROOT}/grayskull/headers/usr_include/   #   boost/config.hpp only exists in grayskull folder but is needed by every arch..
        ${BOOST_INCLUDE_DIRS}
    )
    add_custom_command(
        OUTPUT "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/soc_descriptor.yaml" "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/run.sh"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/versim/wormhole_b0_3/wormhole_b0_3_versim.yaml" "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/soc_descriptor.yaml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/scripts/versim_run.sh" "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/run.sh"
        DEPENDS "${PROJECT_SOURCE_DIR}/versim/wormhole_b0_3/wormhole_b0_3_versim.yaml" "${PROJECT_SOURCE_DIR}/scripts/versim_run.sh"
        COMMENT "Copying configuration files for versim-wormhole-b0_3"
    )
    add_custom_target(copy_versim_wormhole_b0_3_files ALL
        DEPENDS "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/soc_descriptor.yaml" "${CMAKE_BINARY_DIR}/versim-wormhole-b0_3/run.sh"
    )
    add_dependencies(versim-wormhole-b0_3 copy_versim_wormhole_b0_3_files)
else()
    message(WARNING "The directory ${WH_VERSIM_ROOT} does not exist. The versim-wormhole-b0_3 project will not be compiled.")
endif()

versim/wormhole_b0_3/tt_versim_device.cpp

// SPDX-FileCopyrightText: (c) 2023 Tenstorrent Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include <stdio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <optional>
#include "../tt_versim_device.hpp"
#include <boost/detail/sp_typeinfo.hpp>
#include "sim_interactive.h"
#include "command_assembler/soc.h"
namespace CA = CommandAssembler;
void translate_soc_descriptor_to_ca_soc(CA::Soc &soc) {
    CA::xy_pair worker_core_0(0, 3);
    CA::SocNocNode worker_node_0;
    worker_node_0.noc_coord = worker_core_0;
    worker_node_0.worker = true;
    worker_node_0.memory_size = 1499136;
    soc.SetNodeProperties(worker_node_0.noc_coord, worker_node_0);
    CA::xy_pair worker_core_1(0, 4);
    CA::SocNocNode worker_node_1;
    worker_node_1.noc_coord = worker_core_1;
    worker_node_1.worker = true;
    worker_node_1.memory_size = 1499136;
    soc.SetNodeProperties(worker_node_1.noc_coord, worker_node_1);
    /*
    CA::xy_pair worker_core_2(1, 2);
    CA::SocNocNode worker_node_2;
    worker_node_2.noc_coord = worker_core_2;
    worker_node_2.worker = true;
    worker_node_2.memory_size = 1499136;
    soc.SetNodeProperties(worker_node_2.noc_coord, worker_node_2);
*/
    CA::xy_pair dram_core_0(0, 0);
    CA::SocNocNode dram_node_0;
    dram_node_0.noc_coord = dram_core_0;
    dram_node_0.dram = true;
    dram_node_0.memory_size = 1073741824;
    dram_node_0.dram_channel_id = 0;
    soc.SetNodeProperties(dram_node_0.noc_coord, dram_node_0);
    CA::xy_pair router_core_0(0, 1);
    CA::SocNocNode router_node_0;
    router_node_0.noc_coord = router_core_0;
    router_node_0.router_only = true;
    router_node_0.memory_size = 0;
    soc.SetNodeProperties(router_node_0.noc_coord, router_node_0);
    CA::xy_pair router_core_1(0, 2);
    CA::SocNocNode router_node_1;
    router_node_1.noc_coord = router_core_1;
    router_node_1.router_only = true;
    router_node_1.memory_size = 0;
    soc.SetNodeProperties(router_node_1.noc_coord, router_node_1);
}
void tt_VersimDevice::start_device() {
    std::cout << "Start Versim Device " << std::endl;
    // see unroll_vcd_dump_cores in tt_device_params in UMD : 1-1 should be our only functional worker core
    // see also expand_plusargs in tt_device_params in UMD for what should be plusargs
    //std::vector<std::string> dump_cores = {"1-1"};
    std::vector<std::string> dump_cores = {"0-3", "0-4"};
    std::vector<std::string> plusargs = {};
    std::optional<std::string> vcd_suffix;
    if (dump_cores.size() > 0) {
    vcd_suffix = "core_dump.vcd";
    }
    std::vector<std::string> vcd_cores;
    // TODO: For now create a temporary stuff from CA and populate from descriptor before passing back to versim-core
    // interface. mainly bypasses arch_configs etc from llir.  We can populate soc directly
    // MT: have to preserve ca_soc_descriptor object since versim references it at runtime
    //CA::xy_pair CA_grid_size(2, 2);
    CA::xy_pair CA_grid_size(1, 5);
    CA::Soc ca_soc_manager(CA_grid_size);
    std::unique_ptr<CA::Soc> p_ca_soc_manager_unique = std::make_unique<CA::Soc>(CA_grid_size);
    translate_soc_descriptor_to_ca_soc(*p_ca_soc_manager_unique);
    // TODO: End
    std::cout << "Versim Device: turn_on_device ";
    /* Values in BBE
        TRISC0_SIZE = 20 * 1024        // 20KB = 16KB + 4KB local memory
        TRISC1_SIZE = 16 * 1024        // 16KB = 12KB + 4KB local memory
        TRISC2_SIZE = 20 * 1024        // 20KB = 16KB + 4KB local memory
        TRISC_BASE = NCRISC_FIRMWARE_BASE + NCRISC_FIRMWARE_SIZE = 20 * 1024 + 32 * 1024 = 52 * 1024
    */
    /* This Versim will be hardcoded based off values in Metal: 
        Each trisc is 16 * 1024 = 16384 = 16 kB
        Trisc0 base is: (2048 + 512) + 10 * 1024 = 12800
    */ 
    std::vector<std::uint32_t> trisc_sizes = {20*1024, 16*1024, 20*1024};
    std::uint32_t trisc_base = 52 * 1024;
    std::unique_ptr<versim::VersimSimulator> versim_unique = versim::turn_on_device(CA_grid_size, *p_ca_soc_manager_unique, plusargs, vcd_suffix, dump_cores, true, trisc_base, trisc_sizes);
    versim = versim_unique.release();
    std::cout << "Versim Device: write info to tvm db " << std::endl;
    versim::write_info_to_tvm_db(trisc_base, trisc_sizes);
    versim::build_and_connect_tvm_phase();
    versim->spin_threads(*p_ca_soc_manager_unique, false);
    versim::assert_reset(*versim);
    p_ca_soc_manager = (void*)(p_ca_soc_manager_unique.release());
    cores_in_reset = true;
    std::cout << "Versim Device: Done start " << std::endl;
}
void tt_VersimDevice::close_device() {
    std::cout << "Versim Device: Stop " << std::endl;
    versim::turn_off_device(*versim);
    versim->shutdown();
    // Force free of all versim cores
    for (auto x = 0; x < versim->grid_size.x; x++) {
        for (auto y = 0; y < versim->grid_size.y; y++) {
        delete versim->core_grid.at(x).at(y);
        }
    }
    std::cout << "Versim Device: Stop completed " << std::endl;
    delete versim;
}
void tt_VersimDevice::deassert_risc_reset() {
    if (cores_in_reset) {
        std::cout << "Versim Device: Deassert risc resets start" << std::endl;
        versim::handle_resetting_triscs(*versim);
        std::cout << "Versim Device: Start main loop " << std::endl;
        versim::startup_versim_main_loop(*versim);
        cores_in_reset = false;
    } else {
        std::cout << "Versim Device: Cores are already deasserted" << std::endl;
    }
}
void tt_VersimDevice::deassert_risc_reset_at_core(uint64_t core_x, uint64_t core_y) {
    deassert_risc_reset();
}
void tt_VersimDevice::assert_risc_reset() {
    if (!cores_in_reset) {
        std::cout << "Pause all the cores" << std::endl;
        versim::pause(*versim);
        std::cout << "Wait for cores to go to paused state" << std::endl;
        versim::sleep_wait_for_paused (*versim);
        std::cout << "Assert riscv reset" << std::endl;
        versim::assert_riscv_reset(*versim);
        cores_in_reset = true;
    } else {
        std::cout << "Versim Device: Cores are already asserted" << std::endl;
    }
}
void tt_VersimDevice::assert_risc_reset_at_core(uint64_t core_x, uint64_t core_y) {
    assert_risc_reset();
}
void tt_VersimDevice::write_to_device(std::vector<uint32_t> &vec, uint64_t core_x, uint64_t core_y, uint64_t addr) {
    CommandAssembler::xy_pair CA_target(core_x, core_y);
    CommandAssembler::memory CA_tensor_memory(addr, vec);
    nuapi::device::write_memory_to_core(*versim, CA_target, CA_tensor_memory);
}
void tt_VersimDevice::read_from_device(std::vector<uint32_t> &vec, uint64_t core_x, uint64_t core_y, uint64_t addr, uint32_t size) {
    CommandAssembler::xy_pair CA_target(core_x, core_y);
    size_t size_in_words = size / 4;
    auto result = nuapi::device::read_memory_from_core(*versim, CA_target, addr, size_in_words);
    vec = result;
}

versim/wormhole_b0_3/wormhole_b0_3_versim.yaml

grid:
  x_size: 1
  y_size: 5
arc:
  [ ]
pcie:
  [ ]
dram:
  [[0-0]]
dram_views:
  [
    {
      channel: 0,
      eth_endpoint: 0,
      worker_endpoint: 0,
      address_offset: 0
    }
  ]
dram_view_size:
  1073741824
eth:
  [ ]
functional_workers:
  [
   0-3, 0-4
  ]
harvested_workers:
  []
router_only:
  [
   0-1, 0-2
  ]
worker_l1_size:
  1499136
dram_bank_size:
  1073741824
eth_l1_size:
  262144
arch_name: WORMHOLE_B0
features:
  unpacker:
    version: 1
    inline_srca_trans_without_srca_trans_instr: False
  math:
    dst_size_alignment: 32768
  packer:
    version: 1
  overlay:
    version: 1
```

 11) Hey folks, we got this question in LLK channel in TT Discord:  
"I'm wondering if there are any obvious differences in `MOVD2B` for BH vs WH? e.g. does it use `ALU_FORMAT_SPEC_REG_SrcB_override` instead of `ALU_FORMAT_SPEC_REG_SrcA_override`?"[@Syed Gilani](https://tenstorrent.enterprise.slack.com/team/U06QSN795A6) you are most likely to know such details, but anyone with info, please chime in.
* task created!

