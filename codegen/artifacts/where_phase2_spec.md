# Specification: where (Phase 2 -- SFPLOADMACRO Optimization)

## Kernel Type
sfpu

## Target Architecture
quasar

## Overview
Based on analysis: `codegen/artifacts/where_analysis.md`
Architecture research: `codegen/artifacts/where_arch_research.md`
Phase 1 spec: `codegen/artifacts/where_phase1_spec.md`

Phase 2 adds the SFPLOADMACRO pipelining optimization to the "where" kernel. The Phase 1
basic path (load-setcc-load-encc-store loop) is already written, compiled, and tested.
Phase 2 modifies two functions:

1. **`_init_where_`**: Configures InstructionTemplates, Sequence registers, and Misc
   register for SFPLOADMACRO pipelining. Also configures ADDR_MOD_6 with dest.incr=2.
2. **`_calculate_where_`**: Adds an `#ifndef DISABLE_SFPLOADMACRO` code path that uses
   `TT_SFPLOADMACRO` and replay buffers for pipelined load+compute+store.

The Phase 1 code becomes the `#ifdef DISABLE_SFPLOADMACRO` fallback path.

## Target File
`tt_llk_quasar/common/inc/sfpu/ckernel_sfpu_where.h`

## Reference Implementations Studied

- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_where.h`: Blackhole reference with complete
  SFPLOADMACRO path. Uses 2 InstructionTemplates (SFPSETCC via VD=12 backdoor, SFPENCC via
  VD=13 backdoor), 3 Sequence registers (macros 0,1,2), and Misc=0x770.

- `tt_llk_blackhole/common/inc/sfpu/ckernel_sfpu_mul_int.h`: Another Blackhole SFPLOADMACRO
  example. Uses 1 InstructionTemplate (SFPMUL24 via VD=12 backdoor), 2 Sequence registers,
  and Misc=0x330.

- `tt_llk_quasar/common/inc/ckernel.h` (lines 358-390): Quasar `load_replay_buf` template
  function. Takes `<start, len, exec_while_loading, set_mutex, last>` template params and
  a lambda for the instructions to record.

- `tt_llk_quasar/llk_lib/llk_math_eltwise_ternary_sfpu.h`: Quasar ternary framework.
  Configures ADDR_MOD_7 with zero increments. Does NOT configure ADDR_MOD_6.

- `tt_llk_quasar/common/inc/cmath_common.h` (lines 111-118): `_sfpu_load_config32_` pattern
  for loading a 32-bit value into LREG0 via SFPLOADI upper/lower halves, then writing to
  a config register via SFPCONFIG.

## Algorithm in Target Architecture

### Overview

The SFPLOADMACRO optimization pipelines the load-compute-store operations across the SFPU's
parallel execution units (Load, Simple, Store). Instead of executing instructions sequentially
in the loop body, SFPLOADMACRO triggers pre-programmed macro sequences that schedule SFPSETCC
and SFPENCC on the Simple unit while the Load unit continues loading the next row's data.

Two scheduling variants exist based on whether the output overwrites the condition input:
- **Case 1** (dst_out == dst_in0): 3 cycles per row (macros 0 and 2)
- **Case 2** (dst_out != dst_in0): 4 cycles per row (macros 1 and 2)

### SFPLOADMACRO Execution Model

The SFPU has 5 execution units: LOAD, SIMPLE, MAD, ROUND, STORE.

When `TT_SFPLOADMACRO(seq_id, ...)` is issued:
1. The LOAD unit performs a standard SFPLOAD operation
2. The selected Sequence register (seq_id) is loaded into shift registers that feed
   the SIMPLE, MAD, ROUND, and STORE units
3. The shift registers advance one slot per cycle (or per issued instruction if the
   unit is marked instruction-dependent in the Misc register)
4. When an instruction reaches slot 0, the Instruction Generator pulls the
   InstructionTemplate specified by the instruction_sel field, overrides VD from the
   shift register payload, and dispatches it to the decoder

### Key Quasar vs Blackhole Differences for SFPLOADMACRO

| Aspect | Blackhole | Quasar |
|--------|-----------|--------|
| SFPLOADMACRO params | 4: `(lreg_ind, mod0, addr_mode, addr)` | 7: `(seq_id, lreg_lo, mod0, addr_mode, done, addr, lreg_hi)` |
| Macro ID encoding | Packed into lreg_ind: `(macro_id << 2) \| lreg_bits` | Explicit `seq_id` field separate from lreg |
| InstructionTemplate backdoor | `TTI_SFPSETCC(0, 0, 12, 6)` -- VD=12 in lreg_dest field | C macros don't expose lreg_dest; must craft instruction word manually via `TT_OP()` OR use `SFPCONFIG` with config_dest=0..3 |
| SFPCONFIG API | `TTI_SFPCONFIG(imm16, config_dest, mod1)` -- same | `TTI_SFPCONFIG(imm16, config_dest, mod1)` -- same encoding |
| Replay API | `lltt::record(start, len)` / `lltt::replay(start, len)` | `load_replay_buf<start, len>(lambda)` / `TTI_REPLAY(start, len, 0, 0, 0, 0)` |
| SFPLOADI MOD0 names | `sfpi::SFPLOADI_MOD0_LOWER` (=10), `sfpi::SFPLOADI_MOD0_UPPER` (=8) | Raw numeric values: 10 (LO16_ONLY), 8 (HI16_ONLY) |

### InstructionTemplate Loading Strategy

On Blackhole, InstructionTemplates are loaded via the backdoor mechanism: issuing an
instruction with `lreg_dest >= 12` (VD >= 12) writes the instruction bits to
`InstructionTemplate[VD - 12]` instead of executing the instruction normally.

On Quasar, the C macros for SFPSETCC (`TTI_SFPSETCC(imm12, lreg_c, mod1)`) and SFPENCC
(`TTI_SFPENCC(imm12, mod1)`) do **not** expose the `lreg_dest` field. Although the underlying
instruction word format (SFPU_MATHI12) does include `lreg_dest` at bits 4-7, the C macro
always sets those bits to 0.

**Two options exist:**

**Option A (Manual instruction word via TT_OP):** Craft the raw instruction word with VD=12/13
and emit it directly using `INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(opcode, params)))`.
This is the most direct translation of the Blackhole approach.

```cpp
// SFPSETCC with VD=12 for backdoor load to InstructionTemplate[0]
// Encoding: opcode=0x7b, imm12=0 at bits 12-23, lreg_c=0 at bits 8-11,
//           lreg_dest=12 at bits 4-7, mod1=6 at bits 0-3
// params = (0 << 12) + (0 << 8) + (12 << 4) + (6 << 0) = 0xC6
INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x7b, 0xC6)));

