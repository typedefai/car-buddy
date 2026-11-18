# Power board

2S Li-ion input, direct VBAT motor output, two independent 5 V rails, and
INA219 telemetry for the car.

**Status:** v2 schematic rebuilt and verified in EasyEDA Pro
(2026-11-19): one A4 sheet, 8 blocks, 64 components, 32 nets — netlist
matches `netlist.csv` (0 differences) and the cross-net wire-collision
check is clean. **The PCB is still the old v1 layout and needs a v2 redo.**
The editable project is in [easyeda/car-buddy-power](easyeda/car-buddy-power/).
Screenshot: [schematic-v2.png](schematic-v2.png).

## Inputs and outputs

| Item | Value |
|---|---|
| Battery | 2S Li-ion, 2 x 3.7 V 26650 cells (separate holders), 6.0-8.4 V |
| Protection | 2S BMS (bought 保护板) at the battery box + box fuse 7.5 A |
| VBAT motor output | 6.0-8.4 V, 10 A class, XT30, behind its own switch (SW2) |
| 5 V rail #1 | 5.0 V, 3 A, JST-XH -> ESP32 board (U1) |
| 5 V rail #2 | 5.0 V, 3 A, JST-XH -> STM32 carrier (U2) |
| Telemetry | INA219 (0.01 ohm shunt): VBAT + total current over I2C, 3-pin JST-XH |
| Board size | 10x10 cm budget; v2 PCB not laid out yet |

## Architecture

```text
J1 XT30 (2S pack)
  -> F1 ATO blade fuse (7.5 A slow-blow)
  -> SW1 KCD1-105 main switch
  -> Q1 AOD409 reverse-polarity P-FET (+ RC soft start)
  -> VBAT_RAIL
       -> RSH 0.01 ohm -> VBAT_LOAD
            -> D1 SMBJ12A TVS + C1 470 uF + 100 nF + 1 uF
            -> SW2 motor switch -> J5 XT30 (motors)
            -> FB1 -> U1 MP2315 5 V #1 -> FB3 -> J3 (ESP32 board)
            -> FB2 -> U2 MP2315 5 V #2 -> FB4 -> J4 (STM32 carrier)
            -> U4 INA219 (IN+ = battery side) -> J7 I2C
            -> U1 5V1 -> U3 ME6211 3.3 V -> powers U4
       -> D2/D3/D4 power LEDs
```

Detailed connections: [schematic.md](schematic.md) and
[netlist.csv](netlist.csv).

## Why these choices

- **Two independent 5 V bucks.** The ESP32 board's speaker amp is a
  switching noise source; a dedicated buck per board keeps it off the
  STM32 logic supply and gives each board its own fault domain.
- **INA219 replaces the analog VBAT divider.** One part gives pack voltage
  (for the STM32 PWM clamp), total current (stall detection), and battery
  telemetry for the server UI.
- **AOD409 instead of SI4435.** ~8 mOhm vs ~20 mOhm keeps the reverse
  polarity FET cool at 10 A stall current (1 W vs 2 W).
- **Two switches.** The motor rail cuts independently so bench work keeps
  logic alive with wheels physically dead.
- **No 3.3 V distribution.** Every consumer board regulates its own 3.3 V
  (天空星 LDO, STM32 carrier LDO, ESP32 board buck); the power board's own
  ME6211 only feeds the INA219.
- **2S BMS is external, bought.** Over-discharge/over-current/short
  protection lives at the battery box, independent of firmware.

## Safety

- Fuse at the battery box (before the cable run) + board fuse: double
  protection for the high-capacity cells.
- Battery cells are charged individually in an external slot charger; no
  on-board charging, no balance lead in the car.
- Hardware motor-power cut: SW2 + the battery fuse are independent of
  firmware.
- Never connect unprotected Li-ion cells. Never short VBAT to GND.

## Connectors

| Ref | Connector | Pinout |
|---|---|---|
| J1 | XT30PW-M | 1 = VBAT_BMS, 2 = GND |
| J3 | JST-XH 2-pin | 1 = 5V1, 2 = GND (ESP32 board) |
| J4 | JST-XH 2-pin | 1 = 5V2, 2 = GND (STM32 carrier) |
| J5 | XT30PW-F | 1 = VBAT_MOTOR, 2/3/4 = GND (shield tabs) |
| J7 | JST-XH 3-pin | 1 = SDA, 2 = SCL, 3 = GND (pull-ups on carrier) |
| SW1 | KCD1-105 rocker | main power, series with VBAT |
| SW2 | KCD1-105 rocker | motor rail cut |
| F1 | ATO blade holder | 7.5 A slow-blow |
| TP1-TP4 | test points | VBAT, 5V1, 5V2, GND |

## Files

- [schematic.md](schematic.md) — v2 block diagram and detailed connections
- [schematic-v2.png](schematic-v2.png) — screenshot of the v2 sheet
- [BOM.csv](BOM.csv) — bill of materials with LCSC codes
- [netlist.csv](netlist.csv) — net-to-pin list (matches the EasyEDA netlist)
- [easyeda/README.md](easyeda/README.md) — EasyEDA project, scripts and IDs

## Open items

- **PCB redo** for v2: new outline/placement/routing per D10 (VBAT >= 3 mm
  + pours), then DRC and the test plan.
- Measure stall current on the bench to confirm the 7.5 A fuse and the
  AT8236 VREF limit (see DECISIONS.md open questions).
- Update `pcb-layout.md` and `test-plan.md` for the v2 board.
