# Button-triggered ESP-NOW test

The S3 starts a test when you press its button. The C6 replies, and the S3 prints
the round-trip latency and signal strength in each direction. These profiles
send no continuous range-test traffic and do not need manually entered MAC
addresses. Run only one echo board at a time: requests are broadcast, and the
first matching reply completes the test. Replies are unicast to the S3.

The C6 reply now also includes the AJ-SR04M distance measurement. Follow
[AJ-SR04M-SETUP.md](AJ-SR04M-SETUP.md) for sensor wiring and updated firmware.
Both boards must use protocol version 2; reflash both after adding the sensor.

## Wire the button to the S3

Disconnect power before wiring. Use the electrically isolated mechanical switch
contacts you identified with the multimeter:

| Wire colour | Button contact | ESP32-S3 connection |
| --- | --- | --- |
| Green | COM | GND |
| Yellow | NO (connects to COM only while pressed) | GPIO4, labelled `4` or `IO4` |
| Blue | NC | Leave disconnected and insulated |
| Black | Lamp + | GPIO5 through 100 Ohm, see [the lamp section](#optional-wire-the-button-lamp) |
| Red | Lamp − | GND |

These colours are for this particular button. Its lamp wires are the reverse of
the usual convention: **black is +, red is −**.

Use the GPIO label, not the fourth physical header pin. The firmware enables
GPIO4's internal pull-up: released reads HIGH and pressed reads LOW. No external
resistor is needed for a short switch connection. For long/noisy wiring, use a
suitable external pull-up and input protection. Never apply 5 V or the lamp's
supply voltage to the GPIO. The original board assumption is an S3 DevKitC-style
board with GPIO4 available; check your actual board's pinout.

The purchased option was listed as a 3–9 V momentary illuminated button. The
mechanical switch works without illumination; the lamp is optional.

## Optional: wire the button lamp

The firmware drives **GPIO5** as a status output: HIGH means lamp on.

### Current setup: direct from GPIO5

This button's lamp draws about **2 mA at 3 V** (measured with a bench supply),
well within a GPIO's safe output current. It is wired directly and confirmed
working:

```text
S3 GPIO5 ---- 100 Ohm ---- black (lamp +)
                           red (lamp -) ---- S3 GND
```

The 100 Ohm resistor drops only about 0.2 V at this current. It protects the pin
if a wire touches 5 V or GND while working on the circuit. Never connect the lamp
between 5V and a GPIO: that puts 5 V on a 3.3 V pin.

Direct drive is only suitable for a lamp drawing under about 10 mA at 3.3 V. For
a brighter or unknown lamp, check it first and use the transistor switch below.

### 1. Check the lamp first

With the lamp wires disconnected from everything else:

1. Connect two fresh AA cells in series (about 3 V, inside the 3–9 V rating) to the
   two lamp wires. If it does not light, swap the wires. The orientation that lights
   tells you which wire is **+**. Mark it. A short reverse test at 3 V is within
   normal LED reverse ratings.
2. If you have a multimeter, measure the lamp current from the board's 5 V pin
   with the meter in series (mA range). Expect roughly 5–30 mA. Anything above
   100 mA needs a different transistor than the one below.

If the lamp does not light either way at 3 V, check it at 5 V the same way before
continuing. Do not exceed 9 V.

### 2. Low-side transistor switch (for higher-current lamps)

Parts: one NPN transistor such as a 2N2222, PN2222, or BC337, one 1 kOhm and one
10 kOhm resistor. A logic-level N-channel MOSFET (for example a 2N7000 or AO3400)
works as well; use a 100 Ohm gate resistor and keep the 10 kOhm pull-down.

```text
S3 5V/VBUS ---------------- lamp + (black)
                            lamp - (red) ---- collector (C)
S3 GPIO5 ---- 1 kOhm ---+-------------- base (B)
                        |
                     10 kOhm
                        |
S3 GND -----------------+-------------- emitter (E)
```

Check the C/B/E pin order in your transistor's datasheet: 2N2222 metal-can and
PN2222/BC337 plastic packages differ. In the common plastic TO-92 versions the
base is the middle leg, but which outer leg is the collector varies by part and
manufacturer. To check with a multimeter in diode mode: red probe on the base and
black on either outer leg reads about 0.6–0.7 V; base-to-emitter reads slightly
higher than base-to-collector. The 10 kOhm resistor keeps the lamp off
while the S3 is resetting, before the firmware configures GPIO5. Use the GPIO
label `5` or `IO5`, not the fifth header pin.

### Lamp signals

| Lamp | Meaning |
| --- | --- |
| Off | Idle, ready for a press |
| Steady on | Request sent, waiting for the reply (often too short to see at close range) |
| One 0.4 s flash after the press | `PONG`: reply received |
| Three short flashes | `TIMEOUT`: no matching reply within 1 s |
| Six rapid flashes | `SEND_ERROR`: the S3 could not queue the request |

A `BUSY` press does not change the lamp. The lamp is cosmetic: it never changes
the measured RTT. Pin, polarity (`lampActiveHigh`) and flash timings are in
`include/config.h`. With the defaults, GPIO5 is HIGH when the lamp is on. The
lamp only needs a `button_s3` reflash; the C6 and the packet format are unchanged.

## Optional: 0.91" OLED screen

A 0.91" 128x32 I2C OLED (SSD1306 controller, pins `GND VCC SCK SDA`) on the S3
shows each result, so the test works without a laptop. Disconnect USB first.

| OLED pin | ESP32-S3 connection |
| --- | --- |
| GND (may be printed `NGD`) | GND |
| VCC | **3V3** (not 5V) |
| SCK | GPIO9 |
| SDA | GPIO8 |

Use 3V3: many of these modules pull SDA and SCK up to their own VCC, and the S3
pins are not 5 V tolerant. Keep the four wires short. The firmware detects the
screen at address 0x3C or 0x3D. Without a screen it prints `DISPLAY none` and
the button test works as before.

Screen after a reply:

```text
#12  RTT 4.8 ms
RSSI -67 / -64 dBm
Dist 1.234 m
OK 11/12  Lost 1
```

- Line 1: request number and round-trip time.
- Line 2: outward RSSI (as received by the C6) / return RSSI (as received by the S3).
- Line 3: sensor distance, or `--` and the sensor status (for example `no_echo`).
- Line 4: replies / attempts since startup, and timeouts.

After a timeout the screen shows `#13  TIMEOUT` and `No reply in 1000 ms`. The
display is drawn by a low-priority background task, so it never delays button
handling or the RTT measurement. If the text is upside down, set `displayFlip`
to `true` in `include/config.h` and reflash `button_s3`. Pins are configured there
as well.

## Flash both boards

Use PlatformIO's named **Project Tasks**, not the default Upload button (the
default profile still runs the original continuous receiver).

