# Wait Gate

The Tensix frontend contains one so-called Wait Gate per Tensix thread. It is the final stage of the frontend; once an instruction has passed through the Wait Gate, it is considered to be in the backend rather than the frontend. The entire frontend, up to and including the Wait Gate, is strictly in order: instructions leave the Wait Gate in the exact same order as [baby RISCV cores push them](../BabyRISCV/PushTensixInstruction.md). The sole purpose of the Wait Gate is to control the flow of instructions into the backend, meaning that an instruction will wait at the Wait Gate until the backend is ready to accept it. Reasons that an instruction might be held at it's thread's Wait Gate include:
* The backend execution unit required by the instruction is busy executing a previous instruction.
* Multiple threads are wanting to dispatch an instruction to the same backend execution unit, but the unit can only accept one instruction per cycle (so one thread will dispatch, and the others will wait).
* The previous instruction issued by the thread was a [Scalar Unit (ThCon)](ScalarUnit.md) instruction, and that instruction is still executing in the Scalar Unit (ThCon).
* [`ATGETM`](ATGETM.md): The instruction is wanting to acquire a mutex held by some other thread.
* Many [Matrix Unit (FPU)](MatrixUnit.md) instructions: The instruction consumes data from [`SrcA` or `SrcB`](SrcASrcB.md), but the `AllowedClient` setting of the relevant bank isn't `SrcClient::MatrixUnit` (†).
* The thread recently executed a [`STALLWAIT`](STALLWAIT.md) or [`SEMWAIT`](SEMWAIT.md) instruction, the condition specified by that instruction hasn't yet been met, and the block mask specified by that instruction catches the instruction at the Wait Gate.

> (†) The other side of [`SrcA` and `SrcB`](SrcASrcB.md) `AllowedClient` is [Unpackers](Unpackers/README.md): unlike Matrix Unit (FPU) instructions, [`UNPACR`](UNPACR.md) can _start_ executing regardless of `AllowedClient`, but then waits mid way through execution for `AllowedClient` to be appropriate.

Most of these reasons are fairly self-explanatory, with the exception being [`STALLWAIT`](STALLWAIT.md) and [`SEMWAIT`](SEMWAIT.md). The functional model for these instructions assigns to `WaitGate[CurrentThread].LatchedWaitInstruction`. Logic within the Wait Gate then inspects this state every cycle:
```
if (WaitGate[CurrentThread].LatchedWaitInstruction.Opcode != NOP) {
  if (WaitGate[CurrentThread].LatchedWaitInstruction.BlockMask catches the instruction wanting to pass through WaitGate[CurrentThread]) {
    MakeTheInstructionWaitAtTheGate;
  }
  if (WaitGate[CurrentThread].LatchedWaitInstruction.ConditionMask is fully satisfied) {
    WaitGate[CurrentThread].LatchedWaitInstruction.Opcode = NOP;
  }
}
```