import json, subprocess, sys

OPEN = "await eda.dmt_EditorControl.openDocument('4081b372105a948c');\nawait new Promise(r => setTimeout(r, 500));\n"


def run(js, timeout=300):
    res = subprocess.run([sys.executable, "eda.py"], input=OPEN + js,
                         capture_output=True, text=True, timeout=timeout)
    if res.returncode != 0:
        print("ERR", res.stderr[-300:]); return None
    d = json.loads(res.stdout)
    if not d.get("success"):
        print("FAIL", json.dumps(d)[:500]); return None
    return d["result"]


r = run("""
const ok = await eda.pcb_Drc.check(true, false, false);
await new Promise(r => setTimeout(r, 1500));
const res = await eda.pcb_Drc.getDrcResult();
const by = {};
const detail = {};
for (const v of res.violations || []) {
  by[v.errorType] = (by[v.errorType] || 0) + 1;
}
return { ok, total: res.totalCount, by };
""")
print(r)
