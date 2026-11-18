# EasyEDA project — car-buddy-power

The editable EasyEDA Pro project, built through the `run-api-gateway` extension
and saved as a local EPRJ3 folder:

```text
easyeda/car-buddy-power/          <- copy this folder, or open it from EasyEDA
  car-buddy-power.eprj3           project file
  sch/Power/*.esch2               schematic pages
  pcb/PCB1.epcb2                  PCB
```

Open it in EasyEDA Pro with **File -> Open -> Open Local Project** and pick
`car-buddy-power.eprj3`.

## Status

| Document | Content |
|---|---|
| Schematic | 4 pages: `Input`, `5V Rail`, `3V3 Rail`, `Sense & Connectors` |
| Components | 57 (24 nets) |
| PCB | 70 x 50 mm, 2 layers, fully routed |
| DRC | 0 violations |
| Pours | GND on top and bottom (rebuild in the GUI before Gerber export) |
| Mounting | 2 x 3.2 mm NPTH, bottom corners |

## Schematic notes

- Each pin is attached to a short wire carrying the net name, so connectivity
  is defined by net names rather than long routed wires. This keeps the pages
  readable and makes the netlist exactly match `../netlist.csv`.
- Pages are A4 landscape with titled boxes per functional block.
- `U1`/`U2` pin 1 (AAM) is tied to pin 7 (VCC) for forced CCM.
- `J5` pins 3/4 are the XT30-F shield tabs, tied to GND.

## PCB notes

- Board outline: closed polyline on the Board Outline layer (11).
- Placement coordinates are in millimetres from the lower-left corner; see
  `../pcb-layout.md` and `place_pcb.py`.
- Signal tracks 0.25 mm; power nets 0.3 mm (8 segments stay at 0.25 mm where
  0.3 mm would violate clearance).
- GND pours on both layers, inset 0.3 mm, priority 1.

## Scripts

These scripts drive the live EasyEDA client through the local bridge
(`http://127.0.0.1:49620`). They are the reproducible source for the project;
run them from this folder with the EasyEDA window open and the
`run-api-gateway` extension enabled.

| Script | Purpose |
|---|---|
| `eda.py` | Minimal bridge client: sends JS to `/execute`, prints JSON |
| `build_schematic.py PAGE` | Builds one schematic page (`input`, `5v`, `3v3`, `io`) |
| `place_pcb.py` | Draws the board outline and places all 57 components |
| `route_pcb.py` | Clears tracks, autoroutes, saves, adds GND pours, runs DRC |
| `check_netlist.py` | Compares the EasyEDA netlist against `../netlist.csv` |
| `shot.py PAGE_UUID OUT.png` | Screenshot a schematic page |
| `shot_pcb.py OUT.png [x1 y1 x2 y2]` | Screenshot the PCB (region in mm) |

Typical rebuild:

```bash
python3 build_schematic.py input
python3 build_schematic.py 5v
python3 build_schematic.py 3v3
python3 build_schematic.py io
python3 check_netlist.py
python3 place_pcb.py
python3 route_pcb.py
```

After changes, copy the live project back and commit:

```bash
rsync -a --delete \
  ~/Documents/LCEDA-Pro/projects/car-buddy-power/ \
  hardware/power-board/easyeda/car-buddy-power/
```

## Live project IDs

| Item | UUID |
|---|---|
| Project | `f7155a0e0e356c3ee55e5cb5f2ac522bcb1b042d02e0207e74869905c46cecb9` |
| Board | `d41ddd579c7c652e` |
| Schematic | `d1450b3ab5fccb9c` |
| Page `Input` | `8da9c96e54f30398` |
| Page `5V Rail` | `f122051c8588a54e` |
| Page `3V3 Rail` | `b1a685136aabdee8` |
| Page `Sense & Connectors` | `53e158872de05a41` |
| PCB | `aa65e6d7e62b98c1` |

## API notes (learned while building this)

- `pcb_PrimitiveLine.create` only accepts copper layers; draw the board outline
  as a **closed polyline** with `pcb_MathPolygon.createPolygon([x, y, 'L', ...])`
  directly on layer 11. Line segments on layer 11 are rejected, and a line/poly
  built with the `'R'` polygon shorthand is rejected too.
- `PCB_Document.autoRouting()` must run against a board with a valid closed
  outline; with no outline, the job is refused or routes nothing.
- `autoRouting()` can take longer than a bridge HTTP request, which drops the
  result. Use `startAutoRouting()` and poll, then `save()`; otherwise the tracks
  exist only in the canvas and are lost on reload.
- `pcb_PrimitiveComponent.modify` resets unspecified properties, so pass
  `x`, `y` and `rotation` together.
- `createNetLabel()` in the schematic returns `undefined` even when it works;
  named wires are the reliable route.
- Schematic Y grows upward; `pcb_PrimitiveRectangle` height grows downward from
  `topLeftY`.
