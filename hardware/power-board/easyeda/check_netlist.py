#!/usr/bin/env python3
"""Compare the EasyEDA schematic netlist with hardware/power-board/netlist.csv."""
import csv
import json
import subprocess
import sys
from collections import defaultdict

JS = "await eda.dmt_EditorControl.openDocument('1c5eea75209bcc4d');\nawait new Promise(r => setTimeout(r, 800));\nawait eda.sch_Document.save();\nawait new Promise(r => setTimeout(r, 2500));\nreturn await eda.sch_Netlist.getNetlist();"
res = subprocess.run([sys.executable, "eda.py"], input=JS, capture_output=True, text=True, timeout=120)
data = json.loads(res.stdout)
raw = data.get("result")
if not raw:
    print("no netlist", data)
    sys.exit(1)
nl = json.loads(raw) if isinstance(raw, str) else raw

actual = defaultdict(set)  # net -> {(ref,pin)}
refs = {}
for cid, c in nl["components"].items():
    props = c["props"]
    ref = props.get("Designator")
    refs[ref] = props.get("DeviceName")
    for pin, info in c["pinInfoMap"].items():
        net = info.get("net") or ""
        if net:
            actual[net].add((ref, pin))

# expected from csv
expected = defaultdict(set)
with open("../netlist.csv") as f:
    for row in csv.DictReader(f):
        expected[row["Net"]].add((row["Ref"], row["Pin"]))

# F1 and FH1 are merged on the schematic: F1 is the 5x20 holder
merged = defaultdict(set)
for net, pins in expected.items():
    s = set()
    for ref, pin in pins:
        if ref == "FH1":
            continue
        s.add((ref, pin))
    merged[net] |= s

print(f"components: {len(nl['components'])}, designators: {sorted(r for r in refs if r)}")
problems = 0
for net in sorted(set(actual) | set(merged)):
    a = actual.get(net, set())
    e = merged.get(net, set())
    if a != e:
        problems += 1
        print(f"\nNET {net}:")
        if a - e:
            print("  extra in schematic:", sorted(a - e))
        if e - a:
            print("  missing in schematic:", sorted(e - a))
print(f"\nnets: {len(actual)} actual / {len(merged)} expected; problems: {problems}")
