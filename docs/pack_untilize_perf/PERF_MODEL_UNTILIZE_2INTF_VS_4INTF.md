# Performance Model: 2-Interface vs 4-Interface Untilize
## Analysis for block_ct_dim = full_ct_dim = 8, num_faces = 4

---

## Constants
- **face_r_dim** = 16 (rows per face for 32x32 tiles)
- **FACE_C_DIM** = 16 (columns per face)
- **block_ct_dim** = 8 (tiles to process)
- **num_faces** = 4 (full 32x32 tiles)
- **num_faces_per_rdim_tile** = 2 (top faces F0/F1, then bottom faces F2/F3)

---

## Approach 1: _llk_pack_untilize_ (2-Interface Standard)

### MOP Configuration
```
MOP_OUTER_LOOP = face_r_dim = 16
MOP_INNER_LOOP = block_ct_dim = 8
```

### MOP Structure (per outer loop iteration = 1 row)
```
START_OP (executed once at start of outer loop):
  1. ADDRCRZW (restore W counter from W_Cr)

INNER_LOOP (executed 8 times):
  1. INCADCZW (increment W by 1)
  2. PACR (pack 32 datums using 2 interfaces)
     - Last iteration: PACR with Last bit = 1

END_OPS (executed once at end of outer loop):
  1. INCADCXY (increment Y counter)
  2. REPLAY_BUF (4 instructions):
     - ADDDMAREG (update L1 address)
     - STALLWAIT
     - WRCFG
     - NOP
```

### Instruction Count Per Face Pair

**MOP execution (16 outer loop iterations):**
- **START_OP**: 16 × 1 ADDRCRZW = **16 instructions**
- **INNER_LOOP**: 16 × 8 × (1 INCADCZW + 1 PACR) = **256 instructions**
  - 128 INCADCZW
  - 128 PACR (each packs 32 datums using 2 interfaces)
- **END_OPS**: 16 × (1 INCADCXY + 4 replay_buf) = **80 instructions**
  - 16 INCADCXY
  - 64 address update instructions

**Per face_pair subtotal**: 16 + 256 + 80 = **352 instructions**

**Setup/teardown per face_pair:**
- SETADCZW (1)
- SETADC (1)
- SETADCXY (1)
- After MOP: INCADCZW (1) + SETADCXY (1)
- **Overhead**: ~5 instructions per face_pair

**Total for 2 face_pairs**: 2 × (352 + 5) = **714 instructions**

**Final cleanup**: 2 instructions (SETADCZW + set_dst_write_addr)

### Total Datums Packed
- 128 PACR × 32 datums/PACR = **4096 datums per face_pair**
- 2 face_pairs × 4096 = **8192 datums total** (full 8 tiles × 32×32)

### Packer Bandwidth Utilization
- **2 out of 4 interfaces active** (50% utilization)
- Each PACR: 2 interfaces × 16 datums = 32 datums packed

---

## Approach 2: _llk_pack_untilize_4intf_ (4-Interface Optimized)

### MOP Configuration (for TILE_PAIRS = 4, no remainder)
```
MOP_OUTER_LOOP = face_r_dim = 16
MOP_INNER_LOOP = TILE_PAIRS = 4
```

### MOP Structure (per outer loop iteration = 1 row)
```
START_OP (executed once at start of outer loop):
  1. ADDRCRZW (restore W counter from W_Cr)

INNER_LOOP (executed 4 times):
  1. INCADCZW (increment W by 2, skip to next tile pair)
  2. PACR (pack 64 datums using 4 interfaces)
     - Last iteration: PACR with Last bit = 1

END_OPS (executed once at end of outer loop):
  1. INCADCXY (increment Y counter)
  2. REPLAY_BUF (4 instructions):
     - ADDDMAREG (update L1 address)
     - STALLWAIT
     - WRCFG
     - NOP
```

### Instruction Count Per Face Pair

