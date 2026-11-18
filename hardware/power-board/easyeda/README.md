# EasyEDA project — car-buddy-power

The editable EasyEDA Pro project, built through the `run-api-gateway` extension
and saved as a local EPRJ3 folder:

```text
easyeda/car-buddy-power/          <- copy this folder, or open it from EasyEDA
  car-buddy-power-v2.eprj3        project file
  sch/Schematic1/电源原理图.esch2  single-sheet schematic
  pcb/Power.epcb2                 PCB
```

Open it in EasyEDA Pro with **File -> Open -> Open Local Project** and pick
`car-buddy-power-v2.eprj3`.

## Status

| Document | Content |
|---|---|
| Schematic | one A4 sheet, 6 titled blocks, all 57 components |
| Nets | 24, validated against `../netlist.csv` (0 differences) |
| PCB | 70 x 50 mm, 2 layers, fully routed |
| DRC | 0 violations, 0 ratlines |
| Pours | GND on top and bottom (rebuild in the GUI before Gerber export) |
| Mounting | 2 x 3.2 mm NPTH, bottom corners |

## Schematic notes

- Everything is on one page: battery input, reverse protection, VBAT clamp,
  the 3.3 V buck, the 5 V buck, and the connectors/indicators/sense block.
- Each pin is attached to a short wire carrying the net name, so connectivity
  is defined by net names rather than long routed wires. The netlist therefore
  matches `../netlist.csv` exactly.
- `U1`/`U2` pin 1 (AAM) is tied to pin 7 (VCC).
- `J5` pins 3/4 are the XT30-F shield tabs, tied to GND.
- Net names: `VBAT_RAIL`, `VCC3V3` and `VCC5V` are used for the protected
  battery rail and the two buck VCC nodes (see `../README.md`).

## PCB notes

- Board outline: closed polyline on the Board Outline layer (11).
- Placement coordinates are in millimetres from the lower-left corner; see
  `../pcb-layout.md` and `place_pcb.py`.
- Signal tracks 0.25 mm; power nets 0.3 mm where clearance allows.
- GND pours on both layers, inset 0.3 mm, priority 1.

## Scripts

These scripts drive the live EasyEDA client through the local bridge
(`http://127.0.0.1:49620`). They are the reproducible source for the project;
run them from this folder with the EasyEDA window open and the
`run-api-gateway` extension enabled.

| Script | Purpose |
|---|---|
| `eda.py` | Minimal bridge client: sends JS to `/execute`, prints JSON |
| `build_schematic.py clearwires` | Deletes all schematic wires |
| `build_schematic.py place A B` | Places parts `A..B` |
| `build_schematic.py wire A B` | Wires pins/buses for parts `A..B` |
| `build_schematic.py decor` | Redraws the block boxes and title |
| `build_schematic.py save` | Saves the schematic |
| `place_pcb.py` | Places all 57 components on the PCB |
| `route_pcb.py` | Clears tracks, autoroutes, saves, adds GND pours, runs DRC |
| `check_netlist.py` | Compares the EasyEDA netlist against `../netlist.csv` |
| `shot.py PAGE_UUID OUT.png` | Screenshot a schematic page |
| `shot_pcb.py OUT.png` | Screenshot the PCB |

Typical rebuild (the bridge caps a request at ~30 s, hence the chunks):

```bash
python3 build_schematic.py clearwires
for i in 0 10 20 30 40 50; do python3 build_schematic.py place $i $((i+10)); done
for i in 0 8 16 24 32 40 48 56; do python3 build_schematic.py wire $i $((i+8)); done
python3 build_schematic.py decor
python3 build_schematic.py save
python3 check_netlist.py
python3 place_pcb.py
python3 route_pcb.py
```

After changes, copy the live project back and commit:

```bash
rsync -a --delete \
  ~/Documents/LCEDA-Pro/projects/car-buddy-power-v2/ \
  hardware/power-board/easyeda/car-buddy-power/
```

## Live project IDs

| Item | UUID |
|---|---|
| Project | `0f0cf3ed0dd37301365499c8cc0c96ae99b2924e980a27e583f064b23b6f1369` |
| Board | `6f6c8462bc68715a` |
| Schematic | `c105c79ca125469e` |
| Page `电源原理图` | `1c5eea75209bcc4d` |
| PCB `Power` | `4081b372105a948c` |

## API notes (learned while building this)

- Draw the board outline as a **closed polyline** of integer mil coordinates:
  `pcb_MathPolygon.createPolygon([x, y, 'L', ...])` on layer 11. Line
  primitives on layer 11 are rejected, and non-integer coordinates can be
  rejected too.
- `autoRouting()` fails or routes nothing without a valid closed outline; it can
  also exceed the bridge request timeout. Use `startAutoRouting()` + polling,
  then `save()`, otherwise the tracks live only in the canvas.
- `PCB_Document.importChanges(uuid, true)` needs the PCB tab active.
- Schematic nets are merged when a wire endpoint lands within ~5 units of a
  wire of another net. Keep at least 150 units between two-pin parts on the same
  row when using 70/35-unit stubs. This bit us repeatedly: check with a
  `pins vs wire endpoints` sweep after wiring.
- EasyEDA keeps a global net-name table. Some names (`VBAT`, `VCC3`, `VCC5`,
  and later `VBAT_MAIN`, `VCC_3V3`, `VCC_5V`) became bound to GND in this
  client after a bad import and stay broken across projects. Use fresh names
  (`VBAT_RAIL`, `VCC3V3`, `VCC5V`) if a named wire resolves to GND.
- `pcb_PrimitiveComponent.modify` resets unspecified properties: pass `x`, `y`
  and `rotation` together.
- Schematic Y grows upward; `pcb_PrimitiveRectangle` height grows downward from
  `topLeftY`.
