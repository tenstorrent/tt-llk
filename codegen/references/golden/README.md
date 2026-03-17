# Golden Reference Kernels

These are the best manually-ported Quasar kernels. Use them as canonical examples when generating new kernels.

## Files

| File | Complexity | Key Patterns |
|------|------------|--------------|
| `ckernel_sfpu_exp.h` | Simple | SFPNONLINEAR, APPROXIMATION_MODE template |
| `ckernel_sfpu_relu.h` | Simple | SFPNONLINEAR, no template (always same behavior) |
| `ckernel_sfpu_recip.h` | Simple | SFPNONLINEAR with RECIP_MODE |
| `ckernel_sfpu_lrelu.h` | Medium | Conditionals (SFPSETCC/SFPENCC), SFPMAD, SFPLOADI, SFPGT, SFPMOV |

## Usage

When generating a new kernel, agents should read these files to understand:
1. File structure and license header
2. Namespace conventions (`ckernel::sfpu`)
3. Load/Store patterns with `ADDR_MOD_7`
4. How `_incr_counters_` is used in the iteration loop
5. How conditionals work (SFPSETCC/SFPENCC enable/disable pattern)
6. How constants are loaded (SFPLOADI)

## Source

These files are copies from `tt_llk_quasar/common/inc/sfpu/`. If the originals are updated, refresh these copies.
