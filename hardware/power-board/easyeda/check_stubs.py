import json, subprocess, sys
JS = "await eda.dmt_EditorControl.openDocument('1c5eea75209bcc4d');\nawait new Promise(r => setTimeout(r, 800));\nconst ids = await eda.sch_PrimitiveWire.getAllPrimitiveId();\nconst ws = await eda.sch_PrimitiveWire.get(ids);\nreturn (ws||[]).map(w=>({net:w.net||'', line:w.line||[]}));"
res = subprocess.run([sys.executable, "eda.py"], input=JS, capture_output=True, text=True, timeout=120)
data = json.loads(res.stdout)
ws = data.get("result") or []
print("total wires:", len(ws))

def on_seg(px, py, x1, y1, x2, y2, tol=6):
    if min(x1,x2)-tol <= px <= max(x1,x2)+tol and min(y1,y2)-tol <= py <= max(y1,y2)+tol:
        # point-to-segment distance
        import math
        dx, dy = x2-x1, y2-y1
        if dx == 0 and dy == 0:
            d = math.hypot(px-x1, py-y1)
        else:
            t = ((px-x1)*dx + (py-y1)*dy) / (dx*dx + dy*dy)
            t = max(0, min(1, t))
            d = math.hypot(px-(x1+t*dx), py-(y1+t*dy))
        return d <= tol
    return False

def segs_touch(l1, l2, tol=6):
    x1,y1,x2,y2 = l1
    x3,y3,x4,y4 = l2
    # check endpoints of l2 against l1 and vice versa
    if on_seg(x3,y3,x1,y1,x2,y2,tol) or on_seg(x4,y4,x1,y1,x2,y2,tol): return True
    if on_seg(x1,y1,x3,y3,x4,y4,tol) or on_seg(x2,y2,x3,y3,x4,y4,tol): return True
    return False

problems = 0
for i in range(len(ws)):
    for j in range(i+1, len(ws)):
        a, b = ws[i], ws[j]
        if a["net"] == b["net"]: continue
        if not a["line"] or not b["line"]: continue
        if segs_touch(a["line"], b["line"], 6):
            problems += 1
            print(f"TOUCH: {a['net']} {a['line']}  <->  {b['net']} {b['line']}")
print("cross-net touches:", problems)
