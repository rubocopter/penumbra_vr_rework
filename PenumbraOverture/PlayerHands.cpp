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
#include "PlayerHands.h"

#include "Init.h"
#include "Player.h"
#include "PlayerHelper.h"

#include "HudModel_Weapon.h"
#include "HudModel_Throw.h"

#include "VRHelper.hpp"

static const char* ksFingerNames[] = {
	"Middle","Middle","Middle",
	"Ring","Ring","Ring",
	"Little","Little","Little",
	"Index","Index","Index",
	"Thumb","Thumb","Thumb"
};
static const char* ksHandBoneNames[] = {
	"Middle1","Middle2","Middle3",
	"Ring1","Ring2","Ring3",
	"Little1","Little2","Little3",
	"Index1","Index2","Index3",
	"Thumb1","Thumb2","Thumb3"
};
static const int lHandBoneNum = 15;

//////////////////////////////////////////////////////////////////////////
// HELP FUNCTIONS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cHudModelPose GetPoseFromElem(const tString &asName, TiXmlElement *apElem)
{
	cHudModelPose hudPose;
	
	tString sAttrPos = asName+"_Pos";
	tString sAttrRot = asName+"_Rot";

	hudPose.mvPos = cString::ToVector3f(apElem->Attribute(sAttrPos.c_str()),0);
	hudPose.mvRot =	cMath::Vector3ToRad(
										cString::ToVector3f(apElem->Attribute(sAttrRot.c_str()),0));

	return hudPose;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// HUD MODEL
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iHudModel::iHudModel(ePlayerHandType aType)
{
	mType = aType;
	mState = eHudModelState_Idle;
	mpHandAnimState[0] = NULL;
	mpHandAnimState[1] = NULL;
	mbHandRigSetup = false;
	mlHandIndex = 0;
}

//-----------------------------------------------------------------------

bool iHudModel::UpdatePoseMatrix(cMatrixf& aPoseMtx, float afTimeStep)
{
  auto vr_scaleMtx = cMath::MatrixScale(cVector3f(mfVrScale));
  auto vr_rotMtx = cMath::MatrixRotate(mvVrRotOffset, eEulerRotationOrder_XYZ);
  auto vr_transMtx = cMath::MatrixTranslate(mvVrTransOffset);

  cMatrixf vr_handMtx;
  if (mlHandIndex == 0) {
    if (!mpInit->mpGame->vr_left_hand.IsPoseValid()) return false;
    vr_handMtx = mpInit->mpGame->vr_left_hand.GetMatrix();
  }
  else {
    if (!mpInit->mpGame->vr_right_hand.IsPoseValid()) return false;
    vr_handMtx = mpInit->mpGame->vr_right_hand.GetMatrix();
  }

  aPoseMtx = vr_handMtx;

  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_transMtx);
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_rotMtx);
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_scaleMtx);

  aPoseMtx = VRHelper::TrackingToWorldSpace(aPoseMtx, mpInit->mpGame);

  return true;
}

void iHudModel::LoadEntities()
{
	cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
	
	//Mesh entity
	mpMesh->IncUserCount(); //entity is newer user.
	mpEntity = pWorld->CreateMeshEntity(msName,	mpMesh);

	// Create lights
	for(int i=0; i< mpMesh->GetLightNum(); i++)
	{
		iLight3D* pLight = mpMesh->CreateLightInWorld(msName,mpMesh->GetLight(i),mpEntity,pWorld);
		if(pLight){
			mvLightColors.push_back(pLight->GetDiffuseColor());
			mvLightRadii.push_back(pLight->GetFarAttenuation());
			mvLights.push_back(pLight);
		}
	}
	
	// Create billboards
	for(int i=0; i< mpMesh->GetBillboardNum(); i++)
	{
		cBillboard *pBill = mpMesh->CreateBillboardInWorld(msName,mpMesh->GetBillboard(i),mpEntity,pWorld);
		if(pBill) mvBillboards.push_back(pBill);
	}

	// Create particle systems
	for(int i=0; i< mpMesh->GetParticleSystemNum(); i++)
	{
		cParticleSystem3D *pPS = mpMesh->CreateParticleSystemInWorld(msName,mpMesh->GetParticleSystem(i),mpEntity,pWorld);
		if(pPS) mvParticleSystems.push_back(pPS);
	}

	// Create sounds entities
	for(int i=0; i< mpMesh->GetSoundEntityNum(); i++)
	{
		cSoundEntity *pSound = mpMesh->CreateSoundEntityInWorld(msName,mpMesh->GetSoundEntity(i),mpEntity,pWorld);
		if(pSound) mvSoundEntities.push_back(pSound);
	}

	LoadExtraEntites();

	SetupHandAnimation();
}

