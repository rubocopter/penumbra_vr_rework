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
	for(int i=0; i<kFingerTrackNum; ++i)
	{
		mpFingerAnimState[i] = NULL;
		mfFingerWeight[i] = 0.0f;
	}
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

	// Hide collision submeshes (named *collision*, *collider*, etc., case
	// insensitive) so physics-only geometry never renders in HUD models.
	for(int i=0; i<mpEntity->GetSubMeshEntityNum(); ++i)
	{
		cSubMeshEntity *pSubEntity = mpEntity->GetSubMeshEntity(i);
		if(pSubEntity)
		{
			tString sLower = cString::ToLowerCase(pSubEntity->GetSubMesh()->GetName());
			if(sLower.find("collision") != tString::npos || sLower.find("collider") != tString::npos)
			{
				pSubEntity->SetVisible(false);
			}
		}
	}

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

	//Own rotation per joint in degrees (per-joint deltas; they accumulate
	//down each chain). Grab: middle/ring/little/thumb. Trigger: index.
	//Re-tuned 21 August 2026 (tune_finger_pose.py, engine-exact skinning):
	//- Angles rise from the old capped 17/17/10 set (tips barely moved,
	//  user-reported "poco agarre") to a fist-like wrap: Middle/Ring
	//  60/70/40 (170 deg total), Little 50/60/35, Index 45/55/32. Simulated
	//  adjacent-finger clearances stay >= the bind-pose baseline (Ring/
	//  Middle tubes are fused and only 0.09 apart at rest).
	//- Fold directions live in the axis block below: plain +/-Z for the
	//  four fingers (flipped per hand), YZ-tilted per-hand axis for the
	//  thumb.
	static const float kfGrabAngles[lHandBoneNum] = {
		60,70,40,	//Middle
		60,70,40,	//Ring
		60,70,40,	//Little: exact Ring/Middle parity - the pinky chain owns
					//~140 extra web/hypothenar vertices, and any differential
					//fold against Ring sheared that skin toward the ring lane.
					//Identical transforms keep the fused skin coherent; its
					//fold axis below additionally steers the deep curl away
					//from the Ring lane (tips bunching read as contortion).
		0,0,0,		//Index
		0,0,0		//Thumb frozen at bind pose: the chain is too short to reach
					//anything meaningful, every sweep tried read as invisible
					//(user-confirmed), so per user decision it stays static.
	};
	static const float kfTriggerAngles[lHandBoneNum] = {
		0,0,0,		//Middle
		0,0,0,		//Ring
		0,0,0,		//Little
		45,55,32,	//Index
		0,0,0		//Thumb
	};

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
	//Fold directions re-derived after hardware feedback (21 August 2026,
	//scripts/verify_fold_directions.py):
	//- The left rig is a Y-mirror of the right (palmar flesh median at
	//  -1.95 vs +1.95 relative to each Palm joint), so EVERY fold axis must
	//  flip its X/Z sign on the left hand or the fingers curl into the back
	//  of the hand instead of the palm.
	//- Middle/Ring/Little/Index share one plain +/-Z axis. Their anatomical
	//  verticals differ by up to 23 deg, but Ring/Middle share a fused mesh
	//  tube: curling around diverging axes stretches that seam 3.66 units
	//  (user-reported "double finger"); the shared axis keeps it at 0.71
	//  and gives exactly zero lateral drift.
	//- Pinky axis tilted 15 deg out of the shared sagittal plane so its deep
	//  curl drifts ~1.2 units away from the Ring lane instead of bunching
	//  against it (user-reported contortion toward the ring finger).
	cVector3f vFingerAxis = cVector3f(0, 0, -1.0f * (bLeft ? -1.0f : 1.0f));
	cVector3f vPinkyAxis = cVector3f(0.0f, 0.2588f, -0.9659f * (bLeft ? -1.0f : 1.0f));
	//- Thumb axis kept for reference; the thumb pose is frozen at zero angles
	//  above, so no Thumb track is created and this vector stays unused.
	cVector3f vThumbAxis = cVector3f(0.0f, 0.94f, -0.342f * (bLeft ? -1.0f : 1.0f));

	//One pose animation per finger chain so each finger can be driven and
	//smoothed independently instead of moving in lockstep with one blend
	//weight per hand.
	static const char* ksPoseNames[kFingerTrackNum] = {
		"HandMiddle","HandRing","HandLittle","HandIndex","HandThumb"
	};
	static const int klChainStart[kFingerTrackNum] = { 0,3,6,9,12 };
	static const float* kpChainAngles[kFingerTrackNum] = {
		kfGrabAngles,kfGrabAngles,kfGrabAngles,kfTriggerAngles,kfGrabAngles
	};

	for(int f=0; f<kFingerTrackNum; ++f)
	{
		cAnimation *pAnim = hplNew( cAnimation, (ksPoseNames[f], "") );
		pAnim->SetLength(0.001f);

		for(int j=0; j<3; ++j)
		{
			int i = klChainStart[f] + j;
			if(kpChainAngles[f][i] == 0.0f) continue;

			cAnimationTrack *pTrack = pAnim->CreateTrack(ksHandBoneNames[i], eAnimTransformFlag_Rotate);
			pTrack->SetNodeIndex(lBoneIdx[i]);

			cKeyFrame *pFrame = pTrack->CreateKeyFrame(0);
			cVector3f vAxis = (i >= 12) ? vThumbAxis
							: (i >= 6 && i < 9) ? vPinkyAxis : vFingerAxis;
			pFrame->rotation = cQuaternion(cMath::ToRad(kpChainAngles[f][i]), vAxis);

			Log(" [hand-rig] %s: pose=%s finger=%s boneName=%s boneIndex=%d "
				"axis=(%.4f,%.4f,%.4f) angle=%.1f appliedRotation=(w=%.4f,x=%.4f,y=%.4f,z=%.4f)\n",
				sHand, ksPoseNames[f], ksFingerNames[i], ksHandBoneNames[i], lBoneIdx[i],
				vAxis.x, vAxis.y, vAxis.z, kpChainAngles[f][i],
				pFrame->rotation.w, pFrame->rotation.v.x, pFrame->rotation.v.y, pFrame->rotation.v.z);
		}

		mpFingerAnimState[f] = mpEntity->AddAnimation(pAnim, ksPoseNames[f], 1.0f);
		mpFingerAnimState[f]->SetActive(true);
		mpFingerAnimState[f]->SetLoop(true);
		mpFingerAnimState[f]->SetPaused(true);
		mpFingerAnimState[f]->SetWeight(0.0f);
		mfFingerWeight[f] = 0.0f;
	}

	mbHandRigSetup = true;
}

