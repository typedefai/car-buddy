#!/usr/bin/env python3
"""Check the v2 PCB placement: pairwise component bbox overlaps."""
import json
import subprocess
import sys

JS = r"""
await eda.dmt_EditorControl.openDocument('4081b372105a948c');
await new Promise(r => setTimeout(r, 1500));
const ids = await eda.pcb_PrimitiveComponent.getAllPrimitiveId();
const comps = await eda.pcb_PrimitiveComponent.get(ids);
const out = [];
for (const c of comps) {
  const bb = await eda.pcb_Primitive.getPrimitivesBBox([c.primitiveId]);
  if (bb) out.push({ ref: c.designator, x1: bb.minX, y1: bb.minY, x2: bb.maxX, y2: bb.maxY });
}
return out;
"""

res = subprocess.run([sys.executable, "eda.py"], input=JS, capture_output=True, text=True, timeout=300)
data = json.loads(res.stdout)
boxes = data.get("result") or []
MM = 39.3701

problems = 0
for i in range(len(boxes)):
    for j in range(i + 1, len(boxes)):
        a, b = boxes[i], boxes[j]
        ox = min(a["x2"], b["x2"]) - max(a["x1"], b["x1"])
        oy = min(a["y2"], b["y2"]) - max(a["y1"], b["y1"])
        if ox > 0 and oy > 0:
            problems += 1
            print(f"OVERLAP {a['ref']} <-> {b['ref']}: {ox/MM:.2f} x {oy/MM:.2f} mm")
# also check everything is inside the 100x86 board
for b in boxes:
    if b["x1"] < 0 or b["y1"] < 0 or b["x2"] > 100 * MM or b["y2"] > 86 * MM:
        problems += 1
        print(f"OFF-BOARD {b['ref']}: ({b['x1']/MM:.1f},{b['y1']/MM:.1f})-({b['x2']/MM:.1f},{b['y2']/MM:.1f})")
print(f"\ncomponents: {len(boxes)}, problems: {problems}")