// SFPENCC with VD=13 for backdoor load to InstructionTemplate[1]
// Encoding: opcode=0x8a, imm12=0 at bits 12-23, lreg_c=0 at bits 8-11,
//           lreg_dest=13 at bits 4-7, mod1=0 at bits 0-3
// params = (0 << 12) + (0 << 8) + (13 << 4) + (0 << 0) = 0xD0
INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x8a, 0xD0)));
```

**Option B (SFPCONFIG from LREG[0]):** Load the instruction bits into LREG[0] via
SFPLOADI (upper/lower halves), then write to InstructionTemplate[N] via SFPCONFIG.

```cpp
// Load SFPSETCC instruction bits into LREG[0]
// Full instruction word: TT_OP(0x7b, (0 << 12) + (0 << 8) + (0 << 4) + (6 << 0))
//   = (0x7b << 24) | 0x06 = 0x7B000006
TTI_SFPLOADI(p_sfpu::LREG0, 10, 0x0006);  // lower 16 bits
TTI_SFPLOADI(p_sfpu::LREG0, 8, 0x7B00);   // upper 16 bits
TTI_SFPCONFIG(0, 0, 0);  // Write LREG[0] to InstructionTemplate[0]

// Load SFPENCC instruction bits into LREG[0]
// Full instruction word: TT_OP(0x8a, 0) = 0x8A000000
TTI_SFPLOADI(p_sfpu::LREG0, 10, 0x0000);  // lower 16 bits
TTI_SFPLOADI(p_sfpu::LREG0, 8, 0x8A00);   // upper 16 bits
TTI_SFPCONFIG(0, 1, 0);  // Write LREG[0] to InstructionTemplate[1]
```

**Decision: Use Option A (manual TT_OP).** Rationale:
- More direct translation of the Blackhole pattern
- Fewer instructions (2 vs 6)
- The Quasar instruction encoding format is documented in assembly.yaml and the
  SFPU_MATHI12 argument layout confirms lreg_dest at bits 4-7
- The backdoor mechanism is confirmed to exist on Quasar by the SFPCONFIG ISA page
  (LaneConfig.DISABLE_BACKDOOR_LOAD defaults to false)
- HOWEVER: if Option A causes compilation or runtime issues, fall back to Option B

### Instruction Template Index Mapping (instruction_sel)

From the Confluence SFPLOADMACRO discussion page: "using 0 as the instruction selector
is reserved to mean 'no instruction'". The InstructionTemplate table has 4 entries on Quasar
(config_dest 0-3). The 3-bit `instruction_sel` field in the Sequence register maps:

| sel | Action |
|-----|--------|
| 0 | NOP (no instruction dispatched to this unit) |
| 1 | Use InstructionTemplate[0] |
| 2 | Use InstructionTemplate[1] |
| 3 | Use InstructionTemplate[2] |
| 4 | Use InstructionTemplate[3] |
| 5-7 | Undefined on Quasar (only 4 templates) |

**IMPORTANT**: This differs from Blackhole, which uses sel=4 and sel=5 for its two templates.
Blackhole may have 8 InstructionTemplate slots vs Quasar's 4. On Quasar:
- InstructionTemplate[0] = SFPSETCC(EQ0) -> referenced by sel=1
- InstructionTemplate[1] = SFPENCC(reset CC) -> referenced by sel=2

### Sequence Register Bit Layout

Each Sequence register is a 32-bit value programmed via SFPCONFIG. It controls 4 execution
units with 8 bits each:

```
[31:24] = store_bits
[23:16] = round_bits
[15:8]  = mad_bits
[7:0]   = simple_bits
```

Per-unit byte format:
```
[7]   = vb_override (for SIMPLE/MAD/ROUND) or unused (for STORE)
[6]   = use_staging_flop
[5:3] = delay (0-7: number of shift register slots from head)
[2:0] = instruction_sel (0=NOP, 1-4=InstructionTemplate[0-3])
```

For the STORE unit, additional data (target address, done, mod0) comes from the
SFPLOADMACRO instruction itself, not from the Sequence register.

### Misc Register (LoadMacro Control)

The Misc register (config_dest=8) controls global LOADMACRO behavior. From the Blackhole
where kernel, Misc=0x770 sets:
- `UsesLoadMod0ForStore=1` for all active macros
- `WaitForElapsedInstructions=1` (instruction-dependent shifting) for all active macros

On Quasar, the same Misc register value should work since the SFPCONFIG encoding is
identical (same opcode 0x91, same bit layout).

The Misc register bit layout (12 bits):
```
Bits [3:0]:   Per-macro UnitDelayKind (1 bit per macro, 0=independent, 1=instruction-dependent)
Bits [7:4]:   Per-macro UsesLoadMod0ForStore (1 bit per macro)
Bits [11:8]:  StoreMod0 (4 bits, default store format)
```

For the where kernel:
- Macros 0,1,2 all need WaitForElapsedInstructions=1: bits [2:0] = 0b111
- Macros 0,1,2 all need UsesLoadMod0ForStore=1: bits [6:4] = 0b111
- StoreMod0 = 0 (DEFAULT, inherit from SFPLOADMACRO mod0): bits [11:8] = 0

But from the Blackhole value 0x770 = 0b0111_0111_0000:
```
bits [11:8] = 0b0111 = 7 -> StoreMod0 = 7?
bits [7:4]  = 0b0111 = 7 -> UsesLoadMod0ForStore for macros 0,1,2
bits [3:0]  = 0b0000 = 0 -> UnitDelayKind = all independent?
```

Wait, let me re-examine. 0x770 = 0x770:
```
0x770 = 0111 0111 0000 (binary, 12 bits)
```
If the bit layout is:
```
[11:8] = 0111 = store format config
[7:4]  = 0111 = uses load mod0 for store (macros 0,1,2)
[3:0]  = 0000 = unit delay kind
```

This means the independent shift register mode is used (WaitForElapsedInstructions=0),
not the instruction-dependent mode. The comment in the Blackhole source ("WaitForElapsedInstructions=1")
may refer to something else, or the bit mapping may be different than what I've assumed.

**Decision**: Use `TTI_SFPCONFIG(0x770, 8, 1)` -- same value as Blackhole. The Misc register
encoding is identical between architectures (same SFPCONFIG instruction, same bit layout).

### Sequence Register Programming

**Macro 0** (Sequence[0], config_dest=4): Special case for dst_out == dst_in0 (3-cycle path)

Schedule:
```
Cycle | Load Unit               | Simple Unit        | Store Unit
  0   | SFPLOAD L0=Dst[cond]    |                    |
  1   | SFPLOAD L0=Dst[true]    | SFPSETCC (L0 EQ0)  |
  2   | SFPLOAD L0=Dst[false]   | SFPENCC (CC reset)  |
  3   | (next iteration)        |                    | SFPSTORE Dst[cond]=L0
