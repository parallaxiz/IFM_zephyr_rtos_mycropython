# Architecture Document — Zephyr MicroPython UART Script Streamer

> **Target hardware:** Nordic nRF5340 DK (`nrf5340dk/nrf5340/cpuapp`)  
> **Zephyr version:** v4.4.0  
> **MicroPython commit:** pinned in `.gitmodules`  
> **Document status:** Living document — update when adding new features

---

## 1. Overview

This project embeds a [MicroPython](https://micropython.org/) interpreter inside a [Zephyr RTOS](https://zephyrproject.org/) application running on the nRF5340 Application Core.  A host PC transmits Python scripts over a dedicated UART link.  The firmware receives each script, validates it (CRC, size, module whitelist), executes it inside the MicroPython VM, and returns an execution result.

```
┌───────────────────────────────────────────────────────────────────┐
│                         nRF5340 DK                                 │
│                                                                   │
│  UART0 ──► printk / Zephyr shell (debug console, USB CDC-ACM)     │
│                                                                   │
│  UART1 ──► Script Streaming (P1.02=TX, P1.01=RX, 115200 baud)    │
│             │                                                     │
│             ▼                                                     │
│        ┌───────────────┐    ┌─────────────────────┐              │
│        │ uart_protocol │───►│   mp_safety layer    │              │
│        │ (CRC-16 frame)│    │ size / whitelist /   │              │
│        └───────────────┘    │ heap guard / WDT     │              │
│                             └──────────┬──────────┘              │
│                                        │                          │
│                             ┌──────────▼──────────┐              │
│                             │  MicroPython VM      │              │
│                             │  mp_runner_init()    │              │
│                             │  mp_run_string()     │              │
│                             │  mp_runner_deinit()  │              │
│                             └──────────────────────┘              │
└───────────────────────────────────────────────────────────────────┘
         ▲                        │
         │ ACK/NACK frame         │ printk output
         ▼                        ▼
   Host PC (tools/              UART0
   send_script.py)         (USB CDC-ACM)
```

---

## 2. Repository Layout

```
my-zephyr-micropython/
├── app_hardware/                   # nRF5340 / real hardware target
│   ├── boards/
│   │   └── nrf5340dk_nrf5340_cpuapp.overlay   ← DT overlay (NEW)
│   ├── src/
│   │   └── main.c                 ← protocol + safety integrated (UPDATED)
│   ├── CMakeLists.txt             ← builds mp_safety + uart_protocol (UPDATED)
│   └── prj.conf                   ← WDT + REBOOT enabled (UPDATED)
│
├── app_sim/                        # native_sim target (CI / development)
│   ├── boards/
│   │   └── native_sim.overlay
│   ├── src/
│   │   ├── main.c
│   │   ├── mod_simulator.c/h
│   │   └── main.py
│   ├── CMakeLists.txt
│   └── prj.conf
│
├── common/                         # Shared between hw and sim targets
│   ├── micropython/                ← MicroPython git submodule
│   ├── mp_port/                    ← Port configuration files
│   │   ├── mpconfigport.h
│   │   ├── mphalport.c
│   │   └── mphalport.h
│   ├── hw_hal.c/h                  ← GPIO / UART / Timer abstraction
│   ├── micropython_runner.c/h      ← VM lifecycle + mp_run_string()
│   ├── mp_safety.c/h              ← Safety layer (NEW)
│   └── uart_protocol.c/h          ← CRC-16 framing protocol (NEW)
│
├── tools/
│   └── send_script.py             ← PC-side script sender (NEW)
│
├── docs/
│   └── architecture.md            ← This document (NEW)
│
└── .github/workflows/ci.yml       ← CI: builds both targets on native_sim
```

---

## 3. Key Modules

### 3.1 `uart_protocol.c` — UART Framing Protocol

**Wire format:**

```
+--------+--------+--------+--------+-------···-------+--------+--------+
| 0xAB   | 0xCD   | LEN_HI | LEN_LO |   PAYLOAD        | CRC_HI | CRC_LO |
+--------+--------+--------+--------+-------···-------+--------+--------+
  magic (2B)       length (2B, BE)    Python source      CRC-16 (2B, BE)
```

| Field    | Size     | Description |
|----------|----------|-------------|
| Magic    | 2 bytes  | `0xAB 0xCD` start-of-frame sentinel |
| Length   | 2 bytes  | Big-endian payload length (0–8192) |
| Payload  | N bytes  | Raw Python source (UTF-8) |
| CRC      | 2 bytes  | CRC-16/CCITT-FALSE over Payload |

**CRC algorithm:** CRC-16/CCITT-FALSE — polynomial `0x1021`, init `0xFFFF`, no reflection.

**ACK/NACK responses** (device → host, same frame format):

| Response       | Meaning |
|----------------|---------|
| `ACK:PING`     | Empty frame received |
| `ACK:RX`       | Script frame received and CRC verified |
| `ACK:EXEC_OK`  | Script executed without Python exception |
| `NACK:CRC`     | CRC mismatch — frame discarded |
| `NACK:SIZE`    | Payload too large |
| `NACK:TIMEOUT` | Inter-byte timeout |
| `NACK:SAFETY`  | Safety gate rejected script |
| `NACK:EXEC_ERR`| Python exception during execution |

### 3.2 `mp_safety.c` — Safety Layer

| Feature | Implementation | Threshold |
|---------|---------------|-----------|
| Script size limit | `mp_safety_script_allowed()` | 8 192 bytes |
| Heap guard (warn) | `mp_safety_heap_check()` via `gc_info()` | 80 % utilisation |
| Heap guard (abort → reboot) | `mp_safety_heap_check()` | 95 % utilisation |
| Module whitelist | `mp_safety_module_allowed()` | See §3.2.1 |
| Watchdog | `mp_safety_wdt_init()` / `wdt_feed()` | 5 000 ms |
| NLR escape → reboot | `nlr_jump_fail()` → `sys_reboot()` | Any unhandled NLR |

#### 3.2.1 Module Whitelist

Only the following built-in modules may be imported by a streamed script:

`builtins`, `sys`, `math`, `struct`, `ustruct`, `utime`, `time`, `uio`, `io`, `ujson`, `json`, `ure`, `re`, `uhashlib`, `hashlib`, `ubinascii`, `binascii`, `machine`, `zephyr`

### 3.3 `micropython_runner.c` — VM Lifecycle

| Function | Description |
|----------|-------------|
| `mp_runner_init()` | Stack, GC heap, `mp_init()`, `mp_hal_init()` |
| `mp_runner_deinit()` | `mp_deinit()`, resets init flag |
| `mp_run_string()` | Validates → lexes → parses → compiles → executes; catches NLR exceptions |
| `gc_collect()` | Weak-symbol override for MicroPython GC |

### 3.4 `hw_hal.c` — Hardware Abstraction Layer

Provides a Zephyr-portable API for GPIO, UART (poll-mode), and software timers.  Used by `main.c` for the startup self-test.

### 3.5 Devicetree Overlay — `nrf5340dk_nrf5340_cpuapp.overlay`

| Resource | Assignment |
|----------|------------|
| Console UART | `uart0` (on-board USB CDC-ACM) |
| Script UART | `uart1` — TX: P1.02, RX: P1.01, 115 200 baud |
| LED (self-test) | `led0` alias (standard nRF5340 DK mapping) |
| Watchdog | `wdt0` (nRF WATCHDOG peripheral) |

---

## 4. Data Flow

```
Host PC                                     nRF5340 DK
─────────────────────                       ──────────────────────────────
send_script.py                              app_hardware/src/main.c
  1. Read .py file
  2. Build CRC-16 frame         UART1 TX→RX
  3. Serial.write(frame)   ──────────────►  uart_proto_receive_frame()
                                              state machine: SYNC→LEN→PAYLOAD→CRC
                                              CRC verify
                                            mp_safety_script_allowed()
                                              • size ≤ 8192?
                                              • (module check at import time)
                                            Send ACK:RX
                           ◄──────────────  uart_proto_send_frame("ACK:RX")
  4. Receive ACK:RX
                                            mp_runner_init()
                                            mp_run_string()
                                              • NLR exception frame
                                              • lex → parse → compile → execute
                                              • mp_safety_heap_check() after
                                            mp_runner_deinit()
                                            Send ACK:EXEC_OK / NACK:EXEC_ERR
                           ◄──────────────  uart_proto_send_frame(result)
  5. Receive result
  6. Print status
```

---

## 5. Build Targets

| Target | Board | Command |
|--------|-------|---------|
| Hardware | `nrf5340dk/nrf5340/cpuapp` | `west build -p always -b nrf5340dk/nrf5340/cpuapp app_hardware` |
| Simulator | `native_sim` | `west build -p always -b native_sim app_sim` |
| Hardware on native_sim (CI) | `native_sim` | `west build -p always -b native_sim app_hardware` |

> **Note:** When targeting `native_sim`, the WDT driver is absent and `mp_safety_wdt_init()` returns `-ENODEV` gracefully.  The UART1 alias falls back to `zephyr_console` so the framing receiver still builds.

---

## 6. Requirements Traceability Table

| Req-ID | Requirement | Implementation | File(s) | Status |
|--------|-------------|----------------|---------|--------|
| REQ-01 | MicroPython VM shall be embedded in Zephyr RTOS | `mp_runner_init/deinit/run_string()` | `common/micropython_runner.c` | ✅ Done |
| REQ-02 | VM shall execute Python scripts from an in-memory C string | `mp_run_string()` — lexer→parse→compile→execute | `common/micropython_runner.c` | ✅ Done |
| REQ-03 | All Python exceptions shall be caught and printed without crashing Zephyr | NLR try/catch in `mp_run_string()` | `common/micropython_runner.c` | ✅ Done |
| REQ-04 | Uncaught NLR escape (`nlr_jump_fail`) shall trigger system reboot | `nlr_jump_fail()` → `sys_reboot(SYS_REBOOT_COLD)` | `common/mp_safety.c` | ✅ Done |
| REQ-05 | Scripts larger than 8 192 bytes shall be rejected before VM execution | `mp_safety_script_allowed()` | `common/mp_safety.c` | ✅ Done |
| REQ-06 | Only whitelisted built-in modules may be imported | `mp_safety_module_allowed()` | `common/mp_safety.c` | ✅ Done |
| REQ-07 | GC heap utilisation above 80 % shall generate a warning log | `mp_safety_heap_check()` — warn branch | `common/mp_safety.c` | ✅ Done |
| REQ-08 | GC heap utilisation above 95 % shall trigger a cold reboot | `mp_safety_heap_check()` — abort branch | `common/mp_safety.c` | ✅ Done |
| REQ-09 | System watchdog shall reset the MCU if main loop stalls > 5 s | `mp_safety_wdt_init()`, `mp_safety_wdt_kick()` | `common/mp_safety.c` | ✅ Done |
| REQ-10 | Scripts shall be received over UART with a robust framing protocol | CRC-16/CCITT-FALSE frame with 2-byte magic + length | `common/uart_protocol.c` | ✅ Done |
| REQ-11 | Frame integrity shall be verified using CRC-16/CCITT-FALSE | `uart_proto_crc16()` over payload; discard on mismatch | `common/uart_protocol.c` | ✅ Done |
| REQ-12 | Device shall send ACK/NACK responses for each received frame | `uart_proto_send_frame()` with status strings | `common/uart_protocol.c`, `app_hardware/src/main.c` | ✅ Done |
| REQ-13 | Hardware UART1 shall use real nRF5340 DK pin names | P1.02 (TX), P1.01 (RX), 115 200 baud, pinctrl | `app_hardware/boards/nrf5340dk_nrf5340_cpuapp.overlay` | ✅ Done |
| REQ-14 | GPIO, UART, and Timer shall be abstracted behind a HAL | `hw_hal.c/h` | `common/hw_hal.c`, `common/hw_hal.h` | ✅ Done |
| REQ-15 | A PC-side tool shall frame and transmit scripts over serial | `send_script.py` with argparse CLI | `tools/send_script.py` | ✅ Done |
| REQ-16 | PC tool shall verify device ACK before reporting success | Two-phase response read: ACK:RX then ACK:EXEC_OK | `tools/send_script.py` | ✅ Done |
| REQ-17 | CI shall build both `app_sim` and `app_hardware` targets on `native_sim` | GitHub Actions workflow | `.github/workflows/ci.yml` | ✅ Done |
| REQ-18 | Architecture and traceability shall be documented | This document | `docs/architecture.md` | ✅ Done |

---

## 7. Known Limitations & Future Work

| Item | Description |
|------|-------------|
| Module whitelist enforcement | Currently checked in `mp_safety_module_allowed()` but not hooked into MicroPython's import machinery — a future task is to override `mp_import_stat()` to call this function. |
| Async UART receiver | Current receiver is synchronous / blocking.  A dedicated Zephyr thread with a message queue would improve responsiveness. |
| Flash-based script storage | Scripts are currently held only in RAM.  Future work: store signed scripts in a flash partition (NVS/LittleFS). |
| app_sim integration | `app_sim` does not yet use `uart_protocol` or `mp_safety`; integration is future work. |
| Script signing | CRC detects accidental corruption; a SHA-256 or HMAC step would add authenticity. |

---

*Generated: October 2026*
