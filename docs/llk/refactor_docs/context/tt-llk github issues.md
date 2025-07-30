
### Adress all remaining Fast tilize implementation #412 comments #495
  

I agree with this, since tile size is unused it should be removed since we have llk_api wrappers around this?
I think for generality we shouldn't hardcode the else here to be Float16b, we should have it as Float16A or B depending on the input exponent width
Instead of Unpack_block_selection, can this be called something else like unpack_engine_select or something? or atleast a comment that explains this selects between Unpacker 0 and 1, otherwise it sounds like it has something to do with the "block" of tiles
Since the z_stride & w_stride changed for all default ops, was this change tested with all op sweeps in tt-metallium? There should be an explanation now that Z dimension is no longer tile dim
We are trying to move away from this idea of having "unset" or "revert" in the APIs using flags, it will cause confusion and possible debug failures with the API cleanup effort. to comment
Instead of hardcoding sizes, Id rather 256 & 512 are derived from the const values of NUM_FACES, TILE_C_DIM, TILE_R_DIM, etc
All these values should be const or constexpr
Instead of unit_dim, can we call this block_ct_dim or num_tiles_c_dim or some naming convention that already exists? Again this will cause confusion with the clean up effort since we are trying to move to a common naming convention
Im surprised that code size is not increased significantly with the non-constexpr if statements below. It would be much better for trisc code size and also for readability if you change the below to something like this:
Same note as above, should change unit_dim and num_uints into terminology that followings clean up naming conventions
non-constexpr is statements that increase code size with duplicate code, try to combine the duplicate functions like:
Same idea as above, we need to document anything that does a "uinint" because we are trying to move away from this model
Could we maybe create an issue, or otherwise note the ambiguities that we have within the LLKs, so that we can find the answers sometime in the future? I'm afraid that if we just remember, of even note down these things locally, we will forget about them and slow down our development going forward.
I am not sure about the code size but I disagree that ?: operator is more readable than if-else. I really like better the code bellow.

Can we use static_assert here or am I missing smth?

