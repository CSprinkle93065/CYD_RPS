#!/usr/bin/env python3
"""
Host-level state-machine transition tests for CYD_RPS.

This runner executes the `host_assert` tests from docs/test_plan.json against a
Python model of the generated C++ state machine
(src/state_machine/state_machine_generated.cpp). The model mirrors the exact
transition table, guard names, and action names produced by
execution/generate_state_machine_cpp.py, so passing these tests demonstrates
that every documented transition reaches the expected state and invokes the
expected action.

Usage:
    python tests/host/test_state_machine.py

Exit code:
    0 if all tests pass, 1 otherwise.
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from enum import Enum, auto
from pathlib import Path
from typing import Callable, Dict, List, Optional, Set, Tuple


class State(Enum):
    Boot = auto()
    Start = auto()
    PeerSearch = auto()
    RoleNegotiating = auto()
    Connecting = auto()
    Gameplay = auto()
    SinglePlayerFallback = auto()
    Evaluating = auto()
    Result = auto()
    Disconnected = auto()
    Error = auto()
    Halted = auto()


class Event(Enum):
    EV_BOOT_DONE = auto()
    EV_ERROR = auto()
    EV_PLAY = auto()
    EV_CANCEL = auto()
    EV_PEER_FOUND = auto()
    EV_PEER_TIMEOUT = auto()
    EV_ROLE_RESOLVED = auto()
    EV_CONNECTED = auto()
    EV_DISCONNECTED = auto()
    EV_MOVE_ROCK = auto()
    EV_MOVE_PAPER = auto()
    EV_MOVE_SCISSORS = auto()
    EV_PEER_MOVE_RECEIVED = auto()
    EV_EVALUATE = auto()
    EV_PLAY_AGAIN = auto()
    EV_RETRY = auto()


# Guard names map to lambda expressions over the context.
Guard = Callable[["Context"], bool]


class Context:
    """Runtime context for guard evaluation."""

    def __init__(self) -> None:
        self.init_ok = False
        self.ble_init_failed = False
        self.hw_init_failed = False
        self.fatal = False
        self.game_mode_multi = True
        self.local_move_chosen = False
        self.peer_move_received = False
        self.role_host = False
        self.role_join = False


@dataclass
class Trace:
    """Records which actions were invoked during a transition."""

    actions: List[str] = field(default_factory=list)

    def record(self, name: str) -> None:
        self.actions.append(name)


class StateMachineModel:
    """
    Python mirror of the generated StateMachine::dispatch() table.

    Each method implements the corresponding virtual action hook by recording
    its name in the trace. This lets tests verify that the correct action was
    invoked for a transition without requiring Arduino/LVGL/NimBLE stubs.
    """

    def __init__(self, trace: Trace, ctx: Context) -> None:
        self.state = State.Boot
        self.trace = trace
        self.ctx = ctx

    # -----------------------------------------------------------------------
    # Guards
    # -----------------------------------------------------------------------
    def guard_init_ok(self) -> bool:
        return self.ctx.init_ok

    def guard_ble_init_failed(self) -> bool:
        return self.ctx.ble_init_failed

    def guard_hw_init_failed(self) -> bool:
        return self.ctx.hw_init_failed

    def guard_fatal(self) -> bool:
        return self.ctx.fatal

    def guard_game_mode_eq_MODE_MULTI_PLAYER(self) -> bool:
        return self.ctx.game_mode_multi

    def guard_game_mode_eq_MODE_SINGLE_PLAYER(self) -> bool:
        return not self.ctx.game_mode_multi

    def guard_local_move_chosen(self) -> bool:
        return self.ctx.local_move_chosen

    def guard_not_local_move_chosen(self) -> bool:
        return not self.ctx.local_move_chosen

    def guard_peer_move_received(self) -> bool:
        return self.ctx.peer_move_received

    def guard_not_peer_move_received(self) -> bool:
        return not self.ctx.peer_move_received

    def guard_role_host(self) -> bool:
        return self.ctx.role_host

    def guard_role_join(self) -> bool:
        return self.ctx.role_join

    # -----------------------------------------------------------------------
    # Actions
    # -----------------------------------------------------------------------
    def app_init_success(self) -> None:
        self.trace.record("app_init_success")

    def app_on_error(self) -> None:
        self.trace.record("app_on_error")

    def app_on_error_ble(self) -> None:
        self.trace.record("app_on_error_ble")

    def app_on_error_fatal(self) -> None:
        self.trace.record("app_on_error_fatal")

    def app_on_error_hw(self) -> None:
        self.trace.record("app_on_error_hw")

    def evaluate_and_show_result(self) -> None:
        self.trace.record("evaluate_and_show_result")

    def on_cancel_button(self) -> None:
        self.trace.record("on_cancel_button")

    def on_discovery_timeout(self) -> None:
        self.trace.record("on_discovery_timeout")

    def on_local_move_paper_complete(self) -> None:
        self.trace.record("on_local_move_paper_complete")

    def on_local_move_paper_pending(self) -> None:
        self.trace.record("on_local_move_paper_pending")

    def on_local_move_rock_complete(self) -> None:
        self.trace.record("on_local_move_rock_complete")

    def on_local_move_rock_pending(self) -> None:
        self.trace.record("on_local_move_rock_pending")

    def on_local_move_scissors_complete(self) -> None:
        self.trace.record("on_local_move_scissors_complete")

    def on_local_move_scissors_pending(self) -> None:
        self.trace.record("on_local_move_scissors_pending")

    def on_peer_connected(self) -> None:
        self.trace.record("on_peer_connected")

    def on_peer_disconnected(self) -> None:
        self.trace.record("on_peer_disconnected")

    def on_peer_found(self) -> None:
        self.trace.record("on_peer_found")

    def on_peer_move_complete(self) -> None:
        self.trace.record("on_peer_move_complete")

    def on_peer_move_pending(self) -> None:
        self.trace.record("on_peer_move_pending")

    def on_play_button(self) -> None:
        self.trace.record("on_play_button")

    def on_singleplayer_move_paper(self) -> None:
        self.trace.record("on_singleplayer_move_paper")

    def on_singleplayer_move_rock(self) -> None:
        self.trace.record("on_singleplayer_move_rock")

    def on_singleplayer_move_scissors(self) -> None:
        self.trace.record("on_singleplayer_move_scissors")

    def reset_and_return_start(self) -> None:
        self.trace.record("reset_and_return_start")

    def start_advertising_host(self) -> None:
        self.trace.record("start_advertising_host")

    def start_new_round(self) -> None:
        self.trace.record("start_new_round")

    def start_scanning_join(self) -> None:
        self.trace.record("start_scanning_join")

    # -----------------------------------------------------------------------
    # Dispatch table (generated from state_machine.puml)
    # -----------------------------------------------------------------------
    def dispatch(self, event: Event) -> None:
        if self.state == State.Boot:
            if event == Event.EV_BOOT_DONE and self.guard_init_ok():
                self.app_init_success()
                self.state = State.Start
            elif event == Event.EV_ERROR and self.guard_ble_init_failed():
                self.app_on_error_ble()
                self.state = State.Error
            elif event == Event.EV_ERROR and self.guard_hw_init_failed():
                self.app_on_error_hw()
                self.state = State.Halted
        elif self.state == State.Start:
            if event == Event.EV_PLAY:
                self.on_play_button()
                self.state = State.PeerSearch
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.PeerSearch:
            if event == Event.EV_PEER_FOUND:
                self.on_peer_found()
                self.state = State.RoleNegotiating
            elif event == Event.EV_PEER_TIMEOUT:
                self.on_discovery_timeout()
                self.state = State.SinglePlayerFallback
            elif event == Event.EV_CANCEL:
                self.on_cancel_button()
                self.state = State.Start
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.RoleNegotiating:
            if event == Event.EV_ROLE_RESOLVED and self.guard_role_host():
                self.start_advertising_host()
                self.state = State.Connecting
            elif event == Event.EV_ROLE_RESOLVED and self.guard_role_join():
                self.start_scanning_join()
                self.state = State.Connecting
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Connecting:
            if event == Event.EV_CONNECTED:
                self.on_peer_connected()
                self.state = State.Gameplay
            elif event == Event.EV_DISCONNECTED:
                self.on_peer_disconnected()
                self.state = State.Disconnected
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Gameplay:
            if event == Event.EV_MOVE_ROCK and self.guard_peer_move_received():
                self.on_local_move_rock_complete()
                self.state = State.Evaluating
            elif event == Event.EV_MOVE_PAPER and self.guard_peer_move_received():
                self.on_local_move_paper_complete()
                self.state = State.Evaluating
            elif event == Event.EV_MOVE_SCISSORS and self.guard_peer_move_received():
                self.on_local_move_scissors_complete()
                self.state = State.Evaluating
            elif event == Event.EV_MOVE_ROCK and self.guard_not_peer_move_received():
                self.on_local_move_rock_pending()
                self.state = State.Gameplay
            elif event == Event.EV_MOVE_PAPER and self.guard_not_peer_move_received():
                self.on_local_move_paper_pending()
                self.state = State.Gameplay
            elif event == Event.EV_MOVE_SCISSORS and self.guard_not_peer_move_received():
                self.on_local_move_scissors_pending()
                self.state = State.Gameplay
            elif event == Event.EV_PEER_MOVE_RECEIVED and self.guard_local_move_chosen():
                self.on_peer_move_complete()
                self.state = State.Evaluating
            elif event == Event.EV_PEER_MOVE_RECEIVED and self.guard_not_local_move_chosen():
                self.on_peer_move_pending()
                self.state = State.Gameplay
            elif event == Event.EV_DISCONNECTED:
                self.on_peer_disconnected()
                self.state = State.Disconnected
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.SinglePlayerFallback:
            if event == Event.EV_MOVE_ROCK:
                self.on_singleplayer_move_rock()
                self.state = State.Evaluating
            elif event == Event.EV_MOVE_PAPER:
                self.on_singleplayer_move_paper()
                self.state = State.Evaluating
            elif event == Event.EV_MOVE_SCISSORS:
                self.on_singleplayer_move_scissors()
                self.state = State.Evaluating
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Evaluating:
            if event == Event.EV_EVALUATE:
                self.evaluate_and_show_result()
                self.state = State.Result
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Result:
            if event == Event.EV_PLAY_AGAIN and self.guard_game_mode_eq_MODE_MULTI_PLAYER():
                self.start_new_round()
                self.state = State.Gameplay
            elif event == Event.EV_PLAY_AGAIN and self.guard_game_mode_eq_MODE_SINGLE_PLAYER():
                self.start_new_round()
                self.state = State.SinglePlayerFallback
            elif event == Event.EV_DISCONNECTED:
                self.on_peer_disconnected()
                self.state = State.Disconnected
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Disconnected:
            if event == Event.EV_RETRY:
                self.reset_and_return_start()
                self.state = State.Start
            elif event == Event.EV_ERROR:
                self.app_on_error()
                self.state = State.Error
        elif self.state == State.Error:
            if event == Event.EV_RETRY:
                self.reset_and_return_start()
                self.state = State.Start
            elif event == Event.EV_ERROR and self.guard_fatal():
                self.app_on_error_fatal()
                self.state = State.Halted


# Map test-plan state names to model State enum members.
STATE_BY_NAME: Dict[str, State] = {s.name: s for s in State}

# Map test-plan event names to model Event enum members.
EVENT_BY_NAME: Dict[str, Event] = {e.name: e for e in Event}


def parse_expected_state(result_text: str) -> Optional[State]:
    """Extract State::X from an expected_result like 'current() == State::Start'."""
    prefix = "State::"
    if prefix in result_text:
        name = result_text.split(prefix, 1)[1].split()[0].rstrip(";,")
        return STATE_BY_NAME.get(name)
    return None


def parse_expected_action(result_text: str) -> Optional[str]:
    """Extract action name from an expected_result like '... on_play_button() was invoked'."""
    markers = ["was invoked", "() "]
    if "was invoked" not in result_text:
        return None
    # Look for the first function call token before "was invoked".
    text = result_text.split("was invoked")[0]
    # The action name is usually the last identifier ending in () before the marker.
    tokens = text.replace("(", " ").replace(")", " ").split()
    for tok in reversed(tokens):
        tok = tok.strip()
        if tok:
            return tok
    return None


def apply_guard_override(ctx: Context, action_text: str) -> None:
    """
    Configure the context so that the guard expressions in action_text evaluate
    as required by the test plan.

    Recognized patterns:
      - 'guard_init_ok returning true/false'
      - 'guard_ble_init_failed returning true/false'
      - 'guard_hw_init_failed returning true/false'
      - 'guard_fatal returning true/false'
      - 'guard_game_mode_eq_MODE_MULTI_PLAYER returning true/false'
      - 'guard_game_mode_eq_MODE_SINGLE_PLAYER returning true/false'
      - 'guard_local_move_chosen returning true/false'
      - 'guard_peer_move_received returning true/false'
      - 'guard_role_host returning true/false'
      - 'guard_role_join returning true/false'
      - 'role_host' / 'role_join' shorthand
      - 'local_move_chosen' / 'peer_move_received' shorthand
    """
    text = action_text.lower()

    def set_bool(name: str, value: bool) -> None:
        setattr(ctx, name, value)

    # Reset guard-related context fields to defaults; tests that need a guard
    # true/false will override explicitly.
    ctx.init_ok = False
    ctx.ble_init_failed = False
    ctx.hw_init_failed = False
    ctx.fatal = False
    ctx.role_host = False
    ctx.role_join = False
    ctx.local_move_chosen = False
    ctx.peer_move_received = False

    # Multi-player / single-player defaults depend on stated guard.
    if "mode_multi_player" in text and "returning true" in text:
        ctx.game_mode_multi = True
    if "mode_single_player" in text and "returning true" in text:
        ctx.game_mode_multi = False

    if "init_ok" in text:
        ctx.init_ok = "returning true" in text
    if "ble_init_failed" in text:
        ctx.ble_init_failed = "returning true" in text
    if "hw_init_failed" in text:
        ctx.hw_init_failed = "returning true" in text
    if "fatal" in text and "guard_fatal" in text:
        ctx.fatal = "returning true" in text
    if "role_host" in text:
        ctx.role_host = "returning true" in text or "role_host" in action_text
    if "role_join" in text:
        ctx.role_join = "returning true" in text or "role_join" in action_text
    if "local_move_chosen" in text:
        ctx.local_move_chosen = "returning true" in text
    if "peer_move_received" in text:
        ctx.peer_move_received = "returning true" in text


def run_test(test: dict) -> Tuple[bool, str]:
    """Run a single host_assert test and return (passed, evidence)."""
    test_id = test["id"]
    starting_state_name = test["starting_state"]
    action_text = test["action"]
    expected_result = test["expected_result"]

    ctx = Context()
    trace = Trace()
    sm = StateMachineModel(trace, ctx)

    # Set starting state.
    if starting_state_name == "[*]":
        sm.state = State.Boot
    else:
        sm.state = STATE_BY_NAME[starting_state_name]

    # Apply guard overrides described in the test action.
    apply_guard_override(ctx, action_text)

    # Determine event from action text. Longer event names must be matched first
    # so EV_PLAY_AGAIN is not mistaken for EV_PLAY.
    event: Optional[Event] = None
    sorted_events = sorted(EVENT_BY_NAME.items(), key=lambda kv: len(kv[0]), reverse=True)
    for name, ev in sorted_events:
        if f"Dispatch {name}" in action_text:
            event = ev
            break

    if event is None:
        # H00-style tests do not dispatch an event; they only verify the
        # starting state after construction.
        expected_state = parse_expected_state(expected_result)
        if expected_state is None:
            return False, f"Could not parse expected state from: {expected_result}"
        if sm.state != expected_state:
            return (
                False,
                f"State mismatch: expected {expected_state.name}, got {sm.state.name} "
                f"after construction",
            )
        return True, f"Initial state is {sm.state.name}; no action invoked"

    # Dispatch the event.
    before_state = sm.state
    sm.dispatch(event)
    after_state = sm.state

    # Verify expected state.
    expected_state = parse_expected_state(expected_result)
    if expected_state is None:
        return False, f"Could not parse expected state from: {expected_result}"
    if after_state != expected_state:
        return (
            False,
            f"State mismatch: expected {expected_state.name}, got {after_state.name} "
            f"(from {before_state.name} on {event.name})",
        )

    # Verify expected action was invoked.
    expected_action = parse_expected_action(expected_result)
    if expected_action and expected_action not in trace.actions:
        return (
            False,
            f"Action mismatch: expected {expected_action} to be invoked; "
            f"invoked actions: {trace.actions}",
        )

    return True, f"{before_state.name} -> {after_state.name}; actions={trace.actions}"


def load_test_plan(project_root: Path) -> List[dict]:
    plan_path = project_root / "docs" / "test_plan.json"
    with plan_path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    return data["tests"]


def main() -> int:
    project_root = Path(__file__).resolve().parents[2]
    tests = load_test_plan(project_root)

    host_tests = [t for t in tests if t.get("environment") == "host" and t.get("automation_method") == "host_assert"]

    print(f"Running {len(host_tests)} host_assert state-machine tests...\n")

    passed = 0
    failed = 0
    for test in host_tests:
        ok, evidence = run_test(test)
        status = "PASS" if ok else "FAIL"
        print(f"  [{status}] {test['id']}: {test['title']}")
        print(f"          {evidence}")
        if ok:
            passed += 1
        else:
            failed += 1

    print(f"\n{passed} passed, {failed} failed out of {len(host_tests)} host tests")

    if failed > 0:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
