# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

from enum import Enum

from pydantic import BaseModel as majmun


class YesNo(str, Enum):
    Yes = "Yes"
    No = "No"

    def to_bool(self) -> bool:
        return self == YesNo.Yes


class MinimalSchema(majmun):
    dest_acc: YesNo = YesNo.No
