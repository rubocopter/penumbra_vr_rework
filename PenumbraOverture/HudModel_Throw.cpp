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
#include "HudModel_Throw.h"

#include "Init.h"
#include "Player.h"
#include "PlayerHelper.h"
#include "AttackHandler.h"
#include "GameEntity.h"
#include "GameEnemy.h"
#include "MapHandler.h"
#include "Inventory.h"
#include "VRHelper.hpp"


/////////////////////////////////////////////////////////////////////////
// HUD MODEL THROW
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cHudModel_Throw::cHudModel_Throw() : iHudModel(ePlayerHandType_Throw)
{
	mbButtonDown = false;

	mfChargeCount = 0;
	mfPeakSpeed = 0.0f;
	mvPeakVelocity = cVector3f(0, 0, 0);
	mbTrackingVelocity = false;
}

//-----------------------------------------------------------------------

void cHudModel_Throw::LoadData(TiXmlElement *apRootElem)
{
	////////////////////////////////////////////////
	//Load the MAIN element.
	TiXmlElement *pMeleeElem = apRootElem->FirstChildElement("THROW");
	if(pMeleeElem==NULL){
		Error("Couldn't load THROW element from XML document\n");
		return;
	}
	
	//mbDrawDebug = cString::ToBool(pMeleeElem->Attribute("DrawDebug"),false);
	mChargePose = GetPoseFromElem("ChargePose",pMeleeElem);

	mfChargeTime = cString::ToFloat(pMeleeElem->Attribute("ChargeTime"),0);
	mfMinImpulse = cString::ToFloat(pMeleeElem->Attribute("MinImpulse"),0);
	mfMaxImpulse = cString::ToFloat(pMeleeElem->Attribute("MaxImpulse"),0);

	mfReloadTime = cString::ToFloat(pMeleeElem->Attribute("ReloadTime"),0);

	msThrowEntity = cString::ToString(pMeleeElem->Attribute("ThrowEntity"),"");

	msChargeSound = cString::ToString(pMeleeElem->Attribute("ChargeSound"),"");
	msThrowSound = cString::ToString(pMeleeElem->Attribute("ThrowSound"),"");

  mfVrScale = 3.0f;
}

//-----------------------------------------------------------------------

void cHudModel_Throw::OnStart()
{
	mbButtonDown = false;
	mfChargeCount = 0;
	mfPeakSpeed = 0.0f;
	mvPeakVelocity = cVector3f(0, 0, 0);
	mbTrackingVelocity = false;
}

//-----------------------------------------------------------------------

void cHudModel_Throw::ResetExtraData()
{
	mbButtonDown = false;
	mfChargeCount = 0;
	mvPeakVelocity = cVector3f(0, 0, 0);
	mfPeakSpeed = 0.0f;
	mbTrackingVelocity = false;
}

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------

void cHudModel_Throw::OnAttackDown()
{
	if(mState == eHudModelState_Idle)
	{
		mbButtonDown = true;
		mbTrackingVelocity = true;
		mvPeakVelocity = cVector3f(0, 0, 0);
		mfPeakSpeed = 0.0f;
		mfChargeCount = 0.0f;
		
		if(msChargeSound != "")
		{
			cSoundHandler *pSoundHandler = mpInit->mpGame->GetSound()->GetSoundHandler();
			pSoundHandler->PlayGui(msChargeSound,false,1.0f);
		}
	}
}

//-----------------------------------------------------------------------

void cHudModel_Throw::Update(float afTimeStep)
{
	if(mbButtonDown && mbTrackingVelocity)
	{
		// Track peak velocity during throw motion
		cVector3f handVel = VRHelper::DominantHand(mpInit->mpGame).GetVelocity();
		float handSpeed = handVel.Length();
		
		if(handSpeed > mfPeakSpeed)
		{
			mfPeakSpeed = handSpeed;
			mvPeakVelocity = handVel;
		}
		
		// Increment charge count
		if(mfChargeTime > 0.0f)
		{
			mfChargeCount += afTimeStep / mfChargeTime;
			if(mfChargeCount > 1.0f) mfChargeCount = 1.0f;
		}
	}
}

