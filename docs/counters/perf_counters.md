# Tensix Performance Counters Guide

This guide documents the Tensix hardware performance counters available in this repository, how to configure and read them from C++ and Python, which useful derived metrics you can compute, and how to test both the interface and the metrics. It consolidates the hardware documentation provided and aligns it with the implemented interfaces found in this repo.

## Overview
- Five counter categories: `INSTRN_THREAD`, `FPU`, `TDMA_UNPACK`, `L1`, `TDMA_PACK`.
- Each category has debug registers for control and two output registers:
  - `PERF_CNT_*0`: reference period (optional)
  - `PERF_CNT_*1`: mode + counter select + request/grant select
  - `PERF_CNT_*2`: start/stop (rising-edge triggered)
  - `PERF_CNT_OUT_L_*`: cycles counted in the measurement window
  - `PERF_CNT_OUT_H_*`: event-specific count for the selected counter
- Special registers:
  - `PERF_CNT_ALL`: start/stop for multiple groups at once
  - `PERF_CNT_MUX_CTRL`: L1 counter mux (bit 4 selects the L1 slice)

Key semantics:
- Mode register bits: `mode[7:0]` selects counting behavior (0 = continuous, 1 = stop at reference period, 2 = continuous w/o ref). Most users use 0 or 1.
- Counter select: `mode[15:8]` chooses which performance event to read.
- Requests vs Grants selection: `mode[16]` (0 = requests, 1 = grants).
- The cycles output (`OUT_L`) is shared per bank window: scanning multiple events within the same bank yields identical `OUT_L` values.

## Register Map (summary)
The following addresses are used (per Tensix debug regs). Implementation specifics are in `tests/helpers/include/counters.h` and hardware docs.

- `FPU`: `PERF_CNT_FPU0/1/2`, `PERF_CNT_OUT_L_FPU`, `PERF_CNT_OUT_H_FPU`
- `L1`: `PERF_CNT_L1_0/1/2`, `PERF_CNT_OUT_L_DBG_L1`, `PERF_CNT_OUT_H_DBG_L1`, `PERF_CNT_MUX_CTRL`
- `INSTRN_THREAD`: `PERF_CNT_INSTRN_THREAD0/1/2`, `PERF_CNT_OUT_L_INSTRN_THREAD`, `PERF_CNT_OUT_H_INSTRN_THREAD`
- `TDMA_UNPACK`: `PERF_CNT_TDMA_UNPACK0/1/2`, `PERF_CNT_OUT_L_TDMA_UNPACK`, `PERF_CNT_OUT_H_TDMA_UNPACK`
- `TDMA_PACK`: `PERF_CNT_TDMA_PACK0/1/2`, `PERF_CNT_OUT_L_TDMA_PACK`, `PERF_CNT_OUT_H_TDMA_PACK`
- Special: `PERF_CNT_ALL` (group start/stop), `PERF_CNT_MUX_CTRL` (L1 mux; bit 4 selects slice)

Note: Empirically, `TDMA_UNPACK` outputs at `0x108/0x10C` (instead of `0x018/0x01C`) work reliably on hardware — this repository's Python helper uses those.

## Events and Counters (highlights)
- `FPU`: `FPU_OP_VALID`, `SFPU_OP_VALID` (pulses when engines issue work)
- `INSTRN_THREAD`: instruction issue counts (`INST_*`) and stall reasons (`STALL_*`)
- `TDMA_UNPACK`: `UNPACK_BUSY_[0..3]` per-lane busy, plus instruction-pipeline derived signals
- `TDMA_PACK`: `PACK_BUSY_10`, `PACK_BUSY_11`, and pack interface gates
- `L1`: NOC ring IN/OUT per ring/slice, arbitration bundles, and no-arbitration signals
- L1 mux: same IDs map to a different set of signals depending on `PERF_CNT_MUX_CTRL` bit 4 (0 or 1). See `helpers/counters.py:COUNTER_NAMES['L1']` for exact mappings.

