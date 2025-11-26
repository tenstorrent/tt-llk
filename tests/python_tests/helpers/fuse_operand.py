# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from dataclasses import dataclass
from typing import Optional, Tuple

import torch
from helpers.llk_params import DataFormat
from helpers.stimuli_generator import generate_stimuli
from helpers.tilize_untilize import tilize_block


@dataclass
class Operand:
    name: str
    dimensions: Optional[Tuple[int, int, int, int]] = None
    data_format: Optional[DataFormat] = None
    l1_address: Optional[int] = None
    is_output: bool = False
    sfpu: bool = True
    _data: Optional[torch.Tensor] = None
    _raw_data: Optional[torch.Tensor] = None
    _tile_count: Optional[int] = None

    def __post_init__(self):
        if not self.is_output and (self.dimensions is None or self.data_format is None):
            raise ValueError(
                f"Input operand '{self.name}' must have dimensions and data_format"
            )

    def is_input(self) -> bool:
        return not self.is_output

    def generate_data(self):
        if self._data is not None:
            return

        if self.dimensions is None or self.data_format is None:
            raise ValueError(
                f"Cannot generate data for operand '{self.name}' without dimensions and format"
            )

        raw_data, _, tile_count = generate_stimuli(
            self.data_format,
            self.data_format,
            input_dimensions=self.dimensions,
            sfpu=self.sfpu,
        )

        if self.data_format != DataFormat.Bfp8_b:
            tilized_data = tilize_block(
                raw_data, dimensions=self.dimensions, stimuli_format=self.data_format
            )
        else:
            tilized_data = raw_data

        self._raw_data = raw_data
        self._data = tilized_data
        self._tile_count = tile_count

    @property
    def data(self) -> Optional[torch.Tensor]:
        if self._data is None and self.is_input():
            self.generate_data()
        return self._data

    @data.setter
    def data(self, value: torch.Tensor):
        self._data = value

    @property
    def raw_data(self) -> Optional[torch.Tensor]:
        if self._raw_data is None and self.is_input():
            self.generate_data()
        return self._raw_data

    @property
    def tile_count(self) -> Optional[int]:
        if self._tile_count is None and self.is_input():
            self.generate_data()
        return self._tile_count


@dataclass
class OperandMapping:
    inputs: dict[str, str]
    outputs: dict[str, str]

    def get_input_operand(self, key: str, operands: dict[str, Operand]) -> Operand:
        operand_name = self.inputs.get(key)
        if operand_name is None:
            raise KeyError(f"Input key '{key}' not found in mapping")
        if operand_name not in operands:
            raise KeyError(f"Operand '{operand_name}' not found in operands dict")
        return operands[operand_name]

    def get_output_operand(self, key: str, operands: dict[str, Operand]) -> Operand:
        operand_name = self.outputs.get(key)
        if operand_name is None:
            raise KeyError(f"Output key '{key}' not found in mapping")
        if operand_name not in operands:
            raise KeyError(f"Operand '{operand_name}' not found in operands dict")
        return operands[operand_name]


class OperandRegistry:
    def __init__(self):
        self.operands: dict[str, Operand] = {}

    def add_input(
        self,
        name: str,
        dimensions: Tuple[int, int, int, int],
        data_format: DataFormat,
        address: int = None,
        sfpu: bool = True,
    ) -> Operand:
        if name in self.operands:
            raise ValueError(f"Operand '{name}' already exists")

        operand = Operand(
            name=name,
            dimensions=dimensions,
            data_format=data_format,
            l1_address=address,
            is_output=False,
            sfpu=sfpu,
        )
        self.operands[name] = operand
        return operand

    def add_output(self, name: str, address: int = None) -> Operand:
        if name in self.operands:
            raise ValueError(f"Operand '{name}' already exists")

        operand = Operand(
            name=name,
            dimensions=None,
            data_format=None,
            l1_address=address,
            is_output=True,
        )
        self.operands[name] = operand
        return operand

    def get(self, name: str) -> Operand:
        if name not in self.operands:
            raise KeyError(f"Operand '{name}' not found")
        return self.operands[name]

    def get_all_inputs(self) -> list[Operand]:
        return [op for op in self.operands.values() if op.is_input()]

    def get_all_outputs(self) -> list[Operand]:
        return [op for op in self.operands.values() if op.is_output]

    def update_data(self, name: str, data: torch.Tensor):
        if name not in self.operands:
            raise KeyError(f"Operand '{name}' not found")
        self.operands[name].data = data
