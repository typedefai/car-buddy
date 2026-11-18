# Power board PCB layout

## Stackup and rules

| Item | Value |
|---|---|
| Layers | 2 |
| Thickness | 1.6 mm |
| Copper | 1 oz (35 um) as ordered; 2 oz recommended for >2 A continuous |
| Finish | HASL or ENIG |
| Min trace/space | 0.20 mm / 0.15 mm |
| Board | 70 x 50 mm, square corners |
| Mounting | 2 x M3 NPTH (3.2 mm) in the bottom corners |
| Outline | closed polyline on the Board Outline layer |

## Implemented placement

```text
+------------------------------------------------------+
| J1    F1 (5x20)   SW1 (rocker)          Q1    R10     |
| XT30                                                  |
| D1 C1 C12            C2 C3     C13 R11   D5 C18       |
|                                                       |
|        U2 3V3 buck        U1 5V buck      J3 5V       |
|        C7 C8 L2 ...       C4..C6 L1       J4 3V3      |
|                                           J7 sense    |
| D2 D3 D4        R5 R6 C17                             |
| LEDs            sense divider              J5 XT30    |
| TP1..TP5                        J6 terminal  (motor)  |
+------------------------------------------------------+
```

Coordinates are in millimetres from the lower-left board corner; the table is
mirrored in `easyeda/place_pcb.py`.

| Block | Parts | Position |
|---|---|---|
| Battery input | J1 XT30-M | (8, 44) |
| Fuse | F1 5x20 holder | (26, 44) |
| Switch | SW1 KCD1-101 | (50, 43) |
| Reverse protection | Q1 SI4435 | (65, 44) |
| VBAT clamp | D1, C1, C12 | (5, 38), (14, 35), (22, 38) |
| Q1 gate network | R10, D5, C18 | (60, 40), (60, 37), (66, 36) |
| 5 V buck | U1 + C2–C6, C13, C15, C19, R1, R2, R11, R13, L1 | centre-right |
| 3.3 V buck | U2 + C7–C11, C14, C16, C20, R3, R4, R12, R14, L2 | centre-left |
| Outputs | J3 5 V, J4 3.3 V, J7 sense | right edge at x = 64.5 |
| Motor / aux | J5 XT30-F, J6 KF2EDGK | bottom right / bottom centre |
| Sense divider | R5, R6, C17 | (48, 12), (52, 12), (56, 12) |
| Indicators | D2/D3/D4 + R7/R8/R9 | left edge |
| Test points | TP1–TP5 | bottom edge |

## Routing

- Nets are autorouted by the EasyEDA router on both layers; the board is fully
  routed with **0 DRC violations** and **0 remaining ratlines**.
- Signal tracks are 0.25 mm. Power nets (VBAT, VBAT_BMS, VBAT_FUSED, VBAT_SW,
  5 V, 3V3) are 0.3 mm except the eight segments where 0.3 mm would violate
  clearance; those stay at 0.25 mm.
- Ground is a solid pour on both layers (`GND_TOP`, `GND_BOTTOM`), 0.3 mm
  inset from the outline, priority 1.
- Rebuild the copper pours in EasyEDA before exporting Gerbers so the fill is
  up to date.

For a high-current build, widen the power trunks to the values below (they need
a manual re-route; the autorouter packs traces too tightly for 1 mm) or order
2 oz copper, which halves the resistance.

| Net | Target width (1 oz) | Target width (2 oz) |
|---|---|---|
| VBAT (battery to Q1 to J5/J6 to buck inputs) | 2.0 mm | 1.0 mm |
| 5 V | 1.0 mm | 0.5 mm |
| 3.3 V | 1.0 mm | 0.5 mm |
| VBAT_SENSE, FB nodes | 0.25 mm | 0.25 mm |

## Buck layout checklist (as implemented)

For each MP2315:

1. CIN (10 uF x2) is placed next to the IN/GND pins.
2. The SW–L–COUT loop is short; the SW node copper is small.
3. The bootstrap cap and 20 ohm resistor sit across SW/BST.
4. The VCC 100 nF cap is close to pin 7.
5. AAM is tied to VCC.
6. The FB divider sits next to pin 8, away from the inductor.
7. COUT (22 uF x2 + 100 nF) is close to the inductor output.
8. Pin 4 (GND) has copper and vias for heat spreading.
9. The EN pull-up is close to pin 6.
10. The sense divider and feedback do not run under the inductors.

## EMI and noise

- The SW copper is kept small; there is no large SW polygon.
- The inductors are at least 5 mm from the sense divider and J7.
- 100 pF feedforward caps are across the top FB resistors.
- Optional: add an RC snubber (2.2 ohm + 1 nF) from SW to GND if ringing is
  excessive; tune after measuring.

## Silkscreen

- J1 polarity, J3/J4 voltage, J5/J6 VBAT, J7 pinout.
- SW1 ON/OFF, F1 rating, and “2S Li-ion only”.
- TP labels and a “motors off while charging” warning.
- U1/U2 pin 1 and LED polarity are marked by the footprints.

## Assembly

- Use the LCSC codes in `BOM.csv`; verify stock and package before ordering.
- U1/U2, Q1, D5 and D1 are the only fine-pitch parts.
- Keep the fuse holder and XT30 connectors on the top side for clearance.
- Confirm D1/D5 polarity before reflow.
