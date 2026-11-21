"""Safe, deterministic PICO controller mapping for BrainCo Revo2 hands.

Revo2 commands are normalized in this order:
thumb flex, thumb auxiliary, index, middle, ring, pinky.
"""

from __future__ import annotations

import numpy as np


REVO2_DOF = 6

# Conservative starting poses. They intentionally stop short of full closure;
# tune them on the actual card and door handle after the control chain is stable.
GESTURES = {
    "open": np.array([0.40, 0.40, 0.05, 0.05, 0.05, 0.05], dtype=np.float32),
    "pinch_card": np.array([0.55, 0.65, 0.62, 0.05, 0.05, 0.05], dtype=np.float32),
    "grasp_handle": np.array([0.55, 0.52, 0.55, 0.68, 0.68, 0.65], dtype=np.float32),
    "fist": np.array([0.55, 0.52, 0.78, 0.78, 0.78, 0.75], dtype=np.float32),
}


def gesture_name(trigger: float, grip: float, threshold: float = 0.5) -> str:
    """Select one of four fixed gestures from a controller's analog buttons."""
    trigger_pressed = float(trigger) > threshold
    grip_pressed = float(grip) > threshold
    if trigger_pressed and grip_pressed:
        return "fist"
    if trigger_pressed:
        return "pinch_card"
    if grip_pressed:
        return "grasp_handle"
    return "open"


def controller_to_revo2(trigger: float, grip: float) -> np.ndarray:
    """Return one normalized six-joint Revo2 target."""
    return GESTURES[gesture_name(trigger, grip)].copy()


def controller_pair_to_revo2(
    left_trigger: float,
    left_grip: float,
    right_trigger: float,
    right_grip: float,
) -> tuple[np.ndarray, np.ndarray]:
    """Return left and right Revo2 targets with shape ``(1, 6)``."""
    left = controller_to_revo2(left_trigger, left_grip).reshape(1, REVO2_DOF)
    right = controller_to_revo2(right_trigger, right_grip).reshape(1, REVO2_DOF)
    return left, right