//-----------------------------------------------------------------------

void iHudModel::DestroyHandAnimation()
{
	for(int i=0; i<kFingerTrackNum; ++i)
	{
		mpFingerAnimState[i] = NULL;
		mfFingerWeight[i] = 0.0f;
	}
	mbHandRigSetup = false;
}

//-----------------------------------------------------------------------

//Remap a raw 0..1 input through the [aLead,aFull] closing window of one
//finger, with an ease-in-out curve. Staggered windows reproduce the natural
//closing sequence (pinky/ring lead, middle follows, thumb opposes last)
//instead of every finger folding in lockstep.
static float CurlFromWindow(float afValue, float afLead, float afFull)
{
	float fT = (afFull - afLead) > 0.0001f ? (afValue - afLead) / (afFull - afLead) : afValue;
	fT = cMath::Clamp(fT, 0.0f, 1.0f);
	return fT * fT * (3.0f - 2.0f * fT);
}

//Rescale a raw curl through a soft deadzone: devices report a small
//constant resting curl (hpl.log showed thumb 0.063, index ~0.10 at rest),
//which would otherwise leave the hands half-curled while open.
static float CurlDeadzone(float afValue)
{
	const float kfDeadzone = 0.08f;
	if(afValue <= kfDeadzone) return 0.0f;
	return (afValue - kfDeadzone) / (1.0f - kfDeadzone);
}

