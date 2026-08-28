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
		mfFingerWeight[i] = 0.0f;
	}
	mbHandRigSetup = false;
	mlHandIndex = 0;
	mbForceGrabPose = false;
	mfForcedGrabPoseWeight = 1.0f;
	mbForceGrabLogged = false;
	mvVrGripPoint = cVector3f(0, 0, 0);
	mfVrGripRadius = 0.018f;
	mfVrGripTwist = 0.0f;
	mbUseVrGripAnchor = false;
	mfIndexHoldWeight = 0.0f;
	mpPoseCallback = NULL;
	mmtxLastPose = cMatrixf::Identity;
	mbLastPoseValid = false;
	for(int i=0; i<15; ++i)
	{
		mvGrabRot[i] = cQuaternion::Identity;
		mvTriggerRot[i] = cQuaternion::Identity;
		mvHoldRot[i] = cQuaternion::Identity;
	}
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

  // Resolve collision in metre-scaled controller space. Applying the HUD
  // model's tiny visual scale to the physics shape made the previous hand
  // collision attempt effectively disappear.
  cMatrixf vr_worldHandMtx = VRHelper::TrackingToWorldSpace(vr_handMtx, mpInit->mpGame);
  if (mpInit->mpPlayer != NULL)
  {
    cMatrixf vr_resolvedHandMtx;
    if (mpInit->mpPlayer->ResolveVRHandPose((eVRHandIndex)mlHandIndex,
        vr_worldHandMtx, vr_resolvedHandMtx))
      vr_worldHandMtx = vr_resolvedHandMtx;
  }

  //Attachment grip frame derived from the actual posed skeleton. In the
  //hand mesh, all four long fingers describe a cylinder around local Z; the
  //least-squares centres of their three joints sit at X ~= 5 cm and mirror
  //in Y. Composing the item's existing rotation inside the hand frame aligns
  //the whole handle with that cylinder instead of with the controller's Z.
  if(mbUseVrGripAnchor && mpInit->mpPlayerHands != NULL)
  {
    iHudModel *pHandModel = mpInit->mpPlayerHands->GetCurrentModel(mlHandIndex);
    if(pHandModel != NULL &&
      (pHandModel->msName == "Hand" || pHandModel->msName == "LeftHand"))
    {
      const float fPoseWeight = cMath::Clamp(
        1.15f - mfVrGripRadius * 12.5f, 0.72f, 1.0f);
      //Opening the fingers increases the fitted cylinder radius and moves
      //its centre away from the palm by about 3.25 cm per unit of pose
      //weight. Follow that measured centre so radius correction does not
      //reintroduce a lateral offset for thick handles.
      const float fOpenCentreOffset = (1.0f - fPoseWeight) * 0.0325f;
      const cVector3f vGripSocket(0.050f,
        mlHandIndex == 0 ? 0.019f + fOpenCentreOffset :
          -0.014f - fOpenCentreOffset,
        0.0f);
      aPoseMtx = cMath::MatrixMul(vr_worldHandMtx,
        cMath::MatrixTranslate(pHandModel->mvVrTransOffset));
      aPoseMtx = cMath::MatrixMul(aPoseMtx,
        cMath::MatrixRotate(pHandModel->mvVrRotOffset,
          eEulerRotationOrder_XYZ));
      aPoseMtx = cMath::MatrixMul(aPoseMtx,
        cMath::MatrixTranslate(vGripSocket));
      //Twist around the handle at the grip pivot. Keeping this separate from
      //VrRotOffset means tool heads can be reoriented without changing their
      //height or moving the measured contact point out of the hand.
      aPoseMtx = cMath::MatrixMul(aPoseMtx,
        cMath::MatrixRotateZ(mfVrGripTwist));
      aPoseMtx = cMath::MatrixMul(aPoseMtx,
        cMath::MatrixTranslate(mvVrGripPoint * -1.0f));
      aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_rotMtx);
      aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_scaleMtx);
      return true;
    }
  }

  //Legacy/controller-local pose for the hand models themselves and for any
  //old HUD object that has not supplied a measured grip profile.
  aPoseMtx = cMath::MatrixMul(vr_worldHandMtx, vr_transMtx);
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_rotMtx);
  aPoseMtx = cMath::MatrixMul(aPoseMtx, vr_scaleMtx);

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

	// Only the two actual hand models own a finger skeleton. Attachments such
	// as the flashlight and hammer share this class but do not have hand bones;
	// trying to pose them produced a misleading FAILED rig setup every equip.
	const bool bIsHandRig = msName == "Hand" || msName == "LeftHand";
	if(bIsHandRig)
	{
		SetupHandAnimation();

		//Direct finger posing: the callback writes the smoothed rotations onto
		//the bones after the engine animation update and refreshes the world
		//matrices, so the pose never depends on animation-state application.
		if(mpEntity && mpPoseCallback == NULL)
			mpPoseCallback = hplNew( cHandRigPoseCallback, (this) );
		mpEntity->SetCallback(mpPoseCallback);
	}
}