//-----------------------------------------------------------------------

void iHudModel::SetupHandAnimation()
{
	if(mpEntity == NULL || mbHandRigSetup) return;

	const char* sHand = mlHandIndex == 0 ? "left" : "right";

	Log(" [hand-rig] %s: entity has %d bone states\n", sHand, mpEntity->GetBoneStateNum());
	for(int i=0; i< mpEntity->GetBoneStateNum(); ++i)
	{
		cBoneState *pBone = mpEntity->GetBoneState(i);
		Log(" [hand-rig] %s: boneState[%d]='%s'\n", sHand, i, pBone->GetName());
	}

	//Own rotation per joint in degrees. Grab: middle/ring/little/thumb.
	//Trigger: index. (Cumulative: Middle/Ring 70/140/175, Little 55/110/150,
	//Index 55/110/155, Thumb 130/220. The grab angles were re-tuned for the
	//grip=1 fold capped so the fingertips stay near the palm zone instead of
	//~10 units below it (VR probe 2026-08-17: y=-10.2 at grip 0.7 with
	//70/70/35; 17/17/10 gives y~-3.5, near the inner palm face at y~-2.2).
	//M1/M2 are the long segments (6.75/4.54 u) and drive the drop; M3's lever
	//is 0.81 u. Little reduced from 55/55/40 to 30/30/20: with 55/55/40 its
	//tip crossed the ring base line (z=-0.93) into the ring zone by grip
	//~0.55; 30/30/20 keeps it in the pinky zone (z>=-1.3 at grip 0.7) with a
	//shallow curl toward the palm edge.
	static const float kfGrabAngles[lHandBoneNum] = {
		17,17,10,	//Middle
		17,17,10,	//Ring
		30,30,20,	//Little
		0,0,0,		//Index
		130,90,0	//Thumb
	};
	static const float kfTriggerAngles[lHandBoneNum] = {
		0,0,0,		//Middle
		0,0,0,		//Ring
		0,0,0,		//Little
		17,17,10,	//Index
		0,0,0		//Thumb
	};
	//Index trigger folded with the same 17/17/10 discipline as the grab
	//fingers: the old 55/55/45 (155 deg) plunged the tip ~7 units below the
	//palm face at real grips (y=-7.1 at grip 0.7; face at its z is -2.6).
	//Its (0,0,-1) axis is already within 1.9 deg of the anatomical vertical
	//axis of the index chain, so only the amount changed.

	int lBoneIdx[lHandBoneNum];
	for(int i=0; i<lHandBoneNum; ++i)
	{
		lBoneIdx[i] = mpEntity->GetBoneStateIndex(ksHandBoneNames[i]);
		mlBoneIdx[i] = lBoneIdx[i];
		if(lBoneIdx[i] < 0)
		{
			Log(" [hand-rig] %s: bone resolve FAILED for '%s' (idx=%d)\n",
				sHand, ksHandBoneNames[i], lBoneIdx[i]);
			mbHandRigSetup = false;
			return;
		}
	}

	const bool bLeft = (mlHandIndex == 0);
	cVector3f vFingerAxis = cVector3f(0,0, bLeft ? 1.0f : -1.0f);
	//Thumb axis derived from the bind geometry (plane through the thumb chain
	//and the palm centre): the old (0,0.982,0.187) folded in a nearly
	//horizontal plane, sweeping the short 3.5 u chain backwards at the hand
	//edge (tip y ~ -1.0, visually barely moving despite the 130/90 rotations);
	//the new plane sweeps the tip across toward the index at the palm's height
	//(y ~ -1.8). The chain is still too short to reach the palm flesh (model
	//limitation, not an angle issue). The little finger curls into the palm
	//with a 60 deg tilt (a plain finger axis would fold it straight down,
	//beside the ring finger, outside the palm).
	cVector3f vThumbAxis = cVector3f(0.065f, 0.9970f, bLeft ? 0.0460f : -0.0460f);
	cVector3f vPinkyAxis = cVector3f(0, -0.8660f, bLeft ? 0.5000f : -0.5000f);

	static const char* ksPoseNames[] = { "HandGrab", "HandTrigger" };
	static const float* kfPoseAngles[] = { kfGrabAngles, kfTriggerAngles };

	for(int p=0; p<2; ++p)
	{
		cAnimation *pAnim = hplNew( cAnimation, (ksPoseNames[p], "") );
		pAnim->SetLength(0.001f);

		for(int i=0; i<lHandBoneNum; ++i)
		{
			if(kfPoseAngles[p][i] == 0.0f) continue;

			cAnimationTrack *pTrack = pAnim->CreateTrack(ksHandBoneNames[i], eAnimTransformFlag_Rotate);
			pTrack->SetNodeIndex(lBoneIdx[i]);

			cKeyFrame *pFrame = pTrack->CreateKeyFrame(0);
			cVector3f vAxis = (i >= 12) ? vThumbAxis : (i >= 6 && i < 9) ? vPinkyAxis : vFingerAxis;
			pFrame->rotation = cQuaternion(cMath::ToRad(kfPoseAngles[p][i]), vAxis);

			Log(" [hand-rig] %s: pose=%s finger=%s boneName=%s boneIndex=%d "
				"axis=(%.4f,%.4f,%.4f) angle=%.1f appliedRotation=(w=%.4f,x=%.4f,y=%.4f,z=%.4f)\n",
				sHand, ksPoseNames[p], ksFingerNames[i], ksHandBoneNames[i], lBoneIdx[i],
				vAxis.x, vAxis.y, vAxis.z, kfPoseAngles[p][i],
				pFrame->rotation.w, pFrame->rotation.v.x, pFrame->rotation.v.y, pFrame->rotation.v.z);
		}

		mpHandAnimState[p] = mpEntity->AddAnimation(pAnim, ksPoseNames[p], 1.0f);
		mpHandAnimState[p]->SetActive(true);
		mpHandAnimState[p]->SetLoop(true);
		mpHandAnimState[p]->SetPaused(true);
		mpHandAnimState[p]->SetWeight(0.0f);
	}

	mbHandRigSetup = true;
}

