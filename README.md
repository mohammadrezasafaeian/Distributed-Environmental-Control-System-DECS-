> **[🇮🇷 برای مشاهده مستندات به زبان فارسی اینجا کلیک کنید](README_FA.md)**


# Distributed Control and Automation System
*Distributed automation platform: STM32 Brain controlling scalable AVR field nodes via I2C, with ESP32 Web Dashboard.*

## Project Overview
A distributed embedded system designed for scalable environmental automation. While demonstrated here as a **Smart Greenhouse**, the underlying master-slave architecture is designed to be application-agnostic, capable of being repurposed for scenarios like **server room monitoring or cold storage logistics** with minimal firmware changes.

- **Multi-Zone Control**: Independent management of sensors (temp, humidity, light) and actuators (pumps, fans, ligh, humidifier) across multiple nodes.
- **Scalable Bus**: Single I2C bus supports theoretically up to 112 independent zones using just 2 wires.
- **Resilient Design**: Features Hysteresis control, I2C bus recovery sequences, and Watchdog protection.
- **Dual HMI**: Local (OLED+Keypad) for field maintenance, Web Dashboard (ESP32) for remote monitoring.
---
## Project History & Evolution
This project started in 2020 as a simple, monolithic AVR-based controller.
> [👉 **View the original 2020 legacy code here**](Legacy_2020_v0/) to see the architectural evolution from a single-chip beginner project to today's distributed, professional-grade system.

---
## System Architecture
```
┌─────────────────────────────────────────────────────────┐
│                    STM32F4 Master                       │
│  - Profile database & control logic                     │
│  - I2C master (polls zone controllers)                  │
│  - OLED/Keypad UI                                       │
│  - UART → ESP32                                         │
└───────────┬──────────────┬──────────────┬───────────────┘
            │ I2C          │ I2C          │ UART
    ┌───────▼──────┐  ┌────▼──────┐  ┌───▼──────────┐
    │  ATmega32    │  │ ATmega32  │  │   ESP32      │
    │  Zone 1      │  │  Zone 2   │  │  WiFi AP     │
    │ (0x08)       │  │  (0x07)   │  │  Dashboard   │
    │              │  │           │  │  Port 80     │
    │ 3× Sensors   │  │ 3× Sensors│  └──────────────┘
    │ 4× Actuators │  │ 4× Actuators│
    └──────────────┘  └────────────┘
```
### Key Design Decisions
1. **2-Wire Scalability (I2C)**: By using AVRs as addressable I2C nodes, the entire facility requires only **two shared data wires** to control 100+ separate zones (theoretically) via a single master chip. This drastically reduces total cost and cabling complexity compared to star-topology (individual wiring) systems.
2. **Voltage/Noise Isolation**: Field nodes handle 5V relay switching and sensor noise locally, protecting the sensitive 3.3V STM32 master from industrial environment interference.
3. **Centralized Logic**: All profile data and decision algorithms reside solely on the Master; therefore, field nodes require no configuration upon replacement.

