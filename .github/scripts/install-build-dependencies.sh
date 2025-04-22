#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2025 Tenstorrent AI ULC
#
# SPDX-License-Identifier: Apache-2.0


prep_ubuntu_runtime()
{
    echo "Preparing ubuntu ..."
    # Update the list of available packages
    apt-get update
    apt-get install -y --no-install-recommends ca-certificates gpg lsb-release wget software-properties-common gnupg
    wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
    echo "deb http://apt.llvm.org/$UBUNTU_CODENAME/ llvm-toolchain-$UBUNTU_CODENAME-17 main" | tee /etc/apt/sources.list.d/llvm-17.list
    apt-get update
}

prep_ubuntu_build()
{
    echo "Preparing ubuntu ..."
    # Update the list of available packages
    apt-get update
    apt-get install -y --no-install-recommends ca-certificates gpg lsb-release wget software-properties-common gnupg
    # The below is to bring cmake from kitware
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | gpg --dearmor - | tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
    echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $UBUNTU_CODENAME main" | tee /etc/apt/sources.list.d/kitware.list >/dev/null
    apt-get update
}

# We currently have an affinity to clang as it is more thoroughly tested in CI
# However g++-12 and later should also work

install_llvm() {
    LLVM_VERSION="17"
    echo "Checking if LLVM $LLVM_VERSION is already installed..."
    if command -v clang-$LLVM_VERSION &> /dev/null; then
        echo "LLVM $LLVM_VERSION is already installed. Skipping installation."
    else
        echo "Installing LLVM $LLVM_VERSION..."
        TEMP_DIR=$(mktemp -d)
        wget -P $TEMP_DIR https://apt.llvm.org/llvm.sh
        chmod u+x $TEMP_DIR/llvm.sh
        $TEMP_DIR/llvm.sh $LLVM_VERSION
        rm -rf "$TEMP_DIR"
    fi
}

# Install g++-12 if on Ubuntu 22.04
install_gcc12() {
    if [ $VERSION == "22.04" ]; then
        echo "Detected Ubuntu 22.04, installing g++-12..."
        apt-get install -y --no-install-recommends g++-12 gcc-12
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 12
        update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 12
        update-alternatives --set gcc /usr/bin/gcc-12
        update-alternatives --set g++ /usr/bin/g++-12
    fi
}

prep_ubuntu_build
prep_ubuntu_runtime
install_llvm
install_gcc12
