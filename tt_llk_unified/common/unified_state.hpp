// SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

// TODO LP per thread

inline bool _unified_state_unpack_is_32bit_dest_ = false;
inline bool _unified_state_math_is_32bit_dest_   = false;
inline bool _unified_state_pack_is_32bit_dest_   = false;
inline bool _unified_state_unpack_full_sync_     = false;
inline bool _unified_state_math_full_sync_       = false;
inline bool _unified_state_pack_full_sync_       = false;
inline int _unified_state_math_fidelity_         = 0;
