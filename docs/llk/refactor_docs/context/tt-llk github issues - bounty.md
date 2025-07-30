There is some code in tt-metal repo that uses low level TT/TTI instructions which should actually belong to tt-llk repo.  
For some code we have such code for one architecture already in the tt-llk repo but for the other architecture it is in tt-metal repo. Ideally we would want to have the code inside tt-llk and expose it through apis.

For example the function

```
template <bool neginf_srcA = false, std::uint32_t reload_srcB = false, bool zero_srcA = false>
inline void llk_unpack_tilizeA_B(
```

For blackhole in file `tt_metal/hw/ckernels/blackhole/metal/llk_api/llk_unpack_tilize_api.h` was supposed to serve as an api with a call to a function in tt-llk for example as for wormhole_b0 counterpart it calls "void _llk_unpack_tilizeA_B_" inside tt-llk but for blackhole the code is inside the function that is supposed to be just delegating the task to tt-llk. There are many such examples. Below is a list of such files as of now (May-29-2025)

Fixing this will also help us in documenting our code as the same standard from one perspective would be applied to all llk functions and apis and we would be able maximize the decoupling of low level code from tt-metal.

- tt_metal/hw/ckernels/blackhole/metal/llk_api/llk_pack_api.h  
* llk_pack_init should only call _llk_pack_init_. Other calls should be moved to _llk_pack_init_ inside tt-llk repo.  
* llk_pack_untilize_init should only call _llk_pack_untilize_init_. Other calls should be moved to _llk_pack_untilize_init_inside tt-llk repo.  
* llk_pack_untilize_uninit should call _llk_pack_untilize_uninit_ from tt-llk. If it doesn't exist should be created, and body of llk_pack_untilize_uninit moved to it.
* tt_metal/hw/ckernels/blackhole/metal/llk_api/llk_unpack_untilize_api.h  
* llk_unpack_untilize_init should only call _llk_unpack_untilize_init_. If it doesn't exist should be created, and body of llk_unpack_untilize_init moved to it.  
* llk_unpack_untilize_uninit should only call _llk_unpack_untilize_uninit_. If it doesn't exist should be created, and body of llk_unpack_untilize_uninit moved to it.
* tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_pack_api.h  
Similar to above described for BH.
* tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_unpack_untilize_api.h  
    Similar to above described for BH.

# [Bounty $500] Remove extraneous NOPs for SFPSWAP. #102

Despite the docs saying otherwise, SFPSWAP does _not_ need to be followed by NOP (hardware will automatically do this even on Wormhole). SFPSWAP always takes 2 cycles, but removing any NOPs removes confusion for anyone reading LLK code, and potentially saves I$.

Suggest doing a check for unnecessary NOPs and removing them e.g. in topk LLK.

Presumably a similar but wider check can be done for Blackhole, which has scoreboarding, but for now this issue can just be for Wormhole.

I spoke with the Tensix design team, and the conclusion was that the Wormhole hardware implementation does not stall the ready signal after an SFPSWAP. This improvement has been incorporated into Blackhole. The recommendation is to keep the NOPs after the swap in place for Wormhole.

I'm not sure how you were testing this or what your observations were, but based on the information I received from the designers, a NOP is not required if the next instruction does not use the values produced by the swap. This is due to a 2-stage pipeline, which allows feeding another instruction immediately after the swap, as long as it doesn’t depend on the swap's results.

For example, this is OK:

```
SFPSWAP(L0, L1, L2) // swap L1 and L2 and store into L0
SFPIADD(L3, L4, L5)
SFPIADD(L0, L0, L3)
```

On the other hand, this isn't:

```
SFPSWAP(L0, L1, L2) // swap L1 and L2 and store into L0
SFPIADD(L0, L0, L3)
```

We should update the BH LLKs based on this information and remove the NOPs, as they are now superfluous and negatively impact performance.

Thanks for checking with the design team, though I admit I'm still skeptical and will run some tests again when I get time in the future (this is not a priority for me right now).

