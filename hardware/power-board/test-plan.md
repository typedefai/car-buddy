# Power board test plan

Do these tests in order. Use a current-limited bench supply; do not start with
a battery.

## 1. Visual and continuity

- Inspect for solder bridges, especially around U1/U2, Q1, F1, and connectors.
- Measure resistance:
  - VBAT to GND: should be high (not a short).
  - 5 V to GND: should be high.
  - 3.3 V to GND: should be high.
- Confirm D1, D5, D2-D4 orientation.
- Confirm F1 rating and SW1 OFF.

## 2. First power-up, no load

1. Set the bench supply to **7.4 V, 100 mA**.
2. Connect to J1 (respect polarity). Turn SW1 ON.
3. Check:
   - VBAT ≈ 7.4 V minus Q1 drop (a few mV).
   - 5 V = 4.9–5.1 V.
   - 3.3 V = 3.2–3.4 V.
   - Supply current: typically <30 mA with no load.
4. If any rail is wrong, power off and inspect before continuing.

## 3. Load tests

Use electronic loads or power resistors.

| Test | Rail | Load | Duration | Pass |
|---|---|---|---|---|
| 1 | 5 V | 1 A | 5 min | 4.9–5.1 V, no shutdown |
| 2 | 5 V | 2 A | 5 min | 4.8–5.1 V, U1 < 100 °C |
| 3 | 3.3 V | 1 A | 5 min | 3.2–3.4 V |
| 4 | 3.3 V | 2 A | 5 min | 3.2–3.4 V, U2 < 100 °C |
| 5 | 5 V + 3.3 V | 1 A each | 10 min | both rails stable |

Measure ripple with a short ground spring:

- 5 V: < 50 mVpp
- 3.3 V: < 50 mVpp

If U1/U2 exceed 100 °C at 2 A, reduce the continuous rating to 1.5 A or improve
copper/thermal vias.

## 4. Input range

- Set the supply to 6.0 V and repeat the 1 A load tests; rails must stay in
  regulation.
- Set the supply to 8.4 V and repeat; check Q1 gate-source voltage is below
  12 V and U1/U2 input is stable.
- Check the VBAT sense voltage at J7:
  - 6.0 V -> ~1.49 V
  - 7.4 V -> ~1.84 V
  - 8.4 V -> ~2.08 V

## 5. Reverse polarity

Use a current-limited supply (7.4 V, 100 mA) and **no battery**:

- Reverse the polarity at J1.
- Q1 should block; supply current should be near zero; no rail should come up.
- Correct polarity and confirm normal operation.

## 6. Protection

- Confirm F1 opens on a deliberate short at the VBAT output (use a fused test
  cable and eye protection; do not do this with a battery).
- If a 2S BMS is available, verify over-discharge/over-current behavior
  separately with the battery pack.

## 7. System integration

1. Connect the ESP32 board to J3 (5 V) and J7 (sense) with the correct
   polarity. Confirm the ESP32 boots and the 5 V rail stays above 4.8 V.
2. Connect the STM32 board to J3/J4 and the UART link to the ESP32. Confirm
   both boot.
3. Connect the TB6612FNG VM inputs to J5/J6. Wheels off the ground.
4. Run the ESP32 console `w`/`s`/`e` tests; confirm the STM32 stops on link
   loss and e-stop.
5. Measure VBAT sag during motor acceleration; stay above the BMS cutoff and
   below the fuse rating.

## 8. Acceptance

- All rails within tolerance at 6.0–8.4 V input.
- 5 V/3.3 V hold 2 A continuous for 5 minutes with U1/U2 below 100 °C.
- Reverse polarity blocks.
- Fuse/BMS protect a short.
- No brownout on the ESP32/STM32 during motor starts.
