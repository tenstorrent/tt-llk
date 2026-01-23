# Tensix Performance Counters Guide

This guide documents the interface for controlling and reading Tensix hardware performance counters. It covers how to configure and read them from C++ and Python, which useful derived metrics you can compute, and how to test both the interface and the metrics. It consolidates the hardware documentation and aligns it with the implemented interfaces found in this repository.

## Overview

Tensix cores contain hardware performance counters organized into five categories (also called "banks" or "groups"):

| Bank | Description |
|------|-------------|
| INSTRN_THREAD | Instruction issue counts and stall reasons per thread |
| FPU | FPU and SFPU operation valid signals |
| TDMA_UNPACK | Unpacker busy signals and math pipeline status |
| L1 | NoC ring transactions and L1 arbitration events |
| TDMA_PACK | Packer busy signals and destination register access |

Each category has debug registers for control and two output registers:
- `PERF_CNT_*0`: Reference period (optional, only used in mode 1)
- `PERF_CNT_*1`: Mode + counter select + request/grant select
- `PERF_CNT_*2`: Start/stop (rising-edge triggered)
- `PERF_CNT_OUT_L_*`: Cycles counted in the measurement window
- `PERF_CNT_OUT_H_*`: Event-specific count for the selected counter

Special registers:
- `PERF_CNT_ALL`: Start/stop for FPU and INSTRN_THREAD groups simultaneously
- `PERF_CNT_MUX_CTRL`: L1 counter multiplexer control register

### L1 Mux Control Register

The L1 bank has 8 counter IDs (0–7), but the hardware provides 16 different signals to monitor. The `PERF_CNT_MUX_CTRL` register (bit 4) selects which set of 8 signals maps to those IDs:

| Bit 4 | Counter ID 0–7 Maps To |
|-------|------------------------|
| 0 | NOC Ring 0 transactions, L1 arbitration bundles, unpacker arbitration |
| 1 | NOC Ring 1 transactions, TDMA bundle arbitration, extended unpacker/packer signals |

You must set this mux bit before reading L1 counters to ensure you're reading the intended signals.

### Mode Register Bits

The mode register (`PERF_CNT_*1`) controls several aspects:

| Bits | Field | Description |
|------|-------|-------------|
| 7:0 | mode | Counting behavior (see below) |
| 15:8 | counter_sel | Selects which counter event to read (does not affect what is counted) |
| 16 | req/grant | 0 = read request count, 1 = read grant count |
| 31:17 | reserved | Reserved for future use |

**Counting modes:**

| Mode | Behavior |
|------|----------|
| 0 (Continuous) | Counter runs until explicitly stopped. Maintains reference cycle count in `OUT_L`. |
| 1 (Reference period) | Counter automatically stops after the number of cycles specified in `PERF_CNT_*0`. |
| 2 (Continuous, no ref) | Same as mode 0, but does NOT maintain the reference cycle count. |

**Mode 0 vs Mode 2:** Both run continuously until stopped. The difference is that mode 0 tracks elapsed cycles in `OUT_L` (needed for rate calculations), while mode 2 does not. Mode 2 offers slightly less hardware overhead but loses cycle tracking. **Use mode 0 for most cases** since you typically need both event counts and cycle counts.

**Note:** The reference period register is ignored in modes 0 and 2.

### Requests vs Grants

Bit 16 of the mode register selects which tally to expose when reading `OUT_H`:
- **Requests (bit 16 = 0):** Number of cycles the hardware signal was asserted (attempted operations)
- **Grants (bit 16 = 1):** Number of cycles the operation was actually delivered/completed

This distinction is meaningful for:
- TDMA unpack/pack busy signals (request = attempted, grant = completed transfers)
- NoC NIU ring transactions (request = initiated, grant = accepted)

For counters like instruction issue, stalls, and arbitration pulses, the requests/grants distinction is typically not meaningful—treat those as mode-independent.

### Counter Slots

