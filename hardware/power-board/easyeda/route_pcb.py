#!/usr/bin/env python3
"""Route the power board v2, add GND pours and run DRC.

Run after place_pcb.py + check_placement.py. The autorouter works
asynchronously in the EasyEDA client; this script starts the job, waits
until the ratlines disappear, adds the top/bottom GND pours and reports
the DRC result.

Note: the autorouter uses thin tracks for everything. The VBAT/5V power
tracks must be re-routed manually in the GUI (>= 3 mm VBAT + pours, per
DECISIONS.md D10) before Gerber export - widening in place after routing
creates spacing violations.
"""
import json
import subprocess
import sys
import time

MM = 39.3701

BOARD_W, BOARD_H = 100, 86

OPEN = "await eda.dmt_EditorControl.openDocument('4081b372105a948c');\nawait new Promise(r => setTimeout(r, 500));\n"


def run(js, timeout=300):
    js = OPEN + js
    res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=timeout)
    if res.returncode != 0:
        raise SystemExit(res.stderr[-2000:])
    d = json.loads(res.stdout)
    if not d.get("success"):
        raise SystemExit(json.dumps(d)[:2000])
    return d["result"]


def main():
    print("clearing tracks ...")
    run("""
const l = await eda.pcb_PrimitiveLine.getAllPrimitiveId();
if (l.length) await eda.pcb_PrimitiveLine.delete(l);
const v = await eda.pcb_PrimitiveVia.getAllPrimitiveId();
if (v.length) await eda.pcb_PrimitiveVia.delete(v);
await eda.pcb_Document.save();
return true;
""")
    print("starting autoroute ...")
    job = run("return await eda.pcb_Document.startAutoRouting({ layers: [1, 2] });")
    print("job", job)
    counts = {}
    for i in range(40):
        time.sleep(20)
        counts = run("""
const l = await eda.pcb_PrimitiveLine.getAllPrimitiveId();
const v = await eda.pcb_PrimitiveVia.getAllPrimitiveId();
const rl = await eda.pcb_Document.getRatlines();
return { lines: l.length, vias: v.length, ratlines: (rl && rl.items ? rl.items.length : -1) };
""")
        print(f"  {i*20:4d}s lines={counts['lines']} vias={counts['vias']} ratlines={counts['ratlines']}")
        if counts["lines"] > 50 and counts["ratlines"] == 0:
            break
    if counts.get("ratlines") != 0:
        print("WARNING: autoroute did not finish cleanly")
    print("saving ...")
    run("await eda.pcb_Document.save(); return true;")
    print("adding GND pours ...")
    pours = run(f"""
const MM = {MM};
const old = await eda.pcb_PrimitivePour.getAllPrimitiveId();
if (old.length) await eda.pcb_PrimitivePour.delete(old);
const x1 = 0.3 * MM, y1 = 0.3 * MM, x2 = {BOARD_W} * MM - 0.3 * MM, y2 = {BOARD_H} * MM - 0.3 * MM;
const mk = () => eda.pcb_MathPolygon.createPolygon([x1, y1, 'L', x2, y1, x2, y2, x1, y2, x1, y1]);
const top = await eda.pcb_PrimitivePour.create('GND', 1, mk(), 'solid', false, 'GND_TOP', 1, 10, false);
const bot = await eda.pcb_PrimitivePour.create('GND', 2, mk(), 'solid', false, 'GND_BOTTOM', 1, 10, false);
await eda.pcb_Document.save();
return {{ top: !!top, bottom: !!bot }};
""")
    print("pours", pours)
    time.sleep(3)
    drc = run("""
const ok = await eda.pcb_Drc.check(true, false, false);
await new Promise(r => setTimeout(r, 1500));
const res = await eda.pcb_Drc.getDrcResult();
const by = {};
for (const v of res.violations || []) by[v.errorType] = (by[v.errorType] || 0) + 1;
return { ok, total: res.totalCount, by };
""")
    print("DRC", drc)


if __name__ == "__main__":
    main()
