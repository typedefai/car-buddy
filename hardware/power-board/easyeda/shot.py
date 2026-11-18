#!/usr/bin/env python3
"""Screenshot an EasyEDA document page to a PNG (via base64 over the bridge)."""
import base64
import json
import subprocess
import sys

uuid = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else "/tmp/page.png"
js = r"""
const page = '__UUID__';
const tabId = page + '@f7155a0e0e356c3ee55e5cb5f2ac522bcb1b042d02e0207e74869905c46cecb9';
await eda.dmt_EditorControl.openDocument(page);
await new Promise(r => setTimeout(r, 600));
await eda.dmt_EditorControl.zoomToAllPrimitives(undefined, tabId);
await new Promise(r => setTimeout(r, 900));
const blob = await eda.dmt_EditorControl.getCurrentRenderedAreaImage(tabId);
const bytes = new Uint8Array(await blob.arrayBuffer());
let bin = '';
for (let i = 0; i < bytes.length; i += 0x8000) bin += String.fromCharCode.apply(null, bytes.subarray(i, i + 0x8000));
return { b64: btoa(bin) };
""".replace("__UUID__", uuid)
res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=120)
data = json.loads(res.stdout)
b64 = (data.get("result") or {}).get("b64")
if not b64:
    print("no image", data)
    sys.exit(1)
with open(out, "wb") as f:
    f.write(base64.b64decode(b64))
print("saved", out)
