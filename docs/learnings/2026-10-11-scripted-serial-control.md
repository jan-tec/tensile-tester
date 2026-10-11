# Bench Ops Learnings - 2026-10-11 (scripted serial control, #48 session)

How this session drove the machine from the workstation over USB serial:
calibration commands, force reads, boot-log capture, reset. Complements
`2026-08-21-bench-ops.md` (host paths, reset caveat).

## 1. The driver pattern (pyserial)

Open the port with BOTH control lines asserted - that is the safe state
for the ESP32 auto-reset circuit (neither EN nor IO0 pulled):

```python
s = serial.Serial()
s.port = PORT            # Mac: /dev/cu.usbserial-0001, Linux: /dev/ttyUSB0
s.baudrate = 115200
s.timeout = 0.1
s.dtr = True             # both asserted = safe
s.rts = True
s.open()
```

Send one command per line, then drain with a quiet-window (stop after
~1.2 s without output, cap ~4 s). The CLI answers after the command
completes (cal_apply internally settles 1.2 s + averages 10 samples at
10 Hz - budget ~4 s for it).

## 2. Command dispatch gotcha (pinned NQP behavior)

Pass value commands as ONE line: `cal_apply 2465`, not `cal_apply` +
`2465`. A bare `cal_apply` is rejected as `Unbekanntes Kommando` - the
prefix parser requires its value token (quirks #42/#43 family; pinned by
the parser host tests).

## 3. Periodic status lines interleave with replies

`App::loop()` prints a multi-line position/status block roughly every
interval while drivers are enabled or the cell is ready. Filter it out
when scripting (`grep -v "Position"` style) until M6 replaces it with
JSON event lines. During a CSV dump the serial line belongs to the dump
alone (no status interleave, M3 D14).

## 4. Force reads right after cal_apply / boot are filter artifacts

The low-pass filter (alpha 0.18 at 10 Hz) is reset to 0.0 by
`cal_apply` success AND by boot (`LoadCell::begin` tares). Immediately
afterwards `force` prints 0.00 N no matter what is hanging. Converged
reading after ~3-4 s (0.82^n decay). Always re-read before judging.

## 5. Boot auto-tare = free two-sided calibration check

The boot sequence tares whatever load is on the cell at power-up. That
turns a reboot into a verification instrument (used for #48 evidence):

1. hang the known mass, reboot (EN button + background listener)
2. boot log must show `HX711 bereit. Kalibrierfaktor geladen: <factor>`
   and `CHECK id=NVS result=OK meas=<factor>` (persistence proof)
3. remove the mass -> `force` reads the NEGATIVE of the mass
   (2.465 kg -> -24.10 N vs -24.17 N expected, 0.3%)

## 6. Boot capture procedure

Background listener (safe-open, 60-120 s window) writing timestamped
lines to a file, THEN press EN. Two people/two steps, one terminal -
the reset happens while the listener already runs, or the banner is
lost. The listener must own the port: close other serial connections
(including `pio device monitor`) first.
