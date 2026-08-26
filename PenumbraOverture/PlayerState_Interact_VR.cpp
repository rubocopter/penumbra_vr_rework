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
#include "StdAfx.h"
#include "PlayerState_Interact_VR.h"

#include "Init.h"
#include "Player.h"
#include "MapHandler.h"
#include "GameStickArea.h"
#include "GameObject.h"
#include "VRHelper.hpp"
#include "VRHaptics.h"

//////////////////////////////////////////////////////////////////////////
// GRAB STATE
//////////////////////////////////////////////////////////////////////////

float cPlayerState_Grab_VR::mfMassDiv = 5.0f;

cPlayerState_Grab_VR::cPlayerState_Grab_VR(cInit *apInit,cPlayer *apPlayer) : iPlayerState(apInit,apPlayer,ePlayerState_Grab)
{
	mpPushBody = NULL;
	mHandIndex = eVRHandIndex_Right;
}

//-----------------------------------------------------------------------

// The controller that grabbed the body; slot index matches hand index.
#define VR_GRAB_HAND() \
  (mHandIndex == eVRHandIndex_Left ? mpInit->mpGame->vr_left_hand \
                                   : mpInit->mpGame->vr_right_hand)
#define VR_GRAB_HAPTIC() \
  (mHandIndex == eVRHandIndex_Left ? eVRHapticHand_Left : eVRHapticHand_Right)

//-----------------------------------------------------------------------


