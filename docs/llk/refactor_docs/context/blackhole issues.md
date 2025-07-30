### Blackhole Performance Issues

##### Feature - Description - Priority
*explanation is found below every issue*

* Use CFGSHIFTMASK instruction - This instruction can replace the addma/stallwait/wrcfg instructions for mmio address updates with a single cycle instruction. Can be implemented then performance can be measured to see if worth using - Low

Blackhole has new `CFGSHIFTMASK` that can update addresses for the unpacker instructions inside the mop/replay buffers.

Instead of updating addresses in this method:

```
  TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC0_REG3_Base_address_ADDR32);
  TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
  TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
  TTI_WRCFG(p_gpr_unpack::TMP0,0,THCON_SEC0_REG3_Base_address_ADDR32);

```

Using the CFGSHIFTMASK instruction, it could be done like this:

```
TTI_CFGSHIFTMASK(1, 0b011, 32 - 1, 0, 0b11, THCON_SEC0_REG3_Base_address_ADDR32); // THCON_SEC0_REG3_Base_address_ADDR32 =  THCON_SEC0_REG3_Base_address_ADDR32 +  SCRATCH_SEC0_val_ADDR32 
```

as long as the scratch buffer is correctly populated:

```
TTI_WRCFG(p_gpr_unpack::TILE_SIZE_A, 0, SCRATCH_SEC0_val_ADDR32);
TTI_NOP;
```

If an operation is unpacker bound, then using the `CFGSHIFTMASK` should increase performance.

==Solved, Its alright, it was good to confirm that unpacker is not as much of a bottleneck for blackhole, I will close this issue now==


* Unpack full 32x32 tiles - Only implemented for matmul, should review implementing for most kernels to saturate math - Low

Currently the only unpacker kernel that unpacks 32x32 tiles is the `llk_unpack_AB_matmul.h`. The addition was done to saturate the compute kernel for matmul operations. There are other unpacker kernels that could also make use of the same additions:

- llk_unpack_A.h
* llk_unpack_AB.h
* llk_unpack_reduce.h
* llk_unpack_tilize.h
* llk_unpack_untilize.h

Not only would this be helpful for performance, but also it could remove the unnecessary face dimensions configs:

```
    config_unpacker_x_end<UNP_SEL>(face_r_dim);
```

That need to be added to init functions such as `_llk_unpack_A_init_` to be able to fuse operations that switch between unpacking 16x16 and unpacking 32x32.


* Review replay buffer usage - Not all kernels utilize replay buffers, should review if possible to use more - Low

Replay buffers are currently under-utilized in the kernels. For the case of unpacker kernels, replay buffers can be used to update the l1 tile addresses without mmio accesses:

```
  TTI_RDCFG(p_gpr_unpack::TMP0, THCON_SEC0_REG3_Base_address_ADDR32);
  TTI_ADDDMAREG(0, p_gpr_unpack::TMP0, p_gpr_unpack::TMP0, p_gpr_unpack::TILE_SIZE_A);
  TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);
  TTI_WRCFG(p_gpr_unpack::TMP0,0,THCON_SEC0_REG3_Base_address_ADDR32);
```