A "counter slot" refers to one entry in the per-thread L1 configuration buffer. Each slot specifies which bank/counter/mux combination to read out after measurement. The interface supports up to 66 slots per thread, allowing you to capture multiple events in a single measurement window.

## Register Map

The following addresses are used (offsets from `RISCV_DEBUG_REGS_START_ADDR`):

| Register | Offset | Description |
|----------|--------|-------------|
| PERF_CNT_INSTRN_THREAD0 | 0x000 | Reference period |
| PERF_CNT_INSTRN_THREAD1 | 0x004 | Mode |
| PERF_CNT_INSTRN_THREAD2 | 0x008 | Start/Stop |
| PERF_CNT_TDMA_UNPACK0 | 0x00C | Reference period |
| PERF_CNT_TDMA_UNPACK1 | 0x010 | Mode |
| PERF_CNT_TDMA_UNPACK2 | 0x014 | Start/Stop |
| PERF_CNT_FPU0 | 0x018 | Reference period |
| PERF_CNT_FPU1 | 0x01C | Mode |
| PERF_CNT_FPU2 | 0x020 | Start/Stop |
| PERF_CNT_L1_0 | 0x030 | Reference period |
| PERF_CNT_L1_1 | 0x034 | Mode |
| PERF_CNT_L1_2 | 0x038 | Start/Stop |
| PERF_CNT_ALL | 0x03C | Global start/stop |
| PERF_CNT_TDMA_PACK0 | 0x0F0 | Reference period |
| PERF_CNT_TDMA_PACK1 | 0x0F4 | Mode |
| PERF_CNT_TDMA_PACK2 | 0x0F8 | Start/Stop |
| PERF_CNT_OUT_L_INSTRN_THREAD | 0x100 | Cycle count output |
| PERF_CNT_OUT_H_INSTRN_THREAD | 0x104 | Counter value output |
| PERF_CNT_OUT_L_TDMA_UNPACK | 0x108 | Cycle count output |
| PERF_CNT_OUT_H_TDMA_UNPACK | 0x10C | Counter value output |
| PERF_CNT_OUT_L_TDMA_PACK | 0x110 | Cycle count output |
| PERF_CNT_OUT_H_TDMA_PACK | 0x114 | Counter value output |
| PERF_CNT_OUT_L_DBG_L1 | 0x118 | Cycle count output |
| PERF_CNT_OUT_H_DBG_L1 | 0x11C | Counter value output |
| PERF_CNT_OUT_L_FPU | 0x120 | Cycle count output |
| PERF_CNT_OUT_H_FPU | 0x124 | Counter value output |
| PERF_CNT_MUX_CTRL | 0x218 | L1 mux control |

Implementation details are in `tests/helpers/include/counters.h`.

## Events and Counters

### FPU Bank (2 counters)

| ID | Name | Description |
|----|------|-------------|
| 0 | FPU_OP_VALID | Cycles that FPU instructions were issued to the math engine |
| 1 | SFPU_OP_VALID | Cycles that SFPU instructions were issued to the math engine |

Use these to measure compute utilization: `(FPU_OP_VALID + SFPU_OP_VALID) / cycles`.

### INSTRN_THREAD Bank (29 counters)

**Instruction issue counters (IDs 0–7):** Count cycles that specific instruction types were available/issued.

| ID | Name | Description |
|----|------|-------------|
| 0 | INST_CFG | CFG instruction availability |
| 1 | INST_SYNC | SYNC instruction availability |
| 2 | INST_THCON | THCON instruction availability |
| 3 | INST_XSEARCH | XSEARCH instruction availability |
| 4 | INST_MOVE | MOVE instruction availability |
| 5 | INST_MATH | MATH instruction availability (includes FPU/SFPU) |
| 6 | INST_UNPACK | UNPACK instruction availability |
| 7 | INST_PACK | PACK instruction availability |
| 8 | STALLED | Cycles the thread was stalled |

**Stall reason counters (IDs 9–28):** Identify why the thread stalled.

