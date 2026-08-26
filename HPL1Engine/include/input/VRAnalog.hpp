/*
 * Copyright (C) 2026 Penumbra: Overture VR Rework contributors.
 *
 * Licensed under GPL v3 or later — see the repository COPYING notices.
 */
#ifndef HPL_VR_ANALOG_H
#define HPL_VR_ANALOG_H

namespace hpl {

	/**
	 * Rescales an analog stick vector out of its dead zone into the full
	 * [0..1] output range, preserving direction. Pure math on purpose: this
	 * is exercised directly by the unit tests in tests/VRTrackingTest.
	 */
	inline void ApplyStickDeadZone(float& afX, float& afY, float afDeadZone)
	{
		const float magnitude = sqrtf(afX * afX + afY * afY);
		if (magnitude <= afDeadZone || magnitude <= 0.0001f) {
			afX = 0.0f;
			afY = 0.0f;
			return;
		}

		float scaledMagnitude = (magnitude - afDeadZone) / (1.0f - afDeadZone);
		if (scaledMagnitude > 1.0f) scaledMagnitude = 1.0f;
		afX = (afX / magnitude) * scaledMagnitude;
		afY = (afY / magnitude) * scaledMagnitude;
	}

}

#endif // HPL_VR_ANALOG_H
