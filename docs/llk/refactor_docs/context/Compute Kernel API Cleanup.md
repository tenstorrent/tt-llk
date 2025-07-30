- [Issues](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Issues)
    - [1. “long“ inits causing state issues:](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#1.-%E2%80%9Clong%E2%80%9C-inits-causing-state-issues%3A%5BhardBreak%5D)
    - [2. *_init vs *init_short](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#2.-*_init-vs-*init_short%5BhardBreak%5D)
    - [3. “tile“ vs “block“](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#3.-%E2%80%9Ctile%E2%80%9C-vs-%E2%80%9Cblock%E2%80%9C)
    - [4. pack_tile keeps an internal cb ptr updated](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#%5BhardBreak%5D4.-pack_tile-keeps-an-internal-cb-ptr-updated)
    - [5. *_dt* functions vs regular inits](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#5.-*_dt*-functions-vs-regular-inits)
    - [6. Redundant functions added by Op teams](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#%5BhardBreak%5D6.-Redundant-functions-added-by-Op-teams)
- [Proposal](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Proposal)
- [Ongoing Effort](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Ongoing-Effort)
    - [General Workflow](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#General-Workflow)
    - [Transpose Op](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Transpose-Op)
    - [Matmul Op](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Matmul-Op)
    - [Eltwise Binary Ops](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Eltwise-Binary-Ops)
    - [Eltwise Unary and Other Ops](https://tenstorrent.atlassian.net/wiki/spaces/LLK/pages/1181417595/Compute+Kernel+API+Cleanup#Eltwise-Unary-and-Other-Ops)

## Issues

#### 1. “long“ inits causing state issues:  

“long“ inits such as:

```cpp
ALWI void tilize_init(uint32_t icb, uint32_t block, uint32_t ocb) {
    MATH((llk_math_eltwise_unary_datacopy_init<
          A2D,
          BroadcastType::NONE,
          DST_ACCUM_MODE,
          false /*is_int_en*/,
          true /*tilize en*/>(false /*transpose of faces*/, false /*transpose within 16x16 face*/, icb)));
    MATH((llk_math_pack_sync_init<DST_ACCUM_MODE>()));
    MATH((llk_math_hw_configure_disaggregated(icb, icb)));

    PACK((llk_pack_hw_configure_disaggregated<false, DST_ACCUM_MODE, ReluType::NO_RELU, 0, true /*tilize en*/>(ocb)));
    PACK((llk_pack_init<false, false, true /*tilize en*/>(ocb)));
    PACK((llk_pack_dest_init<false, DST_ACCUM_MODE>(ocb)));

    UNPACK((llk_unpack_tilize_hw_configure_disaggregated<DST_ACCUM_MODE>(icb)));
    UNPACK((llk_unpack_tilize_init(icb, block)));
}
```

are not documented to only be called once or else they cause problems with device state.  
  
Moreover, a lot of the the functions in the “long“ inits can become common across all ops. For example these:

```cpp
llk_unpack_*_hw_configure() llk_pack_hw_configure() llk_math_pack_sync_init(); llk_math_hw_configure_disaggregated() llk_pack_dest_init
```

#### Github issue:
* Currently there is a lot of confusion about the following API functions:
```cpp
llk_unpack_*_hw_configure()
llk_pack_hw_configure()
llk_math_pack_sync_init();
llk_math_hw_configure_disaggregated()
llk_pack_dest_init
```
Which are meant to only be called once per kernel, and are used in every compute_kernel_api long init call such as:
```
ALWI void tilize_init(uint32_t icb, uint32_t block, uint32_t ocb) {
    MATH((llk_math_eltwise_unary_datacopy_init<
          A2D,
          BroadcastType::NONE,
          DST_ACCUM_MODE,
          false /*is_int_en*/,
          true /*tilize en*/>(false /*transpose of faces*/, false /*transpose within 16x16 face*/, icb)));
    MATH((llk_math_pack_sync_init<DST_ACCUM_MODE>()));
    MATH((llk_math_hw_configure_disaggregated(icb, icb)));

    PACK((llk_pack_hw_configure_disaggregated<false, DST_ACCUM_MODE, ReluType::NO_RELU, 0, true /*tilize en*/>(ocb)));
    PACK((llk_pack_init<false, false, true /*tilize en*/>(ocb)));
    PACK((llk_pack_dest_init<false, DST_ACCUM_MODE>(ocb)));

    UNPACK((llk_unpack_tilize_hw_configure_disaggregated<DST_ACCUM_MODE>(icb)));
    UNPACK((llk_unpack_tilize_init(icb, block)));
}
```

The above inits are meant to only be called once per kernel, or else they cause race conditions with tensix instructions, and then when a user would like to switch operations, they call shorter init functions:

```
/**
 * Re-initialize for the tilize operation. This can be called after a full init.
 */
ALWI void tilize_init_short(uint32_t icb, uint32_t block, uint32_t ocb) {
    MATH((llk_math_eltwise_unary_datacopy_init<
          A2D,
          BroadcastType::NONE,
          DST_ACCUM_MODE,
          false /*is_int_en*/,
          true /*tilize en*/>(false /*transpose of faces*/, false /*transpose within 16x16 face*/, icb)));
    UNPACK((llk_unpack_tilize_init(icb, block)));

#ifdef ARCH_BLACKHOLE
    PACK((llk_pack_init<false, false, true /*tilize en*/>(ocb)));
#endif
}
```

This distinction has caused confusion with the programming model, so a proposal to fix this is this:

Extract llk_unpack_*_hw_configure/llk_pack_hw_configure etc. outside the above API inits, and make them into a common api function called `hw_start_init`, and remove the long inits, and then all compute kernels can follow this logic:

```
hw_start_init(cb_in0, cb_in1, cb_out)
tilize_init_short(...)
```

So it becomes very evident that each operation only needs a single init that sets up addrmod/mops/etc, while the hw configure only happens once at the beginning of the kernel.

The way to execute this plan is:

- Step 1: Create a `hw_start_init` function as shown above, document it to encourage people to use it instead of long inits
* Step 2: Start changing existing compute kernel files one by one to replace long inits with the hw_init _ short init, for example first iterate through all tilize operations, replace `tilize_init` with `hw_start_init` + `tilize_init_short`. Open one PR per compute kernel operation that has been cleaned up
* Step 3: Remove the long init functions, and this causes new compute kernels to use the correct standard.

#### Revert z and y dim to default in uninit #24694

### Problem description
We don't reset z and y dim back to default on WH when doing tilize uninit.

### What's changed
Revert z and y dim back to default when uninitializing tilize.

* Files changed:
```cpp
// tt_metal/hw/ckernels/wormhole_b0/metal/llk_api/llk_unpack_tilize_api.h

|   |
|---|
|@@ -57,6 +57,10 @@ inline void llk_unpack_tilize_uninit(const std::uint32_t operand, const std::uin|
|std::uint32_t operand_id = get_operand_id(operand);|
|unpack_config_u config = {0};|
||
|// Revert Z and Y dim value back to default.|
|cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1, 16, 0xffff0000>(4);|
|cfg_reg_rmw_tensix<THCON_SEC0_REG0_TileDescriptor_ADDR32 + 1, 0, 0x0000ffff>(1);|
||
|config.f.out_data_format = (uint)unpack_dst_format[operand_id];|
|config.f.throttle_mode = 2;|
|TT_SETDMAREG(0, LOWER_HALFWORD(config.val[0]), 0, LO_16(p_gpr_unpack::TMP0));|
|[](https://github.com/tenstorrent/tt-metal/pull/24694/files#diff-4dbc7f6a47dfd89ac0f97d95232196866c441d4451d57c0359ae4b99339a6fe2)|   |
```