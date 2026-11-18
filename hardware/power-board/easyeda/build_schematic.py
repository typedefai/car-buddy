#!/usr/bin/env python3
"""Build the car-buddy power board schematic in EasyEDA Pro via the API bridge.

Usage:
    python3 build_schematic.py <page-key>     # build one page
    python3 build_schematic.py --list         # list pages

The part list mirrors hardware/power-board/netlist.csv. Nets are attached to
short pin stubs as named wires; EasyEDA merges same-named wires into one net.
Schematic coordinates are 0.01 inch with Y increasing upward.
"""
import json
import subprocess
import sys

LIB = "0819f05c4eef4c71ace90d822a990e87"

# library device uuids (searched in the EasyEDA standard library)
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


PAGES = {
    "input": {
        "uuid": "8da9c96e54f30398",
        "title": "car-buddy power board - 1. Input & protection",
        "parts": [
            P("J1", "xt30m", "XT30PW-M", 140, 720, {"1": "VBAT_BMS", "2": "GND"}),
            P("F1", "fuseh", "5x20 holder", 340, 720, {"1": "VBAT_BMS", "2": "VBAT_FUSED"}),
            P("SW1", "kcd1", "KCD1-101", 540, 720, {"1": "VBAT_FUSED", "2": "VBAT_SW"}),
            P("Q1", "pmos", "SI4435", 820, 700,
              {"4": "Q1_G"},
              [{"pins": ["1", "2", "3"], "net": "VBAT", "len": 35},
               {"pins": ["5", "6", "7", "8"], "net": "VBAT_SW", "len": 35}]),
            P("R10", "res", "100k 1%", 1000, 770, {"1": "Q1_G", "2": "GND"}),
            P("D5", "zener", "BZX84C12", 1000, 660, {"1": "Q1_G", "3": "VBAT"}),
            P("C18", "cap0603", "100nF", 1000, 560, {"1": "Q1_G", "2": "VBAT"}),
            P("D1", "smbj", "SMBJ12A", 140, 560, {"1": "VBAT", "2": "GND"}),
            P("C1", "cap470", "470uF 16V", 340, 560, {"1": "VBAT", "2": "GND"}),
            P("C12", "cap0603", "100nF", 540, 560, {"1": "VBAT", "2": "GND"}),
        ],
        "boxes": [
            {"x": 60, "y": 770, "w": 560, "h": 90, "label": "Battery input"},
            {"x": 680, "y": 800, "w": 440, "h": 260, "label": "Reverse-polarity protection"},
            {"x": 60, "y": 620, "w": 560, "h": 90, "label": "VBAT clamp & bulk"},
        ],
    },
    "5v": {
        "uuid": "f122051c8588a54e",
        "title": "car-buddy power board - 2. 5 V / 3 A buck (U1)",
        "parts": [
            P("U1", "mp2315", "MP2315", 420, 620,
              {"1": "VCC5", "2": "VBAT", "3": "SW5", "4": "GND",
               "5": "BST5", "6": "EN5", "7": "VCC5", "8": "FB5"}),
            P("C2", "cap1206", "10uF 25V", 140, 710, {"1": "VBAT", "2": "GND"}),
            P("C3", "cap1206", "10uF 25V", 340, 710, {"1": "VBAT", "2": "GND"}),
            P("C13", "cap0603", "100nF", 640, 710, {"1": "SW5", "2": "BST5_R"}),
            P("R11", "res", "20R 1%", 860, 710, {"1": "BST5_R", "2": "BST5"}),
            P("C15", "cap0603", "100nF", 640, 620, {"1": "VCC5", "2": "GND"}),
            P("R13", "res", "100k 1%", 860, 620, {"1": "VBAT", "2": "EN5"}),
            P("L1", "ind", "4.7uH", 640, 510, {"1": "SW5", "2": "5V"}),
            P("C4", "cap1206", "22uF 10V", 140, 390, {"1": "5V", "2": "GND"}),
            P("C5", "cap1206", "22uF 10V", 340, 390, {"1": "5V", "2": "GND"}),
            P("C6", "cap0603", "100nF", 540, 390, {"1": "5V", "2": "GND"}),
            P("R1", "res", "100k 1%", 140, 280, {"1": "5V", "2": "FB5"}),
            P("R2", "res", "19.1k 1%", 340, 280, {"1": "FB5", "2": "GND"}),
            P("C19", "cap0603", "100pF", 540, 280, {"1": "5V", "2": "FB5"}),
        ],
        "boxes": [
            {"x": 80, "y": 770, "w": 400, "h": 90, "label": "Input capacitors"},
            {"x": 300, "y": 680, "w": 250, "h": 100, "label": "U1 MP2315"},
            {"x": 580, "y": 770, "w": 420, "h": 90, "label": "Bootstrap"},
            {"x": 580, "y": 680, "w": 420, "h": 90, "label": "VCC / enable"},
            {"x": 580, "y": 560, "w": 220, "h": 90, "label": "Inductor"},
            {"x": 80, "y": 440, "w": 560, "h": 90, "label": "Output capacitors"},
            {"x": 80, "y": 330, "w": 560, "h": 90, "label": "Feedback divider"},
        ],
    },
    "3v3": {
        "uuid": "b1a685136aabdee8",
        "title": "car-buddy power board - 3. 3.3 V / 3 A buck (U2)",
        "parts": [
            P("U2", "mp2315", "MP2315", 420, 620,
              {"1": "VCC3", "2": "VBAT", "3": "SW3", "4": "GND",
               "5": "BST3", "6": "EN3", "7": "VCC3", "8": "FB3"}),
            P("C7", "cap1206", "10uF 25V", 140, 710, {"1": "VBAT", "2": "GND"}),
            P("C8", "cap1206", "10uF 25V", 340, 710, {"1": "VBAT", "2": "GND"}),
            P("C14", "cap0603", "100nF", 640, 710, {"1": "SW3", "2": "BST3_R"}),
            P("R12", "res", "20R 1%", 860, 710, {"1": "BST3_R", "2": "BST3"}),
            P("C16", "cap0603", "100nF", 640, 620, {"1": "VCC3", "2": "GND"}),
            P("R14", "res", "100k 1%", 860, 620, {"1": "VBAT", "2": "EN3"}),
            P("L2", "ind", "4.7uH", 640, 510, {"1": "SW3", "2": "3V3"}),
            P("C9", "cap1206", "22uF 10V", 140, 390, {"1": "3V3", "2": "GND"}),
            P("C10", "cap1206", "22uF 10V", 340, 390, {"1": "3V3", "2": "GND"}),
            P("C11", "cap0603", "100nF", 540, 390, {"1": "3V3", "2": "GND"}),
            P("R3", "res", "100k 1%", 140, 280, {"1": "3V3", "2": "FB3"}),
            P("R4", "res", "32.4k 1%", 340, 280, {"1": "FB3", "2": "GND"}),
            P("C20", "cap0603", "100pF", 540, 280, {"1": "3V3", "2": "FB3"}),
        ],
        "boxes": [
            {"x": 80, "y": 770, "w": 400, "h": 90, "label": "Input capacitors"},
            {"x": 300, "y": 680, "w": 250, "h": 100, "label": "U2 MP2315"},
            {"x": 580, "y": 770, "w": 420, "h": 90, "label": "Bootstrap"},
            {"x": 580, "y": 680, "w": 420, "h": 90, "label": "VCC / enable"},
            {"x": 580, "y": 560, "w": 220, "h": 90, "label": "Inductor"},
            {"x": 80, "y": 440, "w": 560, "h": 90, "label": "Output capacitors"},
            {"x": 80, "y": 330, "w": 560, "h": 90, "label": "Feedback divider"},
        ],
    },
    "io": {
        "uuid": "53e158872de05a41",
        "title": "car-buddy power board - 4. Sense, indicators & connectors",
        "parts": [
            P("R5", "res", "100k 1%", 140, 710, {"1": "VBAT", "2": "VBAT_SENSE"}),
            P("R6", "res", "33k 1%", 340, 710, {"1": "VBAT_SENSE", "2": "GND"}),
            P("C17", "cap0603", "100nF", 540, 710, {"1": "VBAT_SENSE", "2": "GND"}),
            P("J7", "xh3", "XH-3P", 760, 715,
              {"1": "VBAT_SENSE", "2": "GND", "3": "3V3"}),
            P("D2", "led", "Green", 140, 590, {"1": "VBAT", "2": "LED_VBAT"}),
            P("R7", "res", "2.2k 1%", 340, 590, {"1": "LED_VBAT", "2": "GND"}),
            P("D3", "led", "Green", 540, 590, {"1": "5V", "2": "LED_5V"}),
            P("R8", "res", "1k 1%", 740, 590, {"1": "LED_5V", "2": "GND"}),
            P("D4", "led", "Green", 140, 480, {"1": "3V3", "2": "LED_3V3"}),
            P("R9", "res", "1k 1%", 340, 480, {"1": "LED_3V3", "2": "GND"}),
            P("J3", "xh2", "XH-2P 5V", 140, 370, {"1": "5V", "2": "GND"}),
            P("J4", "xh2", "XH-2P 3V3", 340, 370, {"1": "3V3", "2": "GND"}),
            P("J5", "xt30f", "XT30PW-F", 540, 370,
              {"1": "VBAT", "2": "GND", "3": "GND", "4": "GND"}),
            P("J6", "kf2", "KF2EDGK", 760, 370, {"1": "VBAT", "2": "GND"}),
            P("TP1", "tp", "TP", 140, 260, {"1": "VBAT"}),
            P("TP2", "tp", "TP", 340, 260, {"1": "5V"}),
            P("TP3", "tp", "TP", 540, 260, {"1": "3V3"}),
            P("TP4", "tp", "TP", 740, 260, {"1": "GND"}),
            P("TP5", "tp", "TP", 940, 260, {"1": "VBAT_SENSE"}),
        ],
        "boxes": [
            {"x": 60, "y": 760, "w": 780, "h": 90, "label": "VBAT sense"},
            {"x": 60, "y": 640, "w": 860, "h": 200, "label": "Power indicators"},
            {"x": 60, "y": 430, "w": 800, "h": 100, "label": "Output connectors"},
            {"x": 60, "y": 320, "w": 940, "h": 90, "label": "Test points"},
        ],
    },
}

