# ESP-NOW range tester

A small programming project for two ESP32 boards. A portable transmitter sends
a numbered packet every 100 ms; the receiver prints radio signal strength and
sequence-gap statistics once per second. No router, Wi-Fi password, sensor,
display, or wiring between the boards is needed. Power each board by USB.

For a button-triggered request and reply with round-trip latency and signal
strength in both directions, use the **button_s3** and **echo_c6** profiles in
[the button setup guide](BUTTON-SETUP.md). The original continuous test remains
available using the profiles below.

The button test now includes an **AJ-SR04M on the C6**. See
[AJ-SR04M-SETUP.md](AJ-SR04M-SETUP.md) for wiring, the Echo voltage divider, and
distance output. C6 profiles use COM3; S3 profiles use COM7. The updated sensor
packet requires reflashing both `echo_c6` and `button_s3`.

## Open in VS Code

1. Open this folder with **File > Open Folder**, or open
   `esp-now-range-tester.code-workspace`.
2. Install the recommended **PlatformIO IDE** extension from PlatformIO.
3. Let PlatformIO finish its first-run setup. It downloads the compiler and
   framework, which can take several minutes and substantial disk space.
4. Open the PlatformIO sidebar and expand **Project Tasks**. Use the named
   environment's **Build**, **Upload**, and **Monitor** tasks below.

This project uses C++ with **ESP-IDF**, pinned through the official
`espressif32@6.10.0` PlatformIO platform (ESP-IDF 5.4). The standard PlatformIO
C6 board definition supports ESP-IDF; this avoids needing a community Arduino
platform. There is no separate ESP-IDF installation step.

## Board choices

| Role | PlatformIO environment | Assumed board |
| --- | --- | --- |
| Receiver (default) | `receiver_s3` | ESP32-S3 DevKitC-style board, including WROOM-1 N16R8 |
| Transmitter | `transmitter_c6` | ESP32-C6 DevKitC-1-style board |
| Alternative transmitter | `transmitter_esp32` | Classic ESP32 Dev Module |
| Alternative transmitter | `transmitter_s3` | A second ESP32-S3 |

The earlier conversation identified an S3 N16R8 and a C6, so those are the
primary pair. Check the marking on your actual second board before uploading.
These profiles assume development boards with at least 4 MB flash. The S3
profile deliberately uses only a 4 MB flash layout and leaves PSRAM disabled;
the test does not need the full 16 MB flash or 8 MB PSRAM. Boards with different
flash interfaces or unusual USB hardware may need a matching board definition.

## Upload and run

1. Connect only the **S3 receiver**, using a data-capable USB cable.
2. Under `receiver_s3`, run **Build**, then **Upload**, then **Monitor**.
   Serial speed is **115200**. Press RESET if the startup text has already gone
   past. You should see `WAITING` once per second.
3. Close the monitor before uploading again. Connect the **C6 transmitter** and
   use `transmitter_c6` > **Upload**. Power it from a USB power bank for walking
   tests. It does not wait for a computer or serial monitor before transmitting.
4. Reopen the receiver's **Monitor** on its own port. Start with the boards
   a few metres apart. You should see `LIVE` lines. Walk away with the transmitter
   and record distance, obstructions, orientation, RSSI, and gaps.

For a board with two USB connectors, use the one labelled **UART** first.
The configuration also requests secondary USB Serial/JTAG console output on
chips that support it (S3/C6), for boards connected through native USB. Native
USB may appear on a different COM port after reset; select that new port.
The classic ESP32 uses its USB-to-UART bridge.

When both boards are connected, set the correct ports explicitly. Uncomment or
add these lines **inside each relevant environment** in `platformio.ini`:

```ini
upload_port = COM7
monitor_port = COM7
```

`COM7` is an example, not a detected port. PlatformIO's Devices list or Windows
Device Manager shows yours. Use a different port for each connected board.

Equivalent commands in a **PlatformIO terminal**:

```powershell
pio run -e receiver_s3
pio run -e transmitter_c6
pio run -e receiver_s3 -t upload --upload-port COM7
pio run -e transmitter_c6 -t upload --upload-port COM8
pio device monitor --port COM7 --baud 115200
```