**MOP execution (16 outer loop iterations):**
- **START_OP**: 16 × 1 ADDRCRZW = **16 instructions**
- **INNER_LOOP**: 16 × 4 × (1 INCADCZW + 1 PACR) = **128 instructions**
  - 64 INCADCZW
  - 64 PACR (each packs 64 datums using 4 interfaces)
- **END_OPS**: 16 × (1 INCADCXY + 4 replay_buf) = **80 instructions**
  - 16 INCADCXY
  - 64 address update instructions

**Per face_pair subtotal**: 16 + 128 + 80 = **224 instructions**

**Setup/teardown per face_pair:**
- SETADCZW (1)
- SETADC (1)
- SETADCXY (1)
- After MOP: SETADCXY (1)
- **Overhead**: ~4 instructions per face_pair

**Total for 2 face_pairs**: 2 × (224 + 4) = **456 instructions**

**Final cleanup**: 2 instructions (SETADCZW + set_dst_write_addr)

### Total Datums Packed
- 64 PACR × 64 datums/PACR = **4096 datums per face_pair**
- 2 face_pairs × 4096 = **8192 datums total** (full 8 tiles × 32×32)

### Packer Bandwidth Utilization
- **4 out of 4 interfaces active** (100% utilization)
- Each PACR: 4 interfaces × 16 datums = 64 datums packed

---

## Performance Comparison Summary

| Metric | 2-Interface | 4-Interface | Ratio |
|--------|-------------|-------------|-------|
| **Total Instructions** | ~714 | ~456 | **0.64× (36% reduction)** |
| **PACR Instructions** | 256 | 128 | **0.50× (50% reduction)** |
| **Datums per PACR** | 32 | 64 | **2.0× (double)** |
| **Total Datums** | 8192 | 8192 | 1.0× (same) |
| **Interface Utilization** | 50% (2/4) | 100% (4/4) | **2.0× improvement** |
| **MOP Inner Loop Size** | 8 | 4 | 0.50× |
| **Setup Overhead** | ~12 | ~10 | Similar |

---

## Theoretical Speedup Analysis

### Perfect Case (No Memory Bottleneck)
If packer execution is the bottleneck:
- **Speedup from PACR reduction**: 256 / 128 = **2.0×**
- **Speedup from total instructions**: 714 / 456 = **1.57×**

### Realistic Case (Memory-Limited)
If L1 bandwidth is the bottleneck:
- Both approaches pack the same 8192 datums
- **Speedup limited by L1 write bandwidth**
- Expected: **1.0× to 1.3×** (reduced control overhead)

### Expected Range
- **Best case**: **1.5-2.0× speedup** (packer-bound workloads)
- **Typical case**: **1.3-1.5× speedup** (balanced compute/memory)
- **Worst case**: **1.0-1.2× speedup** (memory-bound workloads)

---

## Instruction Breakdown by Type

### 2-Interface (256 inner loop iterations total)
| Instruction | Count | Purpose |
|-------------|-------|---------|
| PACR | 256 | Pack 32 datums (2 intf) |
| INCADCZW | 256 + 2 | Increment W (tiles) or Z (faces) |
| ADDRCRZW | 32 | Restore W at row start |
| INCADCXY | 32 + 2 | Increment Y (rows) |
| SETADCZW | 2 | Reset Z counter |
| SETADC | 2 | Set W_Cr |
| SETADCXY | 6 | Reset XY counters |
| ADDDMAREG | 32 | L1 address update |
| STALLWAIT | 32 | Config sync |
| WRCFG | 32 | Write L1 address |
| NOP | 32 | Pipeline fill |
| **Total** | **~714** | |

