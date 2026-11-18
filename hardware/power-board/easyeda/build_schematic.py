#!/usr/bin/env python3
"""Build the car-buddy power board schematic as a single A4 sheet (chunked).

The API bridge times out long requests, so the build runs in small steps:

    python3 build_schematic.py clear
    python3 build_schematic.py place 0 12
    ...
    python3 build_schematic.py wire 0 8
    ...
    python3 build_schematic.py decor
"""
import json
import subprocess
import sys

LIB = "0819f05c4eef4c71ace90d822a990e87"

DEV = {
    "mp2315": "27894aca5e23483b838e3b1bd3bf7590",
    "pmos": "7c52ab1755c74dfab8dcfea791efaa07",
    "smbj": "59d609d2ea974f2482a79ad6fb674a03",
    "zener": "0c0ebb187db1495c8c312868a9c5dc28",
    "led": "4c45f846f1a545b08ece52c21937d189",
    "res": "631d7ed0c3024699be8538378f8927ab",
    "cap0603": "ffda9548c8be48a086a273a975055f90",
    "cap1206": "fbd2246bbf844aa6a3d7cd681555feae",
    "cap470": "9d572a2c6fc342808f7b3ac7ebc07b53",
    "ind": "7b5c07ccad864393bb3912f55b89b8be",
    "xt30m": "5edfb3a83d404e64a4f8b3c3de7cb3af",
    "xt30f": "d509f016e0cd44b293d3976f0f428b3f",
    "xh2": "9d6335493fbd4851a2660c73a83aaa3e",
    "xh3": "acc8d8d34b314ee6bca836357a21f749",
    "kf2": "68420db33e68408b961041bd5eec5ead",
    "kcd1": "9a168e62d2b147e3a4b67775a8fedf5e",
    "tp": "cf8b789614c04ee491fd17b89f81a8ab",
    "fuseh": "8d258f8faaa9435884a59c72c25dba1a",
}


def P(ref, dev, val, x, y, pins=None, buses=None, rot=0):
    d = {"ref": ref, "dev": DEV[dev], "val": val, "x": x, "y": y, "rot": rot}
    if pins:
        d["pins"] = pins
    if buses:
        d["buses"] = buses
    return d


