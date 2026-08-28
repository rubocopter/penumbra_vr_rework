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
#ifndef GAME_VR_HAND_COLLISION_POLICY_H
#define GAME_VR_HAND_COLLISION_POLICY_H

namespace VRHandCollisionPolicy
{
	// Physical palm dimensions in metres. These remain a close fit to the
	// visible hand; soft contact must never be implemented by shrinking them.
	static const float kCollisionSizeX = 0.190f;
	static const float kCollisionSizeY = 0.052f;
	static const float kCollisionSizeZ = 0.125f;

	// Spatial sampling makes collision independent of render/update rate. The
	// step stays below the palm's thinnest blocking span even with the larger
	// interaction tolerance, so a thin wall cannot fall between two samples.
	static const float kSweepStep = 0.020f;
	static const int kSlideIterations = 4;
	static const int kSweepRefineIterations = 7;

	// Ordinary contact gets only numerical skin. A physical, non-magnetic
	// target may use 8 mm so a drawer face can feel like contact rather than an
	// invisible stand-off. This is deliberately far below the 52 mm palm depth.
	static const float kContactTolerance = 0.002f;
	static const float kInteractionContactTolerance = 0.008f;
	static const float kInteractionTargetDistance = 0.40f;
	static const float kDepenetrationSlop = 0.00025f;
	static const int kOverlapResolveIterations = 4;

	// Rotation is swept separately. A controller turn cannot instantaneously
	// replace the last collision-free orientation and generate a large push.
	static const float kRotationStepRadians = 0.130899694f; // 7.5 degrees
	static const int kRotationRefineIterations = 7;

	static const float kTrackingReanchorDistance = 0.75f;

	// A first tracking sample can legitimately arrive inside a ceiling or a
	// moving door immediately after loading. Recovery starts from a body-side
	// collision-free anchor and still sweeps the full palm to the controller;
	// it is never a teleport through world geometry. The same path is used when
	// a hand has remained constrained in a concave hinge and is being pulled
	// back toward the player.
	static const float kRecoveryAnchorSide = 0.15f;
	static const float kRecoveryAnchorDown = 0.24f;
	static const float kRecoveryAnchorForward = 0.03f;
	static const float kConstrainedHandDistance = 0.10f;
	static const int kConstrainedRecoveryFrames = 12;
	static const float kRecoveryPullbackProgress = 0.0015f;
	// Reject an implausible controller transform before it can place a hand or
	// attached light at an arbitrary world coordinate. This is deliberately
	// larger than a fully extended arm and is relative to the current head.
	static const float kMaxTrackedHandDistanceFromHead = 2.50f;
	static const float kMaxCollisionInteractionReach = 0.18f;

	inline float ContactTolerance(bool abInteractionAssist)
	{
		return abInteractionAssist
			? kInteractionContactTolerance : kContactTolerance;
	}

	inline bool IsBlockingPenetration(float afDepth,
		bool abInteractionAssist)
	{
		return afDepth > ContactTolerance(abInteractionAssist);
	}

	inline int SweepStepCount(float afDistance)
	{
		if(afDistance <= 0.0f) return 1;
		return (int)(afDistance / kSweepStep) + 1;
	}

	inline bool ShouldUseRecoveryAnchor(int alConstrainedFrames,
		float afCurrentDistanceToAnchor,
		float afPreviousDistanceToAnchor)
	{
		return alConstrainedFrames >= kConstrainedRecoveryFrames &&
			afCurrentDistanceToAnchor + kRecoveryPullbackProgress <
				afPreviousDistanceToAnchor;
	}
}

#endif // GAME_VR_HAND_COLLISION_POLICY_H
