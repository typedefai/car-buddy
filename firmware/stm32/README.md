# STM32 firmware

STM32F103ZET6 (LQFP144, 512 KiB flash, 64 KiB RAM) motion controller.
The ESP32-S3 owns audio/UI/network; this MCU owns the real-time motor loop.

## Toolchain

- STM32CubeMX 6.18.1 (classic CubeMX; CubeMX2 does not support STM32F1)
- Arm GNU Toolchain 14.2.rel1 for macOS **x86_64**:
  `/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin`
- CMake 3.30+ and Ninja (the ESP-IDF/Qt copies work)
- OpenOCD (the ESP-IDF copy supports ST-Link and `stm32f1x.cfg`)
- STLINK-V3MINIE (T_VCC is an input; the board must be powered externally)

### Intel Mac zstd workaround

The Arm 14.2 macOS x86_64 toolchain links `cc1` against
`/usr/local/opt/zstd/lib/libzstd.1.dylib`. If that path is missing:

```bash
sudo mkdir -p /usr/local/opt/zstd/lib
sudo ln -sf /opt/local/lib/libzstd.1.dylib /usr/local/opt/zstd/lib/libzstd.1.dylib
```

(MacPorts' `zstd` provides the target file.) Without the symlink, build with
`DYLD_LIBRARY_PATH=/opt/local/lib`, which also pulls in MacPorts libiconv and
prints harmless dyld warnings.

## Build

```bash
export PATH="/Applications/ArmGNUToolchain/14.2.rel1/arm-none-eabi/bin:$PATH"
cd firmware/stm32
cmake --preset Debug
cmake --build build/Debug
```

## Flash

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/Debug/car-buddy-stm32.elf verify reset exit"
```

Target power must be present on T_VCC (3.3 V); the STLINK-V3MINIE does not
supply target power.

## Regenerating from CubeMX

The `.ioc` is the source of truth. Open `car-buddy-stm32.ioc` in STM32CubeMX,
change peripherals/clock/pins, and click **Generate Code**. CubeMX preserves
the `USER CODE` blocks in `Src/main.c` and the user-owned `CMakeLists.txt`
sections. Do not hand-edit CubeMX-generated blocks.

Current CubeMX configuration:

- HSE 8 MHz crystal -> PLL x9 -> 72 MHz
- SWD (PA13/PA14)
- USART1: PA9/PA10, 115200 8N1 (debug console)
- USART2: PA2/PA3, 115200 8N1 (ESP32 link)
- Toolchain: CMake, GCC
- Firmware package: STM32Cube FW_F1 V1.8.7

## Application layout

- `App/app.c` — link state machine, heartbeat, telemetry, setpoint clamping
- `App/motor.c` — TB6612 PWM/direction, encoder timers, skid-steer mixing,
  slew limiting, stall detection, IWDG
- `App/uart_link.c` — USART2 DMA RX ring buffer + DMA TX, IRQ handlers
- `../shared/protocol/` — versioned CRC frame shared with the ESP32/server

The STM32 sends `HEARTBEAT` every 50 ms and `TELEMETRY` every 500 ms. It stops
motion when no valid frame has arrived for 300 ms, and only leaves the safe
state after a `SETPOINT` with `CAR_SETPOINT_ENABLE`.

## Car control

The motor/encoder pin map and the full wiring table are in
[`../../hardware/stm32-car-wiring.md`](../../hardware/stm32-car-wiring.md).

- 4 driven wheels, skid steer: left pair = throttle + steering, right pair =
  throttle - steering.
- 4 quadrature encoders: TIM2/TIM3/TIM4/TIM8.
- TB6612 STBY is low at boot and on every stop/fault.
- IWDG is started in `Motor_Init` (~2 s) and refreshed from the main loop.
- Stall detection stops the motors if a side is driven hard but no encoder
  movement is seen for 1 s.
- `MOTOR_*_INVERT` and `ENCODER_*_SIGN` in `board_config.h` handle reversed
  wiring.

The `.ioc` currently covers clock, SWD, USART1, and USART2. Motor/encoder
timer/GPIO initialization lives in `App/motor.c` and should be migrated into
the `.ioc` once the pin map is frozen. When you add TIM1/encoders to the
`.ioc`, remove the manual `stm32f1xx_hal_tim*.c` sources from `CMakeLists.txt`
and let CubeMX copy/enable the TIM HAL files.

Set `BRINGUP_BLINK_ONLY` in `App/app.c` to `1` for the PC13 blink + USART1
`Hello, World!` bring-up test, or `0` for the car-control firmware.

## ESP32 console

The ESP32 starts a UART0 console at 115200:

```text
w/f : forward      x/b : reverse
a   : turn left    d   : turn right
s   : stop         e   : e-stop latch
c   : clear e-stop +/- : speed step
h/? : help
```

Every command is sent to the STM32 as a `SETPOINT` at 50 Hz. If telemetry
stops for 500 ms, the ESP32 clears its command and the STM32 stops on its own
300 ms timeout.

## Not implemented yet

- Steering servo (current design is skid steer)
- Battery voltage ADC
- IMU / range sensors
- Hardware motor-power cut (required on the real chassis)

## ESP32 wiring

```text
ESP32-S3 GPIO14 (UART1 TX)  -> STM32 PA3 (USART2_RX)
ESP32-S3 GPIO21 (UART1 RX) <- STM32 PA2 (USART2_TX)
ESP32-S3 GND               -- STM32 GND
```

Both sides are 3.3 V; do not connect 5 V to the UART pins.
