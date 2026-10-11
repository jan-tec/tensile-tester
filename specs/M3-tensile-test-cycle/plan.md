# Implementation Plan: M3 - Automated Tensile Test Cycle

Status: Draft | Spec: `spec.md` (FR-001..FR-008, TC-001..TC-007, SC-001..SC-003) |
Created: 2026-10-10 | Depends on: M1 (`ErrorCode`, `DeviceModel`, power monitor),
#71 (FastAccelStepper RMT), M2-T002 (#32, `src/Text.h` message tables)

## 1. Technical Context

- Motion is `FastAccelStepper` (RMT, hardware-timed, own stepper task). Position
  is motor steps; `spindleMmFromSteps()` converts (`SPINDLE_PITCH_MM_PER_REV`
  = 2 mm, 21968 steps/rev). One motor step = 9.1e-5 mm.
- Force comes from `LoadCell::poll()` at `kSampleIntervalMs` = 100 ms (10 Hz):
  one `scale_.read()` per poll, `measuredForceNewton` computed from it, then
  folded into the low-pass `forceNewton_` (alpha 0.18). FR-003 needs the
  **unfiltered** value, which is not exposed - `forceNewton_` is only ever the
  filtered one.
- GPIO 25/26 are genuinely unused (occupied: STEP 18, DIR 19, EN 5+23,
  HX711 32+33, rail 34). They are the DAC-capable pins, used here as plain
  `INPUT_PULLUP` digital inputs - no DAC anywhere in this design, so no conflict.
- Commands run through one policy, `Commands::execute()` (serial + web). Name
  matching is `FirmwareCore::commandFromName()`; value parsing is
  `parseCommandValue()` via a `String` adapter.
- `DeviceModel` is the **health** state machine (`Booting|SelfTest|Idle|Error`)
  and owns the stable fault code. Error already implies driver-off (M1 FR-009).
- `App::loop()` order: `webUi_.handleClient()` -> `loadcell_.poll()` ->
  `serialCli_.poll()` -> `powerSense_.poll()` -> Error-transition driver-off ->
  500 ms status line.
- No endstop code exists. GPIO 25/26 are free (used: STEP 18, DIR 19, EN 5+23,
  HX711 32+33, rail 34). Wiring per spec A2: COM+NC, idle LOW, pressed HIGH.
