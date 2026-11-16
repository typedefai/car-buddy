# STM32 car wiring

STM32F103ZET6 + two TB6612FNG drivers, 4 driven wheels, skid steer.
All logic is 3.3 V. The motor supply (VM) is separate from logic.

## Power

```text
Battery + --- fuse --- hardware power switch ---+--- TB6612 #1 VM
                                                +--- TB6612 #2 VM

Battery - ---+--- TB6612 #1 GND
             +--- TB6612 #2 GND
             +--- STM32 GND
             +--- ESP32 GND
```

- Add **100 uF electrolytic + 100 nF ceramic** across each TB6612 VM/GND.
- Add **100 nF** across each TB6612 VCC/GND.
- TB6612 **VCC = 3.3 V**, not 5 V.
- Power the STM32 from its 5 V/USB input. Do not power motors or the ESP32
  from the STM32 3.3 V regulator.
- The hardware power switch/fuse is the independent motor-power cut.
- **Common ground is mandatory** between battery, both TB6612 boards, STM32,
  and ESP32.

## TB6612 #1 — left side

| TB6612 pin | Connect to | Notes |
|---|---|---|
| VM | Battery + | motor supply, 3–15 V |
| VCC | 3.3 V | logic supply |
| GND | Common GND | |
| PWMA | **PE9** | TIM1_CH1 |
| AIN1 | **PD4** | |
| AIN2 | **PD5** | |
| AO1 / AO2 | Left front motor | swap to reverse direction |
| PWMB | **PE11** | TIM1_CH2 |
| BIN1 | **PD6** | |
| BIN2 | **PD7** | |
| BO1 / BO2 | Left rear motor | swap to reverse direction |
| STBY | **PD2** | high = enabled; add 10 kΩ to GND |

## TB6612 #2 — right side

| TB6612 pin | Connect to | Notes |
|---|---|---|
| VM | Battery + | |
| VCC | 3.3 V | |
| GND | Common GND | |
| PWMA | **PE13** | TIM1_CH3 |
| AIN1 | **PD8** | |
| AIN2 | **PD9** | |
| AO1 / AO2 | Right front motor | |
| PWMB | **PE14** | TIM1_CH4 |
| BIN1 | **PD10** | |
| BIN2 | **PD11** | |
| BO1 / BO2 | Right rear motor | |
| STBY | **PD3** | high = enabled; add 10 kΩ to GND |

The firmware uses TIM1 **full remap** so the four PWM channels are on
PE9/PE11/PE13/PE14. `AFIO_MAPR` is written once with SWD enabled, JTAG
disabled, to avoid losing the debug link.

## Encoders

| Encoder | A | B | Timer |
|---|---|---|---|
| Left front | **PA0** | **PA1** | TIM2 |
| Left rear | **PA6** | **PA7** | TIM3 |
| Right front | **PB6** | **PB7** | TIM4 |
| Right rear | **PC6** | **PC7** | TIM8 |

- Encoder VCC: 3.3 V (preferred) or 5 V per encoder rating; GND common.
- If the encoder output is open-collector, add **10 kΩ pull-ups to 3.3 V**.
  The firmware also enables internal pull-ups, but external ones are better.
- Keep encoder wires away from motor wires; use twisted pairs if possible.

## ESP32 link

| ESP32-S3 | STM32 | Direction |
|---|---|---|
| GPIO14 (UART1 TX) | **PA3** (USART2_RX) | ESP32 → STM32 |
| GPIO21 (UART1 RX) | **PA2** (USART2_TX) | STM32 → ESP32 |
| GND | GND | mandatory |

115200 8N1, 3.3 V logic, no level shifter.

## Debug

| Signal | Pin |
|---|---|
| USART1 TX | PA9 |
| USART1 RX | PA10 |
| SWDIO | PA13 |
| SWCLK | PA14 |

USART1 is 115200 8N1. The STLINK-V3MINIE T_VCC pin is an input; power the
board externally.

## Tuning

In `firmware/stm32/App/board_config.h`:

- `MOTOR_LF_INVERT` … `MOTOR_RR_INVERT`: flip a motor that spins backwards.
- `ENCODER_LF_SIGN` … `ENCODER_RR_SIGN`: flip an encoder counting backwards.
- `ENCODER_COUNTS_PER_REV`: encoder edges per wheel revolution.
- `WHEEL_CIRCUMFERENCE_MM`: wheel circumference.
- `CAR_ENCODER_COUNT`: set to 2 if only one encoder per side is fitted.

## First power-up

1. Wheels off the ground.
2. Verify 3.3 V on TB6612 VCC and the correct battery voltage on VM.
3. Flash the STM32; TB6612 STBY is low at boot, so motors are disabled.
4. Start the ESP32 and open its UART0 console (115200).
5. Press `w` — wheels should turn slowly; `s` stops.
6. Press `e` — immediate e-stop; `c` clears it.
7. Unplug the ESP32↔STM32 UART — the STM32 must stop within 300 ms.