//-----------------------------------------------------------------------

void iHudModel::SetupHandAnimation()
{
	if(mpEntity == NULL || mbHandRigSetup) return;

	const char* sHand = mlHandIndex == 0 ? "left" : "right";

	//Own rotation per joint in degrees (per-joint deltas; they accumulate
	//down each chain). The mesh has all three joints for every finger, but
	//each vertex is rigidly assigned to a single bone, so restrained angles
	//look much more natural than a mathematically complete fist.
	static const float kfGrabAngles[lHandBoneNum] = {
		52,66,38,	//Middle
		52,66,38,	//Ring
		52,66,38,	//Little: same curve avoids shearing the fused web skin
		0,0,0,		//Index
		22,28,16	//Thumb: visible opposition without collapsing the web
	};
	static const float kfTriggerAngles[lHandBoneNum] = {
		0,0,0,		//Middle
		0,0,0,		//Ring
		0,0,0,		//Little
		48,62,36,	//Index
		0,0,0		//Thumb
	};
	//Hold pose driven only while an item is attached. It is deliberately only
	//a little tighter than the live pose: over-curling this old rigid skin is
	//what made fingers appear doubled or folded through the back of the hand.
	static const float kfHoldAngles[lHandBoneNum] = {
		55,70,40,	//Middle
		55,70,40,	//Ring
		55,70,40,	//Little
		52,66,38,	//Index
		28,34,20	//Thumb
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
	//The two meshes are mirrored. Positive curl must point into each palm:
	//right fingers rotate about -Z and left fingers about +Z. The previous
	//signs were reversed and visibly folded every finger toward the dorsum.
	//All four long fingers intentionally share the same axis. In particular,
	//giving the little finger a tilted axis twists its rigidly weighted web
	//toward the ring finger instead of producing anatomical splay.
	cVector3f vFingerAxis = cVector3f(0, 0, bLeft ? 1.0f : -1.0f);
	cVector3f vPinkyAxis = vFingerAxis;
	//The thumb has a diagonal three-joint chain, so it needs opposition as
	//well as flexion. Its Z component mirrors while Y remains common.
	cVector3f vThumbAxis = cVector3f(0.0f, 0.94f,
		bLeft ? 0.342f : -0.342f);

	//Precompute the per-bone bind-relative rotations for the three poses
	//(relaxed grab, trigger, deep hold). ApplyFingerPose writes these
	//straight onto the bones from the entity callback, bypassing the
	//animation-state path.
	static const int klChainStart[kFingerTrackNum] = { 0,3,6,9,12 };

	for(int f=0; f<kFingerTrackNum; ++f)
	{
		for(int j=0; j<3; ++j)
		{
			int i = klChainStart[f] + j;
			cVector3f vAxis = (i >= 12) ? vThumbAxis
							: (i >= 6 && i < 9) ? vPinkyAxis : vFingerAxis;
			mvGrabRot[i] = cQuaternion(cMath::ToRad(kfGrabAngles[i]), vAxis);
			mvTriggerRot[i] = cQuaternion(cMath::ToRad(kfTriggerAngles[i]), vAxis);
			mvHoldRot[i] = cQuaternion(cMath::ToRad(kfHoldAngles[i]), vAxis);
		}
	}

	//Bind-pose locals for the per-frame reset in ApplyFingerPose: the
	//engine only resets bones when an animation state is active, and the
	//callback path no longer uses any, so without this the applied
	//rotations would accumulate every frame.
	if(mpMesh->GetSkeleton())
	{
		for(int i=0; i<15; ++i)
		{
			if(mlBoneIdx[i] < 0) continue;
			cBone *pBone = mpMesh->GetSkeleton()->GetBoneByIndex(mlBoneIdx[i]);
			if(pBone) mvBindLocal[i] = pBone->GetLocalTransform();
		}
	}

	mbHandRigSetup = true;
}

//-----------------------------------------------------------------------

void iHudModel::DestroyHandAnimation()
{
	mfIndexHoldWeight = 0.0f;
	for(int i=0; i<kFingerTrackNum; ++i)
	{
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
	if(!mbHandRigSetup)
	{
		return;
	}

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
	if(mbForceGrabPose)
	{
		//An item is attached to this slot. The target comes from its measured
		//handle radius: a flashlight keeps the hand more open than a glowstick.
		const float fForcedWeight = cMath::Clamp(mfForcedGrabPoseWeight,
			0.72f, 1.0f);
		for(int f=0; f<kFingerTrackNum; ++f) afTarget[f] = fForcedWeight;
		afTarget[3] = 0.0f;
	}

	float fDt = afTimeStep < 0.0f ? 0.0f : (afTimeStep > 0.1f ? 0.1f : afTimeStep);
	float fBlend = 1.0f - expf(-fDt / 0.07f);

	for(int f=0; f<kFingerTrackNum; ++f)
	{
		mfFingerWeight[f] += (afTarget[f] - mfFingerWeight[f]) * fBlend;
	}

	float fHoldTarget = mbForceGrabPose ?
		cMath::Clamp(mfForcedGrabPoseWeight, 0.72f, 1.0f) : 0.0f;
	mfIndexHoldWeight += (fHoldTarget - mfIndexHoldWeight) * fBlend;
}

//Called from the entity callback right after the engine's own animation
//update: writes the smoothed finger rotations directly onto the bones and
//refreshes the world matrices, so the pose is deterministic and never
//depends on the animation-state application order.
void iHudModel::ApplyFingerPose()
{
	if(!mbHandRigSetup || mpEntity == NULL) return;

	if(mbForceGrabPose && !mbForceGrabLogged)
	{
		Log(" [hand-rig] '%s': force=1 weights %.2f/%.2f/%.2f/%.2f/%.2f hold=%.2f\n",
			msName.c_str(), mfFingerWeight[0], mfFingerWeight[1], mfFingerWeight[2],
			mfFingerWeight[3], mfFingerWeight[4], mfIndexHoldWeight);
		mbForceGrabLogged = true;
	}
	else if(!mbForceGrabPose)
	{
		mbForceGrabLogged = false;
	}

	for(int i=0; i<15; ++i)
	{
		if(mlBoneIdx[i] < 0) continue;
		cBoneState *pBone = mpEntity->GetBoneState(mlBoneIdx[i]);
		if(pBone == NULL) continue;

		//Reset to bind first: without this the rotations accumulate every
		//frame because the engine only resets bones when an animation state
		//is active, and this path no longer uses any.
		pBone->SetMatrix(mvBindLocal[i], false);

		int f = i / 3;
		float w;
		const cQuaternion *pq;
		if(mbForceGrabPose)
		{
			w = (f == 3) ? mfIndexHoldWeight : mfFingerWeight[f];
			//Use the dedicated hold pose for every chain. The old branch used
			//it only on the index, despite all five hold weights reaching 1.0.
			pq = &mvHoldRot[i];
		}
		else
		{
			w = mfFingerWeight[f];
			pq = (f == 3) ? &mvTriggerRot[i] : &mvGrabRot[i];
		}

		if(w <= 0.001f) continue;

		cQuaternion qRot = cMath::QuaternionSlerp(w, cQuaternion::Identity, *pq, true);
		pBone->AddRotation(qRot);
	}
}

	//-----------------------------------------------------------------------

void cHandRigPoseCallback::AfterAnimationUpdate(cMeshEntity* apMeshEntity, float afTimeStep)
{
	if(mpModel == NULL) return;
	mpModel->ApplyFingerPose();
	apMeshEntity->RefreshBoneWorldMatrices();
}

	//-----------------------------------------------------------------------

void iHudModel::SetVisible(bool visible) {
  if(mpEntity) mpEntity->SetVisible(visible);
}

//-----------------------------------------------------------------------

void iHudModel::DestroyEntities()
{
	cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();

	//Mesh entity
	if(mpEntity) mpEntity->SetCallback(NULL);
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
		mvAttachmentHudModels[i] = NULL;
		mlInvalidPoseFrames[i] = 0;
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
	//OpenVR can mark an otherwise tracked controller invalid for one or two
	//frames. Keep the last rendered transform during that short gap instead
	//of making the hand and held item flash out immediately.
	bool vbPoseValid[2];
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		vbPoseValid[i] = i == 0 ? mpInit->mpGame->vr_left_hand.IsPoseValid() :
			mpInit->mpGame->vr_right_hand.IsPoseValid();
		if(vbPoseValid[i])
		{
			if(mlInvalidPoseFrames[i] >= 6)
				Log(" [hand-render] slot %d recovered after %d invalid pose frames\n",
					i, mlInvalidPoseFrames[i]);
			mlInvalidPoseFrames[i] = 0;
		}
		else if(mlInvalidPoseFrames[i] < 46)
		{
			++mlInvalidPoseFrames[i];
			if(mlInvalidPoseFrames[i] == 6)
				Log(" [hand-render] slot %d tracking pose remains invalid\n", i);
			else if(mlInvalidPoseFrames[i] == 46)
				Log(" [hand-render] slot %d hidden after tracking grace period\n", i);
		}
	}

	/////////////////////////////////////
	// Update the current model
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvCurrentHudModels[i];
    if (pHudModel == NULL) {continue;}

		if(!vbPoseValid[i]) {
			//At 90 Hz this is roughly half a second. A genuinely disconnected
			//controller is hidden, while ordinary tracking hitches retain the
			//last good pose.
			if(mlInvalidPoseFrames[i] > 45 && pHudModel->mpEntity != NULL)
				pHudModel->SetVisible(false);
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

    iHudModel *pAttachment = mvAttachmentHudModels[i];
    pHudModel->mbForceGrabPose = pAttachment != NULL;
    if(pAttachment != NULL)
    {
      //The inner finger arc is about 3 cm at full curl. Convert the measured
      //handle radius into a stable 0.72..1 pose fraction: thin rods close
      //fully, while thick flashlight bodies retain clearance.
      pHudModel->mfForcedGrabPoseWeight = cMath::Clamp(
        1.15f - pAttachment->mfVrGripRadius * 12.5f, 0.72f, 1.0f);
    }
    else
    {
      pHudModel->mfForcedGrabPoseWeight = 1.0f;
    }

    pHudModel->UpdateHandAnimation(afTimeStep);

		// Call per-model update for things like throw velocity tracking
		pHudModel->Update(afTimeStep);
	}

	/////////////////////////////////////
	// Attachment models use their own calibrated HUD offsets on the same
	// collision-resolved controller pose as the hand. They skip finger
	// animation and the equip/unequip swap chain.
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		iHudModel *pHudModel = mvAttachmentHudModels[i];
		if(pHudModel == NULL) continue;

		if(!vbPoseValid[i]) {
			if(mlInvalidPoseFrames[i] > 45 && pHudModel->mpEntity != NULL)
				pHudModel->SetVisible(false);
      continue;
    }
    if (pHudModel->mpEntity != NULL) pHudModel->SetVisible(true);

    // Use the pose returned by the model itself. The former hard-coded
    // thumb/knuckle socket added 5-8 cm to every item and was the reason
    // flashlights and weapons appeared to float beside the hand.
    cMatrixf mtxPose = cMatrixf::Identity;
    if (!pHudModel->UpdatePoseMatrix(mtxPose, afTimeStep)) continue;

    if (pHudModel->mpEntity != nullptr)
      pHudModel->mpEntity->SetMatrix(mtxPose);
    pHudModel->SetLastPose(mtxPose);

		pHudModel->Update(afTimeStep);
	}
}

