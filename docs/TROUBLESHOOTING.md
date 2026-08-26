# Troubleshooting and compatibility

Most problems are machine-specific in ways the game can now describe by itself. Before anything else, grab the two files it writes:

| File | Where | What it is |
|---|---|---|
| `hpl.log` | `Documents\Penumbra Overture\Episode1\` | Boot report: mod version, Windows build, RAM and free process address space, real GPU used, audio devices found, VR controller diagnostics |
| `penumbravr_crash.dmp` | Next to the game executable (`...\Penumbra Overture\redist\`) | Crash minidump, written automatically the moment the game dies |

When asking for help, attach both. If your `Documents` folder is redirected to **OneDrive**, the log actually lives at `OneDrive\Documents\Penumbra Overture\Episode1\`.

## The game does not start at all

**Steam's "Verify integrity of game files" restores the original launcher.**
Verification replaces `Penumbra.exe` with the vanilla executable, which is not the VR build. Run `Install-PenumbraVR.bat` again after verifying files — it is fully reversible (`-Restore`) and only re-applies its own managed files.

**Antivirus or SmartScreen blocked the executable.**
The mod ships a self-built `Penumbra_vr.exe`; some heuristics flag unsigned game launchers. Restore it from quarantine and add an exclusion for the `redist` folder if needed.

**A "missing VCRUNTIME140.dll / MSVCP140.dll" dialog.**
Since alpha.6.1 both DLLs ship inside the package next to the executable. If you still see this dialog, the installation is incomplete or something removed those files — reinstall the mod. (Windows 7 is not supported: SteamVR itself requires Windows 10+.)

## Performance problems

**Laptop using the wrong GPU (NVIDIA Optimus / hybrid graphics).**
A classic failure mode: SteamVR renders on the discrete GPU while the game silently lands on the integrated one. Check `hpl.log` — the line

```
OpenGL vendor: ... / renderer: ... / version: ...
```

tells you which GPU really created the context. If it shows Intel/AMD integrated instead of your NVIDIA card: **Windows Settings ▸ System ▸ Display ▸ Graphics ▸ Browse ▸ select `Penumbra_vr.exe` ▸ High performance**, then relaunch.

**Overlays and injectors.**
RTSS/MSI Afterburner, GeForce/Radeon overlay, Discord overlay, and RGB suites hook into OpenGL and are a known source of black screens and stutters in old engines. Disable them for this game before reporting rendering issues.

**Resolution too demanding.**
Options ▸ VR Settings ▸ Display ▸ Render Scale multiplies the headset's recommended eye buffer. Above 1.0 the cost grows quadratically and the game stays a 32-bit process — prefer 1.0 or lower first, then raise shadows/anisotropy.

## Audio problems

**Sound comes out of the wrong device.**
OpenAL opens the *default* Windows playback device at launch. Headsets often register their own HDMI/DP or Bluetooth output — set the device you want as default *before* starting the game. `hpl.log` lists every device OpenAL Soft detected.

**Muffled or robotic sound with OEM audio software.**
Nahimic, Sonic Studio, MaxxAudio and friends inject themselves into the audio stack and regularly break positional audio. Exclude the game from these enhancements.

**HRTF/binaural seems off.**
With the default `Auto` mode, binaural rendering only engages when Windows reports the output as headphones (it follows whatever device is default). Force `Headphones` or `Off` under Options ▸ VR Settings ▸ Display; like render scale, it needs a restart.

## Controller problems

**Buttons work but sticks don't move/turn (Valve Index and others).**
SteamVR remembers the binding it stored on first launch; an older alpha's binding leaves the joysticks unbound forever. See [Troubleshooting input](INPUT.md#troubleshooting-input) for the one-time reset. The log calls this situation out explicitly after ~20 seconds of play.

**Unrecognized controller.**
Search `hpl.log` for `[VR input]`: it records the detected headset driver, both controller types, and whether a bundled profile matched. Include this when reporting new hardware.

## Installing without Steam (GOG copy or custom location)

The installer auto-detects Steam libraries. For anything else, point it at the folder that contains `redist`:

```powershell
powershell -ExecutionPolicy Bypass -File Install-PenumbraVR.ps1 -InstallRoot "D:\Games\Penumbra Overture"
```

It requires the original game layout (`redist\Penumbra.exe` plus `config\English.lang`). Uninstall the same way with `-Restore`.

## Where everything lives

| What | Path |
|---|---|
| Mod installation | `<Steam library>\steamapps\common\Penumbra Overture\redist\` |
| Installer state and backups | `<game folder>\.penumbravr\` |
| Settings, saves, `hpl.log` | `Documents\Penumbra Overture\Episode1\` |
| Crash dumps | `redist\penumbravr_crash.dmp` |
| SteamVR bindings | SteamVR ▸ Settings ▸ Controllers ▸ *Manage on desktop* |