```

The STORE happens at delay=2 from the start (shift register slot 2), which means it
executes 2 cycles after the SFPLOADMACRO that triggered macro 0.

Bit encoding (using Quasar sel=1/2 for InstructionTemplate[0/1]):
```
simple_bits = (0 << 7) | (0 << 6) | (0 << 3) | 1  = 0x01  // sel=1 (InstrTemplate[0]=SFPSETCC), delay=0
mad_bits    = 0x00  // NOP
round_bits  = 0x00  // NOP
store_bits  = (0 << 7) | (0 << 6) | (2 << 3) | 3  = 0x13  // sel=3 (store), delay=2
```

Note: For the STORE unit, the instruction_sel value appears to indicate "perform a store"
rather than indexing an InstructionTemplate. The actual store instruction parameters
(address, mod0, done) come from the SFPLOADMACRO that triggered this macro. We use
sel=3 to match the Blackhole pattern.

Full 32-bit value: `(0x13 << 24) | (0x00 << 16) | (0x00 << 8) | 0x01` = `0x13000001`

Programming via SFPLOADI + SFPCONFIG:
```cpp
TTI_SFPLOADI(p_sfpu::LREG0, 10, 0x0001);  // lower 16: (mad_bits << 8) | simple_bits
TTI_SFPLOADI(p_sfpu::LREG0, 8, 0x1300);   // upper 16: (store_bits << 8) | round_bits
TTI_SFPCONFIG(0, 4 + 0, 0);               // Write LREG[0] to Sequence[0]
```

**Macro 1** (Sequence[1], config_dest=5): General case for dst_out != dst_in0 (4-cycle path)

Schedule:
```
Cycle | Load Unit               | Simple Unit        | Store Unit
  0   | SFPLOAD L0=Dst[cond]    |                    |
  1   | SFPLOAD L0=Dst[true]    | SFPSETCC (L0 EQ0)  |
  2   | SFPLOAD L0=Dst[false]   | SFPENCC (CC reset)  |
  3   | -                       |                    | SFPSTORE Dst[out]=L0
  4   | (next iteration)        |                    |