## Read the output

Illustrative receiver output:

```text
LIVE session=9A102BCD seq=104 rssi=-65 dBm rx=103 gaps=2 gap_pct=1.9 late_dup=0 age_ms=42 tx_uptime_ms=10400 queue_drops=0
```

- `rssi`: strength of the most recently received packet, in dBm. Less negative
  means stronger reception. It is not a distance measurement or a rolling average.
- `rx`: forward-progress packets counted since the first observed packet of the
  current transmitter boot session.
- `gaps`: missing sequence numbers between observed packets. For `100, 101, 104`,
  two sequence numbers are missing: 102 and 103.
- `gap_pct`: `100 * gaps / (rx + gaps)`, cumulative for this session.
- `late_dup`: old or duplicate sequence numbers excluded from `rx`.
- `age_ms`: time since the most recent packet from the selected transmitter.
- `STALE`: no packet received for at least two seconds. Displayed RSSI is then
  the last known value. It is no longer a live signal measurement.
- `queue_drops`: packets lost in the receiver's software queue since receiver
  startup. Normally zero; nonzero values can contaminate the radio-loss estimate.
- `tx_uptime_ms`: transmitter uptime at its last accepted forward packet.
- `session`: random transmitter boot ID; a new ID resets the session statistics.

**Measurement limits:** Initial packets missed before the first observation are
not counted. During complete silence, gaps cannot increase until a later packet
reveals its sequence number; use `STALE` and `age_ms` to recognise an outage.
Late packets do not repair previously counted gaps. Local transmitter enqueue
failures also produce gaps, so this is an observed sequence-gap estimate, not a
laboratory measurement of radio-only packet loss. Check `enqueue_errors` on the
transmitter if gaps occur at close range. Its send result means queued locally,
not confirmed delivery to the receiver. The continuous test is a one-way
broadcast test without acknowledgements, retransmissions, or encryption.

The receiver locks onto the first valid transmitter MAC address until it
restarts. Run one transmitter at a time. Reset the receiver to switch devices or
begin a fresh distance trial. Resetting the transmitter also starts a new session.
Packets from unrelated applications are filtered by size, magic number, and
version; these fields are identifiers, not authentication.

## Where the code lives

```text
esp-now-range-tester/
  platformio.ini                  Board and build profiles
  esp-now-range-tester.code-workspace
  CMakeLists.txt                  ESP-IDF project setup
  sdkconfig.defaults              Flash and serial console defaults
  include/
    config.h                      Channel, timing, packet identity
    packet.h                      Shared 20-byte packet format
    ping.h                        Button debounce and request/reply packet format
    lamp.h                        Non-blocking button lamp flash patterns
    text_frame.h                  OLED font and 128x32 text layout
    display.h                     Optional OLED interface
    sensor_data.h                 Distance conversion and sensor validity
    ultrasonic.h                  Background ultrasonic task interface
    ping_radio.h                  Request/reply radio queue interface
    radio.h                       Shared radio setup declaration
    statistics.h                  Sequence tracking and gap calculation
  src/
    CMakeLists.txt                Source registration
    common/radio.cpp              Wi-Fi and ESP-NOW initialization
    common/ping_radio.cpp         Request/reply receive queue and peer setup
    transmitter/main.cpp          Send one numbered packet every 100 ms
    receiver/main.cpp             Queue packets and print measurements
    button/main.cpp               S3 button input, lamp, and round-trip measurement
    button/display.cpp            SSD1306 OLED driver task
    echo/main.cpp                 C6 reply with outward RSSI and turnaround time
    echo/ultrasonic.cpp            AJ-SR04M mode-1 capture with RMT
  .vscode/                        Recommended extension and editor settings
  tests/statistics_test.cpp       Compile-time checks for sequence accounting
  tests/ping_test.cpp             Button debounce and reply correlation checks
  tests/sensor_test.cpp           Distance conversion and stale reading checks
  tests/lamp_test.cpp             Lamp flash pattern timing checks
  tests/display_test.cpp          OLED text placement and clipping checks
```

