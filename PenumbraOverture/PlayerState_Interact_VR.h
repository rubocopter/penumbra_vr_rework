/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of Penumbra Overture.
 *
 * Penumbra Overture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Penumbra Overture is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Penumbra Overture.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef GAME_PLAYER_STATE_INTERACT_H
#define GAME_PLAYER_STATE_INTERACT_H

#include "StdAfx.h"
#include "PlayerState.h"

using namespace hpl;

//-----------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// GRAB STATE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------

class cPlayerState_Grab_VR : public iPlayerState
{
public:
	cPlayerState_Grab_VR(cInit *apInit,cPlayer *apPlayer);
	
	void OnUpdate(float afTimeStep);
	
	void OnDraw();
	
	void OnPostSceneDraw();
	
	bool OnJump();
		
	void OnStartInteractMode();
	
	void OnStartInteract();
	void OnStopInteract();
	void OnStartExamine();
	
	bool OnAddYaw(float afVal);
	bool OnAddPitch(float afVal);
	
	bool OnMoveForwards(float afMul, float afTimeStep);
	bool OnMoveSideways(float afMul, float afTimeStep);
	
	void EnterState(iPlayerState* apPrevState);
	void LeaveState(iPlayerState* apNextState);
	
	void OnStartCrouch();
	void OnStopCrouch();
	bool OnStartInventory();
	bool OnStartInventoryShortCut(int alNum);

	static float mfMassDiv;
private:
	cVector3f mvRelPickPoint;
	iPhysicsBody *mpPushBody;

	ePlayerMoveState mPrevMoveState;
	ePlayerState mPrevState;

	bool mbHasGravity;

	bool mbMoveHand;

	float mfGrabDist;

	float mfDefaultMass;

	bool mbHasPlayerGravityPush;

	bool mbPickAtPoint;

	float mfStartYaw;

	float mfSpeedMul;

  cMatrixf localPickMatrix;

  // Which controller grabbed the body (dominant by default, off hand allowed
  // while it carries no equipment)
  int mHandIndex;

  // Newton hard-clamps body velocities against these map-defined caps, which
  // would strangle the tracking servo; originals are restored on release.
  float mfDefaultMaxLinSpeed;
  float mfDefaultMaxAngSpeed;
};

//-----------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// MOVE STATE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------

class cPlayerState_Move_BodyCallback_VR : public iPhysicsBodyCallback
{
public:
	cPlayerState_Move_BodyCallback_VR(cPlayer *apPlayer, float afTimeStep);
	
	bool OnBeginCollision(iPhysicsBody *apBody, iPhysicsBody *apCollideBody);
	void OnCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody,cPhysicsContactData* apContactData);
	
	float mfTimeStep;
	int mlBackCount;
	cPlayer *mpPlayer;
private:
};

//-----------------------------------------------------------------

class cPlayerState_Move_VR : public iPlayerState
{
public:
	cPlayerState_Move_VR(cInit *apInit,cPlayer *apPlayer);
	~cPlayerState_Move_VR();
	
	void OnUpdate(float afTimeStep);
	
	void OnStartInteract();
	void OnStopInteract();
	
	bool OnJump();
	
	void OnStartExamine();
	
	bool OnMoveForwards(float afMul, float afTimeStep);
	bool OnMoveSideways(float afMul, float afTimeStep);
	
	bool OnAddYaw(float afVal);
	bool OnAddPitch(float afVal);
	
	void EnterState(iPlayerState* apPrevState);
	void LeaveState(iPlayerState* apNextState);
	
	void OnPostSceneDraw();
	
	bool OnStartInventory();
	bool OnStartInventoryShortCut(int alNum);

private:
	cVector3f mvForward;
	cVector3f mvRight;
	cVector3f mvUp;

	cVector3f mvRelPickPoint;
	cVector3f mvPickPoint;

	bool bPausedGravity;

	iPhysicsBody *mpPushBody;

	int mlMoveCount;

	ePlayerMoveState mPrevMoveState;
	ePlayerState mPrevState;

  cPlayerState_Move_BodyCallback_VR *mpCallback;

  cMatrixf localPickMatrix;

  cVector3f mvSlideAxis;
  bool mbHasSlideAxis;

  // Velocity caps overridden while dragging (restored on release)
  float mfDefaultMaxLinSpeed;
  float mfDefaultMaxAngSpeed;

  // For hinge joints: store original angle limits to restore on release
  bool mbHingeLimitsStored;
  float mfOriginalMinAngle;
  float mfOriginalMaxAngle;

  // Lightness factor for heavy hinged objects (taquilla, cofre)
  float mfHingeLightnessFactor;
};

//////////////////////////////////////////////////////////////////////////
// PUSH STATE
//////////////////////////////////////////////////////////////////////////

class cPlayerState_Push_VR : public iPlayerState
{
public:
	cPlayerState_Push_VR(cInit *apInit,cPlayer *apPlayer);
	
	void OnUpdate(float afTimeStep);
	bool OnJump();
	
	void OnStartInteract();
	void OnStopInteract();
	
	void OnStartExamine();
	
	bool OnMoveForwards(float afMul, float afTimeStep);
	bool OnMoveSideways(float afMul, float afTimeStep);
	
	void EnterState(iPlayerState* apPrevState);
	void LeaveState(iPlayerState* apNextState);

	void OnPostSceneDraw();
	
	bool OnStartInventory();
	bool OnStartInventoryShortCut(int alNum);

private:
	cVector3f mvForward;
	cVector3f mvRight;

	cVector3f mvRelPickPoint;
	
	cVector3f mvLocalPickPoint;
  cVector3f mvPickPoint;

	iPhysicsBody *mpPushBody;
	cVector3f mvLastBodyPos;

	cVector2f mvPrevPitchLimits;

	bool mbHasPlayerGravityPush;

	float mfMaxSpeed;

	int mlForward;
	int mlSideways;

	ePlayerMoveState mPrevMoveState;
	ePlayerState mPrevState;

  cMatrixf localPickMatrix;
};



#endif // GAME_PLAYER_STATE_INTERACT_H