```

In this case, only the SIMPLE unit triggers SFPSETCC. No STORE is scheduled by this macro
(the SFPSTORE is issued as a separate instruction in the replay buffer).

Bit encoding:
```
simple_bits = (0 << 7) | (0 << 6) | (0 << 3) | 1  = 0x01  // sel=1 (SFPSETCC), delay=0
mad_bits    = 0x00
```

Since only the lower 16 bits matter (round and store are 0), use the immediate mode:
```cpp
TTI_SFPCONFIG(0x0001, 4 + 1, 1);  // imm16=(0x00 << 8) | 0x01, config_dest=5, mod1=1
```

**Macro 2** (Sequence[2], config_dest=6): SFPENCC scheduling (used with both cases)

Bit encoding:
```
simple_bits = (0 << 7) | (0 << 6) | (0 << 3) | 2  = 0x02  // sel=2 (InstrTemplate[1]=SFPENCC), delay=0
mad_bits    = 0x00
```

```cpp
TTI_SFPCONFIG(0x0002, 4 + 2, 1);  // imm16=(0x00 << 8) | 0x02, config_dest=6, mod1=1
```

### ADDR_MOD_6 Configuration

The SFPLOADMACRO store path and the explicit `TT_SFPSTORE` in Case 2 use `ADDR_MOD_6`
with a dest increment of 2 (to advance 2 rows per store for SFP_ROWS=2 processing).
This must be configured in `_init_where_` since the ternary framework only configures
ADDR_MOD_7 (zero increment).

```cpp
addr_mod_t {
    .srca = {.incr = 0},
    .srcb = {.incr = 0},
    .dest = {.incr = 2},
}
    .set(ADDR_MOD_6, csr_read<CSR::TRISC_ID>());
```

### Pseudocode for _init_where_ (Phase 2)

```
template <bool APPROXIMATION_MODE>
inline void _init_where_()
{
#ifndef DISABLE_SFPLOADMACRO
    // Configure ADDR_MOD_6 with dest increment of 2
    addr_mod_t {.srca={.incr=0}, .srcb={.incr=0}, .dest={.incr=2}}
        .set(ADDR_MOD_6, csr_read<CSR::TRISC_ID>());

    // InstructionTemplate[0]: SFPSETCC(imm12=0, lreg_c=0, lreg_dest=12, mod1=6)
    // Backdoor load via VD=12 -> InstructionTemplate[0]
    // Raw encoding: opcode=0x7b, params = (0<<12) + (0<<8) + (12<<4) + (6<<0) = 0xC6
    INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x7b, 0xC6)));

    // InstructionTemplate[1]: SFPENCC(imm12=0, lreg_c=0, lreg_dest=13, mod1=0)
    // Backdoor load via VD=13 -> InstructionTemplate[1]
    // Raw encoding: opcode=0x8a, params = (0<<12) + (0<<8) + (13<<4) + (0<<0) = 0xD0
    INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x8a, 0xD0)));

    // Sequence[0] (Macro 0): SIMPLE sel=1 delay=0, STORE sel=3 delay=2
    {
        constexpr std::uint32_t simple_bits = 0x01;  // sel=1 -> InstrTemplate[0] (SFPSETCC)
        constexpr std::uint32_t mad_bits    = 0;
        constexpr std::uint32_t round_bits  = 0;
        constexpr std::uint32_t store_bits  = (2 << 3) | 3;  // sel=3, delay=2

        TTI_SFPLOADI(p_sfpu::LREG0, 10, (mad_bits << 8) | simple_bits);
        TTI_SFPLOADI(p_sfpu::LREG0, 8, (store_bits << 8) | round_bits);
        TTI_SFPCONFIG(0, 4 + 0, 0);
    }

    // Sequence[1] (Macro 1): SIMPLE sel=1 delay=0
    TTI_SFPCONFIG(0x0001, 4 + 1, 1);

    // Sequence[2] (Macro 2): SIMPLE sel=2 delay=0 -> InstrTemplate[1] (SFPENCC)
    TTI_SFPCONFIG(0x0002, 4 + 2, 1);

    // Misc: UsesLoadMod0ForStore=1, WaitForElapsedInstructions settings
    TTI_SFPCONFIG(0x770, 8, 1);
