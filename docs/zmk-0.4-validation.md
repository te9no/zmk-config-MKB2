# ZMK 0.4 hardware-validation branch

This branch prepares MKB2 for ZMK 0.4 and Zephyr 4.1 without promoting the
change to `main`. A successful CI build is necessary but does not count as
hardware validation.

- ZMK is pinned to `afe241df80b05c3f4e0cc95ada7584d24422a893`.
- DYA Studio V2 runtime input processor, sensor rotate, combo, macro, BLE
  management, settings, device information, watchdog, key-scan diagnostics,
  devtool, analog input, and CDC modules use fixed revisions already exercised
  by Polaris or Cornix.
- The external PMW3610 and Cirque driver modules are removed in favor of the
  Zephyr 4.1 `pixart,pmw3610` and `cirque,pinnacle` input drivers.
- PMW3610 and Cirque devicetree properties are migrated to their Zephyr 4.1
  bindings.
- USB logging, CDC boot control with ZMK debug-level output, and custom Studio
  RPC remain on the left split central. The right split peripheral builds
  without those transports.
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
| Split | representative left + right pair | pairing, reconnect, and normal key input | pending |
| Display | left and right OLED | boots and updates without freezes | pending |
| Trackball | left TB, right TBv3, right TBv4 | pointer, scroll, and runtime tuning | pending |
| Joystick | left and right JOY | pointer, scroll, analog RPC, and battery reporting | pending |
| Encoder | left and right ENC | rotate, press, and runtime sensor rotation | pending |
| RZT | left and right RZT | pointer and scroll | pending |
| Trackpad | left and right TPD | pointer, scroll, and gestures used by the keymap | pending |
| Keys | left and right KEY | module matrix input | pending |
| LPPS | left and right LPPS | input and display; Studio exception remains stable | pending |
| DYA Studio V2 | non-LPPS left central | device info, settings, BLE management, runtime processor | pending |
| Runtime features | non-LPPS left central | create/use combo and macro, then verify persistence | pending |
| Diagnostics | central + peripheral | key-scan and watchdog data visible through Studio | pending |
| CDC Debug | left central | debug serial output plus the 1200-baud UF2 boot trigger | pending |

Only after every applicable row passes should the result be recorded in
`zmk-shield-fleet` and this branch be considered for merging into `main`.