| ID | Name | Description |
|----|------|-------------|
| 9–11 | SRCA_CLEARED_[0-2] | Cycles waiting for srcA cleared per thread |
| 12–14 | SRCB_CLEARED_[0-2] | Cycles waiting for srcB cleared per thread |
| 15–17 | SRCA_VALID_[0-2] | Cycles srcA was valid per thread |
| 18–20 | SRCB_VALID_[0-2] | Cycles srcB was valid per thread |
| 21 | STALL_THCON | Cycles stalled on THCON |
| 22 | STALL_PACK0 | Cycles stalled on packer |
| 23 | STALL_MATH | Cycles stalled on math |
| 24 | STALL_SEM_ZERO | Cycles stalled on semaphore at zero |
| 25 | STALL_SEM_MAX | Cycles stalled on semaphore at max |
| 26 | STALL_MOVE | Cycles stalled on MOVE instruction |
| 27 | STALL_TRISC_REG_ACCESS | Cycles stalled on TRISC register access |
| 28 | STALL_SFPU | Cycles stalled on SFPU |

### TDMA_UNPACK Bank (11 counters)

| ID | Name | Requests Meaning | Grants Meaning |
|----|------|------------------|----------------|
| 0 | MATH_INSTR_SRC_READY | Inst issue not blocked by src_data_ready | Instruction took 4 HF cycles |
| 1 | MATH_NOT_D2A_STALL | Inst issue not stalled by D2A stall | Instruction took 2 HF cycles |
| 2 | MATH_FIDELITY_PHASES | Pipeline stalled due to fidelity cycles | Instruction took 1 HF cycle |
| 3 | MATH_INSTR_BUF_RDEN | Instruction buffer read enable | DMA to SrcB write not blocked |
| 4 | MATH_INSTR_VALID | Instruction valid | DMA to SrcB write not blocked by wr port |
| 5 | TDMA_SRCB_REGIF_WREN | SrcB register write enable | DMA to SrcA write not blocked |
| 6 | TDMA_SRCA_REGIF_WREN | SrcA register write enable | DMA to SrcA write not blocked by wr port |
| 7–10 | UNPACK_BUSY_[0-3] | Unpacker lane busy | Unpacker lane completed transfer |

### TDMA_PACK Bank (8 counters)

| ID | Name | Requests Meaning | Grants Meaning |
|----|------|------------------|----------------|
| 0–3 | DSTAC_RDEN_RAW_[0-3] | Destination register read attempt | Read granted (ready signal high) |
| 4 | PACK_NOT_DEST_STALL | Pack not stalled by dest | Pack instruction not stalled by dest write port |
| 5 | PACK_NOT_SB_STALL | Pack not stalled by scoreboard | Pack instruction not stalled by scoreboard |
| 6 | PACK_BUSY_10 | Packer lane 10 busy | (Grant always 0) |
| 7 | PACK_BUSY_11 | Packer lane 11 busy | (Grant always 0) |

### L1 Bank (16 counters via mux)

**Mux bit 4 = 0:**

| ID | Name | Description |
|----|------|-------------|
| 0 | NOC_RING0_INCOMING_1 | NOC ring 0 incoming channel 1 read/write |
| 1 | NOC_RING0_INCOMING_0 | NOC ring 0 incoming channel 0 read/write |
| 2 | NOC_RING0_OUTGOING_1 | NOC ring 0 outgoing channel 1 read/write |
| 3 | NOC_RING0_OUTGOING_0 | NOC ring 0 outgoing channel 0 read/write |
| 4 | L1_ARB_TDMA_BUNDLE_1 | L1 arbitration for TDMA bundle 1 |
| 5 | L1_ARB_TDMA_BUNDLE_0 | L1 arbitration for TDMA bundle 0 |
| 6 | L1_ARB_UNPACKER | L1 arbitration for unpacker |
| 7 | L1_NO_ARB_UNPACKER | L1 no-arbitration unpacker path |

**Mux bit 4 = 1:**