---
## Communication Protocols
| Bus     | Direction         | Format                          | Rate   |
|---------|-------------------|---------------------------------|--------|
| I2C     | STM32 → ATmega32  | Commands (0x10-0x17)            | 500ms  |
| I2C     | ATmega32 → STM32  | 6 bytes (3× 16-bit ADC)         | 500ms  |
| UART    | STM32 → ESP32     | JSON status                     | 1000ms |
| SPI     | STM32 → SSD1306   | Display updates                 | 250ms  |
---
## Plant Profile Database
Currently supports 7 profiles (easily extensible in `plant_profiles.c`):
| Profile    | Humidity | Temp | Light | Interval | Duration |
|------------|----------|------|-------|----------|----------|
| Tomato     | 450      | 280  | 650   | 30s      | 5s       |
| Lettuce    | 500      | 200  | 400   | 20s      | 3s       |
| Cucumber   | 490      | 270  | 620   | 35s      | 6s       |
| *(+4 more)*|          |      |       |          |          |
**Adding profiles**: Just append to the `profiles[]` array!
---
## Features
### STM32 Master (STM32F411)
- ✅ Menu system with scrolling (supports 4+ profiles per screen)
- ✅ Manual override mode (direct actuator control)
- ✅ Automatic control with hysteresis
- ✅ I2C bus recovery (handles stuck slaves)
- ✅ Memory corruption detection (stack canary)
### Zone Controller (ATmega32 @ 8MHz)
- ✅ 3× 10-bit ADC readings (humidity/temp/light)
- ✅ 4× GPIO actuators (pump/humidifier/fan/light)
- ✅ I2C slave mode with command processing
- ✅ Oversampling (100 samples per reading)
### ESP32 Web Dashboard
- ✅ Real-time sensor graphs (Chart.js)
- ✅ Live actuator status indicators
- ✅ 24-hour irrigation heatmap
- ✅ Event timeline log
- ✅ Responsive design
- ✅ Access Point mode (192.168.4.1)
### Scalable architechture
The system uses a **shared I2C bus** to control unlimited control zones with just 
**2 wires**:
```
STM32F411                                                    
  PB6 (SCL) ●━━━━━━━━━━━━━━●━━━━━━━━━━━━━━━━━━●━━━━━━━━●
  PB7 (SDA) ●━━━━━━━━━━━━━━┃●━━━━━━━━━━━━━━━━━┃●━━━━━━━●
                           ┃┃                 ┃┃       ┃
                           ┃┃                 ┃┃       ┃
            ┌────────────┐ ┃┃  ┌────────────┐ ┃┃ ┌─────▼──────┐
            │ ATmega32   │ ┃┃  │ ATmega32   │ ┃┃ │  SSD1306   │
            │ I2C: 0x08  │ ┃┃  │ I2C: 0x07  │ ┃┃ │ I2C: 0x3C  │
            │            │ ┃┃  │            │ ┃┃ │   (OLED)   │
            │ PC0 ─────────●┃  │ PC0 ─────────●┃ └────────────┘
            │ PC1 ──────────●  │ PC1 ──────────●
            │            │     │            │
            │ PA0: Humid │     │ PA0: Humid │
            │ PA1: Temp  │     │ PA1: Temp  │
            │ PA2: Light │     │ PA2: Light │
            │            │     │            │
            │ PD2: Pump  │     │ PD2: Pump  │
            │ PD3: Fan   │     │ PD3: Fan   │
            │ PD4: Light │     │ PD4: Light │
            │ PD5: Humid │     │ PD5: Humid │
            └────────────┘     └────────────┘
                  │                  │
                  ↓                  ↓
            Environment 1      Environment 2
            
```
### Why This Matters:
- **Scalability**: Add zones by connecting more ATmega32s to the same 2 wires
- **Cost-effective**: No additional wiring per zone (vs. parallel GPIO)
- **Flexible addressing**: 7-bit I2C = up to 112 devices theoretically
- **Centralized control**: STM32 polls all zones, applies profiles, logs to ESP32
### Current Demo Setup:
- 2 zone controllers (easily expandable to 8-16 in practice)
- 7 plant profiles (add more in `plant_profiles.c`)
- Menu system auto-adjusts to profile count (scrolling UI)
---
## Dual Scalability
| Dimension | Current | Max (Practical) | How to Scale |
|-----------|---------|-----------------|--------------|
| **Zones** | 2 | 8-16* | Flash ATmega32 with new address, connect to I2C bus |
| **Profiles** | 7 | 100+ | Add entry to `profiles[]` array |
*Limited by bus capacitance and polling rate (500ms cycle)
---
## Real-World Expansion Example
**Scenario**: Greenhouse with 10 plant types
1. **Hardware**: Add 8 more ATmega32s to I2C bus (addresses 0x06-0x0F)
2. **Firmware**: Update `node_controller.c` with 8 new `#define`s
3. **Profiles**: Add varius profiles in `plant_profiles.c`
5. **Result**: 10-zone greenhouse with centralized control
**Cost per zone**: ~$3 USD (ATmega32 + sensors + relays)
---
## Hardware Requirements
| Component        | Quantity | Notes                          |
|------------------|----------|--------------------------------|
| STM32F411        | 1        | Master controller              |
| ATmega32         | 2+       | Zone controllers (scalable)    |
| ESP32            | 1        | Web server                     |
| SSD1306 OLED     | 1        | 128×64 I2C display             |
| 4×4 Matrix Keypad| 1        | TTP229 chip (SDO/SCL)         |
| Potentiometers   | 6        | Simulate sensors (3 per zone)  |
| LEDs             | 8        | Simulate actuators (4 per zone)|
### Pin Connections
**STM32F411:**
- `PB6/PB7`: I2C1 (to ATmegas & OLED)
- `PA2/PA3`: USART2 (to ESP32)
- `PB8/PB9`: Keypad (SCL/SDO)
**ATmega32:**
- `PC0/PC1`: I2C (TWI)
- `PA0/PA1/PA2`: ADC inputs
- `PD2/PD3/PD4/PD5`: Actuator outputs
*(Add actual schematic in `/docs`)*
---

## Demo Video


---
## Future Improvements
- [ ] Add EEPROM storage for profile assignments
- [ ] Implement ESP32 ↔ STM32 bidirectional control
- [ ] Add soil moisture sensor support (I2C or analog)
- [ ] Port to FreeRTOS for better task management
- [ ] PCB design (current version is breadboard)
---
## License
MIT License - feel free to use for learning/portfolio purposes
---
## Contact

**Mohammad Reza Safaeian**  
📧 mohammad.rsafaeian@gmail.com  
---
### File Structure
```
See detailed structure above...
```
