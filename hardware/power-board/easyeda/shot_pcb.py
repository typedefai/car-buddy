#!/usr/bin/env python3
"""Screenshot the PCB canvas (optionally a region given in mm) to a PNG.

usage: shot_pcb.py out.png [x1 y1 x2 y2]   # mm, y up
"""
import base64
import json
import subprocess
import sys

out = sys.argv[1] if len(sys.argv) > 1 else "/tmp/pcb.png"
region = sys.argv[2:6]

if len(region) == 4:
    x1, y1, x2, y2 = [float(v) * 39.3701 for v in region]
    nav = f"await eda.pcb_Document.navigateToRegion({x1}, {x2}, {y1}, {y2});"
else:
    nav = "await eda.pcb_Document.zoomToBoardOutline();"

js = r"""
const pcb = 'aa65e6d7e62b98c1';
const tabId = pcb + '@f7155a0e0e356c3ee55e5cb5f2ac522bcb1b042d02e0207e74869905c46cecb9';
await eda.dmt_EditorControl.openDocument(pcb);
await new Promise(r => setTimeout(r, 600));
__NAV__
await new Promise(r => setTimeout(r, 1200));
const blob = await eda.dmt_EditorControl.getCurrentRenderedAreaImage(tabId);
const bytes = new Uint8Array(await blob.arrayBuffer());
let bin = '';
for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
return { b64: btoa(bin) };
""".replace("__NAV__", nav)

res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=120)
data = json.loads(res.stdout)
b64 = (data.get("result") or {}).get("b64")
if not b64:
    print("no image", data)
    sys.exit(1)
open(out, "wb").write(base64.b64decode(b64))
print("saved", out)