//-----------------------------------------------------------------------

bool cPlayerHands::GetHandPalmPose(int alNum, cMatrixf& aPoseMtx,
	bool abResolveCollision)
{
	if(alNum < 0 || alNum >= mlCurrentModelNum) return false;

	TrackedController& hand = alNum == 0 ? mpInit->mpGame->vr_left_hand :
		mpInit->mpGame->vr_right_hand;
	if(!hand.IsPoseValid()) return false;

	aPoseMtx = VRHelper::TrackingToWorldSpace(hand.GetMatrix(), mpInit->mpGame);
	cMatrixf mtxResolved;
	if(abResolveCollision &&
		mpInit->mpPlayer->ResolveVRHandPose((eVRHandIndex)alNum,
			aPoseMtx, mtxResolved))
	{
		aPoseMtx = mtxResolved;
	}

	// Match the visible hand's palm orientation, but deliberately omit its
	// 0.006 mesh scale. Physics anchors must remain rigid metre transforms.
	iHudModel *pHandModel = mvCurrentHudModels[alNum];
	if(pHandModel != NULL &&
		(pHandModel->msName == "Hand" || pHandModel->msName == "LeftHand"))
	{
		aPoseMtx = cMath::MatrixMul(aPoseMtx,
			cMath::MatrixTranslate(pHandModel->mvVrTransOffset));
		aPoseMtx = cMath::MatrixMul(aPoseMtx,
			cMath::MatrixRotate(pHandModel->mvVrRotOffset,
				eEulerRotationOrder_XYZ));
	}

	return true;
}

