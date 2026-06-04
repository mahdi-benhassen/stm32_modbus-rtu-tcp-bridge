# Transparent Modbus RTU ↔ TCP Bridge — User Manual

## 1. Product Overview

This device is a **transparent protocol bridge** that converts Modbus TCP frames (Ethernet) to Modbus RTU frames (RS485) and vice versa. It operates at the **frame level** — it does not interpret Modbus function codes, register addresses, or data payloads. This means any standard Modbus RTU slave device can be accessed remotely via TCP without any modification to the slave or the SCADA/master software.

### Key Features

- Transparent Modbus TCP (port 502) ↔ Modbus RTU (RS485) translation
- Supports up to **4 simultaneous TCP clients**
- Handles up to **247 Modbus RTU slaves** (Unit IDs 1–247) behind a single IP address
- Automatic CRC-16 generation and validation
- Half-duplex RS485 bus management (DE/RE control)
- 3.5-character silence detection for frame-end delimiting
- Configurable slave response timeout
- Graceful exception handling (0x0A, 0x0B) on communication failures
- FreeRTOS-based preemptive multitasking

---

## 2. Hardware Setup

### 2.1 Required Components

| Component | Quantity | Notes |
|-----------|----------|-------|
| STM32F407VGT6 board | 1 | Must have RMII Ethernet PHY (LAN8720A or DP83848) |
| RS485 transceiver module | 1 | MAX485, SP3485, or equivalent |
| 24V DC power supply | 1 | Or power via USB/ST-Link |
| Ethernet cable | 1 | Cat5e or better |

### 2.2 Wiring

#### RS485 Bus

```
STM32F407          MAX485/SP3485        RS485 Bus
---------          -------------        ---------
PA9  (USART1_TX) → DI                  →
PA10 (USART1_RX) ← RO                 ←
PB0              → DE + RE (tied)      →
GND              — GND                 —
                                      → A (Data+)
                                      → B (Data-)
```

**Important:** Connect DE and RE together to a single GPIO (PB0). When PB0 is HIGH, the transceiver is in transmit mode. When LOW, it's in receive mode.

#### Ethernet

Connect the STM32F407 board's RJ45 port to your network switch/router. The default static IP is `192.168.1.100`.

#### Power

Apply 3.3V to the MCU and RS485 transceiver. Most STM32F407 boards can be powered via USB or the ST-Link debugger connector.

### 2.3 RS485 Bus Termination

For reliable operation, install 120Ω termination resistors at **both ends** of the RS485 bus. If the bridge is at one end of the bus, install a 120Ω resistor between A and B at the bridge's RS485 connector.

### 2.4 Bias Resistors (Optional)

For noisy environments, install 560Ω–680Ω pull-up (A to VCC) and pull-down (B to GND) resistors to maintain a defined idle state on the bus.

---

## 3. Network Configuration

### 3.1 Default Network Settings

| Parameter | Default Value |
|-----------|--------------|
| IP Address | 192.168.1.100 |
| Subnet Mask | 255.255.255.0 |
| Gateway | 192.168.1.1 |
| TCP Port | 502 |

### 3.2 Changing Network Settings

Edit `Core/Src/main.c`, function `netif_config()`:

```c
IP4_ADDR(&ipaddr,  192, 168, 1, 100);  /* IP address */
IP4_ADDR(&netmask, 255, 255, 255, 0);  /* Subnet mask */
IP4_ADDR(&gw,      192, 168, 1, 1);    /* Gateway */
```

Rebuild and reflash after changing.

---

## 4. Serial Configuration

### 4.1 Default Serial Settings

| Parameter | Default | Location |
|-----------|---------|----------|
| Baud Rate | 19200 | `app_config.h` → `RS485_DEFAULT_BAUDRATE` |
| Data Bits | 8 | `app_config.h` → `RS485_DATA_BITS` |
| Parity | Even | `app_config.h` → `RS485_PARITY` |
| Stop Bits | 1 | `app_config.h` → `RS485_STOP_BITS` |

### 4.2 Changing Baud Rate

Edit `Core/Inc/app_config.h`:

