# Power board PCB layout

## Stackup and rules

| Item | Value |
|---|---|
| Layers | 2 |
| Thickness | 1.6 mm |
| Copper | 2 oz (70 um) |
| Finish | HASL or ENIG |
| Min trace/space | 0.20 mm / 0.20 mm |
| Min via | 0.30 mm drill / 0.60 mm pad |
| Board | 70 x 50 mm, 3 mm corner radius |
| Mounting | 4 x M3, 3.2 mm holes, 6 mm keepout |

## Placement

```text
+------------------------------------------------------+
| J1 XT30   F1   SW1   Q1        U1 buck    J3 5V      |
| battery                        L1         J4 3V3     |
|                                                      |
| D1/C1/C12     U2 buck          J5 XT30 (motors)      |
|               L2               J6 terminal (aux)     |
|                                                      |
| J7 sense      TP1..TP5         D2/D3/D4 LEDs         |
+------------------------------------------------------+
```

- **J1, F1, SW1, Q1** at the left/top edge, in the battery-current order.
- **U1/U2** in the middle, separated by at least 8 mm so their hot loops do
  not couple.
- **L1/L2** close to the ICs, inductors rotated so their fields do not point
  at the feedback dividers.
- **J5/J6** near the motor side, with short wide VBAT traces.
- **J3/J4** on the right edge, away from the switch node.
- **J7** and the sense divider away from the inductors and SW nodes.
- LEDs near the outputs they indicate.

## Power routing

| Net | Width / method |
|---|---|
| VBAT (battery to Q1 to J5/J6 to buck inputs) | 4 mm polygon or 4 mm trace; 2 oz |
| 5 V | 2 mm |
| 3.3 V | 2 mm |
| GND | bottom-layer ground plane + top stitching |
| VBAT_SENSE | 0.25 mm, away from inductors |
| FB nodes | 0.25 mm, short and away from SW |

- Use a **solid ground plane** on the bottom layer. Stitch top/bottom ground
  with vias around the bucks and connectors.
- Do not split the ground under the buck input/output loops. Keep the input
  capacitor ground and IC GND on the same copper.
- Add thermal relief to the fuse holder, switch, and connector pads.
- Use multiple vias for the Q1 drain/source and the buck GND pins.

## Buck layout checklist

For each MP2315:

1. Place CIN (10 uF x2) within 2–3 mm of IN/GND; loop area must be minimal.
2. Keep SW–L–COUT loop short and wide. The SW node is the noisy node.
3. Place the bootstrap cap and 20 ohm resistor directly across SW/BST.
4. Place the VCC 100 nF cap close to pin 7.
5. Tie AAM to VCC with a short trace.
6. Place the FB divider next to pin 8; route FB away from SW and the inductor.
7. Place COUT (22 uF x2 + 100 nF) close to the inductor output and connector.
8. Add copper area and vias at pin 4 (GND) for heat spreading.
9. Keep the EN pull-up close to pin 6; optionally add a jumper to GND.
10. Do not route the sense divider or feedback under the inductors.

## EMI and noise

- Keep the SW copper small; do not pour a large SW polygon.
- Put the inductors at least 5 mm from the sense divider and J7.
- Optional: 100 pF feedforward caps are already across the top FB resistors.
- Optional: add an RC snubber (2.2 ohm + 1 nF) from SW to GND if ringing is
  excessive; tune after measuring.
- Motor current flows on VBAT, not through the buck ground; keep the motor
  connector return directly to the battery input ground.

## Silkscreen

- Label J1 polarity, J3/J4 voltage, J5/J6 VBAT, J7 pinout.
- Mark SW1 ON/OFF, F1 rating, and “2S Li-ion only”.
- Add TP labels and a “motors off while charging” warning.
- Mark U1/U2 pin 1 and LED polarity.

## Assembly

- Use the LCSC codes in `BOM.csv` where listed; verify stock and package
  before ordering.
- U1/U2 and Q1 are the only fine-pitch parts; Q1 SO-8 and MP2315 TSOT-23-8.
- Keep the fuse holder and XT30 connectors on the top side for clearance.
- Confirm D1/D5 polarity before reflow.