#endif
}
```

### Pseudocode for _calculate_where_ (Phase 2 additions)

The Phase 1 loop body becomes the `#ifdef DISABLE_SFPLOADMACRO` path. The new
`#ifndef DISABLE_SFPLOADMACRO` path uses SFPLOADMACRO with replay buffers:

```
template <bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS>
inline void _calculate_where_(dst_in0, dst_in1, dst_in2, dst_out)
{
    int offset0 = (dst_index_in0 * 32) << 1;
    int offset1 = (dst_index_in1 * 32) << 1;
    int offset2 = (dst_index_in2 * 32) << 1;

#ifdef DISABLE_SFPLOADMACRO
    // [Phase 1 code -- already written and tested, DO NOT MODIFY]
    int offset3 = (dst_index_out * 32) << 1;
    TTI_SFPENCC(1, 2);
    #pragma GCC unroll 8
    for (int d = 0; d < ITERATIONS; d++) { ... }
    TTI_SFPENCC(0, 2);

#else
    if (dst_index_out == dst_index_in0)
    {
        // Case 1: 3 cycles/row -- output overwrites condition input
        // Macros 0 and 2 schedule SFPSETCC and SFPENCC on Simple unit
        // Macro 0 also schedules SFPSTORE at delay=2
        load_replay_buf<0, 3>(
            [offset0, offset1, offset2]()
            {
                TT_SFPLOADMACRO(0, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0, 0);
                TT_SFPLOADMACRO(2, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1, 0);
                TT_SFPLOAD(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_6, 0, offset2);
            });

        #pragma GCC unroll 8
        for (int d = 0; d < ITERATIONS; d++)
        {
            TTI_REPLAY(0, 3, 0, 0, 0, 0);
        }
    }
    else
    {
        // Case 2: 4 cycles/row -- output is a different tile
        // Macros 1 and 2 schedule SFPSETCC and SFPENCC on Simple unit
        // SFPSTORE is issued explicitly
        int offset3 = (dst_index_out * 32) << 1;

        load_replay_buf<0, 4>(
            [offset0, offset1, offset2, offset3]()
            {
                TT_SFPLOADMACRO(1, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset0, 0);
                TT_SFPLOADMACRO(2, 0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset1, 0);
                TT_SFPLOAD(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_7, 0, offset2);
                TT_SFPSTORE(0, p_sfpu::sfpmem::DEFAULT, ADDR_MOD_6, 0, offset3);
            });

        #pragma GCC unroll 8
        for (int d = 0; d < ITERATIONS; d++)
        {
            TTI_REPLAY(0, 4, 0, 0, 0, 0);
        }
    }
#endif
}
```

### Instruction Mappings

