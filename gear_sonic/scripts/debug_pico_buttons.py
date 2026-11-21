"""Live dump of Pico / XRoboToolkit controller buttons.

Uses the same XRT APIs as pico_manager_thread_server.py so you can
verify whether A/B/X/Y are actually being reported.

Usage (from repo root, teleop venv)::

    source .venv_teleop/bin/activate
    python gear_sonic/scripts/debug_pico_buttons.py

Controls:
    Ctrl-C to quit.

Mapping (from XRT pybind):
    A = right primary    B = right secondary
    X = left primary     Y = left secondary
"""

from __future__ import annotations

import subprocess
import sys
import time

try:
    import xrobotoolkit_sdk as xrt
except ImportError:
    print("ERROR: xrobotoolkit_sdk not installed. Activate .venv_teleop first.")
    sys.exit(1)


def _safe(fn, default=None):
    try:
        return fn()
    except Exception as e:
        return default if default is not None else f"ERR:{e}"


def _flag(pressed: bool, label: str) -> str:
    return f"[{label}]" if pressed else f" {label} "


def main() -> None:
    print("Starting XRoboToolkit PC service...")
    subprocess.Popen(["bash", "/opt/apps/roboticsservice/runService.sh"])
    xrt.init()
    print("SDK initialized. Waiting for headset / body data...\n")

    waited = 0
    while not xrt.is_body_data_available():
        print(f"  waiting for body data... ({waited}s)", end="\r")
        time.sleep(1)
        waited += 1
        if waited >= 30:
            print("\nWARNING: body data still not available after 30s.")
            print("Controller buttons may still work. Continuing anyway.\n")
            break
    else:
        print(f"Body data available after {waited}s.\n")

    print("Live button dump (press any button on the Pico controllers).")
    print("Expected combo: A+B+X+Y should print  START_COMBO  on rising edge.")
    print("-" * 88)

    prev_combo = False
    prev_ax = False
    prev_by = False
    last_print = ""

    try:
        while True:
            a = bool(_safe(xrt.get_A_button, False))
            b = bool(_safe(xrt.get_B_button, False))
            x = bool(_safe(xrt.get_X_button, False))
            y = bool(_safe(xrt.get_Y_button, False))

            left_menu = bool(_safe(xrt.get_left_menu_button, False))
            right_menu = bool(_safe(xrt.get_right_menu_button, False))
            left_click = bool(_safe(xrt.get_left_axis_click, False))
            right_click = bool(_safe(xrt.get_right_axis_click, False))

            left_trigger = float(_safe(xrt.get_left_trigger, 0.0))
            right_trigger = float(_safe(xrt.get_right_trigger, 0.0))
            left_grip = float(_safe(xrt.get_left_grip, 0.0))
            right_grip = float(_safe(xrt.get_right_grip, 0.0))

            left_axis = _safe(xrt.get_left_axis, [0.0, 0.0])
            right_axis = _safe(xrt.get_right_axis, [0.0, 0.0])
            lx = float(left_axis[0]) if len(left_axis) >= 1 else 0.0
            ly = float(left_axis[1]) if len(left_axis) >= 2 else 0.0
            rx = float(right_axis[0]) if len(right_axis) >= 1 else 0.0
            ry = float(right_axis[1]) if len(right_axis) >= 2 else 0.0

            combo = a and b and x and y
            ax = a and x
            by = b and y

            events = []
            if combo and not prev_combo:
                events.append("START_COMBO (A+B+X+Y)")
            if ax and not prev_ax and not combo:
                events.append("A+X")
            if by and not prev_by and not combo:
                events.append("B+Y")

            line = (
                f"{_flag(a, 'A')} {_flag(b, 'B')} {_flag(x, 'X')} {_flag(y, 'Y')}  "
                f"menu L={int(left_menu)} R={int(right_menu)}  "
                f"stickClick L={int(left_click)} R={int(right_click)}  "
                f"trig L={left_trigger:4.2f} R={right_trigger:4.2f}  "
                f"grip L={left_grip:4.2f} R={right_grip:4.2f}  "
                f"axis L=({lx:+.2f},{ly:+.2f}) R=({rx:+.2f},{ry:+.2f})"
            )

            if events:
                print(f"\n>>> {', '.join(events)}")
                print(line)
                last_print = line
            elif line != last_print:
                print(line, end="\r", flush=True)
                last_print = line

            prev_combo = combo
            prev_ax = ax
            prev_by = by
            time.sleep(0.02)
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        try:
            xrt.close()
        except Exception:
            pass


if __name__ == "__main__":
    main()