JS = r"""
const P = __PAGE__;
const LIB = '__LIB__';
const log = [];
await eda.dmt_EditorControl.openDocument(P.uuid);
await eda.dmt_EditorControl.waitForDocumentReady(P.uuid, 30000);
const oldC = await eda.sch_PrimitiveComponent.getAllPrimitiveId();
if (oldC.length) await eda.sch_PrimitiveComponent.delete(oldC);
const oldW = await eda.sch_PrimitiveWire.getAllPrimitiveId();
if (oldW.length) await eda.sch_PrimitiveWire.delete(oldW);
const oldT = await eda.sch_PrimitiveText.getAllPrimitiveId();
if (oldT.length) await eda.sch_PrimitiveText.delete(oldT);
const oldR = await eda.sch_PrimitiveRectangle.getAllPrimitiveId();
if (oldR.length) await eda.sch_PrimitiveRectangle.delete(oldR);
await new Promise(r => setTimeout(r, 300));
const placed = {};
for (const p of P.parts) {
  try {
    const c = await eda.sch_PrimitiveComponent.create({ libraryUuid: LIB, uuid: p.dev }, p.x, p.y, '', p.rot || 0, false, true, true);
    if (!c) { log.push('FAIL place ' + p.ref); continue; }
    c.setState_Designator(p.ref);
    c.setState_Name(p.val);
    await c.done();
    placed[p.ref] = c.primitiveId;
  } catch (e) { log.push('ERR place ' + p.ref + ': ' + (e.message || e)); }
}
const pinsByRef = {};
for (const [ref, id] of Object.entries(placed)) {
  const pins = await eda.sch_PrimitiveComponent.getAllPinsByPrimitiveId(id);
  pinsByRef[ref] = (pins || []).map(q => ({ n: q.pinNumber, x: q.x, y: q.y, r: q.rotation }));
}
const dir = r => r === 0 ? [1, 0] : r === 90 ? [0, 1] : r === 180 ? [-1, 0] : [0, -1];
for (const p of P.parts) {
  const pins = pinsByRef[p.ref] || [];
  const byN = {};
  for (const q of pins) byN[q.n] = q;
  for (const [num, net] of Object.entries(p.pins || {})) {
    const pin = byN[num];
    if (!pin) { log.push('missing pin ' + p.ref + '.' + num); continue; }
    const [dx, dy] = dir(pin.r);
    const L = pin.r === 180 ? 70 : 35;
    const ex = pin.x + dx * L, ey = pin.y + dy * L;
    const w = await eda.sch_PrimitiveWire.create([pin.x, pin.y, ex, ey], net);
    if (!w) log.push('FAIL wire ' + p.ref + '.' + num + ' ' + net);
  }
  for (const bus of (p.buses || [])) {
    const pts = [];
    for (const n of bus.pins) {
      const pin = byN[n];
      if (!pin) { log.push('missing bus pin ' + p.ref + '.' + n); continue; }
      const [dx, dy] = dir(pin.r);
      const L = bus.len || 30;
      pts.push({ pin, ex: pin.x + dx * L, ey: pin.y + dy * L });
    }
    if (pts.length < 2) continue;
    const segs = pts.map(q => [q.pin.x, q.pin.y, q.ex, q.ey]);
    const vertical = pts.every(q => q.ex === pts[0].ex);
    if (vertical) {
      const s = [...pts].sort((a, b) => a.ey - b.ey);
      for (let i = 0; i + 1 < s.length; i++) segs.push([s[i].ex, s[i].ey, s[i + 1].ex, s[i + 1].ey]);
    } else {
      const s = [...pts].sort((a, b) => a.ex - b.ex);
      for (let i = 0; i + 1 < s.length; i++) segs.push([s[i].ex, s[i].ey, s[i + 1].ex, s[i + 1].ey]);
    }
    const w = await eda.sch_PrimitiveWire.create(segs, bus.net);
    if (!w) log.push('FAIL bus ' + p.ref + ' ' + bus.net);
  }
}
await eda.sch_PrimitiveText.create(60, 800, P.title, 0, null, null, 18, true, false, false, 1);
for (const b of (P.boxes || [])) {
  await eda.sch_PrimitiveRectangle.create(b.x, b.y, b.w, b.h, 0, 0, '#B0B0B0', null, 1, 1, null);
  await eda.sch_PrimitiveText.create(b.x + 8, b.y - 8, b.label, 0, '#666666', null, 12, false, false, false, 1);
}
return { log, placed: Object.keys(placed).length, pins: pinsByRef };
"""


def build(key):
    page = PAGES[key]
    js = JS.replace("__PAGE__", json.dumps(page)).replace("__LIB__", LIB)
    out = subprocess.run(
        [sys.executable, "eda.py"], input=js, capture_output=True, text=True, timeout=300
    )
    if out.returncode != 0:
        print(out.stderr)
        return None
    data = json.loads(out.stdout)
    if not data.get("success"):
        print(json.dumps(data, indent=2)[:2000])
        return None
    return data["result"]


if __name__ == "__main__":
    if len(sys.argv) < 2 or sys.argv[1] == "--list":
        print("pages:", ", ".join(PAGES))
        sys.exit(0)
    res = build(sys.argv[1])
    if res:
        print("placed:", res["placed"], "log:", res["log"])
        with open(f"pins_{sys.argv[1]}.json", "w") as f:
            json.dump(res["pins"], f, indent=1)
        print("pins saved")
