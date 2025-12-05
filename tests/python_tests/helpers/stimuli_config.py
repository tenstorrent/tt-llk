# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0


from ttexalens.tt_exalens_lib import (
    read_from_device,
    write_to_device,
)

from .format_config import DataFormat, FormatConfig, format_tile_sizes
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


class Stimuli:
    TILE_ELEMENTS = 1024

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

    def __init__(self, data, format, tile_cnt):
        self.data = data
        self.tile_cnt = tile_cnt

        self.tile_size_bytes = format.num_bytes_per_tile(Stimuli.TILE_ELEMENTS)
        self.memory_footprint = self.tile_size_bytes * self.tile_cnt
        self.pack_function = Stimuli.get_packer(self.format)

    def write(self, location):
        pass

    @staticmethod
    def write_matrix(
        buffer, tile_count, pack_function, base_address, tile_size, num_faces, location
    ):
        addresses = []
        packed_data_list = []

        pack_function_lambda = lambda buffer_tile: (
            pack_function(buffer_tile, num_faces=num_faces)
            if pack_function == pack_bfp8_b
            else pack_function(buffer_tile)
        )

        for i in range(tile_count):
            start_idx = StimuliManipulator.TILE_ELEMENTS * i
            tile_data = buffer[start_idx : start_idx + StimuliManipulator.TILE_ELEMENTS]
            packed_data = pack_function_lambda(tile_data)

            addresses.append(base_address + i * tile_size)
            packed_data_list.append(packed_data)

        for addr, data in zip(addresses, packed_data_list):
            write_to_device(location, addr, data)


class StimuliManipulator:
    STIMULI_L1_ADDRESS = 0x64000

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
        self.buffer_B = buffer_B
        self.stimuli_B_format = stimuli_B_format
        self.buffer_C = buffer_C
        self.stimuli_C_format = stimuli_C_format

        self.tile_size_A_bytes = stimuli_A_format.num_bytes_per_tile(
            StimuliManipulator.TILE_ELEMENTS
        )
        self.tile_size_B_bytes = stimuli_B_format.num_bytes_per_tile(
            StimuliManipulator.TILE_ELEMENTS
        )
        self.buf_a_addr = StimuliManipulator.STIMULI_L1_ADDRESS

        self.buf_a_addr = self.buf_a_addr
        self.buf_b_addr = 0
        self.buf_res_addr = 0

    def generate_stimuli_header_addresses(self) -> list[str]:
        buf_a_format = format_tile_sizes[
            DataFormat.Float16_b if self.formats is None else self.formats.input_format
        ]
        buf_b_format = format_tile_sizes[
            DataFormat.Float16_b if self.formats is None else self.formats.input_format
        ]
        buf_res_format = format_tile_sizes[
            DataFormat.Float16_b if self.formats is None else self.formats.output_format
        ]
        return [
            f"constexpr Operand buffer_A({hex(self.buf_a_addr)}, {buf_a_format});",
            f"constexpr Operand buffer_B({hex(self.buf_b_addr)}, {buf_b_format});",
            f"constexpr Operand buffer_Res({hex(self.buf_res_addr)}, {buf_res_format});",
        ]

    def write(self):
        # Calculate L1 addresses
        tile_size_A_bytes = self.stimuli_A_format.num_bytes_per_tile(
            StimuliManipulator.TILE_ELEMENTS
        )
        tile_size_B_bytes = self.stimuli_B_format.num_bytes_per_tile(
            StimuliManipulator.TILE_ELEMENTS
        )

        buffer_A_address = 0x64000
        buffer_B_address = buffer_A_address + tile_size_A_bytes * self.tile_count_A

        # Handle optional third buffer
        if buffer_C is not None:
            if stimuli_C_format is None or tile_count_C is None:
                raise ValueError(
                    "If buffer_C is provided, stimuli_C_format and tile_count_C must also be provided"
                )

            tile_size_C_bytes = stimuli_C_format.num_bytes_per_tile(
                StimuliManipulator.TILE_ELEMENTS
            )
            buffer_C_address = buffer_B_address + tile_size_B_bytes * tile_count_B
            result_buffer_address = buffer_C_address + tile_size_C_bytes * tile_count_C
        else:
            buffer_C_address = None
            result_buffer_address = buffer_B_address + tile_size_B_bytes * tile_count_B

        pack_function_A = get_packer(self.stimuli_A_format)
        pack_function_B = get_packer(self.stimuli_B_format)

        # Validate pack functions for A and B
        if not pack_function_A or not pack_function_B:
            raise ValueError(
                f"Unsupported data formats: {self.stimuli_A_format.name}, {self.stimuli_B_format.name}"
            )

        # Handle optional third buffer pack function
        pack_function_C = None
        if self.buffer_C is not None:
            pack_function_C = get_packer(self.stimuli_C_format)
            if not pack_function_C:
                raise ValueError(
                    f"Unsupported data format for buffer_C: {self.stimuli_C_format.name}"
                )

        StimuliManipulator.write_matrix(
            buffer_A,
            tile_count_A,
            pack_function_A,
            buffer_A_address,
            tile_size_A_bytes,
            num_faces,
            location,
        )
        StimuliManipulator.write_matrix(
            buffer_B,
            tile_count_B,
            pack_function_B,
            buffer_B_address,
            tile_size_B_bytes,
            num_faces,
            location,
        )

        # Write optional third buffer
        if buffer_C is not None:
            StimuliManipulator.write_matrix(
                buffer_C,
                tile_count_C,
                pack_function_C,
                buffer_C_address,
                tile_size_C_bytes,
                num_faces,
            )

        return result_buffer_address

    def collect_results(
        self,
        formats: FormatConfig,
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
        read_bytes_cnt = (
            formats.output_format.num_bytes_per_tile(tile_elements) * tile_count
        )

        read_data = read_from_device(location, address, num_bytes=read_bytes_cnt)
        res_from_L1 = unpack_res_tiles(
            read_data,
            formats,
            tile_count=tile_count,
            sfpu=sfpu,
            num_faces=num_faces,
            face_r_dim=face_r_dim,
        )
        return res_from_L1
