# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
from dataclasses import dataclass
from enum import Enum


@dataclass
class FormatConfig:
    """
    A data class that holds configuration details for formats passed to LLKs.

    Attributes:
    unpack_src (str): The source format for the Unpacker.
    unpack_dst (str): The destination format for the Unpacker.
    math (str): The format used for _llk_math_ functions.
    pack_src (str): The source format for the Packer.
    pack_dst (str): The destination format for the Packer.

    Example:
    >>> formats = FormatConfig(unpack_src="Float32", unpack_dst="Float16", math="add", pack_src="Float16", pack_dst="Float32")
    >>> print(formats.unpack_src)
    Float32
    >>> print(formats.pack_src)
    Float16
    """

    unpack_src: str
    unpack_dst: str
    math: str
    pack_src: str
    pack_dst: str


class DataFormat(Enum):
    """
    An enumeration class that holds all the data formats supported by the LLKs.
    Used to avoid hardcoding the format strings in the test scripts using strings. This avoid typos and future errors.
    DataFormat(Enum) class instances can be compared via unique values.
    When you have a set of related constants and you want to leverage the benefits of enumeration (unique members, comparisons, introspection, etc.).
    It's a good choice for things like state machines, categories, or settings where values should not be changed or duplicated.
    """

    Float16 = "Float16"
    Float16_b = "Float16_b"
    Bfp8_b = "Bfp8_b"
    Float32 = "Float32"
    Int32 = "Int32"
