#pragma once

// #158: plain (non-runner) move endstop guard - the manual-move analogue
// of the homing/pull endstop rules (M3 FR-001 seeks, D26 pull limit).
// Pure and host-tested (test/test_manual_move_guard.cpp): App::loop
// feeds the in-flight move direction (Motion::moveDirection()) and the
// debounced endstop levels (Endstops::bottom()/top()); a true verdict
// means forceStop().
//
// Semantics: LEVEL-triggered in the direction of travel only - a pressed
// bottom switch stops downward moves but never an upward clearing move
// from the datum (a homed carriage rests with E1 closed), and vice versa
// for the top switch. Direction 0 (no plain move in flight) never stops.
inline bool manualMoveBlockedByEndstop(int moveDir, bool endstopBottom,
                                       bool endstopTop) {
  return (moveDir < 0 && endstopBottom) || (moveDir > 0 && endstopTop);
}