//-----------------------------------------------------------------------

void iHudModel::DestroyHandAnimation()
{
	mpHandAnimState[0] = NULL;
	mpHandAnimState[1] = NULL;
	mbHandRigSetup = false;
}

//-----------------------------------------------------------------------

void iHudModel::UpdateHandAnimation()
{
	if(mpHandAnimState[0] == NULL) return;

	TrackedController &hand = mlHandIndex == 0 ? mpInit->mpGame->vr_left_hand
											   : mpInit->mpGame->vr_right_hand;
	const TrackedController::HandSummary &summary = hand.GetHandSummary();

	float fGrip = summary.valid ? summary.grip : 0.0f;
	float fTrigger = summary.valid ? summary.trigger : 0.0f;

	mpHandAnimState[0]->SetWeight(fGrip);
	mpHandAnimState[1]->SetWeight(fTrigger);

	static float sfLastGrip[2] = { -1.0f, -1.0f };
	static float sfLastTrigger[2] = { -1.0f, -1.0f };
	static bool sbLogPending[2] = { false, false };

	const char* sHand = mlHandIndex == 0 ? "left" : "right";
	bool bGripChanged = cMath::Abs(fGrip - sfLastGrip[mlHandIndex]) > 0.02f;
	bool bTriggerChanged = cMath::Abs(fTrigger - sfLastTrigger[mlHandIndex]) > 0.02f;
	sfLastGrip[mlHandIndex] = fGrip;
	sfLastTrigger[mlHandIndex] = fTrigger;

	//Log one frame after a weight change so the engine bone states
	//already reflect the new weights.
	if(bGripChanged || bTriggerChanged)
	{
		Log(" [hand-rig] %s: grip=%.3f trigger=%.3f\n", sHand, fGrip, fTrigger);
		sbLogPending[mlHandIndex] = true;
	}
	else if(sbLogPending[mlHandIndex])
	{
		sbLogPending[mlHandIndex] = false;
		for(int i=0; i<lHandBoneNum; ++i)
		{
			if(mlBoneIdx[i] < 0) continue;
			cBoneState *pState = mpEntity->GetBoneState(mlBoneIdx[i]);
			if(pState == NULL) continue;

			cQuaternion qRot;
			qRot.FromRotationMatrix(pState->GetLocalMatrix().GetRotation());

			float fAngle = cMath::ToDeg(2.0f * acos(cMath::Clamp(qRot.w, -1.0f, 1.0f)));
			float fSin = sqrtf(1.0f - qRot.w * qRot.w);
			cVector3f vAxis(0,0,0);
			if(fSin > 0.0001f) vAxis = qRot.v / fSin;

			float fWeight = (i >= 9 && i < 12) ? fTrigger : fGrip;
			Log(" [hand-rig] %s: finger=%s boneName=%s boneIndex=%d weight=%.3f "
				"engineRotation=(w=%.4f,x=%.4f,y=%.4f,z=%.4f) engineAngle=%.1f engineAxis=(%.4f,%.4f,%.4f)\n",
				sHand, ksFingerNames[i], ksHandBoneNames[i], mlBoneIdx[i], fWeight,
				qRot.w, qRot.v.x, qRot.v.y, qRot.v.z, fAngle, vAxis.x, vAxis.y, vAxis.z);
		}

		//Thumb probe: where the thumb tip ends up relative to the hand origin.
		if(mlBoneIdx[14] >= 0)
		{
			cBoneState *pThumb = mpEntity->GetBoneState(mlBoneIdx[14]);
			if(pThumb)
			{
				cVector3f vTip = pThumb->GetWorldMatrix().GetTranslation();
				cVector3f vHand = mpEntity->GetWorldMatrix().GetTranslation();
				Log(" [hand-rig] %s: thumbTip world=(%.3f,%.3f,%.3f) relHand=(%.3f,%.3f,%.3f) grip=%.3f\n",
					sHand, vTip.x, vTip.y, vTip.z,
					(vTip - vHand).x, (vTip - vHand).y, (vTip - vHand).z, fGrip);
			}
		}

		//Temporary tip probe (right hand): where the Middle3/Ring3 fingertips
		//land during the grab pose, measured from a distal mesh-tip proxy.
		//Logging only; remove once the grab re-tuning is done.
		if(mlHandIndex == 1)
		{
			const int lPalmIdx = mpEntity->GetBoneStateIndex("Palm");
			cBoneState *pPalm = (lPalmIdx >= 0) ? mpEntity->GetBoneState(lPalmIdx) : NULL;
			cVector3f vPalmPos = pPalm ? pPalm->GetWorldMatrix().GetTranslation() : cVector3f(0, 0, 0);

			//Distal tip offsets in the tip bone's local frame (right-hand model
			//hud_object_hand_rig.dae, bind pose, model units): the farthest bind
			//vertex weighted to the tip bone, projected onto the distal segment
			//axis (Middle2->Middle3 / Ring2->Ring3). The bone bind rotations are
			//identity, so the local offset equals the model-space offset from the
			//joint to the fingertip.
			static const cVector3f kfMiddleTipOffset(0.794f, 0.016f, -0.180f);
			static const cVector3f kfRingTipOffset(0.949f, -0.025f, 0.162f);

			for(int t=0; t<2; ++t)
			{
				const int lIdx0 = (t == 0) ? mlBoneIdx[0] : mlBoneIdx[3];
				const int lIdx1 = (t == 0) ? mlBoneIdx[1] : mlBoneIdx[4];
				const int lIdx2 = (t == 0) ? mlBoneIdx[2] : mlBoneIdx[5];
				const char* sFinger = (t == 0) ? "Middle" : "Ring";
				if(lIdx2 < 0) continue;

				cBoneState *pTip = mpEntity->GetBoneState(lIdx2);
				if(pTip == NULL) continue;

				const cMatrixf mTipWorld = pTip->GetWorldMatrix();
				const cVector3f vJointWorld = mTipWorld.GetTranslation();
				const cVector3f vTipWorld = cMath::MatrixMul(mTipWorld,
					(t == 0) ? kfMiddleTipOffset : kfRingTipOffset);
				cVector3f vTipPalmLocal = cVector3f(0, 0, 0);
				if(pPalm)
				{
					vTipPalmLocal = cMath::MatrixMul(cMath::MatrixInverse(pPalm->GetWorldMatrix()), vTipWorld);
				}

				float fAngles[3] = { 0.0f, 0.0f, 0.0f };
				const int lJoints[3] = { lIdx0, lIdx1, lIdx2 };
				for(int j=0; j<3; ++j)
				{
					if(lJoints[j] < 0) continue;
					cBoneState *pJoint = mpEntity->GetBoneState(lJoints[j]);
					if(pJoint == NULL) continue;
					cQuaternion qRot;
					qRot.FromRotationMatrix(pJoint->GetLocalMatrix().GetRotation());
					fAngles[j] = cMath::ToDeg(2.0f * acos(cMath::Clamp(qRot.w, -1.0f, 1.0f)));
				}

				Log(" [hand-rig] %s: tipProbe finger=%s boneName=%s boneIndex=%d "
					"grip=%.3f angles=(%.1f,%.1f,%.1f) "
					"jointWorld=(%.3f,%.3f,%.3f) tipWorld=(%.3f,%.3f,%.3f) "
					"tipPalmLocal=(%.3f,%.3f,%.3f) oldJointDistance=%.3f\n",
					sHand, sFinger, ksHandBoneNames[(t == 0) ? 2 : 5], lIdx2, fGrip,
					fAngles[0], fAngles[1], fAngles[2],
					vJointWorld.x, vJointWorld.y, vJointWorld.z,
					vTipWorld.x, vTipWorld.y, vTipWorld.z,
					vTipPalmLocal.x, vTipPalmLocal.y, vTipPalmLocal.z,
					pPalm ? (vJointWorld - vPalmPos).Length() : -1.0f);
			}
		}
	}
}

