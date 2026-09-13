#!/usr/bin/env bash
set -e

echo "=== [1/4] Updating Package Lists & Installing System Dependencies ==="
sudo apt update
sudo apt install -y build-essential gcc-multilib g++-multilib git cmake ninja-build python3-venv python3-pip

echo "=== [2/4] Creating Python Virtual Environment ==="
if [ ! -d "$HOME/zephyrpython-env" ]; then
    python3 -m venv "$HOME/zephyrpython-env"
    echo "Created virtual environment at $HOME/zephyrpython-env"
else
    echo "Virtual environment already exists at $HOME/zephyrpython-env"
fi

echo "=== [3/4] Activating Virtual Environment & Installing West ==="
source "$HOME/zephyrpython-env/bin/activate"
pip install --upgrade pip
pip install west

echo "=== [4/4] Setting Environment Variables ==="
if ! grep -q "ZEPHYR_TOOLCHAIN_VARIANT" "$HOME/zephyrpython-env/bin/activate"; then
    echo 'export ZEPHYR_TOOLCHAIN_VARIANT=host' >> "$HOME/zephyrpython-env/bin/activate"
    echo "Appended ZEPHYR_TOOLCHAIN_VARIANT=host to activation script."
fi

echo ""
echo "=================================================================="
echo " Setup Complete! Run the following command to activate your env: "
echo "   source ~/zephyrpython-env/bin/activate                         "
echo "=================================================================="
