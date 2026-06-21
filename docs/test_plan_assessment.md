# Assessment: Stage 8 — Test Critic (Re-review)

**Project:** CYD_RPS  
**Version:** 0.2.0  
**Workflow ID:** wvc_20260617_171607  
**Revision Type:** minor_revision  
**Target Environment:** hardware (Wokwi simulation fallback)  
**Assessment Date:** 2026-06-21

**Verdict:** GO

---

## Gate Results Summary

| Gate | Result | Summary |
|------|--------|---------|
| G8.1 | PASS | All 50 state-machine transitions are covered by test cases or documented in `untestable_transitions`. |
| G8.2 | PASS | Host tests exercise the real state machine; test doubles are only implied for hardware-dependent actions. |
| G8.3 | PASS | Expected results are observable and unambiguous. |
| G8.4 | PASS | Hardware-present tests now assert on real data types/values (state enums, mode/role/move enums, score structs, BLE flags, timer values). |
| G8.5 | PASS | Every state-dependent test includes an explicit `reset_action`. |
| G8.6 | PASS | Target tests T10–T12 are now explicitly tagged `manual`; no hidden manual tests remain. |
| G8.7 | PASS | Every Wokwi test includes a concrete `serial_expect` assertion. |

---

## Re-review Context

The previous Test Critic run failed on:

- **G8.4:** Hardware-present tests asserted primarily on serial strings and UI text rather than real data types/values.
- **G8.6:** Target tests T10, T11, and T12 required human touch interaction but were classified as `automation_method: serial_expect` and lacked the `manual` tag.

The Test Planner revised `docs/test_plan.json` to address both findings. This assessment verifies those revisions.

---

## G8.1 — Every transition appears in a test case or in `untestable_transitions`

**Result:** PASS

All 50 transitions in `docs/state_machine.puml` (including the initial `[*] --> Boot` pseudo-state) are covered by at least one test case:

- Boot transitions: H00–H03
- Start transitions: H04–H08, H44-* (serial), M01–M02 (manual), W02–W03, T03–T04
- HostWait transitions: H09–H14, H50, W07, T09, T12
- HostTimeoutDialog transitions: H15–H18, W08–W09, T10–T11
- Joining transitions: H19–H22
- Gameplay transitions: H23-* – H30, H44-* (serial), W04–W06, T05–T08, M03, M08
- Evaluating transitions: H31–H33, W04, T05
- Result transitions: H34–H37, H44-AGAIN, W05, T06–T07, M05, M09
- Disconnected transitions: H38–H40, M10, M13
- Error transitions: H41–H43, M10

The `untestable_transitions` section documents environment-specific limitations for Wokwi (NimBLE not supported and touch controller not usable in Wokwi CLI). Several transitions listed there are also exercised by host unit tests, which is acceptable because the gate only requires each transition to appear in a test case **or** in `untestable_transitions`.

---

## G8.2 — No inappropriate mocks, stubs, or dummies

**Result:** PASS

- Host unit tests dispatch events directly to a real `app::StateMachine` v0.2.0 instance and assert on `current()` and action invocation.
- Serial parser tests (H44-*) feed real input strings to `serial_process_input()` / `serial_command_dispatch()` and assert on the resulting event/state.
- Any test doubles implied by the plan are limited to hardware-dependent actions (`esp_restart()`, BLE initialization, etc.) that cannot be exercised on the host or in Wokwi.

No test replaces state-machine logic, guards, or serial parsing with a mock or stub where the production software could otherwise be exercised.

---

## G8.3 — Every `expected_result` is observable and unambiguous

**Result:** PASS

- Host tests assert on concrete states (`current() == State::<Name>`) and concrete action invocations (`<action>() was invoked`).
- Scoreboard tests assert on explicit counter values (`wins == 1`, `losses == 1`, `draws == 1`, all zeros on boot).
- Target and manual tests assert on concrete data types and values (`game_state_t == STATE_*`, `game_mode_t == MODE_*`, `role_t == ROLE_*`, `local_move_t == MOVE_*`, `session_score_t` counters, `outcome_t`, `ble_connected`, timer values).
- Wokwi tests assert on explicit serial strings (`CYD_RPS setup done`, `STATE: HostWait`, `MODE: SINGLE_PLAYER`, `STATE: Result`, etc.).
- Manual tests describe observable screen contents, serial markers, and the concrete data values expected.

Minor observations:
- H47 expected result uses the word "reflects" rather than an exact score tuple, though the intent is clear in context.
- H44-* expected results refer to an "equivalent button action invoked," which is slightly indirect but maps unambiguously to the named state-machine action.

These do not block the gate.

---

