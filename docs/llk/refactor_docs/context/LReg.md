# `LReg`

Vector Unit (SFPU) instructions assume the presence of the following global variable:

```c
union {uint32_t u32; int32_t i32; float f32;} LReg[17][32];
```

The `[17]` is not entirely uniform:
<table><tr><th><code>LReg[0]</code><br/>...<br/><code>LReg[7]</code></th><td>Each of these is 32 lanes of 32b. Per the <em>lane layout</em> section below, the 32 lanes are often viewed as a 4x8 grid.</td></tr>
<tr><th><code>LReg[8]</code></th><td>Read-only, all lanes contain the floating-point value <code>0.8373</code>.</td></tr>
<tr><th><code>LReg[9]</code></th><td>Read-only, all lanes contain the value zero (identical bit pattern for all data types).</td></tr>
<tr><th><code>LReg[10]</code></th><td>Read-only, all lanes contain the floating-point value <code>1.0</code>.</td></tr>
<tr><th><code>LReg[11]</code><br/>...<br/><code>LReg[14]</code></th><td>Each of these is 32 lanes of 32b, but can only be written using the <a href="SFPCONFIG.md"><code>SFPCONFIG</code></a> instruction, which takes 8 lanes of <code>LReg[0]</code> and broadcasts them up to 32 lanes. As such, each of these is often described as 8 lanes of 32b. Furthermore, if using the SFPI compiler, the compiler reserves <code>LReg[11]</code> to contain the floating-point value <code>-1.0</code> in all lanes.</td></tr>
<tr><th><code>LReg[15]</code></th><td>Read-only, lane <code>i</code> contains the integer value <code>i*2</code> (i.e. 0, 2, 4, ..., 62).</td></tr>
<tr><th><code>LReg[16]</code></th><td>This is 32 lanes of 32b, but is only writable by instructions scheduled via <a href="SFPLOADMACRO.md"><code>SFPLOADMACRO</code></a>, and is only readable by <code>SFPSTORE</code> instructions scheduled via <a href="SFPLOADMACRO.md"><code>SFPLOADMACRO</code></a>.</td></tr></table>

## Data types

Each datum in `LReg` is 32 bits wide, holding one of:
* **FP32 (1 sign bit, 8 bit exponent, 23 bit mantissa)**. This is what most CPU architectures call `float`. This is the same format as [`Dst32b`](Dst.md)'s FP32, albeit `LReg` operations get _closer_ to being fully IEEE754 compliant - see [floating-point bit patterns](FloatBitPatterns.md) for details.
* **Unsigned 32-bit integer**. This is what all CPU architectures call `uint32_t`.
* **Signed 32-bit integer in two's complement form**. This is what most CPU architectures call `int32_t`.
* **Signed 32-bit integer consisting of 1 sign bit and 31 magnitude bits**. This is the same format as [`Dst32b`](Dst.md)'s Integer "32".

Software is permitted to implicitly `bitcast` between any of these types when they are in `LReg`. Notably, all of these integer types have identical bit representations for all non-negative values between `0` and <code>2<sup>31</sup>-1</code>, all signed types have their sign bit in the most significant bit, and FP32 has the conventional IEEE754 layout with the mantissa in the low 23 bits.

## Lane layout

Each `LReg[i]` contains 32 lanes of 32b, though it is often useful to consider those 32 lanes as a 4x8 grid rather than a 1x32 grid. It makes no difference to instructions which operate purely lanewise, but instructions which involve cross-lane data movement tend to involve either horizontal or vertical movement in the 4x8 grid:

<table><tr><td>Lane 0</td><td>Lane 1</td><td>Lane 2</td><td>Lane 3</td><td>Lane 4</td><td>Lane 5</td><td>Lane 6</td><td>Lane 7</td></tr>
<tr><td>Lane 8</td><td>Lane 9</td><td>Lane 10</td><td>Lane 11</td><td>Lane 12</td><td>Lane 13</td><td>Lane 14</td><td>Lane 15</td></tr>
<tr><td>Lane 16</td><td>Lane 17</td><td>Lane 18</td><td>Lane 19</td><td>Lane 20</td><td>Lane 21</td><td>Lane 22</td><td>Lane 23</td></tr>
<tr><td>Lane 24</td><td>Lane 25</td><td>Lane 26</td><td>Lane 27</td><td>Lane 28</td><td>Lane 29</td><td>Lane 30</td><td>Lane 31</td></tr></table>
