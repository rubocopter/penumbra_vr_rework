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

	static const char* ksBoneNames[] = {
		"Middle1","Middle2","Middle3",
		"Ring1","Ring2","Ring3",
		"Little1","Little2","Little3",
		"Index1","Index2","Index3",
		"Thumb1","Thumb2","Thumb3"
	};
	static const int lNumBones = 15;

	//Own rotation per joint in degrees. Grab: middle/ring/little/thumb.
	//Trigger: index. (Cumulative: Middle/Ring 60/120/165, Little 55/110/150,
	//Index 55/110/155, Thumb 40/75.)
	static const float kfGrabAngles[lNumBones] = {
		60,60,45,	//Middle
		60,60,45,	//Ring
		55,55,40,	//Little
		0,0,0,		//Index
		40,35,0		//Thumb
	};
	static const float kfTriggerAngles[lNumBones] = {
		0,0,0,		//Middle
		0,0,0,		//Ring
		0,0,0,		//Little
		55,55,45,	//Index
		0,0,0		//Thumb
	};

	int lBoneIdx[lNumBones];
	for(int i=0; i<lNumBones; ++i)
	{
		lBoneIdx[i] = mpEntity->GetBoneStateIndex(ksBoneNames[i]);
		if(lBoneIdx[i] < 0)
		{
			mbHandRigSetup = false;
			return;
		}
	}

	const bool bLeft = (mlHandIndex == 0);
	cVector3f vFingerAxis = cVector3f(0,0, bLeft ? 1.0f : -1.0f);
	cVector3f vThumbAxis = cVector3f(0, 0.8910f, bLeft ? -0.4540f : 0.4540f);

	static const char* ksPoseNames[] = { "HandGrab", "HandTrigger" };
	static const float* kfPoseAngles[] = { kfGrabAngles, kfTriggerAngles };

	for(int p=0; p<2; ++p)
	{
		cAnimation *pAnim = hplNew( cAnimation, (ksPoseNames[p], "") );
		pAnim->SetLength(0.001f);

		for(int i=0; i<lNumBones; ++i)
		{
			if(kfPoseAngles[p][i] == 0.0f) continue;

			cAnimationTrack *pTrack = pAnim->CreateTrack(ksBoneNames[i], eAnimTransformFlag_Rotate);
			pTrack->SetNodeIndex(lBoneIdx[i]);

			cKeyFrame *pFrame = pTrack->CreateKeyFrame(0);
			cVector3f vAxis = (i >= 12) ? vThumbAxis : vFingerAxis;
			pFrame->rotation = cQuaternion(cMath::ToRad(kfPoseAngles[p][i]), vAxis);
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