void cHudModel_Throw::OnAttackUp()
{
	if(mbButtonDown == false) return;
	
	mbButtonDown = false;
	mbTrackingVelocity = false;
	
	//Play sound
	if(msThrowSound != "")
	{
		cSoundHandler *pSoundHandler = mpInit->mpGame->GetSound()->GetSoundHandler();
		pSoundHandler->PlayGui(msThrowSound,false,1.0f);
	}

///////////////////////////////
	//Create entity
	cMatrixf mtxStart = cMatrixf::Identity;
	iHudModel *pHand = mpInit->mpPlayerHands->GetCurrentModel(VRHelper::DominantHandSlot(mpInit->mpGame));
	if(pHand == NULL || pHand->UpdatePoseMatrix(mtxStart, 0.0f) == false)
	{
		// No valid hand pose: aborting is safer than spawning at a garbage
		// transform (UpdatePoseMatrix leaves the matrix untouched on failure).
		mfChargeCount = 0;
		return;
	}

	iEntity3D *pEntity = mpInit->mpGame->GetScene()->GetWorld3D()->CreateEntity("Throw",mtxStart,
																				msThrowEntity, true);
	if(pEntity)
	{
		iGameEntity *pGameEnt = mpInit->mpMapHandler->GetLatestEntity();

		// Raw hand velocities live in tracking space; rotate them into world
		// space with the pure direction transform (the full T2W would add the
		// player's world position to the velocity vector).
		cVector3f vWorldPeakVel = mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(mvPeakVelocity);
		float peakSpeed = vWorldPeakVel.Length();

		// Direction of the throw: the dominant motion arc when it was fast
		// enough to be meaningful, otherwise where the hand is pointing.
		cVector3f throwDir;
		if(peakSpeed > 0.3f)
		{
			throwDir = cMath::Vector3Normalize(vWorldPeakVel);
		}
		else
		{
			throwDir = cMath::Vector3Normalize(
				cMath::MatrixMul(mtxStart.GetRotation(), cVector3f(0, -1, 0)));
		}

		// Magnitude: real gesture speed slightly amplified, plus a small push
		// from the charge so a held release throws harder than a flick.
		float fChargeBoost = mfMinImpulse * (1 - mfChargeCount) + mfMaxImpulse * mfChargeCount;
		cVector3f throwVel = vWorldPeakVel * 1.15f + throwDir * fChargeBoost;

		cVector3f vWorldAngVel = mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(
			VRHelper::DominantHand(mpInit->mpGame).GetAngularVelocity());

		for(int i=0; i< pGameEnt->GetBodyNum(); ++i)
		{
			iPhysicsBody *pBody = pGameEnt->GetBody(i);
			pBody->SetLinearVelocity(throwVel);
			pBody->SetAngularVelocity(vWorldAngVel * 0.5f);
			pBody->SetActive(true);
			pBody->SetEnabled(true);
		}
	}

	mpInit->mpPlayer->GetHidden()->UnHide();

	//////////////////////
	//Reset animations

	mfChargeCount = 0; 

	mfTime = -mfReloadTime;
	mState = eHudModelState_Equip;

	//////////////////////
	//Decrease item amount
	mpItem->AddCount(-1);
	if(mpItem->GetCount()<=0)
	{
		mfTime = 0.0f;
		mpInit->mpInventory->RemoveItem(mpItem);
		mpInit->mpPlayerHands->SetCurrentModel(VRHelper::DominantHandSlot(mpInit->mpGame),"");
		mpInit->mpPlayer->ChangeState(ePlayerState_Normal);
	}

}

//-----------------------------------------------------------------------

bool cHudModel_Throw::OnMouseMove(const cVector2f &avMovement)
{
	return false;
}

//-----------------------------------------------------------------------
