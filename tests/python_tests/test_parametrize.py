# SPDX-FileCopyrightText: © 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0

import pytest
from helpers.param_config import (
    UnknownDependenciesError,
    _compute_dependency_map,
    _param_dependencies,
    _verify_dependency_map,
)


def test_param_dependencies_constant():
    """
    Parameters can have constant values attached to them.
    Since these are constants, the parameter has no dependencies.
    """
    constraint = 1
    dependencies = _param_dependencies("constraint", constraint)
    assert len(dependencies) == 0
    assert dependencies == []


def test_param_dependencies_list():
    """
    Parameters can have a list of values attached to them.
    This is used to represent a parameter that takes multiple values.
    Since this is a list of constants, the parameter has no dependencies.
    """
    constraint = [1, 2, 3]
    assert _param_dependencies("constraint", constraint) == []


def test_param_dependencies_empty_lambda():
    """
    Parameters can have a lambda function attached to them.
    The parameters of the lambda function are treated as dependencies of the parameter.
    """
    constraint = lambda: []
    assert _param_dependencies("constraint", constraint) == []


def test_param_dependencies_multiple_lambda():
    """
    Parameters can have a lambda function attached to them.
    The parameters of the lambda function are treated as dependencies of the parameter.
    """
    constraint = lambda arg0, arg1, arg2: []
    dependencies = _param_dependencies("constraint", constraint)

    assert len(dependencies) == 3
    assert dependencies[0] == "arg0"
    assert dependencies[1] == "arg1"
    assert dependencies[2] == "arg2"


def test_verify_dependency_map_pass():
    """
    The dependency map is valid if all dependencies are known parameters.
    """
    dependency_map = {
        "exist1": ["exist2", "exist3"],
        "exist2": ["exist3"],
        "exist3": [],
        "exist4": ["exist1"],
    }

    try:
        # no exception should be raised
        _verify_dependency_map(dependency_map)
    except Exception as e:
        assert False, f"Unexpected exception raised: {e}"


def test_verify_dependency_map_fail():
    """
    The dependency map is invalid if a dependency is not a known parameter.
    """
    dependency_map = {
        "exist1": ["exist2", "missing1"],
        "exist2": ["missing2", "exist3"],
        "exist3": [],
        "exist4": ["missing1", "missing2"],
    }

    expected_missing = {
        "exist1": {"missing1"},
        "exist2": {"missing2"},
        "exist4": {"missing1", "missing2"},
    }

    with pytest.raises(UnknownDependenciesError) as error:
        _verify_dependency_map(dependency_map)

    assert error.value.missing == expected_missing


def test_compute_dependency_matrix_pass():
    """
    The dependency matrix is valid if all dependencies are known parameters.
    """

    expected = {
        "exist1": ["exist2", "exist3"],
        "exist2": ["exist3"],
        "exist3": [],
        "exist4": ["exist1"],
        "exist5": [],
    }

    dependency_map = _compute_dependency_map(
        exist1=lambda exist2, exist3: [],
        exist2=lambda exist3: [],
        exist3="value",
        exist4=lambda exist1: [],
        exist5=["value", "value"],
    )

    assert dependency_map == expected


def test_compute_dependency_matrix_fail():
    """
    The dependency matrix is invalid if a dependency is not a known parameter.
    """

    expected_missing = {
        "exist1": {"missing1"},
        "exist2": {"missing2"},
        "exist4": {"missing1", "missing2"},
    }

    with pytest.raises(UnknownDependenciesError) as error:
        _compute_dependency_map(
            exist1=lambda exist2, missing1: [],
            exist2=lambda missing2, exist3: [],
            exist3="value",
            exist4=lambda missing1, missing2: [],
            exist5=["value", "value2"],
        )

    assert error.value.missing == expected_missing
