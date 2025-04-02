#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0
set -e

SFPI_RELEASE_URL=${SFPI_RELEASE_URL:-"https://github.com/tenstorrent/sfpi/releases/download/v6.7.0/sfpi-release.tgz"}
mkdir -p /home/sfpi
wget "$SFPI_RELEASE_URL" -O /home/sfpi/sfpi-release.tgz
tar -xzf /home/sfpi/sfpi-release.tgz -C /home/sfpi
ls -l /home/sfpi
rm /home/sfpi/sfpi-release.tgz