tt-metal [#21624](https://github.com/tenstorrent/tt-metal/issues/21624)
### Problem description
Conventional tilize algorithm using hardware tilizer has bad perf for some input shapes and less bad but still bad perf for other due to often being risc bound.
### What's changed

Implemented a completely new fast_tilize algorithm that uses all the execution units (unpackers, math and packers) to achieve ~100% utilization of the unpacker, actual e2e utilization is closer to 80% for cases of interest because of packer bottlenecks. For more details on the algorithm, implementation and its limitations see the metal issue and brownbag presentation.
### Type of change

- [x]  New feature (non-breaking change which adds functionality)

# Invalid number of cores being used to compute per core M/N and block sizes #25909
### Describe the bug

In create_matmul_1d_systolic_array_program_config(), num_cores is computed from the device's grid size (e.g., 8x8 for WH) is there is no user defined core coord. However, this can lead to invalid per_core_M/out_subblock_h and/or per_core_N/out_subblock_w combinations depending on M and N.

### Steps to reproduce the issue

See matmul_default_height_sharded pytest as an example. Use the inputs:

M = 5376  
N = K = 288  
num_cores = 10

### Expected behavior

The expected behavior is to use the largest # of cores possible that satisfy the constraints in validate(). So we should start with the device's entire grid and work down until we find a num cores that satisfies the constraints.

`pytest   tests/sweep_framework/sweeps/matmul/full/matmul_default_height_sharded.py`

# Investigate large variance in perf tests #477

When running perf tests for fast tilize `perf_fast_tilize.py`, the results indicate a large variance on a random subset of tests.  
Variance can be orders of magnitude larger than mean runtime of the test and tests which exibit this large variance change from run to run.



# Adress all remaining Fast tilize implementation #412 comments #495

  

I agree with this, since tile size is unused it should be removed since we have llk_api wrappers around this?
I think for generality we shouldn't hardcode the else here to be Float16b, we should have it as Float16A or B depending on the input exponent width
Instead of Unpack_block_selection, can this be called something else like unpack_engine_select or something? or atleast a comment that explains this selects between Unpacker 0 and 1, otherwise it sounds like it has something to do with the "block" of tiles
Since the z_stride & w_stride changed for all default ops, was this change tested with all op sweeps in tt-metallium? There should be an explanation now that Z dimension is no longer tile dim
We are trying to move away from this idea of having "unset" or "revert" in the APIs using flags, it will cause confusion and possible debug failures with the API cleanup effort. to comment
Instead of hardcoding sizes, Id rather 256 & 512 are derived from the const values of NUM_FACES, TILE_C_DIM, TILE_R_DIM, etc
All these values should be const or constexpr
Instead of unit_dim, can we call this block_ct_dim or num_tiles_c_dim or some naming convention that already exists? Again this will cause confusion with the clean up effort since we are trying to move to a common naming convention
Im surprised that code size is not increased significantly with the non-constexpr if statements below. It would be much better for trisc code size and also for readability if you change the below to something like this:
Same note as above, should change unit_dim and num_uints into terminology that followings clean up naming conventions
non-constexpr is statements that increase code size with duplicate code, try to combine the duplicate functions like:
Same idea as above, we need to document anything that does a "uinint" because we are trying to move away from this model
Could we maybe create an issue, or otherwise note the ambiguities that we have within the LLKs, so that we can find the answers sometime in the future? I'm afraid that if we just remember, of even note down these things locally, we will forget about them and slow down our development going forward.
I am not sure about the code size but I disagree that ?: operator is more readable than if-else. I really like better the code bellow.
Can we use static_assert here or am I missing smth?

`test_eltwise_unary_sfpu with Int32 breaks subsequent fast_tilize_tests`

# Is t6_mutex_acquire/release(mutex::REG_RMW) needed in LLK configs? #487

This mutex is used in unpack and pack threads to configure a few registers when calling the `cfg_reg_rmw_tensix` function:
[tt-llk/tt_llk_wormhole_b0/common/inc/cunpack_common.h](https://github.com/tenstorrent/tt-llk/blob/c2c44d24ae8ac7b7b46dc71db20284f9ee222d33/tt_llk_wormhole_b0/common/inc/cunpack_common.h#L250)
Line 250 in [c2c44d2](https://github.com/tenstorrent/tt-llk/commit/c2c44d24ae8ac7b7b46dc71db20284f9ee222d33)

|   |
|---|
|t6_mutex_acquire(mutex::REG_RMW);|

and

[tt-llk/tt_llk_wormhole_b0/common/inc/cpack_common.h](https://github.com/tenstorrent/tt-llk/blob/c2c44d24ae8ac7b7b46dc71db20284f9ee222d33/tt_llk_wormhole_b0/common/inc/cpack_common.h#L506)

Line 506 in [c2c44d2](https://github.com/tenstorrent/tt-llk/commit/c2c44d24ae8ac7b7b46dc71db20284f9ee222d33)

|   |
|---|
|t6_mutex_acquire(mutex::REG_RMW);|

and the respective calls for BH.
This issue is to track the investigation into whether these mutex are necessary, and if not, to remove them.

Just an FYI, the test case should introduce perturbation if we want to decide this empirically, but in many cases we would be able to theoretically know if a mutex would be needed at all or not. I saw some code blocks using mutexes, only they would be protected by mutexes, but if we are writing to small independent regions then we should not need them, but probably the reason for using the mutex in the first place was probably to make sure those snippets of code can be expanded in the future. And the coder assumed everybody would use such a mutex while configuring registers.

|   |   |   |
|---|---|---|
|+0 −4||[tt_llk_blackhole/common/inc/cpack_common.h](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-e14aeb60d4d01c1f2ff73a1c81c870e64b40be8335ab49092277e8c895e679c2)|
|+0 −3||[tt_llk_blackhole/common/inc/cunpack_common.h](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b3c64010046dc2c557fe947fb6b8e0ed31d136e4582af8e61ff494ccf019215d)|
|+0 −4||[tt_llk_wormhole_b0/common/inc/cpack_common.h](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-fd54b8fe5c088a26314bf8c7a9ccf33bec47d3c0a0169332e58fe7952fd0b5f7)|
|+0 −3||[tt_llk_wormhole_b0/common/inc/cunpack_common.h](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b22f58a0cad171f0d8d58279cedcbc99a04a4e0d9a950e2d667231896f7b97e8)|
```cpp
tt_llk_blackhole/common/inc/cpack_common.h

|   |
|---|
|@@ -395,8 +395,6 @@ inline void configure_pack(|
||
|set_packer_strides<untilize, tilize>(pack_src_format, pack_dst_format, tile_c_dim);|
||
|t6_mutex_acquire(mutex::REG_RMW);|
||
|// Set Fp8 E4M3 mode for packer|
|if ((pack_dst_format & 0x1F) == static_cast<DataFormatType>(DataFormat::Fp8_e4m3))|
|{|
|[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-e14aeb60d4d01c1f2ff73a1c81c870e64b40be8335ab49092277e8c895e679c2)|   |@@ -413,8 +411,6 @@ inline void configure_pack(|
|constexpr uint hw_relu_mask = STACC_RELU_ApplyRelu_MASK \| STACC_RELU_ReluThreshold_MASK;|
|cfg_reg_rmw_tensix<STACC_RELU_ApplyRelu_ADDR32, 0, hw_relu_mask>(hw_relu_config.val[0]);|
||
|t6_mutex_release(mutex::REG_RMW);|
||
|set_packer_config<is_fp32_dest_acc_en>(pack_src_format, pack_dst_format, num_faces, partial_face);|
||
|// PACK_COUNTERS_SEC0_pack_per_xy_plane = cfg_reg_array[3][0 +: 8];|
```

```cpp
tt_llk_blackhole/common/inc/cunpack_common.h
|   |
|---|
|@@ -252,7 +252,6 @@ inline void configure_unpack_AB(|
|(unpB_ch1_z_stride << UNP1_ADDR_CTRL_ZW_REG_1_Zstride_SHAMT); // Z and W(not used) stride for dest address (ch1)|
||
|// Math ALU_FORMAT_REG|
|t6_mutex_acquire(mutex::REG_RMW);| // deleted
|uint alu_src_format = (0x0 << ALU_FORMAT_SPEC_REG_SrcA_val_SHAMT);|
||
|constexpr uint mask0 = (1 << (ALU_FORMAT_SPEC_REG_Dstacc_override_SHAMT + 1)) - 1;|
|[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b3c64010046dc2c557fe947fb6b8e0ed31d136e4582af8e61ff494ccf019215d)[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b3c64010046dc2c557fe947fb6b8e0ed31d136e4582af8e61ff494ccf019215d)|   |@@ -309,8 +308,6 @@ inline void configure_unpack_AB(|
|cfg_reg_rmw_tensix<THCON_SEC1_REG1_Unp_LF8_4b_exp_RMW>(1);|
|}|
||
|t6_mutex_release(mutex::REG_RMW);| // deleted
||
|// Set tile descriptor|
|unpack_tile_descriptor_u tile_descriptor;|
|for (uint i = 0; i < TILE_DESC_SIZE; i++)|

```

```cpp
tt_llk_wormhole_b0/common/inc/cpack_common.h

|   |
|---|
|@@ -495,8 +495,6 @@ inline void configure_pack(|
||
|set_packer_strides(pack_src_format, pack_dst_format);|
||
|t6_mutex_acquire(mutex::REG_RMW);| // deleted
||
|const uint alu_dst_format = pack_src_format;|
||
|cfg_reg_rmw_tensix<ALU_FORMAT_SPEC_REG2_Dstacc_RMW>(alu_dst_format);|
|[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-fd54b8fe5c088a26314bf8c7a9ccf33bec47d3c0a0169332e58fe7952fd0b5f7)|   |@@ -509,8 +507,6 @@ inline void configure_pack(|
|constexpr uint hw_relu_mask = STACC_RELU_ApplyRelu_MASK \| STACC_RELU_ReluThreshold_MASK;|
|cfg_reg_rmw_tensix<STACC_RELU_ApplyRelu_ADDR32, 0, hw_relu_mask>(hw_relu_config.val[0]);|
||
|t6_mutex_release(mutex::REG_RMW);| // deleted
||
|set_packer_config<is_fp32_dest_acc_en>(pack_src_format, pack_dst_format, num_faces, partial_face);|
||
|set_packer_l1_offset(pack_dst_format, face_r_dim);|
```

```cpp
tt_llk_wormhole_b0/common/inc/cunpack_common.h

|   |
|---|
|@@ -247,7 +247,6 @@ inline void configure_unpack_AB(|
|(unpB_ch1_z_stride << UNP1_ADDR_CTRL_ZW_REG_1_Zstride_SHAMT); // Z and W(not used) stride for dest address (ch1)|
||
|// Math ALU_FORMAT_REG|
|t6_mutex_acquire(mutex::REG_RMW);| // deleted
|uint alu_src_format = (0x0 << ALU_FORMAT_SPEC_REG_SrcA_val_SHAMT);|
||
|constexpr uint mask0 = (1 << (ALU_FORMAT_SPEC_REG_Dstacc_override_SHAMT + 1)) - 1;|
|[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b22f58a0cad171f0d8d58279cedcbc99a04a4e0d9a950e2d667231896f7b97e8)[](https://github.com/tenstorrent/tt-llk/compare/1fb76abf65c59f5a132b7507b2ac00fb9fa1e985...745b58e782e8a1ba29439fd414f172da1b63c042#diff-b22f58a0cad171f0d8d58279cedcbc99a04a4e0d9a950e2d667231896f7b97e8)|   |@@ -293,8 +292,6 @@ inline void configure_unpack_AB(|
|}|
|cfg_reg_rmw_tensix<ALU_ACC_CTRL_Zero_Flag_disabled_src_RMW>(src_zeroflags_disable);|
||
|t6_mutex_release(mutex::REG_RMW);| // deleted
||
|// Set tile descriptor|
|unpack_tile_descriptor_u tile_descriptor;|
```

# Generalized Fused LLK template #463

In this step, unpack and matmul could be swapped with a different op.

*test infra project snnipets*

In order to do integer format sweep we need to be able to have visibility into device.  
We’re currently unable to reliably read integer data from dest registers on the Wormhole architecture due to a halting bug that prevents us from holding the core in place for memory reads.  
On Blackhole, we are able to use Exalens to read dest registers and extract integer data using the standard integer formats. However, on Wormhole, there is a hardware-level bug that prevents Exalens from halting the core, making memory reads unreliable or impossible in some cases.

To unblock myself from waiting on Exalens to support integer decoding on Wormhole, I attempted a workaround:

- Dump raw hex data from srcA and dest registers using tt-exalens functions that will read bytes regardless of format.
- Manually parse this data back into the intended format

While this approach technically works in some cases, it suffers from several limitations on Wormhole:

- MOVDBGA2D instruction (used by Exalens) truncates the lower 3 bits of the integer when copying to dest, resulting in loss of precision. After consulting with Marco Merlini he confirmed that this may be one potential bug. Small int8/uint8 values are particularly affected — if the magnitude is too small, it ends up in the truncated bits and appears as zero.
- Because of this truncation, there's very little flexibility or variety in testing values — most low-range values are wiped out, and data integrity is compromised.

There also appears to be a deeper hardware bug that affects the ability to accurately read or preserve lower-bit data on Wormhole. I consulted with Marco Merlini, who noted that the issue is likely rooted in the RTL, but it's complex and currently not well understood or easily traceable.

So we will wait on debug team to provide integer support for reading integer values before moving forward

# [Perf] Add support for executing performance benchmarks in CI #446

Currently, we can't run Performance Benchmarks in CI because we utilize multiple runners. This is because the performance infrastructure expects all test cases to be executed in a single call to pytest to correctly report the data.

We should create a separate workflow for performance testing, that executes e.g. nightly and utilizes only one runner. I am not sure why number of runners is important, but I might be not familiar with the way you want to execute these tests.

# Explore perf optimization possibilities in llk_pack_untilize for WH_B0 #391

Conv team is currently working on a model that is using llk_pack_untilize kernel.  
It would be of use to explore possibilities for performance optimization within LLK, primarily for bfp8 input dtype.
Proposed steps towards solution:
- Communicate needs, timeline and desired perf results with Conv team
* Explore algorithmic improvements (simulate if needed)
* Establish valid evaluation mechanism (single test, e2e, model test etc.)
    
We are already using this API extensively for all conv based models.
Here are the 3 main use case.
- Halo Op (Data movement op which precedes each conv2d op and prepares data for it)  
    Halo is using [pack_untilize_block](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/tt_metal/include/compute_kernel_api/untilize.h#L47) if tensor width < 8 tiles and [untilize_block](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/tt_metal/include/compute_kernel_api/pack_untilize.h#L40) if not to untilize the input tensor into  
    the convolution as conv2d works with RM input.  
    Main use-case is to untilize bfloat8 to bfloat16.
This is codepath is perf critical for every conv models.
- Conv2d Micro op
If user has requested RM output out of conv2d, conv2d compute kernel performance untilize operation fused with conv2d.  
For this we use [pack_untilize_dst](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/tt_metal/include/compute_kernel_api/pack_untilize.h#L90) if tensor with < 8 or [untilize_block](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/tt_metal/include/compute_kernel_api/pack_untilize.h#L40) if not.
This code path is not perf critical ATM as this is opt in and users avoid as it's slow (but maybe we can change this?)
- Pool2d (max_poo2d, avg_pool2d)  
    Here we on really critical path we have [pack_untilize_dst](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/tt_metal/include/compute_kernel_api/pack_untilize.h#L90) , but with a [specific setup](https://github.com/tenstorrent/tt-metal/blob/f578f4652e3e7950ac4e73aa70501e7bb137669f/ttnn/cpp/ttnn/operations/pool/generic/device/kernels/compute/pool_2d_multi_core.cpp#L47).  
    We output a single row, and data format is bfloat16 input nd bfloat16 output.
This is also on a critical path.
So there isn't any new development here, we already use this and it affects performance, if there is anything you can do to help out here I would really appreciate it!  
Current performance is a bit more than 2x from "theoretical limit". We have had a similar situation with tilize_block that someone is successfully addressing ATM.

# Add sinh implementation #216
### Ticket

[tenstorrent/tt-metal#22305](https://github.com/tenstorrent/tt-metal/issues/22305)
### Problem description
sinh not available in sfpu
### What's changed
add sinh implementation
### Type of change

- [ ]  Bug fix (non-breaking change which fixes an issue)
- [ ]  New feature (non-breaking change which adds functionality)
- [ ]  Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ]  Documentation update
### Checklist
- [ ]  [All post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/all-post-commit-workflows.yaml) CI passes
- [ ]  [Blackhole Post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/blackhole-post-commit.yaml) CI passes (if applicable)

`tt-metal/tt_metal/third_party/tt_llk/tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_trigonometry.h`
`/home/vbabic/vbabic/tt-metal/tt_metal/third_party/tt_llk/tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_trigonometry.h`

```cpp
// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

#include "ckernel_sfpu_log.h"
#include "ckernel_sfpu_sqrt.h"
#include "sfpi.h"

namespace ckernel
{
namespace sfpu
{

template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_sine_maclaurin_series_(sfpi::vFloat val)
{
    // Good for [-pi:pi]
    // Mclauren series = x - x^3/3! + x^5/5! - x^7/7! + x^9/9! - x^11/11!
    sfpi::vFloat tmp = val;
    // x
    sfpi::vFloat output = tmp;
    // x^3/3!
    tmp = tmp * val * val;
    output += -0.166666666 * tmp;
    // x^5/5!
    tmp = tmp * val * val;
    output += 0.0083333333 * tmp;
    // x^7/7!
    tmp = tmp * val * val;
    output += -0.0001984126 * tmp;
    if constexpr (not APPROXIMATION_MODE)
    {
        // x^9/9!
        tmp = tmp * val * val;
        output += 0.0000027557 * tmp;
        // x^11/11!
        tmp = tmp * val * val;
        output += -0.00000002505 * tmp;
    }

    // Write out output
    return output;
}

template <bool APPROXIMATION_MODE>
sfpi_inline sfpi::vFloat _sfpu_cosine_maclaurin_series_(sfpi::vFloat val)
{
    // Good for [-pi:pi]
    // Mclauren series = 1 - x^2/2! + x^4/4! - x^6/6! + x^8/8! - x^10/10! + x^12/12!
    // 1
    sfpi::vFloat output = 1.0f;
    // x^2/2!
    sfpi::vFloat tmp = val * val;
    output += -0.5 * tmp;
    // x^4/4!
    tmp = tmp * val * val;
    output += 0.0416666666 * tmp;
    // x^6/6!
    tmp = tmp * val * val;
    output += -0.0013888888 * tmp;
    if constexpr (not APPROXIMATION_MODE)
    {
        // x^8/8!
        tmp = tmp * val * val;
        output += 0.0000248015 * tmp;
        // x^10/10!
        tmp = tmp * val * val;
        output += -0.0000002755 * tmp;
    }

    // Write out output
    return output;
}

// Legacy implementation.
// Candidate for removal in future versions. See https://github.com/tenstorrent/tt-llk/issues/225 for more details.
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sine_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v             = sfpi::dst_reg[0];
        v                          = 0.318309886183791f * v; // *1/pi to get number of pi rads.
        sfpi::vInt whole_v         = float_to_int16(v, 0);
        sfpi::vFloat whole_v_float = int32_to_float(whole_v, 0);
        v                          = v - whole_v_float;
        v *= 3.141592653589793f; // fractional * pi to get it in [-pi:pi]
        v       = _sfpu_sine_maclaurin_series_<APPROXIMATION_MODE>(v);
        whole_v = whole_v & 0x1;
        v_if (whole_v != 0)
        {
            // odd so flip the sign
            v *= -1;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

// Legacy implementation.
// Candidate for removal in future versions. See https://github.com/tenstorrent/tt-llk/issues/225 for more details.
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_cosine_(const int iterations)
{
    // SFPU microcode
    for (int d = 0; d < iterations; d++)
    {
        sfpi::vFloat v             = sfpi::dst_reg[0];
        v                          = 0.318309886183791f * v; // *1/pi to get number of pi rads.
        sfpi::vInt whole_v         = float_to_int16(v, 0);
        sfpi::vFloat whole_v_float = int32_to_float(whole_v, 0);
        v                          = v - whole_v_float;
        v *= 3.141592653589793f; // fractional * pi to get it in [-pi:pi]
        v       = _sfpu_cosine_maclaurin_series_<APPROXIMATION_MODE>(v);
        whole_v = whole_v & 0x1;
        v_if (whole_v != 0)
        {
            // odd so flip the sign
            v *= -1;
        }
        v_endif;
        sfpi::dst_reg[0] = v;
        sfpi::dst_reg++;
    }
}

<<<<<<< Aswinmcw/LLK_sinh
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_sinh_()
=======
// https://en.wikipedia.org/wiki/Inverse_hyperbolic_functions#Definitions_in_terms_of_logarithms
// acosh(x) = log(x + sqrt(x^2 - 1))
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_acosh_()
>>>>>>> main
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
<<<<<<< Aswinmcw/LLK_sinh
        sfpi::vFloat v      = sfpi::dst_reg[0];
        sfpi::vFloat result = (_sfpu_exp_(v) - _sfpu_exp_(-v)) * 0.5f;
        sfpi::dst_reg[0]    = result;
=======
        sfpi::vFloat inp = sfpi::dst_reg[0];
        v_if (inp < sfpi::vConst1)
        {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN();
        }
        v_elseif (inp == sfpi::vConst1)
        {
            sfpi::dst_reg[0] = sfpi::vConst0;
        }
        v_else
        {
            sfpi::vFloat tmp = inp * inp;
            tmp              = tmp - sfpi::vConst1;
            tmp              = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(tmp);
            tmp              = tmp + inp;
            sfpi::dst_reg[0] = _calculate_log_body_no_init_(tmp);
        }
        v_endif;
>>>>>>> main
        sfpi::dst_reg++;
    }
}

<<<<<<< Aswinmcw/LLK_sinh
=======
// asinh(x) = log(x + sqrt(x^2 + 1))
template <bool APPROXIMATION_MODE, int ITERATIONS>
inline void _calculate_asinh_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat inp = sfpi::dst_reg[0];
        sfpi::vFloat tmp = inp * inp + sfpi::vConst1;
        tmp              = _calculate_sqrt_body_<APPROXIMATION_MODE, 2>(tmp);
        tmp              = tmp + sfpi::abs(inp);
        sfpi::dst_reg[0] = _calculate_log_body_no_init_(tmp);
        v_if (inp < sfpi::vConst0)
        {
            sfpi::dst_reg[0] = -sfpi::dst_reg[0];
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

// atanh[x] = 0.5 * ln((1 + x) / (1 - x))
template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en, int ITERATIONS>
inline void _calculate_atanh_()
{
    // SFPU microcode
    for (int d = 0; d < ITERATIONS; d++)
    {
        sfpi::vFloat inp     = sfpi::dst_reg[0];
        sfpi::vFloat abs_inp = sfpi::abs(inp);
        v_if (abs_inp > sfpi::vConst1)
        {
            sfpi::dst_reg[0] = std::numeric_limits<float>::quiet_NaN();
        }
        v_elseif (abs_inp == sfpi::vConst1)
        {
            sfpi::vFloat inf = std::numeric_limits<float>::infinity();
            sfpi::dst_reg[0] = sfpi::setsgn(inf, inp);
        }
        v_else
        {
            sfpi::vFloat num = sfpi::vConst1 + inp;
            sfpi::vFloat den = sfpi::vConst1 - inp;
            sfpi::vFloat tmp = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(den);
            tmp              = sfpi::setsgn(tmp, den);
            if constexpr (is_fp32_dest_acc_en || APPROXIMATION_MODE)
            {
                den = tmp;
            }
            else
            {
                den = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(tmp, 0));
            }
            num              = num * den;
            den              = _calculate_log_body_no_init_(num);
            sfpi::dst_reg[0] = 0.5f * den;
        }
        v_endif;
        sfpi::dst_reg++;
    }
}

template <bool APPROXIMATION_MODE>
void _init_inverse_hyperbolic_()
{
    _init_sqrt_<APPROXIMATION_MODE>();
}

template <bool APPROXIMATION_MODE>
void _init_atanh_()
{
    _init_reciprocal_<APPROXIMATION_MODE>();
}

>>>>>>> main
} // namespace sfpu
} // namespace ckernel
```

# Enable ZeroAcc Dest Bank Detection #18

* blackhole issue
In Blackhole, ZEROACC instruction is able to automatically detect which Dest bank is being currently used, and to detect that bank and use it in dest offset calculation.
Currently to preserve legacy behaviour, the auto-detect behaviour has been disabled:
```
    // Legacy mode for ZEROACC 
    cfg_reg_rmw_tensix<DEST_ACCESS_CFG_zeroacc_absolute_tile_mode_RMW>(1);
```
in function: `inline void _llk_math_hw_configure()`
After having full blackhole functionality, we should re-enable auto-detection, this will change functions from this:
```
TT_ZEROACC(ZERO_ACC_MODE, 0/*clear fp32*/, 0, ADDR_MOD_1, ((get_dest_buffer_base() >> 4) + (dst_index << 2)) + (0 +         n)); // Clear faces 0 & 1
```
to this:
```
TT_ZEROACC(ZERO_ACC_MODE, 0/*clear fp32*/, 0, ADDR_MOD_1, ((dst_index << 2)) + (0 +         n)); // Clear faces 
```

# [Code improvement] Stop using `using namespace` in header files #20

### Description

Using using namespace in header files is generally considered bad practice because it can lead to namespace pollution and unexpected name conflicts. When you include a header file that uses using namespace, it brings all the names from the specified namespace into the global namespace of any file that includes it. This can cause name collisions and make the code harder to understand and maintain. At the moment of writing this issue, we have 183 using directives in 149 files. Since this is a header-only project, I'd say we have this issue in almost 60% of files.

### Why This Is a Problem

**Namespace Pollution:** All names from the "used" namespaces are brought into the global namespace, which can lead to unintended name clashes.  
**Code Maintainability:** It becomes difficult to track where certain names are coming from, making the code harder to read and maintain.  
**Potential Conflicts:** If another header file or source file includes the same header and defines names that conflict with those in the namespace that was included using `using namespace`directive, it can lead to compilation errors or unexpected behavior.

### Solution

**Remove using namespace directives:** Remove the using namespace directives from all of the header files and fully qualify the names from namespaces where they are used.

```cpp
// fix: add remaining sfpi declarations

fix: add remaining sfpi declarations

|   |
|---|
|@@ -1632,7 +1632,7 @@ sfpi_test_noinline void test9()|
|// Relies on if chain above...|
|v_if (sfpi::dst_reg[0] >= 7.0F)|
|{|
|b = reinterpret<vUInt>(lz(a));|
|b = sfpi::reinterpret<vUInt>(lz(a));|
|v_if (b != 32)|
|{|
|sfpi::dst_reg[9] = 60.0F;|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)|   |@@ -1789,27 +1789,27 @@ sfpi_test_noinline void test10()|
|v_if (sfpi::dst_reg[0] == 7.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|sfpi::dst_reg[10] = setsgn(a, 1);|
|sfpi::dst_reg[10] = sfpi::setsgn(a, 1);|
|}|
|v_elseif (sfpi::dst_reg[0] == 8.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|sfpi::vFloat b = -128.0;|
|sfpi::vFloat r = setsgn(b, a);|
|sfpi::vFloat r = sfpi::setsgn(b, a);|
||
|sfpi::dst_reg[10] = r;|
|}|
|v_elseif (sfpi::dst_reg[0] == 9.0F)|
|{|
|sfpi::vFloat a = -256.0F;|
|sfpi::dst_reg[10] = setsgn(a, 0);|
|sfpi::dst_reg[10] = sfpi::setsgn(a, 0);|
|}|
|v_elseif (sfpi::dst_reg[0] == 10.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|a += 20.0f;|
|sfpi::vFloat b = -512.0F;|
|sfpi::vFloat r = setsgn(a, b);|
|sfpi::vFloat r = sfpi::setsgn(a, b);|
||
|sfpi::dst_reg[10] = r;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)|   |@@ -2234,7 +2234,7 @@ sfpi_test_noinline void test12(int imm)|
|v_if (sfpi::dst_reg[0] == 11.0F)|
|{|
|sfpi::vFloat a = 128.0;|
|sfpi::vFloat r = setsgn(a, imm - 36);|
|sfpi::vFloat r = sfpi::setsgn(a, imm - 36);|
|sfpi::dst_reg[12] = r;|
|}|
|v_elseif (sfpi::dst_reg[0] == 12.0F)|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-77b06c303488a862fc3d76de7330bb7134424793eeff8a58018c63e68620902f)|   |@@ -2570,7 +2570,7 @@ sfpi_test_noinline void test13(int imm)|
|sfpi::vFloat b = 220.0F;|
|v_if (sfpi::dst_reg[0] == 20.0F)|
|{|
|b = setsgn(a, 1);|
|b = sfpi::setsgn(a, 1);|
|}|
|v_endif;|
|v_if (sfpi::dst_reg[0] == 20.0F \| sfpi::dst_reg[0] == 21.0F)|

tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_binary.h

|   |
|---|
|@@ -67,7 +67,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|sfpi::vFloat val = pow * log_result;|
||
|// Force sign to 0 (make number positive)|
|sfpi::vFloat result = _sfpu_exp_(setsgn(val, 0));|
|sfpi::vFloat result = _sfpu_exp_(sfpi::setsgn(val, 0));|
||
|v_if (val < 0)|
|{|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-2b2e744a16df611042c5740438181e551fad52613c4392f326fff50118292ac8)|   |@@ -84,7 +84,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|// if pow is odd integer, set result to negative|
|v_if (pow_int & 0x1)|
|{|
|result = setsgn(result, 1);|
|result = sfpi::setsgn(result, 1);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-2b2e744a16df611042c5740438181e551fad52613c4392f326fff50118292ac8)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-2b2e744a16df611042c5740438181e551fad52613c4392f326fff50118292ac8)|   |@@ -133,7 +133,7 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|v_else|
|{|
|result = std::numeric_limits<float>::infinity();|
|result = setsgn(result, in0);|
|result = sfpi::setsgn(result, in0);|
|}|
|v_endif;|
|}|

tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_exp.h

|   |
|---|
|@@ -213,14 +213,14 @@ void _calculate_exponential_(const int iterations, uint16_t exp_base_scale_facto|
||
|// SHL to move integer bits to exponent|
|val_short <<= 10 - p_exp::FRAC_BITS;|
|sfpi::dst_reg[0] = reinterpret<sfpi::vFloat>(val_short);|
|sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(val_short);|
|}|
|v_endif;|
|}|
|else|
|{|
|// Force sign to 0 (make number positive)|
|sfpi::vFloat result = _sfpu_exp_(setsgn(val, 0));|
|sfpi::vFloat result = _sfpu_exp_(sfpi::setsgn(val, 0));|
||
|v_if (val < 0)|
|{|

tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_recip.h

|   |
|---|
|pi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat in)|
|{|
|// Force sign to 1 (make number negative)|
|sfpi::vFloat val = setsgn(in, 1);|
|sfpi::vFloat val = sfpi::setsgn(in, 1);|
||
|val = setexp(val, 126); // Set exponent to 126 to make the number in 0.5-1|
|// Use 1.44 as first guess at x, ideal value would be 1.33, but we happen to have 1.44 available, so use that to avoid a load|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-877ba27579f28d078bf0cbf23572a24a4744d3b6976bd15077c48f77299d3961)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-877ba27579f28d078bf0cbf23572a24a4744d3b6976bd15077c48f77299d3961)|   |@@ -71,7 +71,7 @@ inline void _calculate_reciprocal_(const int iterations)|
|}|
|else|
|{|
|sfpi::dst_reg[0] = reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));|
|sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(float_to_fp16b(out, 0));|
|}|
||
|sfpi::dst_reg++;|

tt_llk_wormhole_b0/common/inc/ckernel_sfpi.h

|   |
|---|
|// Relies on if chain above...|
|v_if (sfpi::dst_reg[0] >= 7.0F)|
|{|
|b = reinterpret<vUInt>(lz(a));|
|b = sfpi::reinterpret<vUInt>(lz(a));|
|v_if (b != 32)|
|{|
|sfpi::dst_reg[9] = 60.0F;|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)|   |@@ -1789,27 +1789,27 @@ sfpi_test_noinline void test10()|
|v_if (sfpi::dst_reg[0] == 7.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|sfpi::dst_reg[10] = setsgn(a, 1);|
|sfpi::dst_reg[10] = sfpi::setsgn(a, 1);|
|}|
|v_elseif (sfpi::dst_reg[0] == 8.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|sfpi::vFloat b = -128.0;|
|sfpi::vFloat r = setsgn(b, a);|
|sfpi::vFloat r = sfpi::setsgn(b, a);|
||
|sfpi::dst_reg[10] = r;|
|}|
|v_elseif (sfpi::dst_reg[0] == 9.0F)|
|{|
|sfpi::vFloat a = -256.0F;|
|sfpi::dst_reg[10] = setsgn(a, 0);|
|sfpi::dst_reg[10] = sfpi::setsgn(a, 0);|
|}|
|v_elseif (sfpi::dst_reg[0] == 10.0F)|
|{|
|sfpi::vFloat a = sfpi::dst_reg[0];|
|a += 20.0f;|
|sfpi::vFloat b = -512.0F;|
|sfpi::vFloat r = setsgn(a, b);|
|sfpi::vFloat r = sfpi::setsgn(a, b);|
||
|sfpi::dst_reg[10] = r;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)|   |@@ -2234,7 +2234,7 @@ sfpi_test_noinline void test12(int imm)|
|v_if (sfpi::dst_reg[0] == 11.0F)|
|{|
|sfpi::vFloat a = 128.0;|
|sfpi::vFloat r = setsgn(a, imm - 36);|
|sfpi::vFloat r = sfpi::setsgn(a, imm - 36);|
|sfpi::dst_reg[12] = r;|
|}|
|v_elseif (sfpi::dst_reg[0] == 12.0F)|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-e691649f534b6f61defbf0d2b7a18417399c37061edf6246991a9c85bf94ae16)|   |@@ -2570,7 +2570,7 @@ sfpi_test_noinline void test13(int imm)|
|sfpi::vFloat b = 220.0F;|
|v_if (sfpi::dst_reg[0] == 20.0F)|
|{|
|b = setsgn(a, 1);|
|b = sfpi::setsgn(a, 1);|
|}|
|v_endif;|
|v_if (sfpi::dst_reg[0] == 20.0F \| sfpi::dst_reg[0] == 21.0F)|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_binary.h

|   |
|---|
|@@ -34,7 +34,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|v_if (pow_rounded == pow)|
|{|
|// if pow is integer, set base to positive|
|base = setsgn(base, 0);|
|base = sfpi::setsgn(base, 0);|
|}|
|v_endif;|
||
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -48,7 +48,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|sfpi::vInt exp = exexp(base);|
|v_if (exp < 0)|
|{|
|exp = setsgn(~exp + 1, 1);|
|exp = sfpi::setsgn(~exp + 1, 1);|
|}|
|v_endif;|
|sfpi::vFloat expf = int32_to_float(exp, 0);|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -68,7 +68,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|sfpi::vFloat val = pow * log_result;|
||
|// Force sign to 0 (make number positive)|
|sfpi::vFloat result = _sfpu_exp_(setsgn(val, 0));|
|sfpi::vFloat result = _sfpu_exp_(sfpi::setsgn(val, 0));|
||
|v_if (val < 0)|
|{|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -85,7 +85,7 @@ sfpi_inline sfpi::vFloat _calculate_sfpu_binary_power_(sfpi::vFloat base, sfpi::|
|// if pow is odd integer, set result to negative|
|v_if (pow_int & 0x1)|
|{|
|result = setsgn(result, 1);|
|result = sfpi::setsgn(result, 1);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -134,7 +134,7 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|v_else|
|{|
|result = std::numeric_limits<float>::infinity();|
|result = setsgn(result, in0);|
|result = sfpi::setsgn(result, in0);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -144,7 +144,7 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|}|
|v_else|
|{|
|result = in0 * setsgn(_sfpu_reciprocal_<4>(in1), in1);|
|result = in0 * sfpi::setsgn(_sfpu_reciprocal_<4>(in1), in1);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/70/commits/0b3c12c6cdd04742239134628b3f3f51404b32ca#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |
```