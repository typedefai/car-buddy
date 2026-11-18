#!/usr/bin/env python3
"""Build the car-buddy power board v2 schematic as a single A4 sheet (chunked).

v2 changes (DECISIONS.md D10):
  - two independent MP2315 5 V bucks (one per board), ferrite beads on
    buck inputs and outputs
  - INA219 (0.01 ohm shunt) on the VBAT rail + local ME6211 3.3 V LDO
  - AOD409 reverse-polarity FET with gate RC soft-start
  - KCD1-105 switches (main + motor rail), ATO blade fuse holder
  - 3.3 V rail and analog VBAT sense divider removed

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
    "aod409": "bb3fd640aa0245798b054c61559572fe",
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
    "kcd105": "bff339f70f414e04b1c7e185dbf08032",
    "fuse_ato": "b90d9e73f32442c48b2f0a8471b01d40",
    "bead": "6e8e85fe54824e3ea431634e6a431c52",
    "ina219": "a63fabcf95d444249a9369156a828811",
    "me6211": "0cc0998dfe244d129b1deee194b6604c",
    "shunt": "d54de329b2464863be62dda8d6f9a69c",
    "tp": "cf8b789614c04ee491fd17b89f81a8ab",
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
    "title": "车载小车电源板 v2 - 2S 锂电输入 / VBAT 电机轨 / 双路 5V / INA219 遥测",
    "parts": [
        # --- 电池输入 (block 1) ---
        P("J1", "xt30m", "XT30PW-M", 120, 740, {"1": "VBAT_BMS", "2": "GND"}),
        P("F1", "fuse_ato", "ATO blade 7.5A", 300, 740, {"1": "VBAT_BMS", "2": "VBAT_FUSED"}),
        P("SW1", "kcd105", "KCD1-105", 480, 740, {"1": "VBAT_FUSED", "2": "VBAT_SW"}),
        # --- 防反接与软启动 (block 2) ---
        P("Q1", "aod409", "AOD409", 630, 735,
          {"1": "Q1_G", "2": "VBAT_SW", "3": "VBAT_RAIL"}),
        P("R10", "res", "100k 1%", 790, 765, {"1": "Q1_G", "2": "GND"}),
        P("D5", "zener", "BZX84C12", 790, 715, {"1": "Q1_G", "3": "VBAT_RAIL"}),
        P("C18", "cap0603", "100nF", 920, 735, {"1": "Q1_G", "2": "VBAT_RAIL"}),
        # --- VBAT 母线 (block 3) ---
        P("D1", "smbj", "SMBJ12A", 120, 630, {"1": "VBAT_LOAD", "2": "GND"}),
        P("C1", "cap470", "470uF 16V", 270, 630, {"1": "VBAT_LOAD", "2": "GND"}),
        P("C12", "cap0603", "100nF", 420, 630, {"1": "VBAT_LOAD", "2": "GND"}),
        P("C21", "cap0603", "1uF 25V", 540, 630, {"1": "VBAT_LOAD", "2": "GND"}),
        # --- 电机轨开关与输出 (block 4) ---
        P("SW2", "kcd105", "KCD1-105", 750, 630, {"1": "VBAT_LOAD", "2": "VBAT_MOTOR"}),
        P("J5", "xt30f", "XT30PW-F", 930, 625,
          {"1": "VBAT_MOTOR", "2": "GND", "3": "GND", "4": "GND"}),
        # --- 5V buck #1 -> ESP32 (block 5) ---
        P("FB1", "bead", "600R 0805", 120, 560, {"1": "VBAT_LOAD", "2": "VBAT_B1"}),
        P("U1", "mp2315", "MP2315", 270, 515,
          {"1": "VCC5V1", "2": "VBAT_B1", "3": "SW5_1", "4": "GND",
           "5": "BST5_1", "6": "EN5_1", "7": "VCC5V1", "8": "FB5_1"}),
        P("C2", "cap1206", "10uF 25V", 420, 560, {"1": "VBAT_B1", "2": "GND"}),
        P("C3", "cap1206", "10uF 25V", 570, 560, {"1": "VBAT_B1", "2": "GND"}),
        P("C13", "cap0603", "100nF", 720, 560, {"1": "SW5_1", "2": "BST5_1R"}),
        P("R11", "res", "20R 1%", 870, 560, {"1": "BST5_1R", "2": "BST5_1"}),
        P("C15", "cap0603", "100nF", 1020, 560, {"1": "VCC5V1", "2": "GND"}),
        P("R13", "res", "100k 1%", 110, 540, {"1": "VBAT_LOAD", "2": "EN5_1"}),
        P("C19", "cap0603", "100pF", 120, 470, {"1": "5V1", "2": "FB5_1"}),
        P("L1", "ind", "4.7uH", 270, 470, {"1": "SW5_1", "2": "5V1"}),
        P("C4", "cap1206", "22uF 10V", 420, 470, {"1": "5V1", "2": "GND"}),
        P("C5", "cap1206", "22uF 10V", 570, 470, {"1": "5V1", "2": "GND"}),
        P("C6", "cap0603", "100nF", 720, 470, {"1": "5V1", "2": "GND"}),
        P("R1", "res", "100k 1%", 870, 470, {"1": "5V1", "2": "FB5_1"}),
        P("R2", "res", "19.1k 1%", 1020, 470, {"1": "FB5_1", "2": "GND"}),
        P("FB3", "bead", "600R 0805", 120, 420, {"1": "5V1", "2": "5V1_OUT"}),
        P("J3", "xh2", "XH-2P 5V1", 270, 420, {"1": "5V1_OUT", "2": "GND"}),
        # --- 5V buck #2 -> STM32 carrier (block 6) ---
        P("FB2", "bead", "600R 0805", 120, 400, {"1": "VBAT_LOAD", "2": "VBAT_B2"}),
        P("U2", "mp2315", "MP2315", 270, 355,
          {"1": "VCC5V2", "2": "VBAT_B2", "3": "SW5_2", "4": "GND",
           "5": "BST5_2", "6": "EN5_2", "7": "VCC5V2", "8": "FB5_2"}),
        P("C7", "cap1206", "10uF 25V", 420, 400, {"1": "VBAT_B2", "2": "GND"}),
        P("C8", "cap1206", "10uF 25V", 570, 400, {"1": "VBAT_B2", "2": "GND"}),
        P("C14", "cap0603", "100nF", 720, 400, {"1": "SW5_2", "2": "BST5_2R"}),
        P("R12", "res", "20R 1%", 870, 400, {"1": "BST5_2R", "2": "BST5_2"}),
        P("C16", "cap0603", "100nF", 1020, 400, {"1": "VCC5V2", "2": "GND"}),
        P("R14", "res", "100k 1%", 110, 390, {"1": "VBAT_LOAD", "2": "EN5_2"}),
        P("C20", "cap0603", "100pF", 120, 310, {"1": "5V2", "2": "FB5_2"}),
        P("L2", "ind", "4.7uH", 270, 310, {"1": "SW5_2", "2": "5V2"}),
        P("C9", "cap1206", "22uF 10V", 420, 310, {"1": "5V2", "2": "GND"}),
        P("C10", "cap1206", "22uF 10V", 570, 310, {"1": "5V2", "2": "GND"}),
        P("C11", "cap0603", "100nF", 720, 310, {"1": "5V2", "2": "GND"}),
        P("R3", "res", "100k 1%", 870, 310, {"1": "5V2", "2": "FB5_2"}),
        P("R4", "res", "19.1k 1%", 1020, 310, {"1": "FB5_2", "2": "GND"}),
        P("FB4", "bead", "600R 0805", 120, 260, {"1": "5V2", "2": "5V2_OUT"}),
        P("J4", "xh2", "XH-2P 5V2", 270, 260, {"1": "5V2_OUT", "2": "GND"}),
        # --- INA219 + local 3.3V LDO (block 7) ---
        P("RSH", "shunt", "0.01R 3W", 120, 200, {"1": "VBAT_RAIL", "2": "VBAT_LOAD"}),
        P("U4", "ina219", "INA219BIDR", 270, 180,
          {"1": "GND", "2": "GND", "3": "INA_SDA", "4": "INA_SCL",
           "5": "3V3I", "6": "GND", "7": "VBAT_LOAD", "8": "VBAT_RAIL"}),
        P("U3", "me6211", "ME6211C33", 470, 200,
          {"1": "5V1", "2": "GND", "3": "5V1", "5": "3V3I"}),
        P("C22", "cap0603", "1uF 16V", 620, 200, {"1": "5V1", "2": "GND"}),
        P("C23", "cap0603", "1uF 16V", 770, 200, {"1": "3V3I", "2": "GND"}),
        P("C24", "cap0603", "100nF", 920, 200, {"1": "3V3I", "2": "GND"}),
        P("J7", "xh3", "XH-3P I2C", 1070, 200,
          {"1": "INA_SDA", "2": "INA_SCL", "3": "GND"}),
        # --- 指示与测试点 (block 8) ---
        P("D2", "led", "Green", 120, 100, {"1": "VBAT_LOAD", "2": "LED_VBAT"}),
        P("R7", "res", "2.2k 1%", 270, 100, {"1": "LED_VBAT", "2": "GND"}),
        P("D3", "led", "Green", 420, 100, {"1": "5V1", "2": "LED_5V1"}),
        P("R8", "res", "1k 1%", 570, 100, {"1": "LED_5V1", "2": "GND"}),
        P("D4", "led", "Green", 720, 100, {"1": "5V2", "2": "LED_5V2"}),
        P("R9", "res", "1k 1%", 870, 100, {"1": "LED_5V2", "2": "GND"}),
        P("TP1", "tp", "TP", 120, 50, {"1": "VBAT_LOAD"}),
        P("TP2", "tp", "TP", 270, 50, {"1": "5V1"}),
        P("TP3", "tp", "TP", 420, 50, {"1": "5V2"}),
        P("TP4", "tp", "TP", 570, 50, {"1": "GND"}),
    ],
    "boxes": [
        {"x": 60, "y": 790, "w": 470, "h": 70, "label": "电池输入"},
        {"x": 540, "y": 800, "w": 450, "h": 110, "label": "防反接与软启动 (AOD409)"},
        {"x": 60, "y": 680, "w": 540, "h": 60, "label": "VBAT 钳位与储能"},
        {"x": 620, "y": 690, "w": 480, "h": 80, "label": "电机轨开关与输出"},
        {"x": 60, "y": 580, "w": 1120, "h": 170, "label": "5V / 3A 降压 #1 → ESP32 板 (U1)"},
        {"x": 60, "y": 410, "w": 1120, "h": 160, "label": "5V / 3A 降压 #2 → STM32 底板 (U2)"},
        {"x": 60, "y": 225, "w": 1120, "h": 90, "label": "电流遥测 (INA219) + 本地 3.3V LDO"},
        {"x": 60, "y": 130, "w": 900, "h": 95, "label": "指示与测试点"},
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
    const L = pin.rotation === 180 ? 30 : 35;
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
await eda.sch_PrimitiveText.create(60, 830, P.title, 0, null, null, 18, true, false, false, 1);
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
