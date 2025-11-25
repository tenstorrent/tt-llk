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
    stage_id: Optional[int] = None
    sfpu: bool = False
    _data: Optional[torch.Tensor] = None
    _raw_data: Optional[torch.Tensor] = None
    _tile_count: Optional[int] = None

    def __post_init__(self):
        if self.is_input() and (self.dimensions is None or self.data_format is None):
            raise ValueError(
                f"Input operand '{self.name}' must have dimensions and data_format"
            )

    def is_input(self) -> bool:
        return self.stage_id is None and self.l1_address is not None

    def is_intermediate(self) -> bool:
        return self.stage_id is not None and not self.is_output

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

    def __repr__(self) -> str:
        type_str = (
            "input"
            if self.is_input()
            else ("output" if self.is_output else "intermediate")
        )
        addr_str = f"@{hex(self.l1_address)}" if self.l1_address else ""
        stage_str = f"[stage{self.stage_id}]" if self.stage_id is not None else ""
        dims_str = f"{self.dimensions}" if self.dimensions else ""
        fmt_str = f"{self.data_format.name}" if self.data_format else ""
        return (
            f"Operand({self.name}:{type_str}{stage_str}{addr_str} {dims_str} {fmt_str})"
        )


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
        address: int,
        sfpu: bool = False,
    ) -> Operand:
        if name in self.operands:
            raise ValueError(f"Operand '{name}' already exists")

        operand = Operand(
            name=name,
            dimensions=dimensions,
            data_format=data_format,
            l1_address=address,
            is_output=False,
            stage_id=None,
            sfpu=sfpu,
        )
        self.operands[name] = operand
        return operand

    def add_intermediate(
        self, name: str, stage_id: int, address: Optional[int] = None
    ) -> Operand:
        if name in self.operands:
            raise ValueError(f"Operand '{name}' already exists")

        operand = Operand(
            name=name,
            dimensions=None,
            data_format=None,
            l1_address=address,
            is_output=False,
            stage_id=stage_id,
        )
        self.operands[name] = operand
        return operand

    def add_output(self, name: str, stage_id: int, address: int) -> Operand:
        if name in self.operands:
            raise ValueError(f"Operand '{name}' already exists")

        operand = Operand(
            name=name,
            dimensions=None,
            data_format=None,
            l1_address=address,
            is_output=True,
            stage_id=stage_id,
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

    def get_outputs_for_stage(self, stage_id: int) -> list[Operand]:
        return [op for op in self.operands.values() if op.stage_id == stage_id]

    def update_data(self, name: str, data: torch.Tensor):
        if name not in self.operands:
            raise KeyError(f"Operand '{name}' not found")
        self.operands[name].data = data

    def __repr__(self) -> str:
        lines = ["OperandRegistry:"]
        for name, op in self.operands.items():
            lines.append(f"  {op}")
        return "\n".join(lines)
