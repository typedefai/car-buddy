#!/usr/bin/env python3
"""Place the power board v2 components on the PCB, rebuild the outline.

Board: 100 x 86 mm, origin at lower-left. Coordinates are millimetres;
EasyEDA PCB units are mil (1 mm = 39.3701 mil).

Run after the schematic build + importChanges. Then:
    python3 check_placement.py   # pairwise bbox overlap report
    python3 route_pcb.py
"""
import json
import subprocess
import sys

MM = 39.3701

# board outline (mm), closed
BOARD_W, BOARD_H = 100, 86
OUTLINE_MIL = [
    int(0 * MM), int(0 * MM), 'L',
    int(BOARD_W * MM), int(0 * MM),
    int(BOARD_W * MM), int(BOARD_H * MM),
    int(0 * MM), int(BOARD_H * MM),
    int(0 * MM), int(0 * MM),
]

# ref: (x_mm, y_mm, rotation_deg)
PLACEMENT = {
    # zone A — input chain, top
    "J1": (7, 73, 0),        # XT30 battery in, left edge
    "F1": (24, 73, 0),       # ATO blade fuse holder
    "SW1": (47.5, 73.5, 0),  # main switch (23.4 mm square)
    "Q1": (66, 73, 0),       # AOD409 reverse protection
    "R10": (77, 76.5, 90),   # gate pull-down
    "D5": (77, 69.5, 0),     # gate zener
    "C18": (85, 70, 90),     # gate-source soft start
    # zone B — VBAT bus strip
    "D1": (10, 56, 0),      # SMBJ12A
    "C1": (24, 58.5, 0),    # 470 uF bulk
    "C12": (33, 59.5, 0),   # 100 nF
    "C21": (36.5, 59.5, 0), # 1 uF
    "RSH": (46, 59, 0),     # 0.01 R 3W shunt
    # zone C — 5V buck #1 (ESP32), left-middle
    "FB1": (8, 50, 0),
    "U1": (14, 48, 0),
    "C2": (8, 43, 0),
    "C3": (8, 36.5, 0),
    "C13": (20, 52, 90),
    "R11": (24, 52, 90),
    "C15": (20, 44, 90),
    "R13": (24, 44, 90),
    "L1": (14, 37, 0),
    "C4": (9.5, 30, 0),
    "C5": (15.5, 30, 0),
    "C6": (21.5, 30, 0),
    "R1": (24.5, 31.5, 90),
    "R2": (26, 31.5, 90),
    "C19": (30, 31.5, 90),
    "FB3": (36, 37, 0),
    "J3": (4.2, 24.5, 0),  # 5V1 out, left edge
    # zone D — 5V buck #2 (STM32), right-middle
    "FB2": (60, 50, 0),
    "U2": (66, 48, 0),
    "C7": (60, 43, 0),
    "C8": (60, 36.5, 0),
    "C14": (72, 52, 90),
    "R12": (76, 52, 90),
    "C16": (72, 44, 90),
    "R14": (76, 44, 90),
    "L2": (66, 37, 0),
    "C9": (60, 32, 0),
    "C10": (66, 32, 0),
    "C11": (72, 32, 0),
    "R3": (76, 31.5, 90),
    "R4": (78, 31.5, 90),
    "C20": (82, 31.5, 90),
    "FB4": (88, 37, 0),
    "J4": (4.2, 16.5, 0),  # 5V2 out, left edge
    # zone E — bottom: telemetry, motor switch/output, LEDs, TPs
    "U4": (30, 26, 0),      # INA219
    "U3": (38, 27, 0),      # ME6211
    "C22": (44, 27.5, 90),
    "C23": (47, 27.5, 90),
    "C24": (50, 27.5, 90),
    "J7": (14, 16, 0),      # I2C out, left edge
    "SW2": (70, 18, 0),     # motor switch
    "J5": (91, 16, 0),      # XT30 motor out, right edge
    "D2": (42, 8, 0),
    "R7": (46, 8, 90),
    "D3": (42, 12, 0),
    "R8": (46, 12, 90),
    "D4": (42, 16, 0),
    "R9": (46, 16, 90),
    "TP1": (24, 8, 0),
    "TP2": (28, 8, 0),
    "TP3": (32, 8, 0),
    "TP4": (36, 8, 0),
}