```c
#define RS485_DEFAULT_BAUDRATE      19200  /* Change to 9600, 38400, 57600, etc. */
```

Then update the 3.5-character timeout:

| Baud Rate | 1 char time (11 bits) | 3.5-char timeout |
|-----------|----------------------|-------------------|
| 9600 | 1146 µs | 4010 µs |
| 19200 | 573 µs | **2005 µs** (default) |
| 38400 | 286 µs | 1003 µs |
| 57600 | 191 µs | 668 µs |
| 115200 | 95 µs | 334 µs |

Update `MODBUS_3_5_CHAR_TIMEOUT_US` in `app_config.h` accordingly.

### 4.3 Changing Slave Response Timeout

```c
#define MODBUS_RESPONSE_TIMEOUT_MS  1000  /* milliseconds */
```

Choose a value slightly longer than the slowest slave's worst-case response time.

---

## 5. Modbus Protocol Details

### 5.1 Address Mapping

The bridge uses **Unit ID** for routing:

```
TCP Client connects to 192.168.1.100:502
  → Sends Modbus TCP request with Unit ID = 5
    → Bridge translates to RTU frame addressed to slave 5
      → Slave 5 responds on RS485
        → Bridge translates back to TCP response
          → Response sent to the original TCP client
```

Unit IDs 1–247 are valid. Unit ID 0 is reserved for broadcast (not forwarded — broadcasts have no response). Unit IDs 248–255 are reserved per Modbus specification.

### 5.2 Frame Translation Details

#### TCP → RTU (Downstream)

```
TCP Frame:  [TransID(2)] [ProtID(2)] [Length(2)] [UnitID(1)] [PDU(n)]
MBAP Header: ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
                 ↓ stripped, cached
RTU Frame:          [UnitID(1)] [PDU(n)] [CRC16(2)]
```

The MBAP Transaction ID, Protocol ID, and Unit ID are **cached per transaction** and used to reconstruct the TCP response when the RTU slave replies.

The Length field is updated with the new PDU length in the response.

#### RTU → TCP (Upstream)

```
RTU Frame:  [UnitID(1)] [PDU(n)] [CRC16(2)]
                                    ^^^^^^^^
                 ↓ stripped after CRC validation
TCP Frame:  [Cached MBAP(7)] [PDU(n)]
```

The CRC16 is validated. If it fails, the frame is discarded and an exception is returned to the TCP client.

### 5.3 Exception Responses

The bridge generates the following exception responses:

| Scenario | Exception Code | Meaning |
|----------|---------------|---------|
| RS485 slave does not respond within timeout | **0x0B** | Gateway Target Device Failed to Respond |
| CRC-16 mismatch on received RTU frame | **0x0B** | Gateway Target Device Failed to Respond |
| Received RTU Unit ID doesn't match sent ID | **0x0B** | Gateway Target Device Failed to Respond |
| MBAP Protocol ID is not 0x0000 | **0x0A** | Gateway Path Unavailable |
| Frame too short (< 8 bytes TCP, < 4 bytes RTU) | **0x0B** | Gateway Target Device Failed to Respond |

All exception frames follow the standard Modbus TCP format:

```
TCP Exception: [MBAP(7)] [FuncCode|0x80(1)] [ExcCode(1)]
Length field: 3
```

### 5.4 Concurrency and Bus Arbitration

RS485 is a **single-master** bus. The bridge serializes all TCP requests through a FreeRTOS mutex:

1. TCP Client A sends request → queued for bridge engine
2. TCP Client B sends request → queued (waits in queue)
3. Bridge engine processes A's request on RS485, waits for response
4. Bridge engine processes B's request

If the request queue is full (8 pending requests), new requests are silently dropped. TCP clients should implement their own retry/timeout logic.

---

## 6. LED Indicators

| LED | Pin | Meaning |
|-----|-----|---------|
| DEBUG_LED (PD12) | Green | RS485 TX active (on during transmission) |
| DEBUG_LED2 (PD13) | Yellow | Bridge engine busy / socket server alive |

- Both LEDs blink rapidly on unrecoverable error (fault handler)
- DEBUG_LED2 toggles slowly if TCP server fails to listen on port 502