1. Connect the S3 alone with a data USB cable, using the UART connector first if
   there are two connectors. Run **button_s3 > Build**, then **Upload**.
2. Close its serial monitor and connect the C6. Run **echo_c6 > Build**, then
   **Upload**. The C6 can then run from a USB power bank.
3. Reconnect the S3 and open **button_s3 > Monitor**, at **115200 baud**, on its
   COM port. You should see `READY | button=GPIO4 ...`.
4. Release the button, then press it. Holding it down sends only one request.
   Each change must be stable for 30 ms. A button held during startup must be
   released before the first test. The `READY` line also shows `lamp=GPIO5`.
   A `DISPLAY` line reports whether the OLED was found.

The project sets `upload_port` and `monitor_port` to **COM7 for the S3** and
**COM3 for the C6** in `platformio.ini`. Close the monitor before uploading. If necessary, hold BOOT,
tap RESET, then upload; tap RESET afterward to leave download mode.

If upload says a COM port does not exist, run `pio device list` and use the port
actually listed for the connected board. A VS Code upload command containing `--upload-port COM7`
overrides `upload_port` in the project file; change the selected upload port or
run the command with the correct port. The C6 was identified on COM3 during
setup, and its `echo_c6` upload and monitor ports are currently set to COM3.

Equivalent commands in a PlatformIO terminal using the requested port assignments:

