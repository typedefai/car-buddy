# AGENTS.md

Smart-car toy: 4WD chassis, three tiers. STM32 owns real-time motion, ESP32-S3 is the UI/voice/network gateway, and a Linux SBC (ROCK 5B or RK3566) runs the controller/conversation server. Dev machines are macOS (arm64) and Linux; the hardware is custom.

Agreed target layout — scaffold only what you need:

```
firmware/stm32/   STM32CubeMX project (.ioc, Src/, Inc/, App/, Drivers/)
firmware/esp32/   ESP-IDF project
server/           plain C++ controller/conversation server
shared/           wire protocol + config, single source of truth
hardware/         schematics, PCB, pinmap, power/bring-up notes
```

`../esp32-agent` is the prior ESP32-S3 firmware (ESP-IDF 6.1, CMake project `walle`); read its `AGENTS.md` and port from it instead of rewriting. Treat that repo as read-only.

## Hardware designs

- Power board: `hardware/power-board/` — 2S Li-ion input, VBAT direct output, 5 V/3 A and 3.3 V/3 A MP2315 rails. Schematic, BOM, netlist, layout and test plan are in that folder.
- STM32/TB6612/encoder wiring: `hardware/stm32-car-wiring.md`.
- EasyEDA Pro work uses the installed `easyeda-api` skill and the local bridge on ports 49620–49629. The extension is `run-api-gateway.eext`; see `hardware/power-board/easyeda/README.md`.

## Tier boundaries (do not blur)

- STM32: motor PWM, encoders, steering servo, IMU/range sensors, fixed-period control loop. No WiFi, audio, display, or cloud calls.
- ESP32: display, mic/speaker, wake word, audio streaming, WebSocket client. Never drives motors; forwards validated setpoints over UART.
- Server: ASR/LLM/TTS orchestration, telemetry, web UI. Talks only to the ESP32. Plain C++, no ROS in core logic; keep ROS behind an adapter if added later.
- Links: ESP32 <-> STM32 over UART; ESP32 <-> server over WiFi/WebSocket. STM32 has no network stack; Linux never commands motors directly.

## Safety (non-negotiable)

- STM32 stops all motors on UART heartbeat timeout, bad CRC, and until the first valid command after boot; enable the watchdog.
- STM32 clamps every motor/servo setpoint. Never trust values from ESP32 or the server.
- ESP32 stops the car when the WebSocket/server link drops. Local e-stop always wins over any remote command.
- Hardware must provide a motor-power cut independent of firmware.
- No heap allocation, blocking calls, or `printf` in STM32 ISRs/control loop; parse UART outside the ISR (DMA + ring buffer).
- ESP32 runs link, audio, and UI in separate FreeRTOS tasks; audio must not starve the UART/WebSocket link.
- Cloud/LLM credentials live only in server env/config. Never in firmware, commits, or logs.

## Protocol (`shared/`)

- One versioned, CRC-protected definition for UART frames and WebSocket messages, compiled by both firmware and server from `shared/`. Do not hand-roll a second parser.
- Use SI units and monotonic millisecond timestamps in every message. Reject version mismatches explicitly.
- A wire-format change updates `shared/`, both firmware ends, and the server in the same change.
- Keep `shared/` codecs pure C/C++ with no HAL/IDF/OS dependencies so host tests can run without hardware.

## Commands

