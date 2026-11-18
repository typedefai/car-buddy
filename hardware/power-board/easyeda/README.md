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
| Schematic | one A4 sheet, 8 titled blocks, 64 components, 32 nets — **v2 rebuilt 2026-11-19** |
| Nets | validated against `../netlist.csv` (0 differences) + cross-net wire-collision check (0 touches) |
| PCB | **stale** — still the old 70 x 50 mm v1 layout (3.3 V buck, SI4435, 5x20 fuse); needs a v2 redo |

## Schematic notes (v2)

- Eight blocks: battery input, reverse protection + soft start, VBAT clamp,
  motor switch/output, two independent 5 V bucks, INA219 + LDO, indicators.
- Connectivity is defined by net names (short stub wires carry the net
  name); there are no long routed wires. The netlist therefore matches
  `../netlist.csv` exactly.
- **Wire-collision trap (learned the hard way):** EasyEDA merges two nets
  when a new wire endpoint lands within ~5 units of another net's wire.
  Stub wires are 35 units long (30 for left-facing pins), so adjacent
  two-pin parts on the same row need >= ~45 units of clearance between
  facing stubs of different nets. `check_stubs.py` (see Scripts) verifies
  this after every rebuild.
- `U1`/`U2` pin 1 (AAM) is tied to pin 7 (VCC).
- `J5` pins 3/4 are the XT30-F shield tabs, tied to GND.
- `SW1`/`SW2` pin 3 (lamp) is left unconnected — the KCD1-105 lamp is
  AC-only.
- `RSH` (0.01 ohm) sits between `VBAT_RAIL` (battery side) and `VBAT_LOAD`
  (everything else). The INA219 measures IN+ = battery side, IN- = load
  side, so positive current = discharge.
- Net names: `VBAT_RAIL`, `VBAT_LOAD`, `VBAT_MOTOR`, `VCC5V1`, `VCC5V2`,
  `5V1`, `5V2` etc. Never reuse names like `VBAT`, `VCC3`, `VCC5`,
  `VBAT_MAIN`, `VCC_3V3`, `VCC_5V` — they are bound to GND in this client's
  global name table.

## PCB notes

- Board outline: closed polyline on the Board Outline layer (11).
- Placement coordinates are in millimetres from the lower-left corner.
- Signal tracks 0.25 mm; power nets 0.3 mm where clearance allows — the v2
  PCB must use >= 3 mm VBAT traces + pours (D10).
- GND pours on both layers, inset 0.3 mm, priority 1.

## Scripts

These scripts drive the live EasyEDA client through the local bridge
(`http://127.0.0.1:49620`). They are the reproducible source for the project;
run them from this folder with the EasyEDA window open and the
`run-api-gateway` extension enabled.

| Script | Purpose |
|---|---|
| `eda.py` | Minimal bridge client: sends JS to `/execute`, prints JSON |
| `build_schematic.py clear` | Deletes everything on the page (full rebuild start) |
| `build_schematic.py clearwires` | Deletes all schematic wires |
| `build_schematic.py place A B` | Places parts `A..B` |
| `build_schematic.py wire A B` | Wires pins for parts `A..B` |
| `build_schematic.py decor` | Redraws the block boxes and title |
| `build_schematic.py save` | Saves the schematic |
| `check_stubs.py` | Cross-net wire-collision check (0 touches required) |
| `check_netlist.py` | Compares the EasyEDA netlist against `../netlist.csv` |
| `shot.py PAGE_UUID OUT.png` | Screenshot a schematic page |
| `place_pcb.py` / `route_pcb.py` | **v1 PCB scripts — need a v2 rewrite** |

Typical rebuild (the bridge caps a request at ~30 s, hence the chunks):

```bash
python3 build_schematic.py clear
for i in 0 12 24 36 48 60; do python3 build_schematic.py place $i $((i+12)); done
for i in 0 8 16 24 32 40 48 56; do python3 build_schematic.py wire $i $((i+8)); done
python3 build_schematic.py decor
python3 build_schematic.py save
python3 check_stubs.py
python3 check_netlist.py
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
- **Net merging:** see Schematic notes above. After a rebuild always run
  `check_stubs.py` (0 touches) and `check_netlist.py` (0 problems).
- EasyEDA keeps a global net-name table. Some names (`VBAT`, `VCC3`, `VCC5`,
  and later `VBAT_MAIN`, `VCC_3V3`, `VCC_5V`) became bound to GND in this
  client after a bad import and stay broken across projects. Use fresh names
  (`VBAT_RAIL`, `VBAT_LOAD`, `VCC5V1`, ...) if a named wire resolves to GND.
- `pcb_PrimitiveComponent.modify` resets unspecified properties: pass `x`, `y`
  and `rotation` together.
- Schematic Y grows upward; `pcb_PrimitiveRectangle` height grows downward from
  `topLeftY`.
