# EasyEDA project status

The power board design source is in the parent folder. The EasyEDA Pro project
is **not created yet** because the API Gateway extension is not connected:

```json
{"service":"easyeda-bridge","status":"ok","edaConnected":false,"edaWindowCount":0}
```

EasyEDA Pro (嘉立创EDA专业版) is running, but it does not have the
`run-api-gateway.eext` extension installed.

## Enable the bridge

1. The extension file was downloaded to:

   ```text
   ~/Downloads/run-api-gateway.eext
   ```

   If it is missing, download it again:

   ```text
   https://image.lceda.cn/extensions/files/551b05983a2d4509814859a360bda571.eext
   ```

2. In EasyEDA Pro:
   - Open **设置 / Settings -> 扩展 / Extensions -> 扩展管理器 / Extension Manager**.
   - Choose **导入扩展 / Import Extension**.
   - Select `~/Downloads/run-api-gateway.eext`.
   - Enable **允许外部交互 / Allow external interaction**.
   - Enable **显示在顶部菜单 / Show in top menu**.
3. An **API Gateway** menu should appear at the top of EasyEDA.
4. Verify from a terminal:

   ```bash
   curl -s http://127.0.0.1:49620/health
   ```

   `edaConnected` should become `true`, and `edaWindowCount` should be `1`.

The bridge server is already running on port 49620. If it is not:

```bash
cd ~/.config/opencode/skills/easyeda-api
node scripts/bridge-server.mjs &
```

## What will be created

When the bridge is connected, ask the agent:

```text
EasyEDA 已连接，请创建 car-buddy-power 工程：
- 原理图按 hardware/power-board/schematic.md 和 netlist.csv
- PCB 按 hardware/power-board/pcb-layout.md
- BOM 按 hardware/power-board/BOM.csv
- 完成后把工程源文件保存到 hardware/power-board/easyeda/
```

Planned EasyEDA project:

- Project: `car-buddy-power`
- Schematic page: power input, protection, U1 5 V, U2 3.3 V, sense, LEDs,
  connectors.
- PCB: 70 x 50 mm, 2-layer, 2 oz, placement per `pcb-layout.md`.
- Library parts: search by MPN/LCSC, assign footprints, then run DRC.
- Export: EPRJ3 folder if the client supports offline local projects;
  otherwise save the `.epro` project plus the document source (`.esch`/`.epcb`
  or `getDocumentSource` output) in this folder so Git can track it.

## Why not generated offline

A complete EasyEDA schematic needs library symbol/device UUIDs and footprints
from the EasyEDA library. The API can search and place them, but only while
connected to the running EasyEDA client. The design package in the parent
folder is the source of truth until the bridge is connected.