void iHudModel::UpdateHandAnimation(float afTimeStep)
{
	if(mpFingerAnimState[0] == NULL || !mbHandRigSetup) return;

	TrackedController &hand = mlHandIndex == 0 ? mpInit->mpGame->vr_left_hand
											   : mpInit->mpGame->vr_right_hand;
	const TrackedController::HandSummary &summary = hand.GetHandSummary();

	//Per-finger target curl, rig chain order: Middle,Ring,Little,Index,Thumb.
	float afTarget[kFingerTrackNum] = {0,0,0,0,0};
	float fGrip = 0.0f;
	float fTrigger = 0.0f;
	//Set below from the live grip: many devices without true thumb tracking
	//report a constant ~0 thumb curl, which would otherwise leave the thumb
	//frozen while the rest of the hand closes.
	float fThumbFromGrip = 0.0f;

	if(summary.valid)
	{
		fGrip = summary.grip;
		fTrigger = summary.trigger;
		fThumbFromGrip = CurlFromWindow(fGrip, 0.15f, 0.98f);

		if(summary.skeletal)
		{
			//Controllers with skeletal input drive every finger directly from
			//its measured curl (VR finger order: Thumb,Index,Middle,Ring,Pinky).
			afTarget[0] = CurlDeadzone(summary.fingerCurl[vr::VRFinger_Middle]);
			afTarget[1] = CurlDeadzone(summary.fingerCurl[vr::VRFinger_Ring]);
			afTarget[2] = CurlDeadzone(summary.fingerCurl[vr::VRFinger_Pinky]);
			afTarget[3] = CurlDeadzone(summary.fingerCurl[vr::VRFinger_Index]);
			afTarget[4] = cMath::Max(CurlDeadzone(summary.fingerCurl[vr::VRFinger_Thumb]),
									 fThumbFromGrip);
		}
		else
		{
			//Legacy devices only report a grip/trigger pair: synthesize the
			//natural closing sequence from staggered windows. The index stays
			//tied to the trigger exactly like before.
			afTarget[0] = CurlFromWindow(fGrip,    0.10f, 0.95f); //Middle
			afTarget[1] = CurlFromWindow(fGrip,    0.05f, 0.88f); //Ring
			afTarget[2] = CurlFromWindow(fGrip,    0.00f, 0.80f); //Little
			afTarget[3] = CurlFromWindow(fTrigger, 0.00f, 0.85f); //Index
			afTarget[4] = fThumbFromGrip;                         //Thumb
		}
	}

	//Exponential smoothing toward the targets (~70 ms time constant): removes
	//sensor jitter and gives the joints a slight inertia without adding
	//perceptible lag at VR frame rates.
	float fDt = afTimeStep < 0.0f ? 0.0f : (afTimeStep > 0.1f ? 0.1f : afTimeStep);
	float fBlend = 1.0f - expf(-fDt / 0.07f);

	for(int f=0; f<kFingerTrackNum; ++f)
	{
		mfFingerWeight[f] += (afTarget[f] - mfFingerWeight[f]) * fBlend;
		mpFingerAnimState[f]->SetWeight(mfFingerWeight[f]);
	}

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
		Log(" [hand-rig] %s: grip=%.3f trigger=%.3f skeletal=%d\n", sHand, fGrip, fTrigger,
			summary.valid && summary.skeletal ? 1 : 0);
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

			//Bones are laid out in chains of three, matching the track order.
			float fWeight = mfFingerWeight[i / 3];
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

    pHudModel->UpdateHandAnimation(afTimeStep);

		// Call per-model update for things like throw velocity tracking
		pHudModel->Update(afTimeStep);
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
