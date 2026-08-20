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
			  mfHeightCalibration(0.0f),
			  mfPostureOffset(0.0f),
			  mfSeatedOffset(0.0f),
			  mfWorldYaw(0.0f)
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

		void SetPostureOffset(float afPostureOffset)
		{
			mfPostureOffset = afPostureOffset;
		}

		float GetPostureOffset() const
		{
			return mfPostureOffset;
		}

		// Seated play raises the world so the player's seated eye height maps to
		// the configured standing height. It is a play-mode preference, kept
		// separate from posture (crouch) and the manual height calibration.
		void SetSeatedOffset(float afSeatedOffset)
		{
			mfSeatedOffset = afSeatedOffset;
		}

		float GetSeatedOffset() const
		{
			return mfSeatedOffset;
		}

		void SetWorldYaw(float afWorldYaw)
		{
			mfWorldYaw = cMath::Wrap(afWorldYaw + kPif, 0.0f, k2Pif) - kPif;
		}

		void AddWorldYaw(float afWorldYawDelta)
		{
			SetWorldYaw(mfWorldYaw + afWorldYawDelta);
		}

		float GetWorldYaw() const
		{
			return mfWorldYaw;
		}

		cMatrixf GetWorldYawRotation() const
		{
			return cMath::MatrixRotateY(mfWorldYaw);
		}

		cVector3f TrackingDirectionToWorld(const cVector3f& aTrackingDirection) const
		{
			// Direction vectors must only inherit world orientation. Applying the
			// full pose transform would incorrectly add player position and height.
			return cMath::MatrixMul(GetWorldYawRotation(), aTrackingDirection);
		}

		void RecenterOrientation()
		{
			// Align the viewer's current horizontal look direction with Penumbra's
			// world forward while preserving player position and calibrated height.
			cVector3f viewForward =
				cMath::MatrixInverse(GetHeadWorldPose().GetRotation()).GetForward() * -1.0f;
			viewForward.y = 0.0f;
			if(viewForward.Length() <= kEpsilonf) return;

			viewForward = cMath::Vector3Normalize(viewForward);
			AddWorldYaw(atan2(viewForward.x, -viewForward.z));
		}

		cMatrixf GetTrackingToWorldTransform() const
		{
			cMatrixf calibratedHeadPose = mHeadTrackingPose;
			cVector3f calibratedPosition = calibratedHeadPose.GetTranslation();

			// Preserve the legacy floor reach adjustment while making height an
			// explicit concern. World scale remains 1.0.
			calibratedPosition.x = 0.0f;
			calibratedPosition.y = (calibratedPosition.y - 0.2f) * 1.065f +
				mfHeightCalibration + mfPostureOffset + mfSeatedOffset;
			calibratedPosition.z = 0.0f;
			calibratedHeadPose.SetTranslation(calibratedPosition);

			const cMatrixf rotatedHeadPose = cMath::MatrixMul(GetWorldYawRotation(), calibratedHeadPose);
			const cMatrixf headWorldPose = cMath::MatrixMul(mPlayerWorldPose, rotatedHeadPose);
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
		float mfPostureOffset;
		float mfSeatedOffset;
		float mfWorldYaw;
	};

}

#endif // HPL_VR_TRACKING_H
