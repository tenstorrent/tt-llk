# Exponent Histogram

Each packer can create a histogram of the exponents of the datums from `Dst` as they pass through the packer.

## Functional model

Each packer has a copy of the following:

```c
uint8_t ExponentHistogram[32];
uint8_t ExponentHistogramMaxExponent;

void Reset() {
  memset(ExponentHistogram, 0, sizeof(ExponentHistogram));
  ExponentHistogramMaxExponent = 0;
}

void Update(Datum d) {
  uint8_t Exponent = d.Exponent;
  if (ThreadConfig[0].ENABLE_ACC_STATS_Enable
  ||  ThreadConfig[1].ENABLE_ACC_STATS_Enable
  ||  ThreadConfig[2].ENABLE_ACC_STATS_Enable) {
    uint5_t BinNumber = Exponent & 31;
    if (ExponentHistogram[BinNumber] != 255) ExponentHistogram[BinNumber] += 1;
  }
  ExponentHistogramMaxExponent = max(ExponentHistogramMaxExponent, Exponent);
}
```

The `Reset` function is called (for every packer) by the `CLREXPHIST` instruction. The contents of `ExponentHistogram` can be copied to Tensix GPRs using mode 6 or mode 7 of [`SETDMAREG`](../SETDMAREG_Special.md), and packer 0's `ExponentHistogramMaxExponent` can be obtained using mode 9 (it cannot be obtained for packers 1 through 3). The `Update` function is called for every datum fetched from `Dst`, _after_ the early format conversion, edge masking, and ReLU.

The meaning of `d.Exponent` depends on the format produced by the early format conversion:
  * Floating-point types with 5 bits of exponent (FP16, E5M7, E5M6, FP8): the five bits of exponent, in their raw form.
  * Floating-point types with 8 bits of exponent (FP32, TF32, BF16, E8M6): the eight bits of exponent, in their raw form. Note that only the _low_ five of these eight are used for `BinNumber`.
  * INT32: Bitcast from INT32 to FP32, then proceed as above for FP32.
  * INT16: The _low_ eight bits of the integer.
  * INT8: Unspecified.
