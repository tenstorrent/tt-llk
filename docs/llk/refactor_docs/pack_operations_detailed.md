# Pack Operations - Wormhole B0 Detailed Documentation

**File Location**: `tt_llk_wormhole_b0/llk_lib/llk_pack*.h`
**Architecture**: Wormhole B0 Tensix
**Component**: Data Output and Serialization Pipeline

## Overview

Pack operations handle the final stage of the Tensix compute pipeline, responsible for formatting computed results and writing them back to L1 memory. The Wormhole B0 pack implementation provides high-performance data serialization with support for multiple data formats, untilize operations, and advanced memory access patterns.

## Core Pack Operations (`llk_pack.h`)

### Address Mode Configuration

The pack pipeline uses sophisticated address generation for optimal memory access patterns:

```cpp
template <bool untilize = false>
inline void _llk_pack_configure_addrmod_() {
    // Standard address mode (ADDR_MOD_0)
    addr_mod_pack_t {
        .y_src = {.incr = 15},  // 4-bit max value (16-1)
        .y_dst = {.incr = 1},   // Standard increment
    }.set(ADDR_MOD_0);

    if constexpr (untilize) {
        // Untilize mode: specialized strided access
        addr_mod_pack_t {
            .y_src = {.incr = 1, .clr = 0, .cr = 1},  // Carriage return enabled
            .y_dst = {.incr = 1, .clr = 0, .cr = 0},  // Standard destination
        }.set(ADDR_MOD_1);
    } else {
        // Standard mode: sequential access
        addr_mod_pack_t {
            .y_src = {.incr = 0, .clr = 1, .cr = 0},  // Clear on completion
            .y_dst = {.incr = 0, .clr = 1, .cr = 0},  // Clear on completion
            .z_src = {.incr = 0, .clr = 0},           // Z-axis control
            .z_dst = {.incr = 0, .clr = 0},           // Z-axis control
        }.set(ADDR_MOD_1);
    }
}
```

### MOP (Micro-Operation) Configuration

The pack operation is implemented using hardware MOPs for optimal pipeline utilization:

```cpp
template <
    bool untilize = false,
    bool zero_output = false,
    DstTileFaceLayout FaceLayout = DstTileFaceLayout::RowMajor,
    bool write_tile_header = true
>
inline void _llk_pack_mop_config_(
    const std::uint32_t pack_dst_format,
    const std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces  = 4,
    const bool partial_face        = false,
    const bool narrow_tile         = false
);
```

#### Standard Pack Mode
```cpp
if constexpr (!untilize) {
    constexpr uint MOP_OUTER_LOOP = 1;
    ckernel::ckernel_template tmp(
        MOP_OUTER_LOOP, MOP_INNER_LOOP,
        TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1)
    );

    // Optional tile header generation
    if constexpr (write_tile_header) {
        tmp.set_end_op(TT_OP_STOREIND(1, 0, p_ind::LD_16B, LO_16(0),
                       p_ind::INC_NONE, p_gpr_pack::TILE_HEADER, p_gpr_pack::OUTPUT_ADDR));
    }

    tmp.program(instrn_buffer);
}
```

#### Untilize Pack Mode
```cpp
else {
    const uint MOP_OUTER_LOOP = ((face_r_dim == 1) || narrow_tile) ? 1 : (face_r_dim >> 1);

    if ((face_r_dim == 1) || narrow_tile) {
        // Single-row or narrow tile optimization
        ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP,
            TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1));
    } else {
        // Multi-row untilize with address increment
        ckernel::ckernel_template tmp(MOP_OUTER_LOOP, MOP_INNER_LOOP,
            TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT), 0, MEGAROW, 0, 1));
        tmp.set_loop_op0(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0));
    }
}
```

## Pack Common Operations (`llk_pack_common.h`)

### Data Format Configuration

The pack common module handles data format conversions and packer hardware configuration:

