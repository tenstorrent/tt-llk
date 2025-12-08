# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0


from ttexalens.tt_exalens_lib import (
    read_from_device,
    write_to_device,
)

from .format_config import DataFormat
from .llk_params import format_tile_sizes
from .pack import (
    pack_bfp8_b,
    pack_bfp16,
    pack_fp16,
    pack_fp32,
    pack_int8,
    pack_int32,
    pack_uint8,
    pack_uint16,
    pack_uint32,
)
from .unpack import (
    unpack_res_tiles,
)


class StimuliConfig:

    # === STATIC VARIABLES ===
    STIMULI_L1_ADDRESS = 0x64000
    TILE_ELEMENTS = 1024

    def __init__(
        self,
        buffer_A,
        stimuli_A_format: DataFormat,
        buffer_B,
        stimuli_B_format: DataFormat,
        stimuli_res_format: DataFormat,
        tile_count_A: int = 1,
        tile_count_B: int = None,
        tile_count_res: int = 1,
        buffer_C=None,
        stimuli_C_format: DataFormat = None,
        tile_count_C: int = None,
        num_faces: int = 4,
        face_r_dim: int = 16,
        tile_dimensions: list[int] = [32, 32],
        sfpu=False,
    ):

        # Fields init
        self.buffer_A = buffer_A
        self.stimuli_A_format = stimuli_A_format
        self.tile_count_A = tile_count_A
        self.buffer_B = buffer_B
        self.stimuli_B_format = stimuli_B_format
        self.tile_count_B = tile_count_B
        self.buffer_C = buffer_C
        self.stimuli_C_format = stimuli_C_format
        self.tile_count_C = tile_count_C
        self.stimuli_res_format = stimuli_res_format
        self.tile_count_res = tile_count_res
        self.num_faces = num_faces
        self.face_r_dim = face_r_dim
        self.tile_dimensions = tile_dimensions
        self.sfpu = sfpu

        # Stimuli addresses calculation
        tile_size_A_bytes = self.stimuli_A_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )
        tile_size_B_bytes = self.stimuli_B_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )

        self.buf_a_addr = StimuliConfig.STIMULI_L1_ADDRESS
        self.buf_b_addr = self.buf_a_addr + tile_size_A_bytes * self.tile_count_A
        self.buf_res_addr = self.buf_b_addr + tile_size_B_bytes * self.tile_count_B

    def generate_stimuli_header_addresses(self, formats) -> list[str]:
        buf_a_format = format_tile_sizes[
            DataFormat.Float16_b if formats is None else formats.input_format
        ]
        buf_b_format = format_tile_sizes[
            DataFormat.Float16_b if formats is None else formats.input_format
        ]
        buf_res_format = format_tile_sizes[
            DataFormat.Float16_b if formats is None else formats.output_format
        ]
        return [
            f"constexpr Operand buffer_A({hex(self.buf_a_addr)}, {buf_a_format});",
            f"constexpr Operand buffer_B({hex(self.buf_b_addr)}, {buf_b_format});",
            f"constexpr Operand buffer_Res({hex(self.buf_res_addr)}, {buf_res_format});",
        ]

    @staticmethod
    def get_packer(data_format):
        packers = {
            DataFormat.Float16: pack_fp16,
            DataFormat.Float16_b: pack_bfp16,
            DataFormat.Float32: pack_fp32,
            DataFormat.Bfp8_b: pack_bfp8_b,
            DataFormat.Int32: pack_int32,
            DataFormat.UInt32: pack_uint32,
            DataFormat.UInt16: pack_uint16,
            DataFormat.Int8: pack_int8,
            DataFormat.UInt8: pack_uint8,
        }
        return packers.get(data_format)

    @staticmethod
    def write_matrix(buffer, pack_function, base_address, num_faces, location="0,0"):
        pack_function_lambda = lambda buffer_tile: (
            pack_function(buffer_tile, num_faces=num_faces)
            if pack_function == pack_bfp8_b
            else pack_function(buffer_tile)
        )

        packed_data = pack_function_lambda(buffer)
        write_to_device(location, base_address, packed_data)

    def write(self):
        pack_function_A = StimuliConfig.get_packer(self.stimuli_A_format)
        pack_function_B = StimuliConfig.get_packer(self.stimuli_B_format)

        # Validate pack functions for A and B
        if not pack_function_A or not pack_function_B:
            raise ValueError(
                f"Unsupported data formats: srcA({self.stimuli_A_format.name}), srcB({self.stimuli_B_format.name})"
            )

        StimuliConfig.write_matrix(
            self.buffer_A, pack_function_A, self.buf_a_addr, self.num_faces
        )
        StimuliConfig.write_matrix(
            self.buffer_B, pack_function_B, self.buf_b_addr, self.num_faces
        )

    def collect_results(self, location="0,0"):
        # Always read full tiles - hardware still outputs full tile data
        # but with variable face dimensions, only part of it is valid

        tile_elements = self.tile_dimensions[0] * self.tile_dimensions[1]
        read_bytes_cnt = (
            self.stimuli_res_format.num_bytes_per_tile(tile_elements)
            * self.tile_count_res
        )

        read_data = read_from_device(
            location, self.buf_res_addr, num_bytes=read_bytes_cnt
        )
        res_from_L1 = unpack_res_tiles(
            read_data,
            self.stimuli_res_format,
            self.tile_count_res,
            self.sfpu,
            self.num_faces,
            self.face_r_dim,
        )
        return res_from_L1
