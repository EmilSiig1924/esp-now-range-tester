# AJ-SR04M on the C6

The sensor's confirmed labels are `5V`, `RX trigger`, `TX echo`, and `GND`, with
no resistor at R19. The AJ-SR04M manual identifies an open R19 as mode 1, the
trigger/echo interface. Leave R19 open for this firmware.

## Wiring

Disconnect USB and all other power before wiring. These pin choices assume the
C6 development board exposes GPIO18 and GPIO19. Use the GPIO labels rather than
counting physical header pins. GPIO16/17 are left for the UART console and
GPIO12/13 for native USB.

| AJ-SR04M pin | ESP32-C6 connection |
| --- | --- |
| 5V | Board's USB-powered 5V / VBUS pin |
| GND | GND |
| RX trigger | GPIO18 |
| TX echo | Through the voltage divider below to GPIO19 |

**Do not connect a 5 V Echo output directly to GPIO19.** Use two resistors:

```text
Sensor TX/Echo ---- 10 kOhm ----+---- C6 GPIO19
                              |
                           15 kOhm
                              |
Sensor GND -------------------+---- C6 GND
```

The junction between the resistors is the GPIO19 connection. With a 5 V sensor
output this gives approximately 3.0 V: `5 * 15 / (10 + 15)`. GPIO18 supplies a
3.3 V trigger signal. A direct RX/Trigger connection requires the particular
module to recognize 3.3 V as HIGH when powered at 5 V; that threshold has not
been verified for the user's board revision. If not guaranteed, use a suitable
3.3 V-to-5 V logic buffer on Trigger, or the documented 3.3 V supply arrangement
below. Do not put the Echo step-down divider on Trigger. A suitable logic-level
translator can also be used in place of the Echo divider.
Keep the sensor's GND and C6 GND connected.

The manual documents 3–5.5 V module power, but this guide deliberately shows
5 V operation with level reduction on Echo for the user's `5V`-labelled module.
Do not change the supply to 5 V while leaving Echo directly connected to the MCU.
Do not power the module through a GPIO.

## Upload both updated profiles

The reply packet now contains the distance and sensor status, so the ping
protocol is **version 2**, with a 40-byte packet. Both boards need the update;
mixing the old and new firmware produces timeouts.

1. Flash **echo_c6** to the C6 on **COM3**.
2. Flash **button_s3** to the S3 on **COM7**.
3. Open the C6 monitor at 115200 baud. Startup should show:

   ```text
   AJ-SR04M mode=1 | TRIG=GPIO18 ECHO=GPIO19 | interval_ms=250
   ```

4. Point the probe squarely at a flat wall or board about 0.5–1 m away. Allow
   about one second after boot. The C6 prints a `SENSOR` line about once a second.
5. Open the S3 monitor on COM7 at 115200 baud and press the existing button.

Use the named PlatformIO project tasks, or these commands in its terminal:

```powershell
pio run -e echo_c6 -t upload
pio run -e button_s3 -t upload
pio device monitor --port COM7 --baud 115200
```

The project now fixes C6 profiles to COM3 and S3 profiles to COM7 for upload and
monitoring. Close a board's monitor before uploading to it. If VS Code passes a
different `--upload-port`, change that selection: it overrides the project file.
If esptool reports a missing port even though `pio device list` shows it, inspect
the detailed error. `Access is denied` generally means a serial monitor or other
program already owns the port. Press Ctrl+C in that monitor before uploading.

## Reading the output

Illustrative S3 output, shortened here:

```text
PONG id=1 rtt_ms=4.800 outward_rssi=-67 dBm return_rssi=-64 dBm remote_turnaround_us=150 sensor=ok distance_mm=1234 sensor_age_ms=125 ...
```

`distance_mm=1234` means approximately **1.234 m from the probe to the reflecting
object**. This does not measure the separation between the ESP32s. The RSSI and
round-trip latency retain their previous meanings.

The C6 samples the sensor in a separate task every 250 ms, using the RMT
peripheral for pulse timing. A radio request reads the latest cached result
without waiting for an acoustic echo. `sensor_age_ms` is the age at the C6 when
preparing the reply, measured from the start of that sensor measurement. It
includes the acoustic capture interval and excludes the reply's radio journey.
The reply therefore describes a recent sample, not necessarily one taken after
the button press. Background sampling still creates some normal CPU contention;
RTT is not a laboratory measurement isolated from other tasks.

| `sensor` value | Meaning |
| --- | --- |
| `ok` | Valid pulse corresponding to the configured 0.2–8 m envelope |
| `not_ready` | No measurement has completed since boot |
| `no_echo` | No usable HIGH pulse, or capture timed out |
| `out_of_range` | Pulse implies less than 0.2 m or more than 8 m |
| `error` | Echo was already HIGH, capture failed, or pulse train was malformed |
| `stale` | Previously valid sample is at least one second old |

The S3 prints `distance_mm=NA` for every status except `ok`. Failed measurements
replace old successful readings so an unplugged sensor does not silently retain
its last distance. `no_echo` and `out_of_range` can both occur with no useful
target; the precise behavior depends on the module. A radio `PONG` can succeed
while the sensor is invalid: the two statuses are independent.

The manual's envelope is 20 cm to 8 m, but actual usable range depends on the
module revision, target, angle, and surroundings. The conversion assumes a
343 m/s speed of sound, approximately dry air at 20 C, without temperature
compensation. Millimetre output units do not imply millimetre accuracy.

