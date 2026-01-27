// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

// #include <array>
#include <cstdint>

namespace ckernel
{

/*
   The current max constraints are set for large default size of 32x32, but that is until tensorShape is piped to all ops
   Once it is piped to all ops, we can relax max number of faces, to be closer to description of a tensorshape
*/
constexpr uint8_t MAX_FACE_R_DIM      = 16;
constexpr uint8_t MAX_FACE_C_DIM      = 16;
constexpr uint8_t MAX_TILE_R_DIM      = 32;
constexpr uint8_t MAX_TILE_C_DIM      = 32;
constexpr uint8_t MAX_NUM_FACES_R_DIM = 2;
constexpr uint8_t MAX_NUM_FACES_C_DIM = 2;
constexpr uint8_t MAX_NUM_FACES       = MAX_NUM_FACES_R_DIM * MAX_NUM_FACES_C_DIM;

constexpr uint8_t MAX_FPU_ROWS      = 8;
constexpr uint8_t MAX_FPU_ROWS_LOG2 = 3;

/**
 * @brief Standardized tensor shape representation for LLK operations.
 *
 * Replaces inconsistent tile size parameters (num_faces, face_r_dim,
 * narrow_tile, partial_face, VectorMode) with a unified struct.
 *
 * A tile is composed of faces arranged in a grid:
 * - Total tile rows = face_r_dim * num_faces_r_dim
 * - Total tile cols = face_c_dim * num_faces_c_dim
 *
 * Example: 32x32 tile = 4 faces of 16x16 (num_faces_r_dim=2, num_faces_c_dim=2)
 * Example: 32x16 tile = 2 faces of 16x16 (num_faces_r_dim=2, num_faces_c_dim=1)
 */
struct __attribute__((packed)) TensorShape
{
    uint8_t face_r_dim;      ///< Row dimension of each face (typically 16)
    uint8_t face_c_dim;      ///< Column dimension of each face (always 16 for HW)
    uint8_t num_faces_r_dim; ///< Number of faces in row dimension
    uint8_t num_faces_c_dim; ///< Number of faces in column dimension

    /// @brief Get total tile row dimension
    constexpr uint16_t total_row_dim() const
    {
        return static_cast<uint16_t>(face_r_dim) * num_faces_r_dim;
    }

    /// @brief Get total tile column dimension
    constexpr uint16_t total_col_dim() const
    {
        return static_cast<uint16_t>(face_c_dim) * num_faces_c_dim;
    }

    /// @brief Get total number of datums in the tile
    constexpr uint16_t total_tensor_size() const
    {
        return total_row_dim() * total_col_dim();
    }

    /// @brief Get total number of faces
    constexpr uint8_t total_num_faces() const
    {
        return num_faces_r_dim * num_faces_c_dim;
    }
};

static_assert(sizeof(TensorShape) == 4, "TensorShape must be 4 bytes");

/**
 * @brief Operations that are dependent of face positioning within a tile will have this function called to validate the tensor shape.
 *
 * @param tensor_shape: Tensor shape to validate
 **/
inline void validate_tensor_shape_tile_dependent_ops_(const TensorShape &tensor_shape)
{
    constexpr uint8_t VALID_FACE_R_DIMS[] = {1, 2, 4, 8, 16};
    constexpr uint8_t VALID_NUM_FACES[]   = {1, 2, 4};
    LLK_ASSERT(
        std::find(std::begin(VALID_NUM_FACES), std::end(VALID_NUM_FACES), tensor_shape.total_num_faces()) != std::end(VALID_NUM_FACES),
        "total num_faces must be 1, 2, or 4");
    LLK_ASSERT(
        std::find(std::begin(VALID_FACE_R_DIMS), std::end(VALID_FACE_R_DIMS), tensor_shape.face_r_dim) != std::end(VALID_FACE_R_DIMS),
        "face_r_dim must be 1, 2, 4, 8, 16");
    LLK_ASSERT(tensor_shape.face_c_dim == 16, "face_c_dim must be 16");
}

// /**
//  * @brief Build TensorShape array from JIT-generated arrays.
//  *
//  * Converts the JIT compiler output format (face_r_dim, face_c_dim, num_faces, narrow_tile)
//  * into a standardized array of TensorShape structs.
//  *
//  * The conversion logic:
//  * - num_faces_c_dim = narrow_tile ? 1 : 2
//  * - num_faces_r_dim = ceil(num_faces / num_faces_c_dim)
//  *
//  * @tparam N Number of circular buffers (typically NUM_CIRCULAR_BUFFERS)
//  * @param face_r_dim JIT array of face row dimensions
//  * @param face_c_dim JIT array of face column dimensions
//  * @param num_faces JIT array of total face counts
//  * @param narrow_tile JIT array of narrow tile flags
//  * @return std::array of TensorShape structs
//  */
// template <size_t N>
// constexpr std::array<TensorShape, N> make_tensor_shapes(
//     const uint8_t (&face_r_dim)[N],
//     const uint8_t (&face_c_dim)[N],
//     const uint8_t (&num_faces)[N],
//     const uint8_t (&narrow_tile)[N])
// {
//     std::array<TensorShape, N> result{};
//     for (size_t i = 0; i < N; ++i)
//     {
//         uint8_t nf_c = narrow_tile[i] ? 1 : 2;
//         uint8_t nf_r = (num_faces[i] + nf_c - 1) / nf_c;
//         result[i] = TensorShape{face_r_dim[i], face_c_dim[i], nf_r, nf_c};
//     }
//     return result;
// }

// /**
//  * @brief Get TensorShape for a circular buffer at compile time.
//  *
//  * Requires tensor_shapes array to be in scope (built via make_tensor_shapes).
//  *
//  * @tparam cb_id Circular buffer ID (compile-time constant)
//  * @return TensorShape for the specified circular buffer
//  */
// template <uint32_t cb_id>
// constexpr TensorShape get_tensor_shape()
// {
//     return tensor_shapes[cb_id];
// }

// /**
//  * @brief Get TensorShape for a circular buffer at runtime.
//  *
//  * Requires tensor_shapes array to be in scope (built via make_tensor_shapes).
//  *
//  * @param cb_id Circular buffer ID
//  * @return TensorShape for the specified circular buffer
//  */
// inline TensorShape get_tensor_shape(uint32_t cb_id)
// {
//     return tensor_shapes[cb_id];
// }

} // namespace ckernel
