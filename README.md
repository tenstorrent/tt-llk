# tt-llk: CPP Low Level Kernels (LLK) & test infrastructure #

## Overview
This repository contains low level kernels for Tenstorrent AI chips (Grayskull (deprecated), Wormhole_B0, and Blackhole), which represent foundational primitives of compute used as building blocks for higher level software stacks that implement ML OPs. Alongside the kernels is a test environment used for validating LLK APIs.

LLKs are header-only.


# Install
1. Clone repo
2. Set up test environment per https://github.com/tenstorrent/tt-llk/blob/main/tests/README.md


# Contributing
1. Create a new branch.
2. Make your changes and commit.
3. Add new tests to cover your changes if needed and run existing ones.
4. Start a pull request (PR).


### Note
Old LLK repositories (https://github.com/tenstorrent/tt-llk-gs, https://github.com/tenstorrent/tt-llk-wh-b0, https://github.com/tenstorrent/tt-llk-bh) are merged here and archived.

Development continues in this repository.