//-----------------------------------------------------------------------

void iHudModel::SetVisible(bool visible) {
  mpEntity->SetVisible(visible);
}

//-----------------------------------------------------------------------

void iHudModel::DestroyEntities()
{
	cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
	
	//Mesh entity
	pWorld->DestroyMeshEntity(mpEntity);
	mpEntity = NULL;
	
	//Particle systems
	for(size_t i=0; i < mvParticleSystems.size(); ++i)
	{
		pWorld->DestroyParticleSystem(mvParticleSystems[i]);
	}
	mvParticleSystems.clear();
	
	//Billboards
	for(size_t i=0; i < mvBillboards.size(); ++i)
	{
		pWorld->DestroyBillboard(mvBillboards[i]);
	}
	mvBillboards.clear();

	//Lights
	for(size_t i=0; i < mvLights.size(); ++i)
	{
		pWorld->DestroyLight(mvLights[i]);
	}
	mvLights.clear();

	//Sound entities
	for(size_t i=0; i < mvSoundEntities.size(); ++i)
	{
		if(pWorld->SoundEntityExists(mvSoundEntities[i]))
			pWorld->DestroySoundEntity(mvSoundEntities[i]);
	}
	mvSoundEntities.clear();

	DestroyExtraEntities();

	DestroyHandAnimation();
}

