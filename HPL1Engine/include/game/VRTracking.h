/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of HPL1 Engine.
 *
 * HPL1 Engine is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */
#ifndef HPL_VR_TRACKING_H
#define HPL_VR_TRACKING_H

#include "math/Math.h"

namespace hpl {

	/**
	 * Owns the boundary between runtime tracking space and Penumbra world
	 * space. Runtime poses stay unmodified; every world-space pose is produced
	 * through the same rigid transform.
	 */
	class cVRTrackingSpace
	{
	public:
		cVRTrackingSpace()
			: mHeadTrackingPose(cMatrixf::Identity),
			  mPlayerWorldPose(cMatrixf::Identity),
			  mfHeightCalibration(0.0f)
		{
		}

		void SetHeadTrackingPose(const cMatrixf& aPose)
		{
			mHeadTrackingPose = aPose;
		}

		const cMatrixf& GetHeadTrackingPose() const
		{
			return mHeadTrackingPose;
		}

		void SetPlayerWorldPose(const cMatrixf& aPose)
		{
			mPlayerWorldPose = aPose;
		}

		const cMatrixf& GetPlayerWorldPose() const
		{
			return mPlayerWorldPose;
		}

		void SetHeightCalibration(float afHeightCalibration)
		{
			mfHeightCalibration = afHeightCalibration;
		}

		float GetHeightCalibration() const
		{
			return mfHeightCalibration;
		}

		cMatrixf GetTrackingToWorldTransform() const
		{
			cMatrixf calibratedHeadPose = mHeadTrackingPose;
			cVector3f calibratedPosition = calibratedHeadPose.GetTranslation();

			// Preserve the legacy floor reach adjustment while making height an
			// explicit concern. World scale remains 1.0.
			calibratedPosition.x = 0.0f;
			calibratedPosition.y = (calibratedPosition.y - 0.2f) * 1.065f + mfHeightCalibration;
			calibratedPosition.z = 0.0f;
			calibratedHeadPose.SetTranslation(calibratedPosition);

			const cMatrixf headWorldPose = cMath::MatrixMul(mPlayerWorldPose, calibratedHeadPose);
			return cMath::MatrixMul(headWorldPose, cMath::MatrixInverse(mHeadTrackingPose));
		}

		cMatrixf TrackingToWorld(const cMatrixf& aTrackingPose) const
		{
			return cMath::MatrixMul(GetTrackingToWorldTransform(), aTrackingPose);
		}

		cMatrixf GetHeadWorldPose() const
		{
			return TrackingToWorld(mHeadTrackingPose);
		}

		cMatrixf GetHeadWorldPose(const cMatrixf& aHeadLocalOffset) const
		{
			return TrackingToWorld(cMath::MatrixMul(mHeadTrackingPose, aHeadLocalOffset));
		}

	private:
		cMatrixf mHeadTrackingPose;
		cMatrixf mPlayerWorldPose;
		float mfHeightCalibration;
	};

}

#endif // HPL_VR_TRACKING_H
