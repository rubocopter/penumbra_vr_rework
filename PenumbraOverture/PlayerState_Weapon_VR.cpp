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
#include "PlayerState_Weapon_VR.h"

#include "Init.h"
#include "Player.h"

#include "HudModel_Weapon.h"
#include "HudModel_Throw.h"
#include "VRHelper.hpp"


//////////////////////////////////////////////////////////////////////////
// THROW STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_Throw_VR::cPlayerState_Throw_VR(cInit *apInit,cPlayer *apPlayer) : iPlayerState(apInit,apPlayer,ePlayerState_Throw)
{
	mfLastForward = 1.0f;
	mfLastSideways = 1.0f;

	mpHudObject = NULL;
}

//-----------------------------------------------------------------------


void cPlayerState_Throw_VR::OnUpdate(float afTimeStep)
{	
	if(	mpInit->mpNotebook->IsActive()==false && 
		mpInit->mpInventory->IsActive()==false &&
		mpInit->mpNumericalPanel->IsActive()==false &&
		mpInit->mpDeathMenu->IsActive()==false)
	{
		mpPlayer->ResetCrossHairPos();
	}

	/////////////////////////////////////
	// If run is down, run!!
	cInput *pInput = mpInit->mpGame->GetInput();
	if(	pInput->IsTriggerd("Run") && 
		mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
	}

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

	
	//Quick fix but works, the OnLeave of the previous states sets an old cross hair state.
	//mpPlayer->SetCrossHairState(eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnDraw()
{

}

//-----------------------------------------------------------------------


void cPlayerState_Throw_VR::OnPostSceneDraw()
{
	//mpHudObject->PostSceneDraw();
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnJump()
{ 
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStartInteractMode()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStartInteract()
{
	mpHudObject->OnAttackDown();
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStopInteract()
{
	mpHudObject->OnAttackUp();
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStartHolster()
{
	mpInit->mpPlayerHands->SetAttachmentModel(VRHelper::DominantHandSlot(mpInit->mpGame),"");
	mpInit->mpPlayer->ChangeState(ePlayerState_Normal);
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnAddYaw(float afVal)
{ 
	return true;//mpHudObject->OnMouseMove(cVector2f(afVal,0));
}

bool cPlayerState_Throw_VR::OnAddPitch(float afVal)
{
	return true;//return mpHudObject->OnMouseMove(cVector2f(0,afVal));
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnMoveForwards(float afMul, float afTimeStep)
{	
	mfLastForward = afMul;
	return true;
	
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnMoveSideways(float afMul, float afTimeStep)
{
	mfLastSideways = afMul;
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::EnterState(iPlayerState* apPrevState)
{
	mpHudObject->OnStart();
}

//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::LeaveState(iPlayerState* apNextState)
{
	if(	apNextState->mType != ePlayerState_Message &&
		apNextState->mType != ePlayerState_WeaponMelee &&
		mpInit->mpPlayerHands->FindHandModel(VRHelper::DominantHandSlot(mpInit->mpGame), mpHudObject ? mpHudObject->msName : "") == mpHudObject)
	{
		mpInit->mpPlayerHands->SetAttachmentModel(VRHelper::DominantHandSlot(mpInit->mpGame),"");
	}
}

//-----------------------------------------------------------------------

// No running when you have a weapon:
void cPlayerState_Throw_VR::OnStartRun()
{
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
		mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
	if( mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
	{
		mpPlayer->SetPrevMoveState(ePlayerMoveState_Run);
	}
}
void cPlayerState_Throw_VR::OnStopRun()
{
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Run)
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
	else if(mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
		mpPlayer->SetPrevMoveState(ePlayerMoveState_Walk);
}
//-----------------------------------------------------------------------

void cPlayerState_Throw_VR::OnStartCrouch()
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

void cPlayerState_Throw_VR::OnStopCrouch()
{
	if( mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
		mpInit->mpButtonHandler->GetToggleCrouch()==false)
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
	}
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnStartInventory()
{
	//mpPlayer->ChangeState(ePlayerState_Normal);
	return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Throw_VR::OnStartInventoryShortCut(int alNum)
{ 
	return true;
}
//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// WEAPON MELEE STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_WeaponMelee_VR::cPlayerState_WeaponMelee_VR(cInit *apInit,cPlayer *apPlayer) : iPlayerState(apInit,apPlayer,ePlayerState_WeaponMelee)
{
	mfLastForward = 1.0f;
	mfLastSideways = 1.0f;
}

//-----------------------------------------------------------------------


void cPlayerState_WeaponMelee_VR::OnUpdate(float afTimeStep)
{	
	if(	mpInit->mpNotebook->IsActive()==false && 
		mpInit->mpInventory->IsActive()==false &&
		mpInit->mpNumericalPanel->IsActive()==false &&
		mpInit->mpDeathMenu->IsActive()==false)
	{
		mpPlayer->ResetCrossHairPos();
	}

	/////////////////////////////////////
	// If run is down, run!!
	cInput *pInput = mpInit->mpGame->GetInput();
	if(	pInput->IsTriggerd("Run") && 
		mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
	}

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
	
	//Quick fix but works, the OnLeave of the previous states sets an old cross hair state.
	//mpPlayer->SetCrossHairState(eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnDraw()
{

}

//-----------------------------------------------------------------------


void cPlayerState_WeaponMelee_VR::OnPostSceneDraw()
{
	mpHudWeapon->PostSceneDraw();
}

//-----------------------------------------------------------------------

bool cPlayerState_WeaponMelee_VR::OnJump()
{ 
	return true;
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStartInteractMode()
{
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStartInteract()
{
	mpHudWeapon->OnAttackDown();
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStopInteract()
{
	mpHudWeapon->OnAttackUp();
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStartHolster()
{
	mpInit->mpPlayer->ChangeState(ePlayerState_Normal);
}

//-----------------------------------------------------------------------

bool cPlayerState_WeaponMelee_VR::OnAddYaw(float afVal)
{ 
	return mpHudWeapon->OnMouseMove(cVector2f(afVal,0));
}

bool cPlayerState_WeaponMelee_VR::OnAddPitch(float afVal)
{
	return mpHudWeapon->OnMouseMove(cVector2f(0,afVal));
}

//-----------------------------------------------------------------------

bool cPlayerState_WeaponMelee_VR::OnMoveForwards(float afMul, float afTimeStep)
{	
	{
		mfLastForward = afMul;
		return true;
	}
}

//-----------------------------------------------------------------------

bool cPlayerState_WeaponMelee_VR::OnMoveSideways(float afMul, float afTimeStep)
{
	{
		mfLastSideways = afMul;
		return true;
	}


}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::EnterState(iPlayerState* apPrevState)
{
	//get the callback

	//Try not doing this.
	//mpPlayer->GetGlowStick()->SetActive(false);
}

//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::LeaveState(iPlayerState* apNextState)
{
	if(	apNextState->mType != ePlayerState_Message &&
		apNextState->mType != ePlayerState_Throw &&
		mpInit->mpPlayerHands->FindHandModel(VRHelper::DominantHandSlot(mpInit->mpGame), mpHudWeapon ? mpHudWeapon->msName : "") == mpHudWeapon)
	{
		mpInit->mpPlayerHands->SetAttachmentModel(VRHelper::DominantHandSlot(mpInit->mpGame),"");
	}
}

//-----------------------------------------------------------------------

// No running when you have a weapon:
void cPlayerState_WeaponMelee_VR::OnStartRun()
{
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
		mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
	if( mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
	{
		mpPlayer->SetPrevMoveState(ePlayerMoveState_Run);
	}
}
void cPlayerState_WeaponMelee_VR::OnStopRun()
{
	if(mpPlayer->GetMoveState() == ePlayerMoveState_Run)
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
	else if(mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
		mpPlayer->SetPrevMoveState(ePlayerMoveState_Walk);
}
//-----------------------------------------------------------------------

void cPlayerState_WeaponMelee_VR::OnStartCrouch()
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

void cPlayerState_WeaponMelee_VR::OnStopCrouch()
{
	if( mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
		mpInit->mpButtonHandler->GetToggleCrouch()==false)
	{
		mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
	}
}

//-----------------------------------------------------------------------

bool cPlayerState_WeaponMelee_VR::OnStartInventory()
{
	//mpPlayer->ChangeState(ePlayerState_Normal);
	return true;
}

//-----------------------------------------------------------------------
bool cPlayerState_WeaponMelee_VR::OnStartInventoryShortCut(int alNum)
{ 
	return true;
}

