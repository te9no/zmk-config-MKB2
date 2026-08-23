# ZMK 0.4 hardware-validation branch

This branch prepares MKB2 for ZMK 0.4 and Zephyr 4.1 without promoting the
change to `main`. A successful CI build is necessary but does not count as
hardware validation.

- ZMK is pinned directly to `cormoran/zmk` at
  `e5c9b6915b56801193e359dd9bad4a167ce0d1b8`.
- DYA Studio V2 runtime input processor, sensor rotate, combo, macro, BLE
  management, settings, device information, watchdog, key-scan diagnostics,
  devtool, analog input, and CDC modules use fixed revisions already exercised
  by Polaris or Cornix.
- PMW3610 uses the pinned cormoran custom Studio RPC driver; Cirque uses the
  Zephyr 4.1 `cirque,pinnacle` input driver.
- PMW3610 and Cirque devicetree properties use their Zephyr 4.1 bindings.
- USB logging and CDC boot control with ZMK debug-level output are enabled on
  both halves. Custom Studio RPC remains on the left split central.
- Joystick builds enable the pinned battery voltage-divider oversampling
  snippet on both halves.
- The DYA analog-input RPC transport is enabled only on the split central.
- Every west project is pinned to a full 40-character commit SHA.

The LPPS central build remains a deliberate exception: its shield configuration
disables Studio RPC to fit its display and power-management workload in RAM.
The LPPS peripheral still relays watchdog and key-scan diagnostics.

## Hardware checklist

Record the date, tester, and result in this file or the fleet ledger. Do not
promote the branch while any required item is blank or failed.

| Area | Firmware / module | Required result | Result |
| --- | --- | --- | --- |
| Split | representative left + right pair | pairing, reconnect, and normal key input | passed 2026-08-23 (left JOY + right TBv4; right input and reconnect confirmed) |
| Display | left and right OLED | boots and updates without freezes | passed 2026-08-23 (input-responsive Bongo Cat, Peripheral LINK state and local battery) |
| Trackball | left TB, right TBv3, right TBv4 | pointer, scroll, and runtime tuning | partial: right TBv4 passed 2026-08-23 (both axes corrected, PMW3610 Studio diagnostics passed) |
| Joystick | left and right JOY | pointer, scroll, analog RPC, and battery reporting | partial: left JOY passed 2026-08-23 (100 Hz smooth input and oversampling battery monitor) |
| Encoder | left and right ENC | rotate, press, and runtime sensor rotation | pending |
| RZT | left and right RZT | pointer and scroll | pending |
| Trackpad | left and right TPD | pointer, scroll, and gestures used by the keymap | pending |
| Keys | left and right KEY | module matrix input | pending |
| LPPS | left and right LPPS | input and display; Studio exception remains stable | pending |
| DYA Studio V2 | non-LPPS left central | device info, settings, BLE management, runtime processor | passed 2026-08-23 (USB Studio RPC and PMW3610 Peripheral diagnostics) |
| Runtime features | non-LPPS left central | create/use combo and macro, then verify persistence | pending |
| Diagnostics | central + peripheral | key-scan and watchdog data visible through Studio | pending |
| CDC Debug | central + peripheral | debug serial output plus the 1200-baud UF2 boot trigger | passed 2026-08-23 (`MKB_L_MODULE_JOY` COM11 and `MKB_R_MODULE_TBv4` COM12) |

Only after every applicable row passes should the result be recorded in
`zmk-shield-fleet` and this branch be considered for merging into `main`.
