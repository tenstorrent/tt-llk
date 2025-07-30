# `SFPCOMPC` (Conditionally invert vector conditional execution state)

**Summary:** Used to implement `else` when a compiler maps SIMT `if` / `else` onto the vector conditional execution stack.

**Backend execution unit:** [Vector Unit (SFPU)](VectorUnit.md), simple sub-unit

## Syntax

```c
TT_SFPCOMPC(0, 0, /* u4 */ VD, 0)
```

## Encoding

![](../../../Diagrams/Out/Bits32_SFPCOMPC.svg)

## Functional model

```c
lanewise {
  if (VD < 12 || LaneConfig.DISABLE_BACKDOOR_LOAD) {
    // Peek at stack top.
    FlagStackEntry Top;
    if (FlagStack.IsEmpty()) {
      Top.UseLaneFlagsForLaneEnable = true;
      Top.LaneFlags = true;
    } else {
      Top = FlagStack.Top();
    }

    // Invert LaneFlags, subject to Top.
    if (Top.UseLaneFlagsForLaneEnable && UseLaneFlagsForLaneEnable) {
      LaneFlags = Top.LaneFlags && !LaneFlags;
    } else {
      LaneFlags = false;
    }
  }
}
```
