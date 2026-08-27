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
#ifndef GAME_PLAYER_HANDS_H
#define GAME_PLAYER_HANDS_H

#include "StdAfx.h"
#include "GameTypes.h"

using namespace hpl;

class cInit;

//------------------------------------------------

class cHudModelPose
{
public:
	cHudModelPose(){}
	cHudModelPose(const cVector3f& avPos,const cVector3f& avRot)
	{
		mvPos = avPos;
		mvRot = avRot;
	}

	cMatrixf ToMatrix()
	{
		cMatrixf mtxPose = cMath::MatrixRotate(mvRot,eEulerRotationOrder_XYZ);
		mtxPose.SetTranslation(mvPos);
		return mtxPose;
	}

	cVector3f mvPos;
	cVector3f mvRot;
};

//------------------------------------------------

enum eHudModelState
{
	eHudModelState_Idle,
	eHudModelState_Equip,
	eHudModelState_Unequip
};

//------------------------------------------------

class iHudModel;

//Applies the smoothed finger weights directly to the rig bones after the
//engine has run its own animation update, bypassing the animation-state
//path entirely (deterministic: no reliance on state application order).
class cHandRigPoseCallback : public cMeshEntityCallback
{
public:
	cHandRigPoseCallback(iHudModel* apModel) : mpModel(apModel) {}
	void AfterAnimationUpdate(cMeshEntity* apMeshEntity, float afTimeStep);
private:
	iHudModel *mpModel;
};

//------------------------------------------------

cHudModelPose GetPoseFromElem(const tString &asName, TiXmlElement *apElem);

//------------------------------------------------

class iHudModel
{
friend class cPlayerHands;
public:
	iHudModel(ePlayerHandType aType);

	cMeshEntity *GetEntity(){return mpEntity;}

	void LoadEntities();
	void DestroyEntities();

	void EquipEffect(bool abJustCreated);
	void UnequipEffect();

	void Reset();

	tString msName;
	tString msModelFile;
	
	cHudModelPose mEquipPose;
	cHudModelPose mUnequipPose;

	tString msEquipSound;
	tString msUnequipSound;

	float mfEquipTime;
	float mfUnequipTime;

  void SetVisible(bool visible);

	eHudModelState GetState(){ return mState;}

	std::vector<cParticleSystem3D*> mvParticleSystems;
	std::vector<cBillboard*> mvBillboards;
	std::vector<iLight3D*> mvLights;
	std::vector<cColor> mvLightColors;
	std::vector<float> mvLightRadii;
	std::vector<cSoundEntity*> mvSoundEntities;

	virtual void LoadData(TiXmlElement *apRootElem)=0;

  float mfVrScale;
  cVector3f mvVrRotOffset;
  cVector3f mvVrTransOffset;

  //Measured point on the object's handle after VrRotOffset/VrScale, in
  //metres. Attachments place this point on the cylinder described by the
  //posed finger joints instead of assuming that the model origin is a grip.
  cVector3f mvVrGripPoint;
  float mfVrGripRadius;
  //Rotation around the measured handle axis, in radians. This changes which
  //side of a hammer/pick head faces the thumb without moving the grip point.
  float mfVrGripTwist;
  bool mbUseVrGripAnchor;

  //While true the finger rigs close into the full grab pose regardless of
  //the live grip input, so an attached item is seen firmly held.
  bool mbForceGrabPose;
  //Radius-derived fraction of the hold pose; thick handles remain more open
  //so the rigid finger skin does not pass through them.
  float mfForcedGrabPoseWeight;
  //One-shot diagnostic guard for the forced-grab log line.
  bool mbForceGrabLogged;

  //Last world pose the model was placed at (attachments refresh it every
  //frame); consumers such as the throw release read it instead of
  //re-deriving a raw pose that no longer matches the rendered item.
  void SetLastPose(const cMatrixf& aMtx){ mmtxLastPose = aMtx; mbLastPoseValid = true; }
  const cMatrixf& GetLastPose() const { return mmtxLastPose; }
  bool IsLastPoseValid() const { return mbLastPoseValid; }