# mounting holes: (x_mm, y_mm), 3.2 mm NPTH
HOLES = [(3.5, 3.5), (96.5, 3.5), (3.5, 82.5), (96.5, 82.5)]

# enum globals are NOT exposed in the bridge runtime; use documented values:
# EPCB_LayerId: TOP=1, BOTTOM=2, BOARD_OUTLINE=11, MULTI=12
# EPCB_PrimitivePadShapeType.ELLIPSE='ELLIPSE'; EPCB_PrimitivePadHoleType.ROUND='ROUND'
JS = r"""
const MM = 39.3701;
const outline = __OUTLINE__;
const place = __PLACE__;
const holes = __HOLES__;
const log = [];
await eda.dmt_EditorControl.openDocument('4081b372105a948c');
await eda.dmt_EditorControl.waitForDocumentReady('4081b372105a948c', 20000);
await new Promise(r => setTimeout(r, 800));

// 1. remove the old board outline polylines (layer 11)
const polys = await eda.pcb_PrimitivePolyline.getAllPrimitiveId();
if (polys.length) {
  const ps = await eda.pcb_PrimitivePolyline.get(polys);
  const old = (ps || []).filter(p => p.layer === 11).map(p => p.primitiveId);
  if (old.length) await eda.pcb_PrimitivePolyline.delete(old);
  log.push('old outline removed: ' + old.length);
}
// 2. clear old routing and pours
await eda.pcb_Document.clearRouting('all');
const pours = await eda.pcb_PrimitivePour.getAllPrimitiveId();
if (pours.length) await eda.pcb_PrimitivePour.delete(pours);
log.push('routing cleared, pours: ' + pours.length);
// 3. new board outline (closed polyline on layer 11)
const poly = eda.pcb_MathPolygon.createPolygon(outline);
const ol = await eda.pcb_PrimitivePolyline.create('', 11, poly, 10, false);
log.push('outline: ' + !!ol);
// 4. place components
const ids = await eda.pcb_PrimitiveComponent.getAllPrimitiveId();
const comps = await eda.pcb_PrimitiveComponent.get(ids);
const byRef = {};
for (const c of comps) byRef[c.designator] = c;
const missing = [];
let n = 0;
for (const [ref, v] of Object.entries(place)) {
  const c = byRef[ref];
  if (!c) { missing.push(ref); continue; }
  await eda.pcb_PrimitiveComponent.modify(c.primitiveId, { x: v[0] * MM, y: v[1] * MM, rotation: v[2] || 0 });
  n++;
}
// 5. mounting holes (3.2 mm NPTH) - remove any previous H* pads first
const padIds = await eda.pcb_PrimitivePad.getAllPrimitiveId();
if (padIds.length) {
  const pads = await eda.pcb_PrimitivePad.get(padIds);
  const oldHoles = (pads || []).filter(p => (p.getState_PadNumber() || '').startsWith('H'))
    .map(p => p.primitiveId);
  if (oldHoles.length) await eda.pcb_PrimitivePad.delete(oldHoles);
  log.push('old holes removed: ' + oldHoles.length);
}
let holesMade = 0;
for (const [hx, hy] of holes) {
  const h = await eda.pcb_PrimitivePad.create(
    12, 'H' + holesMade,
    Math.round(hx * MM), Math.round(hy * MM), 0,
    ['ELLIPSE', 3.2 * MM, 3.2 * MM],
    '',
    ['ROUND', 3.2 * MM],
    0, 0, 0, false, 0, undefined, null, null, false);
  if (h) holesMade++;
}
await eda.pcb_Document.save();
return { log, placed: n, missing, holes: holesMade };
"""


if __name__ == "__main__":
    js = JS.replace("__OUTLINE__", json.dumps(OUTLINE_MIL)) \
           .replace("__PLACE__", json.dumps(PLACEMENT)) \
           .replace("__HOLES__", json.dumps(HOLES))
    res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=300)
    out = res.stdout
    try:
        d = json.loads(out)
    except Exception:
        print(out[:2000], res.stderr[:2000])
        sys.exit(1)
    print(json.dumps(d.get("result", d), indent=2))