## G8.4 — Hardware-present tests assert on real data types/values; hardware-absent tests assert on UI text, state names, or serial output

**Result:** PASS

Hardware-present tests are defined as tests with `hardware_required: true`: the target tests (T01–T12) and the manual tests (M01–M14). The revised plan now asserts on real data types/values in these tests, for example:

- **T01:** `game_state_t == STATE_START`; `ui_ready` flag true; MAC-derived device ID.
- **T02:** `ble_init()` returns false; `game_state_t == STATE_ERROR`; BLE error code.
- **T03:** `game_state_t == STATE_HOST_WAIT`; `role_t == ROLE_HOST`; timeout timer started.
- **T04:** `game_state_t == STATE_GAMEPLAY`; `game_mode_t == MODE_SINGLE_PLAYER`.
- **T05:** `local_move_t == MOVE_ROCK`; `game_state_t` transitions through `STATE_EVALUATING` to `STATE_RESULT`; `outcome_t` is a valid outcome.
- **T06:** `session_score_t` counters match cumulative `outcome_t`.
- **T07:** `game_state_t == STATE_GAMEPLAY`; `local_move_t == MOVE_NONE`; `peer_move_t == MOVE_NONE`; `session_score_t` unchanged.
- **T09:** `game_state_t == STATE_HOST_TIMEOUT_DIALOG`; `host_wait_timer elapsed == 15 s`.
- **T10–T12:** State enums, mode enums, timer values, and beacon/scan status.

Manual tests (M01–M14) likewise assert on concrete data types:
- `game_state_t`, `game_mode_t`, `role_t`, `local_move_t`, `peer_move_t`, `session_score_t`, `outcome_t`, `ble_connected`, peer MAC reachability, and `ESP.restart()` invocation.

Hardware-absent tests (host and Wokwi) continue to assert on serial strings, state names, and UI markers, which is appropriate for those environments. Serial strings in target/manual tests are now secondary or corroborating assertions, not the primary expected result.

---

## G8.5 — Tests ordered for sequential execution or include explicit reset actions

**Result:** PASS

Every state-dependent test includes an explicit `reset_action` describing how to return to or reach the required `starting_state`, for example:

- "Construct fresh StateMachine"
- "Reach Start by dispatching EV_BOOT_DONE [init_ok] from Boot"
- "Reach Gameplay by dispatching EV_SOLO from Start"
- "Reach HostWait by dispatching EV_HOST_GAME from Start"

The plan therefore supports sequential execution without requiring undocumented manual resets between tests.

---

## G8.6 — No hidden manual tests

**Result:** PASS

Any test requiring human interaction is now explicitly tagged `manual` and uses `automation_method: manual` with `environment: manual`.

The previously non-compliant target tests have been corrected:

- **T10** — "Host timeout dialog Retry restarts HostWait on target hardware"  
  Action: "Tap the Retry button in the timeout dialog"  
  Now: `automation_method: manual`, `environment: manual`, tags include `manual` and `touch`.

- **T11** — "Host timeout dialog Solo enters single-player Gameplay on target hardware"  
  Action: "Tap the Solo button in the timeout dialog"  
  Now: `automation_method: manual`, `environment: manual`, tags include `manual` and `touch`.

- **T12** — "Cancel hosting returns to Start on target hardware"  
  Action: "Tap the Cancel button on SCR_HostWait"  
  Now: `automation_method: manual`, `environment: manual`, tags include `manual` and `touch`.

All M01–M14 manual tests remain correctly tagged. No hidden manual tests remain.

---

## G8.7 — Every Wokwi test includes expected serial output or diagram.json probe assertions

**Result:** PASS

All nine Wokwi tests (W01–W09) include a concrete `serial_expect` value:

| Test | `serial_expect` |
|------|-----------------|
| W01 | `CYD_RPS setup done` |
| W02 | `STATE: HostWait` |
| W03 | `MODE: SINGLE_PLAYER` |
| W04 | `STATE: Result` |
| W05 | `STATE: Gameplay` |
| W06 | `CYD_RPS setup done` |
| W07 | `STATE: HostTimeoutDialog` |
| W08 | `STATE: HostWait` |
| W09 | `MODE: SINGLE_PLAYER` |

No Wokwi test lacks an explicit serial-output assertion.

---

## Summary

The Test Planner's revisions satisfactorily address the previous NO-GO findings:

- **G8.4:** Target and manual hardware-present tests now assert on real data types/values (state/mode/role/move enums, score structs, BLE flags, timer values) rather than relying solely on serial strings or UI text.
- **G8.6:** T10, T11, and T12 are now correctly classified as manual tests with the `manual` tag and `manual` automation method.

All seven quality gates pass.

**Verdict: GO.** The test plan is approved for implementation.