From memory, I found that SFPSWAP _always_ took 2 cycles, with or without a NOP. This supports the explanation by [@corsix](https://github.com/corsix) that SFPSWAP inserts a NOP automatically on WH. This contradicts your assertion above, where you can use a non-dependent instruction instead of a NOP.

(Side note: your examples should say `SFPSWAP(L0, L1)` or similar - there are only two registers that get swapped.)

  
Let's put some concrete testing and observations on the table then.

I'll start with **experiment 1**, based around the example I think you were trying to convey:

```c
TTI_REPLAY(0, 5, 0, 1); // Record the next 5 instructions...
TTI_SFPLOADI(1, 2, 10); // L1 = 10
TTI_SFPLOADI(2, 2, 20); // L2 = 20
TTI_SFPLOADI(3, 2, 30); // L3 = 30
TTI_SFPSWAP(0, 2, 3, 0); // SWAP(L2, L3)
TTI_SFPIADD(1, 1, 4, 5); // L4 = L1 + 1
TTI_REPLAY(0, 5, 0, 0); // Play back the recorded instructions
```

I'm wrapping the 5 instructions of interest in a replay. You can remove the two `TTI_REPLAY`instructions if you wish, but they're there to ensure that the 5 instructions of interest get executed as close together as possible - without the `TTI_REPLAY` instructions, someone might say "well, perhaps there's an idle cycle inserted somewhere due to the RISCV suffering an instruction cache miss, and that idle cycle is causing the result you're seeing". If I execute the above on the n300s I've got lying around, the resultant contents of the registers is:

```
L1 = 10
L2 = 30
L3 = 20
L4 = 11
```

This all executes as everyone expects; `SWAP(L2, L3)` has happened, and `L4 = L1 + 1` has happened, and neither instruction depends upon the results of the other, so nobody is contending that the result should be anything else.

---

Moving on to **experiment 2**, which is exactly the same as before, except changing `L4 = L1 + 1`to `L4 = L2 + 1`:

```c
TTI_REPLAY(0, 5, 0, 1); // Record the next 5 instructions...
TTI_SFPLOADI(1, 2, 10); // L1 = 10
TTI_SFPLOADI(2, 2, 20); // L2 = 20
TTI_SFPLOADI(3, 2, 30); // L3 = 30
TTI_SFPSWAP(0, 2, 3, 0); // SWAP(L2, L3)
TTI_SFPIADD(1, 2, 4, 5); // L4 = L2 + 1
TTI_REPLAY(0, 5, 0, 0); // Play back the recorded instructions
```

If I execute the above on the n300s I've got lying around, the resultant contents of the registers is:

```
L1 = 10
L2 = 30
L3 = 20
L4 = 31
```

We see that `SWAP(L2, L3)` has happened, as expected. The value in `L4` tells us that `L4 = L2 + 1` observed the value of `L2` from _after_ the `SWAP`. You say "On the other hand, this isn't [OK]", but the experiment shows it to be OK.

---

For completeness, **experiment 3**, trying `L4 = L3 + 1`:

```c
TTI_REPLAY(0, 5, 0, 1); // Record the next 5 instructions...
TTI_SFPLOADI(1, 2, 10); // L1 = 10
TTI_SFPLOADI(2, 2, 20); // L2 = 20
TTI_SFPLOADI(3, 2, 30); // L3 = 30
TTI_SFPSWAP(0, 2, 3, 0); // SWAP(L2, L3)
TTI_SFPIADD(1, 3, 4, 5); // L4 = L3 + 1
TTI_REPLAY(0, 5, 0, 0); // Play back the recorded instructions
```

If I execute the above on the n300s I've got lying around, the resultant contents of the registers is:

```
L1 = 10
L2 = 30
L3 = 20
L4 = 21
```

As with experiment 2, we see that `L4 = L3 + 1` observed the value of `L3` from _after_ the `SWAP`. Again, you say isn't OK, experiment says is OK.

---

Moving on to **experiment 4**, let's look at things from a different angle, and try to determine how many cycles elapse for every `SFPSWAP` instruction. I'm going to put two instructions in the replay buffer, configure a MOP to execute that sequence of two instructions 10000 times, and measure the elapsed cycle count before and afterwards:

```c
// See https://www.corsix.org/content/tt-wh-part5 for explanation of MOP_CFG fields
// End effect is the MOP will execute loop_body_insn 10000 times.
#define set_mop_cfg(i, val) (*(volatile uint32_t*)(TENSIX_MOP_CFG_BASE + (i)*4) = (val))
uint32_t loop_body_insn = TT_OP_REPLAY(0, 2, 0, 0); // Play back two instructions from the replay buffer
set_mop_cfg(0, 100);
set_mop_cfg(1, 100);
set_mop_cfg(2, TT_OP_NOP);
set_mop_cfg(3, TT_OP_NOP);
set_mop_cfg(4, TT_OP_NOP);
set_mop_cfg(5, loop_body_insn);
set_mop_cfg(6, TT_OP_NOP);
set_mop_cfg(7, loop_body_insn);
set_mop_cfg(8, loop_body_insn);

TTI_REPLAY(0, 2, 0, 1); // Record the next 2 instructions...
TTI_SFPSWAP(0, 1, 2, 0); // SWAP(L1, L2)
TTI_SFPSWAP(0, 3, 4, 0); // SWAP(L3, L4)
TTI_LOADREG(0, 0x121F0 >> 2); // GPR[0] = RISCV_DEBUG_REG_WALL_CLOCK_L
TTI_MOP(1, 0, 0);
TTI_LOADREG(1, 0x121F0 >> 2); // GPR[1] = RISCV_DEBUG_REG_WALL_CLOCK_L
tensix_sync();
uint32_t t0 = *(volatile uint32_t*)(REGFILE_BASE + 0);
uint32_t t1 = *(volatile uint32_t*)(REGFILE_BASE + 4);
printf("%u (%u -> %u)\n", t1 - t0, t0, t1);
```

If I execute the above on the n300s I've got lying around, it prints:

```
40001 (123878813 -> 123918814)
```

In other words, ~20000 instructions were executed (`SWAP(L1, L2)`, `SWAP(L3, L4)`, `SWAP(L1, L2)`, `SWAP(L3, L4)`, `SWAP(L1, L2)`, `SWAP(L3, L4)`, ...), but ~40000 cycles elapsed. There's going to be +/- 10 cycles of measurement error, partly due to a 1 cycle delay switching out of the MOP, partly due to the 2nd `TTI_LOADREG` not waiting for the SFPU to drain, but +/- 10 cycles is fine when the numbers we're looking at are > 10000. This measurement forms the first row of the following table, and other rows are formed by varying the two instructions after `TTI_REPLAY(0, 2, 0, 1)`:

|Instruction sequence (repeated 10000 times)|Total instructions executed|Total cycles elapsed|
|---|---|---|
|`TTI_SFPSWAP(0, 1, 2, 0); // SWAP(L1, L2)`  <br>`TTI_SFPSWAP(0, 3, 4, 0); // SWAP(L3, L4)`|~20000|~40000|
|`TTI_SFPSWAP(0, 1, 2, 0); // SWAP(L1, L2)`  <br>`TTI_SFPNOP; // NOP`|~20000|~20000|
|`TTI_SFPSWAP(0, 1, 2, 0); // SWAP(L1, L2)`  <br>`TTI_SFPIADD(1, 3, 4, 5); // L4 = L3 + 1`|~20000|~30000|
|`TTI_SFPIADD(1, 3, 4, 5); // L4 = L3 + 1`  <br>`TTI_SFPIADD(1, 3, 4, 5); // L4 = L3 + 1`|~20000|~20000|
|`TTI_SFPNOP; // NOP`  <br>`TTI_SFPNOP; // NOP`|~20000|~20000|

These numbers are entirely consistent with the theory that SFPSWAP takes two cycles to execute, unless it is followed by SFPNOP, in which case it takes one and the SFPNOP takes one. Or, in other words, that a one cycle stall is automatically inserted after SFPSWAP if it isn't followed by SFPNOP. If you're putting forward a different theory, it also needs to be consistent with these numbers.

Note that SFPSWAP "takes two cycles" in a rather different way to how SFPMAD "takes two cycles". For SFPMAD, what you're saying is exactly the case: it is indeed not OK for the instruction immediately after SFPMAD to try to consume the result of the SFPMAD. For bonus fun, some modes of SFPSHFT2 "take two cycles", and do so in yet another different way: if the instruction after such a SFPSHFT2 _isn't_ SFPNOP, then very bad things can happen, depending on exactly what that instruction is.

It seems BH and WH have identical implementation wrt stalling ready signal after SFP SWAP. I'll recheck with the design team, as it seems that our specification is really in contradiction with the hardware implementation.

[@corsix](https://github.com/corsix) Thanks for the detailed analysis — it really helped clear up my doubts. 🙇

* 2 commits made, remove extraneous SFPSWAP NOPs for both blackhole and wormhole
```cpp
// code changes:

tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_sfpu/ckernel_sfpu_binary_max_min.h

|   |
|---|
|@@ -23,7 +23,6 @@ inline void calculate_binary_max_min(const uint dst_offset) {|
||
|// Swap and store maximum in lreg1, minimum in lreg0|
|TTI_SFPSWAP(0, p_sfpu::LREG1, p_sfpu::LREG0, 1);|
|TTI_SFPNOP;|
||
|if constexpr (IS_MAX_OP) {|
|TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_3, 0);|
|[](https://github.com/tenstorrent/tt-metal/pull/21646/commits/932015daa70fa3a700942f84ce904c28a2c6b6f6#diff-8b10f80640f71419a34e5fe259ba76715bd261d5f1d6ee52f5cde9ce94ff4834)|   |@@ -45,7 +44,6 @@ inline void calculate_binary_max_min_int32(const uint dst_offset) {|
||
|// Swap and store maximum in lreg1, minimum in lreg0|
|TTI_SFPSWAP(0, p_sfpu::LREG1, p_sfpu::LREG0, 1);|
|TTI_SFPNOP;|
||
|if constexpr (IS_MAX_OP) {|
|TTI_SFPSTORE(p_sfpu::LREG1, 12, ADDR_MOD_3, 0);|

tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_sfpu/ckernel_sfpu_unary_max_min.h

|   |
|---|
|#include "ckernel.h"|
|#include "ckernel_defs.h"|
|#include "noc_nonblocking_api.h"|
||
|using namespace sfpi;|
||
|[](https://github.com/tenstorrent/tt-metal/pull/21646/commits/932015daa70fa3a700942f84ce904c28a2c6b6f6#diff-dce077eefff76b34d4bcc202aae126f19efa6609788ad4ad829d66021cd01d3a)|   |@@ -15,18 +14,22 @@ namespace sfpu {|
||
|template <bool IS_MAX_OP = true, bool APPROXIMATION_MODE, int ITERATIONS = 8>|
|inline void calculate_unary_max_min(uint value) {|
||
|// Load value param to lreg2|
|TT_SFPLOADI(p_sfpu::LREG2, 10, value & 0xFFFF);|
|TT_SFPLOADI(p_sfpu::LREG2, 8, value >> 16);|
||
|#pragma GCC unroll 0|
|for (int d = 0; d < ITERATIONS; d++) {|
|// Load value param to lreg1|
|TT_SFPLOADI(p_sfpu::LREG1, 10, value & 0xFFFF);|
|TT_SFPLOADI(p_sfpu::LREG1, 8, value >> 16);|
||
|// Load input to lreg0|
|TTI_SFPLOAD(p_sfpu::LREG0, 0, ADDR_MOD_3, 0);|
||
|// Copy value param to lreg1|
|TTI_SFPMOV(0, p_sfpu::LREG2, p_sfpu::LREG1, 0);|
||
|// Swap and store maximum in lreg1, minimum in lreg0|
|TTI_SFPSWAP(0, p_sfpu::LREG1, p_sfpu::LREG0, 1);|
|TTI_SFPNOP;|
||
|if constexpr (IS_MAX_OP) {|
|TTI_SFPSTORE(p_sfpu::LREG1, 0, ADDR_MOD_3, 0);|
```

# Rewrite {recip,rsqrt,sqrt} LLKs. #230

|~~ut inf handling~~) New operations|
|---|
|recip|false|27|~~14~~ 22 (or 21 on BH)|
|recip|true|23|~~10~~ 18 (or 17 on BH)|
|sqrt|false|30|25|
|sqrt|true|5|~~12~~ 13|
|rsqrt|false|100?|~~24~~ 29|
|rsqrt|true|100?|~~12~~ 17|

The relatively poor performance of the old rsqrt is partly due to a high number of iterations used, but even with a more sensible number of iterations, it will be slower and have worse precision than the new version.

The only case where the new sqrt takes more operations is `approx_mode=true` (12 vs 5), but the old approximate sqrt has worse precision.

### Type of change

- [ ]  Bug fix (non-breaking change which fixes an issue)
- [ ]  New feature (non-breaking change which adds functionality)
- [ ]  Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ]  Documentation update

