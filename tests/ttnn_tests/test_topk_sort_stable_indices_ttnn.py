# SPDX-FileCopyrightText: © 2026 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
import torch

try:
    import ttnn
except ImportError:  # pragma: no cover - handled via skip fixture
    ttnn = None


@pytest.fixture(scope="module")
def ttnn_device():
    if ttnn is None:
        pytest.skip("ttnn is not available in this test environment")

    device = ttnn.CreateDevice(0)
    try:
        yield device
    finally:
        ttnn.CloseDevice(device)


def _make_tie_heavy_input(shape, seed=0):
    # Many repeated values force tie handling to dominate stable index parity.
    g = torch.Generator()
    g.manual_seed(seed)
    return torch.randint(low=0, high=8, size=shape, generator=g, dtype=torch.int32).to(
        torch.bfloat16
    )


@pytest.mark.parametrize("descending", [False, True], ids=["ascending", "descending"])
@pytest.mark.parametrize(
    "width", [64, 96, 128, 160, 1600], ids=["w64", "w96", "w128", "w160", "w1600"]
)
def test_sort_stable_indices_strict_ttnn(descending, width, ttnn_device):
    shape = [1, 1, 32, width]
    torch_input = _make_tie_heavy_input(shape, seed=17 + width + int(descending))

    tt_input = ttnn.from_torch(
        torch_input, ttnn.bfloat16, layout=ttnn.Layout.TILE, device=ttnn_device
    )
    tt_values, tt_indices = ttnn.sort(
        tt_input, dim=-1, descending=descending, stable=True
    )

    torch_values, torch_indices = torch.sort(
        torch_input, dim=-1, descending=descending, stable=True
    )

    tt_values_torch = ttnn.to_torch(tt_values).to(torch.bfloat16)
    tt_indices_torch = ttnn.to_torch(tt_indices).to(torch.int64)

    assert torch.equal(tt_values_torch, torch_values)
    assert torch.equal(tt_indices_torch, torch_indices.to(torch.int64))


@pytest.mark.parametrize("largest", [False, True], ids=["smallest", "largest"])
@pytest.mark.parametrize(
    "width,k",
    [(64, 32), (96, 32), (128, 32), (160, 32), (1600, 32)],
    ids=["w64", "w96", "w128", "w160", "w1600"],
)
def test_topk_stable_indices_strict_ttnn(largest, width, k, ttnn_device):
    shape = [1, 1, 32, width]
    torch_input = _make_tie_heavy_input(shape, seed=131 + width + int(largest))

    tt_input = ttnn.from_torch(
        torch_input, ttnn.bfloat16, layout=ttnn.Layout.TILE, device=ttnn_device
    )
    tt_values, tt_indices = ttnn.topk(
        tt_input, k, dim=-1, largest=largest, sorted=True, stable=True
    )

    torch_values, torch_indices = torch.sort(
        torch_input, dim=-1, descending=largest, stable=True
    )
    torch_values = torch_values[..., :k]
    torch_indices = torch_indices[..., :k]

    tt_values_torch = ttnn.to_torch(tt_values).to(torch.bfloat16)
    tt_indices_torch = ttnn.to_torch(tt_indices).to(torch.int64)

    assert torch.equal(tt_values_torch, torch_values)
    assert torch.equal(tt_indices_torch, torch_indices.to(torch.int64))