Requests vs Grants:
- For events like `UNPACK_BUSY_*`, NOC transactions, and pack busy, `requests` reflect attempted operations, `grants` reflect delivered operations. For arbitration pulse counters or high-level stalls, `requests` vs `grants` is typically not meaningful and should be treated as mode-independent.

## Memory Layout (per TRISC thread)
Configuration and data buffers in L1 (per thread):
- UNPACK: config `0x2F7D0` (66 words), data `0x2F8D8` (132 words)
- MATH:   config `0x2FAE8` (66 words), data `0x2FBF0` (132 words)
- PACK:   config `0x2FE00` (66 words), data `0x2FF08` (132 words)

Config word encoding (per counter slot):
- Bits: `[valid(31), mux_ctrl_bit4(17), mode(16), counter_id(8..15), bank(0..7)]`
- Up to 66 counters per thread (unused slots are zeroed).
- Data area returns interleaved per-counter results: `(cycles, count)` per valid slot.

## Interfaces

### How Everything Works
This section describes, end-to-end, how we configure, measure, read, and present hardware performance counters—from Python host code to kernel C++ and back. It is meant to be sufficient for someone to reimplement or extend the flow.

1) Responsibilities (who does what)
- Python (`tests/python_tests/helpers/counters.py`):
  - Defines friendly names for banks and events (including L1 mux-specific names).
  - Encodes the requested set of counters into per-thread L1 config buffers (up to 66 slots) and clears corresponding L1 data buffers.
  - After the kernel run, reads metadata + data from L1, decodes each result (bank/event/mode/mux), and prints or feeds into metrics.
- C++ kernel (`tests/helpers/include/counters.h` and `tests/sources/*.cpp`):
  - If the kernel selects counters itself, call `add()` then `configure()` to write metadata into L1 before `start()`.
  - `PerfCounters.start()` reads the thread’s L1 config, adopts the mode bit from L1 metadata if present (Python-driven), programs Tensix debug registers (`counter_sel`, req/grant selection via the adopted mode, and L1 mux), and issues start pulses.
  - `PerfCounters.stop()` stops counting, scans configured slots, sets read-out selection (`counter_sel` + req/grant + L1 mux), reads `OUT_L/OUT_H`, and writes `(cycles,count)` pairs back to the thread’s L1 data buffer in slot order.

2) Buffers and encodings in L1 (per thread: UNPACK, MATH, PACK)
- Config buffer (66 words): each slot has a 32-bit word with fields `[valid(31), mux_ctrl_bit4(17), mode(16), counter_id(8..15), bank(0..7)]`.
- Data buffer (132 words): after `stop()`, contains `2 * valid_count` words: for slot i, `data[2*i] = cycles`, `data[2*i+1] = count`.
- Distinct base addresses per thread are defined in both C++ and Python; Python writes first, C++ reads same regions.

3) Start—how measurement begins (C++)
- Read config: determine active slots (bit31 set) and total `valid_count`.
- Program per-bank controls:
  - If a slot targets `L1`, set `PERF_CNT_MUX_CTRL` bit 4 to match that slot’s `mux_ctrl_bit4` before interacting with those IDs.
  - Setup `PERF_CNT_<BANK>1`’s low 8 mode bits as needed (typically 0 for continuous window). Note: `counter_sel` written during `start()` does not limit what is counting; it only affects read-out unless mode 1 (ref-period gated) is used.
  - Mode (`REQUESTS`/`GRANTS`) is global and adopted from L1 metadata (bit 16) if present; otherwise it defaults to `GRANTS`.
- Issue start: rising edge on `PERF_CNT_<BANK>2` bit 0 or use `PERF_CNT_ALL` for supported groups. Counting across all events in a group proceeds concurrently.

