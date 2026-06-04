# STM32F407 Modbus RTU ↔ TCP Transparent Bridge

Industrial-grade transparent gateway bridging Ethernet-based Modbus TCP clients with legacy RS485-based Modbus RTU serial networks. The device acts as a **Modbus TCP Server** (port 502) and a **Modbus RTU Master** on the serial bus.

[![Build & CI](https://github.com/mahdi-benhassen/stm32_modbus-rtu-tcp-bridge/actions/workflows/build.yml/badge.svg)](https://github.com/mahdi-benhassen/stm32_modbus-rtu-tcp-bridge/actions/workflows/build.yml)

## Architecture

```
┌─────────────┐       TCP :502       ┌──────────────────────┐      RS485        ┌──────────┐
│  SCADA /    │◄═══════════════════►│  STM32F407 Bridge     │◄═════════════════►│  RTU     │
│  Modbus TCP │   MBAP + PDU         │  ┌──────┐ ┌────────┐ │  UnitID + PDU    │  Slaves  │
│  Client     │                      │  │ TCP  │→│ Bridge │→│  + CRC16         │  1..247  │
└─────────────┘                      │  │Server│ │ Engine │ │                   └──────────┘
                                     │  └──────┘ └────────┘ │
                                     │    FreeRTOS + lwIP    │
                                     └──────────────────────┘
```

The bridge is **transparent** — it does not inspect Modbus function codes or data payloads. It only translates between the network and serial frame formats:

- **TCP → RTU**: strips MBAP header, extracts PDU, appends Unit ID + CRC16
- **RTU → TCP**: strips CRC16, caches MBAP header fields (Transaction ID, Protocol ID, Unit ID), rebuilds TCP frame

## Hardware

| Component | Part | Purpose |
|-----------|------|---------|
| MCU | STM32F407VGT6 (Cortex-M4, 168 MHz) | Main processor |
| Ethernet PHY | LAN8720A or DP83848 (RMII) | 10/100 Ethernet |
| RS485 Transceiver | MAX485 / SP3485 | Half-duplex RS485 interface |
| USART1 | PA9 (TX), PA10 (RX) | RS485 data |
| DE/RE Control | PB0 | Driver enable |

### Peripheral Mapping

| Peripheral | Function | Pins |
|-----------|----------|------|
| USART1 | RS485 | PA9 (TX), PA10 (RX), PB0 (DE) |
| TIM2 | 3.5-char silence | Internal |
| TIM3 | Response timeout | Internal |
| DMA2 Stream7 | USART1 TX | Internal |
| ETH MAC | RMII | PA1, PA2, PA7, PB11-13, PC1, PC4-5 |

## Software Stack

| Layer | Component | Version |
|-------|-----------|---------|
| RTOS | FreeRTOS Kernel | V10.6.2 |
| TCP/IP | lwIP (Socket API) | 2.2.0 |
| HAL | STM32F4xx HAL Driver | main branch |
| CMSIS | ARM CMSIS 5 (Core) + STM32F4 (Device) | main branch |
| Toolchain | arm-none-eabi-gcc | 13.3 |

### Task Structure

| Task | Priority | Role |
|------|----------|------|
| `TcpServer` | 3 (idle+3) | Accept TCP clients on port 502, receive frames, forward to bridge engine |
| `BridgeEng` | 4 (idle+4) | Translate TCP↔RTU, manage RS485 bus, handle timeouts |
| `tcpip_thread` | 2 (idle+2) | lwIP internal thread |

## Build

### Prerequisites

- `arm-none-eabi-gcc` (ARM GNU Toolchain 13.3+)
- `make`
- `curl`, `tar`, `git` (for dependency fetch)

### Quick Start

```bash
# 1. Clone the repository
git clone https://github.com/mahdi-benhassen/stm32_modbus-rtu-tcp-bridge.git
cd stm32_modbus-rtu-tcp-bridge

# 2. Download HAL, CMSIS, FreeRTOS, lwIP dependencies
bash scripts/fetch_deps.sh

# 3. Build
make

# 4. Flash (requires OpenOCD + ST-Link)
make flash
```

Output files:
- `modbus_bridge.elf` — ELF with debug symbols
- `modbus_bridge.hex` — Intel HEX
- `modbus_bridge.bin` — Raw binary for flashing

### CI/CD

GitHub Actions builds the firmware on every push and pull request:
- **Debug build**: `-Og -g3` (both build matrix rows currently use debug opts)
- **Lint**: cppcheck static analysis on `Core/Src/`

Artifacts (`.elf`, `.hex`, `.bin`, `.map`) are retained for 30 days.

## Configuration

| Parameter | File | Default |
|-----------|------|---------|
| RS485 baud rate | `app_config.h` | 19200, 8E1 |
| TCP port | `app_config.h` | 502 |
| Static IP | `main.c` | 192.168.1.100/24 |
| Slave response timeout | `app_config.h` | 1000 ms |
| Max TCP clients | `app_config.h` | 4 |
| PHY address | `main.h` | 0x01 |

## Protocol Details

### Modbus TCP Frame (Downstream → RS485)

```
[MBAP Header 7 bytes] + [PDU n bytes]
- Transaction ID (2)  = copied transparently
- Protocol ID    (2)  = 0x0000 (validated)
- Length         (2)  = PDU length + 1 (Unit ID)
- Unit ID        (1)  = RS485 slave address (1-247)
```

### Modbus RTU Frame (Upstream → TCP)

```
[Unit ID 1 byte] + [PDU n bytes] + [CRC16 2 bytes]
- CRC polynomial: 0xA001 (reflected 0x8005)
- CRC init value: 0xFFFF
```

### Timing (at 19200 bps)

| Parameter | Value | Timer |
|-----------|-------|-------|
| 3.5-char silence (frame end) | 2005 µs | TIM2 |
| 1.5-char inter-char timeout | 860 µs | — |
| Slave response timeout | 1000 ms (configurable) | TIM3 |

### Exception Handling

| Condition | Exception | MBAP Code |
|-----------|-----------|-----------|
| Slave no response / timeout | Gateway Target Device Failed | 0x0B |
| CRC mismatch on RX | Gateway Target Device Failed | 0x0B |
| Non-zero Protocol ID | Gateway Path Unavailable | 0x0A |
| Invalid Unit ID (0 or >247) | Frame dropped | — |
| Unit ID mismatch on RX | Gateway Target Device Failed | 0x0B |

## Project Structure

```
stm32_modbus-rtu-tcp-bridge/
├── Core/
│   ├── Inc/
│   │   ├── main.h                  # Pin/peripheral mappings
│   │   ├── app_config.h            # Timing, frame sizes, task config
│   │   ├── FreeRTOSConfig.h        # FreeRTOS kernel configuration
│   │   ├── lwipopts.h              # lwIP stack configuration
│   │   ├── stm32f4xx_hal_conf.h    # HAL module enables + type prereqs
│   │   ├── modbus_crc.h            # CRC-16 API
│   │   ├── rs485_driver.h          # RS485 half-duplex driver API
│   │   ├── bridge_engine.h         # TCP↔RTU translation engine API
│   │   ├── tcp_server.h            # TCP server task API
│   │   └── stm32f4xx_it.h          # ISR declarations
│   └── Src/
│       ├── main.c                  # Entry point, clock config, task creation
│       ├── system_stm32f4xx.c      # SystemInit, HAL tick, delay
│       ├── stm32f4xx_it.c          # ISR implementations
│       ├── modbus_crc.c            # CRC-16/MODBUS computation
│       ├── rs485_driver.c          # RS485 DE control, DMA TX, IRQ RX, timers
│       ├── bridge_engine.c         # Core protocol translation engine
│       ├── tcp_server.c            # lwIP TCP server (select-based)
│       ├── ethernetif.c            # lwIP Ethernet interface (minimal stub)
│       ├── sys_arch.c              # lwIP OS abstraction for FreeRTOS
│       ├── syscalls.c              # newlib-nano syscall stubs
│       └── system_stm32f4xx.c      # SystemInit, core clock, HAL tick
├── scripts/
│   └── fetch_deps.sh               # Downloads STM32Cube, FreeRTOS, lwIP
├── Middlewares/
│   └── Third_Party/
│       └── lwIP/port/arch/
│           ├── cc.h                 # lwIP compiler/platform adaptation
│           └── sys_arch.h           # lwIP OS abstraction type mappings
├── Drivers/                        # (populated by fetch_deps.sh)
├── Makefile
├── stm32f407_flash.ld
├── startup_stm32f407xx.s
├── .github/workflows/build.yml     # GitHub Actions CI pipeline
└── .gitignore
```

## Known Limitations

1. **Ethernet interface is a minimal stub** — the full RMII TX/RX path uses a HAL API incompatible with the standalone HAL driver repo. The `ethernetif.c` provides required symbols for linking but does not perform actual Ethernet I/O. Use the monolithic STM32CubeF4 HAL for full ETH support.
2. **Hardcoded static IP** — 192.168.1.100. DHCP is compiled in but not started.
3. **No 1.5-char inter-character timeout** — only 3.5-char frame-end silence is detected.
4. **No runtime configuration** — baud rate, IP, and timeouts are compile-time constants.
5. **Single-master RS485 bus** — multiple TCP requests are serialized through a mutex.

## License

MIT License — see `LICENSE.md` in the STM32CubeF4 repository for HAL/CMSIS terms.

---

For detailed operational documentation, see [docs/USER_MANUAL.md](docs/USER_MANUAL.md).
