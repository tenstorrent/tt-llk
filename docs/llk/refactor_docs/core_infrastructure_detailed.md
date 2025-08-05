# Core Kernel Infrastructure - Wormhole B0 Detailed Documentation

**File Location**: `tt_llk_wormhole_b0/common/inc/`
**Architecture**: Wormhole B0 Tensix
**Component**: Low-Level Kernel Infrastructure

## Overview

The core kernel infrastructure provides the fundamental building blocks for all TT-LLK operations on Wormhole B0. This layer handles hardware abstraction, instruction generation, synchronization primitives, and register management, forming the foundation upon which all higher-level operations are built.

## Main Kernel Interface (`ckernel.h`)

### Core Infrastructure Constants

```cpp
namespace ckernel {
    // Pack operation control flags
    constexpr uint PACK_FLUSH_COUNTERS =
        (1 << PACK_COUNTERS_SEC2_pack_per_xy_plane_SHAMT) |
        (1 << PACK_COUNTERS_SEC2_pack_reads_per_xy_plane_SHAMT) |
        (1 << PACK_COUNTERS_SEC2_pack_xys_per_tile_SHAMT);

    // Kernel execution state constants
    constexpr uint RESET_VAL          = 0;
    constexpr uint KERNEL_IN_PROGRESS = 15;
    constexpr uint KERNEL_COMPLETE    = 1;

    // Global hardware register pointers
    extern volatile uint tt_reg_ptr *reg_base;
    extern volatile uint tt_reg_ptr *pc_buf_base;
    extern volatile uint tt_reg_ptr *regfile;
    extern volatile uint tt_reg_ptr *mailbox_base[4];
}
```

### Synchronization Primitives

#### Tensix Core Synchronization
```cpp
inline void tensix_sync() {
    volatile uint foo = 0;
    volatile uint *fooptr = &foo;

    // Write to PC buffer to order all pending writes
    pc_buf_base[1] = foo;

    // Read blocks until Tensix core is idle
    *fooptr = pc_buf_base[1];
}
```

**Purpose**: Ensures all previous Tensix operations complete before proceeding
**Use Case**: Critical synchronization points, configuration changes
**Performance**: High-cost operation, use sparingly

#### MOP Synchronization
```cpp
inline void mop_sync() {
    volatile uint foo = 0;
    volatile uint *fooptr = &foo;

    // Write to PC buffer to order pending writes
    pc_buf_base[2] = foo;

    // Read blocks until all MOPs complete
    *fooptr = pc_buf_base[2];
}
```

**Purpose**: Waits for all micro-operations (MOPs) to finish
**Use Case**: Between dependent MOP sequences
**Performance**: Medium-cost, more efficient than full tensix_sync

### Global State Management

```cpp
namespace ckernel {
    // Configuration and state tracking
    extern uint32_t cfg_state_id;      // Current configuration state
    extern uint32_t dest_offset_id;    // Destination buffer offset
    extern uint32_t dbg_event_index;   // Debug event tracking
    extern uint32_t dbg_event_end;     // Debug event boundary

    // Mailbox communication
    extern uint8_t mailbox_index;      // Current mailbox index
    const extern uint8_t mailbox_end;  // Mailbox boundary
    extern volatile uint16_t tt_reg_ptr *debug_mailbox_base;
}
```

## Instruction Buffer Management (`ckernel_main.h`)

### Instruction Buffer Interface
```cpp
extern volatile uint32_t __instrn_buffer[];

namespace ckernel {
    // Type-safe instruction buffer access
    constexpr inline volatile uint32_t(tt_reg_ptr &instrn_buffer)[] = __instrn_buffer;
}
```

**Buffer Usage**:
- **Primary storage** for generated instruction sequences
- **Template target** for MOP instruction generation
- **Hardware interface** for instruction execution

## Instruction Template System (`ckernel_template.h`)

### Template-Based Instruction Generation

```cpp
namespace ckernel {
    class ckernel_template {
    public:
        ckernel_template(uint32_t outer_loop, uint32_t inner_loop, uint32_t main_op);

        void set_start_op(uint32_t instruction);    // Pre-loop operation
        void set_loop_op0(uint32_t instruction);    // First loop operation
        void set_loop_op1(uint32_t instruction);    // Second loop operation
        void set_end_op(uint32_t instruction);      // Post-loop operation

        void program(volatile uint32_t* buffer);    // Generate final sequence
    };
}
```

#### Template Structure
```cpp
// Generated instruction pattern:
// START_OP (optional)
// OUTER_LOOP_BEGIN
//   INNER_LOOP_BEGIN
//     MAIN_OP
//     LOOP_OP0 (optional)
//     LOOP_OP1 (optional)
//   INNER_LOOP_END
// OUTER_LOOP_END
// END_OP (optional)
```

#### Usage Example
```cpp
const uint32_t outerloop = 4;    // Process 4 faces
constexpr uint32_t innerloop = 1; // Single operation per face
constexpr uint unpack_op = TT_OP_UNPACR(SrcA, 0b1, 0, 0, 0, 1, 1, /*...*/);

ckernel_template tmp(outerloop, innerloop, unpack_op);
tmp.set_loop_op0(TT_OP_INCADCXY(p_setadc::UNP_A, 0, 0, 1, 0));  // Address increment
tmp.program(instrn_buffer);
```

