# ZMK 0.4 hardware-validation branch

This branch prepares MKB2 for ZMK 0.4 and Zephyr 4.1 without promoting the
change to `main`. A successful CI build is necessary but does not count as
hardware validation.

- ZMK is pinned to `afe241df80b05c3f4e0cc95ada7584d24422a893`.
- DYA runtime input processor, BLE management, settings RPC, and CDC boot
  trigger modules use the revisions already exercised by Polaris and SAA.
- The external PMW3610 and Cirque driver modules are removed in favor of the
  Zephyr 4.1 `pixart,pmw3610` and `cirque,pinnacle` input drivers.
- PMW3610 and Cirque devicetree properties are migrated to their Zephyr 4.1
  bindings.
- USB logging, CDC boot control, and custom Studio RPC remain on the left
  split central. The right split peripheral builds without those transports.
- Joystick builds enable the pinned battery voltage-divider oversampling
  snippet on both halves.
- The DYA analog-input RPC transport is enabled only on the split central.
- Every west project is pinned to a full 40-character commit SHA.

Before promotion, flash representative left and right builds and verify split
pairing, keys, OLED, each installed pointing module, Studio RPC, CDC UF2 entry,
and battery reporting. Record the result in `zmk-shield-fleet`; only then may
this branch be considered for merging into `main`.