4) Counting semantics (hardware)
- Groups count in parallel between start and stop. You do not need to “switch” the active event while counting.
- The per-bank cycles (`OUT_L`) represent the same measurement window for all events in that group; values are typically identical when scanning multiple event IDs in the same bank.
- Requests vs Grants: bit 16 of the mode register selects which accumulated tally `OUT_H` exposes at read time (see “Requests vs Grants” above). It does not change what was counted—only which tally you read.
- L1 mux: `PERF_CNT_MUX_CTRL` bit 4 changes which signals each ID maps to (see L1 notes above). Ensure the mux matches the IDs you are scanning.

5) Stop—how readout happens (C++)
- Freeze: pulse stop (0→1 on bit 1) per bank (or via `PERF_CNT_ALL`). Counters hold their values.
- Scan slots in config order:
  - Decode `bank`, `counter_id`, `mode bit16`, and optionally `mux_ctrl_bit4` for L1.
  - If L1, set `PERF_CNT_MUX_CTRL` bit 4 accordingly.
  - Program read-out selection by writing `PERF_CNT_<BANK>1 = (req_grant<<16) | (counter_id<<8) | mode[7:0]`.
  - Read `PERF_CNT_OUT_L_<BANK>` (cycles) and `PERF_CNT_OUT_H_<BANK>` (event count for the selected ID and req/grant).
  - Write the pair `(cycles, count)` into the L1 data buffer at the next position.
- Result: the thread’s L1 data buffer holds a dense array of `(cycles,count)` pairs for exactly the active slots, in the same order as the config.

6) Host readback and presentation (Python)
- Read the 66-word config to learn the slot definitions and `valid_count`.
- Read exactly `valid_count * 2` words from the data buffer.
- For slot i: map `(cycles, count)` to the decoded `bank`, `counter_id`, `mode`, and `mux_ctrl_bit4` (if L1), and attach a friendly `counter_name`.
- For display:
  - Group by bank, show the shared cycles and each counter’s raw count and rate (`count / cycles`).
  - Show the mode as `requests` or `grants`. For acceptance-style insights, compare `requests` vs `grants` across a separate pass, or scan both modes.

7) Reading many counters via `counter_sel`
- Because all events in a group accumulate in parallel, you can read multiple events by changing only `counter_sel` (and bit 16 if you need requests vs grants) and then sampling `OUT_H` for each selection.
- Practically:
  - Stop first to snapshot stable values.
  - For each event ID: write `PERF_CNT_<BANK>1` with that `counter_id` (and desired bit 16), then read `OUT_H`. Read `OUT_L` once per bank (shared window).
  - For L1, remember to set `PERF_CNT_MUX_CTRL` bit 4 to the intended slice before scanning IDs from that slice.
- The C++ `stop()` method automates this scan based on the L1 config slots, then persists the results into the L1 data buffer for Python.

8) Derived metrics and further analysis
- Python’s `helpers/metrics.py` computes utilizations (compute/unpack/pack), NoC activity and bytes/cycle, L1 congestion, RISC stalls, bound classification, and acceptance ratios by comparing REQUESTS vs GRANTS for meaningful counters (TDMA busy, NIU ring IN/OUT).
- Use `summarize_kernel_metrics_dual` to compare REQUESTS vs GRANTS runs and identify pressure vs delivered throughput.

9) Gotchas and best practices
- Snapshot before scanning: it’s legal to read while running, but stop+scan yields a consistent snapshot.
- Bank cycles are shared: expect identical `OUT_L` when scanning multiple IDs in a bank.
- Not all counters have meaningful requests/grants semantics (e.g., arbitration pulses, instruction issue, high-level stalls). Treat those as mode-independent when interpreting results.
- Respect per-thread limits: ≤66 slots per thread; clear the data region before reusing; keep L1 mux consistent when scanning L1 events.

### C++ API (`tests/helpers/include/counters.h`)
Types:
- `llk_perf::CounterBank`: `{INSTRN_THREAD, FPU, TDMA_UNPACK, L1, TDMA_PACK}`
- `llk_perf::CounterMode`: `{REQUESTS, GRANTS}`
- `llk_perf::CounterResult`: `{cycles, count, bank, counter_id}`

