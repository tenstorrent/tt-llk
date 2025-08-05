Matrix multiplication formula is: 

`𝑐𝑖𝑗=𝑎𝑖1x𝑏1𝑗+𝑎𝑖2x𝑏2𝑗+…+𝑎𝑖𝑛x𝑏𝑛𝑗`
`cij=ai1xb1j+ai2xb2j+…+ainxbnj`

Ex. To calculate `c22` in 5x5 matrix, we need to multiply second row of matrix A with second column of matrix B.

Let us look at example of multiplication `[2,3] x [3,2]`

Given Matrix A (2×3), Matrix B (3×2), Then the result of A × B is matrix C (2×2)::
```
[ a₁₁  a₁₂  a₁₃ ]     [ b₁₁  b₁₂ ]     [ c₁₁  c₁₂ ]
[ a₂₁  a₂₂  a₂₃ ]  ×  [ b₂₁  b₂₂ ]  =  [ c₂₁  c₂₂ ]
                      [ b₃₁  b₃₂ ]
```

```
### Element-wise Calculations for C = A × B

- **c₁₁** = a₁₁·b₁₁ + a₁₂·b₂₁ + a₁₃·b₃₁
- **c₁₂** = a₁₁·b₁₂ + a₁₂·b₂₂ + a₁₃·b₃₂
- **c₂₁** = a₂₁·b₁₁ + a₂₂·b₂₁ + a₂₃·b₃₁
- **c₂₂** = a₂₁·b₁₂ + a₂₂·b₂₂ + a₂₃·b₃₂
```

We can write it differently, to calculate matrix in three iterations.

```### Step-by-step calculation of matrix elements

Initialize:

- **c₁₁** = a₁₁ · b₁₁
- **c₁₂** = a₁₁ · b₁₂
- **c₂₁** = a₂₁ · b₁₁
- **c₂₂** = a₂₁ · b₁₂

Add second terms:

- **c₁₁** = c₁₁ + a₁₂ · b₂₁
- **c₁₂** = c₁₂ + a₁₂ · b₂₂
- **c₂₁** = c₂₁ + a₂₂ · b₂₁
- **c₂₂** = c₂₂ + a₂₂ · b₂₂

Add third terms:

- **c₁₁** = c₁₁ + a₁₃ · b₃₁
- **c₁₂** = c₁₂ + a₁₃ · b₃₂
- **c₂₁** = c₂₁ + a₂₃ · b₃₁
- **c₂₂** = c₂₂ + a₂₃ · b₃₂
```

We need only 2 elements from A and B to perform one iteration, which is ideal for streaming, and does not require too much memory. We are accumulating

```
### Stepwise calculation of \( c_{11}, c_{12}, c_{21}, c_{22} \):

---

#### First, using \( a_{11}, a_{21} \) and \( b_{11}, b_{12} \):

\[
\begin{aligned}
c_{11} &= a_{11} \cdot b_{11} \\
c_{12} &= a_{11} \cdot b_{12} \\
c_{21} &= a_{21} \cdot b_{11} \\
c_{22} &= a_{21} \cdot b_{12}
\end{aligned}
\]

---

#### Then, using \( a_{12}, a_{22} \) and \( b_{21}, b_{22} \):

\[
\begin{aligned}
c_{11} &= c_{11} + a_{12} \cdot b_{21} \\
c_{12} &= c_{12} + a_{12} \cdot b_{22} \\
c_{21} &= c_{21} + a_{22} \cdot b_{21} \\
c_{22} &= c_{22} + a_{22} \cdot b_{22}
\end{aligned}
\]

---

#### Finally, using \( a_{13}, a_{23} \) and \( b_{31}, b_{32} \):

\[
\begin{aligned}
c_{11} &= c_{11} + a_{13} \cdot b_{31} \\
c_{12} &= c_{12} + a_{13} \cdot b_{32} \\
c_{21} &= c_{21} + a_{23} \cdot b_{31} \\
c_{22} &= c_{22} + a_{23} \cdot b_{32}
\end{aligned}
\]
```

```cpp
int c[4]; 
int a[2]; 
int b[2]; 
for (int i = 0; i < 4; i++) c[i]= 0; 
for (int i = 0; i < 3; ++i) { 
   get_next_two_elements(a); // elements are received in following order
a11, a21, a12,a22, a13,a23a11, a21, a12,a22, a13,a23

   get_next_two_elements(b); // elements are received in following order
b11, b12, b21,b22, b31,b32b11, b12, b21,b22, b31,b32

   for (int j = 0; j < 2; j++) 
    for (int k = 0; k < 2; k++) 
      c[j*2+k] = c[j*2+k] + a[k] * b[j]  

}
```

