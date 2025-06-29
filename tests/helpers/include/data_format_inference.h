// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0
#include <new>     // for placement new
#include <cstdlib> // for malloc/free
#include "tensix_types.h"

#if defined(ARCH_WORMHOLE) && defined(ARCH_BLACKHOLE)
#error "Only one of ARCH_WORMHOLE or ARCH_BLACKHOLE can be defined"
#elif defined(ARCH_WORMHOLE)
constexpr bool is_blackhole = false;
constexpr bool is_wormhole  = true;
#elif defined(ARCH_BLACKHOLE)
constexpr bool is_blackhole = true;
constexpr bool is_wormhole  = false;
#else
#error "You must define either ARCH_WORMHOLE or ARCH_BLACKHOLE"
#endif

struct FormatConfig
{
    const uint32_t unpack_src;
    const uint32_t unpack_dst;
    const uint32_t math;
    const uint32_t pack_src;
    const uint32_t pack_dst;
    constexpr FormatConfig(
        uint32_t unpack_src_,
        uint32_t unpack_dst_,
        uint32_t math_,
        uint32_t pack_src_,
        uint32_t pack_dst_): 
        unpack_src(unpack_src_),
        unpack_dst(unpack_dst_),
        math(math_),
        pack_src(pack_src_),
        pack_dst(pack_dst_) {}
};

constexpr bool is_exponentB(DataFormat format)
{
    return (format == DataFormat::Float16_b || format == DataFormat::Bfp8_b || format == DataFormat::Tf32);
}

constexpr bool is_format_combination_outlier(DataFormat input, DataFormat output, bool is_fp32_dest_acc_en)
{
    return (is_exponentB(input) && output == DataFormat::Float16 && !is_fp32_dest_acc_en);
}

constexpr FormatConfig get_data_formats(DataFormat unpack_in, DataFormat unpack_out, DataFormat math, DataFormat pack_in, DataFormat pack_out)
{
    return {static_cast<uint32_t>(unpack_in), static_cast<uint32_t>(unpack_out), static_cast<uint32_t>(math), static_cast<uint32_t>(pack_in), static_cast<uint32_t>(pack_out)};

}

constexpr FormatConfig infer_data_formats(DataFormat input, DataFormat output, bool is_fp32_dest_acc_en, bool truncate_16bit)
{
    DataFormat unpack_in  = input;
    DataFormat unpack_out = input;
    DataFormat pack_out   = output;
    DataFormat pack_in    = DataFormat::Invalid; // Invalid format as placeholder
    DataFormat math = DataFormat::Invalid; // Invalid format as placeholder


    if (input == DataFormat::Float32 && !UNPACKING_TO_DEST)
    {
        unpack_out = DataFormat::Tf32;
        if (is_fp32_dest_acc_en || is_exponentB(output))
        {
            pack_in = output;
        }
        else
        {
            pack_in = DataFormat::Tf32;
        }
    }
    else if (input == DataFormat::Float16 && output == DataFormat::Bfp8_b && !is_fp32_dest_acc_en)
    {
        pack_in = DataFormat::Bfp8;
    }
    else if (is_format_combination_outlier(input, output, is_fp32_dest_acc_en))
    {
        pack_in = (is_wormhole) ? DataFormat::Float32 : output;
    }
    else
    {
        pack_in = is_fp32_dest_acc_en ? output : input;
    }

    if (is_wormhole && is_fp32_dest_acc_en && output == DataFormat::Float16)
    {
        pack_in = DataFormat::Float32; // Gasket in wormhole cannot convert fp32 to fp16, and since dest accumulation turns on for
                                       // outlier cases we have fp32 in dest, so gasket cannot convert it to fp16, packer must do that
    }

    math = unpack_out;
    return get_data_formats(unpack_in, unpack_out, math, pack_in, pack_out);
}

// FormatConfig* set_data_formats(
//     DataFormat input, 
//     DataFormat output,
//     bool is_fp32_dest_acc_en,
//     bool truncate_16bit,
//     int total_configs = 1)
// {
//     int L1_to_L1_iterations_left = total_configs - 1;

//     // Allocate raw memory
//     void* raw = std::malloc(sizeof(FormatConfig) * total_configs);
//     if (!raw) return nullptr; // exception handling disabledwe just return null

//     FormatConfig* data_formats = static_cast<FormatConfig*>(raw);

//     for (int i = 0; i < L1_to_L1_iterations_left; ++i)
//     {
//         new (&data_formats[i]) FormatConfig(
//             infer_data_formats(input, input, is_fp32_dest_acc_en, truncate_16bit)
//         );
//     }

//     new (&data_formats[L1_to_L1_iterations_left]) FormatConfig(
//         infer_data_formats(input, output, is_fp32_dest_acc_en, truncate_16bit)
//     );

//     return data_formats; // ⚠️ Must be manually destroyed and freed
// }
