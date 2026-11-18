# Hardware

Three-board split for the car:

| Board | Purpose | Status |
|---|---|---|
| `power-board/` | 2S Li-ion input, VBAT direct output + 5 V rail (v2) | v2 spec decided, see `DECISIONS.md` D10; EasyEDA revision pending |
| `esp32-board/` | ESP32-S3 (WROOM-1-N16R8) integrated: OLED, ES7210/ES8311 codec, mic, speaker, RGB LED, UART to STM32 | not started |
| `stm32-board/` | carrier for 立创·天空星 STM32F407VGT6 core: four AT8236 motor drivers, four encoders, UART to ESP32, expansion headers | core bought; carrier pin map + PCB not started |

## Power tree

```text
2S Li-ion pack (6.0-8.4 V, with 2S BMS)
  XT30
   |
  fuse 7.5 A
   |
  power switch
   |
  reverse-polarity P-MOSFET
   |
  VBAT ------------------------------------------------------+
   |            |             |             |                |
  XT30        terminal      U1 buck       U2 buck        divider
  (motors)    (aux)         5 V / 3 A     3.3 V / 3 A    VBAT sense
                               |             |
                            5 V out       3.3 V out
```

- **VBAT** is the direct battery rail for the TB6612FNG motor drivers.
- **5 V** powers the ESP32 board, speaker amplifier, and STM32 board.
- **3.3 V** powers logic/sensors that are not already fed by a board's own
  regulator.
- The 2S BMS is part of the battery pack. Never connect unprotected Li-ion
  cells to this board.
- A hardware motor-power cut (switch/fuse) is required and is independent of
  firmware.

## Interconnect convention

Use separate cables per rail rather than one combined high-current connector:

| Rail | Connector | Notes |
|---|---|---|
| VBAT | XT30 | motors/TB6612 VM, 10 A class |
| 5 V | JST-XH 2-pin | 3 A class |
| 3.3 V | JST-XH 2-pin | 3 A class |
| VBAT sense | JST-XH 3-pin | sense, GND, 3.3 V reference |

ESP32 <-> STM32 remains a 3-wire UART link (TX, RX, GND) at 3.3 V.

## Board designs

- [Hardware decisions](DECISIONS.md)
- [Power board design](power-board/README.md)
- [Power board schematic](power-board/schematic.md)
- [Power board BOM](power-board/BOM.csv)
- [Power board netlist](power-board/netlist.csv)
- [Power board PCB layout](power-board/pcb-layout.md)
- [Power board test plan](power-board/test-plan.md)
- [STM32 car wiring](stm32-car-wiring.md)