//-----------------------------------------------------------------------

void iHudModel::Reset()
{
	mfTime =0;
	ResetExtraData();
}

//-----------------------------------------------------------------------

void iHudModel::EquipEffect(bool abJustCreated)
{
  SetVisible(true);

	if(msEquipSound != "")
	{
		cSoundHandler *pSoundHanlder = mpInit->mpGame->GetSound()->GetSoundHandler();
		pSoundHanlder->PlayGui(msEquipSound,false,1);
	}

	for(size_t i=0; i< mvLights.size(); ++i)
	{
		if(abJustCreated) mvLights[i]->SetDiffuseColor(cColor(0,0));
		mvLights[i]->FadeTo(mvLightColors[i], mvLightRadii[i],0.3f);
	}
}

void iHudModel::UnequipEffect()
{
	if(msUnequipSound != "")
	{
		cSoundHandler *pSoundHanlder = mpInit->mpGame->GetSound()->GetSoundHandler();
		pSoundHanlder->PlayGui(msUnequipSound,false,1);
	}

	for(size_t i=0; i< mvLights.size(); ++i)
	{
		mvLights[i]->FadeTo(cColor(0,0), mvLightRadii[i],0.3f);
	}
}

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cPlayerHands::cPlayerHands(cInit *apInit)  : iUpdateable("FadeHandler")
{
	mpInit = apInit;

	mpMeshManager = mpInit->mpGame->GetResources()->GetMeshManager();

	mlCurrentModelNum = 2;

	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		mvCurrentHudModels[i] = NULL;
	}
}

