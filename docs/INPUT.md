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
| Triangle | Holster the equipped tool or weapon |
| Cross | Jump |
| R2 | Interact, or use the equipped tool or weapon |
| R1 | Inventory |
| Circle | Examine |
| Options | Pause menu |

### Menus and inventory

| Control | Action |
| --- | --- |
| Aim with the right controller | Move the pointer |
| R2 | Select or perform an inventory item's default action |
| R3 | Drag or combine inventory items |
| Circle | Open an inventory item's actions, or go back |
| R1, Square, or Options | Close the current screen |

When a tool such as the hammer is equipped, R2 operates that tool. Press Triangle to holster it before using R2 to interact with doors and other objects again.

To use an inventory item on the world, open the inventory with R1, aim at the
item with the right controller, and press R2. The inventory closes with the item
readied; aim at its target from interaction range and press R2 again. R2 also
selects an action opened with Circle, while R3 remains available for the
original drag-and-combine interaction.

Input edges are latched when the game switches between gameplay and UI action
sets. A button that opened a screen must be released and pressed again before
its UI binding can close that screen.

## HTC Vive compatibility defaults

The Vive binding preserves the historical layout: left trackpad movement, right trackpad sprint or UI drag, left grip quick light, left trigger jump, left menu notebook, right trigger interact/select, right grip inventory/close, and right menu examine/back.

## Runtime diagnostics

On Windows, inspect `Documents/Penumbra Overture/Episode1/hpl.log` after closing the game. It reports one of these paths:

- `SteamVR Input actions are active.` means the action manifest and controller binding are in use.
- `Falling back to legacy controller polling.` means SteamVR Input could not provide active actions and the historical input path is being used.

Controller poses use the logical left/right SteamVR pose actions, with the
existing OpenVR tracked-device role loop retained as a compatibility fallback.
The log reports when each action pose is acquired or lost. While a pose is
invalid, the stale hand and its UI ray are hidden, spatial selection is blocked,
and a held interaction is released cleanly; non-spatial controls such as closing
a screen remain available.

## PS VR2 Bluetooth stability

PS VR2 Sense controllers communicate with the PC independently over Bluetooth.
A headset can therefore keep tracking normally while both hands disconnect. If
SteamVR records `playstation_vr2: Send operation timed out. Closing device`, the
failure is below the game input layer.

Sony lists specific adapter and placement guidance. In particular, the TP-Link
UB500/UB5A has a documented PS VR2 compatibility issue and requires TP-Link's
PS VR2 driver `1.9.1038.3020`, rather than `1.9.1038.3001` or the ordinary
Windows-supplied package:

- [PlayStation: Bluetooth adapters compatible with PS VR2 on PC](https://www.playstation.com/en-us/support/hardware/pc-ps-vr2-bluetooth/)
- [TP-Link: UB500/UB5A PS VR2 stability driver](https://www.tp-link.com/us/support/faq/4188/)

After changing the driver, restart Windows, pair both Sense controllers again,
and use **PlayStation VR2 App → Check Bluetooth Connection Quality** before a
gameplay test. Sony also recommends a USB 2.0 port away from USB 3.0 devices or
an extension cable with clear line of sight to the controllers.