Constants:
- `llk_perf::COUNTER_BANK_COUNT`: number of banks (5).
- `llk_perf::COUNTER_SLOT_COUNT`: max counters per thread (66).

Event IDs (C++):
- `llk_perf::counter_id::<bank>` namespaces with snake_case names, e.g. `counter_id::fpu::FPU_OP_VALID`, `counter_id::l1::NOC_RING0_INCOMING_1`.

Class: `llk_perf::PerfCounters`
- `add(CounterBank bank, uint8_t counter_id, uint8_t l1_mux = 0) -> bool`: Add a counter (returns false if capacity reached). Only L1 uses `l1_mux` (maps to mux bit 4).
- `configure()`: Write the current in-memory selection (from `add()`) into the thread’s L1 config buffer; encodes the current global mode bit into metadata.
- `start()`: Reads configuration from L1 (written by Python or via `configure()`), adopts mode from metadata if present, counts how many valid slots, applies mux, and starts the counters.
- `stop() -> std::array<CounterResult, COUNTER_SLOT_COUNT>`: Stops counters and returns a fixed-size array of results. Only the first `size()` entries are populated; use `size()` to know how many are valid.
- `size() const -> uint32_t`: Number of active counters.
- `empty() const -> bool`: Whether any counters are configured.
- `get_count() const -> uint32_t`: Alias for `size()`.

Mode handling:
- Global mode is adopted from the first valid L1 metadata entry during `start()`. If no metadata is present, the default is `GRANTS`.
- `configure()` encodes the current global mode into metadata; the class defaults to `GRANTS`. There is no per-counter mode.

Notes:
- If configuration was written by Python into the L1 buffers, you can skip `add()`/`configure()` and just call `start()`/`stop()`.
- `start()`/`stop()` act on entire groups per bank semantics.

For end-to-end mechanics (register programming, readout, and L1 write-back), see “How Everything Works”.

Example A (kernel side, Python-provided configuration):
```cpp
#include "counters.h"

void run_kernel(const volatile RuntimeParams* params) {
    llk_perf::PerfCounters counters;
    counters.start();
    // ... your kernel work ...
    counters.stop();
}
```

Example B (kernel side, kernel-provided configuration):
```cpp
#include "counters.h"

using namespace llk_perf;

void run_kernel(const volatile RuntimeParams* params) {
  PerfCounters counters; // defaults to GRANTS mode
  counters.add(CounterBank::FPU, counter_id::fpu::FPU_OP_VALID);
  counters.add(CounterBank::L1, counter_id::l1::NOC_RING0_OUTGOING_0, /*l1_mux=*/0);
  counters.configure(); // writes metadata (including mode) to L1

  counters.start();
  // ... your kernel work ...
  counters.stop(); // writes (cycles,count) pairs to L1 data
}
```

Migration note:
- If older code relied on `add()` or `start()` implicitly writing metadata, call `configure()` after your `add()` calls. `start()` now expects configuration to already be present in L1 (from Python or `configure()`).
- `set_mode()` was removed. Mode is encoded in metadata and adopted from L1 during `start()`; the default is `GRANTS` when the kernel provides configuration.

### Python API (`tests/python_tests/helpers/counters.py`)
Functions:
- `counter(bank: str, counter_name: str, mux_ctrl_bit4: int = 0) -> Dict`: Selects a counter by name (supports L1 mux).
- `configure_perf_counters(counters: List[Dict], location: str = "0,0", thread: str = "MATH", mode: str = "GRANTS") -> None`:
  - Encodes 66 config words into L1 for the specified thread (`UNPACK`, `MATH`, `PACK`).
  - Clears the corresponding data area (132 words) to avoid stale results.
  - `mode` controls `REQUESTS` vs `GRANTS` via bit 16.
- `read_perf_counters(location: str = "0,0", thread: str = "MATH") -> List[Dict]`:
  - Reads metadata (66 words), counts valid slots, reads only valid `(cycles,count)` pairs.
  - Decodes bank, counter id, mode, L1 mux, counter names.
