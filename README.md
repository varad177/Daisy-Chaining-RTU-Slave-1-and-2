# Daisy Chaining RTU — Slave 1 & Slave 2

**Part of the WMIND Edge Gateway System**

This repository contains PlatformIO firmware for two ESP32 devices configured as Modbus RTU slaves on a shared RS-485 bus. Both devices connect to a Raspberry Pi running the WMIND Edge Gateway, which reads sensor data and forwards it to the WMIND Cloud platform.

---
## Overview

| Device | Slave ID | Sensor | Modbus Registers | Data Type |
|--------|----------|--------|-----------------|-----------|
| ESP32 Slave 1 | `1` | Rotary Encoder (GPIO 18, 19) | `0` and `1` | 32-bit float |
| ESP32 Slave 2 | `2` | DS18B20 Temperature (GPIO 14) | `4` and `5` | 32-bit float (°C) |

Both slaves share a single RS-485 two-wire bus and a single RS-485 to USB adapter connected to the Raspberry Pi. No additional serial ports or USB adapters are required.

---

## Hardware Required

| Item | Component | Qty |
|------|-----------|-----|
| 1 | ESP32 Development Board | 2 |
| 2 | MAX485 RS-485 Serial Converter Module | 2 |
| 3 | Rotary Encoder Module | 1 |
| 4 | DS18B20 Digital Temperature Sensor | 1 |
| 5 | RS-485 to USB Serial Adapter | 1 (shared) |
| 6 | Jumper Wires (Male-to-Male, Male-to-Female) | 20–30 |
| 7 | Micro-USB Power Supply (5 V, min 1 A) | 2 |

> **Note:** Only one RS-485 to USB adapter is needed. It connects to the Raspberry Pi and serves both slaves on the shared bus.

---

## Wiring

### Slave 1 — Rotary Encoder Wiring

| Encoder Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| VCC (+) | 3V3 | 3.3 V power supply |
| GND | GND | Common ground |
| CLK (Clock) | GPIO 19 | Rotation clock signal |
| DT (Data) | GPIO 18 | Rotation direction signal |
| SW (Switch) | Not connected | Push-button — not used |

---

### Slave 2 — DS18B20 Temperature Sensor Wiring

| DS18B20 Pin | ESP32 Pin | Description |
|-------------|-----------|-------------|
| VCC (+) | **VIN (5 V)** | Power supply — connect to VIN, **not 3V3** |
| GND (–) | GND | Common ground |
| S (Signal / Data) | GPIO 14 | Digital temperature data |

> ⚠️ **Warning:** Connect DS18B20 VCC to **VIN (5 V)**, not to 3V3. The GPIO 14 signal pin operates at 3.3 V logic and is safe to connect directly. Only the power pin uses the 5 V rail.

---

### MAX485 to ESP32 — Both Slaves

The MAX485 wiring is identical for both slaves. Each ESP32 gets its own MAX485 module.

| MAX485 Pin | ESP32 Pin | Direction | Description |
|------------|-----------|-----------|-------------|
| DI (Data Input) | GPIO 17 (TX2) | ESP32 → MAX485 | ESP32 transmits data to RS-485 bus |
| RO (Receiver Output) | GPIO 16 (RX2) | MAX485 → ESP32 | ESP32 receives data from RS-485 bus |
| DE (Driver Enable) | GPIO 4 (D4) | ESP32 → MAX485 | Connect DE **and** RE together to GPIO 4 |
| RE (Receiver Enable) | GPIO 4 (D4) | ESP32 → MAX485 | Connect to same GPIO as DE |
| VCC | 3V3 | Power | 3.3 V power supply |
| GND | GND | Ground | Common ground |
| A (+) | → Shared bus A wire | Bus | Positive differential line |
| B (–) | → Shared bus B wire | Bus | Negative differential line |

> ⚠️ **Warning:** Both DE and RE **must** be connected to GPIO 4. If either pin is left floating, the MAX485 will behave unpredictably and corrupt all traffic on the shared bus, affecting both devices.

---


### Pin Out Diagram


---

### Shared RS-485 Bus

This is the daisy chain connection. All three A terminals are joined on the same wire, and all three B terminals are joined on the same wire.