```cpp
template <bool untilize_en = false>
inline void _llk_pack_hw_configure_(
    const std::uint32_t pack_src_format,
    const std::uint32_t pack_dst_format,
    const std::uint32_t tile_size,
    const std::uint32_t face_r_dim = FACE_R_DIM
) {
    // Format-specific configuration
    reconfig_packer_data_format(pack_src_format, pack_dst_format, tile_size, face_r_dim, tile_c_dim);

    // Untilize-specific settings
    if constexpr (untilize_en) {
        configure_pack_untilize();
    }
}
```

### Packer Configuration Details

#### Data Format Reconfiguration
```cpp
inline void reconfig_packer_data_format(
    const uint pack_src_format,
    const uint pack_dst_format,
    const uint tile_size,
    const uint face_r_dim,
    const uint tile_c_dim
) {
    volatile uint* cfg = get_cfg_pointer();

    const uint pack_output_src_format = (uint)pack_src_format & 0xF;
    const uint pack_output_dst_format = (uint)pack_dst_format & 0xF;

    pack_config_u config;
    config.val[2] = 0;
    config.f.uncompress      = 1;
    config.f.out_data_format = pack_output_dst_format;
    config.f.in_data_format  = pack_output_src_format;

    // Critical timing: THCON instruction followed by CFG
    TT_SETDMAREG(0, LOWER_HALFWORD(config.val[2]), 0, LO_16(p_gpr_pack::TMP_LO));
    TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);  // Ensure ordering
    TTI_WRCFG(p_gpr_pack::TMP_LO, p_cfg::WRCFG_32b, THCON_SEC0_REG1_Row_start_section_size_ADDR32 + 2);
}
```

## Pack Untilize Operations (`llk_pack_untilize.h`)

### Untilize Algorithm

Untilize operations perform matrix transpose during pack for efficient data layout conversion:

```cpp
template <
    bool untilize_en = true,
    bool zero_output = false,
    bool write_tile_header = false
>
inline void _llk_pack_untilize_(
    std::uint32_t tile_index,
    std::uint32_t pack_dst_format,
    std::uint32_t face_r_dim = FACE_R_DIM,
    const std::uint32_t num_faces = 4
) {
    static_assert(untilize_en, "Untilize must be enabled for this operation");

    // Configure strided access for transpose
    const uint PACK_INTF_SEL = p_pacr::TWO_INTFS_ACTIVE;
    constexpr uint ZERO_OUTPUT_FLAG = zero_output ? p_pacr::P_ZERO_OUTPUT_ENABLED
                                                  : p_pacr::P_ZERO_OUTPUT_DISABLED;

    // Execute untilize pack operation
    TTI_PACR(p_pacr::CFG_CTXT_0, p_pacr::NO_ROW_PAD_ZERO, p_pacr::DST_ACCESS_STRIDED_MODE,
             ADDR_MOD_2, p_pacr::ADDR_CNT_CTXT_0, ZERO_OUTPUT_FLAG, PACK_INTF_SEL,
             0, 0, p_pacr::NO_CTXT_CTRL, 0, 1);

    // Critical: Reset Z counters after operation
    TT_SETADCZW(p_setadc::PAC, 0, 0, 0, 0, 0b0101);
}
```

## Performance Characteristics

### Cycle Costs by Operation Type

| Operation Mode | Cycles per Face | Memory Pattern | Notes |
|----------------|-----------------|----------------|-------|
| **Standard Pack** | 1-2 | Sequential | Optimal for row-major output |
| **Untilize Pack** | 2-4 | Strided | Transpose overhead included |
| **Partial Face** | 1-3 | Variable | BFP format optimization |
| **Zero Output** | 1 | Skip | Sparsity acceleration |

### Format-Specific Performance

| Data Format | Pack Overhead | Compression | Bandwidth |
|-------------|---------------|-------------|-----------|
| **FP32** | Baseline | None | 100% |
| **FP16** | 0.9x | 50% | 180% |
| **BF16** | 0.9x | 50% | 180% |
| **INT8** | 0.8x | 75% | 320% |

## Advanced Features

### Tile Header Management
```cpp
// Automatic tile header generation
if constexpr (write_tile_header) {
    tmp.set_end_op(TT_OP_STOREIND(
        1, 0,                           // Size and offset
        p_ind::LD_16B,                  // 16-byte load
        LO_16(0),                       // Address mode
        p_ind::INC_NONE,                // No increment
        p_gpr_pack::TILE_HEADER,        // Header register
        p_gpr_pack::OUTPUT_ADDR         // Output address
    ));
}
```