  virtual bool UpdatePoseMatrix(cMatrixf& aPoseMtx, float afTimeStep);

	virtual void Update(float afTimeStep) {}

  void SetHandIndex(int handIndex) {
    mlHandIndex = handIndex;
  }

  void UpdateHandAnimation(float afTimeStep);
  void ApplyFingerPose();

protected:
	virtual void ResetExtraData()=0;

	virtual void LoadExtraEntites(){}
	virtual void DestroyExtraEntities(){}

	virtual void PostSceneDraw(){}

  void SetupHandAnimation();
  void DestroyHandAnimation();

	ePlayerHandType mType;

	cInit *mpInit;

	cMesh *mpMesh;
	cMeshEntity *mpEntity;
	
	eHudModelState mState;

	float mfTime;

	tString msNextModel;

  int mlHandIndex;

  //One animation state per finger chain (order matches the bone table:
  //Middle, Ring, Little, Index, Thumb) so every finger follows its own
  //curl value with independent timing and smoothing.
  static const int kFingerTrackNum = 5;
  float mfFingerWeight[kFingerTrackNum];
  int mlBoneIdx[15];
  bool mbHandRigSetup;

  //Precomputed per-bone rotations (bind-relative, about the per-chain fold
  //axes) for the three poses: relaxed grab, trigger, and the deeper hold
  //used while an item is attached. Zero-angle bones stay identity.
  cQuaternion mvGrabRot[15];
  cQuaternion mvTriggerRot[15];
  cQuaternion mvHoldRot[15];
  //Bind-pose local transforms: the callback resets every bone to these
  //before applying its rotation, so poses never accumulate across frames.
  cMatrixf mvBindLocal[15];
  float mfIndexHoldWeight;

  cHandRigPoseCallback *mpPoseCallback;

  cMatrixf mmtxLastPose;
  bool mbLastPoseValid;
};

//------------------------------------------------

class cHudModel_Normal : iHudModel
{
	friend class cPlayerHands;
public:
	cHudModel_Normal() : iHudModel(ePlayerHandType_Normal){}

	void LoadData(TiXmlElement *apRootElem){}

	void ResetExtraData(){}
};

//------------------------------------------------

typedef std::map<tString,iHudModel*> tHudModelMap;
typedef tHudModelMap::iterator tHudModelMapIt;

//------------------------------------------------

class cPlayerHands : public iUpdateable
{
public:
	cPlayerHands(cInit *apInit);
	~cPlayerHands();

	void OnStart();
	void Update(float afTimeStep);
	void Reset();
	void OnDraw();

	void OnWorldExit();
	void OnWorldLoad();

	void AddHudModel(iHudModel* apHudModel);
	bool AddModelFromFile(const tString &asFile);

	iHudModel* GetModel(const tString& asName);

	void SetCurrentModel(int alNum, const tString& asName);
	iHudModel* GetCurrentModel(int alNum){ return mvCurrentHudModels[alNum];}
	// Collision-resolved, unscaled palm transform for physics interaction.
	bool GetHandPalmPose(int alNum, cMatrixf& aPoseMtx);

	// Extra mesh rendered on the same controller pose as the slot's hand rig,
	// so an equipped item (flashlight, glowstick, flare) is seen held by a
	// visible hand instead of replacing it. Equip and clear are instant.
	void SetAttachmentModel(int alNum, const tString& asName);
	iHudModel* GetAttachmentModel(int alNum){ return mvAttachmentHudModels[alNum];}
	// First current or attachment model of slot alNum whose name matches.
	iHudModel* FindHandModel(int alNum, const tString& asName);
	// Show/hide both models of slot alNum (hand rig and attachment together).
	void SetSlotVisible(int alNum, bool abX);

private:
	cInit* mpInit;
	cMeshManager *mpMeshManager;

	tHudModelMap m_mapHudModels;
	iHudModel* mvCurrentHudModels[2];
	iHudModel* mvAttachmentHudModels[2];
	//Grace period for transient OpenVR pose loss; shared by hand and item.
	int mlInvalidPoseFrames[2];
	int mlCurrentModelNum;
};


#endif // GAME_PLAYER_HANDS_H