| From | To | Notes |
|------|----|-------|
| MAX485 Slave 1 — A (+) | MAX485 Slave 2 — A (+) | Daisy chain link between the two slaves |
| MAX485 Slave 1 — A (+) *(same junction)* | RS-485 USB Adapter — A (+) | USB adapter joins the shared A wire |
| MAX485 Slave 1 — B (–) | MAX485 Slave 2 — B (–) | Daisy chain link between the two slaves |
| MAX485 Slave 1 — B (–) *(same junction)* | RS-485 USB Adapter — B (–) | USB adapter joins the shared B wire |

> ❌ **Caution:** A and B polarity must be consistent across all three nodes. Swapping A and B on any device inverts the differential signal for that device — no valid Modbus frames will be decoded. This is the most common RS-485 wiring error. If one device stops responding, swap its A and B wires first.

---

## Modbus Register Map

Both slaves use holding registers. Each sensor value is a 32-bit IEEE 754 floating-point number stored across two consecutive 16-bit registers in Big Endian word order.

| Register Address | Slave | Sensor | Data Type | Read Procedure |
|-----------------|-------|--------|-----------|----------------|
| 0 and 1 | Slave 1 | Rotary Encoder (voltage equivalent) | 32-bit float | Read both registers in one request and combine the two 16-bit words into one float |
| 4 and 5 | Slave 2 | DS18B20 Temperature (°C) | 32-bit float | Read both registers in one request and combine the two 16-bit words into one float |

**Float decoding (Big Endian word order):**

```
Register N     → High word (most significant 16 bits)
Register N + 1 → Low word  (least significant 16 bits)
```

---

## Firmware

### Slave 1 — Key Parameters

```cpp
#define SLAVE_ID       1          // Unique Modbus Slave ID
#define RS485_DE_PIN   4          // DE and RE both wired to GPIO 4
#define REG_VOLTAGE    0          // Holding register start address
#define ROT_CLK        18         // Encoder clock — GPIO 18
#define ROT_DT         19         // Encoder data  — GPIO 19

// Serial2: RX = GPIO 16 (RX2),  TX = GPIO 17 (TX2)
// Baud rate: 9600, 8N1
// Registers 0 and 1 — encoder value as 32-bit float, Big Endian word order
// Encoder range constrained to 15.0 – 30.0 (steps of 0.1 per click)
```

**What Slave 1 does:**
- Reads the rotary encoder on GPIO 18 and 19 using interrupts
- Each encoder click changes the stored value by ±0.1
- Value is clamped between 15.0 and 30.0
- Updates Modbus holding registers 0 and 1 every loop cycle
- Logs each incoming Modbus request to Serial (115200 baud) for debugging

---

### Slave 2 — Key Parameters

```cpp
const uint8_t  SLAVE_ID         = 2;   // Unique Modbus Slave ID
const uint8_t  PIN_RS485_DE     = 4;   // DE and RE both wired to GPIO 4
#define        TEMP_PIN           14   // DS18B20 data pin — GPIO 14
const uint16_t REG_TEMPERATURE   = 4;  // Holding register start address

// Serial2: RX = GPIO 16 (RX2),  TX = GPIO 17 (TX2)
// Baud rate: 9600, 8N1
// Registers 4 and 5 — temperature in °C as 32-bit float, Big Endian word order
// Temperature is read from DS18B20 once per second
// DS18B20 VCC must connect to VIN (5 V), not 3V3
```

**What Slave 2 does:**
- Reads temperature from the DS18B20 sensor on GPIO 14 every 1000 ms using the DallasTemperature library
- Ignores readings that return `DEVICE_DISCONNECTED_C` and holds the last valid value
- Updates Modbus holding registers 4 and 5 every loop cycle
- Logs each incoming Modbus request to Serial (115200 baud) for debugging

---

## Flashing with PlatformIO

> Perform all steps on a workstation (laptop or desktop), not on the Raspberry Pi.

### 1. Install VS Code and PlatformIO