### Partial Face Support
```cpp
if (partial_face && IS_BFP_FORMAT(pack_dst_format)) {
    tmp.set_start_op(TT_OP_PACR(ADDR_MOD_0, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT),
                                0, MEGAROW, 0, 0)); // Don't close tile
    tmp.set_loop_op0(TT_OP_INCADCXY(p_setadc::PAC, 0, 0, 1, 0));  // Address increment
    tmp.set_loop_op1(TT_OP_PACR(ADDR_MOD_1, ZERO_OUTPUT_FLAG, PACK_SEL(PACKCNT),
                                0, MEGAROW, 0, 1)); // Close tile
}
```

## Synchronization and Timing

### Critical Timing Requirements

#### Configuration Timing
```cpp
// WRCFG timing constraint: minimum 2 instructions between WRCFG and dependent PACK
TT_SETDMAREG(/*...*/);                    // Instruction 1 (THCON type)
TTI_STALLWAIT(p_stall::STALL_CFG, p_stall::THCON);  // Ensure completion
TTI_WRCFG(/*...*/);                       // Instruction 2 (CFG type)
// Minimum 2-instruction gap before dependent pack operation
TTI_NOP();                                // Optional padding
TTI_NOP();                                // Optional padding
TTI_PACR(/*...*/);                        // Dependent pack operation
```

#### Pack Pipeline Synchronization
```cpp
// Pack operations can overlap subject to hardware constraints
while (true) {
    WRCFG(REGISTER_PACK_DEPENDS_ON_AND_LATCHES);  // Configuration
    run_mop_with_pack();                          // Execute pack MOP
    // Hardware guarantees: WRCFG bypasses current MOP, latches for next MOP
}
```

## Error Conditions and Debugging

### Common Issues

#### Address Mode Conflicts
- **Symptom**: Incorrect memory layout or corrupted output
- **Cause**: Incompatible address mode configuration
- **Solution**: Verify ADDR_MOD settings match operation type

#### Format Mismatch
- **Symptom**: Data corruption or unexpected values
- **Cause**: pack_src_format != actual DEST format
- **Solution**: Ensure format consistency through pipeline

#### Timing Violations
- **Symptom**: Intermittent corruption or hangs
- **Cause**: Insufficient delay between WRCFG and pack operation
- **Solution**: Add minimum 2-instruction gap

### Debug Support
```cpp
// Register inspection during pack operation
volatile uint* cfg = get_cfg_pointer();
uint pack_status = cfg[PACK_STATUS_OFFSET];

// Instruction trace support
TTI_RECORD_START();
pack_operation();
TTI_RECORD_STOP();
```

## Integration Patterns

### Basic Pack Workflow
```cpp
// 1. Initialize pack subsystem
llk_pack_init<DataFormat::Float16>(output_cb_id);

// 2. Configure address modes
_llk_pack_configure_addrmod_<false>();  // Standard mode

// 3. Configure MOP
_llk_pack_mop_config_<false, false>(pack_dst_format, face_r_dim, num_faces);

// 4. Execute pack operation
llk_pack<DstTileFaceLayout::RowMajor>(tile_index, output_cb_id);

// 5. Synchronize completion
llk_pack_dest_section_done<DstSync::SyncFull>();
```

### Untilize Workflow
```cpp
// Specialized untilize configuration
llk_pack_untilize_init<DataFormat::Float16>(output_cb_id);
_llk_pack_configure_addrmod_<true>();   // Untilize mode
_llk_pack_mop_config_<true, false>(pack_dst_format, face_r_dim, num_faces);
llk_pack_untilize<true>(tile_index, output_cb_id);
```

---

**Next**: [Unpack Operations Detailed Documentation](unpack_operations_detailed.md)
**Related**: [Pack/Unpack Implementation Details](discussions%20on%20Pack%20and%20Unpack%20operation%20details%20vs.%20actual%20implementation.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/llk_lib/llk_pack*.h`