ESP32 (ESP-IDF 6.1, target `esp32s3`):

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh   # this machine; otherwise source $IDF_PATH/export.sh
idf.py set-target esp32s3                        # once per checkout; sdkconfig is generated
idf.py build
idf.py -p /dev/cu.usbmodemXXXX -b 115200 flash monitor
idf.py fullclean
```

- Track `sdkconfig.defaults`, `dependencies.lock`, and `partitions.csv`; `sdkconfig`, `build/`, and `managed_components/` are generated. After changing `partitions.csv`, delete `sdkconfig` and re-run `set-target`.
- The board reports **16 MB flash / 8 MB embedded PSRAM**. `sdkconfig.defaults` enables octal PSRAM and 240 MHz, and `partitions.csv` is `4 MB factory + 6 MB ESP-SR model + ~5.9 MB SPIFFS`; keep those in sync.
- **Flash over the board's COM/UART USB port.** The native USB-JTAG port connects and reads chip info but esptool failed while uploading its stub / writing flash (`0107 Checksum error`, `0105 invalid message`) on this board.
- Sources are listed explicitly in `main/CMakeLists.txt`; add new `.cpp` files there.
- OLED is a 128x64 SSD1306 at I2C `0x3C` on SDA 5 / SCL 4, running at 400 kHz. `OledDisplay` uses `esp_lcd` + LVGL 9.5 with an I1 full-refresh buffer; `sdkconfig.defaults` disables LVGL examples and enables the mono theme.
- Audio is an **ES7210 + ES8311 + NS4150B** codec module on I2C 12/11 and full-duplex I2S0: MCLK 10, BCLK 9, WS 17, DOUT 18, DIN 3, PA EN 13. Module silkscreen DI/DO follows the ESP32's DIN/DOUT naming: DI (pin 10) is mic data into GPIO3, DO (pin 8) is playback data out of GPIO18. **Never cross DI/DO**: I2C and clocks still look fine, but the speaker stays silent and the mic reads ~0 (measured DI ≈0.9 V, DO ≈1.2 V when crossed). ES7210 is at 8-bit address `0x82` (7-bit `0x41`); ES8311 at `0x30`. `AudioCodec` uses `esp_codec_dev` at 16 kHz/16-bit: ES7210 runs 4-slot TDM (MIC1/MIC2 voice, MIC3 AEC reference, MIC4 unused) and ES8311 runs stereo output. PGA 30 dB, output volume 80. The old INMP441/MAX98357A code is removed.
- STM32 link: UART1 on GPIO14 (TX) / GPIO21 (RX), 115200, using the versioned frame in `shared/protocol`. A UART0 bench console accepts `w/x/a/d/s/e/c/+/-`; it sends `SETPOINT` at 50 Hz and stops on telemetry loss.
- Wake word uses ESP-SR `espressif/esp-sr ^2.6.2` with `wn9_xiaoaitongxue` (`CONFIG_SR_WN_WN9_XIAOAITONGXUE`, 小爱同学), threshold 0.4. On detection: `Yes?` -> ack tone -> 3 s record -> `Playing...` -> `Say Xiao Ai`. **Do not call WakeNet `clean()` after detection** — it crashes for this model; WakeNet re-arms itself. `idf.py flash` writes the 6 MB model partition; `idf.py app-flash` does not.
- RGB LED is a WS2812 on GPIO48, driven by `espressif/led_strip` over RMT+DMA. `RgbLed::breathe()` runs a 3 s blue breathing effect; it is independent of the audio pipeline.
- LVGL is not thread-safe. All UI updates go through `OledDisplay::showText()` and the LVGL task calls `OledDisplay::process()`; both take the display mutex. Do not call LVGL directly from audio or network tasks.
- WAV assets live in `spiffs_image/` and are flashed at `0xa10000`; the ESP-SR model is at `0x410000`. After changing assets run `idf.py flash`; `idf.py app-flash` updates only the app.

STM32 (CubeMX + CMake):

- The `.ioc` is the source of truth. Regenerate with CubeMX instead of hand-editing generated blocks.
- Build with the generated presets, e.g. `cmake --preset Debug && cmake --build build/Debug`; flash with STM32CubeProgrammer or OpenOCD.
- Current `.ioc` scope is clock (72 MHz HSE), SWD, USART1 (debug), and USART2 (ESP32 link). Motor/encoder timer and GPIO init lives in `App/motor.c` + `App/board_config.h`; migrate it into the `.ioc` once the pin map is frozen.
- Pin map: TIM1 full remap PWM on PE9/PE11/PE13/PE14; TB6612 DIR on PD4–PD11, STBY on PD2/PD3; encoders TIM2/3/4/8 on PA0/PA1, PA6/PA7, PB6/PB7, PC6/PC7. Full wiring table in `hardware/stm32-car-wiring.md`.
- Safety: TB6612 STBY is low at boot and on stop/fault, the IWDG is started in `Motor_Init`, and the main loop stops motion after 300 ms without a valid link frame. A hardware motor-power cut is still required.

Server:

- CMake, plain C++, targeting aarch64. Avoid x86-only dependencies; build on the board or cross-compile.
- Verify protocol code and server logic on the host (`ctest`), not only on the boards.

## Environment gotchas

- macOS serial ports are `/dev/cu.*`; `/dev/tty.*` blocks on carrier detect. Linux uses `/dev/ttyACM*` or `/dev/ttyUSB*`. Never hardcode a port; take a CLI flag or env var.
- Reserve ESP32 UART0 for flashing/console; use UART1/UART2 for the STM32 link.
- Server address is configuration, not a hardcoded IP; expect DHCP/mDNS changes.
- Pin assignments are owned by `hardware/`; mirror them into firmware config headers and update both together.

## Integration order

Prove links bottom-up: STM32 motor loop standalone -> UART heartbeat with ESP32 -> ESP32/server WebSocket -> audio/voice -> conversation. Do not start with the UI or LLM path.