//-----------------------------------------------------------------------

cPlayerHands::~cPlayerHands(void)
{
	///////////////////////////////////////////
	//Replace this and use some cache instead
	tHudModelMapIt it = m_mapHudModels.begin();
	for(; it != m_mapHudModels.end(); ++it)
	{
		iHudModel *pHandModel = it->second;

		mpMeshManager->Destroy(pHandModel->mpMesh);	
	}

	STLMapDeleteAll(m_mapHudModels);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cPlayerHands::OnStart()
{

}

//-----------------------------------------------------------------------

void cPlayerHands::Update(float afTimeStep)
{
	/////////////////////////////////////
	// Update the current model
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvCurrentHudModels[i];
    if (pHudModel == NULL) {continue;}

    const bool poseValid = i == 0 ? mpInit->mpGame->vr_left_hand.IsPoseValid() :
      mpInit->mpGame->vr_right_hand.IsPoseValid();
    if (!poseValid) {
      if (pHudModel->mpEntity != NULL) pHudModel->SetVisible(false);
      continue;
    }
    if (pHudModel->mpEntity != NULL) pHudModel->SetVisible(true);
		
		cMatrixf mtxPose = cMatrixf::Identity;

		////////////////////
		//Update state
		switch(pHudModel->mState)
		{
			//Idle
			case eHudModelState_Idle:
			{
        pHudModel->UpdatePoseMatrix(mtxPose, afTimeStep);

				break;
			}
			//Equip
			case eHudModelState_Equip:
			{
        pHudModel->UpdatePoseMatrix(mtxPose, afTimeStep);

        pHudModel->mState = eHudModelState_Idle;

				break;
			}
			//Unequip
			case eHudModelState_Unequip:
				{
            pHudModel->mState = eHudModelState_Idle;
						pHudModel->mfTime =0;
						
						pHudModel->DestroyEntities();

						mvCurrentHudModels[i] = NULL;
						
						if(pHudModel->msNextModel!="")
						{
							SetCurrentModel(i,pHudModel->msNextModel);
						}
						pHudModel->Reset();

						continue;
					break;
				}
		}

    if (pHudModel->mpEntity != nullptr)
      pHudModel->mpEntity->SetMatrix(mtxPose);

    pHudModel->UpdateHandAnimation();
	}
}

//-----------------------------------------------------------------------

void cPlayerHands::Reset()
{
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvCurrentHudModels[i];
		if(pHudModel)
		{
			pHudModel->DestroyEntities();
		}
		
		mvCurrentHudModels[i] = NULL;
	}
	
	tHudModelMapIt it = m_mapHudModels.begin();
	for(; it != m_mapHudModels.end(); ++it)
	{
		iHudModel *pModel = it->second;
		pModel->Reset();
	}
}

