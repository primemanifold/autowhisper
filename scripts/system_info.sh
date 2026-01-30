#!/bin/bash

# System Information Gathering Script
# Collects GPU, CPU, RAM, CUDA, and NVIDIA driver information

LOG_FILE="${1:-system_info_$(date +%Y%m%d_%H%M%S).log}"

echo "Gathering system information..."
echo "Output will be saved to: $LOG_FILE"

{
    echo "========================================"
    echo "SYSTEM INFORMATION REPORT"
    echo "Generated: $(date)"
    echo "Hostname: $(hostname)"
    echo "========================================"
    echo ""

    # CPU Information
    echo "========================================"
    echo "CPU INFORMATION"
    echo "========================================"
    if command -v lscpu &> /dev/null; then
        lscpu
    else
        cat /proc/cpuinfo
    fi
    echo ""

    # RAM Information
    echo "========================================"
    echo "RAM INFORMATION"
    echo "========================================"
    free -h
    echo ""
    echo "Detailed memory info:"
    cat /proc/meminfo | head -20
    echo ""

    # GPU Information
    echo "========================================"
    echo "GPU INFORMATION"
    echo "========================================"
    if command -v nvidia-smi &> /dev/null; then
        echo "--- NVIDIA SMI Output ---"
        nvidia-smi
        echo ""
        echo "--- GPU Details ---"
        nvidia-smi -q
        echo ""
        echo "--- GPU List ---"
        nvidia-smi -L
    else
        echo "nvidia-smi not found. Checking for other GPUs..."
        if command -v lspci &> /dev/null; then
            lspci | grep -i vga
            lspci | grep -i nvidia
            lspci | grep -i 3d
        fi
    fi
    echo ""

    # NVIDIA Driver Information
    echo "========================================"
    echo "NVIDIA DRIVER INFORMATION"
    echo "========================================"
    if command -v nvidia-smi &> /dev/null; then
        echo "Driver Version: $(nvidia-smi --query-gpu=driver_version --format=csv,noheader | head -1)"
        echo ""
        echo "--- Kernel Module Info ---"
        if command -v modinfo &> /dev/null; then
            modinfo nvidia 2>/dev/null | head -20 || echo "nvidia module info not available"
        fi
    else
        echo "NVIDIA driver not detected"
    fi
    echo ""

    # CUDA Information
    echo "========================================"
    echo "CUDA INFORMATION"
    echo "========================================"

    # CUDA Compiler
    echo "--- CUDA Compiler (nvcc) ---"
    if command -v nvcc &> /dev/null; then
        nvcc --version
    else
        echo "nvcc not found in PATH"
        # Check common CUDA locations
        for cuda_path in /usr/local/cuda*/bin/nvcc /opt/cuda*/bin/nvcc; do
            if [ -f "$cuda_path" ]; then
                echo "Found: $cuda_path"
                "$cuda_path" --version
            fi
        done
    fi
    echo ""

    # CUDA Toolkit Paths
    echo "--- CUDA Toolkit Paths ---"
    echo "CUDA_HOME: ${CUDA_HOME:-not set}"
    echo "CUDA_PATH: ${CUDA_PATH:-not set}"
    echo ""

    echo "--- Installed CUDA Versions ---"
    ls -la /usr/local/ 2>/dev/null | grep cuda || echo "No CUDA found in /usr/local/"
    echo ""

    # CUDA Libraries
    echo "--- CUDA Libraries ---"
    if [ -d "/usr/local/cuda/lib64" ]; then
        ls -la /usr/local/cuda/lib64/*.so* 2>/dev/null | head -20
    fi
    echo ""

    # cuDNN Information
    echo "--- cuDNN Information ---"
    cudnn_header="/usr/local/cuda/include/cudnn_version.h"
    if [ -f "$cudnn_header" ]; then
        echo "cuDNN version from header:"
        grep -E "CUDNN_MAJOR|CUDNN_MINOR|CUDNN_PATCHLEVEL" "$cudnn_header"
    else
        # Try alternate location
        cudnn_header="/usr/include/cudnn_version.h"
        if [ -f "$cudnn_header" ]; then
            echo "cuDNN version from header:"
            grep -E "CUDNN_MAJOR|CUDNN_MINOR|CUDNN_PATCHLEVEL" "$cudnn_header"
        else
            echo "cuDNN header not found"
        fi
    fi

    # Check for cuDNN libraries
    echo ""
    echo "cuDNN libraries:"
    find /usr -name "libcudnn*.so*" 2>/dev/null | head -10 || echo "No cuDNN libraries found"
    echo ""

    # Python CUDA packages
    echo "========================================"
    echo "PYTHON CUDA PACKAGES"
    echo "========================================"
    if command -v python3 &> /dev/null; then
        echo "--- PyTorch CUDA ---"
        python3 -c "import torch; print(f'PyTorch version: {torch.__version__}'); print(f'CUDA available: {torch.cuda.is_available()}'); print(f'CUDA version: {torch.version.cuda}'); print(f'cuDNN version: {torch.backends.cudnn.version()}'); print(f'GPU count: {torch.cuda.device_count()}')" 2>/dev/null || echo "PyTorch not installed or CUDA not available"
        echo ""

        echo "--- TensorFlow GPU ---"
        python3 -c "import tensorflow as tf; print(f'TensorFlow version: {tf.__version__}'); print(f'GPU devices: {tf.config.list_physical_devices(\"GPU\")}')" 2>/dev/null || echo "TensorFlow not installed or GPU not available"
    else
        echo "Python3 not found"
    fi
    echo ""

    # Environment Variables
    echo "========================================"
    echo "RELEVANT ENVIRONMENT VARIABLES"
    echo "========================================"
    echo "PATH entries with cuda:"
    echo "$PATH" | tr ':' '\n' | grep -i cuda
    echo ""
    echo "LD_LIBRARY_PATH: ${LD_LIBRARY_PATH:-not set}"
    echo ""

    echo "========================================"
    echo "END OF REPORT"
    echo "========================================"

} > "$LOG_FILE" 2>&1

echo "System information saved to: $LOG_FILE"
echo ""
echo "Quick summary:"
grep -A1 "Driver Version:" "$LOG_FILE" 2>/dev/null || echo "No NVIDIA driver found"
grep -A3 "CUDA Compiler" "$LOG_FILE" 2>/dev/null | tail -2
