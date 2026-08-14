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
#include "VRSettings.h"

#include "StdAfx.h"
#include "resources/ConfigFile.h"

using namespace hpl;

namespace
{
	const float kDefaultMoveSpeed = 1.0f;
	const float kDefaultMoveDeadZone = 0.15f;
	const float kDefaultHeightOffset = 0.0f;
	const float kDefaultSnapTurnAngle = 45.0f;
	const float kDefaultSmoothTurnSpeed = 90.0f;
	const float kDefaultTurnDeadZone = 0.20f;
	const float kDefaultUIDistance = 1.75f;
	const float kDefaultUIScale = 1.0f;
	const float kDefaultPhysicalCrouchDepth = 0.25f;
	const float kDefaultSubtitleScale = 1.35f;

	const char *TurnModeName(eVRTurnMode aMode)
	{
		switch(aMode)
		{
		case eVRTurnMode_Disabled: return "Disabled";
		case eVRTurnMode_Smooth: return "Smooth";
		default: return "Snap";
		}
	}

	const char *CrouchModeName(eVRCrouchMode aMode)
	{
		switch(aMode)
		{
		case eVRCrouchMode_Physical: return "Physical";
		case eVRCrouchMode_Button: return "Button";
		default: return "Hybrid";
		}
	}
}

cVRSettings::cVRSettings()
	: mfMoveSpeed(kDefaultMoveSpeed),
	  mfMoveDeadZone(kDefaultMoveDeadZone),
	  mfHeightOffset(kDefaultHeightOffset),
	  mTurnMode(eVRTurnMode_Snap),
	  mfSnapTurnAngle(kDefaultSnapTurnAngle),
	  mfSmoothTurnSpeed(kDefaultSmoothTurnSpeed),
	  mfTurnDeadZone(kDefaultTurnDeadZone),
	  mfUIDistance(kDefaultUIDistance),
	  mfUIScale(kDefaultUIScale),
	  mCrouchMode(eVRCrouchMode_Hybrid),
	  mfPhysicalCrouchDepth(kDefaultPhysicalCrouchDepth),
	  mfSubtitleScale(kDefaultSubtitleScale)
{
}

void cVRSettings::Load(cConfigFile *apConfig)
{
	if(apConfig == NULL) return;

	mfMoveSpeed = Clamp(apConfig->GetFloat("VR", "MoveSpeed", kDefaultMoveSpeed), 0.25f, 3.0f);
	mfMoveDeadZone = Clamp(apConfig->GetFloat("VR", "MoveDeadZone", kDefaultMoveDeadZone), 0.0f, 0.9f);
	mfHeightOffset = Clamp(apConfig->GetFloat("VR", "HeightOffset", kDefaultHeightOffset), -0.5f, 0.5f);

	const tString sTurnMode = cString::ToLowerCase(apConfig->GetString("VR", "TurnMode", "Snap"));
	if(sTurnMode == "disabled") mTurnMode = eVRTurnMode_Disabled;
	else if(sTurnMode == "smooth") mTurnMode = eVRTurnMode_Smooth;
	else mTurnMode = eVRTurnMode_Snap;

	mfSnapTurnAngle = Clamp(apConfig->GetFloat("VR", "SnapTurnAngle", kDefaultSnapTurnAngle), 15.0f, 90.0f);
	mfSmoothTurnSpeed = Clamp(apConfig->GetFloat("VR", "SmoothTurnSpeed", kDefaultSmoothTurnSpeed), 30.0f, 360.0f);
	// Builds before the comfort pass persisted 120 deg/s as their untouched
	// default. Migrate that one legacy value once while preserving deliberate
	// custom speeds on existing profiles.
	const int lSettingsVersion = apConfig->GetInt("VR", "SettingsVersion", 0);
	if(lSettingsVersion < 1 && mfSmoothTurnSpeed > 119.9f && mfSmoothTurnSpeed < 120.1f)
		mfSmoothTurnSpeed = kDefaultSmoothTurnSpeed;
	mfTurnDeadZone = Clamp(apConfig->GetFloat("VR", "TurnDeadZone", kDefaultTurnDeadZone), 0.0f, 0.9f);
	mfUIDistance = Clamp(apConfig->GetFloat("VR", "UIDistance", kDefaultUIDistance), 0.75f, 3.0f);
	mfUIScale = Clamp(apConfig->GetFloat("VR", "UIScale", kDefaultUIScale), 0.5f, 2.0f);

	const tString sCrouchMode = cString::ToLowerCase(apConfig->GetString("VR", "CrouchMode", "Hybrid"));
	if(sCrouchMode == "physical") mCrouchMode = eVRCrouchMode_Physical;
	else if(sCrouchMode == "button") mCrouchMode = eVRCrouchMode_Button;
	else mCrouchMode = eVRCrouchMode_Hybrid;
	mfPhysicalCrouchDepth = Clamp(apConfig->GetFloat("VR", "PhysicalCrouchDepth", kDefaultPhysicalCrouchDepth), 0.10f, 0.60f);
	mfSubtitleScale = Clamp(apConfig->GetFloat("VR", "SubtitleScale", kDefaultSubtitleScale), 0.75f, 2.0f);

	Log(" VR settings: move speed %.2f, dead zone %.2f, height offset %.2f m; "
		"turn %s, snap %.0f deg, smooth %.0f deg/s, turn dead zone %.2f; "
		"UI distance %.2f m, scale %.2f; crouch %s, depth %.2f m; subtitle scale %.2f.\n",
		mfMoveSpeed, mfMoveDeadZone, mfHeightOffset, TurnModeName(mTurnMode),
		mfSnapTurnAngle, mfSmoothTurnSpeed, mfTurnDeadZone, mfUIDistance, mfUIScale,
		CrouchModeName(mCrouchMode), mfPhysicalCrouchDepth, mfSubtitleScale);
}

void cVRSettings::Save(cConfigFile *apConfig) const
{
	if(apConfig == NULL) return;

	apConfig->SetFloat("VR", "MoveSpeed", mfMoveSpeed);
	apConfig->SetFloat("VR", "MoveDeadZone", mfMoveDeadZone);
	apConfig->SetFloat("VR", "HeightOffset", mfHeightOffset);
	apConfig->SetString("VR", "TurnMode", TurnModeName(mTurnMode));
	apConfig->SetFloat("VR", "SnapTurnAngle", mfSnapTurnAngle);
	apConfig->SetFloat("VR", "SmoothTurnSpeed", mfSmoothTurnSpeed);
	apConfig->SetFloat("VR", "TurnDeadZone", mfTurnDeadZone);
	apConfig->SetFloat("VR", "UIDistance", mfUIDistance);
	apConfig->SetFloat("VR", "UIScale", mfUIScale);
	apConfig->SetString("VR", "CrouchMode", CrouchModeName(mCrouchMode));
	apConfig->SetFloat("VR", "PhysicalCrouchDepth", mfPhysicalCrouchDepth);
	apConfig->SetFloat("VR", "SubtitleScale", mfSubtitleScale);
	apConfig->SetInt("VR", "SettingsVersion", 1);
}

float cVRSettings::Clamp(float afValue, float afMinimum, float afMaximum)
{
	if(afValue < afMinimum) return afMinimum;
	if(afValue > afMaximum) return afMaximum;
	return afValue;
}
