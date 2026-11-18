# Power board

2S Li-ion input, direct VBAT output, 5 V/3 A, and 3.3 V/3 A rails for the car.

**Status:** schematic/netlist/BOM/layout package v0.1. The EasyEDA project is
not created yet because the EasyEDA API Gateway extension is not connected;
see `easyeda/README.md`.

## Inputs and outputs

| Item | Value |
|---|---|
| Battery | 2S Li-ion, 2 x 3.7 V cells, 6.0–8.4 V |
| Recommended pack | 2S1P 18650 with integrated 2S BMS (10 A, with balancing) |
| VBAT direct output | 6.0–8.4 V, 10 A class, XT30 + screw terminal |
| 5 V rail | 5.0 V, 3 A peak / 2 A continuous |
| 3.3 V rail | 3.27 V, 3 A peak / 2 A continuous |
| VBAT sense | 0–2.1 V for 6.0–8.4 V, 3-pin header |
| Board size | 70 x 50 mm, 2-layer, 1.6 mm, 2 oz copper |

## Architecture

```text
J1 XT30 (2S pack)
  -> F1 7.5 A fuse
  -> SW1 power switch
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
[netlist.csv](netlist.csv).

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
  charger. Do not charge through the board unless the pack's BMS explicitly
  supports same-port charging and the motors are disabled.

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

- **F1 7.5 A fuse** protects the battery wiring and board traces.
- **SW1** is the main power switch; it carries the full load. Use a 6 A or
  higher switch.
- **Q1 SI4435DDY** blocks reverse battery polarity. Drain to the battery side,
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
| J5 | XT30PW-F | 1 = VBAT, 2 = GND |
| J6 | 5.08 mm 2-pin terminal | 1 = VBAT, 2 = GND |
| J7 | JST-XH 3-pin | 1 = VBAT_SENSE, 2 = GND, 3 = 3.3 V |
| SW1 | 6 A slide switch | series with VBAT |
| F1 | 5 x 20 mm fuse holder | 7.5 A slow-blow |
| TP1–TP5 | test points | VBAT, 5 V, 3.3 V, GND, VBAT_SENSE |

## Files

- [schematic.md](schematic.md) — block diagram and detailed buck connections
- [BOM.csv](BOM.csv) — bill of materials
- [netlist.csv](netlist.csv) — net-to-pin list
- [pcb-layout.md](pcb-layout.md) — stackup, placement, trace widths
- [test-plan.md](test-plan.md) — bring-up and load tests
- [easyeda/README.md](easyeda/README.md) — EasyEDA project creation status

## Open items

- Confirm the actual 2S pack/BMS connector and cell holder.
- Confirm motor stall current and choose F1 (7.5 A vs 10 A).
- Add a charge connector only if the chosen BMS supports same-port charging.
- Create the EasyEDA schematic and PCB once the API Gateway extension is
  connected; export EPRJ3 into this folder.
- Add a 5 V/3.3 V power-good signal to the STM32 if brownout diagnostics are
  needed.