//-----------------------------------------------------------------------

void cPlayerHands::OnDraw()
{

}

//-----------------------------------------------------------------------

void cPlayerHands::AddHudModel(iHudModel* apHudModel)
{
	apHudModel->mpMesh = mpMeshManager->CreateMesh(apHudModel->msModelFile);
	apHudModel->mpInit = mpInit;
	
	m_mapHudModels.insert(tHudModelMap::value_type(cString::ToLowerCase(apHudModel->msName),
													apHudModel));
}

//-----------------------------------------------------------------------

static ePlayerHandType ToHandType(const char* apString)
{
	if(apString==NULL) return ePlayerHandType_Normal;
    
	tString sType = cString::ToLowerCase(apString);

	if(sType == "normal") return ePlayerHandType_Normal;
	else if(sType == "weaponmelee") return ePlayerHandType_WeaponMelee;
	else if(sType == "throw") return ePlayerHandType_Throw;
	
	return ePlayerHandType_Normal;
}

////////////////////////////////////

bool cPlayerHands::AddModelFromFile(const tString &asFile)
{
	tString sFileName = cString::SetFileExt(asFile,"hud");
	tString sPath = mpInit->mpGame->GetResources()->GetFileSearcher()->GetFilePath(sFileName);
	if(sPath=="")
	{
		Error("Couldn't find '%s' in resource directories!\n",sFileName.c_str());
		return false;
	}
	
	////////////////////////////////////////////////
	//Load the document
	TiXmlDocument *pXmlDoc = hplNew( TiXmlDocument, (sPath.c_str()) );
	if(pXmlDoc->LoadFile()==false){
		Error("Couldn't load XML document '%s'\n",sPath.c_str());
		hplDelete( pXmlDoc );
		return false;
	}
	
	////////////////////////////////////////////////
	//Load the root
	TiXmlElement *pRootElem = pXmlDoc->FirstChildElement();
	if(pRootElem==NULL){
		Error("Couldn't load root from XML document '%s'\n",sPath.c_str());
		hplDelete( pXmlDoc );
		return false;
	}
	
	////////////////////////////////////////////////
	//Load the MAIN element.
	TiXmlElement *pMainElem = pRootElem->FirstChildElement("MAIN");
	if(pMainElem==NULL){
		Error("Couldn't load MAIN element from XML document '%s'\n",sPath.c_str());
		hplDelete( pXmlDoc ); 
		return false;
	}

	ePlayerHandType handType = ToHandType(pMainElem->Attribute("Type"));
	iHudModel* pHudModel = NULL;
	
	switch(handType)
	{
	case ePlayerHandType_Normal:		pHudModel = hplNew( cHudModel_Normal,() ); break;
	case ePlayerHandType_WeaponMelee:	pHudModel = hplNew( cHudModel_WeaponMelee,() ); break;
	case ePlayerHandType_Throw:			pHudModel = hplNew( cHudModel_Throw,() ); break;
	}

	pHudModel->msName = cString::ToString(pMainElem->Attribute("Name"),"");
	pHudModel->msModelFile = cString::ToString(pMainElem->Attribute("ModelFile"),"");
	pHudModel->mfEquipTime = cString::ToFloat(pMainElem->Attribute("EquipTime"),0.3f);
	pHudModel->mfUnequipTime = cString::ToFloat(pMainElem->Attribute("UnequipTime"),0.3f);
	

	pHudModel->mEquipPose = GetPoseFromElem("EquipPose",pMainElem);
	
	pHudModel->mUnequipPose = GetPoseFromElem("UnequipPose",pMainElem);
	
	pHudModel->msEquipSound =  cString::ToString(pMainElem->Attribute("EquipSound"),"");
	pHudModel->msUnequipSound =  cString::ToString(pMainElem->Attribute("UnequipSound"),""); 

  pHudModel->mfVrScale = cString::ToFloat(pMainElem->Attribute("VrScale"), 1.0f);
  pHudModel->mvVrRotOffset = cString::ToVector3f(pMainElem->Attribute("VrRotOffset"), 0);
  pHudModel->mvVrTransOffset = cString::ToVector3f(pMainElem->Attribute("VrTransOffset"), 0);
	
	pHudModel->LoadData(pRootElem);

	AddHudModel(pHudModel);

	hplDelete( pXmlDoc );
	return true;
}

