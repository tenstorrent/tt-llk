#!/usr/bin/env bash

# Save original shell options
ORIGINAL_OPTS=$(set +o)  # Save all current shell options
set -o pipefail  # Catch errors in pipelines

# Function to print usage info
usage() {
    echo "Usage: $0 [--reuse] [--clean] [--help]"
    echo "  --reuse      Skip some setup steps if a virtual environment already exists"
    echo "  --clean      Remove existing setup (virtual environment, temporary files)"
    echo "  --help       Display this help message"
    exit 1
}

# Variables
REUSE=false
CLEAN=false
LOGFILE="setup.log"
TT_SMI_REPO="https://github.com/tenstorrent/tt-smi"
SFPI_RELEASE_URL="https://github.com/tenstorrent/sfpi/releases/download/v6.5.0-sfpi/sfpi-release.tgz"

exec 3>&1 4>&2
# Redirect output to a log file
exec > >(tee -i "$LOGFILE") 2>&1

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --reuse) REUSE=true ;;
        --clean) CLEAN=true ;;
        --help) usage ;;
        *) echo "Unknown option: $1"; usage ;;
    esac
    shift
done

# Cleanup logic
if [[ "$CLEAN" == true ]]; then
    echo "Cleaning up environment..."
    rm -rf tt-smi sfpi .venv arch.dump sfpi-release.tgz
    echo "Cleanup complete."
    exit 0
fi

# Check Python version
PYTHON_VERSION=$(python3 --version 2>&1 | awk '{print $2}')
if [[ "$PYTHON_VERSION" < "3.8" ]]; then
    echo "Error: Python 3.8 or higher is required. Detected version: $PYTHON_VERSION"
    exit 1
fi

# Deactivate any active virtual environment
if [[ -n "$VIRTUAL_ENV" ]]; then
    echo "Deactivating current Python environment: $VIRTUAL_ENV"
    deactivate
fi

# If not reusing, perform setup
if [[ "$REUSE" == false ]]; then
    echo "Updating system packages..."
    sudo apt update
    sudo apt install -y curl software-properties-common build-essential libyaml-cpp-dev libhwloc-dev libzmq3-dev git-lfs xxd wget

    # Clone tt-smi repository if not already cloned
    if [ ! -d "tt-smi" ]; then
        echo "Cloning tt-smi repository..."
        git clone "$TT_SMI_REPO"
    else
        echo "tt-smi repository already exists. Skipping clone."
    fi

    # Create virtual environment in the current directory if not already created
    if [ ! -d ".venv" ]; then
        echo "Creating Python virtual environment in the current directory..."
        python3 -m venv .venv
        if [ ! -d ".venv" ]; then
            echo "Failed to create virtual environment"
            exit 1
        fi
    else
        echo "Virtual environment already exists in the current directory. Skipping creation."
    fi
    source .venv/bin/activate

    echo "Upgrading pip..."
    pip install --upgrade pip

    echo "Installing tt-smi"
    cd tt-smi
    pip install .
    cd ..

    echo "Installing required Python packages..."
    pip install -r requirements.txt

    # Detect architecture for chip
    echo "Running tt-smi -ls to detect architecture..."
    ARCH_DUMP=$(mktemp)
    tt-smi -ls > "$ARCH_DUMP"
    result=$(python3 helpers/find_arch.py "Wormhole" "Blackhole" "$ARCH_DUMP")
    rm -f "$ARCH_DUMP"
    echo "Detected architecture: $result"

    if [ -z "$result" ]; then
        echo "Error: Architecture detection failed!"
        exit 1
    fi

    export CHIP_ARCH="$result"
    echo "CHIP_ARCH is: $CHIP_ARCH"

    # Install tt-exalens
    echo "Installing tt-exalens..."
    pip install git+https://github.com/tenstorrent/tt-exalens.git@cdca310241827b05a1752db2a15edd11e89a9712

    # Download and extract SFPI release
    if [ ! -f "sfpi-release.tgz" ]; then
        echo "Downloading SFPI release..."
        wget "$SFPI_RELEASE_URL" -O sfpi-release.tgz
    else
        echo "SFPI release already downloaded. Skipping."
    fi
    echo "Extracting SFPI release..."
    tar -xzvf sfpi-release.tgz
    rm -f sfpi-release.tgz
else   # Reuse existing environment setup
    # Deactivate any active virtual environment
    if [[ -n "$VIRTUAL_ENV" ]]; then
        echo "Deactivating current Python environment: $VIRTUAL_ENV"
        deactivate
    fi
    echo "Reusing existing virtual environment setup..."
    source .venv/bin/activate

    echo "Running tt-smi -ls to detect architecture..."
    ARCH_DUMP=$(mktemp)
    tt-smi -ls > "$ARCH_DUMP"
    result=$(python3 helpers/find_arch.py "Wormhole" "Blackhole" "$ARCH_DUMP")
    rm -f "$ARCH_DUMP"
    echo "Detected architecture: $result"

    if [ -z "$result" ]; then
        echo "Error: Architecture detection failed!"
        exit 1
    fi

    echo "Setting CHIP_ARCH variable..."
    export CHIP_ARCH="$result"
    echo "CHIP_ARCH is: $CHIP_ARCH"
fi

echo "Setup completed successfully!"
exec 1>&3 2>&4
# Restore original shell options at the end of the script
eval "$ORIGINAL_OPTS"
