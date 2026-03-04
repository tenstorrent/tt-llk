# LLK Architecture Reference

Low-Level Kernels (LLK) for Tenstorrent Quasar architecture.

## Kernel Categories

| Category | Purpose | Location | Naming Pattern |
|----------|---------|----------|----------------|
| **SFPU** | Vector operations (sigmoid, exp, etc.) | `common/inc/sfpu/` | `ckernel_sfpu_{op}.h` |
| **Math** | Matrix/tensor operations (matmul, reduce) | `llk_lib/` | `llk_math_{op}.h` |
| **Pack** | Pack data from dest to L1 | `llk_lib/` | `llk_pack_{op}.h` |
| **Unpack** | Unpack data from L1 to src registers | `llk_lib/` | `llk_unpack_{op}.h` |

---

## 1. SFPU Kernels

Vector operations on the Special Function Processing Unit.

### Architecture
- 32 lanes in 4x8 grid
- FP32 precision internally
- 8 local registers (LREG0-LREG7)
- Processes 2 rows per iteration (SFP_ROWS)

### Key Instructions
```cpp
TTI_SFPLOAD(lreg, mem_type, addr_mode, addr, done)  // Load from dest
TTI_SFPLOADI(lreg, mode, imm16)                     // Load immediate
TTI_SFPSTORE(lreg, mode, addr_mode, addr, done)     // Store to dest
TTI_SFPMAD(a, b, c, dest, mod)                      // dest = a*b + c
TTI_SFPNONLINEAR(src, dest, mode)                   // Nonlinear ops
```

### SFPNONLINEAR Modes
```cpp
p_sfpnonlinear::RECIP_MODE  // 1/x
p_sfpnonlinear::RELU_MODE   // max(0, x)
p_sfpnonlinear::SQRT_MODE   // sqrt(x)
p_sfpnonlinear::EXP_MODE    // e^x
p_sfpnonlinear::TANH_MODE   // tanh(x)
```

### Template Structure
```cpp
template <bool APPROXIMATION_MODE>
inline void _calculate_{op}_(const int iterations)
{
#pragma GCC unroll 8
    for (int d = 0; d < iterations; d++)
    {
        // Load → Process → Store
        TTI_SFPLOAD(p_sfpu::LREG0, ...);
        // ... operation ...
        TTI_SFPSTORE(p_sfpu::LREG1, ...);
        ckernel::math::_incr_counters_<0x0, 0x0, ckernel::math::SFP_ROWS, 0x0>();
    }
}
```

### Examples
- `ckernel_sfpu_relu.h` - Simple (one instruction)
- `ckernel_sfpu_exp.h` - SFPNONLINEAR usage
- `ckernel_sfpu_sigmoid.h` - Complex (multiple steps)

---

## 2. Math Kernels

Matrix and tensor operations using FPU/SFPU.

### Key Files
| File | Operations |
|------|------------|
| `llk_math_matmul.h` | Matrix multiplication |
| `llk_math_reduce.h` | Reduction (sum, max, avg) |
| `llk_math_eltwise_binary.h` | Element-wise binary ops |
| `llk_math_eltwise_unary_datacopy.h` | Data copy |

### Common Patterns
```cpp
// Namespace usage
using namespace ckernel;
using namespace ckernel::trisc;
using namespace ckernel::math;

// Template parameters
template <PoolType type, ReduceDim dim, MathFidelity math_fidelity>
inline void _llk_math_reduce_(...) { ... }
```

### Key Instructions (Math)
```cpp
TTI_GMPOOL(...)    // Global max pool
TTI_GAPOOL(...)    // Global average pool
TTI_ELWADD(...)    // Element-wise add
TTI_MOVD2B(...)    // Move dest to srcB
TTI_SETRWC(...)    // Set read/write counters
TTI_ZEROSRC(...)   // Zero source registers
```

### Reduce Dimensions
```cpp
ReduceDim::REDUCE_ROW  // Reduce along rows
ReduceDim::REDUCE_COL  // Reduce along columns
```

### Pool Types
```cpp
PoolType::MAX  // Max pooling
PoolType::AVG  // Average pooling
PoolType::SUM  // Sum reduction
```

---

## 3. Pack Kernels

Pack data from destination registers to L1 memory.

### Key Files
| File | Purpose |
|------|---------|
| `llk_pack.h` | Basic pack operations |
| `llk_pack_common.h` | Common pack utilities |
| `llk_pack_matmul.h` | Pack for matmul output |
| `llk_pack_untilize.h` | Pack with untilize |

### Template Structure
```cpp
template <std::uint8_t PACK_SEL>
inline void _llk_pack_init_(const std::uint8_t buf_desc_id, const std::uint32_t num_tiles)
{
    _llk_pack_mop_config_<PACK_SEL>(buf_desc_id, num_tiles);
}

template <std::uint8_t PACK_SEL>
inline void _llk_pack_(
    const std::uint32_t start_math_dest_tile_idx,
    const std::uint32_t start_l1_tile_idx)
{ ... }
```

### Pack Resources
```cpp
p_pacr::PACK0  // First packer
p_pacr::PACK1  // Second packer
```

### Key Instructions (Pack)
```cpp
TT_OP_PACR0_TILE_INC(...)  // Pack with PACK0
TT_OP_PACR1_TILE_INC(...)  // Pack with PACK1
```

---

## 4. Unpack Kernels

Unpack data from L1 memory to source registers.

### Key Files
| File | Purpose |
|------|---------|
| `llk_unpack_common.h` | Common unpack utilities |
| `llk_unpack_matmul.h` | Unpack for matmul inputs |
| `llk_unpack_reduce.h` | Unpack for reduction |
| `llk_unpack_tilize.h` | Unpack with tilize |

### Template Structure
```cpp
template <std::uint8_t UNPACK_SEL>
inline void _llk_unpack_init_(...)
{ ... }

template <std::uint8_t UNPACK_SEL>
inline void _llk_unpack_(...)
{ ... }
```

### Unpack Resources
```cpp
p_unpacr::SRCAB  // Unpack to both SRC A and B
p_unpacr::SRCA   // Unpack to SRC A only
p_unpacr::SRCB   // Unpack to SRC B only
```

---

## Common Includes

### SFPU Kernels
```cpp
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Math/Pack/Unpack Kernels
```cpp
#include "ckernel_trisc_common.h"
#include "llk_math_common.h"   // for math
#include "llk_pack_common.h"   // for pack
#include "llk_unpack_common.h" // for unpack
```

---

## Namespaces

```cpp
namespace ckernel { }           // Main namespace
namespace ckernel::sfpu { }     // SFPU operations
namespace ckernel::math { }     // Math utilities
namespace ckernel::trisc { }    // TRISC utilities
```

---

## MathFidelity

```cpp
MathFidelity::LoFi    // Low fidelity (faster)
MathFidelity::HiFi    // High fidelity (more accurate)
MathFidelity::HiFi2   // Higher fidelity
MathFidelity::HiFi3   // Highest fidelity
MathFidelity::HiFi4   // Maximum fidelity
```

---

## Reference Paths

| Architecture | SFPU | Math/Pack/Unpack |
|--------------|------|------------------|
| Blackhole | `tt_llk_blackhole/common/inc/sfpu/` | `tt_llk_blackhole/llk_lib/` |
| Quasar | `tt_llk_quasar/common/inc/sfpu/` | `tt_llk_quasar/llk_lib/` |
| Wormhole | `tt_llk_wormhole_b0/common/inc/sfpu/` | `tt_llk_wormhole_b0/llk_lib/` |