```powershell
pio run -e button_s3 -t upload --upload-port COM7
pio run -e echo_c6 -t upload --upload-port COM3
pio device monitor --port COM7 --baud 115200
```

The original `receiver_s3` / `transmitter_c6` profiles do not participate in this
protocol. Both boards must use the new profiles and the same `config::channel`.

## Interpret the results

Illustrative S3 output:

```text
PONG id=1 rtt_ms=4.800 outward_rssi=-67 dBm return_rssi=-64 dBm remote_turnaround_us=130 sensor=ok distance_mm=1234 sensor_age_ms=125 attempts=1 replies=1 timeouts=0 local_errors=0 queue_drops=0 peer=AA:BB:CC:DD:EE:FF
```

- `rtt_ms`: time from just before the S3 calls `esp_now_send()` to entry into
  its receive callback for the matching reply. Timing uses the S3's own clock;
  the clocks on the two boards do not need synchronization.
- `outward_rssi`: strength of the request as received by the C6, returned inside
  the reply. `return_rssi`: strength of the reply as received by the S3. Less
  negative values mean stronger reception. Neither value measures distance.
- `remote_turnaround_us`: time on the C6 from its request receive callback to
  just before its reply send call, including task scheduling and peer setup.
  It excludes subsequent radio transmit queue time. It is included in `rtt_ms`.
- `attempts`, `replies`, `timeouts`, `local_errors`: counts since S3 startup.
  Local send API failures are counted separately from unanswered requests.
- `queue_drops`: S3 software receive-queue losses since startup. The C6 logs its
  own queue drops in its monitor. Normally both are zero.
- `sensor`, `distance_mm`, `sensor_age_ms`: latest C6 ultrasonic measurement,
  validity, and sample age. See the sensor guide. These are independent of the
  radio's round-trip time; invalid sensor readings display `distance_mm=NA`.

`TIMEOUT` means a matching reply did not arrive within 1000 ms. It does not say
which direction failed. Late, duplicate, and unrelated replies are ignored.
There are no application retries. The radio may retry unicast replies at the
MAC layer, which is part of the measured latency. A press during an outstanding
test prints `BUSY`; release and press again after completion or timeout.

This is application round-trip latency, including Wi-Fi scheduling, queueing,
remote processing, and radio transmission. It is not radio-only flight time,
accurate one-way delay, or a distance measurement. Do not treat RTT/2 as a measured
one-way delay. The first exchange can include extra peer-registration work on the
C6. The protocol is unencrypted; its magic, session, and ID prevent accidental
mix-ups, not malicious interference.

The configurable GPIO, debounce time, timeout, and shared radio channel live in
`include/config.h`. Rebuild and upload the affected board after changing them;
upload both after changing channel or packet format.

## Checks

Both `button_s3` and `echo_c6` compiled and linked successfully using the pinned
PlatformIO ESP-IDF platform. The compile-time checks below passed with the
installed ESP32-S3 C++ compiler.

Compile-time checks cover contact bounce, held-button suppression, a button held
during startup, wrong sessions/IDs, and replies at the timeout boundary:

```text
g++ -std=c++17 -fsyntax-only -I include tests/statistics_test.cpp tests/ping_test.cpp tests/sensor_test.cpp tests/lamp_test.cpp tests/display_test.cpp
```

`tests/lamp_test.cpp` covers the lamp's flash counts, on/off timing, and a new
event replacing a running pattern. The lamp firmware was flashed to the S3 and
confirmed working by the user with the direct GPIO5 wiring above.

The earlier button-only `echo_c6` firmware was uploaded successfully on COM3,
with flash hashes verified, and the user subsequently reported completing both
uploads. The current sensor update changes the protocol and needs to be uploaded
to both boards again. The updated S3 firmware has now been uploaded successfully
on COM7 with flash verification. Physical sensor operation has not yet been verified.
