# Extending AEL: 1 Hz Blinky on ESP32-WROOM-32D

A personal project that extends the [AI Embedded Lab (AEL)](../../../README.md) framework with a new closed-loop test target. Builds firmware, flashes it to a real ESP32, observes UART output, and emits an automated PASS/FAIL — all driven by AEL's pack/plan/board-profile architecture.

**Result: PASS, verified on real hardware.** Run artifacts in this folder.

---

## Context

AEL is an AI-driven embedded engineering framework: the agent generates firmware, flashes it to MCUs, observes runtime behavior, and verifies results — closing the build/flash/observe/iterate loop without human intervention. The repo ships with first-class support for the ESP32-WROOM-32D dev board, including a `hello` (boot-only) target and a `test_pwm` (LEDC at 1 kHz) target, but no plain GPIO blink target.

This project adds one. The work began by reading AEL's conventions — board configs, test plans, packs, firmware target layout — then mirroring the existing patterns to introduce `blinky_1hz` as a new Stage 0 bring-up test.

I had no prior exposure to AEL. Setup also included a fresh ESP-IDF v5.3 install in WSL2 and configuring usbipd-win for USB passthrough from Windows to Linux.

## What I Added

Six files, all following AEL's existing conventions:

```
firmware/targets/esp32_wroom32d/blinky_1hz/
├── CMakeLists.txt              # ESP-IDF project descriptor
├── sdkconfig.defaults          # minimal config — no WiFi/BLE
└── main/
    ├── CMakeLists.txt          # IDF component descriptor
    └── main.c                  # firmware: GPIO2 toggles every 500 ms
tests/plans/esp32_wroom32d/blinky_1hz.json    # AEL test plan: UART observe + expect_patterns
packs/esp32_wroom32d_blinky_1hz.json          # AEL pack: runnable bundle
```

## How the Test Passes

The firmware drives GPIO2 (onboard LED) at 1 Hz and prints `AEL_BLINKY_DONE` after twenty 500 ms toggles. The test plan declares the regex AEL must match in 20 seconds of captured UART output:

```c
// firmware/targets/esp32_wroom32d/blinky_1hz/main/main.c (excerpted)
gpio_reset_pin(LED_GPIO);
gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
for (int i = 1; i <= VERIFY_TICKS; i++) {
    level ^= 1;
    gpio_set_level(LED_GPIO, level);
    printf("AEL_BLINKY tick=%d state=%s\n", i, level ? "on" : "off");
    vTaskDelay(pdMS_TO_TICKS(HALF_PERIOD_MS));
}
printf("AEL_BLINKY_DONE ticks=%d freq_hz=1\n", VERIFY_TICKS);
```

```json
// tests/plans/esp32_wroom32d/blinky_1hz.json (excerpted)
"observe_uart": {
  "enabled": true,
  "duration_s": 20,
  "expect_patterns": ["AEL_BLINKY_DONE"]
}
```

The contract is intentional and explicit: firmware emits a sentinel string, plan declares the regex. The two are coupled — change one without the other and the test breaks.

## Run Output

```
PASS: Run verified
Summary: validation board=ESP32-WROOM-32D CP210X test=blinky_1hz result=pass
Summary: executed_stages=plan,run,check,report
Summary: key_checks_passed=uart.verify
Summary: port=/dev/ttyUSB0
LKG: board=ESP32-WROOM-32D CP210X test=blinky_1hz
```

Snapshot of run artifacts in this folder:

| File                  | What it shows                                          |
|-----------------------|--------------------------------------------------------|
| `result.json`         | Top-level pass/fail and run metadata                   |
| `meta.json`           | Run context, host, timestamps                          |
| `run_plan.json`       | Resolved plan (board config + test plan, merged)       |
| `evidence.json`       | Captured UART output and per-pattern match results     |
| `verify_result.json`  | Verification output: which patterns matched           |
| `uart_observe.json`   | UART observation step result                           |
| `observe_uart.log`    | Raw byte stream captured from `/dev/ttyUSB0`           |

## Hardware

- **MCU**: ESP32 (Xtensa LX6 dual-core, 240 MHz)
- **Board**: ESP32-WROOM-32D dev board, Silicon Labs CP2102 USB-UART bridge
- **LED**: onboard, GPIO2
- **Host**: Windows 11 + WSL2 Ubuntu 24.04
- **USB passthrough**: [usbipd-win](https://github.com/dorssel/usbipd-win)

I had two ESP32 boards available — one with a CP2102 USB-UART bridge, one with WCH CH9102X. I picked the CP2102 because AEL's board config (`configs/boards/esp32_wroom32d_cp210x.yaml`) is tuned for that exact chip family: in-kernel Linux drivers, predictable RTS/DTR auto-reset behavior, and a decade of esptool compatibility testing. Choosing it eliminated an entire class of "weird flashing failure" risk.

## How to Reproduce

```bash
# 1. Set up ESP-IDF v5.3 in WSL2 Ubuntu
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf
sudo apt install -y python3.12-venv python3-pip cmake ninja-build ccache \
                    libffi-dev libssl-dev dfu-util libusb-1.0-0 flex bison gperf wget
~/esp/esp-idf/install.sh esp32
. ~/esp/esp-idf/export.sh

# 2. Attach the ESP32 to WSL (in PowerShell as admin, one-time):
#   usbipd bind --busid 1-3
# Then any session:
#   usbipd attach --wsl --busid 1-3

# 3. Run via AEL
cd ~/ael-fork   # this repo
python3 -m ael pack \
  --pack packs/esp32_wroom32d_blinky_1hz.json \
  --board esp32_wroom32d_cp210x
```

## Design Notes

Three decisions worth surfacing:

**Modeled on `hello`, not `test_pwm`.** The existing `test_pwm` target uses LEDC PWM hardware — closer in spirit to "blink at frequency" — but it pulls in shared `ael_board` components and a heavy `sdkconfig` with WiFi, BLE, and custom partitions enabled. Overkill for a Stage 0 GPIO toggle. The leaner `hello` pattern was the right base: direct `gpio_set_level()` calls, no extra components, smallest reasonable binary. Picking the right ancestor matters more than picking the closest one.

**The pass/fail contract is a string by design.** AEL supports richer ground-truth paths (`datacapture: true` for logic-analyzer verification via the ESP32JTAG instrument), but in their absence, regex over UART is the right default. It's universal across MCUs, debuggable by humans, and doesn't require an instrument. The cost is the firmware/plan coupling — explicit, but real.

**What a stronger version would do.** The current firmware self-attests by printing `DONE` after the loop completes; it doesn't verify the pin actually toggled. A stricter version would read the GPIO_OUT register back after each `gpio_set_level()`, assert it matches the requested level, and only emit `AEL_BLINKY PASS` when every readback matched — then the test plan regex becomes `AEL_BLINKY.*PASS` (mirroring `test_pwm`'s pattern). For true closed-loop verification you'd route GPIO2 to AEL's logic-analyzer instrument input and assert frequency = 1.0 Hz ± tolerance. That's the pattern AEL was built for; this contribution is the printf-baseline that the instrument-driven version would later supersede.

## Stack

C, FreeRTOS, ESP-IDF v5.3, CMake + Ninja, esptool, AEL (Python), WSL2 Ubuntu, usbipd-win.

## Credits

This work extends [AEL (AI Embedded Lab)](../../../README.md). Upstream code is unchanged; the contribution is the new `blinky_1hz` target, its plan and pack, and the curated artifact snapshot in this folder.
