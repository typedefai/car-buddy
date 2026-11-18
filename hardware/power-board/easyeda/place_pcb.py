#!/usr/bin/env python3
"""Place the power board components on the PCB and draw the board outline.

Coordinates are millimetres with the board origin at the lower-left corner;
EasyEDA PCB units are mil (1 mm = 39.3701 mil).
"""
import json
import subprocess
import sys

MM = 39.3701

OUTLINE = [(0, 0), (70, 0), (70, 50), (0, 50)]

# ref: (x_mm, y_mm, rotation_deg)
PLACEMENT = {
    # input protection, top edge
    "J1": (8, 44, 0),
    "F1": (26, 44, 0),
    "SW1": (50, 43, 0),
    "Q1": (65, 44, 0),
    "D1": (5, 38, 0),
    "C1": (14, 35, 0),
    "C12": (22, 38, 0),
    "R10": (60, 40, 90),
    "D5": (60, 37, 0),
    "C18": (66, 36, 90),
    # 5 V buck (U1), centre right
    "U1": (45, 31, 0),
    "C2": (35, 36, 0),
    "C3": (35, 34, 0),
    "C13": (51, 35, 90),
    "R11": (55, 35, 90),
    "C15": (51, 29, 90),
    "R13": (55, 29, 90),
    "L1": (45, 26, 0),
    "C4": (38, 19, 90),
    "C5": (41, 19, 90),
    "C6": (44, 19, 90),
    "R1": (50, 19, 90),
    "R2": (53, 19, 90),
    "C19": (56, 19, 90),
    # 3.3 V buck (U2), centre left
    "U2": (22, 31, 0),
    "C7": (6, 32, 0),
    "C8": (6, 29.5, 0),
    "C14": (28, 34, 90),
    "R12": (32, 34, 90),
    "C16": (28, 27, 90),
    "R14": (32, 27, 90),
    "L2": (22, 25, 0),
    "C9": (8, 20, 90),
    "C10": (11, 20, 90),
    "C11": (14, 20, 90),
    "R3": (20, 18, 90),
    "R4": (23, 18, 90),
    "C20": (26, 18, 90),
    # connectors
    "J5": (57, 6, 0),
    "J6": (42, 8, 0),
    "J3": (64.5, 30, 180),
    "J4": (64.5, 24, 180),
    "J7": (64.5, 17, 180),
    # sense divider
    "R5": (48, 12, 90),
    "R6": (52, 12, 90),
    "C17": (56, 12, 90),
    # indicators
    "D2": (4, 15, 0),
    "R7": (8, 15, 90),
    "D3": (4, 11, 0),
    "R8": (8, 11, 90),
    "D4": (4, 7, 0),
    "R9": (8, 7, 90),
    # test points
    "TP1": (16, 4, 0),
    "TP2": (20, 4, 0),
    "TP3": (24, 4, 0),
    "TP4": (28, 4, 0),
    "TP5": (32, 4, 0),
}

JS = r"""
const MM = 39.3701;
const outline = __OUTLINE__;
const place = __PLACE__;
const log = [];
await eda.dmt_EditorControl.openDocument('4081b372105a948c');
await eda.dmt_EditorControl.waitForDocumentReady('4081b372105a948c', 20000);
await new Promise(r => setTimeout(r, 600));

// board outline and mounting holes are drawn by make_outline.py
const outlinePrim = true;

const ids = await eda.pcb_PrimitiveComponent.getAllPrimitiveId();
const comps = await eda.pcb_PrimitiveComponent.get(ids);
const byRef = {};
for (const c of comps) byRef[c.designator] = c;
const missing = [];
for (const [ref, v] of Object.entries(place)) {
  const c = byRef[ref];
  if (!c) { missing.push(ref); continue; }
  await eda.pcb_PrimitiveComponent.modify(c.primitiveId, { x: v[0] * MM, y: v[1] * MM, rotation: v[2] || 0 });
}
return { log, placed: Object.keys(place).length, missing, outline: !!outlinePrim };
"""


if __name__ == "__main__":
    js = JS.replace("__OUTLINE__", json.dumps(OUTLINE)).replace("__PLACE__", json.dumps(PLACEMENT))
    res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=300)
    out = res.stdout
    try:
        d = json.loads(out)
    except Exception:
        print(out[:2000], res.stderr[:2000])
        sys.exit(1)
    print(json.dumps(d.get("result", d), indent=2))
