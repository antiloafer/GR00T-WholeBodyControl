import numpy as np

from gear_sonic.utils.teleop.solver.hand.brainco_revo2_mapping import (
    GESTURES,
    controller_pair_to_revo2,
    controller_to_revo2,
    gesture_name,
)


def test_gesture_selection():
    assert gesture_name(0.0, 0.0) == "open"
    assert gesture_name(0.6, 0.0) == "pinch_card"
    assert gesture_name(0.0, 0.6) == "grasp_handle"
    assert gesture_name(0.6, 0.6) == "fist"


def test_targets_are_six_dof_and_normalized():
    for target in GESTURES.values():
        assert target.shape == (6,)
        assert target.dtype == np.float32
        assert np.all(target >= 0.0)
        assert np.all(target <= 0.85)


def test_mapping_returns_copies():
    first = controller_to_revo2(0.0, 0.0)
    first[0] = 1.0
    second = controller_to_revo2(0.0, 0.0)
    assert second[0] == GESTURES["open"][0]


def test_pair_shape():
    left, right = controller_pair_to_revo2(1.0, 0.0, 0.0, 1.0)
    assert left.shape == (1, 6)
    assert right.shape == (1, 6)
    np.testing.assert_array_equal(left[0], GESTURES["pinch_card"])
    np.testing.assert_array_equal(right[0], GESTURES["grasp_handle"])
