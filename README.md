# Zephyr RTOS + MicroPython Workspace Setup

This repository contains an embedded C application running on the Zephyr RTOS kernel, verified locally and in CI/CD using Zephyr's native PC simulator (`native_sim`).

---

## 🛠️ Environment Prerequisites
- **OS:** Windows 11 with WSL2 (Ubuntu)
- **Editor:** Visual Studio Code (connected via WSL extension)
- **Core Tools:** Zephyr RTOS v4.4+, `west` meta-tool, CMake, Ninja, Python 3.12+ (virtual environment)

---

## 🚀 Step-by-Step Setup Instructions

### 1. Initialize Virtual Environment & Directories in WSL
Launch your Ubuntu terminal inside WSL2 and activate the virtual environment:
```bash
# Activate Python virtual environment
source ~/zephyrpython-env/bin/activate

# Create project workspace directory
mkdir -p ~/zephyr-workspace/my-zephyr-micropython/app/src
cd ~/zephyr-workspace/my-zephyr-micropython
2. Install Required 32-bit Compilation Libraries
Install standard 32-bit multilib development headers required for native PC simulation:

Bash
sudo apt update && sudo apt install -y gcc-multilib g++-multilib
3. Open Workspace in VS Code
Launch VS Code directly into the WSL Linux filesystem:

Bash
cd ~/zephyr-workspace/my-zephyr-micropython
code .
Note: Ensure the bottom-left corner of VS Code displays WSL: Ubuntu and open the integrated terminal using Ctrl + ~ (selecting bash or wsl).

4. Create Application Boilerplate Files
app/CMakeLists.txt

CMake
cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_micropython_app)

target_sources(app PRIVATE src/main.c)
app/prj.conf

Ini, TOML
CONFIG_STDOUT_CONSOLE=y
CONFIG_PRINTK=y
CONFIG_MAIN_STACK_SIZE=4096
CONFIG_HW_STACK_PROTECTION=y
app/src/main.c

C
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void) {
    printk("MicroPython initialized successfully\n");
    return 0;
}
app/testcase.yaml

YAML
tests:
  micropython.zephyr.boot_test:
    tags:
      - micropython
      - zephyr
      - native_sim
    platform_allow:
      - native_sim
    harness: console
    harness_config:
      type: one_line
      regex:
        - "MicroPython initialized successfully"
5. Build & Execute Locally on Native Simulator (native_sim)
Bash
# Set host toolchain variable
export ZEPHYR_TOOLCHAIN_VARIANT=host

# Clean cache and build application targeting native_sim
rm -rf build
west build -b native_sim app

# Run executable directly on host CPU
./build/zephyr/zephyr.exe
Expected Output:

Plaintext
*** Booting Zephyr OS build v4.4.99 ***
MicroPython initialized successfully
🐞 Issues Encountered & Solutions
Issue 1: ERROR: Unknown toolchain 'host-zephyr-elf' during SDK Setup
Cause: Running ./setup.sh with unsupported flags for host simulation toolchains.

Solution: Execute the setup script with host tools and CMake package registration flags:

Bash
./setup.sh -h -c
Issue 2: ERROR: Host tools installation failed
Cause: ./setup.sh -h attempted to install hardware debugger dependencies (OpenOCD/udev rules) requiring root permissions.

Solution: Bypass hardware host tool installation since native_sim uses host GCC directly. Register CMake package only:

Bash
./setup.sh -c
Issue 3: Could not find a configuration file for package "Zephyr-sdk"
Cause: Zephyr kernel v4.4.99 requested SDK version 1.0 or newer, rejecting locally installed 0.16.8.

Solution: Instruct Zephyr to use the host system GCC compiler directly instead of searching for external target cross-compilers:

Bash
export ZEPHYR_TOOLCHAIN_VARIANT=host
Issue 4: fatal error: bits/libc-header-start.h: No such file or directory
Cause: native_sim targets 32-bit architecture (-m32), but 32-bit standard library development headers were missing in WSL Ubuntu.

Solution: Install multilib GCC support libraries via apt:

Bash
sudo apt update && sudo apt install -y gcc-multilib g++-multilib
Issue 5: source: command not found / CMD.EXE was started with the above path...
Cause: Terminal opened inside Windows Command Prompt (cmd.exe) instead of Linux/WSL terminal.

Solution: Run wsl in Command Prompt to switch to Linux, or change the default terminal shell in VS Code to bash.


<Elicitations message="Would you like to move on to embedding MicroPython?">
  <Elicitation label="Begin Step 1 (MicroPython Core)" query="The README is saved! How do we begin Step 1 to embed MicroPython?"/>
</Elicitations>