1. Download and install [VS Code](https://code.visualstudio.com/)
2. Open the Extensions panel (`Ctrl+Shift+X`)
3. Search for **PlatformIO IDE** and click **Install**
4. Restart VS Code after installation

### 2. Clone this repository

```bash
git clone https://github.com/varad177/Daisy-Chaining-RTU-Slave-1-and-2.git
cd Daisy-Chaining-RTU-Slave-1-and-2
code .
```

### 3. Flash Slave 1

1. Open the **Slave 1** project folder in PlatformIO
2. Connect ESP32 Slave 1 to the workstation via Micro-USB
3. Click the **Upload** button (→ icon in the bottom toolbar) or press `Ctrl+Alt+U`
4. PlatformIO will download all required libraries, compile, and flash
5. Open the **Serial Monitor** and confirm encoder values are printing
6. **Disconnect Slave 1** before proceeding

### 4. Flash Slave 2

1. Open the **Slave 2** project folder in PlatformIO
2. Connect ESP32 Slave 2 to the workstation via Micro-USB
3. Click **Upload** or press `Ctrl+Alt+U`
4. Open the **Serial Monitor** and confirm temperature values are printing
5. **Disconnect Slave 2** before proceeding

> ⚠️ **Warning:** Never flash both ESP32 devices at the same time. Flash and verify each device individually before connecting them to the shared RS-485 bus.

### 5. Connect both devices to the bus

Once both devices are flashed and verified individually, connect the MAX485 A and B wires as described in the [Shared RS-485 Bus](#shared-rs-485-bus) section, then power both ESP32s on.

---

## WMIND Cloud Device Registration

Register each ESP32 as a **separate device entry** in the WMIND Cloud dashboard. Both share the same serial port but are distinguished by their Slave IDs.

### Slave 1

| Field | Value |
|-------|-------|
| Protocol | Modbus RTU |
| Serial Port | `/dev/ttyUSB0` |
| Baud Rate | `9600` |
| Slave / Unit ID | `1` |
| Encoder Registers | `0` and `1` |

### Slave 2

| Field | Value |
|-------|-------|
| Protocol | Modbus RTU |
| Serial Port | `/dev/ttyUSB0` |
| Baud Rate | `9600` |
| Slave / Unit ID | `2` |
| Temperature Registers | `4` and `5` |

> **Note:** Both devices use `/dev/ttyUSB0` — this is correct. The Edge Gateway distinguishes between them using the Slave ID, not the port.

---

## How Daisy Chaining Works

RS-485 is a differential bus — all devices share the same two wires (A and B). The Modbus master (Edge Gateway) addresses each slave by its unique Slave ID embedded in every request frame. Slaves that do not match the requested Slave ID ignore the frame completely and do not respond.

**Polling sequence for this setup:**

```
1. Edge Gateway → sends request to Slave ID 1
2. Both MAX485 modules receive the frame on the shared bus
3. ESP32 Slave 2 ignores it (ID mismatch)
4. ESP32 Slave 1 responds with encoder register values
5. Edge Gateway → sends request to Slave ID 2
6. ESP32 Slave 1 ignores it (ID mismatch)
7. ESP32 Slave 2 responds with temperature register values
```

Because RS-485 is **half-duplex**, only one device drives the bus at any time. The DE/RE pin (GPIO 4) on each MAX485 controls the direction: HIGH = transmit, LOW = receive. Both pins are held LOW by default (receive mode) and are raised HIGH only for the duration of a response transmission.

---

## Troubleshooting

| Symptom | Probable Cause | Fix |
|---------|---------------|-----|
| One device responds; the other does not | Duplicate Slave ID, or A/B wires swapped on the non-responding device | Verify unique Slave IDs in firmware and WMIND Cloud. Check A/B polarity on the non-responding MAX485. |
| Neither device responds after joining the bus | A/B wires crossed during daisy chain extension, or short between A and B | Disconnect Slave 2. If Slave 1 recovers, fault is in Slave 2 wiring. Verify A→A and B→B throughout the chain. |
| Slave 2 temperature reads -127 °C | DS18B20 GPIO 14 connection broken, or VCC not on VIN | Inspect GPIO 14 jumper. Confirm DS18B20 VCC is on VIN (5 V), not 3V3. |
| Intermittent data loss on both devices | DE/RE pin floating or incorrectly wired on one MAX485 | Confirm GPIO 4 is connected to both DE and RE on both MAX485 modules. |
| WMIND Cloud shows data for one slave only | Second device not registered, or wrong Slave ID in WMIND Cloud | Log in to WMIND Cloud and verify both device records exist with correct Slave IDs. |
| `/dev/ttyUSB0` not found | RS-485 to USB adapter not detected | Try a different Raspberry Pi USB port. Run `ls /dev/ttyUSB*` to confirm. |
| Serial Monitor shows garbage characters | Baud rate mismatch between Serial Monitor and firmware | Set Serial Monitor baud rate to `115200`. |

---

## Part of the WMIND Edge Gateway System

This repository covers the ESP32 firmware layer only. For the complete system — including Raspberry Pi setup, OPC UA Server, Edge Gateway configuration, and WMIND Cloud integration — refer to the full **WMIND Edge Gateway Build and Setup Guide (WMIND-EGW-001)**.
