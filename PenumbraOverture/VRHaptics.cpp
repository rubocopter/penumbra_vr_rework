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
#include "StdAfx.h"
#include "VRHaptics.h"

#include "Init.h"
#include "system/LowLevelSystem.h"

namespace
{
	struct cVRHapticProfile
	{
		float mfDuration;
		float mfFrequency;
		float mfAmplitude;
		unsigned long mlMinIntervalMs;
		const char *msName;
	};

	unsigned long glLastSubmission[eVRHapticEvent_LastEnum][2] = {};
	bool gbHasSubmitted[eVRHapticEvent_LastEnum][2] = {};

	cVRHapticProfile GetProfile(eVRHapticEvent aEvent)
	{
		switch(aEvent)
		{
		case eVRHapticEvent_UISelect:
			return {0.025f, 120.0f, 0.18f, 60, "ui_select"};
		case eVRHapticEvent_ObjectPickup:
			return {0.045f, 90.0f, 0.30f, 80, "object_pickup"};
		case eVRHapticEvent_ObjectDrop:
			return {0.025f, 80.0f, 0.20f, 80, "object_drop"};
		case eVRHapticEvent_Interaction:
			return {0.040f, 95.0f, 0.28f, 100, "interaction"};
		case eVRHapticEvent_LightToggle:
			return {0.035f, 100.0f, 0.24f, 150, "light_toggle"};
		case eVRHapticEvent_MeleeImpact:
			return {0.070f, 65.0f, 0.75f, 250, "melee_impact"};
		case eVRHapticEvent_Damage:
			return {0.120f, 45.0f, 1.00f, 200, "damage"};
		default:
			return {0.030f, 90.0f, 0.20f, 100, "unknown"};
		}
	}

	float Clamp01(float afValue)
	{
		if(afValue < 0.0f) return 0.0f;
		if(afValue > 1.0f) return 1.0f;
		return afValue;
	}

	const char *GetHandName(eVRHapticHand aHand)
	{
		switch(aHand)
		{
		case eVRHapticHand_Left: return "left";
		case eVRHapticHand_Right: return "right";
		case eVRHapticHand_Both: return "both";
		default: return "unknown";
		}
	}

	bool SubmitToHand(cInit *apInit, eVRHapticEvent aEvent,
		eSteamVRHand aSteamVRHand, int alHandIndex, const cVRHapticProfile &aProfile,
		float afAmplitude, unsigned long alNow)
	{
		const bool poseValid = aSteamVRHand == eSteamVRHand_Left
			? apInit->mpGame->vr_left_hand.IsPoseValid()
			: apInit->mpGame->vr_right_hand.IsPoseValid();
		if(!poseValid) return false;

		if(gbHasSubmitted[aEvent][alHandIndex] &&
			alNow - glLastSubmission[aEvent][alHandIndex] < aProfile.mlMinIntervalMs)
		{
			return false;
		}

		if(!apInit->mpGame->vr_input.TriggerHaptic(aSteamVRHand,
			aProfile.mfDuration, aProfile.mfFrequency, afAmplitude))
		{
			return false;
		}

		gbHasSubmitted[aEvent][alHandIndex] = true;
		glLastSubmission[aEvent][alHandIndex] = alNow;
		return true;
	}
}

bool cVRHaptics::Play(cInit *apInit, eVRHapticEvent aEvent,
	eVRHapticHand aHand, float afStrength)
{
	if(apInit == NULL || apInit->mpGame == NULL) return false;
	if(aEvent < 0 || aEvent >= eVRHapticEvent_LastEnum) return false;

	const cVRHapticProfile profile = GetProfile(aEvent);
	const float fAmplitude = Clamp01(profile.mfAmplitude * Clamp01(afStrength));
	if(fAmplitude <= 0.0f) return false;

	const unsigned long lNow = GetApplicationTime();
	bool bSuccess = false;
	if(aHand == eVRHapticHand_Left || aHand == eVRHapticHand_Both)
	{
		bSuccess = SubmitToHand(apInit, aEvent, eSteamVRHand_Left, 0,
			profile, fAmplitude, lNow) || bSuccess;
	}
	if(aHand == eVRHapticHand_Right || aHand == eVRHapticHand_Both)
	{
		bSuccess = SubmitToHand(apInit, aEvent, eSteamVRHand_Right, 1,
			profile, fAmplitude, lNow) || bSuccess;
	}

	if(bSuccess)
	{
		Log(" [VR haptics +%lu ms] %s -> %s (submitted).\n",
			lNow, profile.msName, GetHandName(aHand));
	}
	return bSuccess;
}
