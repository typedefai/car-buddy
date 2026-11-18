# Power board

2S Li-ion input, direct VBAT output, 5 V/3 A, and 3.3 V/3 A rails for the car.

**Status:** schematic and PCB complete in EasyEDA Pro. 4 schematic pages, fully
routed 2-layer 70 x 50 mm PCB, DRC clean (0 violations). The editable project is
in [easyeda/car-buddy-power](easyeda/car-buddy-power/).

## Inputs and outputs

| Item | Value |
|---|---|
| Battery | 2S Li-ion, 2 x 3.7 V cells, 6.0–8.4 V |
| Recommended pack | 2S1P 18650 with integrated 2S BMS (10 A, with balancing) |
| VBAT direct output | 6.0–8.4 V, 10 A class, XT30 + screw terminal |
| 5 V rail | 5.0 V, 3 A peak / 2 A continuous |
| 3.3 V rail | 3.27 V, 3 A peak / 2 A continuous |
| VBAT sense | 0–2.1 V for 6.0–8.4 V, 3-pin header |
| Board size | 70 x 50 mm, 2-layer, 1.6 mm, 1 oz copper |

## Architecture

```text
J1 XT30 (2S pack)
  -> F1 5x20 fuse holder (7.5 A slow-blow)
  -> SW1 KCD1-101 power switch
  -> Q1 SI4435 reverse-polarity P-FET
  -> VBAT
       -> D1 SMBJ12A TVS
       -> C1 470 uF + C12 100 nF
       -> J5 XT30 (motors / TB6612 VM)
       -> J6 5.08 mm terminal (aux VBAT)
       -> U1 MP2315 5 V / 3 A -> J3 5 V
       -> U2 MP2315 3.3 V / 3 A -> J4 3.3 V
       -> R5/R6 divider -> J7 VBAT_SENSE
       -> D2/D3/D4 power LEDs
```

Detailed connections: [schematic.md](schematic.md) and
[netlist.csv](netlist.csv). The EasyEDA netlist was validated against
`netlist.csv` pin by pin (24 nets, 57 components).

## Why these choices

- **2S BMS is external, inside the pack.** It provides overcharge,
  over-discharge, over-current, short-circuit, and balancing protection.
  Building a BMS on this board would add a dedicated charger and safety
  certification work; the first prototype should use a protected pack.
- **Two buck converters from VBAT.** 5 V and 3.3 V are generated independently
  so the 3.3 V rail does not dissipate the 5 V-to-3.3 V drop in an LDO. Each
  rail uses an MP2315 (4.5–24 V in, 3 A, 500 kHz, internal compensation).
- **Direct VBAT output** is the motor rail. The TB6612FNG VM accepts 2.5–13.5 V;
  the 2S pack stays within that range. Motor noise is isolated from the buck
  inputs by the fuse, switch, TVS, and bulk capacitor.
- **No on-board charger.** Charge the protected pack with a proper 2S Li-ion
  charger. Do not charge through the board.

## Load budget

| Load | Rail | Typical | Peak |
|---|---|---|---|
| ESP32-S3 board | 5 V | 0.25 A | 0.50 A |
| Speaker amplifier (3 W) | 5 V | 0.20 A | 0.80 A |
| STM32 board | 5 V | 0.10 A | 0.20 A |
| Sensors/logic | 3.3 V | 0.20 A | 0.50 A |
| **5 V total** | | **~0.55 A** | **~1.5 A** |
| **3.3 V total** | | **~0.20 A** | **~0.5 A** |

The 3 A buck rating is a peak rating; keep each rail below **2 A continuous**
unless the thermal test in [test-plan.md](test-plan.md) passes with margin.

## Protection and safety

- **F1 5x20 fuse holder** with a 7.5 A slow-blow fuse protects the battery
  wiring and board traces.
- **SW1 KCD1-101** is the main power switch and carries the full load.
- **Q1 SI4435** blocks reverse battery polarity. Drain to the battery side,
  source to VBAT, gate pulled to GND through 100 k, 12 V gate-source zener.
- **D1 SMBJ12A** clamps motor/inductive transients on VBAT.
- **C1 470 uF** bulk capacitor absorbs motor current steps.
- **TB6612 STBY pulldowns** (on the STM32 board) keep the motors off at reset.
- **Hardware motor-power cut**: the battery switch/fuse is independent of
  firmware. Keep it accessible.
- Never connect unprotected cells. Never short VBAT to GND. Use a current-limited
  bench supply for first power-up.

## Connectors

| Ref | Connector | Pinout |
|---|---|---|
| J1 | XT30PW-M | 1 = VBAT_BMS+, 2 = GND |
| J3 | JST-XH 2-pin | 1 = 5 V, 2 = GND |
| J4 | JST-XH 2-pin | 1 = 3.3 V, 2 = GND |
| J5 | XT30PW-F | 1 = VBAT, 2 = GND, 3/4 = shield tabs to GND |
| J6 | 5.08 mm 2-pin terminal | 1 = VBAT, 2 = GND |
| J7 | JST-XH 3-pin | 1 = VBAT_SENSE, 2 = GND, 3 = 3.3 V |
| SW1 | KCD1-101 rocker | series with VBAT |
| F1 | 5x20 fuse holder | 7.5 A slow-blow |
| TP1–TP5 | test points | VBAT, 5 V, 3.3 V, GND, VBAT_SENSE |
| H1, H2 | 3.2 mm NPTH | M3 mounting holes, bottom corners |

## Files

- [schematic.md](schematic.md) — block diagram and detailed buck connections
- [BOM.csv](BOM.csv) — bill of materials with LCSC codes
- [netlist.csv](netlist.csv) — net-to-pin list (matches the EasyEDA netlist)
- [pcb-layout.md](pcb-layout.md) — stackup, placement, trace widths
- [test-plan.md](test-plan.md) — bring-up and load tests
- [easyeda/README.md](easyeda/README.md) — EasyEDA project, scripts and IDs

## Open items

- Confirm the actual 2S pack/BMS connector and cell holder.
- Confirm motor stall current; the fuse is 7.5 A.
- Widen the VBAT/5 V/3.3 V tracks to the values in `pcb-layout.md` before a
  high-current build (the autorouter used 0.3 mm for power nets).
- Rebuild the copper pours in the EasyEDA GUI before exporting Gerbers.
- Add a 5 V/3.3 V power-good signal to the STM32 if brownout diagnostics are
  needed.
