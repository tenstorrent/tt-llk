// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ckernel
{

enum class ReduceDim : uint8_t
{
    REDUCE_ROW,
    REDUCE_COL,
    REDUCE_SCALAR,
};

enum class PoolType : uint8_t
{
    SUM,
    AVG,
    MAX,
};

enum class DataCopyType : uint8_t
{
    A2D,
    B2D,
};

enum class EltwiseBinaryType : uint8_t
{
    ELWMUL,
    ELWADD,
    ELWSUB,
};

// Broadcasts only occur on SrcB
enum class BroadcastType : uint8_t
{
    NONE,
    COL,
    ROW,
    SCALAR,
};

enum class SfpuType : uint32_t
{
    tanh,
    gelu,
    exponential,
    reciprocal,
    sqrt,
    relu,
    lrelu,
    relumin,
    relumax,
    stochround,
    typecast
};

enum class DstSync : uint8_t
{
    SyncHalf,
    SyncFull,
};

enum class MathFidelity : uint8_t
{
    LoFi  = 0,
    HiFi2 = 1,
    HiFi3 = 2,
    HiFi4 = 3
};

} // namespace ckernel