- `print_perf_counters(results: List[Dict], thread: str = None) -> None`:
  - Prints per-bank groups, shared cycles diagnostics, and per-counter count/rate, with `mode` shown simply as `requests` or `grants`.

Result schema (`read_perf_counters`):
```python
{
  "bank": "FPU" | "L1" | "INSTRN_THREAD" | "TDMA_UNPACK" | "TDMA_PACK",
  "counter_name": "...",
  "counter_id": int,
  "cycles": int,   # shared per-bank window
  "count": int,    # event-specific
  "mode": "REQUESTS" | "GRANTS",
  "mux_ctrl_bit4": 0 | 1 | None
}
```

Example usage:
```python
from helpers.counters import configure_perf_counters, read_perf_counters, print_perf_counters, counter

# Select counters (example subset)
all_counters = [
    counter("FPU", "FPU_OP_VALID"),
    counter("FPU", "SFPU_OP_VALID"),
    counter("TDMA_UNPACK", "UNPACK_BUSY_0"),
    counter("L1", "NOC_RING0_INCOMING_1", mux_ctrl_bit4=0),
    # ... add more as needed ...
]

# Configure for a thread in REQUESTS mode
configure_perf_counters(all_counters, location="0,0", thread="MATH", mode="REQUESTS")

# Run your workload (kernel)
# ...

# Read and print
results = read_perf_counters(thread="MATH")
print_perf_counters(results, thread="MATH")
```

For the full configuration, readback, and display flow across threads, see “How Everything Works”.

## Useful Derived Metrics
Implemented in `tests/python_tests/helpers/metrics.py`.

Core helpers:
- Rate: $\text{rate} = \frac{\text{count}}{\text{cycles}}$.
- Aggregations sum relevant event counts by bank and name.

Per-thread metrics (selected):
- Compute utilization: $\frac{\text{FPU\_OP\_VALID} + \text{SFPU\_OP\_VALID}}{\text{cycles\_fpu}}$.
- Unpack utilization: $\frac{\sum_{i=0}^{3} \text{UNPACK\_BUSY\_i}}{\max(1, 4 \cdot \text{cycles\_unpack})}$.
- Pack utilization: $\frac{\text{PACK\_BUSY\_10} + \text{PACK\_BUSY\_11}}{\max(1, 2 \cdot \text{cycles\_pack})}$.
- NoC transactions per cycle: $\frac{\text{noc\_in} + \text{noc\_out}}{\text{cycles\_l1}}$ from both L1 mux slices.
- NoC bytes per cycle: $\text{txn\_per\_cycle} \times \text{noc\_word\_bytes}$ if provided.
- L1 congestion index: $\frac{\text{arb\_pulses}}{\max(1, \text{arb\_pulses} + \text{no\_arb\_pulses})}$.
- RISC stall and instruction issue rates: per `INSTRN_THREAD` counters.

Bound classification (heuristic):
- Scores: `math` = compute util; `unpack` = unpack util; `pack` = pack util; `risc` = max(stall, trisc/move/thcon stalls) minus 0.5× instruction issue.
- Top score among `{math-bound, unpack-bound, pack-bound, risc-bound}` identifies likely bottleneck.

Acceptance ratios (REQUESTS vs GRANTS):
- Where meaningful (TDMA busy, NIU ring transactions), compute $\text{acceptance} = \frac{\text{GRANTS}}{\max(1, \text{REQUESTS})}$ to estimate delivery vs pressure.
- Arbitration and stall counters are not used for acceptance.

Kernel-level dual summary:
- `summarize_kernel_metrics_dual` compares REQUESTS vs GRANTS across threads and reports:
  - Average compute/unpack/pack utilization, L1 congestion, NoC activity.
  - Compute vs data-movement bound classification.
  - Dominant component bound (math/unpack/pack/risc).
  - Acceptance ratios for TDMA and NIU (where meaningful).