### Checklist

- [ ]  [All post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/all-post-commit-workflows.yaml) CI passes
- [ ]  [Blackhole Post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/blackhole-post-commit.yaml) CI passes (if applicable)

`[tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_recip.h](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-d0b7e05be8585c2142dc41d1159b97221aa4a8637894868b2c412c5acfffe0da)`

Can you try adding the tests following the same approach as in [#101](https://github.com/tenstorrent/tt-llk/pull/101)?  
See [https://github.com/tenstorrent/tt-llk/blob/main/tests/README.md](https://github.com/tenstorrent/tt-llk/blob/main/tests/README.md) on how to run the tests. One note: at the moment automatic detection of cards is broken, I'm looking into it, but it should work if you `export CHIP_ARCH=wormhole`.  
These tests are checking pcc, not ideal thing, but should be enough for now. We plan to start checking the ULPs, but we're not there yet.

I'm confused why Sin/Cos tests are failing as I haven't touched them:

```
>           assert torch.isclose(
                golden_tensor[i], res_tensor[i], rtol=rtol, atol=atol
            ), f"Failed at index {i} with values {golden[i]} and {res_from_L1[i]}"
E           AssertionError: Failed at index 0 with values 0.937653277514828 and 131008.0
E           assert tensor(False)
E            +  where tensor(False) = <built-in method isclose of type object at 0x7f2937f86bc0>(tensor(0.94, dtype=torch.bfloat16), tensor(131072., dtype=torch.bfloat16), rtol=0.1, atol=0.05)
E            +    where <built-in method isclose of type object at 0x7f2937f86bc0> = torch.isclose

test_eltwise_unary_sfpu.py:169: AssertionError
```

Edit: looks like they break on main too, so nothing to do with me!

That’s odd. They were passing yesterday.

I tried a fresh clone of tt-llk, ran `setup_external_testing_env.sh`, activated venv, `cd python_tests`, and this fails:

```
pytest 'test_eltwise_unary_sfpu.py::test_eltwise_unary_sfpu[unpack_src=Float16 | pack_dst=Float32 | dest_acc=Yes | approx_mode=true | mathop=Cos]'
```

Let's disable it for now. I can take care of that.

I've only disabled the Cos tests, which were failing for some reason. Still quite confused by what's causing that.

I've updated the description with some performance numbers. The new methods are faster in almost all cases, and much more precise in all cases.

```cpp
// Code changes
tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_binary.h

|   |
|---|
|@@ -144,7 +144,7 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|}|
|v_else|
|{|
|result = in0 * sfpi::setsgn(_sfpu_reciprocal_<4>(in1), in1);|
|result = in0 * sfpi::setsgn(_sfpu_reciprocal_(in1), in1);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -165,9 +165,14 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|template <bool APPROXIMATION_MODE /*unused*/, BinaryOp BINOP>|
|inline void _sfpu_binary_init_()|
|{|
|if constexpr (BINOP == BinaryOp::DIV \| BINOP == BinaryOp::POW)|
|if constexpr (BINOP == BinaryOp::DIV)|
|{|
|_init_reciprocal_<APPROXIMATION_MODE>();|
|_init_reciprocal_<false>();|
|}|
|if constexpr (BINOP == BinaryOp::POW)|
|{|
|// note: calls _init_reciprocal_|
|_init_exponential_<false>();|
|}|
|}|
||

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_exp.h

|   |
|---|
|@@ -25,7 +25,7 @@ sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val)|
|v_endif;|
||
|// Run series in Horner form|
|sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(0.863281);|
|sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::vConstFloatPrgm2;|
|val = val * tmp + sfpi::vConst1;|
||
|v_if (exp >= 0)|
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-44cee679e6a137340275f903f5ba2e3cc002a167ada7ebbb09d3aad3e5529773)[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-44cee679e6a137340275f903f5ba2e3cc002a167ada7ebbb09d3aad3e5529773)|   |@@ -360,8 +360,7 @@ inline void _init_exponential_()|
|}|
|else|
|{|
|sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip|
|sfpi::vConstFloatPrgm1 = 2.0f;|
|_init_reciprocal_<APPROXIMATION_MODE>();|
|sfpi::vConstFloatPrgm2 = 0.863281f;|
|}|
|}|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_gelu.h

|   |
|---|
|@@ -189,10 +189,6 @@ inline void _init_gelu_()|
|template <bool APPROXIMATION_MODE>|
|inline void _init_gelu_derivative_()|
|{|
|sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip|
|sfpi::vConstFloatPrgm1 = 2.0f;|
|sfpi::vConstFloatPrgm2 = 0.863281f;|
||
|uint imm0;|
|uint imm1;|
|uint imm2;|
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-8bdbc08208a9a1e2a653d6c28bfab96522e3179542ad86f538aa49aa33ffcb8a)[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-8bdbc08208a9a1e2a653d6c28bfab96522e3179542ad86f538aa49aa33ffcb8a)|   |@@ -230,6 +226,8 @@ inline void _init_gelu_derivative_()|
|}|
|else|
|{|
|_init_exponential_<false>();|
||
|imm0 = 0x28FF;|
|imm1 = 0x3020;|
|_sfpu_load_imm16_(0, imm0);|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_recip.h

|   |   |   |
|---|---|---|
||   |   |
|@@ -1,4 +1,5 @@|
|// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-d0b7e05be8585c2142dc41d1159b97221aa4a8637894868b2c412c5acfffe0da)|   |@@ -11,61 +12,37 @@ namespace ckernel|
|namespace sfpu|
|{|
||
|template <int max_iter = 3>|
|// See: Cezary J. Walczyk, Leonid V. Moroz, Volodymyr Samotyy, and Jan L. Cieśliński.|
|// Optimal Approximation of the 1/x Function using Chebyshev Polynomials and Magic Constants.|
|// https://doi.org/10.1145/3708472|
||
|template <bool APPROXIMATE = false>|
|sfpi_inline sfpi::vFloat _sfpu_reciprocal_(const sfpi::vFloat in)|
|{|
|// Force sign to 1 (make number negative)|
|sfpi::vFloat val = sfpi::setsgn(in, 1);|
||
|val = setexp(val, 126); // Set exponent to 126 to make the number in 0.5-1|
|// Use 1.44 as first guess at x, ideal value would be 1.33.|
|// Grayskull has hardwired 1.44 and uses it to avoid a load.|
|// We use it here for consistency.|
|sfpi::vFloat vConstLn2Recip = sfpi::vConstFloatPrgm0;|
|sfpi::vFloat two = sfpi::vConstFloatPrgm1;|
|sfpi::vFloat result = vConstLn2Recip * (val * vConstLn2Recip + two);|
||
|for (int s_iter = 0; s_iter < (max_iter - 1); s_iter++)|
|{|
|result = result * (val * result + two);|
|}|
|sfpi::vFloat negative_x = -in;|
|sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - sfpi::reinterpret<sfpi::vInt>(in));|
|sfpi::vFloat t = sfpi::vConstFloatPrgm2 + negative_x * y;|
|y = y * sfpi::vConstFloatPrgm1;|
|y = y * t;|
||
|sfpi::vInt orig_exp = exexp(in);|
|sfpi::vInt new_exp = exexp(result);|
||
|// "Subtract" exponents, and re-bias.|
|// Execute: -1 - exp, then exp += 127|
|new_exp -= orig_exp;|
|new_exp += 126;|
||
|v_if (new_exp < 0)|
|if constexpr (!APPROXIMATE)|
|{|
|// If rebiased exponent is negative, we need to saturate at 0.|
|// This means the initial number was too big so reciprocal result should be 0|
|result = 0.0F;|
|new_exp = 0;|
|// 2nd iteration of Newton-Raphson|
|t = y * negative_x + sfpi::vConst1;|
|y = y * t + y;|
|}|
|v_endif;|
||
|// Set newly denormalized exponent to result exponent field|
|return setexp(result, new_exp);|
|return y;|
|}|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS, bool is_fp32_dest_acc_en = true>|
|template <bool APPROXIMATION_MODE, bool is_fp32_dest_acc_en = true>|
|inline void _calculate_reciprocal_(const int iterations)|
|{|
|#pragma GCC unroll 8|
|for (int d = 0; d < iterations; d++)|
|{|
|sfpi::vFloat in = sfpi::dst_reg[0];|
|sfpi::vFloat out = _sfpu_reciprocal_<APPROXIMATION_MODE ? 2 : 3>(in);|
||
|v_if (in < 0.0F)|
|{|
|// Invert sign on calculated value if CC=1 (number is negative)|
|out = -out;|
|}|
|v_endif;|
|sfpi::vFloat out = _sfpu_reciprocal_<APPROXIMATION_MODE>(in);|
||
|if constexpr (is_fp32_dest_acc_en \| APPROXIMATION_MODE)|
|{|
|**jasondavies** marked this conversation as resolved.|   |   |
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-d0b7e05be8585c2142dc41d1159b97221aa4a8637894868b2c412c5acfffe0da)|   |@@ -83,8 +60,9 @@ inline void _calculate_reciprocal_(const int iterations)|
|template <bool APPROXIMATION_MODE>|
|inline void _init_reciprocal_()|
|{|
|sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip|
|sfpi::vConstFloatPrgm1 = 2.0f;|
|sfpi::vConstIntPrgm0 = 0x7eb504f3;|
|sfpi::vConstFloatPrgm1 = 1.94090888923f;|
|sfpi::vConstFloatPrgm2 = 1.43566017178f;|
|}|
||
|} // namespace sfpu|
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-d0b7e05be8585c2142dc41d1159b97221aa4a8637894868b2c412c5acfffe0da)|   |


tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_sqrt.h

|   |   |   |
|---|---|---|
||   |   |
|SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC|
|// SPDX-FileCopyrightText: © 2025 Jason Davies <jason@jasondavies.com>|
|//|
|// SPDX-License-Identifier: Apache-2.0|
||
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-fba39c46447b08439ccb05ec2814a3d3638894d13e04f63a3fd4e1ab4a3279f7)|   |@@ -12,48 +13,58 @@ namespace ckernel|
|namespace sfpu|
|{|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS, int RECIPROCAL_ITERATIONS>|
|inline void _calculate_sqrt_(const int iterations)|
|// See: Kokosiński, Z., Gepner, P., Moroz, L. et al.|
|// Fast and accurate approximation algorithms for computing floating point square root. Numer Algor (2024).|
|// https://doi.org/10.1007/s11075-024-01932-7|
||
|template <bool APPROXIMATE = false, bool RECIPROCAL = false>|
|sfpi_inline sfpi::vFloat _sfpu_sqrt_(const sfpi::vFloat x)|
|{|
|#pragma GCC unroll 8|
|for (int d = 0; d < iterations; d++)|
|{|
|sfpi::vFloat val = sfpi::dst_reg[0];|
|sfpi::vInt i = sfpi::reinterpret<sfpi::vInt>(sfpi::reinterpret<sfpi::vUInt>(x) >> 1);|
|sfpi::vFloat y = sfpi::reinterpret<sfpi::vFloat>(sfpi::vConstIntPrgm0 - i);|
||
|if constexpr (APPROXIMATION_MODE)|
|if constexpr (APPROXIMATE)|
|{|
|// Algorithm SQRT_10-bits, with modifications for reciprocal.|
|sfpi::vFloat c = x * y;|
|sfpi::vFloat negative_y = -y;|
|if constexpr (RECIPROCAL) {|
|y = y * (sfpi::vConstFloatPrgm1 + negative_y * c);|
|}|
|else|
|{|
|sfpi::vUInt magic = sfpi::vConstIntPrgm0;|
||
|// sqrt initial approximation|
|// adjust bias|
|sfpi::vUInt val_s = magic + sfpi::reinterpret<sfpi::vUInt>(val);|
|y = c * (sfpi::vConstFloatPrgm1 + negative_y * c);|
|}|
|}|
|else|
|{|
|// Algorithm SQRT_23-bits, with modifications for reciprocal.|
|sfpi::vFloat negative_x = -x;|
|sfpi::vFloat c = negative_x * y * y;|
|y = y * (sfpi::vConstFloatPrgm1 + c * (sfpi::vConstFloatPrgm2 + c));|
|sfpi::vFloat half_y = sfpi::addexp(y, -1);|
||
|// approximation of square root|
|val_s >>= 1;|
|sfpi::dst_reg[0] = sfpi::reinterpret<sfpi::vFloat>(val_s);|
|if constexpr (RECIPROCAL) {|
|y = y + (sfpi::vConst1 + negative_x * y * y) * half_y;|
|}|
|else|
|{|
|// Recip root method|
|//// Init approx|
|// u.i = SQRT_MAGIC_F - (u.i >> 1);|
|v_if (val != 0.0f)|
|{|
|sfpi::vUInt magic = sfpi::vConstIntPrgm0;|
|sfpi::vFloat approx = sfpi::reinterpret<sfpi::vFloat>(magic - (sfpi::reinterpret<sfpi::vUInt>(val) >> 1));|
||
|// Reciproot iterations|
|for (int r = 0; r < RECIPROCAL_ITERATIONS; r++)|
|{|
|// x*r*(1.5f - xhalf*r*r);|
|approx = ((approx * approx) * (val * -0.5f) + 1.5f) * approx;|
|}|
||
|sfpi::dst_reg[0] = approx * val;|
|}|
|v_endif;|
|half_y = -half_y;|
|**jasondavies** marked this conversation as resolved.|   |   |
|y = x * y;|
|y = y + (y * y + negative_x) * half_y;|
|}|
|}|
||
|return y;|
|}|
||
|template <bool APPROXIMATION_MODE, int ITERATIONS, bool RECIPROCAL>|
|inline void _calculate_sqrt_()|
|{|
|#pragma GCC unroll 8|
|for (int d = 0; d < ITERATIONS; d++)|
|{|
|sfpi::dst_reg[0] = _sfpu_sqrt_<APPROXIMATION_MODE, RECIPROCAL>(sfpi::dst_reg[0]);|
|sfpi::dst_reg++;|
|}|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/230/files/77dfc15b5711c83566bf6f17a74dd8222c8c8818#diff-fba39c46447b08439ccb05ec2814a3d3638894d13e04f63a3fd4e1ab4a3279f7)|   |@@ -63,11 +74,14 @@ inline void _init_sqrt_()|
|{|
|if (APPROXIMATION_MODE)|
|{|
|sfpi::vConstFloatPrgm0 = sfpi::s2vFloat16b(127 << 7);|
|sfpi::vConstIntPrgm0 = 0x5f0b3892;|
|sfpi::vConstFloatPrgm1 = 1.89099014875f;|
|}|
|else|
|{|
|sfpi::vConstFloatPrgm0 = sfpi::s2vFloat16b(0x5f37);|
|sfpi::vConstIntPrgm0 = 0x5f1110a0;|
|sfpi::vConstFloatPrgm1 = 2.2825186f;|
|sfpi::vConstFloatPrgm2 = 2.2533049f;|
|}|
|}|

```

# _calculate_reciprocal_ has poor precision #154

The existing implementation uses Newton's method, with a default iteration count of 3. Here is a histogram of precision error (ULPs) for all 224 32-bit float patterns (ignoring sign and exponents, since we rescale the exponent to calculate the reciprocal anyway, and I'm not including denormals):

*graph*

Of course, it gets worse for the "approximate mode" version, which uses 2 iterations:

*graph*

# Improve reciprocal precision. #155

### Ticket

[#154](https://github.com/tenstorrent/tt-llk/issues/154)

### Problem description

The original implementation used a poor initial guess, which led to high ULPs (~24749) even when using 3 iterations.

### What's changed

The improved initial guess uses one additional FMA. The maximum error is now approximately 1 ULPs (not checked exactly) for 3 iterations, and 201 ULPs for 2 iterations (used in "approximation mode").

The code was also simplified to reduce operation count.

### Type of change

- [x]  Bug fix (non-breaking change which fixes an issue)
- [ ]  New feature (non-breaking change which adds functionality)
- [ ]  Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ]  Documentation update
### Checklist

- [ ]  [All post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/all-post-commit-workflows.yaml) CI passes
- [ ]  [Blackhole Post commit](https://github.com/tenstorrent/tt-metal/actions/workflows/blackhole-post-commit.yaml) CI passes (if applicable)

* 5 commits

```cpp
tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_recip.h

|   |   |   |
|---|---|---|
||   |   |
|@@ -74,6 +74,11 @@ inline void _calculate_reciprocal_(const int iterations)|
|template <bool APPROXIMATION_MODE>|
|inline void _init_reciprocal_()|
|{|
|// The following constants are used to calculate an initial estimate for 1/D using a linear approximation.|
|// The linear approximation with minimum worst-case absolute error on the interval [0.5, 1] is:|
|// X_0 = 48/17 - 32/17 D|
|// See https://en.wikipedia.org/wiki/Division_algorithm#Initial_estimate for the full derivation.|
||
|sfpi::vConstFloatPrgm0 = 48.0f / 17.0f;|
|**jasondavies** marked this conversation as resolved.|   |   |
|sfpi::vConstFloatPrgm1 = 32.0f / 17.0f;|
|}|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_binary.h

|   |
|---|
|@@ -144,7 +144,7 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|}|
|v_else|
|{|
|result = in0 * sfpi::setsgn(_sfpu_reciprocal_<4>(in1), in1);|
|result = in0 * sfpi::setsgn(_sfpu_reciprocal_<3>(in1), in1);|
|}|
|v_endif;|
|}|
|[](https://github.com/tenstorrent/tt-llk/pull/155/files#diff-8b1cf9e2535e8f1581301682d238124914691d4e02cbdb212a1fca34f8b36fdc)|   |@@ -165,10 +165,15 @@ inline void _calculate_sfpu_binary_(const uint dst_offset)|
|template <bool APPROXIMATION_MODE /*unused*/, BinaryOp BINOP>|
|inline void _sfpu_binary_init_()|
|{|
|if constexpr (BINOP == BinaryOp::DIV \| BINOP == BinaryOp::POW)|
|if constexpr (BINOP == BinaryOp::DIV)|
|{|
|_init_reciprocal_<APPROXIMATION_MODE>();|
|}|
|if constexpr (BINOP == BinaryOp::POW)|
|{|
|// note: calls _init_reciprocal_|
|_init_exponential_<APPROXIMATION_MODE>();|
|}|
|}|
||
|} // namespace sfpu|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_exp.h

|   |
|---|
|@@ -25,7 +25,7 @@ sfpi_inline sfpi::vFloat _sfpu_exp_(sfpi::vFloat val)|
|v_endif;|
||
|// Run series in Horner form|
|sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::s2vFloat16b(0.863281);|
|sfpi::vFloat tmp = val * sfpi::vConst0p8373 + sfpi::vConstFloatPrgm2;|
|val = val * tmp + sfpi::vConst1;|
||
|v_if (exp >= 0)|
|[](https://github.com/tenstorrent/tt-llk/pull/155/files#diff-44cee679e6a137340275f903f5ba2e3cc002a167ada7ebbb09d3aad3e5529773)[](https://github.com/tenstorrent/tt-llk/pull/155/files#diff-44cee679e6a137340275f903f5ba2e3cc002a167ada7ebbb09d3aad3e5529773)|   |@@ -360,8 +360,7 @@ inline void _init_exponential_()|
|}|
|else|
|{|
|sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip|
|sfpi::vConstFloatPrgm1 = 2.0f;|
|_init_reciprocal_<APPROXIMATION_MODE>();|
|sfpi::vConstFloatPrgm2 = 0.863281f;|
|}|
|}|

tt_llk_wormhole_b0/common/inc/sfpu/ckernel_sfpu_gelu.h

|   |
|---|
|@@ -189,10 +189,6 @@ inline void _init_gelu_()|
|template <bool APPROXIMATION_MODE>|
|inline void _init_gelu_derivative_()|
|{|
|sfpi::vConstFloatPrgm0 = 1.442695f; // ln2_recip|
|sfpi::vConstFloatPrgm1 = 2.0f;|
|sfpi::vConstFloatPrgm2 = 0.863281f;|
||
|uint imm0;|
|uint imm1;|
|uint imm2;|
|[](https://github.com/tenstorrent/tt-llk/pull/155/files#diff-8bdbc08208a9a1e2a653d6c28bfab96522e3179542ad86f538aa49aa33ffcb8a)[](https://github.com/tenstorrent/tt-llk/pull/155/files#diff-8bdbc08208a9a1e2a653d6c28bfab96522e3179542ad86f538aa49aa33ffcb8a)|   |@@ -230,6 +226,8 @@ inline void _init_gelu_derivative_()|
|}|
|else|
|{|
|_init_exponential_<false>();|
||
|imm0 = 0x28FF;|
|imm1 = 0x3020;|
|_sfpu_load_imm16_(0, imm0);|
```

# [Bounty $1000] Reciprocal and related functions have poor precision. #21622

### Describe the bug

The `_calculate_reciprocal_` LLK function has poor precision due to a low quality initial estimate for Newton-Raphson, which affects various other functions that rely on it.

See [tenstorrent/tt-llk#154](https://github.com/tenstorrent/tt-llk/issues/154) for report and ~~[tenstorrent/tt-llk#155](https://github.com/tenstorrent/tt-llk/pull/155)~~ [tenstorrent/tt-llk#230](https://github.com/tenstorrent/tt-llk/pull/230) for proposed fix.

As part of fixing this issue, I noticed that several places in `tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_sfpu/` use hardcoded constants instead of calling the LLK initialisation function to set these constants.

In order to use updated LLK with new constants, these hardcoded constants need to be removed and ideally replaced by calls to the appropriate LLK init functions.

### Steps to reproduce the issue

_No response_

### Expected behavior

_No response_

### Please complete the following environment information

_No response_

# Use exp_init/recip_init instead of hardcoded constants, rsqrt_tile instead of sqrt_tile+recip_tile. #21623

Users of exp/recip LLKs should not set their own hardcoded constants, otherwise they'll break when the constants change. They should call the appropriate _init functions instead.

As part of this change, I'm also replacing sqrt_tile+recip_tile with rsqrt_tile as it will be more efficient.

Additionally, I found that rsqrt_tile had a `fast_and_approx_mode` parameter that was completely ignored. I've changed rsqrt_tile so that it is treated the same as sqrt_tile, i.e. it respects `approx_mode` instead.