//-----------------------------------------------------------------------

void cPlayerHands::SetCurrentModel(int alNum,const tString& asName)
{
	//////////////////////////////////////////////
	//Check so that it is not already equipped
	if(mvCurrentHudModels[alNum] &&
		cString::ToLowerCase(asName) == cString::ToLowerCase(mvCurrentHudModels[alNum]->msName) &&
		mvCurrentHudModels[alNum]->mState == eHudModelState_Idle)
	{
		return;
	}
	
	///////////////////////////
	//Add a newer hud model
	if(asName != "")
	{
		tHudModelMapIt it = m_mapHudModels.find(cString::ToLowerCase(asName));
		if(it == m_mapHudModels.end())
		{
			Log(" Couldn't find hud model '%s'!\n",asName.c_str());
			return;
		}

		iHudModel *pHandModel = it->second;
		
		cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();

		if(mvCurrentHudModels[alNum])
		{
			if(mvCurrentHudModels[alNum] != pHandModel)
			{
				if(mvCurrentHudModels[alNum]->mState != eHudModelState_Unequip)
				{
					mvCurrentHudModels[alNum]->UnequipEffect();

					mvCurrentHudModels[alNum]->mState = eHudModelState_Unequip;
					mvCurrentHudModels[alNum]->mfTime = 1.0f - mvCurrentHudModels[alNum]->mfTime;
				}
				mvCurrentHudModels[alNum]->msNextModel = asName;
			}
			else
			{
				pHandModel->EquipEffect(false);

				mvCurrentHudModels[alNum]->mState = eHudModelState_Equip;
				mvCurrentHudModels[alNum]->mfTime = 1.0f - mvCurrentHudModels[alNum]->mfTime;
				mvCurrentHudModels[alNum]->msNextModel = asName;
			}
		}
		else
		{
			pHandModel->SetHandIndex(alNum);
			pHandModel->LoadEntities();
			pHandModel->EquipEffect(true);

			if(mvCurrentHudModels[alNum] == pHandModel)	{
				pHandModel->mfTime = 1.0f - pHandModel->mfTime;
			}
			else{
				pHandModel->mfTime =0;
			}

			pHandModel->mState = eHudModelState_Equip;

			mvCurrentHudModels[alNum] = pHandModel;
		}
	}
	///////////////////////////
	//Only remove old
	else
	{
		if(mvCurrentHudModels[alNum])
		{
			mvCurrentHudModels[alNum]->UnequipEffect();

			mvCurrentHudModels[alNum]->mState = eHudModelState_Unequip;
			mvCurrentHudModels[alNum]->mfTime = 1.0f - mvCurrentHudModels[alNum]->mfTime;
			mvCurrentHudModels[alNum]->msNextModel = asName;
		}
	}
}

//-----------------------------------------------------------------------

iHudModel* cPlayerHands::GetModel(const tString& asName)
{
	tHudModelMapIt it = m_mapHudModels.find(cString::ToLowerCase(asName));
	if(it == m_mapHudModels.end())
	{
		return NULL;
	}
	
	return it->second;
}

//-----------------------------------------------------------------------

void cPlayerHands::OnWorldExit()
{
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvCurrentHudModels[i];
		if(pHudModel)
		{
			pHudModel->DestroyEntities();
		}
	}
}

//-----------------------------------------------------------------------

void cPlayerHands::OnWorldLoad()
{
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvCurrentHudModels[i];
		if(pHudModel)
		{
			pHudModel->SetHandIndex(i);
			pHudModel->LoadEntities();
		}
	}
}

//-----------------------------------------------------------------------
