# SteamVR input

Penumbra VR uses SteamVR Input actions instead of assuming fixed HTC Vive button IDs and axis numbers. Gameplay and UI have separate action sets, so a physical control can have an appropriate meaning in menus without leaking into movement or interaction. If the action manifest cannot be initialized or no actions are active, the game automatically falls back to the historical OpenVR controller polling path.

The action manifest is installed as `vr/actions.json` beside `Penumbra_vr.exe`. Bindings can be inspected and customized through SteamVR's controller binding interface.

## PS VR2 Sense defaults

### Gameplay

| Control | Action |
| --- | --- |
| Left stick | Move |
| L3 | Sprint |
| L1 | Toggle the current quick light |
| Square | Notebook |
| Cross | Jump |
| R2 | Interact |
| R1 | Inventory |
| Circle | Examine |
| Options | Pause menu |

### Menus and inventory

| Control | Action |
| --- | --- |
| R2 | Select or activate |
| R3 | Drag or combine |
| Circle | Back or examine |
| R1, Square, or Options | Close the current screen |

## HTC Vive compatibility defaults

The Vive binding preserves the historical layout: left trackpad movement, right trackpad sprint or UI drag, left grip quick light, left trigger jump, left menu notebook, right trigger interact/select, right grip inventory/close, and right menu examine/back.

## Runtime diagnostics

On Windows, inspect `Documents/Penumbra Overture/Episode1/hpl.log` after closing the game. It reports one of these paths:

- `SteamVR Input actions are active.` means the action manifest and controller binding are in use.
- `Falling back to legacy controller polling.` means SteamVR Input could not provide active actions and the historical input path is being used.

Controller poses still come from the existing OpenVR tracked-device loop in this checkpoint. Pose and haptic actions are already declared so those systems can migrate independently after controller input is verified on hardware.