void cPlayerState_Grab_VR::OnUpdate(float afTimeStep)
{	
	cCamera3D *pCamera = mpPlayer->GetCamera();
	iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();

	cInput *pInput = mpInit->mpGame->GetInput();
	
	if(pInput->IsTriggerd("WheelUp"))
	{
		if(mfGrabDist < mpPlayer->mfCurrentMaxInteractDist*0.8f)
			mfGrabDist += 0.1f;
	}
	if(pInput->IsTriggerd("WheelDown"))
	{
		if(mfGrabDist > mpPlayer->GetSize().x/2.0f + 0.25f)
		{
			//TODO: Collison check? (might be a bit complex)			
			mfGrabDist -= 0.1f;	
		}
	}

  // Kinematic hold: the body follows the grabbing hand EXACTLY, teleporting
  // onto the hand matrix every frame (original mod behaviour).  Velocity
  // servos fight Newton's contact resolution and read as telekinesis rather
  // than a firm grip.  Hand velocity is still written into the body so world
  // contacts react and releases inherit momentum.
  mpPushBody->SetGravity(false);
  mpPushBody->SetEnabled(true);
  mpPushBody->SetActive(true);
  mpPushBody->SetAutoDisable(false);

  // Anchor to the RENDERED hand rig pose (controller pose plus the hud model
  // offsets) so held items sit inside the visible palm instead of floating
  // beside it.
  cMatrixf handMat;
  iHudModel *pGrabModel = mpInit->mpPlayerHands->GetCurrentModel(mHandIndex);
  if (pGrabModel && pGrabModel->UpdatePoseMatrix(handMat, 0.0f))
  {
    // pose matrix already includes tracking-to-world
  }
  else
  {
    handMat = VRHelper::TrackingToWorldSpace(VR_GRAB_HAND().GetMatrix(), mpInit->mpGame);
  }

  bool bJointed = mpPushBody->GetJointNum() > 0;
  cMatrixf destMatrix = cMath::MatrixMul(handMat, localPickMatrix);
  mpPushBody->SetWorldMatrix(destMatrix);

  TrackedController& grabHand = VR_GRAB_HAND();
  cVector3f vHandVelW =
    mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(grabHand.GetVelocity());
  float fCarriedSpeed = vHandVelW.Length();
  const float kMaxCarriedSpeed = bJointed ? 3.0f : 20.0f;
  if (fCarriedSpeed > kMaxCarriedSpeed)
    vHandVelW = vHandVelW * (kMaxCarriedSpeed / fCarriedSpeed);
  mpPushBody->SetLinearVelocity(vHandVelW);

  cVector3f vHandAngW =
    mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(grabHand.GetAngularVelocity());
  mpPushBody->SetAngularVelocity(vHandAngW * 0.5f);

  mpPushBody->SetCollidePlayer(false);

  /////////////////////////////////////////////////
  // Cast ray to see if anything could be examined.
  cVector3f vStart, vEnd;

  vStart = mpPlayer->GetCamera()->GetPosition();
  vEnd = vStart + mpPlayer->GetCamera()->GetForward() * mpPlayer->GetExamineRay()->mfMaxDistance;

  mpPlayer->GetExamineRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetExamineRay(), vStart, vEnd, true, false, true);
  mpPlayer->GetExamineRay()->CalculateResults();

  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerPick();

    eCrossHairState CrossState = pEntity->GetPickCrossHairState(mpPlayer->GetExamineBody(), true);

    if ((CrossState == eCrossHairState_Examine && !pEntity->GetHasBeenExamined()))
    {
      mpPlayer->SetCrossHairState(CrossState);
    }
    else
    {
      mpPlayer->SetCrossHairState(eCrossHairState_None);
    }
  }
  else
  {
    mpPlayer->SetCrossHairState(eCrossHairState_None);
  }

  /////////////////////////////////////////////////
  for (int i = 0; i < 2; ++i)
    mpPlayer->SetHandCrossHairState(i, eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnDraw()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnPostSceneDraw()
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnJump()
{ 
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnStartInteractMode()
{
	mbMoveHand = !mbMoveHand;

	if(mbMoveHand){
		mPrevState = ePlayerState_InteractMode;
	}
	else{
		mPrevState = ePlayerState_Normal;
		mpPlayer->ResetCrossHairPos();
	}
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnStartInteract()
{
}

void cPlayerState_Grab_VR::OnStopInteract()
{
	mpPlayer->ChangeState(mPrevState);
}


//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnAddYaw(float afVal)
{
	afVal *= mfSpeedMul*0.75f;
	if(mbMoveHand)
	{
		if(mpPlayer->AddCrossHairPos(cVector2f(afVal * 800.0f,0)))
		{
			mpPlayer->GetCamera()->AddYaw( -afVal * mpPlayer->GetLookSpeed());
			mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
		}

		return false;
	}
	else
	{
		mpPlayer->GetCamera()->AddYaw( -afVal * mpPlayer->GetLookSpeed());
		mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());

		return false;
	}
}

bool cPlayerState_Grab_VR::OnAddPitch(float afVal)
{
	float fInvert = 1;
	if(mpInit->mpButtonHandler->GetInvertMouseY()) fInvert = -1;

	afVal *= mfSpeedMul*0.75f;
	if(mbMoveHand)
	{
		if(mpPlayer->AddCrossHairPos(cVector2f(0,afVal * 600.0f)))
		{
			mpPlayer->GetCamera()->AddPitch( -afVal * mpPlayer->GetLookSpeed());
		}
		return false;
	}
	else
	{
		mpPlayer->GetCamera()->AddPitch( -afVal*fInvert* mpPlayer->GetLookSpeed());
		return false;
	}
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnMoveForwards(float afMul, float afTimeStep)
{
	return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnMoveSideways(float afMul, float afTimeStep)
{
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::EnterState(iPlayerState* apPrevState)
{
	//Detach the body if stuck to a sticky area
	cGameStickArea *pStickArea = mpInit->mpMapHandler->GetBodyStickArea(mpPlayer->GetPushBody());
	if(pStickArea)
	{
		if(pStickArea->GetCanDeatch()){
			pStickArea->DetachBody();
		}
		else {
			mpPlayer->ChangeState(apPrevState->mType);
			return;
		}
	}

  // Remember which controller initiated this grab so tracking, haptics and
  // the visible hand slot all follow it instead of the dominant hand.
  mHandIndex = (int)mpPlayer->GetVRInteractHand();

  mpInit->mpPlayerHands->SetSlotVisible(mHandIndex, false);
	
	cCamera3D *pCamera = mpPlayer->GetCamera();

	//Make sure the player is not running
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Run || mPrevMoveState == ePlayerMoveState_Jump)
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);

	//Get last state, if this is a message use the last previous state instead.
	if(apPrevState->mType != ePlayerState_Message) mPrevState = apPrevState->mType;

	mbPickAtPoint = mpPlayer->mbPickAtPoint;

	if(mPrevState == ePlayerState_InteractMode)
		mbMoveHand = true;
	else 
		mbMoveHand = false;

	//Get the body to push
	mpPushBody = mpPlayer->GetPushBody();
	mpPlayer->SetVRHeldBody((eVRHandIndex)mHandIndex, mpPushBody);
	mbHasGravity = mpPushBody->GetGravity();
	if(mbPickAtPoint==false) mpPushBody->SetGravity(false);
	mpPushBody->SetAutoDisable(false);

	mbHasPlayerGravityPush = mpPushBody->GetPushedByCharacterGravity();
	mpPushBody->SetPushedByCharacterGravity(true);

	//Set a newer player mass
	mpPlayer->SetMass(mpPlayer->GetMass() + mpPushBody->GetMass());

	// Newton clamps body velocity to the map-defined MaxLinearSpeed cap every
	// step; without raising it the tracking servo's requested velocity gets
	// strangled and held objects trail behind the hand like on a rubber band.
	bool bHoldingJointed = mpPushBody->GetJointNum() > 0;
	mfDefaultMaxLinSpeed = mpPushBody->GetMaxLinearSpeed();
	mfDefaultMaxAngSpeed = mpPushBody->GetMaxAngularSpeed();
	mpPushBody->SetMaxLinearSpeed(bHoldingJointed ? 4.0f : 20.0f);
	mpPushBody->SetMaxAngularSpeed(bHoldingJointed ? 6.0f : 30.0f);

	//Lower the mass while holding
	mfDefaultMass = mpPushBody->GetMass();
	mpPushBody->SetMass(mpPushBody->GetMass()/ mfMassDiv);

	//Set the player speed mul
	float fMass = mpPushBody->GetMass();
	mfSpeedMul = 1;
	if(fMass > 3)
	{
		float fMul = 1 - ((fMass-3.0f)/37.0f)*0.34f;
		fMul = cMath::Min(fMul,0.66f);

		mfSpeedMul = fMul;
	}
	mpPlayer->SetSpeedMul(mfSpeedMul);

	//If we want to use the normal mass,reset the divison
	if(mpPlayer->mbUseNormalMass) mpPushBody->SetMass(mfDefaultMass);

	//Get the orientation of the body
	cMatrixf mtxInvModel = cMath::MatrixInverse(mpPushBody->GetLocalMatrix());

	mfGrabDist = cMath::Vector3Dist(mpPlayer->GetCamera()->GetPosition(),
									mpPlayer->GetPickedPos());
	//mfGrabDist = mpPlayer->GetPickedDist();

	//The pick point relative to the body
	if(mbPickAtPoint)
		mvRelPickPoint = cMath::MatrixMul(mtxInvModel, mpPlayer->GetPickedPos());
	else
		mvRelPickPoint =	mpPlayer->GetPickedPos() -
							cMath::MatrixMul(	mpPushBody->GetWorldMatrix(),
												mpPushBody->GetMassCentre());

  // Compute the grab anchor against the SAME rendered hand pose used during
  // hold, so the object keeps exactly its visual relationship to the palm.
  cMatrixf grabAnchorMat;
  iHudModel *pEnterModel = mpInit->mpPlayerHands->GetCurrentModel(mHandIndex);
  if (pEnterModel && pEnterModel->UpdatePoseMatrix(grabAnchorMat, 0.0f))
  {
    // world-space rig pose ready
  }
  else
  {
    grabAnchorMat = VRHelper::TrackingToWorldSpace(VR_GRAB_HAND().GetMatrix(), mpInit->mpGame);
  }
  localPickMatrix = cMath::MatrixMul(cMath::MatrixInverse(grabAnchorMat), mpPushBody->GetLocalMatrix());

  // Put the actual contact point in the palm while preserving the object's
  // orientation at pickup. This removes the long lever arm that made an item
  // appear to orbit around the controller instead of being firmly held.
  if (mbPickAtPoint)
  {
    cMatrixf localRotation = localPickMatrix.GetRotation();
    localPickMatrix.SetTranslation(
      cMath::MatrixMul(localRotation, mvRelPickPoint) * -1.0f);
  }

	//Set cross hair image.
	//mpPlayer->SetCrossHairState(eCrossHairState_Grab);

	mfStartYaw = pCamera->GetYaw();

mpPushBody->SetCollidePlayer(false);

  cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectPickup, VR_GRAB_HAPTIC());
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::LeaveState(iPlayerState* apNextState)
{
	mpPlayer->SetVRHeldBody((eVRHandIndex)mHandIndex, NULL);

  mpInit->mpPlayerHands->SetSlotVisible(mHandIndex, true);

	mpPushBody->SetPushedByCharacterGravity(mbHasPlayerGravityPush);
	mpPushBody->SetGravity(mbHasGravity);
	mpPushBody->SetActive(true);
	mpPushBody->SetAutoDisable(true);

	if(mpPlayer->mbUseNormalMass==false)
		mpPushBody->SetMass(mfDefaultMass);

	// Restore the map-defined velocity caps
	mpPushBody->SetMaxLinearSpeed(mfDefaultMaxLinSpeed);
	mpPushBody->SetMaxAngularSpeed(mfDefaultMaxAngSpeed);

	//Reset newer player mass
	mpPlayer->SetMass(mpPlayer->GetDefaultMass());

	mpPlayer->SetSpeedMul(1.0f);

  // Apply hand velocity for the throw.  TrackingDirectionToWorld rotates only
  // orientation (no translation) so the velocity stays in world space.
  // Clamped: raw controller velocity spikes would launch light objects hard.
  TrackedController& hand = VR_GRAB_HAND();
  cVector3f vWorldHandVel =
    mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(hand.GetVelocity());
  cVector3f vWorldAngVel =
    mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(hand.GetAngularVelocity());

  vWorldHandVel = vWorldHandVel * 1.25f;
  float fThrowSpeed = vWorldHandVel.Length();
  const float kMaxThrowSpeed = 9.0f;
  if (fThrowSpeed > kMaxThrowSpeed)
    vWorldHandVel = vWorldHandVel * (kMaxThrowSpeed / fThrowSpeed);

  float fAngSpeed = vWorldAngVel.Length();
  const float kMaxThrowAngVel = 12.0f;
  if (fAngSpeed > kMaxThrowAngVel)
    vWorldAngVel = vWorldAngVel * (kMaxThrowAngVel / fAngSpeed);

  mpPushBody->SetLinearVelocity(vWorldHandVel);
  mpPushBody->SetAngularVelocity(vWorldAngVel * 0.5f);

  mpPushBody->SetCollidePlayer(true);

  cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectDrop, VR_GRAB_HAPTIC());
}

//-----------------------------------------------------------------------

void cPlayerState_Grab_VR::OnStartCrouch()
{
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

	if(mpInit->mpButtonHandler->GetToggleCrouch())
	{
		if(mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
			mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
		else
			mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
	}
	else
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
	}
}

void cPlayerState_Grab_VR::OnStopCrouch()
{
	if( mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
		mpInit->mpButtonHandler->GetToggleCrouch()==false)
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
	}
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnStartInventory()
{
	return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Grab_VR::OnStartInventoryShortCut(int alNum)
{ 
	return false;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// MOVE STATE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cPlayerState_Move_BodyCallback_VR::cPlayerState_Move_BodyCallback_VR(cPlayer *apPlayer, float afTimeStep)
{
	mpPlayer = apPlayer;
	mfTimeStep = afTimeStep;
	mlBackCount =0;
}

bool cPlayerState_Move_BodyCallback_VR::OnBeginCollision(iPhysicsBody *apBody, iPhysicsBody *apCollideBody)
{
	return true;
}


void cPlayerState_Move_BodyCallback_VR::OnCollide(iPhysicsBody *apBody, iPhysicsBody *apCollideBody,cPhysicsContactData* apContactData)
{
	if(mpPlayer->GetCharacterBody()->GetBody() == apCollideBody)
	{
		mlBackCount = 5;
	}
}

//-----------------------------------------------------------------------

cPlayerState_Move_VR::cPlayerState_Move_VR(cInit *apInit,cPlayer *apPlayer) : iPlayerState(apInit,apPlayer,ePlayerState_Move)
{
	mpPushBody = NULL;
	mpCallback = hplNew( cPlayerState_Move_BodyCallback_VR, (apPlayer, apInit->mpGame->GetStepSize()) );
	mbHasSlideAxis = false;
	mvSlideAxis = cVector3f(0, 0, 0);
}

cPlayerState_Move_VR::~cPlayerState_Move_VR()
{
	hplDelete( mpCallback );
}

//-----------------------------------------------------------------------


void cPlayerState_Move_VR::OnUpdate(float afTimeStep)
{
	//////////////////////////////////////
	//Check if the player is supposed to back
	if(mpCallback->mlBackCount > 0)
	{
		mpCallback->mlBackCount--;
		mpPlayer->GetCharacterBody()->Move(eCharDir_Forward,-1,afTimeStep);
	}

  //cVector3f pos = VRHelper::TrackingToWorldSpace(mpInit->mpGame->vr_right_hand.GetMatrix(), mpInit->mpGame).GetTranslation();

  //mpPushBody->SetLinearVelocity(mpPushBody->GetWorldPosition() - pos);

  auto handMat = VRHelper::TrackingToWorldSpace(VRHelper::DominantHand(mpInit->mpGame).GetMatrix(), mpInit->mpGame);
  cMatrixf destMatrix = cMath::MatrixMul(handMat, localPickMatrix);

  auto destTranslation = destMatrix.GetTranslation();

  auto destDiff = destTranslation - mvPickPoint; // mpPushBody->GetWorldPosition();
  auto moveDirection = cMath::Vector3Normalize(destDiff);

  if (mpPushBody->GetJointNum() > 0)
  {
    // Jointed bodies — angular velocity along hinge pin.
    iPhysicsJoint *pJoint = mpPushBody->GetJoint(0);
    if (pJoint)
    {
      cVector3f vPinDir = pJoint->GetPinDir();
      cVector3f vPivot = pJoint->GetPivotPoint();
      cVector3f vToBody = mpPushBody->GetWorldPosition() - vPivot;
      float fRadius = vToBody.Length();
      if (fRadius < 0.01f) fRadius = 0.3f;

      cVector3f vPerpDir = cMath::Vector3Normalize(
          cMath::Vector3Cross(vPinDir, vToBody));

      cVector3f vDragVel = destDiff * 10.0f;
      float fDotPerp = cMath::Vector3Dot(vDragVel, vPerpDir);
      cVector3f vAllowedVel = vPerpDir * fDotPerp;

      float fAllowedSpeed = vAllowedVel.Length();
      if (fAllowedSpeed > 3.5f)
        fAllowedSpeed = 3.5f;

      float fAngSpeed = fAllowedSpeed / fRadius;
      cVector3f vAngVel = vPerpDir * fAngSpeed;

      float fAngMag = vAngVel.Length();
      float fMaxAng = 12.0f;
      if (fAngMag > fMaxAng)
        vAngVel = vAngVel * (fMaxAng / fAngMag);

      mpPushBody->SetLinearVelocity(cVector3f(0, 0, 0));
      mpPushBody->SetAngularVelocity(vAngVel);
    }
    else
    {
      cVector3f vDragVel = destDiff * 10.0f;
      float fDragSpeed = vDragVel.Length();
      if (fDragSpeed > 3.5f)
        vDragVel = vDragVel * (3.5f / fDragSpeed);
      mpPushBody->SetLinearVelocity(vDragVel);
      mpPushBody->SetAngularVelocity(cVector3f(0.0f, 0.0f, 0.0f));
    }
  }
  else if (mbHasSlideAxis)
  {
    cVector3f vDragVel = destDiff * 10.0f;
    float fDotSlide = cMath::Vector3Dot(vDragVel, mvSlideAxis);
    cVector3f vSlideVel = mvSlideAxis * fDotSlide;
    float fSlideSpeed = vSlideVel.Length();
    if (fSlideSpeed > 3.5f)
      vSlideVel = vSlideVel * (3.5f / fSlideSpeed);
    mpPushBody->SetLinearVelocity(vSlideVel);
    mpPushBody->SetAngularVelocity(cVector3f(0.0f, 0.0f, 0.0f));
  }
  else
  {
    mpPushBody->SetLinearVelocity(cVector3f(0.0f, 0.0f, 0.0f));
    mpPushBody->SetAngularVelocity(cVector3f(0.0f, 0.0f, 0.0f));
    mpPushBody->AddForceAtPosition((destDiff * 2500.0f * mpPushBody->GetMass()) / 2.0f, mvPickPoint);
  }
  mlMoveCount = 20;

	//////////////////////////////////////
	//Calculate the pick position mnmbnm,bn,n
	mvPickPoint = cMath::MatrixMul(mpPushBody->GetLocalMatrix(),mvRelPickPoint);

  /////////////////////////////////////////////////
  // Cast ray to see if anything could be examined.
  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  vStart = mpPlayer->GetCamera()->GetPosition();
  vEnd = vStart + mpPlayer->GetCamera()->GetForward() * mpPlayer->GetExamineRay()->mfMaxDistance;

  mpPlayer->GetExamineRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetExamineRay(), vStart, vEnd, true, false, true);
  mpPlayer->GetExamineRay()->CalculateResults();

  //Log("Picked body: %d\n",(size_t)mpPlayer->GetPickedBody());

  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    //Call entity
    pEntity->PlayerPick();

    //Set cross hair state
    eCrossHairState CrossState = pEntity->GetPickCrossHairState(mpPlayer->GetExamineBody(), true);

    if ((CrossState == eCrossHairState_Examine && !pEntity->GetHasBeenExamined()))
    {
      mpPlayer->SetCrossHairState(CrossState);
    }
    else
    {
      mpPlayer->SetCrossHairState(eCrossHairState_None);
    }
  }
  else
  {
    mpPlayer->SetCrossHairState(eCrossHairState_None);
  }

  /////////////////////////////////////////////////
  // Cast ray to see if anything is picked for hands.
  cMatrixf poseMatrix;

  for (int i = 0; i < 2; ++i)
    mpPlayer->SetHandCrossHairState(i, eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_Move_VR::OnStartInteract()
{
	//mpPlayer->ChangeState(ePlayerState_Normal);
}

void cPlayerState_Move_VR::OnStopInteract()
{
	mpPlayer->ChangeState(mPrevState);
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnJump()
{ 
	return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Move_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnMoveForwards(float afMul, float afTimeStep)
{
	cVector3f vForce = (mvForward * (afMul * 4.0f));
	mpPushBody->AddForceAtPosition(vForce,mvPickPoint);
	mlMoveCount = 20;

	return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnMoveSideways(float afMul, float afTimeStep)
{
	cVector3f vForce = (mvRight * (afMul * 4.0f));
	mpPushBody->AddForceAtPosition(vForce,mvPickPoint);
	mlMoveCount = 20;

	return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnAddYaw(float afVal)
{ 
	if(std::abs(afVal) > kEpsilonf)
	{
		cVector3f vForce = (mvRight * (afVal * 100.0f * mpPlayer->mfRightMul));
		mpPushBody->AddForceAtPosition(vForce,mvPickPoint);
		mlMoveCount = 20;
	}
	else
	{
		if(mlMoveCount>0) mlMoveCount--;
	}

	return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnAddPitch(float afVal)
{
	if(std::abs(afVal) > kEpsilonf)
	{
		cVector3f vForce = (mvUp * (-afVal * 100.0f  * mpPlayer->mfUpMul)) + 
			(mvForward * (afVal * -80.0f *mpPlayer->mfForwardUpMul));
		mpPushBody->AddForceAtPosition(vForce,mvPickPoint);
		mlMoveCount = 20;
	}
	else
	{
		if(mlMoveCount>0) mlMoveCount--;
	}

	return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Move_VR::EnterState(iPlayerState* apPrevState)
{
	//Detach the body if stuck to a sticky area
	cGameStickArea *pStickArea = mpInit->mpMapHandler->GetBodyStickArea(mpPlayer->GetPushBody());
	if(pStickArea && pStickArea->GetCanDeatch())
	{
		if(pStickArea->GetCanDeatch()){
			pStickArea->DetachBody();
		}
		else {
			mpPlayer->ChangeState(apPrevState->mType);
			return;
		}
	}

  mpInit->mpPlayerHands->SetSlotVisible(VRHelper::DominantHandSlot(mpInit->mpGame), false);

	cCamera3D *pCamera = mpPlayer->GetCamera();

	//Change move state so the player is still
	mPrevMoveState = mpPlayer->GetMoveState();
	//mpPlayer->ChangeMoveState(ePlayerMoveState_Still);
	mpPlayer->SetSpeedMul(0.3f);
	mpPlayer->SetHeadMoveSizeMul(0.2f);
	mpPlayer->SetHeadMoveSpeedMul(0.2f);

	//Get last state, if this is a message use the last previous state instead.
	if(apPrevState->mType != ePlayerState_Message) mPrevState = apPrevState->mType;

	//Set the directions to move the body in
	mvForward = pCamera->GetForward();
	mvRight = pCamera->GetRight();
	mvUp = pCamera->GetUp();

	//make forward non y dependant
	mvForward.y =0;
	mvForward.Normalise();

	//Get the body to push
	mpPushBody = mpPlayer->GetPushBody();
	mpPlayer->SetVRHeldBody(eVRHandIndex_Right, mpPushBody);
	mpPushBody->SetAutoDisable(false);

	// Lift the map-defined velocity caps while dragging so the pull force or
	// servo is not strangled by Newton's per-step clamp.
	mfDefaultMaxLinSpeed = mpPushBody->GetMaxLinearSpeed();
	mfDefaultMaxAngSpeed = mpPushBody->GetMaxAngularSpeed();
	mpPushBody->SetMaxLinearSpeed(mpPushBody->GetJointNum() > 0 ? 5.0f : 10.0f);
	mpPushBody->SetMaxAngularSpeed(mpPushBody->GetJointNum() > 0 ? 8.0f : 15.0f);

	//The pick point relative to the body
  auto handMat = VRHelper::TrackingToWorldSpace(VRHelper::DominantHand(mpInit->mpGame).GetMatrix(), mpInit->mpGame);
	mvPickPoint = handMat.GetTranslation();

	/////////////////////////////////////////
	//Check if all controllers should be paused.
	iGameEntity *pEntity = (iGameEntity*)mpPushBody->GetUserData();
	if(pEntity->GetPauseControllers())
	{
		for(int i=0; i< mpPushBody->GetJointNum(); ++i)
		{
			mpPushBody->GetJoint(i)->SetAllControllersPaused(true);
		}
	}
	/////////////////////////////////////////
	//Check if gravtty should be paused.
	if(mpPushBody->GetGravity() && pEntity->GetPauseGravity())
	{
		mpPushBody->SetGravity(false);
		bPausedGravity = true;
	}
	else
	{
		bPausedGravity = false;
	}
cMatrixf mtxInvModel = cMath::MatrixInverse(mpPushBody->GetLocalMatrix());
	mvRelPickPoint = cMath::MatrixMul(mtxInvModel, mvPickPoint);

	localPickMatrix = cMath::MatrixMul(cMath::MatrixInverse(handMat), cMath::MatrixTranslate(mvPickPoint));

	//Set cross hair image.
	//mpPlayer->SetCrossHairState(eCrossHairState_Grab);

	//Add callback to body if needed
	/*if(mpPushBody->GetCollideCharacter())
	{
	mpPushBody->AddBodyCallback(mpCallback);
	}*/

	mpCallback->mlBackCount =0;

	mlMoveCount = 0;

  // Keep the native HPL1 player collision while moving hinged objects. The
  // former VR override made swing doors non-solid to the character body; a
  // player could then lean through a locked door and walk to its far side.
  if(pEntity != NULL && pEntity->GetType() == eGameEntityType_SwingDoor)
  {
    mpPushBody->SetCollidePlayer(true);
      Log(" [VR collision +%lu ms] swing-door interaction kept player collision enabled.\n",
        GetApplicationTime());
  }

  if (mpPushBody->GetJointNum() > 0)
  {
    iPhysicsJoint *pJoint = mpPushBody->GetJoint(0);
    cVector3f vPin = pJoint ? pJoint->GetPinDir() : cVector3f(0,0,0);
    cVector3f vPiv = pJoint ? pJoint->GetPivotPoint() : cVector3f(0,0,0);
    cVector3f vBody = mpPushBody->GetWorldPosition();
    Log(" [VR hinge +%lu ms] body '%s' joints=%d pin=(%.3f %.3f %.3f) pivot=(%.3f %.3f %.3f) body=(%.3f %.3f %.3f) dist=%.3f\n",
        GetApplicationTime(),
        mpPushBody->GetName().c_str(),
        mpPushBody->GetJointNum(),
        vPin.x, vPin.y, vPin.z,
        vPiv.x, vPiv.y, vPiv.z,
        vBody.x, vBody.y, vBody.z,
        (vBody - vPiv).Length());
  }

  mbHasSlideAxis = false;
  mvSlideAxis = cVector3f(0, 0, 0);
  if (pEntity && pEntity->GetType() == eGameEntityType_Object && mpPushBody->GetJointNum() == 0)
  {
    cGameObject *pObject = static_cast<cGameObject*>(pEntity);
    if (pObject->GetInteractMode() == eObjectInteractMode_Move ||
        pObject->GetInteractMode() == eObjectInteractMode_Grab)
    {
      iPhysicsBody *pFrame = NULL;
      iPhysicsBody *pDrawer = mpPushBody;
      for (int b = 0; b < pObject->GetBodyNum(); ++b)
      {
        iPhysicsBody *pCandidate = pObject->GetBody(b);
        if (pCandidate && pCandidate != mpPushBody && pCandidate->GetMass() == 0.0f)
        {
          pFrame = pCandidate;
          break;
        }
      }
      if (pFrame)
      {
        cVector3f vFramePos = pFrame->GetWorldPosition();
        cVector3f vDrawerPos = pDrawer->GetWorldPosition();
        cVector3f vAxis = vDrawerPos - vFramePos;
        float fLen = vAxis.Length();
        if (fLen > 0.01f)
        {
          mvSlideAxis = cMath::Vector3Normalize(vAxis);
          mbHasSlideAxis = true;
          Log(" [VR slide +%lu ms] body '%s' slide axis=(%.3f %.3f %.3f) dist=%.3f\n",
              GetApplicationTime(),
              mpPushBody->GetName().c_str(),
              mvSlideAxis.x, mvSlideAxis.y, mvSlideAxis.z, fLen);
        }
      }
    }
  }

	cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectPickup, VRHelper::DominantHapticHand(mpInit->mpGame));
}

//-----------------------------------------------------------------------

void cPlayerState_Move_VR::LeaveState(iPlayerState* apNextState)
{
	mpPlayer->SetVRHeldBody(eVRHandIndex_Right, NULL);

	//Remove callback to body if needed
	/*if(mpPushBody->GetCollideCharacter())
	{
	mpPushBody->RemoveBodyCallback(mpCallback);
	}*/

  mpInit->mpPlayerHands->SetSlotVisible(VRHelper::DominantHandSlot(mpInit->mpGame), true);

	// Restore the map-defined velocity caps
	mpPushBody->SetMaxLinearSpeed(mfDefaultMaxLinSpeed);
	mpPushBody->SetMaxAngularSpeed(mfDefaultMaxAngSpeed);

	////////////////////////////
	//Pause controllers
	iGameEntity *pObject = (iGameEntity*)mpPushBody->GetUserData();
	if(pObject->GetPauseControllers())
	{
		for(int i=0; i< mpPushBody->GetJointNum(); ++i)
		{
			mpPushBody->GetJoint(i)->SetAllControllersPaused(false);
		}
	}

	////////////////////////////
	//Pause gravity
	if(bPausedGravity)
	{
		mpPushBody->SetGravity(true);
	}

	mpPushBody->SetAutoDisable(true);

	if(	mPrevMoveState != ePlayerMoveState_Run && mPrevMoveState != ePlayerMoveState_Jump)
		mpPlayer->ChangeMoveState(mPrevMoveState);
	else
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);

	mpPlayer->SetSpeedMul(1.0f);
	mpPlayer->SetHeadMoveSizeMul(1.0f);
	mpPlayer->SetHeadMoveSpeedMul(1.0f);


	if(mPrevState == ePlayerState_Normal)
		mpPlayer->ResetCrossHairPos();

  cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectDrop, VRHelper::DominantHapticHand(mpInit->mpGame));
}

//-----------------------------------------------------------------------

void cPlayerState_Move_VR::OnPostSceneDraw()
{
	//mpInit->mpGame->GetGraphics()->GetLowLevel()->DrawSphere(mvPickPoint,0.1f,cColor(1,1,1,1));
}

//-----------------------------------------------------------------------
bool cPlayerState_Move_VR::OnStartInventory()
{
	return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Move_VR::OnStartInventoryShortCut(int alNum)
{ 
	return false;
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUSH STATE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cPlayerState_Push_VR::cPlayerState_Push_VR(cInit *apInit,cPlayer *apPlayer) : iPlayerState(apInit,apPlayer,ePlayerState_Push)
{
	mpPushBody = NULL;
}

//-----------------------------------------------------------------------


void cPlayerState_Push_VR::OnUpdate(float afTimeStep)
{	

  auto handMat = VRHelper::TrackingToWorldSpace(VRHelper::DominantHand(mpInit->mpGame).GetMatrix(), mpInit->mpGame);

  auto destDiff = handMat.GetTranslation() - (mpPushBody->GetLocalPosition() + mvRelPickPoint);
  auto moveDirection = cMath::Vector3Normalize(destDiff);
  moveDirection.y = 0.0f;

  mpPushBody->AddForce(moveDirection * 300.0f);
}

//-----------------------------------------------------------------------

bool cPlayerState_Push_VR::OnJump()
{ 
	return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Push_VR::OnStartInteract()
{
	//mpPlayer->ChangeState(mPrevState);
}

void cPlayerState_Push_VR::OnStopInteract()
{
	mpPlayer->ChangeState(mPrevState);
}

//-----------------------------------------------------------------------

void cPlayerState_Push_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_Push_VR::OnMoveForwards(float afMul, float afTimeStep)
{
	if(afMul<0){
		if(mpPlayer->mbCanBePulled==false) return false;		
		afMul *=0.7f;
	}

	float fSpeed = mpPushBody->GetLinearVelocity().Length();

	///////////////////////////////////
	// Set the direction and if it is newer add extra force	
	if(afMul>0)
	{
		if(mlForward!=1 && fSpeed<0.01f) afMul *=0.6f * mpPushBody->GetMass();
		mlForward =1;
	}
	else if(afMul<0)
	{
		//If player is to close, push him back.
		if(mlForward!=-1)
		{
			float fPosAdd = (mfMaxSpeed)*afTimeStep;
			iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
			iCharacterBody *pPlayerBody = mpPlayer->GetCharacterBody();

			cMatrixf mtxBodyMove = mpPushBody->GetLocalMatrix();
			mtxBodyMove.SetTranslation(mtxBodyMove.GetTranslation() + (mvForward*-1*fPosAdd));

			cCollideData collData;
			collData.SetMaxSize(32);
			bool bCollide = pPhysicsWorld->CheckShapeCollision(
				pPlayerBody->GetShape(), cMath::MatrixTranslate(pPlayerBody->GetPosition()),
				mpPushBody->GetShape(), mtxBodyMove,collData,32);

			if(bCollide)
			{
				cVector3f vPos = mpPlayer->GetCharacterBody()->GetPosition();
				cVector3f vOldPos = vPos;
				cVector3f vNewPos;
				vPos += mvForward * -1 * (fPosAdd+0.1f);
				mpPlayer->GetCharacterBody()->SetPosition(vPos);

				//Not good since slopes does not work:
				/*pPhysicsWorld->CheckShapeWorldCollision(&vNewPos, pPlayerBody->GetShape(),
											cMath::MatrixTranslate(pPlayerBody->GetPosition()),
											mpPushBody,false,true,NULL,false);
				
				if(vNewPos.x != vPos.x || vNewPos.y != vPos.y)
				{
					mpPlayer->GetCharacterBody()->SetPosition(vOldPos);
				}*/
			}
		}

		if(mlForward!=-1 && fSpeed<0.01f) afMul *=1.2f * mpPushBody->GetMass();
		mlForward = -1;
	}
	else
	{
		mlForward = 0;
		return false;
	}

	//Log("Mul: %f\n",afMul);

	/////////////////////////////////////
	//If the velocity is not to high add force.		

	if(fSpeed < mfMaxSpeed)
	{
		
		if(mpPlayer->mbPickAtPoint)
		{
			//NON WORKING ATM:
			/*cVector3f vForce = mvForward * afMul*100.0f*0.5f;

			cVector3f vPos = cMath::MatrixMul(	mpPushBody->GetLocalMatrix(), 
												mvLocalPickPoint);
			vPos.y = mpPushBody->GetLocalPosition().y;

			mpPushBody->AddForceAtPosition(vForce,vPos);*/
		}
		else
		{
			cVector3f vForce = mvForward * afMul*100.0f;//*0.5f;

			//mpPushBody->AddForceAtPosition(vForce,mpPushBody->GetLocalPosition() + mvRight*0.4f);
			//mpPushBody->AddForceAtPosition(vForce,mpPushBody->GetLocalPosition() + mvRight*-0.4f);
			mpPushBody->AddForce(vForce);
		}
	}

	//returning true calls move state as well.
	return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Push_VR::OnMoveSideways(float afMul, float afTimeStep)
{
	cVector3f vVel = mpPushBody->GetLinearVelocity();

	if(vVel.Length() < mfMaxSpeed)
	{
		//mpPushBody->AddForce(mvRight * afMul*100.0f);

		cVector3f vForce = mvRight * afMul*100.0f;//*0.5f;

		//mpPushBody->AddForceAtPosition(vForce,mpPushBody->GetLocalPosition() + mvForward*0.4f);
		//mpPushBody->AddForceAtPosition(vForce,mpPushBody->GetLocalPosition() + mvForward*-0.4f);
		mpPushBody->AddForce(vForce);
	}

	//returning true calls move state as well.
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_Push_VR::EnterState(iPlayerState* apPrevState)
{
	//Detach the body if stuck to a sticky area
	cGameStickArea *pStickArea = mpInit->mpMapHandler->GetBodyStickArea(mpPlayer->GetPushBody());
	if(pStickArea && pStickArea->GetCanDeatch())
	{
		if(pStickArea->GetCanDeatch()){
			pStickArea->DetachBody();
		}
		else {
			mpPlayer->ChangeState(apPrevState->mType);
			return;
		}
	}

  mpInit->mpPlayerHands->SetSlotVisible(VRHelper::DominantHandSlot(mpInit->mpGame), false);

	cCamera3D *pCamera = mpPlayer->GetCamera();

	mfMaxSpeed = mpPlayer->GetMaxPushSpeed();
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Crouch) mfMaxSpeed *= 0.5f;

	//Change move state so the player is still
	mPrevMoveState = mpPlayer->GetMoveState();
	mpPlayer->ChangeMoveState(ePlayerMoveState_Still);

	//Get last state, if this is a message use the last previous state instead.
	if(apPrevState->mType != ePlayerState_Message) mPrevState = apPrevState->mType;

	//Set the directions to move the body in
	mvForward = pCamera->GetForward();
	mvForward.y =0;
	mvForward.Normalise();

	mvRight = pCamera->GetRight();
	mvRight.y =0;
	mvRight.Normalise();

	//Get the body to push
	mpPushBody = mpPlayer->GetPushBody();
	mpPlayer->SetVRHeldBody(eVRHandIndex_Right, mpPushBody);

	//All pushed bodies shall be affected by player gravity.
	mbHasPlayerGravityPush = mpPushBody->GetPushedByCharacterGravity();
	mpPushBody->SetPushedByCharacterGravity(true);

	//Set the last position.
	mvLastBodyPos = mpPushBody->GetLocalPosition();

	//Set cross hair image.
	//mpPlayer->SetCrossHairState(eCrossHairState_Grab);

	//Set newer yaw and pitch limits
	mvPrevPitchLimits = pCamera->GetPitchLimits();

	cVector2f vMaxHeadLimits = mpPlayer->GetMaxPushHeadMovement();
	cVector2f vMinHeadLimits = mpPlayer->GetMinPushHeadMovement();
	float fXmax = pCamera->GetYaw() + vMaxHeadLimits.x;
	float fYmax = pCamera->GetPitch() + vMaxHeadLimits.y;

	float fXmin = pCamera->GetYaw() + vMinHeadLimits.x;
	float fYmin = pCamera->GetPitch() + vMinHeadLimits.y;

	pCamera->SetPitchLimits(cVector2f(fYmax, fYmin));
	pCamera->SetYawLimits(cVector2f(fXmax, fXmin));

	//Create a little effect when grabbing the body
	mpPushBody->SetAutoDisable(false);
	//mpPushBody->AddForce(cVector3f(0,-1,0) *60.0f *mpPushBody->GetMass());

	//This is to check the last used direction
	mlForward = 0;
	mlSideways = 0;

  auto handMat = VRHelper::TrackingToWorldSpace(VRHelper::DominantHand(mpInit->mpGame).GetMatrix(), mpInit->mpGame);

  //The pick point relative to the body
  mvRelPickPoint = handMat.GetTranslation() - mpPushBody->GetLocalPosition();

  // mpPushBody->SetCollidePlayer(false);

  cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectPickup, VRHelper::DominantHapticHand(mpInit->mpGame));
}

//-----------------------------------------------------------------------

void cPlayerState_Push_VR::LeaveState(iPlayerState* apNextState)
{
	mpPlayer->SetVRHeldBody(eVRHandIndex_Right, NULL);

  mpInit->mpPlayerHands->SetSlotVisible(VRHelper::DominantHandSlot(mpInit->mpGame), true);

	mpPushBody->SetPushedByCharacterGravity(mbHasPlayerGravityPush);

	if(mPrevMoveState != ePlayerMoveState_Run && mPrevMoveState != ePlayerMoveState_Jump)
		mpPlayer->ChangeMoveState(mPrevMoveState);
	else
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);

	mpPlayer->GetCamera()->SetPitchLimits(mvPrevPitchLimits);
	mpPlayer->GetCamera()->SetYawLimits(cVector2f(0, 0));

	//Create a little effect when letting go of the body
	mpPushBody->SetAutoDisable(true);
	//mpPushBody->AddForce(cVector3f(0,-1,0) *60.0f *mpPushBody->GetMass());

	cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectDrop, VRHelper::DominantHapticHand(mpInit->mpGame));
}


//-----------------------------------------------------------------------

void cPlayerState_Push_VR::OnPostSceneDraw()
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Push_VR::OnStartInventory()
{
	return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Push_VR::OnStartInventoryShortCut(int alNum)
{ 
	return false;
}
