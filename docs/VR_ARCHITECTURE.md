# VR architecture

The VR rework keeps runtime tracking, world placement, player state, and
gameplay interaction as separate concerns. World scale is always `1.0`.

## Coordinate spaces

OpenVR poses enter the engine in **tracking space** and remain unchanged there.
The HMD and both controllers share the same tracking origin. Code that needs a
pose in Penumbra's world must use the single rigid **TrackingToWorld** transform
owned by `cVRTrackingSpace`.

The current transform deliberately reproduces the historical mod's placement:

1. Keep the runtime HMD rotation.
2. Anchor horizontal head position to the player's world position.
3. Apply the existing floor-reach height correction.
4. Derive one rigid TrackingToWorld matrix and apply it to HMD and hand poses.

Height calibration is stored separately from the rigid world transform. It
must never be implemented by changing world scale.

Future recenter and turning changes belong in TrackingToWorld. Recenter changes
the relationship between physical tracking space and Penumbra world space; it
does not teleport the player character. Snap or smooth turn changes world yaw
through that same relationship so the HMD, hands, held objects, and world-space
UI remain coherent.

## Engine/game boundary

HPL owns generic runtime concerns: tracking poses, coordinate conversion,
input actions, and haptic output. Penumbra owns gameplay meaning: which entity
a hand targets, what it holds, interaction rules, locomotion, crouch state, and
UI behaviour.

This avoids a monolithic VR subsystem that would make engine code depend on
Penumbra entity types.

## Planned increments

1. Central TrackingToWorld boundary without a gameplay change. **Complete.**
2. Independent left/right hand target and held-object state; reuse interaction
   collision shapes.
3. Persistent declarative VR settings.
4. Snap/smooth/disabled turn, recenter, movement deadzone, and speed.
5. Button crouch on R3, kept separate from physical height calibration.
6. Configurable UI distance and scale.
7. Centralized haptic profiles and gameplay feedback.