---

## 7. Troubleshooting

### 7.1 No Ethernet Link

1. Check PHY address: default is **0x01**. Some boards use 0x00. Edit `main.h` and `lwipopts.h`.
2. Verify RMII pin connections match the pin mapping in `main.h`.
3. Check that the Ethernet PHY oscillator/crystal is present (25 MHz or 50 MHz depending on PHY).
4. Verify the PHY reset pin is properly connected (if applicable on your board).

### 7.2 No RS485 Communication

1. Verify USART1 TX (PA9) and RX (PA10) are connected to the transceiver.
2. Verify DE pin (PB0) is connected to the transceiver's DE+RE pins.
3. Check baud rate and parity match the RS485 slaves.
4. Verify RS485 bus termination and bias.
5. Verify A/B polarity — swap if no communication.

### 7.3 Modbus Exceptions (0x0A / 0x0B)

- **0x0B (Gateway Target Device Failed)**: The RS485 slave didn't respond. Check:
  - Slave is powered and connected
  - Slave address (Unit ID) matches
  - Baud rate and parity match
  - Response timeout is long enough for the slave
- **0x0A (Gateway Path Unavailable)**: The TCP client sent a non-zero Protocol ID. This is a client software issue — the Protocol ID must be 0x0000.

### 7.4 CRC Errors

CRC errors indicate noise or wiring issues on the RS485 bus:
- Check termination resistors
- Check cable length (< 1200m for RS485)
- Verify ground connection between all RS485 devices
- Reduce baud rate if operating at high speeds over long distances

---

## 8. Building and Flashing

### 8.1 Development Build

```bash
# Fetch dependencies (first time only)
bash scripts/fetch_deps.sh

# Build
make

# Flash via ST-Link
make flash
```

### 8.2 Production Build

For production, modify the Makefile to use optimization:

```makefile
CFLAGS := $(MCU_FLAGS) $(C_DEFS) $(C_INCLUDES) \
          -O2 -DNDEBUG -g -Wall -Wextra \
          ...
```

### 8.3 Firmware Update (DFU)

The STM32F407 supports USB DFU (Device Firmware Upgrade) for field updates. Connect a USB cable, set the BOOT0 pin high, and use `dfu-util` or STM32CubeProgrammer:

```bash
dfu-util -a 0 -s 0x08000000 -D modbus_bridge.bin
```

---

## 9. Modbus TCP Client Configuration

### 9.1 Example: Python (pymodbus)

```python
from pymodbus.client import ModbusTcpClient

client = ModbusTcpClient("192.168.1.100", port=502)
client.connect()

# Read 10 holding registers from slave address 5
result = client.read_holding_registers(address=0, count=10, slave=5)
print(result.registers)

client.close()
```

### 9.2 Example: Node-RED

1. Add a **Modbus Read** node
2. Set Server = `192.168.1.100`, Port = `502`
3. Set Unit-Id = slave address (1–247)
4. Configure FC, address, and quantity as needed

### 9.3 Example: SCADA / HMI

Configure the Modbus TCP driver with:
- **IP Address**: 192.168.1.100
- **Port**: 502
- **Unit ID / Slave Address**: 1–247 (one per physical slave)

---

## 10. Performance Characteristics

| Metric | Typical Value |
|--------|--------------|
| TCP → RTU transaction time | ~5–10 ms + slave response time |
| Maximum TCP clients | 4 concurrent |
| Maximum RTU slaves | 247 (one at a time on the bus) |
| Bus guard time between transactions | 10 ms |
| Firmware size | ~58 KB flash, ~59 KB RAM |
| CPU utilization (idle) | < 1% |

---

## 11. Safety and Compliance

- **Industrial environments**: The bridge implements Modbus standard exception handling to recover gracefully from bus noise, disconnections, and protocol violations.
- **Watchdog**: Not yet implemented. Consider adding an independent watchdog (IWDG) for production deployments.
- **Ground isolation**: The RS485 transceiver (MAX485/SP3485) provides some isolation, but for harsh industrial environments, consider an isolated transceiver (e.g., ADM2483, ISO3082).

---

## 12. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-06 | Initial release |
