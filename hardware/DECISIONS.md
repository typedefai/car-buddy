# Hardware decisions

Log of hardware architecture decisions for the car. Statuses: **decided**,
**proposed**, **open**.

_Updated 2026-11-19._

## Project goals (agreed)

- Learning/playing project; every board should be reusable in future projects.
- Follow the classic flight-controller learning path (传统飞控路线): pick one
  MCU family that future boards keep using ("one-and-done", no chip swaps).
- Leave expansion headroom: screen (FSMC parallel LCD), Ethernet (RMII),
  camera (DCMI).
- PCBs from JLCPCB, chip hand-soldered, parts from LCSC — no gray-market
  supply hassle.

## Decided

### D1 — Three-board split

Power board / ESP32 board (screen + speaker + mic) / STM32 board (motor control).

Rationale: mirrors the firmware tier boundaries, enables independent bring-up
(bottom-up: motor loop -> UART -> WebSocket -> voice), and keeps each board
reusable on its own.

### D2 — Battery: 2S Li-ion 26650, reusing idle cells (decided 2026-11-19)

2 x 3.7 V Li-ion 26650 cells (already owned), 6.0-8.4 V, placed separately
in the chassis in single-cell holders. Charging is external only - the
cells are removed and charged one by one in a slot charger; there is no
on-board charger. The motor rail runs directly off VBAT (AT8236 VM accepts
5.5-36 V).

Safety architecture (high-capacity cells, long wire runs, vibration):

- Cell health check before reuse: resting voltage >= ~2.5 V per cell, no
  swelling/dents/leaks; a capacity test in a slot charger is preferred;
  install both cells fully charged (4.2 V) so they start balanced. The two
  cells should be the same model/age - a mismatched pair over-discharges
  the weaker one (the BMS protects it, but runtime follows the weak cell).
  If they differ too much, buy one new matched pair.
- 2S BMS = a bought off-the-shelf 保护板 (~5-10 CNY, 2S Li-ion, >= 15 A
  continuous, with balance), NOT designed by us. It mounts near the
  battery boxes between the cells and the XT30 output and gives
  firmware-independent over-discharge (2.5 V/cell), over-current, and
  short-circuit protection. Balance is unused for external per-cell
  charging but harmless. Wiring: B+/B- carry the full load current
  (16 AWG), BM (middle tap, cell1- = cell2+) is a thin balance wire.
BMS wiring (the only "design" work on the battery side):

```text
cell box 1            cell box 2
  [26650]               [26650]
   + --------- B+ ------------> BMS     16 AWG, full load current
   - --+------- BM ------------> BMS     thin wire, balance current only
       |        (middle tap: cell1- = cell2+)
       +------ series link ----> cell2+  16 AWG, full load current
   cell2- ----------- B- ------> BMS     16 AWG, full load current

BMS P+ ---- box fuse 7.5A ----> XT30 -> power board
BMS P- -----------------------> XT30 -> power board
```

BMS buying checklist: 2S (not 1S/3S), Li-ion 3.7 V (not LiFePO4 3.2 V),
continuous current >= 15 A.

- Pack-side fuse: 7.5 A slow-blow blade holder at the battery box, before
  the cable run to the power board (the board fuse is the second line).
  16 AWG silicone wire, strain-relieved, heatshrink on every junction,
  keyed XT30 connectors.
- Firmware (second line of defense): INA219 pack voltage -> warn at 6.4 V,
  stop motors at 6.0 V.
- Mechanics: holders screwed down + velcro strap over the cells; label the
  series junction (cell1- = cell2+).

### D3 — Motors: TT 3-6 V, 1:90, x4

Skid steer, 4 driven wheels. Consequence: a fresh 2S pack (8.4 V) exceeds the
6 V motor rating, so firmware must clamp PWM (see D7). Expected top speed
~0.3-0.4 m/s (1:90 at 6 V ~= 110-150 RPM with ~66 mm wheels) — a torquey
walking pace, well matched to voice-control latency.

### D4 — STM32: STM32F407VGT6 (LQFP-100) — decided 2026-11-19

Replaces the earlier STM32F103ZET6 idea.