//-----------------------------------------------------------------------

bool cPlayerHands::GetHandMiddleFingerPosition(int alNum,
	cVector3f& avPosition)
{
	if(alNum < 0 || alNum >= mlCurrentModelNum) return false;

	iHudModel *pHandModel = mvCurrentHudModels[alNum];
	if(pHandModel == NULL || pHandModel->mpEntity == NULL ||
		(pHandModel->msName != "Hand" && pHandModel->msName != "LeftHand"))
		return false;

	// Middle2 is the central knuckle of the posed middle-finger chain. Reading
	// its bone state keeps the cue attached to the rendered finger as it curls,
	// instead of using the controller/palm origin that sits toward the thumb.
	if(!pHandModel->mbHandRigSetup) return false;
	const int lMiddleBone = pHandModel->mlBoneIdx[1];
	if(lMiddleBone < 0) return false;

	cBoneState *pBone = pHandModel->mpEntity->GetBoneState(lMiddleBone);
	if(pBone == NULL) return false;

	avPosition = pBone->GetWorldPosition();
	return true;
}

//-----------------------------------------------------------------------

void cPlayerHands::Reset()
{
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		mlInvalidPoseFrames[i] = 0;
		iHudModel *pHudModel = mvCurrentHudModels[i];
		if(pHudModel)
		{
			pHudModel->DestroyEntities();
		}
		
		mvCurrentHudModels[i] = NULL;

		if(mvAttachmentHudModels[i])
		{
			mvAttachmentHudModels[i]->DestroyEntities();
			mvAttachmentHudModels[i] = NULL;
		}
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
  pHudModel->mvVrGripPoint = cString::ToVector3f(pMainElem->Attribute("VrGripPoint"), 0);
  pHudModel->mfVrGripRadius = cString::ToFloat(pMainElem->Attribute("VrGripRadius"), 0.018f);
  pHudModel->mfVrGripTwist = cString::ToFloat(pMainElem->Attribute("VrGripTwist"), 0.0f);
	
	pHudModel->LoadData(pRootElem);

	AddHudModel(pHudModel);

	hplDelete( pXmlDoc );
	return true;
}