### 4-Interface (128 inner loop iterations total)
| Instruction | Count | Purpose |
|-------------|-------|---------|
| PACR | 128 | Pack 64 datums (4 intf) |
| INCADCZW | 128 + 2 | Increment W (tile pairs) or Z (faces) |
| ADDRCRZW | 32 | Restore W at row start |
| INCADCXY | 32 + 2 | Increment Y (rows) |
| SETADCZW | 2 | Reset Z counter |
| SETADC | 2 | Set W_Cr |
| SETADCXY | 4 | Reset XY counters |
| ADDDMAREG | 32 | L1 address update |
| STALLWAIT | 32 | Config sync |
| WRCFG | 32 | L1 address |
| NOP | 32 | Pipeline fill |
| **Total** | **~456** | |

---

## Critical Path Analysis

### Both Approaches
The critical path likely includes:
1. **PACR execution time** (dominant for packer-bound)
2. **L1 write latency** (dominant for memory-bound)
3. **Address counter updates** (minimal overhead)
4. **L1 address configuration** (32 STALLWAIT + WRCFG cycles)

### Key Differences
- **4-interface**: Fewer PACR executions (128 vs 256) → reduced pipeline overhead
- **4-interface**: Fewer W counter increments (128 vs 256) → reduced control overhead
- **4-interface**: Same L1 address updates (32) → no difference in memory latency

---

## Odd block_ct_dim Analysis (e.g., block_ct_dim = 7)

### 4-Interface with Remainder
For block_ct_dim = 7:
- **TILE_PAIRS** = 3
- **HAS_REMAINDER** = true (1 tile)

**MOP phase (tile pairs)**:
- 16 × 3 × 2 = **96 instructions** per face_pair
- 2 face_pairs = **192 instructions**

**Remainder tile (explicit loop)**:
- Per face_pair: 16 rows × (1 PACR + 1 INCADCXY + 4 address updates) = **96 instructions**
- 2 face_pairs = **192 instructions**
- Plus: INCADCZW between face_pairs

**Total for block_ct_dim=7**: ~192 + 192 + overhead = **~400 instructions**

**vs 2-Interface**:
- 2-interface: 7 inner loops → 16 × 7 × 2 = 224 per face_pair = **~464 instructions**
- **Speedup**: 464 / 400 = **1.16×** (still beneficial!)

---

## Performance Expectations vs Reality

### What to Measure
1. **Total cycles** for untilize operation
2. **Packer active cycles** (from performance counters)
3. **Memory stall cycles** (to identify bottleneck)
4. **L1 write bandwidth utilization**

### Expected Observations

**If Packer-Bound:**
- 4-interface shows **~1.5-2.0× speedup**
- Packer active cycles reduced by ~50%
- Memory stalls unchanged

**If Memory-Bound:**
- 4-interface shows **~1.0-1.3× speedup**
- Packer active cycles reduced, but total cycles similar
- Memory stalls dominate both cases

**If Balanced:**
- 4-interface shows **~1.3-1.5× speedup**
- Both packer and memory contribute to cycle count
- Reduced packer cycles provide moderate overall benefit

### Validation Checks
- [ ] Same number of datums packed (8192)
- [ ] Same L1 address update count (32)
- [ ] Half the PACR count (128 vs 256)
- [ ] Similar setup overhead (~10 cycles)
- [ ] Speedup correlates with packer utilization %

---

## Conclusion

For **block_ct_dim = 8**:
- **4-interface reduces instruction count by 36%** (714 → 456)
- **4-interface doubles packer bandwidth** (2 → 4 interfaces)
- **Expected speedup: 1.3-2.0×** depending on bottleneck
- **Best for packer-bound workloads** where PACR execution dominates
- **Still beneficial for memory-bound** due to reduced control overhead

The 4-interface optimization trade-off:
- ✅ **50% fewer PACR instructions**
- ✅ **100% packer bandwidth utilization**
- ✅ **Minimal code complexity** (no MOP reprogramming)
- ⚠️ **Requires interleaved dest layout** (handled by Python test infrastructure)
- ⚠️ **Remainder tile adds small overhead** for odd block_ct_dim (still net positive)