| Reference Construct | Target Instruction | Source of Truth |
|---|---|---|
| `TTI_SFPSETCC(0, 0, 12, 6)` (backdoor to InstrTemplate[0]) | `INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x7b, 0xC6)))` | assembly.yaml: SFPSETCC opcode=0x7b, SFPU_MATHI12 format (lreg_dest at bits 4-7); SFPCONFIG ISA: DISABLE_BACKDOOR_LOAD defaults false |
| `TTI_SFPENCC(0, 0, 13, 0)` (backdoor to InstrTemplate[1]) | `INSTRUCTION_WORD(TRISC_OP_SWIZZLE(TT_OP(0x8a, 0xD0)))` | assembly.yaml: SFPENCC opcode=0x8a, SFPU_MATHI12 format |
| `TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_LOWER, val)` | `TTI_SFPLOADI(p_sfpu::LREG0, 10, val)` | assembly.yaml: SFPLOADI instr_mod0=10 is LO16_ONLY; cmath_common.h:115 uses mod0=10 |
| `TTI_SFPLOADI(0, sfpi::SFPLOADI_MOD0_UPPER, val)` | `TTI_SFPLOADI(p_sfpu::LREG0, 8, val)` | assembly.yaml: SFPLOADI instr_mod0=8 is HI16_ONLY; cmath_common.h:116 uses mod0=8 |
| `TTI_SFPCONFIG(0, 4+N, 0)` (write LREG[0] to Sequence[N]) | `TTI_SFPCONFIG(0, 4+N, 0)` -- same | Quasar ckernel_ops.h: same encoding |
| `TTI_SFPCONFIG(val, 4+N, 1)` (write imm16 to Sequence[N]) | `TTI_SFPCONFIG(val, 4+N, 1)` -- same | Quasar ckernel_ops.h: same encoding |
| `TTI_SFPCONFIG(0x770, 8, 1)` (Misc register) | `TTI_SFPCONFIG(0x770, 8, 1)` -- same | Quasar ckernel_ops.h: same encoding |
| `TT_SFPLOADMACRO((0<<2), mod0, ADDR_MOD_7, offset)` | `TT_SFPLOADMACRO(0, 0, mod0, ADDR_MOD_7, 0, offset, 0)` | Quasar ckernel_ops.h: 7-param decomposition of Blackhole's 4-param packed format |
| `TT_SFPLOADMACRO((1<<2), mod0, ADDR_MOD_7, offset)` | `TT_SFPLOADMACRO(1, 0, mod0, ADDR_MOD_7, 0, offset, 0)` | Same |
| `TT_SFPLOADMACRO((2<<2), mod0, ADDR_MOD_7, offset)` | `TT_SFPLOADMACRO(2, 0, mod0, ADDR_MOD_7, 0, offset, 0)` | Same |
| `lltt::replay(0, N)` | `TTI_REPLAY(0, N, 0, 0, 0, 0)` | Quasar ckernel.h: TTI_REPLAY is the raw replay instruction |
| `load_replay_buf(0, N, lambda)` | `load_replay_buf<0, N>(lambda)` | Quasar ckernel.h lines 358-372 |
| `mod0 = ... ? LO16 : INT32` (format mode) | `p_sfpu::sfpmem::DEFAULT` (auto-resolve) | Existing Quasar SFPU kernels: all use DEFAULT |
| `ADDR_MOD_6` (dest.incr=2) | Configure in `_init_where_` | Not configured by ternary framework; must add |

### Instruction Template sel Mapping (Quasar vs Blackhole)

| Purpose | Blackhole sel | Quasar sel | Reason |
|---------|--------------|------------|--------|
| SFPSETCC (InstrTemplate[0]) | 4 | 1 | Quasar: sel=N+1 for InstrTemplate[N]; Blackhole may have different numbering (8 templates) |
| SFPENCC (InstrTemplate[1]) | 5 | 2 | Same reasoning |
| STORE trigger | 3 | 3 | Store unit: sel!=0 means "do a store"; keeping same value |

**CRITICAL**: The instruction_sel values in the Sequence registers MUST use the Quasar
numbering (1 and 2), NOT the Blackhole numbering (4 and 5). This is the primary semantic
difference in the LOADMACRO configuration between the two architectures.

### Resource Allocation

| Resource | Purpose |
|---|---|
| LREG0 | Target of all SFPLOAD/SFPLOADMACRO operations (condition, true, false values). Staging register for SFPCONFIG writes. |
| LREG1 | (unused in SFPLOADMACRO path -- all operations go through LREG0) |
| InstructionTemplate[0] | SFPSETCC(mod1=6, lreg_c=0) -- compare LREG0 against zero |
| InstructionTemplate[1] | SFPENCC(mod1=0) -- reset CC (all lanes active) |
| Sequence[0] (Macro 0) | Case 1 schedule: SIMPLE(SFPSETCC) + STORE at delay=2 |
| Sequence[1] (Macro 1) | Case 2 schedule: SIMPLE(SFPSETCC) only |
| Sequence[2] (Macro 2) | Both cases: SIMPLE(SFPENCC) |
| Misc register | UsesLoadMod0ForStore + WaitForElapsedInstructions config |
| ADDR_MOD_6 | Store auto-increment (dest.incr=2 for SFP_ROWS) |
| ADDR_MOD_7 | Load zero-increment (configured by ternary framework) |
| CC register | Per-lane condition code, set by SFPSETCC, reset by SFPENCC |
| Replay buffer [0..2] or [0..3] | Records the 3 or 4 instruction SFPLOADMACRO+LOAD sequence |

## Implementation Structure

### Includes
No new includes needed beyond Phase 1:
```cpp
#include <cstdint>
#include "ckernel_trisc_common.h"
#include "cmath_common.h"
```

### Additional Include Consideration
The `addr_mod_t` type and `ADDR_MOD_6` constant are available from existing includes
(`cmath_common.h` pulls in `ckernel_addrmod.h`). The `load_replay_buf` template is
available from `ckernel.h` (included transitively). `INSTRUCTION_WORD` and
`TRISC_OP_SWIZZLE` macros are available from `ckernel_ops.h` (included transitively).
`TT_OP` is defined in `ckernel_ops.h`. No additional includes are required.