Start reading `packet.h`, then `transmitter/main.cpp`, then
`receiver/main.cpp`. ESP-IDF enters through `app_main()`. Each board runs a loop;
FreeRTOS delays yield time to the radio and other tasks. The receiver callback
copies the packet, RSSI, and arrival time into a queue. The main loop does the
counting and printing so it does not block the Wi-Fi callback.

## First programming experiments

1. Change `sendIntervalMs` in `include/config.h` from 100 to 500. Rebuild and
   upload the transmitter. Watch packets arrive more slowly.
2. Change `channel` from 1 to 6, then rebuild and upload **both** boards.
3. Add a field to `TestPacket`, update its size assertion, populate it on the
   transmitter, and print it on the receiver. Upload both sides together.

## Troubleshooting

- **Upload waits for a connection:** hold BOOT, tap RESET, start the upload,
  and release BOOT once connecting starts. Use a data cable and close other
  serial monitors. After flashing in download mode, tap RESET to run the program.
- **No serial text:** verify the COM port and 115200 baud; try the UART connector.
  A charge-only USB cable cannot upload or show output.
- **Only WAITING:** confirm both roles are flashed, the channels match, and
  the transmitter has power. Verify that you did not upload the receiver build
  to both boards. Start close together and reset the receiver.
- **Power bank shuts off:** some power banks turn off with small loads; use its
  always-on mode if available or another supply.
- **Windows path-length build error:** move the whole project folder to a
  shorter path such as `C:\dev\esp-now-range-tester`, reopen it, and rebuild.
- **After changing SDK defaults:** generated `sdkconfig.<environment>` files
  preserve earlier settings. Delete only the affected generated config, run
  PlatformIO **Clean**, then **Build**. Do not delete `sdkconfig.defaults`.

## References

- [PlatformIO ESP-IDF project documentation](https://docs.platformio.org/en/stable/frameworks/espidf.html)
- [Pinned platform release](https://github.com/platformio/platform-espressif32/tree/v6.10.0)
- [PlatformIO C6 board definition](https://github.com/platformio/platform-espressif32/blob/v6.10.0/boards/esp32-c6-devkitc-1.json)
- [Espressif ESP-NOW API (ESP-IDF 5.4)](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32c6/api-reference/network/esp_now.html)

## Validation

Both primary profiles, `receiver_s3` and `transmitter_c6`, compiled and linked
successfully with PlatformIO 6.2.0 and the pinned platform. The sequence
statistics checks passed. The alternative transmitter profiles are provided but
were not build-tested. No firmware was uploaded; physical range and USB-port
behavior still need checking with your boards.

The additional `button_s3` and `echo_c6` profiles also compiled and linked
successfully with the pinned ESP-IDF platform. The button debounce and reply
correlation checks in `tests/ping_test.cpp` passed with the installed ESP32-S3
C++ compiler. These checks do not replace physical testing of the button or
radio link. See [BUTTON-SETUP.md](BUTTON-SETUP.md) for wiring and flashing.
The earlier `echo_c6` firmware was subsequently uploaded to the detected C6 on
COM3 and passed flash hash verification; the user reported completing both board
uploads. The AJ-SR04M update requires reflashing both boards with protocol version
2. The updated `button_s3` was uploaded on COM7 with flash verification after
closing a monitor that held the port open. The diagnostic `echo_c6` was also
uploaded on COM3 with verification. The output audit found GPIO18 reads LOW
even when its enabled output latch is HIGH; the firmware now stops further
Trigger attempts until reset after this fault. An unloaded-pin check is pending.
Valid sensor distance readings remain unverified; see the sensor guide's
electrical checks.
Both updated sensor profiles compiled and linked successfully, and the distance
conversion and validity checks in `tests/sensor_test.cpp` passed.

The statistics checks use C++17 compile-time assertions for missing packets,
duplicates, reboot resets, and sequence wraparound. With any C++17 compiler:

```text
g++ -std=c++17 -fsyntax-only -I include tests/statistics_test.cpp tests/ping_test.cpp tests/sensor_test.cpp tests/lamp_test.cpp tests/display_test.cpp
```