//-----------------------------------------------------------------------

void cPlayerHands::SetCurrentModel(int alNum,const tString& asName)
{
	Log(" [hand-rig] SetCurrentModel slot %d -> '%s'\n", alNum, asName.c_str());

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

void cPlayerHands::SetAttachmentModel(int alNum,const tString& asName)
{
	iHudModel *&pSlot = mvAttachmentHudModels[alNum];

	if(pSlot && asName != "" &&
		cString::ToLowerCase(asName) == cString::ToLowerCase(pSlot->msName))
	{
		return;
	}

	if(pSlot)
	{
		Log(" [hand-rig] detach '%s' from %s hand.\n", pSlot->msName.c_str(),
			alNum == 0 ? "left" : "right");
		pSlot->UnequipEffect();
		pSlot->mbUseVrGripAnchor = false;
		pSlot->DestroyEntities();
		pSlot->Reset();
		pSlot = NULL;
	}

	if(asName == "") return;

	tHudModelMapIt it = m_mapHudModels.find(cString::ToLowerCase(asName));
	if(it == m_mapHudModels.end())
	{
		Log(" Couldn't find hud model '%s'!\n",asName.c_str());
		return;
	}

	iHudModel *pAttachModel = it->second;

	Log(" [hand-rig] attach '%s' to %s hand.\n", pAttachModel->msName.c_str(),
		alNum == 0 ? "left" : "right");

	pAttachModel->SetHandIndex(alNum);
	pAttachModel->mbUseVrGripAnchor = true;
	pAttachModel->LoadEntities();
	pAttachModel->EquipEffect(true);
	pAttachModel->mfTime = 0;
	pAttachModel->mState = eHudModelState_Idle;
	const float fPoseWeight = cMath::Clamp(
		1.15f - pAttachModel->mfVrGripRadius * 12.5f, 0.72f, 1.0f);
	Log(" [hand-grip] '%s': point %s radius %.3f twist %.2f pose %.2f\n",
		pAttachModel->msName.c_str(), pAttachModel->mvVrGripPoint.ToString().c_str(),
		pAttachModel->mfVrGripRadius, pAttachModel->mfVrGripTwist, fPoseWeight);

	pSlot = pAttachModel;
}

//-----------------------------------------------------------------------

iHudModel* cPlayerHands::FindHandModel(int alNum,const tString& asName)
{
	const tString sLower = cString::ToLowerCase(asName);

	iHudModel* apCandidates[2] = { mvCurrentHudModels[alNum], mvAttachmentHudModels[alNum] };
	for(int i=0; i<2; ++i)
	{
		if(apCandidates[i] &&
			cString::ToLowerCase(apCandidates[i]->msName) == sLower &&
			apCandidates[i]->GetState() != eHudModelState_Unequip)
		{
			return apCandidates[i];
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------

void cPlayerHands::SetSlotVisible(int alNum, bool abX)
{
	if(mvCurrentHudModels[alNum])
		mvCurrentHudModels[alNum]->SetVisible(abX);
	if(mvAttachmentHudModels[alNum])
		mvAttachmentHudModels[alNum]->SetVisible(abX);
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

		if(mvAttachmentHudModels[i])
		{
			mvAttachmentHudModels[i]->DestroyEntities();
		}
	}
}

//-----------------------------------------------------------------------

void cPlayerHands::OnWorldLoad()
{
	for(int i=0; i< mlCurrentModelNum; ++i)
	{
		mlInvalidPoseFrames[i] = 0;
		iHudModel *pHudModel = mvCurrentHudModels[i];
		if(pHudModel)
		{
			pHudModel->SetHandIndex(i);
			pHudModel->LoadEntities();
		}

		if(mvAttachmentHudModels[i])
		{
			mvAttachmentHudModels[i]->SetHandIndex(i);
			mvAttachmentHudModels[i]->LoadEntities();
		}
	}
}

//-----------------------------------------------------------------------
