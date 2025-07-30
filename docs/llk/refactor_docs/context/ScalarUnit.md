# Scalar Unit (ThCon)

The Scalar Unit can perform various operations on the Tensix GPRs of the issuing thread. The Configuration Unit accesses these same GPRs, and they are also mapped into the RISCV address space. They are not used by any other backend units.

The Scalar Unit can accept at most one instruction per cycle, and usually takes multiple cycles to complete each instruction. Until it has completed that instruction:
* No instructions from any other thread can pass through their Wait Gate and enter the Scalar Unit. In other words, the Scalar Unit is executing at most one instruction at a time, and has no internal pipelining.
* No instructions of any kind from the issuing thread can pass through its Wait Gate. In other words, once a thread has started executing a Scalar Unit instruction, it cannot start executing its next instruction until the Scalar Unit instruction completes, regardless of which unit that next instruction executes in. Note that the converse is not true: a thread can usually start executing a Scalar Unit instruction _before_ prior instructions at other units have finished executing.

The instructions executed by the Scalar Unit are:

|Instruction(s)|Number of cycles required for execution|
|---|---|
|[`DMANOP`](DMANOP.md)|1|
|[`SETDMAREG`](SETDMAREG.md)|1|
|[`REG2FLOP`](REG2FLOP.md)|≥ 2|
|[`FLUSHDMA`](FLUSHDMA.md)|≥ 2|
|[`ADDDMAREG`](ADDDMAREG.md), [`CMPDMAREG`](CMPDMAREG.md), [`MULDMAREG`](MULDMAREG.md), [`SUBDMAREG`](SUBDMAREG.md)|3 or 4|
|[`BITWOPDMAREG`](BITWOPDMAREG.md), [`SHIFTDMAREG`](SHIFTDMAREG.md)|3 or 4|
|[`STOREIND`](STOREIND.md), [`STOREREG`](STOREREG.md), [`ATSWAP`](ATSWAP.md)|≥ 3|
|[`LOADIND`](LOADIND.md), [`LOADREG`](LOADREG.md), [`ATINCGET`](ATINCGET.md)|≥ 3 (†)|
|[`ATCAS`](ATCAS.md), [`ATINCGETPTR`](ATINCGETPTR.md)|≥ 15|

(†) Occupies the Scalar Unit for ≥ 3 cycles, though the load result isn't delivered until some time later.

## GPRs

The descriptions of various Scalar Unit and Configuration Unit instructions assume the following global variable representing the Scalar Unit's 192 GPRs:

```c
uint32_t GPRs[3][64];
```

The `[3]` is always indexed as `[CurrentThread]`, i.e. each Tensix thread has 64 GPRs, and each thread has exclusive access to its own GPRs (there is no cross-thread access). Some instructions access an aligned group of four GPRs, and some instructions can access either the low 16-bit or high 16-bit part of a GPR.

## RISCV access to GPRs

RISCV B has the entire `GPRs` array mapped into its address space, starting at address `REGFILE_BASE`. Meanwhile, each RISCV T<sub>i</sub> has the `GPRs[i]` sub-array mapped into its address space, again starting at address `REGFILE_BASE`.

If a RISCV core writes to a Tensix GPR immediately prior to pushing a Tensix instruction which consumes that GPR, there is _usually_ enough latency in the instruction path for the GPR write to complete before the instruction starts executing. However, to guarantee race-free operation, one of the following approaches is recommended:
* Push a pair of [`SETDMAREG`](SETDMAREG_Immediate.md) instructions (each writing 16 bits) instead of writing 32 bits directly to `GPRs`.
* Push a [`STALLWAIT`](STALLWAIT.md) instruction after writing to `GPRs` and before pushing the instruction of interest, where the `STALLWAIT` specifies condition C13 and specifies a block mask which will cause the instruction of interest to wait for that condition.
* Use RISCV [memory ordering](../BabyRISCV/MemoryOrdering.md) techniques to ensure that the write to `GPRs` has _finished_ before the write to push a Tensix instruction _starts_. One possible sequence of instructions for this is:
```
sw t0, 0(t1)   # Write to Tensix GPR
lw t2, 0(t1)   # Read back from same address; guaranteed to finish after the store finishes
addi x0, t2, 0 # Consume result of load; guarantees subsequent instructions cannot begin until load finishes
sw t3, 0(t4)   # Push Tensix instruction
```

If a RISCV core wants to execute a Tensix instruction which writes to GPRs, and then observe the effect of that instruction by reading from `REGFILE_BASE`, it needs to be aware that RISCV execution and Tensix execution proceed asynchronously from each other: once a Tensix instruction has been pushed, it will _eventually_ execute, but a variable number of RISCV instructions might execute before the Tensix instruction executes.