- Cortex-M4F @ 168 MHz, 1 MB flash, 192 KB RAM, ~82 GPIO.
- Timer fit: TIM1 4-channel PWM for the motors; TIM2/3/4/8 as four hardware
  quadrature encoder decoders. (TIM5's encoder pins on the 100-pin package
  collide with TIM2's, hence TIM8 for the 4th encoder.)
- Expansion buses present: FSMC (16-bit data, one NE chip-select), RMII
  Ethernet (LAN8720 module), DCMI camera. On the 100-pin package the rule is
  "two of three at once" (FSMC and DCMI share pins); the 144-pin ZGT6 would
  run all three simultaneously but is much harder to hand-solder.
- Classic FC lineage (Betaflight/ArduPilot F4 family) with abundant
  正点原子/野火/江科大 tutorial material.
- ~30 CNY at LCSC; hand-solderable LQFP-100 (drag soldering). Buy from LCSC,
  not Taobao — STM32 gray-market counterfeits are common.
- Pin map not yet done; at design time keep motor/encoder pins off the FSMC
  pins (PD/PE block, e.g. PD4/PD5 = NOE/NWE) and DCMI pins (PB6/7, PC6/7) so
  the buses stay available on headers.

### D5 — Motor driver: 4x AT8236 (decided 2026-11-19, replaces 2x TB6612FNG)

One single-channel H-bridge per motor (杭州中科微, ESOP-8).

- 3 A continuous / 6 A peak per channel: TT stall (~1.5-2.5 A at 6 V
  effective) sits entirely inside the rating, unlike TB6612's 1.2 A
  continuous. 200 mOhm RDS(on) keeps stall heating low (~0.8 W at 2 A).
- VREF pin sets a hardware peak-current limit (~2.5 A via resistor divider):
  a physical stall clamp that works even if firmware misbehaves.
- Integrated OCP / short-circuit / UVLO / over-temperature protection;
  the AT8236N variant adds a fault output pin to the STM32.
- PH/EN-style control: 2 GPIO per motor (1 PWM + 1 direction), 8 pins total
  vs 14 for TB6612; TIM1's four PWM channels drive the four PWM inputs.
- 5.5-36 V input: fine for 2S down to the 6.0 V cutoff; no 1S reuse (that
  was TB6612's edge). ~4-6 CNY each at LCSC.
- ESOP-8 exposed pad needs paste/hot-air or a back-side via solder fill —
  one extra hand-soldering step vs TB6612.

Alternatives considered: 2x TB6612FNG (1.2 A continuous rating sits below TT
stall current; kept as fallback if tutorial compatibility is ever preferred),
4x DRV8874 (per-motor IPROPI current sensing, pricier, harder package).

### D6 — Hardware safety

- A hardware motor-power cut independent of firmware (battery switch/fuse;
  see proposed P3 for a dedicated motor-rail switch).
- Common ground across all boards and rails.
- Never connect unprotected Li-ion cells to the power board.

### D7 — Firmware safety & control (agreed in discussion)

- STM32 clamps PWM duty against the INA219 bus voltage (read over I2C at
  ~100 ms cadence) so the effective motor voltage stays <= ~6 V across the
  discharge curve; fall back to a fixed 71 % duty cap if the I2C link is
  lost.
- Encoder-based stall detection: PWM high + no counts for ~200-500 ms -> back
  the duty off.
- Hardware QEI decoding on the four encoder timers (no ISR pulse counting).
- Acceleration ramp on setpoints (protects the plastic 1:90 gearboxes).

### D8 — Core + carrier architecture, final form (decided 2026-11-19)

STM32 side is core + carrier; the ESP32 side integrates the module directly.

- **STM32 core = bought 立创·天空星 STM32F407VGT6 开发板-高配版** (~50 CNY,
  LCKFB-LSPI-SkyStar-STM32F407VGT6-PRO): 168 MHz, 1 MB flash, W25Q128 SPI
  flash, TF slot, 8 MHz HSE + 32.768 kHz LSE, USB-C (power + DFU), two
  2x20 headers breaking out 70 IO, 2x5 SWD/USART1 debug header. Open-source
  design (GPL 3.0) with full hardware docs; the STM32 carrier copies its
  pinout. Headers ship unsoldered (solder before plugging in).
- **ESP32 board = WROOM-1-N16R8 module soldered directly.** The module is
  the reusable unit; there is no separate ESP32 core board.
- **STM32 carrier (to design)**: 4x AT8236, encoder connectors, UART to
  ESP32, I2C, VBAT in, 5 V in, two 2x20 core sockets, FSMC/RMII/DCMI
  break-out headers.
- Power rule: carriers receive VBAT/5 V only; motor current never flows
  through the core-carrier connector.
- Before carrier layout, verify required pins against the 天空星 pinout:
  TIM1 PWM PE9/11/13/14; encoders PA0/1, PA6/7 (fallback PB4/5), PB6/7,
  PC6/7; UART to ESP32; I2C. Known board-resident pins: USB PA9/11/12,
  SDIO PC8-12 + PD2, LSE PC14/15, SWD PA13/14 + PB3; the SPI flash may
  take PA5/6/7 (TIM3_CH1/2 alternates are PB4/PB5).

### D9 — Debug & flashing tooling (decided 2026-11-19)

- Debugger: **ST-Link V3 MINIE** (already owned) via the 天空星 2x5 SWD
  header (SWDIO/SWCLK/SWO + USART1 + power). Needs an STDC14-to-10-pin
  adapter. Flash/debug with STM32CubeProgrammer or OpenOCD (both support
  the V3 MINIE).
- USART1 doubles as console through the ST-Link VCP; the business UART to
  the ESP32 stays separate.
- Fallback: 天空星 WebUSB DFU browser flashing (no programmer needed).

### D10 — Power board v2 + board-level power (decided 2026-11-19)

The old `power-board/` design is revised in place into a VBAT + 5 V board.

Output channels (4) + input (1):

```text
battery box (2x26650 + BMS + box fuse 7.5A)
   |  XT30
   v
+-----------------------------------------------------+
|                  POWER BOARD v2                      |
|                                                      |
|  IN -- board fuse -- main switch -- AOD409 --        |
|        (7.5A blade)  (rocker)      rev-polarity      |
|                                    (+gate RC soft-   |
|                                     start)           |
|                                                      |
|                        VBAT bus (SMBJ12A + 470uF     |
|                                  + 0.1uF + 1uF)      |
|   VBAT --+-- motor switch (rocker) --> [1] XT30      |
|          |                               motors      |
|          |                               ~10A peak   |
|          +-- INA219 (0.01 ohm) ------> [4] JST-XH    |
|          |    + local 3.3V LDO           3-pin       |
|          |    (ME6211 from 5V)           SDA/SCL/GND |
|          |                                           |
|          +-- ferrite -- MP2315 #1 ---> [2] JST-XH    |
|          |                  5V/3A         5V ESP32   |
|          +-- ferrite -- MP2315 #2 ---> [3] JST-XH    |
|                             5V/3A         5V STM32   |
+-----------------------------------------------------+
```

| # | Channel | Connector | To | Spec | Cuttable |
|---|---|---|---|---|---|
| 1 | VBAT motor rail | XT30 | STM32 carrier -> 4x AT8236 -> motors | 6.0-8.4 V, ~10 A peak | motor rocker |
| 2 | 5 V #1 | JST-XH 2-pin | ESP32 board (module + amp + OLED) | 5 V/3 A, ~1.3 A peak | main rocker |
| 3 | 5 V #2 | JST-XH 2-pin | STM32 carrier (core + encoders) | 5 V/3 A, ~0.3 A | main rocker |
| 4 | I2C telemetry | JST-XH 3-pin | STM32 carrier (INA219 data) | SDA/SCL/GND | - |

Test points (VBAT/5 V/GND) are not counted as channels.

- Rails: VBAT direct (motors) + two independent MP2315 5 V / 3 A, one per
  output (review change: the ESP32 board's amp is a switching noise source;
  a dedicated buck per board keeps it off the STM32 logic supply and gives
  each board its own fault domain). Each output keeps a ferrite bead +
  bulk cap.
- INA219 on the VBAT rail (0.01 ohm shunt, ~10 A range), powered by a
  local ME6211 3.3 V LDO from 5 V on the power board (review change: no
  power backfeed from the carrier and the power board is bench-testable
  standalone). Data goes out on a 3-pin JST-XH (SDA/SCL/GND); I2C
  pull-ups live on the STM32 carrier. Replaces the analog VBAT divider.
- Switches: main rocker + motor rocker (VBAT -> motor XT30 only), both
  rated >= 15 A DC (automotive/marine class - AC-rated rockers are not
  enough for a 10 A inductive DC load). Fuses: blade type, 7.5 A
  slow-blow at the battery box (D2) and on the board; value to be
  confirmed by stall measurement.
- Reverse-polarity FET: AOD409 (TO-252, ~8 mOhm, -40 V) instead of SI4435
  (review change: SI4435 is ~20 mOhm at 2S gate drive = 2 W at 10 A
  stall, too hot for SOIC-8; AOD409 = ~1 W, and ~0.45 W at the 7.5 A fuse
  limit). Gate RC soft-start: 100 nF across gate-source with the 100 k
  pulldown (~10 ms turn-on) limits inrush into the bulk caps; normal
  power-on is via the rocker anyway.
- TVS: keep SMBJ12A - its 12 V standoff is already above the 8.4 V full
  charge, so it cannot falsely trigger (the "10-13 V" review concern is
  satisfied by this part; the 13.3 V figure is its breakdown voltage, not
  its working voltage).
- Kept from the old design: 470 uF bulk in parallel with 0.1 uF + 1 uF
  ceramics. Buck inputs get a ferrite bead + 10 uF MLCC against motor PWM
  spikes on VBAT. VBAT traces >= 3 mm + pours from the start.
- Grounding: no galvanic isolation anywhere (low-voltage battery system,
  common ground is mandatory). Motor current has its own rail and return
  path; power and logic ground meet at a single star point near the
  battery input.
- Per-board 3.3 V: 天空星 regulates its own from 5 V (onboard LDO); the
  STM32 carrier has a small LDO (ME6211 class) from 5 V for encoders; the
  ESP32 board has a 1.5-2 A buck (SY8088 class) from 5 V for the module +
  OLED + logic.
- ESP32 board details: the speaker amp (NS4150B) runs directly on 5 V with
  a ferrite bead + 100 uF; the SSD1306 OLED (~20 mA) runs on the local
  3.3 V with 100 nF + 10 uF at the connector - it does not need a
  dedicated rail, but a tiny ME6211 from 5 V is cheap insurance if
  desired. (If an OLED module is fed 5 V directly, verify its I2C pull-ups
  are not to 5 V - ESP32-S3 pins are not 5 V tolerant.)

## Open questions

- Bench measurements: real stall current per motor at 6 V and 8.4 V (fixes
  the 7.5 A fuse value and the AT8236 VREF limit), and actual speed to
  confirm the D3 expectations.
- 26650 cell health check and pair matching before installation (D2).
