/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of Penumbra Overture.
 *
 * Penumbra Overture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef GAME_VR_HAPTICS_H
#define GAME_VR_HAPTICS_H

class cInit;

enum eVRHapticHand
{
	eVRHapticHand_Left,
	eVRHapticHand_Right,
	eVRHapticHand_Both
};

enum eVRHapticEvent
{
	eVRHapticEvent_UISelect,
	eVRHapticEvent_ObjectPickup,
	eVRHapticEvent_ObjectDrop,
	eVRHapticEvent_Interaction,
	eVRHapticEvent_LightToggle,
	eVRHapticEvent_MeleeImpact,
	eVRHapticEvent_Damage,
	eVRHapticEvent_LastEnum
};

// Penumbra owns the meaning of feedback events. The HPL input backend only
// receives a device-independent hand plus vibration parameters.
class cVRHaptics
{
public:
	static bool Play(cInit *apInit, eVRHapticEvent aEvent,
		eVRHapticHand aHand = eVRHapticHand_Right, float afStrength = 1.0f);
};

#endif // GAME_VR_HAPTICS_H
