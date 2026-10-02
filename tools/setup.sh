#!/bin/bash
# One-time setup: fetch the decomp and its tools, apply the mod's hook patch,
# and create the Python venv the decomp needs. Ubuntu 22.04/24.04.
set -e
cd "$(dirname "$0")/.."

if [ "$1" = "--deps" ]; then
    sudo apt-get update
    sudo apt-get install -y $(cat decomp/packages.txt 2>/dev/null || echo "binutils-mips-linux-gnu gcc-mips-linux-gnu build-essential cmake python3 python3-venv python3-pip curl")
fi

git submodule update --init --recursive

if git -C decomp apply --reverse --check ../patches/0001-nutsandbolts64-hooks.patch 2>/dev/null; then
    echo "hook patch already applied"
else
    git -C decomp apply ../patches/0001-nutsandbolts64-hooks.patch
    echo "applied hook patch"
fi

if [ ! -x decomp/.venv/bin/python3 ]; then
    python3 -m venv decomp/.venv
    decomp/.venv/bin/python3 -m pip install -q -r decomp/requirements.txt
fi
python3 -c "import PIL" 2>/dev/null || python3 -m pip install --user pillow

command -v cargo >/dev/null || echo "NOTE: install Rust (https://rustup.rs) - the decomp's ROM tools need cargo"
echo "setup done. Now: make BASEROM=\"/path/to/Banjo-Kazooie (USA).z64\"   (quote the path; Banjo-Kazooie USA v1.0, not Tooie)"