PAGE = {
    "uuid": "1c5eea75209bcc4d",
    "title": "车载小车电源板 - 2S 锂电池输入, VBAT / 5V / 3.3V 输出",
    "parts": [
        # --- battery input and reverse protection (top) ---
        P("J1", "xt30m", "XT30PW-M", 120, 740, {"1": "VBAT_BMS", "2": "GND"}),
        P("F1", "fuseh", "5x20 holder", 280, 740, {"1": "VBAT_BMS", "2": "VBAT_FUSED"}),
        P("SW1", "kcd1", "KCD1-101", 450, 740, {"1": "VBAT_FUSED", "2": "VBAT_SW"}),
        P("Q1", "pmos", "SI4435", 630, 735,
          {"4": "Q1_G"},
          [{"pins": ["1", "2", "3"], "net": "VBAT_RAIL", "len": 35},
           {"pins": ["5", "6", "7", "8"], "net": "VBAT_SW", "len": 35}]),
        P("R10", "res", "100k 1%", 770, 765, {"1": "Q1_G", "2": "GND"}),
        P("D5", "zener", "BZX84C12", 770, 700, {"1": "Q1_G", "3": "VBAT_RAIL"}),
        P("C18", "cap0603", "100nF", 890, 740, {"1": "Q1_G", "2": "VBAT_RAIL"}),
        # --- VBAT clamp ---
        P("D1", "smbj", "SMBJ12A", 120, 630, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C1", "cap470", "470uF 16V", 280, 630, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C12", "cap0603", "100nF", 440, 630, {"1": "VBAT_RAIL", "2": "GND"}),
        # --- 3.3 V buck (U2) ---
        P("U2", "mp2315", "MP2315", 170, 515,
          {"1": "VCC3V3", "2": "VBAT_RAIL", "3": "SW3", "4": "GND",
           "5": "BST3", "6": "EN3", "7": "VCC3V3", "8": "FB3"}),
        P("C7", "cap1206", "10uF 25V", 330, 560, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C8", "cap1206", "10uF 25V", 480, 560, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C14", "cap0603", "100nF", 630, 560, {"1": "SW3", "2": "BST3_R"}),
        P("R12", "res", "20R 1%", 780, 560, {"1": "BST3_R", "2": "BST3"}),
        P("C16", "cap0603", "100nF", 930, 560, {"1": "VCC3V3", "2": "GND"}),
        P("R14", "res", "100k 1%", 1080, 560, {"1": "VBAT_RAIL", "2": "EN3"}),
        P("C20", "cap0603", "100pF", 170, 470, {"1": "3V3", "2": "FB3"}),
        P("L2", "ind", "4.7uH", 330, 470, {"1": "SW3", "2": "3V3"}),
        P("C9", "cap1206", "22uF 10V", 480, 470, {"1": "3V3", "2": "GND"}),
        P("C10", "cap1206", "22uF 10V", 630, 470, {"1": "3V3", "2": "GND"}),
        P("C11", "cap0603", "100nF", 780, 470, {"1": "3V3", "2": "GND"}),
        P("R3", "res", "100k 1%", 930, 470, {"1": "3V3", "2": "FB3"}),
        P("R4", "res", "32.4k 1%", 1080, 470, {"1": "FB3", "2": "GND"}),
        # --- 5 V buck (U1) ---
        P("U1", "mp2315", "MP2315", 170, 355,
          {"1": "VCC5V", "2": "VBAT_RAIL", "3": "SW5", "4": "GND",
           "5": "BST5", "6": "EN5", "7": "VCC5V", "8": "FB5"}),
        P("C2", "cap1206", "10uF 25V", 330, 400, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C3", "cap1206", "10uF 25V", 480, 400, {"1": "VBAT_RAIL", "2": "GND"}),
        P("C13", "cap0603", "100nF", 630, 400, {"1": "SW5", "2": "BST5_R"}),
        P("R11", "res", "20R 1%", 780, 400, {"1": "BST5_R", "2": "BST5"}),
        P("C15", "cap0603", "100nF", 930, 400, {"1": "VCC5V", "2": "GND"}),
        P("R13", "res", "100k 1%", 1080, 400, {"1": "VBAT_RAIL", "2": "EN5"}),
        P("C19", "cap0603", "100pF", 170, 310, {"1": "5V", "2": "FB5"}),
        P("L1", "ind", "4.7uH", 330, 310, {"1": "SW5", "2": "5V"}),
        P("C4", "cap1206", "22uF 10V", 480, 310, {"1": "5V", "2": "GND"}),
        P("C5", "cap1206", "22uF 10V", 630, 310, {"1": "5V", "2": "GND"}),
        P("C6", "cap0603", "100nF", 780, 310, {"1": "5V", "2": "GND"}),
        P("R1", "res", "100k 1%", 930, 310, {"1": "5V", "2": "FB5"}),
        P("R2", "res", "19.1k 1%", 1080, 310, {"1": "FB5", "2": "GND"}),
        # --- connectors ---
        P("J3", "xh2", "XH-2P 5V", 110, 250, {"1": "5V", "2": "GND"}),
        P("J4", "xh2", "XH-2P 3V3", 270, 250, {"1": "3V3", "2": "GND"}),
        P("J7", "xh3", "XH-3P", 440, 255, {"1": "VBAT_SENSE", "2": "GND", "3": "3V3"}),
        P("J5", "xt30f", "XT30PW-F", 600, 245,
          {"1": "VBAT_RAIL", "2": "GND", "3": "GND", "4": "GND"}),
        P("J6", "kf2", "KF2EDGK", 800, 250, {"1": "VBAT_RAIL", "2": "GND"}),
        # --- indicators ---
        P("D2", "led", "Green", 90, 200, {"1": "VBAT_RAIL", "2": "LED_VBAT"}),
        P("R7", "res", "2.2k 1%", 300, 200, {"1": "LED_VBAT", "2": "GND"}),
        P("D3", "led", "Green", 510, 200, {"1": "5V", "2": "LED_5V"}),
        P("R8", "res", "1k 1%", 720, 200, {"1": "LED_5V", "2": "GND"}),
        P("D4", "led", "Green", 930, 200, {"1": "3V3", "2": "LED_3V3"}),
        # --- VBAT sense divider ---
        P("R9", "res", "1k 1%", 90, 150, {"1": "LED_3V3", "2": "GND"}),
        P("R5", "res", "100k 1%", 300, 150, {"1": "VBAT_RAIL", "2": "VBAT_SENSE"}),
        P("R6", "res", "33k 1%", 510, 150, {"1": "VBAT_SENSE", "2": "GND"}),
        # --- filter and test points ---
        P("C17", "cap0603", "100nF", 90, 100, {"1": "VBAT_SENSE", "2": "GND"}),
        P("TP1", "tp", "TP", 300, 100, {"1": "VBAT_RAIL"}),
        P("TP2", "tp", "TP", 510, 100, {"1": "5V"}),
        P("TP3", "tp", "TP", 90, 50, {"1": "3V3"}),
        P("TP4", "tp", "TP", 300, 50, {"1": "GND"}),
        P("TP5", "tp", "TP", 510, 50, {"1": "VBAT_SENSE"}),
    ],
    "boxes": [
        {"x": 60, "y": 790, "w": 470, "h": 90, "label": "电池输入"},
        {"x": 560, "y": 800, "w": 400, "h": 140, "label": "防反接保护"},
        {"x": 60, "y": 680, "w": 360, "h": 80, "label": "VBAT 钳位与储能"},
        {"x": 60, "y": 590, "w": 1060, "h": 150, "label": "3.3V / 3A 降压电路 (U2)"},
        {"x": 60, "y": 430, "w": 1060, "h": 140, "label": "5V / 3A 降压电路 (U1)"},
        {"x": 60, "y": 280, "w": 1020, "h": 250, "label": "接口、指示与采样"},
    ],
}

COMMON = r"""
const P = __PAGE__;
const LIB = '__LIB__';
const page = P.uuid;
const openPage = async () => {
  await eda.dmt_EditorControl.openDocument(page);
  await eda.dmt_EditorControl.waitForDocumentReady(page, 20000);
};
const dir = r => r === 0 ? [1, 0] : r === 90 ? [0, 1] : r === 180 ? [-1, 0] : [0, -1];
"""

PLACE = COMMON + r"""
await openPage();
const [a, b] = [__A__, __B__];
const log = [];
const existing = await eda.sch_PrimitiveComponent.get(await eda.sch_PrimitiveComponent.getAllPrimitiveId());
const refs = new Set(existing.map(c => c.designator));
let n = 0;
for (const p of P.parts.slice(a, b)) {
  if (refs.has(p.ref)) continue;
  const c = await eda.sch_PrimitiveComponent.create({ libraryUuid: LIB, uuid: p.dev }, p.x, p.y, '', p.rot || 0, false, true, true);
  if (!c) { log.push('FAIL ' + p.ref); continue; }
  c.setState_Designator(p.ref);
  c.setState_Name(p.val);
  await c.done();
  n++;
}
return { placed: n, log };
"""

WIRE = COMMON + r"""
await openPage();
const [a, b] = [__A__, __B__];
const log = [];
const comps = await eda.sch_PrimitiveComponent.get(await eda.sch_PrimitiveComponent.getAllPrimitiveId());
const byRef = {};
for (const c of comps) byRef[c.designator] = c;
let wires = 0;
for (const p of P.parts.slice(a, b)) {
  const comp = byRef[p.ref];
  if (!comp) { log.push('missing ' + p.ref); continue; }
  const pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId(comp.primitiveId);
  const byN = {};
  for (const q of pins || []) byN[q.pinNumber] = q;
  for (const [num, net] of Object.entries(p.pins || {})) {
    const pin = byN[num];
    if (!pin) { log.push('missing pin ' + p.ref + '.' + num); continue; }
    const [dx, dy] = dir(pin.rotation);
    const L = pin.rotation === 180 ? 70 : 35;
    const w = await eda.sch_PrimitiveWire.create([pin.x, pin.y, pin.x + dx * L, pin.y + dy * L], net);
    if (!w) log.push('FAIL wire ' + p.ref + '.' + num); else wires++;
  }
  for (const bus of (p.buses || [])) {
    const pts = [];
    for (const n of bus.pins) {
      const pin = byN[n];
      if (!pin) { log.push('missing bus pin ' + p.ref + '.' + n); continue; }
      const [dx, dy] = dir(pin.rotation);
      const L = bus.len || 35;
      pts.push({ pin, ex: pin.x + dx * L, ey: pin.y + dy * L });
    }
    if (pts.length < 2) continue;
    const segs = pts.map(q => [q.pin.x, q.pin.y, q.ex, q.ey]);
    const s = [...pts].sort((x, y) => x.ey - y.ey);
    for (let i = 0; i + 1 < s.length; i++) segs.push([s[i].ex, s[i].ey, s[i + 1].ex, s[i + 1].ey]);
    const w = await eda.sch_PrimitiveWire.create(segs, bus.net);
    if (!w) log.push('FAIL bus ' + p.ref); else wires++;
  }
}
return { wires, log };
"""


def run(js):
    res = subprocess.run([sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=120)
    if res.returncode != 0:
        print(res.stderr[-1500:])
        return None
    d = json.loads(res.stdout)
    if not d.get("success"):
        print(json.dumps(d)[:800])
        return None
    return d["result"]


def main():
    args = sys.argv[1:]
    mode = args[0] if args else "help"
    base = PAGE
    if mode == "clear":
        js = (COMMON + r"""
await openPage();
const c = await eda.sch_PrimitiveComponent.getAllPrimitiveId();
if (c.length) await eda.sch_PrimitiveComponent.delete(c);
const w = await eda.sch_PrimitiveWire.getAllPrimitiveId();
if (w.length) await eda.sch_PrimitiveWire.delete(w);
const t = await eda.sch_PrimitiveText.getAllPrimitiveId();
if (t.length) await eda.sch_PrimitiveText.delete(t);
const r = await eda.sch_PrimitiveRectangle.getAllPrimitiveId();
if (r.length) await eda.sch_PrimitiveRectangle.delete(r);
return { comps: c.length, wires: w.length, texts: t.length, rects: r.length };
""").replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        print(run(js))
    elif mode == "clearwires":
        js = (COMMON + r"""
await openPage();
const ids = await eda.sch_PrimitiveWire.getAllPrimitiveId();
return { wires: ids.length, ids };
""").replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        r = run(js)
        print("wires:", r["wires"] if r else None)
        if r:
            ids = r["ids"]
            for i in range(0, len(ids), 40):
                chunk = ids[i:i + 40]
                js = COMMON + "await openPage(); return await eda.sch_PrimitiveWire.delete(%s);" % json.dumps(chunk)
                js = js.replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
                print("deleted", i, run(js))
    elif mode == "move":
        a, b = int(args[1]), int(args[2])
        js = (COMMON + r"""
await openPage();
const comps = await eda.sch_PrimitiveComponent.get(await eda.sch_PrimitiveComponent.getAllPrimitiveId());
const byRef = {};
for (const c of comps) byRef[c.designator] = c;
const log = [];
let n = 0;
for (const p of P.parts.slice(__A__, __B__)) {
  const c = byRef[p.ref];
  if (!c) { log.push('missing ' + p.ref); continue; }
  await eda.sch_PrimitiveComponent.modify(c.primitiveId, { x: p.x, y: p.y, rotation: p.rot || 0 });
  n++;
}
return { moved: n, log };
""").replace("__A__", str(a)).replace("__B__", str(b))
        js = js.replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        print(run(js))
    elif mode == "place":
        a, b = int(args[1]), int(args[2])
        js = PLACE.replace("__A__", str(a)).replace("__B__", str(b))
        js = js.replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        print(run(js))
    elif mode == "wire":
        a, b = int(args[1]), int(args[2])
        js = WIRE.replace("__A__", str(a)).replace("__B__", str(b))
        js = js.replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        print(run(js))
    elif mode == "decor":
        js = (COMMON + r"""
await openPage();
const t = await eda.sch_PrimitiveText.getAllPrimitiveId();
if (t.length) await eda.sch_PrimitiveText.delete(t);
const r = await eda.sch_PrimitiveRectangle.getAllPrimitiveId();
if (r.length) await eda.sch_PrimitiveRectangle.delete(r);
await eda.sch_PrimitiveText.create(60, 810, P.title, 0, null, null, 18, true, false, false, 1);
for (const b of P.boxes) {
  await eda.sch_PrimitiveRectangle.create(b.x, b.y, b.w, b.h, 0, 0, '#B0B0B0', null, 1, 1, null);
  await eda.sch_PrimitiveText.create(b.x + 8, b.y - 8, b.label, 0, '#666666', null, 12, false, false, false, 1);
}
await eda.sch_Document.save();
return { boxes: P.boxes.length };
""").replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)
        print(run(js))
    elif mode == "save":
        js = COMMON + "await openPage(); await eda.sch_Document.save(); return true;"
        print(run(js.replace("__PAGE__", json.dumps(base)).replace("__LIB__", LIB)))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