## How to Use (end-to-end)
See `tests/python_tests/test_matmul.py` for a complete flow:
1. Configure all counters for `UNPACK`, `MATH`, `PACK` in `REQUESTS` mode.
2. Run the kernel once; read and print counters.
3. Reconfigure counters in `GRANTS` mode.
4. Run the kernel again; read and print counters.
5. Produce a dual-mode derived summary with `summarize_kernel_metrics_dual`.

## Testing Strategy

### Unit testing the interface (Python)
- Mock `ttexalens.tt_exalens_lib.write_words_to_device` and `read_words_from_device` to validate:
  - Config encoding: bit 31 (valid), bit 17 (L1 mux), bit 16 (mode), bits 8–15 (counter id), bits 0–7 (bank id).
  - Data clearing: 132 zero words written before counting.
  - Reading logic: only reads `valid_count * 2` words; correctly decodes mode and mux.
- Verify `print_perf_counters` output formatting:
  - Shared cycles diagnostic for a bank.
  - Per-counter rates and mode labels as `requests`/`grants`.

### Unit testing the interface (C++)
- Minimal kernels (as in `tests/sources/matmul_test.cpp`) that:
  - Call `PerfCounters.start()` before work and `PerfCounters.stop()` after.
  - Ensure `get_count()` matches the number of valid L1 slots.
  - Optionally, compare `CounterResult` arrays against expected sizes and monotonicity (non-negative counts).

### Unit testing the metrics
- Construct synthetic `results` arrays (matching `read_perf_counters` schema) and validate:
  - `compute_thread_metrics` returns expected rates for: compute, unpack, pack, NoC, congestion.
  - Edge cases: zero cycles (rates clamp to 0), missing counters (sum-zero), L1 mux mixing.
  - Bound classification picks the correct top category under controlled inputs.
  - Acceptance ratio logic divides GRANTS by REQUESTS for the correct categories only.

### Integration testing
- Use `pytest` to run `tests/python_tests/test_matmul.py`:
```bash
pytest -q tests/python_tests/test_matmul.py::test_matmul
```
- Confirms counters configured/read across REQUESTS and GRANTS, derived summary printed, and numerical results match golden tensors.

## Limitations and Pitfalls
- Shared cycles per bank: `OUT_L` reflects the bank window and will be identical across selections in the same bank. Always compute rates using the matching bank cycles.
- Not all counters are meaningful in `REQUESTS` vs `GRANTS`: arbitration pulses, high-level stalls, and instruction issue counts should be treated as mode-independent.
- Some legacy counters documented for removal exist in older lists; rely on repository mappings (`helpers/counters.py`) for current, supported events.
- Address quirks: `TDMA_UNPACK` outputs are read at `0x108/0x10C` in practice.
- Capacity: up to 66 counters per thread; configuration must not exceed this.
- Start/Stop affects entire groups per bank: changing `counter_sel` in mode register controls only which event is read out, not which events are counted.

## Quick Reference
- `requests` vs `grants`: use `grants` for utilization “truth” and `requests` for “pressure”; compute acceptance where appropriate.
- L1 mux control: `PERF_CNT_MUX_CTRL` bit 4 selects between slices; ensure your counter selections match the intended mux.
- Derived formulas:
  - Rate: $\text{count}/\text{cycles}$
  - Unpack util: $\sum \text{busy}/(4\cdot \text{cycles})$
  - Pack util: $\sum \text{busy}/(2\cdot \text{cycles})$
  - L1 congestion: $\text{arb}/(\text{arb}+\text{no\_arb})$
  - Acceptance: $\text{GRANTS}/\text{REQUESTS}$ (TDMA, NIU only)

## Examples
- Python configuration and reading: `tests/python_tests/test_matmul.py`
- C++ kernel usage of `PerfCounters`: `tests/sources/matmul_test.cpp`
- Counter name mappings and bank IDs: `tests/python_tests/helpers/counters.py`
- Derived metrics implementation: `tests/python_tests/helpers/metrics.py`