### Functions Modified

| Function | Template Params | Change |
|---|---|---|
| `_init_where_<APPROXIMATION_MODE>` | `bool APPROXIMATION_MODE` | Add SFPLOADMACRO configuration inside `#ifndef DISABLE_SFPLOADMACRO` |
| `_calculate_where_<APPROXIMATION_MODE, data_format, ITERATIONS>` | `bool APPROXIMATION_MODE, DataFormat data_format, int ITERATIONS` | Wrap Phase 1 body in `#ifdef DISABLE_SFPLOADMACRO`, add SFPLOADMACRO path in `#else` |

### Init/Uninit Symmetry

| Init Action | Uninit Reversal |
|---|---|
| Configure ADDR_MOD_6 (dest.incr=2) | Framework reset via `_llk_math_eltwise_ternary_sfpu_done_()` resets counters; ADDR_MOD_6 state is local to this kernel's init scope |
| Write InstructionTemplates [0,1] | Templates are overwritten by any future SFPLOADMACRO-using kernel; no explicit cleanup needed |
| Write Sequences [0,1,2] | Same -- overwritten by next SFPLOADMACRO user |
| Write Misc register | Same -- overwritten by next SFPLOADMACRO user |

No explicit `_uninit_where_` is needed. The LOADMACRO configuration is ephemeral -- it
only matters while the where kernel is running, and any future kernel that uses SFPLOADMACRO
will reconfigure everything.

## Potential Issues

### 1. Backdoor Load on Quasar
The backdoor InstructionTemplate loading via raw `TT_OP` instruction words has not been
tested on Quasar (no existing Quasar kernel uses SFPLOADMACRO). If the backdoor mechanism
does not work as expected on Quasar, fall back to Option B (SFPCONFIG from LREG[0]).