Values from vector A are used in the following order `𝒂𝟏𝟏, 𝒂𝟐𝟏, 𝒂𝟏𝟐,𝒂𝟐𝟐, 𝒂𝟏𝟑,𝒂𝟐𝟑𝒂𝟏𝟏, 𝒂𝟐𝟏, 𝒂𝟏𝟐,𝒂𝟐𝟐, 𝒂𝟏𝟑,𝒂𝟐𝟑`, which is in memory representation of matrix A transposed!

Values from vector B are used in the following order `𝒃𝟏𝟏, 𝒃𝟏𝟐, 𝒃𝟐𝟏,𝒃𝟐𝟐, 𝒃𝟑𝟏,𝒃𝟑𝟐𝒃𝟏𝟏, 𝒃𝟏𝟐, 𝒃𝟐𝟏,𝒃𝟐𝟐, 𝒃𝟑𝟏,𝒃𝟑𝟐`, which is in memory representation of matrix B.
### Matmul implementation using tiles

Our chip can multiply two 32x32 matrix. 32x32 matrix are called tiles. We can operate only on tensors which dimensions are multiply of 32. 

Ex. Let us have input tensors A[64x96] and B[96x64], which produces output tensor C[64x64]. 

Tensor A can be presented as tile matrix 2x3, B as tile matrix 3x2 and C as tile matrix 2x2.

To calculate each tensor C, we are calculating tiles C1, C2, C3 and C4: 

```
C1 = A1xB1 + A2xB3 + A3xB5 
C2 = A1xB2 + A2xB4 + A3xB6 
C3 = A4xB1 + A5xB3 + A6xB5 
C4 = A4xB2 + A5xB4 + A6xB6 
```
Similarly, as before, if matrix A is transposed, and tiles (previously were elements) are received in order A1, A4, A2, A5, A3, A6, and B1, B2, B3, B4, B5, B6, we can only wait for 2 tiles from tensors A and B to start multiplication.

### Macro and Micro Block

We have introduced logical concepts of macro and micro block (mblock, ublock).  

Size of tensor C that is calculated on one Tensix core, is defined with macro block dimension mxn(mblock). One element inside macro block matrix is called micro block. Micro block consists of k x l tiles.  

For example, we can have 4x4 tiles tensor where ublock(micro block) is 2x2, macro block  (mblock) is 2x2. Micro block has 4 tiles, macro block has 4 micro blocks or 16 tiles.

This tensor can be output of matrix multiplication of two tensors with dimension 4x4 tiles.

```
A X B = C 

CU1 = AU1 * BU1 + AU2* BU3 

CU2 = AU1 * BU2 + AU2 *BU4 

CU3 = AU3 * BU1 + AU4* BU3 

CU4 = AU3 * BU2 + AU4* BU4 
```

Instead of AU1, AU2, AU3, AU4, if streaming order of macro blocks is AU1, AU3, AU2, AU4, we can optimize how much data we are buffering and start with multiplication without waiting for all tiles. In the first step we can read AU1, AU3 and BU1, BU2, and calculate values, and then AU2, AU4 and BU3, BU4 and add them to the previously accumulated result.

Matmul implementation expects micro blocks to arrive in order where A is transposed at micro-block level. i.e., the order of micro-blocks is transposed, whereas tiles within each micro-block are in row-major order. 

### Executing Matmul on multiple cores 

Let us have tensor 256x256(8x8 tiles) which is output of matmul operation (A(256x256) x B(256x256)) on 4 chips (2x2), where each chip produces tensor 128x128 (4x4 tiles)

### Chip Inputs and Multicast Usage

- **Chip (1,1)**
    - **IN0:** AM1, AM2
    - **IN1:** BM1, BM3
- **Chip (1,2)**
    - **IN0:** AM1, AM2
    - **IN1:** BM2, BM4
- **Chip (2,1)**
    - **IN0:** AM3, AM4
    - **IN1:** BM1, BM3
- **Chip (2,2)**
    - **IN0:** AM3, AM4
    - **IN1:** BM2, BM4

### Calculation 

For Chip1:

`CM1 = AM1*BM1 + AM2*BM3`
This means that we do not reorder macro blocks, but micro blocks inside macroblocks will be reordered as in previous example where we multiplied two macroblocks and switched micro block 2 and 3 for first matmul operand. 
`AM1 = AU11, AU13, AU12, AU14`

Default for streaming output tiles is micro-block row order(below). Netlist parameter ublock_order:r/c defines, column or row order of micro block.

Example:

Let us have a netlist that has 2 input queues, matmul operation and output queue. Below is an extract from the netlist.

```
queues: 

  out      : {type: ram, input: matmul0 , entries: 16, grid_size: [2, 2], t: 1, mblock: [2, 2], ublock: [2, 2], df: Float16, target_device: 0, loc: dram, dram: [[0, 0x11000000], [0, 0x11100000], [0, 0x11200000], [0, 0x11300000]]} 

  in1      : {type: queue, input: HOST , entries: 16, grid_size: [2, 2], t: 1, mblock: [2, 2], ublock: [2, 2], df: Float16, target_device: 0, loc: dram, dram: [[0, 0x12000000], [0, 0x12100000], [0, 0x12200000], [0, 0x12300000]]} 

  in2      : {type: queue, input: HOST , entries: 16, grid_size: [2, 2], t: 1, mblock: [2, 2], ublock: [2, 2], df: Float16, target_device: 0, loc: dram, dram: [[0, 0x13000000], [0, 0x13100000], [0, 0x13200000], [0, 0x13300000]]} 


graphs: 

  test_graph: 

    target_device: 0 

    input_count: 16 

    matmul0: {type: matmul, grid_loc: [0, 0], grid_size: [2, 2], inputs: [in1, in2], in_df: [Float16, Float16], acc_df: Float16, out_df: Float16, gradient_op: false, intermed_df: Float16, ublock_order: r, buf_size_mb: 2, math_fidelity: HiFi3, untilize_output: false, t: 1, mblock: [2, 2], ublock: [2, 2], attributes: {m_k: 4, u_kt: 2}} 
```
Input queues:

Input queues:
- 4 dram location each 
- Grid size is [2,2] 
Matmul: 
- It has 4 cores, dimensions of output are same as dimensions of input 
Output queue: 
- Same dimension as input. Data stored in 4 locations 
*Pipegen.yaml 

### Buffers 

16 buffers - for each core it generates 4 L1 buffers:(input0, input1, intermediate (used for accumulation) and output buffer) 
8 buffers - related to input queues 
4 buffers - related to output queues 

### Pipes 
4 pipes - connects L1 output buffers with corresponding dram buffers. 
2 pipes - connects dram input buffers with corresponding L1 input0 buffers 
2 pipes - connects dram input buffers with corresponding L1 input1 buffers

Here is definition of two pipes that represent transfer to input 0. 

```
pipe_10000280000:  # Op: matmul0, input 0 

  id: 10000280000 

  input_list: [10000080000, 10000080008, 10000080004, 10000080012, 10000090000, 10000090008, 10000090004, 10000090012] 

  pipe_periodic_repeat: 0 

  output_list: [10000210000, 10000240000] 

  incoming_noc_id: 0 

  outgoing_noc_id: 0 

  mcast_core_rc: [0, 1, 0] 

pipe_10000270000:  # Op: matmul0, input 0 

  id: 10000270000 

  input_list: [10000060000, 10000060008, 10000060004, 10000060012, 10000070000, 10000070008, 10000070004, 10000070012] 

  pipe_periodic_repeat: 0 

  output_list: [10000150000, 10000180000] 

  incoming_noc_id: 0 

  outgoing_noc_id: 0 

  mcast_core_rc: [0, 0, 0]
Chip (1,1) 
   IN0: AM1, AM2
Chip (1,2): 
   IN0: AM1, AM2 
Chip (2,1)  
  In0: AM3, AM4 
Chip (2,2) 
  In0: AM3, AM4
```
 How do we interpret this input list? 

`input_list: [10000080000, 10000080008, 10000080004, 10000080012, 10000090000, 10000090008, 10000090004, 10000090012] `

This input list represents AM1 and AM2 (10000080000 and 10000090000) transposed.

`output_list: [10000150000, 10000180000] `

This means that we multicast to 2 buffers which are on Chip (1,1) and Chip (1,2).

`mcast_core_rc: [0, 0, 0] `

This represents core location used for multicast. Although it can be on same location where input buffer is placed, it will need an additional L1 buffer specifically for multicast.
