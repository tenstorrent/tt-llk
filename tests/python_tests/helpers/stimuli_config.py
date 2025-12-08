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
    STIMULI_L1_ADDRESS = 0x64000
    TILE_ELEMENTS = 1024

    def __init__(
        self,
        buffer_A,
        stimuli_A_format: DataFormat,
        buffer_B,
        stimuli_B_format: DataFormat,
        tile_count_A: int = 1,
        tile_count_B: int = None,
        num_faces=4,
        buffer_C=None,
        stimuli_C_format: DataFormat = None,
        tile_count_C: int = None,
    ):
        self.buffer_A = buffer_A
        self.stimuli_A_format = stimuli_A_format
        self.tile_count_A = tile_count_A
        self.buffer_B = buffer_B
        self.stimuli_B_format = stimuli_B_format
        self.tile_count_B = tile_count_B
        self.buffer_C = buffer_C
        self.stimuli_C_format = stimuli_C_format
        self.num_faces = num_faces

        self.tile_size_A_bytes = stimuli_A_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )
        self.tile_size_B_bytes = stimuli_B_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )
        self.buf_a_addr = StimuliConfig.STIMULI_L1_ADDRESS

        self.buf_a_addr = self.buf_a_addr
        self.buf_b_addr = 0
        self.buf_res_addr = 0

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
    def write_matrix(
        buffer,
        tile_count,
        pack_function,
        base_address,
        tile_size,
        num_faces,
        location="0,0",
    ):
        addresses = []
        packed_data_list = []

        pack_function_lambda = lambda buffer_tile: (
            pack_function(buffer_tile, num_faces=num_faces)
            if pack_function == pack_bfp8_b
            else pack_function(buffer_tile)
        )

        for ind in range(tile_count):
            start_idx = StimuliConfig.TILE_ELEMENTS * ind
            tile_data = buffer[start_idx : start_idx + StimuliConfig.TILE_ELEMENTS]
            packed_data = pack_function_lambda(tile_data)

            # print(f"TILE[{ind}] - {base_address + ind * tile_size} {tile_size}")

            addresses.append(base_address + ind * tile_size)
            packed_data_list.append(packed_data)

        for addr, data in zip(addresses, packed_data_list):
            write_to_device(location, addr, data)

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

    def write(self):
        tile_size_A_bytes = self.stimuli_A_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )
        tile_size_B_bytes = self.stimuli_B_format.num_bytes_per_tile(
            StimuliConfig.TILE_ELEMENTS
        )

        self.buf_a_addr = StimuliConfig.STIMULI_L1_ADDRESS
        self.buf_b_addr = self.buf_a_addr + tile_size_A_bytes * self.tile_count_A

        pack_function_A = StimuliConfig.get_packer(self.stimuli_A_format)
        pack_function_B = StimuliConfig.get_packer(self.stimuli_B_format)

        # Validate pack functions for A and B
        if not pack_function_A or not pack_function_B:
            raise ValueError(
                f"Unsupported data formats: {self.stimuli_A_format.name}, {self.stimuli_B_format.name}"
            )

        self.buf_res_addr = self.buf_b_addr + tile_size_B_bytes * self.tile_count_B

        StimuliConfig.write_matrix(
            self.buffer_A,
            self.tile_count_A,
            pack_function_A,
            self.buf_a_addr,
            tile_size_A_bytes,
            self.num_faces,
        )
        StimuliConfig.write_matrix(
            self.buffer_B,
            self.tile_count_B,
            pack_function_B,
            self.buf_b_addr,
            tile_size_B_bytes,
            self.num_faces,
        )

    def collect_results(
        self,
        output_format,
        tile_count: int,
        address: int = 0x68000,  # Default L1 address of result, placed so both coverage and non-coverage elfs don't run it over
        location: str = "0,0",
        sfpu: bool = False,
        tile_dimensions=[32, 32],
        num_faces: int = 4,
        face_r_dim: int = 16,  # Default to 16 for backward compatibility
    ):
        # Always read full tiles - hardware still outputs full tile data
        # but with variable face dimensions, only part of it is valid
        tile_elements = tile_dimensions[0] * tile_dimensions[1]
        read_bytes_cnt = output_format.num_bytes_per_tile(tile_elements) * tile_count

        read_data = read_from_device(location, address, num_bytes=read_bytes_cnt)
        res_from_L1 = unpack_res_tiles(
            read_data,
            output_format,
            tile_count=tile_count,
            sfpu=sfpu,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
        )
        return res_from_L1