- `spec.md` A2 wiring (#76) is a **user hardware task**: all target-side
  behaviour in this plan is blocked for *verification*, not for *coding*.

## 2. Architecture

```
lib/FirmwareCore (pure C++, host-tested, no Arduino/String/PROGMEM)
  TestCycle.h    TestCycleModel    FR-002, TC-001   cycle FSM
  Homing.h       HomingModel       FR-001, FR-008, TC-006, TC-007  staged homing FSM
  BreakDetect.h  BreakDetector     FR-003, TC-002   arm/peak/drop verdict
  TestData.h     TestSample, SampleBuffer, formatCsvChunk, formatStatus   FR-004, FR-005, TC-004, TC-005
  TestConfig.h   TestConfig, clampTestConfig        FR-005 persistence contract
  ErrorCode.h    + 0x05xx endstop block (append only)                   FR-007
  FirmwareCore.h commandFromName: + Home, HomeTop, Test, Stop already   FR-005
                 exists, + parseCommandIndex() for "data <n>"
                 isMotionCommand(): + home/home top/test

src (hardware + policy, target-tested)
  Endstops.h/.cpp        GPIO25/26, INPUT_PULLUP, debounce          FR-001, FR-002, FR-007
  TestRunner.h/.cpp      orchestrates: homing/pull/tail over IMotion +
                         ILoadCell + Endstops, owns SampleBuffer,
                         ticks from App::loop                        FR-002, FR-004, FR-006
  IMotion                + startContinuous(int dir), + forceStop()   FR-001, FR-002
  LoadCell               + rawForceNewton() (pre-filter value)       FR-003
  Settings               + TestConfig load/save (namespace testmachine) FR-005
  Commands               + home | home top | test | testspeed | testarm |
                         testdrop | data [from] | testdata          FR-005
  App                    owns Endstops + TestRunner, ticks them      FR-002, FR-004
  Diagnostics            kMaxChecks 5 -> 7: CHECK ENDSTOP1/ENDSTOP2  FR-007
  Text.h                 + Msg rows for every new reply              FR-005 (see §8)
  WebUi                  UNCHANGED this milestone (spec NFR)
```

Flow (US1): `test` -> `TestRunner::startTest()` -> `TestCycleModel` Idle->Homing
-> `HomingModel` drives `IMotion` until `Endstops::bottom()` -> datum set,
position zeroed -> Pulling: continuous up at `testSpeed`, each `LoadCell` tick
pushes `{positionMm, rawForceN}` into `SampleBuffer` and feeds `BreakDetector`
-> `Broken` -> `IMotion::forceStop()` -> Tail (1 s, still sampling) -> Done ->
summary. `data` (US2) drains the buffer in chunks from the loop task. US3 paths:
seek timeout / instant trigger / stuck switch -> Error code 0x05xx ->
`DeviceModel` Error -> driver off; D26 during pull -> `Limit` (no code, guarded
stop); power FAIL (M1) -> `DeviceModel` Error -> cycle goes to Error, data kept.

## 3. Key Decisions

| # | Decision | Rationale |
|---|----------|-----------|
| D1 | The test cycle is a **second, orthogonal FSM** (`TestCycleModel`); `DeviceModel` stays the health state machine | M1 codes and the `/api/status` `state` field are stable contracts; re-using `Error` for a running test would conflate "healthy but busy" with "fault". A test fault still lands in `DeviceModel` through the normal event path |
| D2 | All decisions (FSM transitions, homing staging, break verdict, CSV, clamping) are pure `FirmwareCore`; `TestRunner` only actuates | House rule (AGENTS.md): state machines are host-tested synchronously, GPIO timing stays on target. Also makes TC-001/002/005/006/007 possible at all |
| D3 | Homing is model-driven, not `FastAccelStepper` homing | The library has no endstop-seek semantics for this wiring, and FR-001's clearance/instant-trigger/debounce rules must be host-testable |
| D4 | Debounce = N consecutive equal samples (`debounceTicks`, default 3 at the 1 kHz runner tick = 30 ms), inside `Endstops`, with the *accepted* flag consumed by the pure model | #81 asked for debounce; a pure model cannot see GPIO jitter, so the counter is hardware-side and its output (stable bool) is the pure model's input. Tune on the bench (KW4-3Z-3 is snap-action, so 30 ms is conservative) |
| D5 | `IMotion::forceStop()` maps to `FastAccelStepper::forceStop()` (instant, no decel ramp), `stop()` keeps the current decelerating semantics | FR-002(4) says forceStop semantics; the existing `stop` command must not change behavior (`stop` during a normal move stays a controlled decel) |
| D6 | `LoadCell::rawForceNewton()` returns the pre-filter value **snapshotted inside `poll()`**; `forceNewton()` stays the filtered display value. `ILoadCell::rawSample()` is *not* the reuse here: it issues a second blocking `scale_.read()`, which would consume an extra HX711 conversion and desynchronise the 10 Hz cadence | FR-003 requires the detector not to be slowed by the 0.18 low-pass; existing status/web output must not change; one conversion per 100 ms stays the sampling contract |
| D7 | `SampleBuffer` is a fixed static array (8192 x 8 B = 64 KB in BSS), no allocation | No `new`/`delete` anywhere in the firmware; TC-004 measures the remaining headroom instead of hoping the allocator copes |
| D8 | CSV is emitted by a **pure formatter** into a caller-provided buffer; the loop task writes at most `kCsvChunkBytes` (256 B) per tick, gated by `Serial.availableForWrite()` | TC-005 must test exact-once chunk resume without a serial port; the chunk gate keeps the loop responsive (NFR: dump must not block the motor task) |
| D9 | CSV uses `.` as the decimal separator always, and a `#`-prefixed header + end marker | The plot host is a spreadsheet/Python: a German locale comma would break parsing; `#` lines are ignorable by parsers and give the count/truncated flag in-band |
| D10 | Endstop fault codes go in a new `0x05xx` block, appended, never renumbered; the D26 travel-limit trip carries **no** code | `ErrorCode.h` stability rule; FR-002 explicitly makes the limit a guarded stop, not a fault |
| D11 | Cycle commands are exclusive at the `TestRunner` level (`active()` check) and the running cycle is untouched by a rejected command | FR-005; also keeps `stop` as the only interruptor, so a typo cannot silently abort a running test |
| D12 | `home top` never redefines the datum | FR-008 (position zero stays the bottom datum); it is a positioning utility, not a zeroing operation |
| D13 | New reply text becomes `Msg` rows in `src/Text.h`, serial only | M2 D8 (`Msg` is the single source of user-facing text). Consequence in §8 - the flash budget must be settled first |
| D14 | The periodic status line in `App::loop()` is **suppressed while a CSV dump is in flight** (`TestRunner::dumpInProgress()`), and restored when the dump completes or is interrupted | `App::loop()` prints `Commands::printPosition()` every 500 ms whenever the driver is enabled or the load cell is ready - which is always true right after a test. A 64 KB dump is ~256 chunk writes, so status lines would land inside the CSV stream and break the host parser (SC-002). The dump is the only consumer that needs exclusivity on the serial line |
| D15 | `data`, `testdata` and `pos` stay usable in the `Error` state; `home`, `home top` and `test` are rejected there | M1 FR-009 already rejects motion commands in Error and keeps readout usable - the retrieved data of a failed test is exactly what the operator needs (`isMotionCommand()` extension in T007 enforces this) |

## 4. Data Model

| Entity | Where | Fields | Notes |
|--------|-------|--------|-------|
| `TestSample` | `TestData.h` | `float positionMm, forceN` | 8 B, fixed 10 Hz cadence, index implicit (FR-004) |
| `SampleBuffer` | `TestData.h` | `samples_[8192]`, `count_`, `truncated_` | static storage; `push()` false when full |
| `TestConfig` | `TestConfig.h` | `testSpeedMmPerMin`, `armForceN`, `dropPercent`, `homingClearanceMm`, `homingSeekMmPerMin`, `homingTimeoutMs` | defaults: 12 mm/min, 50 N, 50 %, 2 mm, **0 = follow `testSpeedMmPerMin`**, 30 000 ms; clamped by `clampTestConfig()` (a stored 0 keeps the follow-me behaviour, FR-005) |
| `TestRun` (run record) | `TestRunner` | state, sample count, truncated, peak force, break displacement | one run in RAM, replaced by the next test (A3) |
| NVS keys (namespace `testmachine`) | `Settings` | `test_speed`, `test_arm`, `test_drop`, `home_clear`, `home_seek`, `home_tout` | floats + uint32; names <= 15 chars (Preferences limit); calibration keys untouched |

Position datum (FR-006): at the **stage-2 trigger** the carriage is touching the
endstop, so the datum is simply `IMotion::zero()` at that instant -
`positionMm()` then reads 0 at the stop face, and the post-touch back-off moves
into positive mm. No offset arithmetic is involved.

"Homed" is a `TestRunner` flag (`homed()`, false at boot, true after a
successful bottom homing; cleared by nothing else - a reset re-boots and clears
it). `pos` (FR-006) reads it through `Commands` and reports "unknown" (new `Msg`
row) while it is false; `home` and `test` set it themselves. It is deliberately
not stored in NVS: a datum is only valid for the power cycle in which the
endstop was actually touched (A2 broken-wire rule).

`homingSeekMmPerMin` is not an independent knob: its default is the current
`testSpeedMmPerMin` (one speed control for the operator, FR-005 clamps still
apply). If a separate homing speed is ever wanted, the spec gets an FR/assumption
first - the plan does not invent a second default silently.

## 5. Contracts (the interface T001-T007 pin RED-first)

```cpp
// BreakDetect.h - FR-003, TC-002
class BreakDetector {
 public:
  enum class Verdict { Waiting, Armed, Broken };
  void configure(float armForceN, float dropFraction);   // 50 N, 0.50 (A1)
  void reset();
  Verdict sample(float forceN);   // latching: once Broken, stays Broken
  bool armed() const;  float peakForceN() const;  uint32_t peakIndex() const;
};
// rules: no arm below threshold; peak tracked only while armed;
// Broken when force < dropFraction * peak; NaN/inf samples are ignored.

// Homing.h - FR-001, FR-008, TC-006, TC-007
// Bottom (datum) homing: BackOff -> SeekDown -> FinalSeek -> Done.
// Top positioning (FR-008): SeekUp -> BackOffDown -> Done, never a datum.
enum class HomePhase { Idle, BackOff, SeekDown, FinalSeek, SeekUp, BackOffDown,
                       Done, Fault };
enum class HomeFault { None, SeekTimeout, InstantTrigger, StuckSwitch };
class HomingModel {
 public:
  void configure(float clearanceMm, float finalSpeedFraction,
                 uint32_t timeoutMs, float oneStepMm);
  void start();                                   // -> BackOff (bottom datum)
  void startFromTop();                            // D26 hit during BackOff
  void startTopSeek();                            // FR-008 `home top`
  void tick(float positionMm, bool bottomStable, bool topStable,
            uint32_t elapsedMs);
  HomePhase phase() const;  HomeFault fault() const;
  bool motionWanted() const;  int direction() const;      // -1 down, +1 up
  float speedFraction() const;                            // 1.0 or finalSpeed
  bool backOffCappedByTopLimit() const;                    // FR-001 note
  bool datumSetThisRun() const;   // true only from a bottom FinalSeek trigger
};
// rules: trigger in SeekDown before `clearanceMm` travelled = InstantTrigger;
// trigger in FinalSeek before `clearanceMm` = StuckSwitch; timeout in
// SeekDown/FinalSeek/SeekUp = SeekTimeout; Done only from a FinalSeek trigger
// (datum) or a BackOffDown completion (top seek, no datum).
// SeekUp has no instant-trigger guard: unlike the datum seek it makes no
// promise about travel before the trigger, so an immediate D26 hit is a legal
// result (carriage already at the top), reported as the reached span.

// TestCycle.h - FR-002, TC-001
enum class TestState { Idle, Homing, Pulling, Tail, Done, Limit, Aborted, Error };
enum class TestEvent { Start, HomeDone, HomeFault, BreakDetected, TopLimitHit,
                       TailComplete, BufferFull, StopRequested, PowerFail,
                       Reset };
class TestCycleModel {
 public:
  TestState state() const;  bool active() const;   // Homing|Pulling|Tail
  bool dispatch(TestEvent);                        // false = invalid, no-op
  static const char* name(TestState);              // stable log/status names
};
```

Transition table (TC-001 pins every row, including the invalid ones):

| From | Event | To |
|------|-------|-----|
| Idle | Start | Homing |
| Homing | HomeDone | Pulling |
| Homing | HomeFault \| PowerFail \| StopRequested | Error \| Error \| Aborted |
| Pulling | BreakDetected | Tail |
| Pulling | BufferFull | Tail (`truncated` set by the runner) |
| Pulling | TopLimitHit | Limit |
| Pulling | PowerFail \| StopRequested | Error \| Aborted |
| Tail | TailComplete | Done |
| Tail | PowerFail \| StopRequested | Error \| Aborted |
| Done \| Limit \| Aborted | Start | Homing |
| Error | Reset | Idle |
| Idle | everything but Start | no-op (dispatch false) |

```cpp
// TestData.h - FR-004, FR-005, TC-004, TC-005
struct TestSample { float positionMm; float forceN; };
class SampleBuffer {
 public:
  static constexpr size_t CAPACITY = 8192;                 // 64 KB, TC-004
  bool push(const TestSample&);  size_t count() const;
  bool truncated() const;  void clear();  const TestSample& at(size_t) const;
};
// CSV contract: "# index,displacement_mm,force_N" header, then one
// "i,x,y" line per sample with '.' decimals, then "# END samples=<n>
// truncated=<0|1>". formatCsvChunk writes from fromIndex into buf and sets
// *nextIndex; a resumed dump (data <from-index>) replays nothing, drops nothing.
// WHOLE LINES ONLY: a chunk that would split a line stops before it and leaves
// *nextIndex at that line - otherwise a resumed dump would drop bytes and
// TC-005 exact-once would be untestable. A bufSize too small for one line
// writes 0 bytes and leaves *nextIndex unchanged (caller enlarges the chunk).
size_t formatCsvChunk(const SampleBuffer&, size_t fromIndex, char* buf,
                      size_t bufSize, size_t* nextIndex);
// `testdata` reply (FR-005) is built by a pure formatter too, so the
// state/count/capacity/peak/truncation text is host-tested.

// TestConfig.h - FR-005
TestConfig clampTestConfig(TestConfig);
// speed -> [MIN_MOVE_MM_PER_MIN, MAX_MOVE_MM_PER_MIN]; arm -> (0, 4903];
// drop -> [10, 90] %; clearance -> [0.5, 10] mm; seek -> 0 means "follow the
// test speed", otherwise the feed range; timeout -> [1 s, 300 s].
// Non-finite input -> that field's default.
```

Fault -> code mapping (FR-007, `ErrorCode.h` append-only; the runner translates,
the model stays code-free):

| Source | Code | Device effect |
|--------|------|---------------|
| `HomeFault::SeekTimeout` | `EndstopSeekTimeout` 0x0501 | Error, driver off (M1 FR-009) |
| `HomeFault::InstantTrigger` | `EndstopInstantTrigger` 0x0502 | Error, driver off |
| `HomeFault::StuckSwitch` | `EndstopStuckSwitch` 0x0503 | Error, driver off |
| boot: endstop reads HIGH | `EndstopPressedOrBroken` 0x0504 | WARN row only, never Error |
| D26 during pull | none | guarded stop, run marked `limit` (FR-002) |
| runtime power FAIL | `PowerLow` 0x0201 (existing) | Error, driver off, cycle -> Error |
| buffer full | none | pull stops, `truncated` flag (FR-004) |

`src/` contracts: `Endstops` (pins 25/26, `INPUT_PULLUP`, `poll()`,
`bottom()`, `top()`, `bootState()`), `IMotion::startContinuous(int)` +
`IMotion::forceStop()`, `LoadCell::rawForceNewton()`,
`Settings::loadTestConfig()/saveTestConfig()`, and `TestRunner` with
`startHome(bool topOnly)`, `startTest()`, `abort()`, `tick()`, `active()`,
`homed()` (FR-006 datum flag, §4), `dumpInProgress()` (D14), `samples()`,
`nextDumpIndex()` reset per run.

## 6. TDD mapping

| Task | Phase | RED artifact | TC |
|------|-------|--------------|-----|
| T001 | new logic | `test/test_break_detect.cpp` | TC-002 |
| T002 | new logic | `test/test_homing_model.cpp` | TC-006, TC-007 |
| T003 | new logic | `test/test_cycle_model.cpp` (full transition table) | TC-001 |
| T004 | new logic | `test/test_sample_data.cpp` (buffer + CSV chunk resume) | TC-004, TC-005 |
| T005 | new logic | `test/test_config.cpp` (clamps, defaults, non-finite) | FR-005 |
| T006 | new logic | `test/test_error_code.cpp` extended (new 0x05xx names) | FR-007 |
| T007 | new logic | `test/test_command_parser.cpp` extended (`home top` before `home`, `data <n>` index, `isMotionCommand`) | FR-005 |
| T008-T013 | hardware/policy | no host mock: target smoke matrix (on-target parts blocked on #76) | SC-001..SC-003 |
| T014 | verify | bench log + heap report | TC-003, TC-004 |

`TestRunner` gets no host unit test by design (AGENTS.md host/target boundary):
it is GPIO/timing glue over three host-tested models, verified by the T013/T014
bench matrix.

## 7. Bench verification matrix (T014, needs #76)

| Row | Setup | Expected |
|-----|-------|----------|
| B1 | `test` on a real sample | home -> pull -> break -> stop within the TC-003 bound; summary shows peak + break displacement (SC-001) |
| B2 | `data` after B1, then `data <mid>` | full CSV plots elastic region + peak + drop; resumed chunk has no duplicate/missing line (SC-002, TC-005) |
| B3 | endstop wire disconnected, `home` | WARN at boot (`CHECK ENDSTOP1`), seek timeout/instant-trigger -> 0x0501/0x0502, driver off (SC-003) |
| B4 | switch held closed at boot | `CHECK ENDSTOP1` WARN 0x0504, no Error |
| B5 | D26 tripped during pull (no sample / jam) | immediate stop, run marked `limit`, no machine code, data retrievable (SC-003) |
| B6 | 5 V rail pulled below 4.5 V during pull | cycle -> Error, driver off, code 0x0201, data retrievable (SC-003) |
| B7 | `test` while a test is running | rejected with the actionable message, running cycle unaffected (FR-005) |
| B8 | free heap at boot with the 64 KB BSS buffer + WiFi AP up | documented minimum, compared to the 148-226 KB reference in TC-004 |
| B9 | `data` while the driver is enabled / load cell ready (the 500 ms status printer is active) | the CSV stream contains **no** interleaved `Position [...]` lines (D14); after the dump the status printer resumes |

## 8. Cross-milestone dependencies

| Dependency | Effect on M3 |
|-----------|--------------|
| #76 endstop wiring (user hardware) | Blocks the on-target behaviour checks in T008-T013 and the whole T014 matrix; T001-T012 are host + compile work |
| M2-T003 (#33) routes replies through `msg(Msg)` | M3 adds reply text: if M3 lands first, its replies must already be `Msg` rows (D13), i.e. M3 depends on the `msg()` helper existing. Sequencing: land M2-T003 first, or M3 adds its rows plus a minimal `msg()` in the same PR |
| #37 string-table flash budget | The M2-T002 tables already use ~3872 B of the 4096 B budget; M3 adds an estimated 12-16 reply rows (~700-1000 B EN+DE). **The budget must be decided before M3 strings land** |
| #88 index tables by `Msg` | Adding rows is exactly the drift scenario #88 guards against; #88 should land before M3's string additions |
| M6 JSON serial API | M6 re-frames these same commands as JSON lines; the command *names* and reply semantics here are M6's input, so keep the reply text and the machine-readable parts separable (already required by D9/`testdata`) |
| M4 constant-force hold | Reuses `TestRunner`'s homing + safety path; keep `HomingModel`/`Endstops` free of test-cycle assumptions |

## 9. Risks

| Risk | Mitigation |
|------|-----------|
| Break detected too late -> over-travel damages fixture/sample | TC-003 computes the bound (30 mm/min machine cap, 2-sample latency = 0.1 mm + `forceStop()` no-decel) and the spec owner accepts it explicitly in T013; `testarm`/`testdrop` tunable per sample |
| `LoadCell` is polled from the loop task while the Web server and CSV dump run -> sample interval jitter | Sampling rides the existing 10 Hz `poll()`; the runner stamps the tick, it does not block on writes (D8). Jitter is documented, not corrected - the CSV index is nominal 10 Hz per FR-004 |
| 64 KB BSS + WiFi/WebServer memory pressure | TC-004/B8 measures free heap at boot; if < ~100 KB, reduce `CAPACITY` (the constant is the only change point) |
| Endstop bounce counted as a second trigger | D4 debounce + the FR-001 clearance guard; TC-007 pins both |
| NC wiring makes a broken wire look like a pressed switch | FR-007 boot WARN row + the instant-trigger rule (spec A2); never a phantom datum |
| M2/M3 string work collides on `Text.h` | §8 sequencing note; `Text.h` rows for M3 land in one task, after #88 |
| `home top` mistaken by an operator for a re-zero | FR-008 + the reply text states the datum is unchanged; the run report prints the reached span |
| **`WebUi::statusJson()` silently caps the selftest overall rating at 5 checks** (`CheckStatus statuses[5]`, `i < 5` in the gather loop, `src/WebUi.cpp:83-84`), while the emit loop runs to `count` | Pre-existing, latent until a 6th check exists - and T013 adds two. Fix in T013: size the array from the check set (or iterate directly into `overallStatus`), and bump `json.reserve(800)` with its "~740 B with 5 selftest checks" comment. Host-side this is invisible (`WebUi` is Arduino-only), so T013 must verify `overall` on the bench with all 7 rows |

## 10. Prior research (already recorded in `spec.md`)

| Topic | Where |
|-------|-------|
| Two-stage homing + debounce practice (GRBL) | spec FR-001, #81, #84 |
| YZC-516C capacity/sensitivity window for `testarm` default | spec A1 |
| KW4-3Z-3 switch model, COM+NC wiring, pulled-high logic | spec A2 (#76) |
| ISO 899-1 jolt-free approach | M4 spec (adjacent milestone), not M3 |