## Hardware Operations (`ckernel_ops.h`)

### Instruction Generation Macros

The operations header provides comprehensive instruction generation capabilities:

#### Unpack Operations
```cpp
#define TT_OP_UNPACR(src, z_inc, ch1_y_inc, ch0_y_inc, ch1_x_inc, set_ovrd_thread_id, \
                     set_dvalid, rarefy_b, halo_x, halo_y, halo_x_inc, halo_y_inc, close_block) \
    /* Hardware instruction encoding */

#define TT_OP_UNPACR_NOP(src, mode) \
    /* No-operation unpack instruction with mode flags */
```

#### Pack Operations
```cpp
#define TT_OP_PACR(cfg_context, row_pad_zero_flag, access_mode, addr_mod, addr_cnt_context, \
                   zero_output_flag, pack_intf_sel, megarow, halo, ctxt_ctrl, packer_thread_id, close_block) \
    /* Pack operation instruction encoding */

#define TT_OP_STOREIND(size, offset, load_store_mode, addr_sel, addr_inc_mode, gpr_src, gpr_addr) \
    /* Indirect store instruction for tile headers and metadata */
```

#### Address Management
```cpp
#define TT_OP_SETADCZW(thread, ch0_z_inc, ch1_z_inc, ch0_w_inc, ch1_w_inc, clr_flags) \
    /* Set address counters for Z and W dimensions */

#define TT_OP_INCADCXY(thread, ch0_x_inc, ch1_x_inc, ch0_y_inc, ch1_y_inc) \
    /* Increment address counters for X and Y dimensions */
```

## SFPI Interface (`ckernel_sfpi.h`)

### Special Function Processing Interface

The SFPI provides high-level access to SFPU operations with C++-style syntax:

```cpp
namespace sfpi {
    // Vector register types
    class vFloat;     // 32-lane floating-point vector
    class vInt;       // 32-lane integer vector
    class vUInt;      // 32-lane unsigned integer vector

    // Register file access
    extern vFloat dst_reg[];    // Destination register file
    extern vUInt l_reg[];       // Local register file

    // Constants and immediate values
    extern vFloat vConstFloatPrgm0;  // Programmable constant 0
    extern vFloat vConstFloatPrgm1;  // Programmable constant 1
}
```

#### Vector Operations Interface
```cpp
namespace sfpi {
    // Arithmetic operations (hardware-mapped)
    vFloat operator+(const vFloat& a, const vFloat& b);
    vFloat operator-(const vFloat& a, const vFloat& b);
    vFloat operator*(const vFloat& a, const vFloat& b);

    // Special functions
    vFloat exp(const vFloat& x);        // Exponential function
    vFloat log(const vFloat& x);        // Natural logarithm
    vFloat sqrt(const vFloat& x);       // Square root
    vFloat tanh(const vFloat& x);       // Hyperbolic tangent

    // Type conversions
    vFloat s2vFloat16b(float scalar);   // Scalar to vector broadcast
    vUInt s2vUInt16b(uint32_t scalar);  // Scalar to vector broadcast
}
```

## Register and Address Mapping

### GPR (General Purpose Register) Map (`ckernel_gpr_map.h`)

```cpp
namespace p_gpr_pack {
    constexpr uint TILE_HEADER   = 0;   // Tile metadata storage
    constexpr uint OUTPUT_ADDR   = 1;   // Output address register
    constexpr uint TMP_LO        = 2;   // Temporary low register
    constexpr uint TMP_HI        = 3;   // Temporary high register
}

namespace p_gpr_unpack {
    constexpr uint INPUT_ADDR_A  = 0;   // Source A address
    constexpr uint INPUT_ADDR_B  = 1;   // Source B address
    constexpr uint FORMAT_A      = 2;   // Source A format
    constexpr uint FORMAT_B      = 3;   // Source B format
}
```

### Address Map (`ckernel_addr_map.h`)

```cpp
// Hardware register address offsets
constexpr uint DEST_OFFSET_LO    = 0x00;    // Destination low offset
constexpr uint DEST_OFFSET_HI    = 0x04;    // Destination high offset
constexpr uint CFG_STATE_ID      = 0x08;    // Configuration state
constexpr uint MAILBOX_OFFSET    = 0x0C;    // Mailbox communication
```

## Address Mode Configuration (`ckernel_addrmod.h`)

### Address Mode Structures

```cpp
// Pack address mode configuration
struct addr_mod_pack_t {
    struct {
        uint incr : 4;      // Increment value
        uint clr  : 1;      // Clear on completion
        uint cr   : 1;      // Carriage return enable
    } x_src, y_src, z_src, x_dst, y_dst, z_dst;

    void set(uint addr_mod_index);  // Apply to hardware
};

// Unpack address mode configuration
struct addr_mod_unpack_t {
    struct {
        uint incr : 4;      // Increment value
        uint clr  : 1;      // Clear on completion
        uint cr   : 1;      // Carriage return enable
    } x_src, y_src, z_src;

    void set(uint addr_mod_index);  // Apply to hardware
};
```