| ID | Name | Description |
|----|------|-------------|
| 0 | NOC_RING1_INCOMING_1 | NOC ring 1 incoming channel 1 read/write |
| 1 | NOC_RING1_INCOMING_0 | NOC ring 1 incoming channel 0 read/write |
| 2 | NOC_RING1_OUTGOING_1 | NOC ring 1 outgoing channel 1 read/write |
| 3 | NOC_RING1_OUTGOING_0 | NOC ring 1 outgoing channel 0 read/write |
| 4 | TDMA_BUNDLE_1_ARB | TDMA bundle 1 arbitration |
| 5 | TDMA_BUNDLE_0_ARB | TDMA bundle 0 arbitration |
| 6 | TDMA_EXT_UNPACK_9_10 | TDMA extended unpacker interface |
| 7 | TDMA_PACKER_2_WR | TDMA packer 2 write interface to L1 |

## Memory Layout (per TRISC thread)

Configuration and data buffers in L1 (per thread):

| Thread | Config Address | Data Address |
|--------|----------------|--------------|
| UNPACK | 0x2F7D0 (66 words) | 0x2F8D8 (132 words) |
| MATH | 0x2FAE8 (66 words) | 0x2FBF0 (132 words) |
| PACK | 0x2FE00 (66 words) | 0x2FF08 (132 words) |

**Config word encoding (per counter slot):**

| Bit(s) | Field | Description |
|--------|-------|-------------|
| 31 | valid | 1 if this slot is active |
| 17 | l1_mux | L1 mux bit 4 value (only for L1 bank) |
| 16 | mode | 0 = REQUESTS, 1 = GRANTS |
| 15:8 | counter_id | Counter ID within the bank |
| 7:0 | bank_id | Bank ID (0=INSTRN_THREAD, 1=FPU, 2=TDMA_UNPACK, 3=L1, 4=TDMA_PACK) |

**Data area:** After measurement, contains interleaved `(cycles, count)` pairs for each valid slot.

## Interfaces

### C++ API

Location: `tests/helpers/include/counters.h`

**Types:**
- `llk_perf::CounterBank`: Enum with values `INSTRN_THREAD`, `FPU`, `TDMA_UNPACK`, `L1`, `TDMA_PACK`
- `llk_perf::CounterMode`: Enum with values `REQUESTS`, `GRANTS`
- `llk_perf::CounterResult`: Struct with `cycles`, `count`, `bank`, `counter_id`

**Class: `llk_perf::PerfCounters`**

| Method | Description |
|--------|-------------|
| `start()` | Read config from L1 (written by Python) and start all counters |
| `stop()` | Stop counters, scan all slots, write results to L1, return results array |

**RAII wrapper: `llk_perf::ScopedPerfCounters`**

Automatically starts on construction and stops on destruction. Use when Python pre-configures the counters.

**Example (Python-provided configuration):**
```cpp
#include "counters.h"

void run_kernel(const volatile RuntimeParams* params) {
    llk_perf::ScopedPerfCounters scoped;
    // ... kernel work ...
    // Results written to L1 on destruction
}
```

### Python API

Location: `tests/python_tests/helpers/counters.py`

**Functions:**

| Function | Description |
|----------|-------------|
| `configure_counters(location, mode)` | Configure all 66 counters on all threads (UNPACK, MATH, PACK) |
| `read_counters(location)` | Read counter results from all threads |
| `print_counters(results_by_thread)` | Display formatted results |
| `export_counters(results_requests, results_grants, filename, test_params, worker_id)` | Export counter results to CSV |

**Example:**
```python
from helpers.counters import configure_counters, read_counters, print_counters, export_counters

# Configure all 66 counters in REQUESTS mode
configure_counters(location="0,0", mode="REQUESTS")
# ... run kernel ...
results_requests = read_counters(location="0,0")

# Configure all 66 counters in GRANTS mode
configure_counters(location="0,0", mode="GRANTS")
# ... run kernel again ...
results_grants = read_counters(location="0,0")

# Display results
print_counters(results_requests)
print_counters(results_grants)

# Export to CSV
export_counters(results_requests, results_grants, "my_test_counters")
```

