# LED Status Language (GPIO2)

The DevKit onboard LED is the machine's glance channel: blink patterns
encode the machine state, plus short overlays for hardware events. The
authoritative log remains serial + web; the LED is for reading the
machine from across the bench without a screen. Spec:
`specs/M8-led-status/spec.md` (issue #128).

All patterns respect the flash-safety cap (max 2 flashes/s, WCAG 2.3.1
headroom) - enforced permanently by host test TC-101.

## Base patterns (machine state)

| State | Pattern | Brightness |
|-------|---------|------------|
| SelfTest (boot) | 1 long flash (600 ms), 2.1 s cycle | 100% |
| Idle | 1 short flash (150 ms) every 3 s (heartbeat) | 20% |
| Homing* | 2 short flashes, 2.5 s cycle | 60% |
| Running (test)* | steady ON | 40% |
| Error | 3 short flashes, 3 s cycle | 100% |

\* Homing/Running went live with M3-T011/T012 (test runner + command surface).

## Event overlays (about 2 s, then back to the base pattern)

| Event | Pattern | When it fires |
|-------|---------|---------------|
| Endstop pressed (D25 bottom / D26 top) | 1 long flash, twice | machine not in Error; since M8-T002 |
| Sample broke | 2 short flashes, twice | while Running (M8-T003, #154) |
| Calibration applied | short + long, twice | in Idle (`cal_apply`, M8-T003, #154) |

Brightness is gamma-corrected PWM (level^2.2), 4 steps only - brightness
is emphasis, never a data value.

## Reading notes

- Long flash = mechanical (endstop touch). Short flashes = counts matter.
- In Error the LED shows only the Error pattern; nothing else preempts it
  (reset/reboot is the recovery per M1 FR-009).
- Endstop edges are one overlay per press (edge-triggered; releasing a
  switch shows nothing).