#### Usage Pattern
```cpp
// Configure strided access for untilize operations
addr_mod_pack_t strided_mode {
    .y_src = {.incr = 1, .clr = 0, .cr = 1},  // Enable carriage return
    .y_dst = {.incr = 1, .clr = 0, .cr = 0},  // Standard increment
};
strided_mode.set(ADDR_MOD_1);
```

## Instruction Parameters (`ckernel_instr_params.h`)

### Parameter Namespace Organization

```cpp
namespace p_pacr {
    // Pack instruction parameters
    constexpr uint P_ZERO_OUTPUT_ENABLED  = 1;
    constexpr uint P_ZERO_OUTPUT_DISABLED = 0;
    constexpr uint DST_ACCESS_STRIDED_MODE = 1;
    constexpr uint DST_ACCESS_NORMAL_MODE = 0;
}

namespace p_unpacr {
    // Unpack instruction parameters
    constexpr uint RAREFYB_ENABLE  = 1;
    constexpr uint RAREFYB_DISABLE = 0;
}

namespace p_setadc {
    // Address counter parameters
    constexpr uint UNP_A = 0;    // Unpack source A
    constexpr uint UNP_B = 1;    // Unpack source B
    constexpr uint PAC   = 2;    // Pack unit
}
```

## Debug and Development Support (`ckernel_debug.h`)

### Debug Event System

```cpp
// Debug event recording
inline void debug_event_start(uint32_t event_id) {
    dbg_event_scratch[dbg_event_index++] = event_id;
}

inline void debug_event_end(uint32_t event_id) {
    dbg_event_scratch[dbg_event_index++] = event_id | 0x80000000;
}

// Performance monitoring
inline uint32_t cycle_count_start() {
    return reg_base[CYCLE_COUNTER_OFFSET];
}

inline uint32_t cycle_count_end(uint32_t start_cycles) {
    return reg_base[CYCLE_COUNTER_OFFSET] - start_cycles;
}
```

### Instruction Tracing

```cpp
// Enable detailed instruction tracing
#ifdef ENABLE_INSTRUCTION_TRACE
    #define TRACE_INSTRUCTION(instr) \
        debug_event_start(TRACE_INSTRUCTION_EVENT); \
        instr; \
        debug_event_end(TRACE_INSTRUCTION_EVENT)
#else
    #define TRACE_INSTRUCTION(instr) instr
#endif
```

## Performance Optimization Features

### Compile-Time Optimizations

```cpp
// Force inlining for critical functions
#define TT_ALWAYS_INLINE inline __attribute__((always_inline))

// Loop unrolling hints
#define UNROLL_LOOP(factor) GCC unroll factor

// Branch prediction hints
#define UNLIKELY(condition) __builtin_expect(static_cast<bool>(condition), 0)
```

### Memory Access Optimization

```cpp
// Double buffering support
#ifndef EN_DEST_DOUBLE_BUFFERING
#define EN_DEST_DOUBLE_BUFFERING 1
#endif

// Local memory utilization
#ifndef LOCAL_MEM_EN
#define LOCAL_MEM_EN 0
#endif
```

## Integration Guidelines

### Initialization Pattern
```cpp
// 1. Initialize core infrastructure
ckernel::cfg_state_id = 0;
ckernel::dest_offset_id = 0;

// 2. Configure address modes
addr_mod_pack_t pack_addr_mode = {/*...*/};
pack_addr_mode.set(ADDR_MOD_0);

// 3. Set up instruction templates
ckernel_template operation_template(outer_loop, inner_loop, main_operation);
operation_template.program(instrn_buffer);

// 4. Execute and synchronize
execute_operation();
tensix_sync();  // Ensure completion
```

### Error Handling
```cpp
// Validate state before operations
assert(cfg_state_id < MAX_CFG_STATES);
assert(dest_offset_id < MAX_DEST_OFFSETS);

// Check instruction buffer bounds
assert(instruction_count < INSTRN_BUFFER_SIZE);

// Verify mailbox communication
if (mailbox_read_timeout(MAILBOX_TIMEOUT_CYCLES) == MAILBOX_TIMEOUT) {
    // Handle communication failure
}
```

## Hardware-Specific Considerations

### Wormhole B0 Constraints
- **Instruction buffer size**: Limited capacity requires efficient template usage
- **Address mode count**: Fixed number of configurable address modes
- **Synchronization cost**: Hardware sync operations have significant overhead
- **Register file**: Limited local register storage requires careful management

### Performance Guidelines
- **Minimize tensix_sync()**: Use only when absolutely necessary
- **Batch operations**: Group related instructions into single templates
- **Optimize address modes**: Reuse address mode configurations when possible
- **Pipeline awareness**: Arrange operations to hide latencies

---

**Next**: [SFPU Operations Detailed Documentation](sfpu_operations_detailed.md)
**Related**: [LLK Library Operations](llk_library_operations.md), [Math Operations](math_operations_detailed.md)
**Implementation**: Source files in `tt_llk_wormhole_b0/common/inc/`
