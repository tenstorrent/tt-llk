# Performance Evaluation Methodology

## 1. Introduction

This document outlines the goals, metrics, tests, and methodologies for evaluating the performance of LLKs (Low-Level Kernels) on the Tenstorrent Tensix engine.

---

## 2. Performance Evaluation Goals and Scope

The main objectives of LLK performance evaluation are:

- **Quantify HW Utilization:**  
  Provide a quantitative measure of how efficiently LLKs utilize hardware resources compared to their maximum theoretical capability. The hardware design and architecture set the upper performance limits of the Tensix engine and LLKs. The aim is to measure how effectively different LLKs and their specific configurations leverage these resources.

- **Quantify Per-LLK Throughput:**  
  Provide a quantitative measure of each LLK’s throughput for a given set of input parameters. Higher-level software stacks, such as tt-metal or Forge (direct-to-metal approach), are the primary consumers of LLKs. The goal is to deliver an overview of LLK performance implications with respect to input parameters, providing performance-based guidance for LLK usage.

- **Quantify LLK HW Initialization Cost:**  
  Measure the duration of hardware initialization routines, allowing assessment of initialization overhead and enabling more accurate modeling of total operation lenght.

---

## 3. Performance Metrics

- **Cycles per Tile**  
  This universal metric is used for performance evaluation across all LLKs. It is simple and effective, enabling the measurement of memory bandwidth, computational throughput, or simply the length of an LLK. Cycles per tile is easily understood by higher software layers and supports performance estimation at various abstraction levels.

- **Utilization**  
  Utilization is a percentage-based performance metric that quantifies the proportion of a hardware resource's theoretical best-case capacity that is actually used by an LLK. This measurement highlights how efficiently the LLK leverages available hardware, reveals potential bottlenecks, and identifies opportunities for optimization. Utilization is calculated by comparing the measured cycles per tile to the estimated best-case value.

- **Execution Time in Cycles**  
  This metric is especially useful for measuring the complexity of operations that are not "tile-dependent," such as hardware initialization or deinitialization routines.

---

## 4. Best-case Performance Metrics

Best-case performance metrics are determined strictly by hardware architectural limitations (e.g., data bus width, memory interface size, number and size of multipliers in the FPU and similar). These metrics represent theoretical maximums, which are typically only achievable under idealized conditions, without accounting for overheads introduced by software, synchronization mechanisms, or other system factors. For each LLK call, an analytical model of best-case performance based on the input parameters should be provided. This theoretical maximum will serve as a baseline to assess and quantify actual hardware utilization.

---

## 5. Performance Tests

### 5.1 Tensix Programming Model and LLK Placement

Tenstorrent’s programming model assumes that three TRISCs are used to drive different parts of the Tensix engine and perform operations on data stored in L1 memory (results are also stored in L1). Each LLK (i.e., function from the tt-llk repository) is assigned to a specific TRISC processor based on function it should perform and part of Tensix engine it should control:

- **TRISC0 (Unpacker):**  
  Reads data from L1, optionally performs data conversion, and stores data in source registers.

- **TRISC1 (Math):**  
  Moves data from source registers to the FPU/SFPU, executes the mathematical operation, then stores results in destination registers.

- **TRISC2 (Packer):**  
  Reads results from destination registers and writes final output to L1 memory.

A complete L1-to-L1 operation thus requires all three TRISCs to execute code and control their respective subsystems—typically involving three LLKs running on different TRISCs. As TRISCs are asynchronous, additional synchronization ensures correct sequencing and data integrity.

### 5.2 Types of Performance Tests

To meet performance evaluation goals, two types of performance tests should be run:

1. **Isolated Thread Performance Test**  
   Measures hardware utilization of individual LLKs by decoupling synchronization between TRISC cores and running only one LLK on its intended processor. This isolates the LLK under test from inter-TRISC dependencies and synchronization mechanisms.

2. **Operation Performance Test**  
   Measures the full operation sequence, using all three relevant LLKs to execute the complete pipeline (L1-to-L1). This test determines the end-to-end operation length, forming the basis for throughput calculation and capturing operation initialization overhead.

---

## 6. Test Environment

The performance evaluation suite should be a new test in the LLK test infrastructure, employing all available resources to implement performance evaluation for individual LLKs. Performance tests should focus on logical mathematical operations (e.g., matmul, eltwise FPU/SFU) rather than single LLK functions, as this aligns with how higher-level software layers invoke LLKs. Each logical math operation should have dedicated performance evaluation tests that consist of:

1. **Isolated Thread Performance Test** for each LLK involved.
2. **Operation Performance Test** running the operation pipeline end-to-end.

---

## 7. Thread Isolation Test

The goal of thread isolation is to measure the hardware utilization metric by decomposing an operation into its typical three LLK calls and running them independently. The evaluation suite accomplishes thread isolation through the following mechanisms:

- **Using Mock/Empty LLKs for Unpacker and Math:**  
  Hardware-implemented synchronization (using write-ready/data-valid flags) exists between Unpacker and Math. To break this synchronization for testing without affecting LLK functionality, mock LLK instances are used. For example, when evaluating the Unpacker thread, the Math thread runs a mock LLK to continually set the write-ready flag, and vice versa.  
  **TODO:** Verify that use of mock LLKs does not introduce side effects that impact correctness of performance measurements.

- **Disabling SW Synchronization between Packer and Math Threads:**  
  Synchronization here is implemented using software primitives. For isolated testing, simply remove synchronization calls.

---

## 8. Operation Level Test

Operation performance tests simply execute the full sequence of required LLKs to complete the operation under evaluation end to end.

---

## 9. Performance Metric Measurement Infrastructure

To quantitatively assess performance, specific measurement infrastructure should be created. This includes a profiler build that enables placing of timing zones within LLK code to measure specific intervals in cycles. Each test should define time zones to support measuring length of:

1. **Hardware Initialization**
2. **Isolated LLK per Thread** (Unpacker, Math, Packer)
3. **L1-to-L1 (End-to-End) Operation** (from Unpacker start to Packer end)

---

## 10. Parameter Sweep

Each test must define a set of input parameters that will be swept during execution. The parameter sweep should be based on existing LLK functional test configurations to ensure coverage of all possible values for LLK input arguments.  
**TODO:** Verify parameter sweep configurations in LLK functional tests.

---

## 11. Test Iterations per Tile and Running Scheme

To ensure that timing overhead from measurement infrastructure does not distort results, choose the number of operation iterations within a timing zone so that the combined estimated execution time (per best-case model) is at least two orders of magnitude greater than the measurement overhead.  
Additionally, repeat each test sufficiently (10–30 runs) to yield statistically meaningful metrics such as mean, standard deviation, minimum, and maximum values.

---

## 12. Performance Metric Calculation and Reporting

For each combination of test input parameters and test type (isolated thread and operation performance), run 10–30 iterations. The final performance evaluation report for each input set must include:

1. **Hardware Utilization (%)** for each LLK in the operation, relative to the best-case analytical performance model.
2. **End-to-End Operation Duration** (cycles per tile).
3. **Hardware Initialization Duration** (in cycles).


