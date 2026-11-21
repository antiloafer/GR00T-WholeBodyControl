# BrainCo Revo2 Touch robot service

The workstation publishes six normalized joints per hand on:

- `rt/brainco/left/cmd`
- `rt/brainco/right/cmd`

The G1 Jetson bridge publishes feedback on the corresponding `/state` topics
and converts fresh DDS commands to the two RS485 links.

Touch feedback is published at 10 Hz on `rt/brainco/{left,right}/touch` using a
five-element `MotorStates_` as a portable container. Finger order is thumb to
pinky: `q=normal_N`, `dq=tangential_N`, `ddq=direction_deg`,
`tau_est=proximity_raw`, `mode=health_bits`, `temperature=sequence`, and
`lost=full_status_raw`.

`brainco_touch_dds_smoke.cpp` is a read-only subscriber that checks both touch
topics, their five-finger schema, and prints the five raw status words.

Hardware verified on 2026-08-28:

- left: ID 126, `/dev/ttyUSB2`, firmware `1.0.14.U`
- right: ID 127, `/dev/ttyUSB1`, firmware `1.0.14.U`
- RS485 baud: 460800

`brainco_hand_service_safety.patch` applies to Unitree
`brainco_hand_service` commit `0e9c79ff3a98d87fbaf5f146a447d46b36d0293b`.
It adds a 300 ms watchdog, prevents movement before the first command, caps
speed at 20%, initializes Revo2 Touch correctly, and fixes state publication.
Apply `brainco_hand_touch.patch` after the safety patch to add touch DDS output.

The supplied user service can be installed on the robot without modifying the
system-wide service directory:

```bash
mkdir -p ~/.config/systemd/user
cp brainco-hand.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now brainco-hand.service
systemctl --user status brainco-hand.service
```

The user service starts with the `unitree` desktop/login session. Enabling true
pre-login boot startup additionally requires an administrator to enable linger
for `unitree`.

## Read-only Touch probe

`brainco_touch_probe.cpp` targets the official Stark SDK v2.x and reads the
five processed fingertip records plus raw channels without enabling, resetting,
calibrating, or moving the hand. It reads the documented Revo2 registers
directly to avoid a six-channel bounds bug in the v2.0.3 high-level raw helper.
Stop the hand service before running it because each serial port has a single
owner.

```bash
g++ -std=c++17 -I/path/to/sdk-v2/dist/include brainco_touch_probe.cpp \
  -L/path/to/sdk-v2/dist/shared/linux \
  -Wl,-rpath,/path/to/sdk-v2/dist/shared/linux \
  -lbc_stark_sdk -lpthread -ldl -o /tmp/brainco_touch_probe
/tmp/brainco_touch_probe /dev/ttyUSB2 126  # left
/tmp/brainco_touch_probe /dev/ttyUSB1 127  # right
```