## Derived Metrics

Location: `tests/python_tests/helpers/metrics.py`

The metrics module computes higher-level performance indicators from raw counter data. Platform-specific bandwidth parameters are automatically detected based on the connected chip architecture.

### Usage

The metrics functions automatically detect the chip architecture and apply appropriate bandwidth parameters:

```python
from helpers.metrics import print_metrics, export_metrics

# Platform is auto-detected - no configuration needed
# Print formatted metrics to console
print_metrics(results_requests, results_grants)

# Export metrics to CSV
export_metrics(results_requests, results_grants, "my_test_metrics")
```

**Functions:**

| Function | Description |
|----------|-------------|
| `print_metrics(results_requests, results_grants)` | Print kernel metrics to console |
| `export_metrics(results_requests, results_grants, filename, test_params, worker_id)` | Export kernel metrics to CSV |

### Platform Bandwidth Parameters

The module internally maintains bandwidth parameters for each supported architecture:

| Platform | NoC Word (bytes) | Unpacker Peak (B/cyc) | Packer Peak (B/cyc) | Notes |
|----------|------------------|----------------------|---------------------|-------|
| Wormhole B0 | 32 | 80.0 | 80.0 | 256-bit NoC flit; 80 B/cyc from tt-metal perf model |
| Blackhole | 256 | 120.0 | 120.0 | 2048-bit NoC flit; wider bus enables higher bandwidth |
| Quasar | 32 | 80.0 | 80.0 | Placeholder values |

These values are used to convert raw utilization ratios into estimated bytes/cycle throughput.

### Output Interpretation

The metrics output shows both utilization and estimated bandwidth:

| Metric | Example Output |
|--------|----------------|
| Unpacker | `util=0.228 → est ~18.21 B/cyc` |
| Packer | `util=0.382 → est ~30.57 B/cyc` |
| NoC | `45.43 B/cyc (txn/cyc 1.42)` |

- **util** = fraction of cycles the component was busy (0.0–1.0)
- **est B/cyc** = `util × peak_bytes_per_cycle` = estimated actual throughput
- Values below peak indicate headroom; values near peak indicate saturation

### Core Metrics

| Metric | Formula | Description |
|--------|---------|-------------|
| Compute utilization | `(FPU_OP_VALID + SFPU_OP_VALID) / cycles_fpu` | FPU/SFPU activity rate |
| Unpack utilization | `sum(UNPACK_BUSY_[0-3]) / (4 * cycles_unpack)` | Average unpacker lane activity |
| Pack utilization | `(PACK_BUSY_10 + PACK_BUSY_11) / (2 * cycles_pack)` | Average packer lane activity |
| NoC transactions/cycle | `(noc_in + noc_out) / cycles_l1` | L1-NoC interface activity |
| L1 congestion index | `arb_pulses / (arb_pulses + no_arb_pulses)` | Arbitration contention ratio |
| RISC stall rate | `STALLED / cycles_instrn` | Thread stall fraction |

### Bound Classification

The metrics module provides a heuristic classification of the performance bottleneck:

| Classification | Score Based On |
|----------------|----------------|
| math-bound | Compute utilization |
| unpack-bound | Unpack utilization |
| pack-bound | Pack utilization |
| risc-bound | Stall rate minus instruction issue efficiency |

The classification with the highest score is reported as the likely bottleneck.

**Note:** The "compute vs data-movement bound" classification is more relevant for tt-metal where data movement is a significant factor. In the LLK repository context, focus on the individual component bounds (math/unpack/pack/risc).

### Acceptance Ratios

For counters where requests/grants distinction is meaningful:

```
acceptance_ratio = GRANTS / REQUESTS  (or N/A if REQUESTS = 0)
```

This indicates delivery efficiency for TDMA transfers and NoC transactions. A value of 1.0 means no backpressure; values below 1.0 indicate some requests were not granted. If no requests were made, the metric displays as N/A rather than an artificial value.