If distance is invalid, check R19 is open, the divider orientation, common GND,
RX/Trigger versus TX/Echo wiring, and that the target is beyond the blind zone.
Do not expect reliable measurements against soft, small, or steeply angled
surfaces. The exposed circuit board must stay dry.

### Diagnose a persistent NA reading

The C6 now prints capture diagnostics once a second. A line such as
`reason=capture_timeout gpio_rises=0 gpio_falls=0` means RMT did not finish a
capture and an independent GPIO interrupt counter saw no digital transitions on
GPIO19 during that attempt. It does not prove the sensor itself is defective:
power, wiring, logic levels, probe connection, and the target remain possible
causes. `echo_high_before_trigger` indicates an already-HIGH input, while
`no_high_pulse` means RMT completed without recording a HIGH pulse. `raw_us`
lists the first captured level/duration pairs when available.

For a controlled check of the divider/logic level, the AJ-SR04M manual permits
3–5.5 V supply. With all power disconnected, move the sensor power wire from
the C6's 5V/VBUS to **3V3**, remove the Echo divider, and connect sensor TX/Echo
directly to GPIO19. Keep RX/Trigger on GPIO18 and GND shared. Restore power and
test against the same flat target. **Direct Echo wiring is only for this
3.3 V supply arrangement; do not reconnect sensor power to 5 V without restoring
the divider or a suitable level translator.**

If no pulses are still detected, measure the actual DC voltage across the
sensor's own power and GND terminals while powered, rather than only checking
which MCU header the wire uses. Also check the probe connector is fully seated.

### Diagnose a missing Trigger

The current C6 firmware enables input feedback on its output pin and checks the
actual pad after commanding HIGH and LOW. `trig_read=1/0` is the expected result.
`trig_attempts` counts attempted pulses. `trig_latch`, `trig_oe`, and `trig_hold`
record the output latch, output enable, and digital-pad hold at the HIGH check.
On a failed readback the firmware leaves Trigger LOW and stops retrying until
reset, reporting `trigger_output_fault_reset_required`. GPIO configuration is
dumped in that fault state to help distinguish routing from external loading.

The audit observed `trig_read=0/0 trig_latch=1 trig_oe=1 trig_hold=0`, with input
and output both enabled and simple GPIO routing selected on GPIO18. This means
the requested HIGH did not appear as a digital HIGH at the pad; it does not by
itself distinguish an external clamp/short from a damaged pin or other hardware
issue. The previous firmware did not check this, so an absent Trigger could be
reported downstream as `no_echo`.

To isolate it, power off the C6, remove every wire and analyzer probe from its
GPIO18 header pin, then power back on. If the readback becomes `1/0`, investigate
the removed sensor cable, sensor power, connector orientation, and analyzer
lead assignment before reconnecting. If it stays `0/0`, verify the exact board
model and GPIO mapping before selecting another pin or diagnosing board damage.
The DevKitC-style board definition is an assumption, not identification of the
physical header on an unknown board.

Independently check the Kingst input: with a common GND, a 1.5 V threshold, and
all trigger conditions cleared, capture analyzer input 0 on the C6's 3V3 pin.
It should be HIGH; on GND it should be LOW. Do this with a numbered analyzer
input, not a GND lead or PWM output. Verify that KingstVIS is connected to the
physical LA5016 rather than in demo mode. This test needs no sensor response.

Pin assignments, sampling interval, and stale threshold are in `include/config.h`.
The original continuous range-test profiles do not read this sensor.

## References and validation

Both updated firmware profiles, `button_s3` and `echo_c6`, compiled and linked
successfully with the pinned ESP-IDF platform. The distance, validity, button,
and reply-correlation compile-time checks passed with the ESP32-S3 C++ compiler.
The resolved PlatformIO configuration was checked for COM3 on the C6 and COM7
on the S3, for both upload and monitoring.

- [AJ-SR04M manual: supply, R19 modes, and measurement envelope](https://mantech.co.za/Datasheets/Products/AJ-SR04M-200925A.pdf)
- [AJ-SR04M driver author's electrical-interface notes](https://github.com/CyrilB1531/aj_sr04m/#hardware)
- [C6 DevKitC pinout](https://documentation.espressif.com/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html)
- [ESP-IDF 5.4 RMT peripheral](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32c6/api-reference/peripherals/rmt.html)

The compile-time checks include distance conversion, the blind zone, out-of-range
pulses, overflow, and stale/startup readings, in addition to the button and reply
correlation checks:

```text
g++ -std=c++17 -fsyntax-only -I include tests/statistics_test.cpp tests/ping_test.cpp tests/sensor_test.cpp tests/lamp_test.cpp tests/display_test.cpp
```

The updated `button_s3` sensor firmware was uploaded successfully on COM7 with
flash hashes verified after closing a serial monitor that held the port open.
The updated diagnostic `echo_c6` was uploaded on COM3 with flash verification.
The subsequent trigger-readback audit reports a GPIO18 output readback fault;
earlier logs reported capture timeouts with zero GPIO Echo edges.
Valid physical distance readings remain unverified; check against a measured
flat target after the electrical checks above. The diagnostic update keeps the
same version-2 packet, so the already-updated S3 does not need reflashing.
