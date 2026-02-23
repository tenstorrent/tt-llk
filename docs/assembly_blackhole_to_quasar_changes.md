# Assembly ISA: Blackhole vs Quasar Instructions Changes

**73** instructions with real encoding changes | **17** with trivial-only changes | **74** new in Quasar | **47** removed from Blackhole

---

## Table of Contents

### Changed Instructions
- [Mutex / Synchronization (5)](#mutex_synchronization)
- [Atomic Memory Operations (6)](#atomic_memory_operations)
- [Register File Move (Src/Dst) (6)](#register_file_move_srcdst)
- [Compute — Dest / Src Control (4)](#compute_dest_src_control)
- [RWC Counters (2)](#rwc_counters)
- [Pack / Unpack (1)](#pack_unpack)
- [Memory Indirect / Register (4)](#memory_indirect_register)
- [CFG Instructions (5)](#cfg_instructions)
- [Control Flow / Meta (5)](#control_flow_meta)
- [SFPU — Load/Store/Misc (4)](#sfpu_loadstoremisc)
- [Other (31)](#other)
- [Instructions With Identical Bit Layout (17)](#instructions_with_identical_bit_layout)

### New Instructions (Quasar)
- [GPR Arithmetic (7)](#new_gpr_arithmetic)
- [Pack (PACR) Variants (12)](#new_pack_pacr_variants)
- [Unpack (UNPACR) Variants (28)](#new_unpack_unpacr_variants)
- [Tile Buffer Management (4)](#new_tile_buffer_management)
- [Address Mode (4)](#new_address_mode)
- [SFPU (1)](#new_sfpu)
- [CFG Aliases (5)](#new_cfg_aliases)
- [Other New (9)](#new_other_new)
- [Other New (4)](#new_other_new)

### Removed Instructions (Blackhole only)
- [DMA Register Arithmetic (7)](#rem_dma_register_arithmetic)
- [Convolution / Pooling (8)](#rem_convolution_pooling)
- [Address Control (8)](#rem_address_control)
- [Compute Misc (12)](#rem_compute_misc)
- [Pack / Unpack / DMA (8)](#rem_pack_unpack_dma)
- [CFG / Stream (3)](#rem_cfg_stream)
- [SFPU (1)](#rem_sfpu)

---

# Changed Instructions

## Mutex / Synchronization

### `STALLWAIT`

```
Blackhole:
┌───────────────────────────┬─────────────────────────────────────────────┐
│         stall_res         │                   wait_res                  │
│         9b [23:15]        │                  15b [14:0]                 │
└───────────────────────────┴─────────────────────────────────────────────┘

Quasar:
┌───────────────────────────┬────────────────┬────────────────┬────────────────┐
│         stall_res         │ wait_res_idx_2 │ wait_res_idx_1 │ wait_res_idx_0 │
│         9b [23:15]        │   5b [14:10]   │    5b [9:5]    │    5b [4:0]    │
└───────────────────────────┴────────────────┴────────────────┴────────────────┘
```

- **Removed args**: `wait_res`
- **Added args**: `wait_res_idx_0`, `wait_res_idx_1`, `wait_res_idx_2`
- **`stall_res`**: description changed
- **`wait_res_idx_0`** (NEW) [4:0] 5b: Resource to be waited for. This is an enum value to select one of the following options:  0 - (nothing)  1 - thcon  2 -
- **`wait_res_idx_1`** (NEW) [9:5] 5b: resource to be waited for - see wait_res_idx_0 for enum codes
- **`wait_res_idx_2`** (NEW) [14:10] 5b: resource to be waited for - see wait_res_idx_0 for enum codes

### `SEMINIT`

```
Blackhole:
┌────────────┬────────────┬──────────────────────────────────────────┬──────────┐
│ max_value  │ init_value │                 sem_sel                  │   ---    │
│ 4b [23:20] │ 4b [19:16] │                14b [15:2]                │ 2b [1:0] │
└────────────┴────────────┴──────────────────────────────────────────┴──────────┘

Quasar:
┌────────────┬────────────┬────────────┬───────────────┬────────────────────────┐
│ max_value  │ init_value │    ---     │  sem_bank_sel │        sem_sel         │
│ 4b [23:20] │ 4b [19:16] │ 3b [15:13] │   5b [12:8]   │        8b [7:0]        │
└────────────┴────────────┴────────────┴───────────────┴────────────────────────┘
```

- **BH unused gap** [1:0] (2b), now used by `sem_sel`
- **Added args**: `sem_bank_sel`
- **`sem_sel`**: start_bit 2 → 0; width 14b → 8b; description changed
- **`sem_bank_sel`** (NEW) [12:8] 5b: The semaphores are divided into groups of 8; this selects the group. For example, if 0, this selects semaphores 0..7

### `SEMPOST`

```
Blackhole:
┌──────────────────────────────────────────────────────────────────┬──────────┐
│                             sem_sel                              │   ---    │
│                            22b [23:2]                            │ 2b [1:0] │
└──────────────────────────────────────────────────────────────────┴──────────┘

Quasar:
┌─────────────────────────────────┬───────────────┬────────────────────────┐
│               ---               │  sem_bank_sel │        sem_sel         │
│           11b [23:13]           │   5b [12:8]   │        8b [7:0]        │
└─────────────────────────────────┴───────────────┴────────────────────────┘
```

- **BH unused gap** [1:0] (2b), now used by `sem_sel`
- **Added args**: `sem_bank_sel`
- **`sem_sel`**: start_bit 2 → 0; width 22b → 8b; description changed
- **`sem_bank_sel`** (NEW) [12:8] 5b: The semaphores are divided into groups of 8; this selects the group. For example, if 0, this selects semaphores 0..7

### `SEMGET`

```
Blackhole:
┌──────────────────────────────────────────────────────────────────┬──────────┐
│                             sem_sel                              │   ---    │
│                            22b [23:2]                            │ 2b [1:0] │
└──────────────────────────────────────────────────────────────────┴──────────┘

Quasar:
┌─────────────────────────────────┬───────────────┬────────────────────────┐
│               ---               │  sem_bank_sel │        sem_sel         │
│           11b [23:13]           │   5b [12:8]   │        8b [7:0]        │
└─────────────────────────────────┴───────────────┴────────────────────────┘
```

- **BH unused gap** [1:0] (2b), now used by `sem_sel`
- **Added args**: `sem_bank_sel`
- **`sem_sel`**: start_bit 2 → 0; width 22b → 8b; description changed
- **`sem_bank_sel`** (NEW) [12:8] 5b: The semaphores are divided into groups of 8; this selects the group. For example, if 0, this selects semaphores 0..7

### `SEMWAIT`

```
Blackhole:
┌───────────────────────────┬───────────────────────────────────────┬───────────────┐
│         stall_res         │                sem_sel                │ wait_sem_cond │
│         9b [23:15]        │               13b [14:2]              │    2b [1:0]   │
└───────────────────────────┴───────────────────────────────────────┴───────────────┘

Quasar:
┌───────────────────────────┬───────────────┬───────────────┬────────────────────────┐
│         stall_res         │ wait_sem_cond │  sem_bank_sel │        sem_sel         │
│         9b [23:15]        │   2b [14:13]  │   5b [12:8]   │        8b [7:0]        │
└───────────────────────────┴───────────────┴───────────────┴────────────────────────┘
```

- **Field reorder**: BH `wait_sem_cond→sem_sel→stall_res` vs QS `sem_sel→wait_sem_cond→stall_res`
- **Added args**: `sem_bank_sel`
- **`sem_sel`**: start_bit 2 → 0; width 13b → 8b; description changed
- **`wait_sem_cond`**: start_bit 0 → 13
- **`stall_res`**: description changed
- **`sem_bank_sel`** (NEW) [12:8] 5b: The semaphores are divided into groups of 8; this selects the group. For example, if 0, this selects semaphores 0..7

---

## Atomic Memory Operations

### `ATINCGET`

```
Blackhole:
┌────────────┬───────────────────────────┬────────────┬──────────────────┬──────────────────┐
│ MemHierSel │          WrapVal          │   Sel32b   │   DataRegIndex   │   AddrRegIndex   │
│  1b [23]   │         9b [22:14]        │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴───────────────────────────┴────────────┴──────────────────┴──────────────────┘

Quasar:
┌─────────┬───────────────────────────┬────────────┬──────────────────┬──────────────────┐
│   ---   │          WrapVal          │   Sel32b   │  Data_GPR_Index  │  Addr_GPR_Index  │
│ 1b [23] │         9b [22:14]        │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└─────────┴───────────────────────────┴────────────┴──────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`, `MemHierSel`
- **Added args**: `Addr_GPR_Index`, `Data_GPR_Index`
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Address register index (16B word address)
- **`Data_GPR_Index`** (NEW) [11:6] 6b: Destination data register index; bits[11:8] are GPR address (16B granularity) bits[7:6] are used to select the 32-bits o

### `ATINCGETPTR`

```
Blackhole:
┌────────────┬─────────┬────────────┬────────────┬────────────┬──────────────────┬──────────────────┐
│ MemHierSel │  NoIncr │  IncrVal   │  WrapVal   │   Sel32b   │   DataRegIndex   │   AddrRegIndex   │
│  1b [23]   │ 1b [22] │ 4b [21:18] │ 4b [17:14] │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴─────────┴────────────┴────────────┴────────────┴──────────────────┴──────────────────┘

Quasar:
┌─────────┬─────────┬────────────┬────────────┬────────────┬──────────────────┬──────────────────┐
│   ---   │  NoIncr │  IncrVal   │  WrapVal   │   Sel32b   │  Data_GPR_Index  │  Addr_GPR_Index  │
│ 1b [23] │ 1b [22] │ 4b [21:18] │ 4b [17:14] │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└─────────┴─────────┴────────────┴────────────┴────────────┴──────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`, `MemHierSel`
- **Added args**: `Addr_GPR_Index`, `Data_GPR_Index`
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Address register index (16B word address)
- **`Data_GPR_Index`** (NEW) [11:6] 6b: Destination data register index

### `ATSWAP`

```
Blackhole:
┌────────────┬───────────────────────────┬────────────────────────┬──────────────────┐
│ MemHierSel │          SwapMask         │      DataRegIndex      │   AddrRegIndex   │
│  1b [23]   │         9b [22:14]        │       8b [13:6]        │     6b [5:0]     │
└────────────┴───────────────────────────┴────────────────────────┴──────────────────┘

Quasar:
┌─────────┬───────────────────────────┬────────────────────────┬──────────────────┐
│   ---   │          SwapMask         │     Data_GPR_Index     │  Addr_GPR_Index  │
│ 1b [23] │         9b [22:14]        │       8b [13:6]        │     6b [5:0]     │
└─────────┴───────────────────────────┴────────────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`, `MemHierSel`
- **Added args**: `Addr_GPR_Index`, `Data_GPR_Index`
- **`SwapMask`**: description changed
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Address register index (16B word address)
- **`Data_GPR_Index`** (NEW) [13:6] 8b: First of four data register indexes. Must be aligned to 4.

### `ATCAS`

```
Blackhole:
┌────────────┬───────────────┬────────────┬────────────┬──────────────────┬──────────────────┐
│ MemHierSel │    SwapVal    │   CmpVal   │   Sel32b   │   DataRegIndex   │   AddrRegIndex   │
│  1b [23]   │   5b [22:18]  │ 4b [17:14] │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴───────────────┴────────────┴────────────┴──────────────────┴──────────────────┘

Quasar:
┌────────────┬────────────┬────────────┬────────────┬──────────────────┬──────────────────┐
│    ---     │  SwapVal   │   CmpVal   │   Sel32b   │       ---        │  Addr_GPR_Index  │
│ 2b [23:22] │ 4b [21:18] │ 4b [17:14] │ 2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴────────────┴────────────┴────────────┴──────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`, `MemHierSel`
- **Added args**: `Addr_GPR_Index`
- **`SwapVal`**: width 5b → 4b
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Address register index (16B word address)

### `ATGETM`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                              mutex_index                               │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────────────────────────────┐
│          ---           │                  mutex_index                   │
│       8b [23:16]       │                   16b [15:0]                   │
└────────────────────────┴────────────────────────────────────────────────┘
```

- **`mutex_index`**: width 24b → 16b; description changed

### `ATRELM`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                              mutex_index                               │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────────────────────────────┐
│          ---           │                  mutex_index                   │
│       8b [23:16]       │                   16b [15:0]                   │
└────────────────────────┴────────────────────────────────────────────────┘
```

- **`mutex_index`**: width 24b → 16b; description changed

---

## Register File Move (Src/Dst)

### `MOVA2D`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬────────────┬────────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │                dst                 │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │             12b [11:0]             │
└─────────────┴──────────────────┴────────────┴────────────┴────────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬────────────┬─────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │   ---   │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │ 1b [11] │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴────────────┴─────────┴─────────────────────────────────┘
```

- **`dst`**: width 12b → 11b
- **`instr_mod`**: description changed
- **`src`**: description changed

### `MOVB2D`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬──────────────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ movb2d_instr_mod │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │    3b [13:11]    │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴──────────────────┴─────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬─────────────┬──────────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ transfer_sz │ bcast_datum0 │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │  2b [13:12] │   1b [11]    │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴─────────────┴──────────────┴─────────────────────────────────┘
```

- **Removed args**: `movb2d_instr_mod`
- **Added args**: `bcast_datum0`, `transfer_sz`
- **`src`**: description changed
- **`bcast_datum0`** (NEW) [11] 1b: If 1, the datum from column 0 will be broadcast to all 16 columns.
- **`transfer_sz`** (NEW) [13:12] 2b: Number of rows to write into dest. All addresses must be aligned to a multiple of the transfer size. There is one except

### `MOVD2A`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬────────────┬────────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │                dst                 │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │             12b [11:0]             │
└─────────────┴──────────────────┴────────────┴────────────┴────────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬────────────┬─────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │   ---   │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │ 1b [11] │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴────────────┴─────────┴─────────────────────────────────┘
```

- **`dst`**: width 12b → 11b
- **`instr_mod`**: description changed

### `MOVD2B`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬────────────┬────────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │                dst                 │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │             12b [11:0]             │
└─────────────┴──────────────────┴────────────┴────────────┴────────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬────────────┬───────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │ transpose │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │  1b [11]  │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴────────────┴───────────┴─────────────────────────────────┘
```

- **Added args**: `transpose`
- **`dst`**: width 12b → 11b
- **`instr_mod`**: description changed
- **`transpose`** (NEW) [11] 1b: Indicates the data should be transposed when writing into srcB (`TRANSPSRCB` instruction got removed in Quasar, and replaced with this bit)

### `MOVDBGA2D`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬────────────┬────────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │                dst                 │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │             12b [11:0]             │
└─────────────┴──────────────────┴────────────┴────────────┴────────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬────────────┬─────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ instr_mod  │   ---   │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │ 2b [13:12] │ 1b [11] │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴────────────┴─────────┴─────────────────────────────────┘
```

- **`dst`**: width 12b → 11b
- **`instr_mod`**: description changed
- **`src`**: description changed

### `MOVDBGB2D`

```
Blackhole:
┌─────────────┬──────────────────┬────────────┬──────────────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ movb2d_instr_mod │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │    3b [13:11]    │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴──────────────────┴─────────────────────────────────┘

Quasar:
┌─────────────┬──────────────────┬────────────┬─────────────┬──────────────┬─────────────────────────────────┐
│ dest_32b_lo │       src        │ addr_mode  │ transfer_sz │ bcast_datum0 │               dst               │
│   1b [23]   │    6b [22:17]    │ 3b [16:14] │  2b [13:12] │   1b [11]    │            11b [10:0]           │
└─────────────┴──────────────────┴────────────┴─────────────┴──────────────┴─────────────────────────────────┘
```

- **Removed args**: `movb2d_instr_mod`
- **Added args**: `bcast_datum0`, `transfer_sz`
- **`src`**: description changed
- **`bcast_datum0`** (NEW) [11] 1b: If 1, the datum from column 0 will be broadcast to all 16 columns.
- **`transfer_sz`** (NEW) [13:12] 2b: Number of rows to write into dest. All addresses must be aligned to a multiple of the transfer size. There is one except

---

## Compute — Dest / Src Control

### `ZEROACC`

- **`ex_resource`**: `MATH` → `INSTISSUE`

```
Blackhole:
┌───────────────┬─────────────────┬──────────────────┬────────────┬──────────────────────────────────────────┐
│   clear_mode  │ use_32_bit_mode │ clear_zero_flags │ addr_mode  │                  where                   │
│   5b [23:19]  │     1b [18]     │     1b [17]      │ 3b [16:14] │                14b [13:0]                │
└───────────────┴─────────────────┴──────────────────┴────────────┴──────────────────────────────────────────┘

Quasar:
┌───────────────┬─────────────────┬──────────────────┬────────────┬──────────────────────────────────────────┐
│   clear_mode  │ use_32_bit_mode │ clear_zero_flags │ addr_mode  │                  where                   │
│   5b [23:19]  │     1b [18]     │     1b [17]      │ 3b [16:14] │                14b [13:0]                │
└───────────────┴─────────────────┴──────────────────┴────────────┴──────────────────────────────────────────┘
```


### `ZEROSRC`

- **`ex_resource`**: `MATH` → `INSTISSUE`

```
Blackhole:
┌────────────────────────────────────────────────────────────┬────────────┬───────────┬──────────┐
│                          zero_val                          │ write_mode │ bank_mask │ src_mask │
│                         20b [23:4]                         │   1b [3]   │   1b [2]  │ 2b [1:0] │
└────────────────────────────────────────────────────────────┴────────────┴───────────┴──────────┘

Quasar:
┌────────────────────────────────────────────────┬────────────┬─────────┬──────────┬──────────┬────────────┬───────────┬──────────┐
│                      ---                       │ packed_fmt │ int_fmt │ exp_bias │ zero_val │ write_mode │ bank_mask │ src_mask │
│                   16b [23:8]                   │   1b [7]   │  1b [6] │  1b [5]  │  1b [4]  │   1b [3]   │   1b [2]  │ 2b [1:0] │
└────────────────────────────────────────────────┴────────────┴─────────┴──────────┴──────────┴────────────┴───────────┴──────────┘
```

- **Added args**: `exp_bias`, `int_fmt`, `packed_fmt`
- **`zero_val`**: width 20b → 1b
- **`exp_bias`** (NEW) [5] 1b: 0 - 5-bit exponent, 1 - 8-bit exponent  (Used only when zero_val is -inf)
- **`int_fmt`** (NEW) [6] 1b: 0 - floating point format, 1 - integer format (Used only when zero_val is -inf)
- **`packed_fmt`** (NEW) [7] 1b: 0 - unpacked format, 1 - packed MXFP4 or INT8 format (Used only when zero_val is -inf)

### `SETDVALID`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                                setvalid                                │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌─────────────────────────────────────────────────────────┬───────────────┐
│                           ---                           │    setvalid   │
│                        19b [23:5]                       │    5b [4:0]   │
└─────────────────────────────────────────────────────────┴───────────────┘
```

- **`setvalid`**: width 24b → 5b; description changed

### `CLEARDVALID`

- **`ex_resource`**: `MATH` → `INSTISSUE`

```
Blackhole:
┌─────────────┬──────────────────────────────────────────────────────────────────┐
│ cleardvalid │                              reset                               │
│  2b [23:22] │                            22b [21:0]                            │
└─────────────┴──────────────────────────────────────────────────────────────────┘

Quasar:
┌─────────────┬───────────────┬──────────────────┬───────────────────┬───────────────────────────────┬─────────────────┬──────────┐
│ cleardvalid │ cleardvalid_S │       ---        │ dest_dvalid_reset │ dest_dvalid_client_bank_reset │ dest_pulse_last │  reset   │
│  2b [23:22] │   2b [21:20]  │    6b [19:14]    │     4b [13:10]    │            4b [9:6]           │     4b [5:2]    │ 2b [1:0] │
└─────────────┴───────────────┴──────────────────┴───────────────────┴───────────────────────────────┴─────────────────┴──────────┘
```

- **Added args**: `dest_pulse_last`, `dest_dvalid_client_bank_reset`, `dest_dvalid_reset`, `cleardvalid_S`
- **`reset`**: width 22b → 2b
- **`dest_pulse_last`** (NEW) [5:2] 4b: Dest has four clients managed by hardware synchronization: unpack-to-dest, math, sfpu, and packer. This field has four b
- **`dest_dvalid_client_bank_reset`** (NEW) [9:6] 4b: Reset dest bank ID to 0 for given client bitmask (bit 0 = unpack-to-dest, bit 1 = math, bit 2 = sfpu, bit 3 = packer)
- **`dest_dvalid_reset`** (NEW) [13:10] 4b: Reset dest dvalid bit to 0 for given bitmask. This does not correspond to the clients, it corresponds to the four dvalid
- **`cleardvalid_S`** (NEW) [21:20] 2b: Clear srcS data valid (2 bits). Bit 20 will clear the unpacker data_valid for srcS and bit 21 will clear the packer data

---

## RWC Counters

### `SETRWC`

- **`ex_resource`**: `MATH` → `INSTISSUE`

```
Blackhole:
┌──────────────┬────────────┬────────────┬────────────┬────────────┬──────────────────┐
│ clear_ab_vld │   rwc_cr   │   rwc_d    │   rwc_b    │   rwc_a    │     BitMask      │
│  2b [23:22]  │ 4b [21:18] │ 4b [17:14] │ 4b [13:10] │  4b [9:6]  │     6b [5:0]     │
└──────────────┴────────────┴────────────┴────────────┴────────────┴──────────────────┘

Quasar:
┌──────────────┬────────────┬────────────────────────────────────┬──────────────────┐
│ clear_ab_vld │   rwc_cr   │              rwc_val               │     BitMask      │
│  2b [23:22]  │ 4b [21:18] │             12b [17:6]             │     6b [5:0]     │
└──────────────┴────────────┴────────────────────────────────────┴──────────────────┘
```

- **Removed args**: `rwc_a`, `rwc_b`, `rwc_d`
- **Added args**: `rwc_val`
- **`rwc_cr`**: description changed
- **`rwc_val`** (NEW) [17:6] 12b: Value to set for ALL counters named in the BitMask

### `INCRWC`

- **`ex_resource`**: `MATH` → `INSTISSUE`

```
Blackhole:
┌──────────────────┬────────────┬────────────┬────────────┬──────────────────┐
│      rwc_cr      │   rwc_d    │   rwc_b    │   rwc_a    │       ---        │
│    6b [23:18]    │ 4b [17:14] │ 4b [13:10] │  4b [9:6]  │     6b [5:0]     │
└──────────────────┴────────────┴────────────┴────────────┴──────────────────┘

Quasar:
┌──────────────────┬───────────────┬───────────────┬────────────────────────┐
│      rwc_cr      │     rwc_a     │     rwc_b     │         rwc_d          │
│    6b [23:18]    │   5b [17:13]  │   5b [12:8]   │        8b [7:0]        │
└──────────────────┴───────────────┴───────────────┴────────────────────────┘
```

- **Field reorder**: BH `rwc_a→rwc_b→rwc_d→rwc_cr` vs QS `rwc_d→rwc_b→rwc_a→rwc_cr`
- **BH unused gap** [5:0] (6b), now used by `rwc_d`
- **`rwc_d`**: start_bit 14 → 0; width 4b → 8b; description changed
- **`rwc_b`**: start_bit 10 → 8; width 4b → 5b; description changed
- **`rwc_a`**: start_bit 6 → 13; width 4b → 5b; description changed

---

## Pack / Unpack

### `UNPACR_NOP`

```
Blackhole:
┌─────────────────┬─────────────────────┬─────────────┬────────────┬──────────────────┬─────────────────┬───────────────┬─────────────────┬────────────┐
│ Unpacker_Select │      Stream_Id      │ Msg_Clr_Cnt │ Set_Dvalid │ Clr_to1_fmt_Ctrl │ Stall_Clr_Cntrl │ Bank_Clr_Ctrl │ Src_ClrVal_Ctrl │ Unpack_Pop │
│     1b [23]     │      7b [22:16]     │  4b [15:12] │ 4b [11:8]  │     2b [7:6]     │      1b [5]     │     1b [4]    │     2b [3:2]    │  2b [1:0]  │
└─────────────────┴─────────────────────┴─────────────┴────────────┴──────────────────┴─────────────────┴───────────────┴─────────────────┴────────────┘

Quasar:
┌──────────────────────────────────────────┬─────────────────┬────────────┬────────┬─────────────┬───────────────┬─────────────────┬──────────┐
│                   ---                    │ Unpacker_Select │ Set_Dvalid │  ---   │ Stall_Cntrl │ Bank_Clr_Ctrl │ Src_ClrVal_Ctrl │ Nop_type │
│               14b [23:10]                │     2b [9:8]    │   1b [7]   │ 1b [6] │    1b [5]   │     1b [4]    │     2b [3:2]    │ 2b [1:0] │
└──────────────────────────────────────────┴─────────────────┴────────────┴────────┴─────────────┴───────────────┴─────────────────┴──────────┘
```

- **Removed args**: `Unpack_Pop`, `Stall_Clr_Cntrl`, `Clr_to1_fmt_Ctrl`, `Msg_Clr_Cnt`, `Stream_Id`
- **Added args**: `Nop_type`, `Stall_Cntrl`
- **`Src_ClrVal_Ctrl`**: description changed
- **`Bank_Clr_Ctrl`**: description changed
- **`Set_Dvalid`**: start_bit 8 → 7; width 4b → 1b; description changed
- **`Unpacker_Select`**: start_bit 23 → 8; width 1b → 2b; description changed
- **`Nop_type`** (NEW) [1:0] 2b: 0 - UNP_CLR_SRC (clear srcA or srcB when unpack is done) 1 - UNP_NOP (inject cycle delay between back to back unpack) 2
- **`Stall_Cntrl`** (NEW) [5] 1b: Valid only for UNP_CLR_SRC and UNP_NOP_SETDAVLID   0 - stall until Data-Valid of the bank FPU is reading from goes low

---

## Memory Indirect / Register

### `LOADIND`

```
Blackhole:
┌────────────┬────────────────────────┬─────────────┬──────────────────┬──────────────────┐
│  SizeSel   │      OffsetIndex       │ AutoIncSpec │   DataRegIndex   │   AddrRegIndex   │
│ 2b [23:22] │       8b [21:14]       │  2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴────────────────────────┴─────────────┴──────────────────┴──────────────────┘

Quasar:
┌────────────┬────────────────────────┬─────────────┬──────────────────┬──────────────────┐
│  SizeSel   │      OffsetIndex       │ AutoIncSpec │  Data_GPR_Index  │  Addr_GPR_Index  │
│ 2b [23:22] │       8b [21:14]       │  2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴────────────────────────┴─────────────┴──────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`
- **Added args**: `Addr_GPR_Index`, `Data_GPR_Index`
- **`AutoIncSpec`**: description changed
- **`OffsetIndex`**: description changed
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Index of the GPR containing the L1 "base" address. The final L1 address accessed by LOADIND will be this "base" address
- **`Data_GPR_Index`** (NEW) [11:6] 6b: Index (32-bit granular) of GPR in which to place the L1 read data

### `STOREIND`

```
Blackhole:
┌────────────┬─────────┬────────────┬─────────────────────┬─────────────┬──────────────────┬──────────────────┐
│ MemHierSel │ SizeSel │ RegSizeSel │     OffsetIndex     │ AutoIncSpec │   DataRegIndex   │   AddrRegIndex   │
│  1b [23]   │ 1b [22] │  1b [21]   │      7b [20:14]     │  2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴─────────┴────────────┴─────────────────────┴─────────────┴──────────────────┴──────────────────┘

Quasar:
┌────────────┬─────────┬─────────────────────┬─────────────┬──────────────────┬──────────────────┐
│  SizeSel   │  MemSel │     OffsetIndex     │ AutoIncSpec │  Data_GPR_Index  │  Addr_GPR_Index  │
│ 2b [23:22] │ 1b [21] │      7b [20:14]     │  2b [13:12] │    6b [11:6]     │     6b [5:0]     │
└────────────┴─────────┴─────────────────────┴─────────────┴──────────────────┴──────────────────┘
```

- **Removed args**: `AddrRegIndex`, `DataRegIndex`, `RegSizeSel`, `MemHierSel`
- **Added args**: `Addr_GPR_Index`, `Data_GPR_Index`, `MemSel`
- **`OffsetIndex`**: description changed
- **`SizeSel`**: width 1b → 2b; description changed
- **`Addr_GPR_Index`** (NEW) [5:0] 6b: Index of GPR which contains the destination address
- **`Data_GPR_Index`** (NEW) [11:6] 6b: Index of GPR which contains the source data address
- **`MemSel`** (NEW) [21] 1b: 1'b0 = Write to MMIO space 1'b1 = Write to L1

### `LOADREG`

```
Blackhole:
┌──────────────────┬──────────────────────────────────────────────────────┐
│ TdmaDataRegIndex │                       RegAddr                        │
│    6b [23:18]    │                      18b [17:0]                      │
└──────────────────┴──────────────────────────────────────────────────────┘

Quasar:
┌──────────────────┬──────────────────────────────────────────────────────┐
│  Data_GPR_Index  │                       RegAddr                        │
│    6b [23:18]    │                      18b [17:0]                      │
└──────────────────┴──────────────────────────────────────────────────────┘
```

- **Removed args**: `TdmaDataRegIndex`
- **Added args**: `Data_GPR_Index`
- **`RegAddr`**: description changed
- **`Data_GPR_Index`** (NEW) [23:18] 6b: Dest data register index

### `STOREREG`

```
Blackhole:
┌──────────────────┬──────────────────────────────────────────────────────┐
│ TdmaDataRegIndex │                       RegAddr                        │
│    6b [23:18]    │                      18b [17:0]                      │
└──────────────────┴──────────────────────────────────────────────────────┘

Quasar:
┌──────────────────┬──────────────────────────────────────────────────────┐
│  Data_GPR_Index  │                       RegAddr                        │
│    6b [23:18]    │                      18b [17:0]                      │
└──────────────────┴──────────────────────────────────────────────────────┘
```

- **Removed args**: `TdmaDataRegIndex`
- **Added args**: `Data_GPR_Index`
- **`Data_GPR_Index`** (NEW) [23:18] 6b: Source data register index

---

## CFG Instructions

### `RMWCIB0`

- **`op_binary`**: 179 (0xB3) → 178 (0xB2)

```
Blackhole:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│          Mask          │          Data          │       CfgRegAddr       │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

- **Field reorder**: BH `CfgRegAddr→Data→Mask` vs QS `Data→Mask→CfgRegAddr`
- **`Data`**: start_bit 8 → 0
- **`Mask`**: start_bit 16 → 8
- **`CfgRegAddr`**: start_bit 0 → 16

### `RMWCIB1`

```
Blackhole:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│          Mask          │          Data          │       CfgRegAddr       │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

- **Field reorder**: BH `CfgRegAddr→Data→Mask` vs QS `Data→Mask→CfgRegAddr`
- **`Data`**: start_bit 8 → 0
- **`Mask`**: start_bit 16 → 8
- **`CfgRegAddr`**: start_bit 0 → 16

### `RMWCIB2`

- **`op_binary`**: 181 (0xB5) → 182 (0xB6)

```
Blackhole:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│          Mask          │          Data          │       CfgRegAddr       │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

- **Field reorder**: BH `CfgRegAddr→Data→Mask` vs QS `Data→Mask→CfgRegAddr`
- **`Data`**: start_bit 8 → 0
- **`Mask`**: start_bit 16 → 8
- **`CfgRegAddr`**: start_bit 0 → 16

### `RMWCIB3`

- **`op_binary`**: 182 (0xB6) → 184 (0xB8)

```
Blackhole:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│          Mask          │          Data          │       CfgRegAddr       │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘

Quasar:
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

- **Field reorder**: BH `CfgRegAddr→Data→Mask` vs QS `Data→Mask→CfgRegAddr`
- **`Data`**: start_bit 8 → 0
- **`Mask`**: start_bit 16 → 8
- **`CfgRegAddr`**: start_bit 0 → 16

### `CFGSHIFTMASK`

- **`op_binary`**: 184 (0xB8) → 186 (0xBA)

```
Blackhole:
┌─────────────────────────┬────────────┬───────────────┬──────────────────┬─────────────┬────────────────────────┐
│ disable_mask_on_old_val │ operation  │   mask_width  │ right_cshift_amt │ scratch_sel │         CfgReg         │
│         1b [23]         │ 3b [22:20] │   5b [19:15]  │    5b [14:10]    │   2b [9:8]  │        8b [7:0]        │
└─────────────────────────┴────────────┴───────────────┴──────────────────┴─────────────┴────────────────────────┘

Quasar:
┌────────────────────────┬─────────────────────────┬────────────┬───────────────┬──────────────────┬─────────────┐
│       CfgRegAddr       │ disable_mask_on_old_val │ operation  │   mask_width  │ right_cshift_amt │ scratch_sel │
│       8b [23:16]       │         1b [15]         │ 3b [14:12] │   5b [11:7]   │     5b [6:2]     │   2b [1:0]  │
└────────────────────────┴─────────────────────────┴────────────┴───────────────┴──────────────────┴─────────────┘
```

- **Removed args**: `CfgReg`
- **Added args**: `CfgRegAddr`
- **`scratch_sel`**: start_bit 8 → 0
- **`right_cshift_amt`**: start_bit 10 → 2
- **`mask_width`**: start_bit 15 → 7
- **`operation`**: start_bit 20 → 12; description changed
- **`disable_mask_on_old_val`**: start_bit 23 → 15
- **`CfgRegAddr`** (NEW) [23:16] 8b: configuration register address to write data to (32-bit aligned)

---

## Control Flow / Meta

### `MOP`

```
Blackhole:
┌──────────┬─────────────────────┬────────────────────────────────────────────────┐
│ mop_type │      loop_count     │            zmask_lo16_or_loop_count            │
│ 1b [23]  │      7b [22:16]     │                   16b [15:0]                   │
└──────────┴─────────────────────┴────────────────────────────────────────────────┘

Quasar:
┌──────────┬─────────┬─────────────────────┬─────────────────────────────────────────────┐
│ mop_type │   done  │      loop_count     │           zmask_lo8_or_loop_count           │
│ 1b [23]  │ 1b [22] │      7b [21:15]     │                  15b [14:0]                 │
└──────────┴─────────┴─────────────────────┴─────────────────────────────────────────────┘
```

- **Removed args**: `zmask_lo16_or_loop_count`
- **Added args**: `zmask_lo8_or_loop_count`, `done`
- **`loop_count`**: start_bit 16 → 15
- **`zmask_lo8_or_loop_count`** (NEW) [14:0] 15b: Low 8-bit zmask for unpacker loop;  9-bit Inner loop count for math/packer if non-zero value. Also lower 6-bits (bits[5:
- **`done`** (NEW) [22] 1b: Indicates this is the last MOP opcode for the current mop_config bank and causes a bank switch when the MOP_CONFIG.sw_co

### `MOP_CFG`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                               zmask_hi16                               │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌────────────────────────────────────────────────────────────────────────┐
│                               zmask_hi24                               │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘
```

- **Removed args**: `zmask_hi16`
- **Added args**: `zmask_hi24`
- **`zmask_hi24`** (NEW) [23:0] 24b: High 24-bit zmask for unpacker loop

### `REPLAY`

```
Blackhole:
┌──────────────────────────────┬──────────────────────────────┬───────────────────────┬───────────┐
│          start_idx           │             len              │ execute_while_loading │ load_mode │
│         10b [23:14]          │          10b [13:4]          │        3b [3:1]       │   1b [0]  │
└──────────────────────────────┴──────────────────────────────┴───────────────────────┴───────────┘

Quasar:
┌──────────────────────────────┬──────────────────────────────┬────────┬───────────┬───────────────────────┬───────────┐
│          start_idx           │             len              │  last  │ set_mutex │ execute_while_loading │ load_mode │
│         10b [23:14]          │          10b [13:4]          │ 1b [3] │   1b [2]  │         1b [1]        │   1b [0]  │
└──────────────────────────────┴──────────────────────────────┴────────┴───────────┴───────────────────────┴───────────┘
```

- **Added args**: `set_mutex`, `last`
- **`execute_while_loading`**: width 3b → 1b
- **`set_mutex`** (NEW) [2] 1b: Set mutex for current replay bank to 1. This prevents future replay instructions from accessing that bank until SW relea
- **`last`** (NEW) [3] 1b: The replay unit has double-banking support, meaning it is possible for you to be loading one bank while the other bank i

### `RESOURCEDECL`

```
Blackhole:
┌─────────────────────────────────┬───────────────────────────┬────────────┐
│           linger_time           │         resources         │  op_class  │
│           11b [23:13]           │         9b [12:4]         │  4b [3:0]  │
└─────────────────────────────────┴───────────────────────────┴────────────┘

Quasar:
┌───────────────┬─────────────┬──────────────────────────────┬───────────────┐
│      ---      │ linger_time │          resources           │    op_class   │
│   5b [23:19]  │  4b [18:15] │          10b [14:5]          │    5b [4:0]   │
└───────────────┴─────────────┴──────────────────────────────┴───────────────┘
```

- **`op_class`**: width 4b → 5b; description changed
- **`resources`**: start_bit 4 → 5; width 9b → 10b; description changed
- **`linger_time`**: start_bit 13 → 15; width 11b → 4b

### `DMANOP`

- **`ex_resource`**: `TDMA` → `THCON`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                                  ---                                   │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌────────────────────────────────────────────────────────────────────────┐
│                                  ---                                   │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘
```


---

## SFPU — Load/Store/Misc

### `SFPLOAD`

```
Blackhole:
┌────────────┬────────────┬────────────────┬───────────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │ sfpu_addr_mode │             dest_reg_addr             │
│ 4b [23:20] │ 4b [19:16] │   3b [15:13]   │               13b [12:0]              │
└────────────┴────────────┴────────────────┴───────────────────────────────────────┘

Quasar:
┌────────────┬────────────┬────────────────┬─────────┬─────────┬─────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │ sfpu_addr_mode │   ---   │   done  │          dest_reg_addr          │
│ 4b [23:20] │ 4b [19:16] │   3b [15:13]   │ 1b [12] │ 1b [11] │            11b [10:0]           │
└────────────┴────────────┴────────────────┴─────────┴─────────┴─────────────────────────────────┘
```

- **Added args**: `done`
- **`dest_reg_addr`**: width 13b → 11b; description changed
- **`sfpu_addr_mode`**: description changed
- **`instr_mod0`**: description changed
- **`done`** (NEW) [11] 1b: If this bit is set and this instruction targets SrcS, the SrcS read bank ID and dvalid will toggle when this instruction

### `SFPSTORE`

```
Blackhole:
┌────────────┬────────────┬────────────────┬───────────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │ sfpu_addr_mode │             dest_reg_addr             │
│ 4b [23:20] │ 4b [19:16] │   3b [15:13]   │               13b [12:0]              │
└────────────┴────────────┴────────────────┴───────────────────────────────────────┘

Quasar:
┌────────────┬────────────┬────────────────┬─────────┬─────────┬─────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │ sfpu_addr_mode │   ---   │   done  │          dest_reg_addr          │
│ 4b [23:20] │ 4b [19:16] │   3b [15:13]   │ 1b [12] │ 1b [11] │            11b [10:0]           │
└────────────┴────────────┴────────────────┴─────────┴─────────┴─────────────────────────────────┘
```

- **Added args**: `done`
- **`dest_reg_addr`**: width 13b → 11b; description changed
- **`sfpu_addr_mode`**: description changed
- **`instr_mod0`**: description changed
- **`done`** (NEW) [11] 1b: If this bit is set and this instruction targets SrcS, the SrcS write bank ID and dvalid will toggle when this instructio

### `SFPLOADMACRO`

```
Blackhole:
┌────────────┬────────────┬────────────────┬───────────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │ sfpu_addr_mode │             dest_reg_addr             │
│ 4b [23:20] │ 4b [19:16] │   3b [15:13]   │               13b [12:0]              │
└────────────┴────────────┴────────────────┴───────────────────────────────────────┘

Quasar:
┌────────────┬─────────────┬────────────┬────────────────┬─────────┬─────────┬──────────────────────────────┬─────────────┐
│   seq_id   │ lreg_ind_lo │ instr_mod0 │ sfpu_addr_mode │   ---   │   done  │        dest_reg_addr         │ lreg_ind_hi │
│ 2b [23:22] │  2b [21:20] │ 4b [19:16] │   3b [15:13]   │ 1b [12] │ 1b [11] │          10b [10:1]          │    1b [0]   │
└────────────┴─────────────┴────────────┴────────────────┴─────────┴─────────┴──────────────────────────────┴─────────────┘
```

- **Removed args**: `lreg_ind`
- **Added args**: `lreg_ind_hi`, `done`, `lreg_ind_lo`, `seq_id`
- **`dest_reg_addr`**: start_bit 0 → 1; width 13b → 10b; description changed
- **`sfpu_addr_mode`**: description changed
- **`instr_mod0`**: description changed
- **`lreg_ind_hi`** (NEW) [0] 1b: lreg index bit 2 (bit 3 tied to 0)
- **`done`** (NEW) [11] 1b: If this bit is set and this instruction targets SrcS, the SrcS read bank ID and dvalid will toggle when this instruction
- **`lreg_ind_lo`** (NEW) [21:20] 2b: lreg index bits 1:0
- **`seq_id`** (NEW) [23:22] 2b: LOADMACRO sequence ID to run

### `SFPNOP`

```
Blackhole:
┌────────────────────────────────────────────────────────────────────────┐
│                                  ---                                   │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘

Quasar:
┌───────────────────────────────────────────────────────────────┬──────────────┬──────────────┬───────────┐
│                              ---                              │ srcs_wr_done │ srcs_rd_done │ dest_done │
│                           21b [23:3]                          │    1b [2]    │    1b [1]    │   1b [0]  │
└───────────────────────────────────────────────────────────────┴──────────────┴──────────────┴───────────┘
```

- **Added args**: `dest_done`, `srcs_rd_done`, `srcs_wr_done`
- **`dest_done`** (NEW) [0] 1b: indicates that SFPU operations for the current dest bank are done and the bank can be reused by the unpacker
- **`srcs_rd_done`** (NEW) [1] 1b: indicates that SFPU load operations for the current srcs bank are done and the bank can be reused by the unpacker
- **`srcs_wr_done`** (NEW) [2] 1b: indicates that SFPU store operations for the current srcs bank are done and the bank can be read by the packer

---

## Other

### `ELWADD`

```
Blackhole:
┌──────────────┬───────────────┬─────────────┬───────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │   addr_mode   │                   dst                    │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │   5b [18:14]  │                14b [13:0]                │
└──────────────┴───────────────┴─────────────┴───────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬───────────────┬─────────────┬────────────┬────────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │    ---     │ addr_mode  │    ---     │               dst               │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │ 2b [18:17] │ 3b [16:14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴───────────────┴─────────────┴────────────┴────────────┴────────────┴─────────────────────────────────┘
```

- **`dst`**: width 14b → 11b
- **`addr_mode`**: width 5b → 3b
- **`instr_mod19`**: description changed

### `ELWMUL`

```
Blackhole:
┌──────────────┬───────────────┬─────────────┬───────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │   addr_mode   │                   dst                    │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │   5b [18:14]  │                14b [13:0]                │
└──────────────┴───────────────┴─────────────┴───────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬───────────────┬─────────────┬────────────┬────────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │    ---     │ addr_mode  │    ---     │               dst               │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │ 2b [18:17] │ 3b [16:14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴───────────────┴─────────────┴────────────┴────────────┴────────────┴─────────────────────────────────┘
```

- **`dst`**: width 14b → 11b
- **`addr_mode`**: width 5b → 3b
- **`instr_mod19`**: description changed

### `ELWSUB`

```
Blackhole:
┌──────────────┬───────────────┬─────────────┬───────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │   addr_mode   │                   dst                    │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │   5b [18:14]  │                14b [13:0]                │
└──────────────┴───────────────┴─────────────┴───────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬───────────────┬─────────────┬────────────┬────────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ dest_accum_en │ instr_mod19 │    ---     │ addr_mode  │    ---     │               dst               │
│  2b [23:22]  │    1b [21]    │  2b [20:19] │ 2b [18:17] │ 3b [16:14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴───────────────┴─────────────┴────────────┴────────────┴────────────┴─────────────────────────────────┘
```

- **`dst`**: width 14b → 11b
- **`addr_mode`**: width 5b → 3b
- **`instr_mod19`**: description changed

### `GAPOOL`

```
Blackhole:
┌──────────────┬─────────────┬────────────────┬───────────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │ pool_addr_mode │ max_pool_index_en │                   dst                    │
│  2b [23:22]  │  3b [21:19] │   4b [18:15]   │      1b [14]      │                14b [13:0]                │
└──────────────┴─────────────┴────────────────┴───────────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬─────────────┬─────────┬────────────────┬─────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │   ---   │ pool_addr_mode │   rsvd  │    ---     │               dst               │
│  2b [23:22]  │  3b [21:19] │ 1b [18] │   3b [17:15]   │ 1b [14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴─────────────┴─────────┴────────────────┴─────────┴────────────┴─────────────────────────────────┘
```

- **Removed args**: `max_pool_index_en`
- **Added args**: `rsvd`
- **`dst`**: width 14b → 11b
- **`pool_addr_mode`**: width 4b → 3b
- **`rsvd`** (NEW) [14] 1b: Reserved for future use. Must be 0.

### `GMPOOL`

```
Blackhole:
┌──────────────┬─────────────┬────────────────┬───────────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │ pool_addr_mode │ max_pool_index_en │                   dst                    │
│  2b [23:22]  │  3b [21:19] │   4b [18:15]   │      1b [14]      │                14b [13:0]                │
└──────────────┴─────────────┴────────────────┴───────────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬─────────────┬─────────┬────────────────┬─────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │   ---   │ pool_addr_mode │   rsvd  │    ---     │               dst               │
│  2b [23:22]  │  3b [21:19] │ 1b [18] │   3b [17:15]   │ 1b [14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴─────────────┴─────────┴────────────────┴─────────┴────────────┴─────────────────────────────────┘
```

- **Removed args**: `max_pool_index_en`
- **Added args**: `rsvd`
- **`dst`**: width 14b → 11b
- **`pool_addr_mode`**: width 4b → 3b
- **`rsvd`** (NEW) [14] 1b: Reserved for future use. Must be 0.

### `MVMUL`

```
Blackhole:
┌──────────────┬─────────────┬───────────────┬──────────────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │   addr_mode   │                   dst                    │
│  2b [23:22]  │  3b [21:19] │   5b [18:14]  │                14b [13:0]                │
└──────────────┴─────────────┴───────────────┴──────────────────────────────────────────┘

Quasar:
┌──────────────┬─────────────┬────────────┬────────────┬────────────┬─────────────────────────────────┐
│ clear_dvalid │ instr_mod19 │    ---     │ addr_mode  │    ---     │               dst               │
│  2b [23:22]  │  3b [21:19] │ 2b [18:17] │ 3b [16:14] │ 3b [13:11] │            11b [10:0]           │
└──────────────┴─────────────┴────────────┴────────────┴────────────┴─────────────────────────────────┘
```

- **`dst`**: width 14b → 11b
- **`addr_mode`**: width 5b → 3b
- **`instr_mod19`**: description changed

### `SFPABS`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `imm12_math`
- **`instr_mod1`**: description changed
- **`lreg_c`**: description changed

### `SFPADD`

```
Blackhole:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│       lreg_src_a       │ lreg_src_b │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│         lreg_a         │   lreg_b   │   lreg_c   │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`, `lreg_src_b`, `lreg_src_a`
- **Added args**: `lreg_c`, `lreg_b`, `lreg_a`
- **`instr_mod1`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index
- **`lreg_b`** (NEW) [15:12] 4b: lreg src_b index
- **`lreg_a`** (NEW) [23:16] 8b: lreg src_a index

### `SFPAND`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │    ---     │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `instr_mod1`, `imm12_math`
- **`lreg_dest`**: description changed

### `SFPCAST`

```
Blackhole:
┌────────────────────────────────────────────────┬────────────┬────────────┐
│                   lreg_src_c                   │ lreg_dest  │ instr_mod1 │
│                   16b [23:8]                   │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`
- **Added args**: `lreg_c`
- **`instr_mod1`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index - src_c provides the value to be cast

### `SFPCOMPC`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────────────────────────────────────────┐
│                                  ---                                   │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘
```

- **Removed args**: `instr_mod1`, `lreg_dest`, `lreg_c`, `imm12_math`

### `SFPENCC`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────────────────┬────────────┐
│             imm12_math             │          ---           │ instr_mod1 │
│            12b [23:12]             │       8b [11:4]        │  4b [3:0]  │
└────────────────────────────────────┴────────────────────────┴────────────┘
```

- **Removed args**: `lreg_dest`, `lreg_c`
- **`instr_mod1`**: description changed
- **`imm12_math`**: description changed

### `SFPEXEXP`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `imm12_math`
- **`instr_mod1`**: description changed

### `SFPEXMAN`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `imm12_math`
- **`instr_mod1`**: description changed

### `SFPLUT`

```
Blackhole:
┌────────────┬────────────┬────────────────────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │                 dest_reg_addr                  │
│ 4b [23:20] │ 4b [19:16] │                   16b [15:0]                   │
└────────────┴────────────┴────────────────────────────────────────────────┘

Quasar:
┌────────────┬────────────┬────────────────────────────────────────────────┐
│  lreg_ind  │ instr_mod0 │                      ---                       │
│ 4b [23:20] │ 4b [19:16] │                   16b [15:0]                   │
└────────────┴────────────┴────────────────────────────────────────────────┘
```

- **Removed args**: `dest_reg_addr`
- **`instr_mod0`**: description changed
- **`lreg_ind`**: description changed

### `SFPLUTFP32`

```
Blackhole:
┌────────────────────────────────────────────────────────────┬────────────┐
│                         lreg_dest                          │ instr_mod1 │
│                         20b [23:4]                         │  4b [3:0]  │
└────────────────────────────────────────────────────────────┴────────────┘

Quasar:
┌────────────────────────────────────────────────┬────────────┬────────────┐
│                      ---                       │ lreg_dest  │ instr_mod1 │
│                   16b [23:8]                   │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────────────────┴────────────┴────────────┘
```

- **`instr_mod1`**: description changed
- **`lreg_dest`**: width 20b → 4b

### `SFPLZ`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `imm12_math`
- **`instr_mod1`**: description changed

### `SFPMAD`

```
Blackhole:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│       lreg_src_a       │ lreg_src_b │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│         lreg_a         │   lreg_b   │   lreg_c   │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`, `lreg_src_b`, `lreg_src_a`
- **Added args**: `lreg_c`, `lreg_b`, `lreg_a`
- **`instr_mod1`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index
- **`lreg_b`** (NEW) [15:12] 4b: lreg src_b index
- **`lreg_a`** (NEW) [23:16] 8b: lreg src_a index

### `SFPMOV`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `imm12_math`
- **`instr_mod1`**: description changed

### `SFPMUL`

```
Blackhole:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│       lreg_src_a       │ lreg_src_b │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│         lreg_a         │   lreg_b   │   lreg_c   │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`, `lreg_src_b`, `lreg_src_a`
- **Added args**: `lreg_c`, `lreg_b`, `lreg_a`
- **`instr_mod1`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index
- **`lreg_b`** (NEW) [15:12] 4b: lreg src_b index
- **`lreg_a`** (NEW) [23:16] 8b: lreg src_a index

### `SFPMUL24`

```
Blackhole:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│       lreg_src_a       │ lreg_src_b │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────┬────────────┬────────────┬────────────┬────────────┐
│         lreg_a         │   lreg_b   │    ---     │ lreg_dest  │ instr_mod1 │
│       8b [23:16]       │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────┴────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`, `lreg_src_b`, `lreg_src_a`
- **Added args**: `lreg_b`, `lreg_a`
- **`instr_mod1`**: description changed
- **`lreg_b`** (NEW) [15:12] 4b: lreg src_b index
- **`lreg_a`** (NEW) [23:16] 8b: lreg src_a index

### `SFPNOT`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │    ---     │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `instr_mod1`, `imm12_math`

### `SFPOR`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │    ---     │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `instr_mod1`, `imm12_math`
- **`lreg_dest`**: description changed

### `SFPPOPC`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────────────────────────────┬────────────┐
│                            ---                             │ instr_mod1 │
│                         20b [23:4]                         │  4b [3:0]  │
└────────────────────────────────────────────────────────────┴────────────┘
```

- **Removed args**: `lreg_dest`, `lreg_c`, `imm12_math`
- **`instr_mod1`**: description changed

### `SFPPUSHC`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────────────────────────────┬────────────┐
│                            ---                             │ instr_mod1 │
│                         20b [23:4]                         │  4b [3:0]  │
└────────────────────────────────────────────────────────────┴────────────┘
```

- **Removed args**: `lreg_dest`, `lreg_c`, `imm12_math`
- **`instr_mod1`**: description changed

### `SFPSETCC`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │    ---     │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_dest`
- **`instr_mod1`**: description changed
- **`imm12_math`**: description changed

### `SFPSHFT2`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`
- **Added args**: `lreg_c`
- **`instr_mod1`**: description changed
- **`lreg_dest`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index

### `SFPSWAP`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`
- **Added args**: `lreg_c`
- **`instr_mod1`**: description changed
- **`imm12_math`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index

### `SFPTRANSP`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────────────────────────────────────────┐
│                                  ---                                   │
│                               24b [23:0]                               │
└────────────────────────────────────────────────────────────────────────┘
```

- **Removed args**: `instr_mod1`, `lreg_dest`, `lreg_c`, `imm12_math`

### `SFPXOR`

```
Blackhole:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│             imm12_math             │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │    ---     │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `instr_mod1`, `imm12_math`
- **`lreg_dest`**: description changed

### `SFP_STOCH_RND`

```
Blackhole:
┌────────────┬───────────────┬────────────┬────────────┬────────────┬────────────┐
│  rnd_mode  │   imm8_math   │ lreg_src_b │ lreg_src_c │ lreg_dest  │ instr_mod1 │
│ 3b [23:21] │   5b [20:16]  │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────┴───────────────┴────────────┴────────────┴────────────┴────────────┘

Quasar:
┌────────────┬───────────────┬────────────┬────────────┬────────────┬────────────┐
│  rnd_mode  │   imm8_math   │   lreg_b   │   lreg_c   │ lreg_dest  │ instr_mod1 │
│ 3b [23:21] │   5b [20:16]  │ 4b [15:12] │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────┴───────────────┴────────────┴────────────┴────────────┴────────────┘
```

- **Removed args**: `lreg_src_c`, `lreg_src_b`
- **Added args**: `lreg_c`, `lreg_b`
- **`instr_mod1`**: description changed
- **`imm8_math`**: description changed
- **`rnd_mode`**: description changed
- **`lreg_c`** (NEW) [11:8] 4b: lreg src_c index - src_c provides the value to be rounded
- **`lreg_b`** (NEW) [15:12] 4b: lreg src_b index - src_b[4:0] provides 5-bit descale value for rounding to int8 from int32

---

## Instructions With Identical Bit Layout

These instructions changed only in `ttsync_resource` tag and/or field description text.

### Non-SFPU

| Instruction | op_binary | What changed |
|---|---|---|
| `MOVB2A` | 0x0B | `ttsync_resource: OTHERS`; `instr_mod` description |
| `NOP` | 0x02 | `ttsync_resource: OTHERS` |
| `RDCFG` | 0xB1 | `ttsync_resource: RDCFG` |
| `SHIFTXB` | 0x18 | `ttsync_resource: OTHERS` |
| `WRCFG` | 0xB0 | `ttsync_resource: WRCFG` |

### SFPU

All gained explicit `size` annotations in the Quasar YAML; bit layout unchanged.

| &nbsp; | &nbsp; | &nbsp; | &nbsp; | &nbsp; |
| --- | --- | --- | --- | --- |
| `SFPADDI` | `SFPCONFIG` | `SFPDIVP2` | `SFPGT` | `SFPIADD` |
| `SFPLE` | `SFPLOADI` | `SFPMULI` | `SFPSETEXP` | `SFPSETMAN` |
| `SFPSETSGN` | `SFPSHFT` | &nbsp; | &nbsp; | &nbsp; |

**`SFPADDI`.`instr_mod1`** diverges at:
> BH: `...instruction modifier...`
> QS: `...Instruction Modifier bit 3:   0: Result is written to the LREG with index lreg_d...`

**`SFPADDI`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_c index...`

**`SFPADDI`.`imm16_math`** diverges at:
> BH: `...ediate 16bit operand...`
> QS: `...ediate 16bit operand in FP16_B format...`

**`SFPCONFIG`.`instr_mod1`** diverges at:
> BH: `...instruction modifier :   [0]        : if config_dest is     one of the constant_...`
> QS: `...Instruction Modifier bit 3:   0: Config register update is applied to all SFPU c...`

**`SFPCONFIG`.`config_dest`** diverges at:
> BH: `...            : 0x8   RESERVED                       : 0xA - 0x9   Programmable co...`
> QS: `...            : 0x8   LUT constant          lreg[ 9] : 0x9   LUT constant         ...`

**`SFPDIVP2`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier:   0: Replace the exponent of src_c with imm12_math[7:0]   ...`

**`SFPGT`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bit 3:   0: Do not write anything to lreg_dest   1: Fill lr...`

**`SFPGT`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPGT`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...Extension of the instr_mod1 field. bits 11:1:   Reserved, must always be 0  bit ...`

**`SFPIADD`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bit 3:   0: CC Result sense is preserved   1: CC Result sen...`

**`SFPIADD`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPIADD`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...2's complement immediate operand...`

**`SFPLE`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bit 3:   0: Do not write anything to lreg_dest   1: Fill lr...`

**`SFPLE`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPLE`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...Extension of the instr_mod1 field. bits 11:1:   Reserved, must always be 0  bit ...`

**`SFPLOADI`.`imm16`** diverges at:
> BH: `...immediate op...`
> QS: `...immediate op...`

**`SFPLOADI`.`instr_mod0`** diverges at:
> BH: `...instruction modifier...`
> QS: `...Immediate format:   FP16B         4'b0000   FP16A         4'b0001   UINT16      ...`

**`SFPMULI`.`instr_mod1`** diverges at:
> BH: `...instruction modifier...`
> QS: `...Instruction Modifier bit 3:   0: Result is written to the LREG with index lreg_d...`

**`SFPMULI`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_a index...`

**`SFPMULI`.`imm16_math`** diverges at:
> BH: `...ediate 16bit operand...`
> QS: `...ediate 16bit operand in FP16_B format...`

**`SFPSETEXP`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bits 3:2:   Reserved, must always be 0  Instruction Modifie...`

**`SFPSETEXP`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPSETEXP`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...Immediate source for new exponent...`

**`SFPSETMAN`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bits 3:1:   Reserved, must always be 0  Instruction Modifie...`

**`SFPSETMAN`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPSETMAN`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...Immediate source for new mantissa...`

**`SFPSETSGN`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bits 3:1:   Reserved, must always be 0  Instruction Modifie...`

**`SFPSETSGN`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPSETSGN`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...Immediate source for new sign...`

**`SFPSHFT`.`instr_mod1`** diverges at:
> BH: `...instruction modifier; for SFPAND and SPFOR when instrn_mod1[0] is set imm12_math...`
> QS: `...Instruction Modifier bit 3:   Reserved, must always be 0  Instruction Modifier b...`

**`SFPSHFT`.`lreg_dest`** diverges at:
> BH: `...lreg dest index...`
> QS: `...lreg dest and src_b index...`

**`SFPSHFT`.`imm12_math`** diverges at:
> BH: `...immediate 12bit operand...`
> QS: `...2's complement shift amount...`

---

# New Instructions (Quasar only)

74 instructions present in Quasar but not in Blackhole.

## GPR Arithmetic

| Instruction | op_binary | type | arguments |
|---|---|---|---|
| `ADDGPR` | 0x58 | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpB_is_Const[23:23] |
| `BITWOPGPR` | 0x5B | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpSel[18:22], OpB_is_Const[23:23] |
| `CMPGPR` | 0x5D | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpSel[18:22], OpB_is_Const[23:23] |
| `MULGPR` | 0x5A | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpB_is_Const[23:23] |
| `SETGPR` | 0x45 | TDMA | GPR_Index16b[0:6], SetSignalsMode[7:7], Payload_SigSel[8:21], Payload_SigSelSize[22:23] |
| `SHIFTGPR` | 0x5C | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpSel[18:22], OpB_is_Const[23:23] |
| `SUBGPR` | 0x59 | TDMA | OpA_GPR_Index[0:5], OpB_GPR_Index[6:11], Result_GPR_Index[12:17], OpB_is_Const[23:23] |

Representative layout (`ADDGPR`):

```
┌──────────────┬───────────────┬──────────────────┬──────────────────┬──────────────────┐
│ OpB_is_Const │      ---      │ Result_GPR_Index │  OpB_GPR_Index   │  OpA_GPR_Index   │
│   1b [23]    │   5b [22:18]  │    6b [17:12]    │    6b [11:6]     │     6b [5:0]     │
└──────────────┴───────────────┴──────────────────┴──────────────────┴──────────────────┘
```

---

## Pack (PACR) Variants

| Instruction | op_binary | type | arguments |
|---|---|---|---|
| `PACR0_FACE` | 0x1F | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `PACR0_FACE_INC` | 0x20 | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `PACR0_ROW` | 0x2A | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `PACR0_ROW_INC` | 0x2B | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `PACR0_TILE` | 0x0F | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:15], Dst_Tile_Idx[16:23] |
| `PACR0_TILE_INC` | 0x19 | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:15], Dst_Tile_Idx_Inc[16:23] |
| `PACR1_FACE` | 0x2E | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `PACR1_FACE_INC` | 0x2F | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `PACR1_ROW` | 0x3B | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `PACR1_ROW_INC` | 0x6E | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:8], Dst_Tile_Offset_Idx_Inc[9:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `PACR1_TILE` | 0x2C | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:15], Dst_Tile_Idx[16:23] |
| `PACR1_TILE_INC` | 0x2D | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:15], Dst_Tile_Idx_Inc[16:23] |

Representative layout (`PACR0_FACE`):

```
┌────────────────────────┬──────────────┬──────────────┬─────────────────────────┬─────────────────────────┬─────────────────────────────┬─────────────┬────────┐
│          ---           │ Dst_Face_Idx │ Src_Face_Idx │ Dst_Tile_Offset_Idx_Inc │ Src_Tile_Offset_Idx_Inc │ Buffer_Descriptor_Table_Sel │ ClrDatValid │  ---   │
│       8b [23:16]       │  2b [15:14]  │  2b [13:12]  │        3b [11:9]        │         2b [8:7]        │           5b [6:2]          │    1b [1]   │ 1b [0] │
└────────────────────────┴──────────────┴──────────────┴─────────────────────────┴─────────────────────────┴─────────────────────────────┴─────────────┴────────┘
```

---

## Unpack (UNPACR) Variants

| Instruction | op_binary | type | arguments |
|---|---|---|---|
| `UNPACR0_FACE` | 0x47 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `UNPACR0_FACE_INC` | 0x3F | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `UNPACR0_ROW` | 0x4B | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `UNPACR0_ROW_INC` | 0x4C | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `UNPACR0_STRIDE` | 0x6F | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], L1_16datums_Row_Index[7:12], Row_Mask_Reg_Sel[13:15], Tile_Idx_Inc[16:16], L1_Tile_Idx_or_Tile_Idx_Inc[17:19], Src_Reg_Y_Cntr_Incr[20:23] |
| `UNPACR0_TILE` | 0x3C | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:14], Dst_Tile_Idx[15:23] |
| `UNPACR0_TILE_INC` | 0x44 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:14], Dst_Tile_Idx_Inc[15:23] |
| `UNPACR1_FACE` | 0x6A | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `UNPACR1_FACE_INC` | 0x6B | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `UNPACR1_ROW` | 0x6C | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `UNPACR1_ROW_INC` | 0x6D | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `UNPACR1_STRIDE` | 0xAA | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], L1_16datums_Row_Index[7:12], Row_Mask_Reg_Sel[13:15], Tile_Idx_Inc[16:16], L1_Tile_Idx_or_Tile_Idx_Inc[17:19], Src_Reg_Y_Cntr_Incr[20:23] |
| `UNPACR1_TILE` | 0x5F | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:14], Dst_Tile_Idx[15:23] |
| `UNPACR1_TILE_INC` | 0x69 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:14], Dst_Tile_Idx_Inc[15:23] |
| `UNPACR2_FACE` | 0x9D | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `UNPACR2_FACE_INC` | 0x9E | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `UNPACR2_ROW` | 0x9F | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `UNPACR2_ROW_INC` | 0xA8 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `UNPACR2_STRIDE` | 0x4E | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], L1_16datums_Row_Index[7:12], Row_Mask_Reg_Sel[13:15], Tile_Idx_Inc[16:16], L1_Tile_Idx_or_Tile_Idx_Inc[17:19], Src_Reg_Y_Cntr_Incr[20:23] |
| `UNPACR2_TILE` | 0x9B | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:14], Dst_Tile_Idx[15:23] |
| `UNPACR2_TILE_INC` | 0x9C | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:14], Dst_Tile_Idx_Inc[15:23] |
| `UNPACR_DEST_FACE` | 0xAE | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15] |
| `UNPACR_DEST_FACE_INC` | 0xAF | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15] |
| `UNPACR_DEST_ROW` | 0xA7 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx[12:13], Dst_Face_Idx[14:15], Src_Row_Idx[16:19], Dst_Row_Idx[20:23] |
| `UNPACR_DEST_ROW_INC` | 0x65 | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Offset_Idx_Inc[7:9], Dst_Tile_Offset_Idx_Inc[10:11], Src_Face_Idx_Inc[12:13], Dst_Face_Idx_Inc[14:15], Src_Row_Idx_Inc[16:19], Dst_Row_Idx_Inc[20:23] |
| `UNPACR_DEST_STRIDE` | 0xBD | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], L1_16datums_Row_Index[7:12], Row_Mask_Reg_Sel[13:15], Tile_Idx_Inc[16:16], L1_Tile_Idx_or_Tile_Idx_Inc[17:19], Src_Reg_Y_Cntr_Incr[20:23] |
| `UNPACR_DEST_TILE` | 0xAC | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:14], Dst_Tile_Idx[15:23] |
| `UNPACR_DEST_TILE_INC` | 0xAD | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx_Inc[7:14], Dst_Tile_Idx_Inc[15:23] |

Representative layout (`UNPACR0_FACE`):

```
┌────────────────────────┬──────────────┬──────────────┬─────────────────────────┬─────────────────────────┬─────────────────────────────┬─────────────┬────────┐
│          ---           │ Dst_Face_Idx │ Src_Face_Idx │ Dst_Tile_Offset_Idx_Inc │ Src_Tile_Offset_Idx_Inc │ Buffer_Descriptor_Table_Sel │ SetDatValid │  ---   │
│       8b [23:16]       │  2b [15:14]  │  2b [13:12]  │        2b [11:10]       │         3b [9:7]        │           5b [6:2]          │    1b [1]   │ 1b [0] │
└────────────────────────┴──────────────┴──────────────┴─────────────────────────┴─────────────────────────┴─────────────────────────────┴─────────────┴────────┘
```

---

## Tile Buffer Management

### `POP_TILES`

- **op_binary**: 62 (0x3E)
- **instrn_type**: TDMA
- **ex_resource**: `UNPACK`

```
┌──────────────────┬────────────────────────────┬──────────────────────────────┬───────────────┐
│       ---        │ unpacker_rd_done_wait_mask │          num_tiles           │   buffer_sel  │
│    6b [23:18]    │         3b [17:15]         │          10b [14:5]          │    5b [4:0]   │
└──────────────────┴────────────────────────────┴──────────────────────────────┴───────────────┘
```

### `PUSH_TILES`

- **op_binary**: 61 (0x3D)
- **instrn_type**: TDMA
- **ex_resource**: `PACK`

```
┌─────────────────────┬──────────────────────────┬──────────────────────────────┬───────────────┐
│         ---         │ packer_wr_done_wait_mask │          num_tiles           │   buffer_sel  │
│      7b [23:17]     │        2b [16:15]        │          10b [14:5]          │    5b [4:0]   │
└─────────────────────┴──────────────────────────┴──────────────────────────────┴───────────────┘
```

### `WAIT_FREE`

- **op_binary**: 171 (0xAB)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `SYNC`

```
┌───────────────────────────┬──────────────────────────────┬───────────────┐
│         stall_res         │          num_tiles           │   buffer_sel  │
│         9b [23:15]        │          10b [14:5]          │    5b [4:0]   │
└───────────────────────────┴──────────────────────────────┴───────────────┘
```

### `WAIT_TILES`

- **op_binary**: 169 (0xA9)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `SYNC`

```
┌───────────────────────────┬──────────────────────────────┬───────────────┐
│         stall_res         │          num_tiles           │   buffer_sel  │
│         9b [23:15]        │          10b [14:5]          │    5b [4:0]   │
└───────────────────────────┴──────────────────────────────┴───────────────┘
```

---

## Address Mode

### `INC_DST_TILE_FACE_ROW_IDX`

- **op_binary**: 14 (0x0E)
- **instrn_type**: ADDRMOD
- **ex_resource**: `TDMA`

```
┌─────────┬───────────────────┬────────────┬──────────────────────────────────────────────────────┐
│   ---   │ Tile_Face_Row_Sel │ EngineSel  │                        Value                         │
│ 1b [23] │     2b [22:21]    │ 3b [20:18] │                      18b [17:0]                      │
└─────────┴───────────────────┴────────────┴──────────────────────────────────────────────────────┘
```

### `INC_SRC_TILE_FACE_ROW_IDX`

- **op_binary**: 7 (0x07)
- **instrn_type**: ADDRMOD
- **ex_resource**: `TDMA`

```
┌─────────┬───────────────────┬────────────┬──────────────────────────────────────────────────────┐
│   ---   │ Tile_Face_Row_Sel │ EngineSel  │                        Value                         │
│ 1b [23] │     2b [22:21]    │ 3b [20:18] │                      18b [17:0]                      │
└─────────┴───────────────────┴────────────┴──────────────────────────────────────────────────────┘
```

### `SET_DST_TILE_FACE_ROW_IDX`

- **op_binary**: 13 (0x0D)
- **instrn_type**: ADDRMOD
- **ex_resource**: `TDMA`

```
┌─────────┬───────────────────┬────────────┬──────────────────────────────────────────────────────┐
│   ---   │ Tile_Face_Row_Sel │ EngineSel  │                        Value                         │
│ 1b [23] │     2b [22:21]    │ 3b [20:18] │                      18b [17:0]                      │
└─────────┴───────────────────┴────────────┴──────────────────────────────────────────────────────┘
```

### `SET_SRC_TILE_FACE_ROW_IDX`

- **op_binary**: 6 (0x06)
- **instrn_type**: ADDRMOD
- **ex_resource**: `TDMA`

```
┌─────────┬───────────────────┬────────────┬──────────────────────────────────────────────────────┐
│   ---   │ Tile_Face_Row_Sel │ EngineSel  │                        Value                         │
│ 1b [23] │     2b [22:21]    │ 3b [20:18] │                      18b [17:0]                      │
└─────────┴───────────────────┴────────────┴──────────────────────────────────────────────────────┘
```

---

## SFPU

### `SFPNONLINEAR`

- **op_binary**: 153 (0x99)
- **instrn_type**: SFPU
- **ex_resource**: `SFPU`

```
┌────────────────────────────────────┬────────────┬────────────┬────────────┐
│                ---                 │   lreg_c   │ lreg_dest  │ instr_mod1 │
│            12b [23:12]             │ 4b [11:8]  │  4b [7:4]  │  4b [3:0]  │
└────────────────────────────────────┴────────────┴────────────┴────────────┘
```

---

## CFG Aliases

### `CFGSHIFTMASK_BUT_ALIAS_BIT_8_OF_CFG_REG_ADDR_WITH_LSB_OF_OPCODE`

- **op_binary**: 187 (0xBB)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `CFG`

```
┌────────────────────────┬─────────────────────────┬────────────┬───────────────┬──────────────────┬─────────────┐
│       CfgRegAddr       │ disable_mask_on_old_val │ operation  │   mask_width  │ right_cshift_amt │ scratch_sel │
│       8b [23:16]       │         1b [15]         │ 3b [14:12] │   5b [11:7]   │     5b [6:2]     │   2b [1:0]  │
└────────────────────────┴─────────────────────────┴────────────┴───────────────┴──────────────────┴─────────────┘
```

### `RMWCIB0_BUT_ALIAS_BIT_8_OF_CFG_REG_ADDR_WITH_LSB_OF_OPCODE`

- **op_binary**: 179 (0xB3)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `CFG`

```
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

### `RMWCIB1_BUT_ALIAS_BIT_8_OF_CFG_REG_ADDR_WITH_LSB_OF_OPCODE`

- **op_binary**: 181 (0xB5)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `CFG`

```
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

### `RMWCIB2_BUT_ALIAS_BIT_8_OF_CFG_REG_ADDR_WITH_LSB_OF_OPCODE`

- **op_binary**: 183 (0xB7)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `CFG`

```
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

### `RMWCIB3_BUT_ALIAS_BIT_8_OF_CFG_REG_ADDR_WITH_LSB_OF_OPCODE`

- **op_binary**: 185 (0xB9)
- **instrn_type**: LOCAL_CREGS
- **ex_resource**: `CFG`

```
┌────────────────────────┬────────────────────────┬────────────────────────┐
│       CfgRegAddr       │          Mask          │          Data          │
│       8b [23:16]       │       8b [15:8]        │        8b [7:0]        │
└────────────────────────┴────────────────────────┴────────────────────────┘
```

---

## Other New

| Instruction | op_binary | type | arguments |
|---|---|---|---|
| `COMMIT_SHADOW` | 0x41 | OTHERS | force_commit[0:19] |
| `HALT` | 0x23 | COMPUTE | (none) |
| `PACR_STRIDE` | 0x1D | TDMA | ClrDatValid[0:0], PackerSel[1:1], Buffer_Descriptor_Table_Sel[2:6], L1_16datums_Row_Index[7:12], Tile_Idx_Inc[13:13], L1_Tile_Idx_or_Tile_Idx_Inc[14:16], Src_Row_Idx_Inc[17:17], Src_Row_Idx_or_Inc_Mul4[18:23] |
| `PACR_UNTILIZE` | 0x42 | TDMA | ClrDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Packer_Sel[7:7], Src_Z_Cntr_inc[8:9], Dst_Z_Cntr_inc[10:11], Cntr_Reset_mask[12:13], Reserved[14:23] |
| `RV_PACR` | 0x35 | TDMA | reg_idx0[0:4], reg_idx1[5:9], reg_idx2[10:14] |
| `RV_UNPACR` | 0x39 | TDMA | reg_idx0[0:4], reg_idx1[5:9], reg_idx2[10:14] |
| `RV_WRCFG` | 0x54 | LOCAL_CREGS | index_of_reg_containing_wrdata_lsbs[0:4], index_of_reg_containing_wrdata_msbs[5:9], index_of_reg_containing_cfg_index[10:14], write_64b[15:15], byte_mask[16:23] |
| `UNPACR_TILE_MISC` | 0xBF | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Src_Tile_Idx[7:11], Dst_Tile_Idx[12:13], Tile_Idx_Inc[14:14], Row_Bcast_Row_Idx[15:20], Unpack_Type[21:23] |
| `UNPACR_TILIZE` | 0xBE | TDMA | SetDatValid[1:1], Buffer_Descriptor_Table_Sel[2:6], Unpack_Sel[7:8], Src_Z_Cntr_inc[9:10], Dst_Z_Cntr_inc[11:12], Cntr_Reset_mask[13:14], Reserved[15:23] |

Representative layout (`COMMIT_SHADOW`):

```
┌────────────┬────────────────────────────────────────────────────────────┐
│    ---     │                        force_commit                        │
│ 4b [23:20] │                         20b [19:0]                         │
└────────────┴────────────────────────────────────────────────────────────┘
```

---

## Other New

### `ELWADDDI`

- **op_binary**: 49 (0x31)
- **instrn_type**: COMPUTE
- **ex_resource**: `MATH`

```
┌──────────────┬────────────┬────────────┬────────────┬───────────┬────────────────────────┐
│ clear_dvalid │  ins_mod   │ srcb_addr  │ srca_addr  │ addr_mode │          dst           │
│  2b [23:22]  │ 3b [21:19] │ 4b [18:15] │ 4b [14:11] │ 3b [10:8] │        8b [7:0]        │
└──────────────┴────────────┴────────────┴────────────┴───────────┴────────────────────────┘
```

### `ELWMULDI`

- **op_binary**: 58 (0x3A)
- **instrn_type**: COMPUTE
- **ex_resource**: `MATH`

```
┌──────────────┬────────────┬────────────┬────────────┬───────────┬────────────────────────┐
│ clear_dvalid │  ins_mod   │ srcb_addr  │ srca_addr  │ addr_mode │          dst           │
│  2b [23:22]  │ 3b [21:19] │ 4b [18:15] │ 4b [14:11] │ 3b [10:8] │        8b [7:0]        │
└──────────────┴────────────┴────────────┴────────────┴───────────┴────────────────────────┘
```

### `ELWSUBDI`

- **op_binary**: 50 (0x32)
- **instrn_type**: COMPUTE
- **ex_resource**: `MATH`

```
┌──────────────┬────────────┬────────────┬────────────┬───────────┬────────────────────────┐
│ clear_dvalid │  ins_mod   │ srcb_addr  │ srca_addr  │ addr_mode │          dst           │
│  2b [23:22]  │ 3b [21:19] │ 4b [18:15] │ 4b [14:11] │ 3b [10:8] │        8b [7:0]        │
└──────────────┴────────────┴────────────┴────────────┴───────────┴────────────────────────┘
```

### `MVMULDI`

- **op_binary**: 37 (0x25)
- **instrn_type**: COMPUTE
- **ex_resource**: `MATH`

```
┌──────────────┬────────────┬────────────┬────────────┬───────────┬────────────────────────┐
│ clear_dvalid │  ins_mod   │ srcb_addr  │ srca_addr  │ addr_mode │          dst           │
│  2b [23:22]  │ 3b [21:19] │ 4b [18:15] │ 4b [14:11] │ 3b [10:8] │        8b [7:0]        │
└──────────────┴────────────┴────────────┴────────────┴───────────┴────────────────────────┘
```

---

# Removed Instructions (available in Blackhole only)

47 instructions present in Blackhole but not in Quasar.

## DMA Register Arithmetic

| Instruction | op_binary | type |
|---|---|---|
| `ADDDMAREG` | 0x58 | TDMA |
| `SUBDMAREG` | 0x59 | TDMA |
| `MULDMAREG` | 0x5A | TDMA |
| `SHIFTDMAREG` | 0x5C | TDMA |
| `BITWOPDMAREG` | 0x5B | TDMA |
| `CMPDMAREG` | 0x5D | TDMA |
| `SETDMAREG` | 0x45 | TDMA |

---

## Convolution / Pooling

| Instruction | op_binary | type |
|---|---|---|
| `CONV3S1` | 0x22 | COMPUTE |
| `CONV3S2` | 0x23 | COMPUTE |
| `APOOL3S1` | 0x25 | COMPUTE |
| `APOOL3S2` | 0x32 | COMPUTE |
| `MPOOL3S1` | 0x24 | COMPUTE |
| `MPOOL3S2` | 0x31 | COMPUTE |
| `MFCONV3S1` | 0x3A | COMPUTE |
| `DOTPV` | 0x29 | COMPUTE |

---

## Address Control

| Instruction | op_binary | type |
|---|---|---|
| `SETADC` | 0x50 | ADDRMOD |
| `SETADCXX` | 0x5E | ADDRMOD |
| `SETADCXY` | 0x51 | ADDRMOD |
| `SETADCZW` | 0x54 | ADDRMOD |
| `ADDRCRXY` | 0x53 | ADDRMOD |
| `ADDRCRZW` | 0x56 | ADDRMOD |
| `INCADCXY` | 0x52 | ADDRMOD |
| `INCADCZW` | 0x55 | ADDRMOD |

---

## Compute Misc

| Instruction | op_binary | type |
|---|---|---|
| `CLREXPHIST` | 0x21 | COMPUTE |
| `GATESRCRST` | 0x35 | COMPUTE |
| `RAREB` | 0x15 | COMPUTE |
| `TRNSPSRCA` | 0x14 | COMPUTE |
| `TRNSPSRCB` | 0x16 | COMPUTE |
| `SHIFTXA` | 0x17 | COMPUTE |
| `SETASHRMH` | 0x1E | COMPUTE |
| `SETASHRMH0` | 0x1A | COMPUTE |
| `SETASHRMH1` | 0x1B | COMPUTE |
| `SETASHRMV` | 0x1C | COMPUTE |
| `SETPKEDGOF` | 0x1D | COMPUTE |
| `SETIBRWC` | 0x39 | COMPUTE |

---

## Pack / Unpack / DMA

| Instruction | op_binary | type |
|---|---|---|
| `PACR` | 0x41 | TDMA |
| `PACR_SETREG` | 0x4A | TDMA |
| `UNPACR` | 0x42 | TDMA |
| `REG2FLOP` | 0x48 | TDMA |
| `FLUSHDMA` | 0x46 | TDMA |
| `RSTDMA` | 0x44 | TDMA |
| `TBUFCMD` | 0x4B | TDMA |
| `XMOV` | 0x40 | TDMA |

---

## CFG / Stream

| Instruction | op_binary | type |
|---|---|---|
| `SETC16` | 0xB2 | LOCAL_CREGS |
| `STREAMWAIT` | 0xA7 | LOCAL_CREGS |
| `STREAMWRCFG` | 0xB7 | LOCAL_CREGS |

---

## SFPU

| Instruction | op_binary | type |
|---|---|---|
| `SFPARECIP` | 0x99 | SFPU |

---
