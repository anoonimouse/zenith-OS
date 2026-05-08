#!/bin/bash
# TinyOS Toolchain Setup Script for WSL (Ubuntu)
set -e

echo "Updating package lists..."
sudo apt update

echo "Installing build tools, assembler, and emulator..."
sudo apt install -y build-essential nasm qemu-system-x86 grub-pc-bin grub-common xorriso

echo "Setup complete! You are ready to build TinyOS."
