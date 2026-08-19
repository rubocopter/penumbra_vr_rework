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
#ifndef GAME_VR_SETTINGS_H
#define GAME_VR_SETTINGS_H

namespace hpl {
	class cConfigFile;
}

enum eVRTurnMode
{
	eVRTurnMode_Disabled,
	eVRTurnMode_Snap,
	eVRTurnMode_Smooth
};

enum eVRCrouchMode
{
	eVRCrouchMode_Physical,
	eVRCrouchMode_Button,
	eVRCrouchMode_Hybrid
};

enum eVRHandedness
{
	eVRHandedness_Left,
	eVRHandedness_Right
};

enum eVRPlayMode
{
	eVRPlayMode_Standing,
	eVRPlayMode_Seated
};

class cVRSettings
{
public:
	cVRSettings();

	void Load(hpl::cConfigFile *apConfig);
	void Save(hpl::cConfigFile *apConfig) const;

	float GetMoveSpeed() const { return mfMoveSpeed; }
	float GetMoveDeadZone() const { return mfMoveDeadZone; }
	float GetHeightOffset() const { return mfHeightOffset; }
	eVRTurnMode GetTurnMode() const { return mTurnMode; }
	float GetSnapTurnAngle() const { return mfSnapTurnAngle; }
	float GetSmoothTurnSpeed() const { return mfSmoothTurnSpeed; }
	float GetTurnDeadZone() const { return mfTurnDeadZone; }
	float GetUIDistance() const { return mfUIDistance; }
	float GetUIScale() const { return mfUIScale; }
	eVRCrouchMode GetCrouchMode() const { return mCrouchMode; }
	float GetPhysicalCrouchDepth() const { return mfPhysicalCrouchDepth; }
	float GetSubtitleScale() const { return mfSubtitleScale; }
	eVRHandedness GetHandedness() const { return mHandedness; }
	eVRPlayMode GetPlayMode() const { return mPlayMode; }
	float GetPlayerHeight() const { return mfPlayerHeight; }

	void SetMoveSpeed(float afValue) { mfMoveSpeed = Clamp(afValue, 0.25f, 3.0f); }
	void SetMoveDeadZone(float afValue) { mfMoveDeadZone = Clamp(afValue, 0.0f, 0.9f); }
	void SetHeightOffset(float afValue) { mfHeightOffset = Clamp(afValue, -0.5f, 0.5f); }
	void SetTurnMode(eVRTurnMode aMode) { mTurnMode = aMode; }
	void SetSnapTurnAngle(float afValue) { mfSnapTurnAngle = Clamp(afValue, 15.0f, 90.0f); }
	void SetSmoothTurnSpeed(float afValue) { mfSmoothTurnSpeed = Clamp(afValue, 30.0f, 360.0f); }
	void SetTurnDeadZone(float afValue) { mfTurnDeadZone = Clamp(afValue, 0.0f, 0.9f); }
	void SetUIDistance(float afValue) { mfUIDistance = Clamp(afValue, 0.75f, 3.0f); }
	void SetUIScale(float afValue) { mfUIScale = Clamp(afValue, 0.5f, 2.0f); }
	void SetCrouchMode(eVRCrouchMode aMode) { mCrouchMode = aMode; }
	void SetPhysicalCrouchDepth(float afValue) { mfPhysicalCrouchDepth = Clamp(afValue, 0.10f, 0.60f); }
	void SetSubtitleScale(float afValue) { mfSubtitleScale = Clamp(afValue, 0.75f, 2.0f); }
	void SetHandedness(eVRHandedness aHandedness) { mHandedness = aHandedness; }
	void SetPlayMode(eVRPlayMode aPlayMode) { mPlayMode = aPlayMode; }
	void SetPlayerHeight(float afValue) { mfPlayerHeight = Clamp(afValue, 1.40f, 2.10f); }

private:
	static float Clamp(float afValue, float afMinimum, float afMaximum);

	float mfMoveSpeed;
	float mfMoveDeadZone;
	float mfHeightOffset;
	eVRTurnMode mTurnMode;
	float mfSnapTurnAngle;
	float mfSmoothTurnSpeed;
	float mfTurnDeadZone;
	float mfUIDistance;
	float mfUIScale;
	eVRCrouchMode mCrouchMode;
	float mfPhysicalCrouchDepth;
	float mfSubtitleScale;
	eVRHandedness mHandedness;
	eVRPlayMode mPlayMode;
	float mfPlayerHeight;
};

#endif // GAME_VR_SETTINGS_H