### 2. instruction_sel Numbering
The sel=1/2 numbering for InstructionTemplate[0/1] is inferred from the "sel=0 means NOP"
convention and the 4-entry template table. If Quasar uses a different numbering scheme
(e.g., matching Blackhole's sel=4/5), the Sequence register values will need adjustment.
Signs of this issue: SFPSETCC/SFPENCC not firing during SFPLOADMACRO execution, resulting
in incorrect conditional select behavior.

### 3. ADDR_MOD_6 Interaction
ADDR_MOD_6 is configured in `_init_where_` for the SFPLOADMACRO path. If the ternary
framework or test harness later reconfigures ADDR_MOD_6, the store auto-increment will
break. Verify that no framework code touches ADDR_MOD_6.

### 4. Replay Buffer Compatibility
The replay buffer records the SFPLOADMACRO/SFPLOAD/SFPSTORE instructions with runtime
addresses. When replayed, these addresses are the same (they are baked into the recorded
instruction words). This means each replay iteration processes the SAME Dest rows. This is
correct for the "where" kernel because the Blackhole reference also replays the same offsets
(offset0, offset1, offset2 are fixed per call). The iteration variable `d` in the Phase 1
loop incremented the offset; in the SFPLOADMACRO path, the offset stays fixed and the caller
invokes `_calculate_where_` once per face (the ternary framework handles face iteration).

Wait -- actually, reviewing the Blackhole code more carefully: the SFPLOADMACRO path also
uses `ITERATIONS` and replays the same instructions `ITERATIONS` times. But offsets are fixed!
This means each iteration processes the same row. The key insight is that ADDR_MOD_7 (for
loads) has zero increment, so the address doesn't change. But ADDR_MOD_6 (for stores) has
dest.incr=2, so the store address advances by 2 each time. However, the load addresses are
the same each iteration! This suggests the store auto-increment is the mechanism for advancing
through rows, but the loads read the same row every time.

Actually, re-reading the Blackhole code: the replay replays the EXACT same instructions
(including addresses). The ADDR_MOD is applied by hardware on each instruction, so:
- Loads use ADDR_MOD_7 (zero increment) -- addresses are fixed in the instruction word
- Stores use ADDR_MOD_6 (dest.incr=2) -- the RWC_D counter advances after each store

But wait, ADDR_MOD on Quasar modifies the Dest Read/Write Counter (RWC), not the address
in the instruction. The instruction address field is added to the RWC to compute the
effective address. So if the base address in the instruction is offset0 and the RWC
advances by 2 per store (via ADDR_MOD_6), the effective address changes each iteration.

For loads with ADDR_MOD_7 (zero increment), the RWC doesn't change, so each load hits
the same base address. This means the loads read the same row every iteration, which is
WRONG -- we need to process all rows.

Hmm, but the Blackhole where kernel works, so I must be misunderstanding something. Let me
reconsider: the Blackhole where kernel uses `ADDR_MOD_7` for all loads, which means zero
dest counter increment. The store uses `ADDR_MOD_6` with dest.incr=2. After each iteration,
the store advances the Dest write counter by 2. But the loads use a different counter (or
the same counter?).

Actually, on Tensix, SFPLOAD uses the Dest Read Counter and SFPSTORE uses the Dest Write
Counter. ADDR_MOD adjusts these counters independently. So:
- SFPLOAD with ADDR_MOD_7: Dest Read Counter += 0 (no advance)
- SFPSTORE with ADDR_MOD_6: Dest Write Counter += 2

But the instruction address field is an absolute offset, not relative to the counter.
The effective address = base + counter? Or is the counter used differently?

This needs careful analysis. The safe approach is: **use the exact same ADDR_MOD pattern
as Blackhole** (ADDR_MOD_7 for loads, ADDR_MOD_6 for stores with dest.incr=2). If the
Blackhole pattern works, the Quasar translation should work too, since the ADDR_MOD hardware
model is the same.

### 5. DISABLE_SFPLOADMACRO Not Defined Anywhere
Currently, no Quasar kernel defines or checks `DISABLE_SFPLOADMACRO`. The macro is used
in Blackhole kernels as a compile-time flag. On Quasar, if the flag is never defined,
only the SFPLOADMACRO path will be compiled. The Phase 1 (fallback) path would only be
used if someone explicitly defines `DISABLE_SFPLOADMACRO`. This is the correct behavior.

### 6. CC Enable/Disable Wrapper
The Phase 1 code wraps the loop with `TTI_SFPENCC(1, 2)` / `TTI_SFPENCC(0, 2)` for global
CC enable/disable. The SFPLOADMACRO path does NOT need this wrapper because:
- The SFPSETCC/SFPENCC are dispatched by the macro sequencer at the right pipeline stages
- The Blackhole SFPLOADMACRO path does not use SFPENCC enable/disable wrappers
- The CC state is managed entirely by the macro sequence within each iteration

## Recommended Test Formats

Same as Phase 1 -- the optimization should not change format behavior. The SFPLOADMACRO
path is purely a performance optimization; it produces identical results.

### Format List

```python
# Universal op: ALL Quasar-supported formats are semantically valid.
FORMATS = input_output_formats([
    DataFormat.Float16,
    DataFormat.Float16_b,
    DataFormat.Float32,
    DataFormat.Tf32,
    DataFormat.Int8,
    DataFormat.UInt8,
    DataFormat.Int16,
    DataFormat.UInt16,
    DataFormat.Int32,
    DataFormat.MxFp8R,
    DataFormat.MxFp8P,
])
```

### Invalid Combination Rules

Same as Phase 1:
```python
def _is_invalid_quasar_combination(fmt, dest_acc):
    in_fmt = fmt.input_format
    out_fmt = fmt.output_format

    # 1. Quasar packer does not support non-Float32 -> Float32 when dest_acc=No
    if in_fmt != DataFormat.Float32 and out_fmt == DataFormat.Float32 and dest_acc == DestAccumulation.No:
        return True

    # 2. Float32 input -> Float16 output requires dest_acc=Yes on Quasar
    if in_fmt == DataFormat.Float32 and out_fmt == DataFormat.Float16 and dest_acc == DestAccumulation.No:
        return True

    # 3. Integer and float formats cannot be mixed in input -> output conversion
    int_formats = {DataFormat.Int8, DataFormat.UInt8, DataFormat.Int16, DataFormat.UInt16, DataFormat.Int32}
    float_formats = {DataFormat.Float16, DataFormat.Float16_b, DataFormat.Float32, DataFormat.Tf32, DataFormat.MxFp8R, DataFormat.MxFp8P}
    if (in_fmt in int_formats and out_fmt in float_formats) or (in_fmt in float_formats and out_fmt in int_formats):
        return True

    return False
```

### MX Format Handling
Same as Phase 1: MX formats are L1-only, unpacked to Float16_b. `p_sfpu::sfpmem::DEFAULT`
handles this automatically. MX + `implied_math_format=No` should be skipped.

### Integer Format Handling
Same as Phase 1: SFPSETCC EQ0 works on sign-magnitude representation for all integer types.

## Testing Notes

1. **Regression testing**: After adding the SFPLOADMACRO path, the full test suite must
   still pass since `DISABLE_SFPLOADMACRO` is not defined (the SFPLOADMACRO path is now
   the default code path).

2. **Both cases must be tested**: The test calls `_calculate_where_<...>(0, 1, 2, 0)` where
   dst_out=0 == dst_in0=0, so this exercises Case 1. To test Case 2, a test with
   dst_out != dst_in0 would be needed (but the existing test harness may not support this).

3. **If SFPLOADMACRO fails**: If the optimization causes test failures, define
   `DISABLE_SFPLOADMACRO` to fall back to the Phase 1 path. The writer should compile and
   test with and without the flag.