or can also use CFGSHIFTMASK method here: [#4](https://github.com/tenstorrent/tt-llk-bh/issues/4)

The following unpacker kernels do not use replay buffers:

- llk_unpack_A.h
* llk_unpack_AB.h
* llk_unpack_reduce.h
* llk_unpack_tilize.h

Performance measurements for the above kernels should be done, and operations that are unpack bound can try implementing the addresses updates for performance increase. Eltwise binary/unary operations for example are around ~15% math util in buda performance measurements.

* Re-enable ZEROACC dest bank autodetect - This feature has been disabled to preserve legacy behaviour, but after bringing up Blackhole functionality, this feature should be re-enabled, and all ZEROACC usage should be changed to work appropriately - Low

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


* Improve Pack Untilize Perf - Pack Untilize outputs 2x16 rows, algorithm can be improved as highlighted in the issue - Low
Pack untilize kernel needed to be re-written for Blackhole, due to Blackhole packer limitations:
1. Only one Dest offset register for the packer instance (but we can use strided mode of 16 rows apart)
2. PACR instructions can only write L1 contiguous output.
3. These L1 Offset registers, are now set per PACR Context, not per rows output from PACR instruction (i.e PACK_INTF_SEL)
```
THCON_SEC0_REG1_L1_Dest_addr_ADDR32 -> Context 0
THCON_SEC0_REG8_L1_Dest_addr_ADDR32 -> Context 1
THCON_SEC1_REG1_L1_Dest_addr_ADDR32 -> Context 2
THCON_SEC1_REG8_L1_Dest_addr_ADDR32 -> Context 3
```

* Current algorithm:
For wormhole b0, 8x16 rows can be output per tile (before incrementing counters), while for blackhole with the above implementation, only 2x16 can. Since pack_untilize feature needs to be fused with other operations, we cannot change the unpacker to use a different unpacking scheme (Fastest untilize scheme would be T0F0, T0F1, T1F0, T1F1, T0F2, T0F3, T1F2, T1,F3)). However, we should use that implementation if the use case is doing a pack_untilize for a block of tiles (i.e not fused).

One major optimization, is to enable the use of contexts.

THCON_SEC0_REG1_L1_Dest_addr_ADDR32 = CNTX0 = tile_offset (Configured for top faces)  
THCON_SEC0_REG8_L1_Dest_addr_ADDR32 = CNTX2 = tile_offset + block_ct_dim * TILE_C_DIM *datum_size ((Configured for bottom faces)

Then algorithm can do 4x16 rows, will change to something like this:

```
CH0 x_stride = number of bytes per datum (2 bytes for Float16b default)
CH0 y_stride = FACE_C_DIM*x_stride
CH0 z_stride = FACE_R_DIM*y_stride (1x16x16 datums default, already done in pack_hw_config)
CH0 w_stride = 4*z_stride (32x32 by default)
PACK_INTF_SELECT_0 = 0b0011 (read 2 rows from dest)
PACK_INTF_SELECT_1 = 0b1100 (read 2 rows from dest)
Dest Mode = DST_STRIDED_MODE (Each row read from dest is 16 apart)

    for face_r_dim (16 by default):
        for block_ct_dim (2 in this example) {
            PACK(CNTX0, PACK_INTF_SELECT_0); (Reads 2 rows, writes them contigous in L1, dest rows = 0,16, 1, 17, 2, 18 .....)
            PACK(CNTX1, PACK_INTF_SELECT_1); (Reads 2 rows, writes them contigous in L1, dest rows = 32,48, 33, 49,  .....)
            W_CNT += 1 (Jumps to next tile)
        }
        Y_CNT+=1
    }
```

The problem here is if contexts are used, initial hardware configuration for data formats, tile sizes, etc, all need to also be programmed.

* Answer:

* Measured perf for pack untilize block test with following args:  
block C dim: 4 tiles  
block R dim: 1 tile  
Num cores: 1

To reproduce:

```
git checkout branch
ENABLE_TRACY=1 scripts/build_scripts/build_with_profiler_opt.sh
ninja tests -C build
ENABLE_TRACY=1 TT_METAL_DEVICE_PROFILER=1 TT_METAL_SLOW_DISPATCH_MODE=1 ./build/test/tt_metal/unit_tests --gtest_filter=“*ComputePackUntilize*”
```


Cycles can be calculated using the machines AICLK and The GPU execution time

The branch has the blocking calls for circular buffers and math-pack semaphores commented out. The results for the number of cycles the PACK takes is:

WHB0: ~145 cycles (average 10 runs)  
BH: ~155 cycles (average 10 runs)

The results consistently show BH and WHB0 have around the same cycles count difference for block of tiles less than 6. For c_dim > 6, unfortunately Wormhole B0 has a smaller depth instruction buffer (16 insns), in comparison to Blackhole (32 insns), and that affects the results since at around 6 tiles is when math issues 16 insns and needs to stall for wormhole b0. Better results can be obtained from waveforms.