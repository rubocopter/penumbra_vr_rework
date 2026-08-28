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
#include "PlayerState_Misc_VR.h"

#include "Init.h"
#include "Player.h"
#include "EffectHandler.h"
#include "GameLadder.h"
#include "GameArea.h"
#include "GameItem.h"
#include "GameObject.h"
#include "GameSwingDoor.h"
#include "scene/PortalContainer.h"

#include "VRHelper.hpp"
#include "VRHaptics.h"

namespace
{
  const float kVRNudgeMinHandSpeed = 0.03f;
  const float kVRNudgeBroadPhasePadding = 0.15f;
  const float kVRMagneticItemRange = 2.35f;
  const int kVRMagneticRankedCandidateCount = 5;

  iPhysicsJoint *GetVRNudgeJoint(iPhysicsBody *apBody)
  {
    if (apBody == NULL) return NULL;

    iPhysicsJoint *pFallback = NULL;
    for (int i = 0; i < apBody->GetJointNum(); ++i)
    {
      iPhysicsJoint *pJoint = apBody->GetJoint(i);
      if (pJoint == NULL) continue;
      if (pFallback == NULL) pFallback = pJoint;
      if (pJoint->GetChildBody() == apBody) return pJoint;
    }

    // Follow detachable/inserted handles to the joint that drives their
    // child mechanism (for example ht_steelrodShape -> ht_joint1).
    for (int i = 0; i < apBody->GetJointNum(); ++i)
    {
      iPhysicsJoint *pLinkJoint = apBody->GetJoint(i);
      if (pLinkJoint == NULL || pLinkJoint->GetParentBody() != apBody) continue;

      iPhysicsBody *pChildBody = pLinkJoint->GetChildBody();
      if (pChildBody == NULL) continue;
      for (int j = 0; j < pChildBody->GetJointNum(); ++j)
      {
        iPhysicsJoint *pDriveJoint = pChildBody->GetJoint(j);
        if (pDriveJoint != NULL && pDriveJoint != pLinkJoint &&
            pDriveJoint->GetChildBody() == pChildBody)
          return pDriveJoint;
      }
    }
    return pFallback;
  }

  // Half-Life: Alyx-style hand aim is deliberately limited to inventory
  // pickups. Doors, drawers, loose props, and arbitrary scripted objects keep
  // their normal physical acquisition and constraints.
  bool GetVRMagneticCandidate(iPhysicsBody *apBody, iGameEntity *apEntity,
    float& afRange, float& afPriorityBias)
  {
    if(apBody == NULL || apEntity == NULL || !apEntity->IsActive() ||
      !apBody->IsActive() || !apBody->GetCollide() || apBody->IsCharacter() ||
      apBody->IsPlayer())
      return false;

    if(apEntity->GetType() != eGameEntityType_Item) return false;

    // Consumables and stacks receive the strongest assistance. Important
    // equipment remains eligible, but only when the hand intent is closer and
    // more deliberate. "Normal" includes keys and map-authored small tools.
    const eGameItemType itemType = static_cast<cGameItem*>(apEntity)->GetItemType();
    switch(itemType)
    {
    case eGameItemType_Battery:
    case eGameItemType_Food:
    case eGameItemType_GlowStick:
    case eGameItemType_Flare:
    case eGameItemType_Painkillers:
      afRange = kVRMagneticItemRange;
      afPriorityBias = -0.20f;
      break;
    case eGameItemType_Normal:
    case eGameItemType_Note:
    case eGameItemType_Map:
      afRange = 1.90f;
      afPriorityBias = -0.10f;
      break;
    case eGameItemType_Notebook:
    case eGameItemType_Flashlight:
    case eGameItemType_WeaponMelee:
    case eGameItemType_Throw:
      afRange = 1.45f;
      afPriorityBias = 0.0f;
      break;
    default:
      return false;
    }
    return true;
  }

  // The normal pick callback is intentionally interaction-oriented: areas and
  // non-entity bodies receive special treatment. Magnetic visibility instead
  // needs the first solid physics body, regardless of whether it is usable.
  class cVRSolidSightCallback : public iPhysicsRayCallback
  {
  public:
    cVRSolidSightCallback(iPhysicsBody *apCandidate)
      : mpCandidate(apCandidate), mpNearestBody(NULL),
        mfNearestDistance(9999.0f), mvNearestPoint(0, 0, 0) {}

    bool BeforeIntersect(iPhysicsBody *apBody)
    {
      if(apBody == NULL || !apBody->IsActive() || apBody->IsCharacter() ||
        apBody->IsPlayer())
        return false;

      // Non-colliding area/helper bodies are not visible barriers. The target
      // itself is retained defensively even if map data changes its flag.
      return apBody == mpCandidate || apBody->GetCollide();
    }

    bool OnIntersect(iPhysicsBody *apBody, cPhysicsRayParams *apParams)
    {
      if(!BeforeIntersect(apBody) || apParams == NULL ||
        apParams->mfDist < 0.0f ||
        apParams->mfDist >= mfNearestDistance)
        return true;

      mfNearestDistance = apParams->mfDist;
      mvNearestPoint = apParams->mvPoint;
      mpNearestBody = apBody;
      return true;
    }

    iPhysicsBody *mpCandidate;
    iPhysicsBody *mpNearestBody;
    float mfNearestDistance;
    cVector3f mvNearestPoint;
  };

  bool CastVRSolidSight(iPhysicsWorld *apPhysicsWorld,
    const cVector3f& avOrigin, iPhysicsBody *apCandidate,
    const cVector3f& avPoint, cVector3f& avHitPoint)
  {
    if(apPhysicsWorld == NULL || apCandidate == NULL) return false;

    cVector3f vRay = avPoint - avOrigin;
    const float fRayLength = vRay.Length();
    if(fRayLength <= 0.001f) return false;

    // Continue a few centimetres into the target so a sample exactly on its
    // AABB surface still produces a physics hit.
    const cVector3f vEnd = avPoint + vRay * (0.03f / fRayLength);
    cVRSolidSightCallback callback(apCandidate);
    apPhysicsWorld->CastRay(&callback, avOrigin, vEnd,
      true, false, true, true);
    if(callback.mpNearestBody != apCandidate) return false;

    avHitPoint = callback.mvNearestPoint;
    return true;
  }

  struct cVRMagneticCandidate
  {
    iPhysicsBody *mpBody;
    cVector3f mvVisibleSample;
    cVector3f mvCenter;
    float mfScore;
  };

  void InsertVRMagneticCandidate(cVRMagneticCandidate *apCandidates,
    int& alCount, const cVRMagneticCandidate& aCandidate)
  {
    int lInsert = alCount;
    if(lInsert > kVRMagneticRankedCandidateCount)
      lInsert = kVRMagneticRankedCandidateCount;

    while(lInsert > 0 &&
      apCandidates[lInsert - 1].mfScore > aCandidate.mfScore)
      --lInsert;

    if(lInsert >= kVRMagneticRankedCandidateCount) return;

    int lLast = alCount < kVRMagneticRankedCandidateCount
      ? alCount : kVRMagneticRankedCandidateCount - 1;
    for(int i = lLast; i > lInsert; --i)
      apCandidates[i] = apCandidates[i - 1];
    apCandidates[lInsert] = aCandidate;
    if(alCount < kVRMagneticRankedCandidateCount) ++alCount;
  }

  bool FindVRMagneticTarget(cInit *apInit, cPlayer *apPlayer,
    eVRHandIndex aHand, iPhysicsBody*& apBody, cVector3f& avHitPoint,
    float& afDistance)
  {
    TrackedController& trackedHand = aHand == eVRHandIndex_Left
      ? apInit->mpGame->vr_left_hand : apInit->mpGame->vr_right_hand;
    if(!trackedHand.IsPoseValid()) return false;

    cMatrixf aimPose = VRHelper::TrackingToWorldSpace(
      VRHelper::ControllerAimMatrix(trackedHand), apInit->mpGame);
    const cVector3f vOrigin = aimPose.GetTranslation();
    const cVector3f vDirection = cMath::Vector3Normalize(
      cMath::MatrixMul(aimPose.GetRotation(), cVector3f(0.0f, 0.0f, -1.0f)));

    // Query a tight axis-aligned box around the aim segment instead of a large
    // cube around the hand. On long corridors this substantially reduces the
    // number of portal entities considered by both hands every frame.
    const cVector3f vSearchEnd = vOrigin + vDirection * kVRMagneticItemRange;
    const cVector3f vSearchDelta = vSearchEnd - vOrigin;
    const float fSearchPadding = 0.50f;
    cBoundingVolume searchBV;
    searchBV.SetSize(cVector3f(fabsf(vSearchDelta.x) + fSearchPadding * 2.0f,
      fabsf(vSearchDelta.y) + fSearchPadding * 2.0f,
      fabsf(vSearchDelta.z) + fSearchPadding * 2.0f));
    searchBV.SetPosition((vOrigin + vSearchEnd) * 0.5f);

    cWorld3D *pWorld = apInit->mpGame->GetScene()->GetWorld3D();
    iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();
    cPortalContainerEntityIterator it =
      pWorld->GetPortalContainer()->GetEntityIterator(&searchBV);

    cVRMagneticCandidate rankedCandidates[kVRMagneticRankedCandidateCount];
    int lRankedCandidateCount = 0;

    while(it.HasNext())
    {
      iPhysicsBody *pBody = static_cast<iPhysicsBody*>(it.Next());
      if(pBody == NULL || pBody->GetUserData() == NULL) continue;

      iGameEntity *pEntity = static_cast<iGameEntity*>(pBody->GetUserData());
      float fCandidateRange = 0.0f;
      float fPriorityBias = 0.0f;
      if(!GetVRMagneticCandidate(pBody, pEntity,
          fCandidateRange, fPriorityBias))
        continue;

      if(pBody == apPlayer->GetVRHeldBody(eVRHandIndex_Left) ||
        pBody == apPlayer->GetVRHeldBody(eVRHandIndex_Right))
        continue;

      cBoundingVolume *pBodyBV = pBody->GetBV();
      if(pBodyBV == NULL) continue;
      const cVector3f vTargetCenter = pBodyBV->GetWorldCenter();
      const cVector3f vToTarget = vTargetCenter - vOrigin;
      const float fForward = cMath::Vector3Dot(vToTarget, vDirection);
      if(fForward <= 0.08f || fForward > fCandidateRange) continue;

      cVector3f vPerpendicular = vToTarget - vDirection * fForward;
      const float fPerpendicularSq = vPerpendicular.SqrLength();
      float fBodyAllowance = pBodyBV->GetRadius();
      if(fBodyAllowance > 0.12f) fBodyAllowance = 0.12f;
      const float fConeRadius = 0.07f + fForward * 0.12f + fBodyAllowance;
      if(fPerpendicularSq > fConeRadius * fConeRadius) continue;

      // Aim at the closest portion of the body's AABB to the hand ray. This
      // keeps partly visible items selectable when their centre lies behind a
      // shelf lip, while the ray still rejects genuinely occluded objects.
      const cVector3f vAimPoint = vOrigin + vDirection * fForward;
      const cVector3f vMin = pBodyBV->GetMin();
      const cVector3f vMax = pBodyBV->GetMax();
      cVector3f vVisibleSample(
        cMath::Clamp(vAimPoint.x, vMin.x, vMax.x),
        cMath::Clamp(vAimPoint.y, vMin.y, vMax.y),
        cMath::Clamp(vAimPoint.z, vMin.z, vMax.z));
      vVisibleSample = vVisibleSample * 0.9f + vTargetCenter * 0.1f;

      float fScore = fPerpendicularSq / (fConeRadius * fConeRadius) +
        fForward * 0.02f + fPriorityBias;

      cVRMagneticCandidate candidate = {
        pBody, vVisibleSample, vTargetCenter, fScore
      };
      InsertVRMagneticCandidate(rankedCandidates,
        lRankedCandidateCount, candidate);
    }

    // Run expensive raycasts only for the five best geometric candidates.
    // A target must be unobstructed both from the controller and from the HMD:
    // this prevents a real hand pushed through a closed locker door from using
    // a ray whose origin has already crossed the door.
    const cVector3f vHeadOrigin =
      apInit->mpGame->GetScene()->GetVRWorldSpaceHeadMatrix().GetTranslation();
    iPhysicsBody *pBestBody = NULL;
    cVector3f vBestHit(0, 0, 0);
    for(int i = 0; i < lRankedCandidateCount; ++i)
    {
      cVRMagneticCandidate& candidate = rankedCandidates[i];
      cVector3f vHandHit;
      cVector3f vHeadHit;
      const bool bSampleVisible =
        CastVRSolidSight(pPhysicsWorld, vOrigin, candidate.mpBody,
          candidate.mvVisibleSample, vHandHit) &&
        CastVRSolidSight(pPhysicsWorld, vHeadOrigin, candidate.mpBody,
          candidate.mvVisibleSample, vHeadHit);
      const bool bCenterVisible = !bSampleVisible &&
        CastVRSolidSight(pPhysicsWorld, vOrigin, candidate.mpBody,
          candidate.mvCenter, vHandHit) &&
        CastVRSolidSight(pPhysicsWorld, vHeadOrigin, candidate.mpBody,
          candidate.mvCenter, vHeadHit);
      if(!bSampleVisible && !bCenterVisible) continue;

      pBestBody = candidate.mpBody;
      vBestHit = vHandHit;
      break;
    }

    // Publish only the winning result into the legacy interaction mirror.
    apPlayer->GetPickRay()->Clear();
    if(pBestBody == NULL) return false;
    const float fBestDistance = (vBestHit - vOrigin).Length();
    apPlayer->GetPickRay()->mpPickedBody = pBestBody;
    apPlayer->GetPickRay()->mvPickedPos = vBestHit;
    apPlayer->GetPickRay()->mfPickedDist = fBestDistance;
    apBody = pBestBody;
    avHitPoint = vBestHit;
    afDistance = fBestDistance;
    return true;
  }
}


//////////////////////////////////////////////////////////////////////////
// NORMAL STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_Normal_VR::cPlayerState_Normal_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Normal)
{
  for(int i = 0; i < eVRHandIndex_Count; ++i)
    mpLastMagneticTargets[i] = NULL;
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnUpdate(float afTimeStep)
{
  if (mpInit->mpNotebook->IsActive() == false &&
    mpInit->mpInventory->IsActive() == false &&
    mpInit->mpNumericalPanel->IsActive() == false &&
    mpInit->mpDeathMenu->IsActive() == false)
  {
    mpPlayer->ResetCrossHairPos();
  }

  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  /////////////////////////////////////
  // If run is down, run!!
  cInput *pInput = mpInit->mpGame->GetInput();
  if (pInput->IsTriggerd("Run") &&
    mpPlayer->GetMoveState() == ePlayerMoveState_Walk)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
  }

  iCollideShape *collider = mpPlayer->GetVRHandInteractionShape();

  struct touchedObject {
    iPhysicsBody* physicsBody;
    float planeDistance;
    float touchDistanceSq;
    eCrossHairState crosshair;
    cVector3f touchPoint;
  };

  if (collider != NULL) {
    cMatrixf poseMatrix;

    // Keep the dominant index for the legacy picked-body mirror below. Both
    // bare hands still maintain their own target and can initiate interaction.
    const eVRHandIndex dominantIndex = mpInit->mpGame->vr_dominant_hand == eSteamVRHand_Left
      ? eVRHandIndex_Left : eVRHandIndex_Right;

    for (int i = 0; i < eVRHandIndex_Count; ++i) {
      const eVRHandIndex handIndex = (eVRHandIndex)i;
      mpPlayer->ClearVRHandTarget(handIndex);
      bool bMagneticTarget = false;

// The off hand may also target and grab, but only while it carries no
      // equipment (flashlight, glowstick, flare); an equipped hand keeps its
      // original behaviour and never highlights world objects.
      if (handIndex != dominantIndex &&
          mpInit->mpPlayerHands->GetAttachmentModel(i) != NULL)
      {
        mpLastMagneticTargets[i] = NULL;
        continue;
      }

// The left and right hand rigs are distinct HUD models named "LeftHand"
      // and "Hand"; both are valid pointing hands for target detection.
      iHudModel *pHandModel = mpInit->mpPlayerHands->GetCurrentModel(i);
      if (pHandModel &&
          (pHandModel->msName == "Hand" || pHandModel->msName == "LeftHand")) {

        // Use the scale-free physical palm frame. UpdatePoseMatrix contains
        // the hand mesh's 0.006 render scale, which forced the old collision
        // query to use artificial 50x20x30 dimensions.
        if (!mpInit->mpPlayerHands->GetHandPalmPose(i, poseMatrix))
        {
          mpLastMagneticTargets[i] = NULL;
          continue;
        }

        // Target acquisition follows the player's real hand slightly beyond
        // the collision-stopped render hand. This is especially important for
        // short drawer handles surrounded by a frame: the visible palm can rest
        // on the front while the controller has already reached the handle.
        // Limit the extension and retain the LOS check below, so this cannot be
        // used as an unrestricted reach through walls.
        const cVector3f handCenter = poseMatrix.GetTranslation();
        cMatrixf interactionPose = poseMatrix;
        float fInteractionReach = 0.0f;
        cMatrixf rawPalmPose;
        if(mpInit->mpPlayerHands->GetHandPalmPose(i, rawPalmPose, false))
        {
          cVector3f vReach = rawPalmPose.GetTranslation() - handCenter;
          const float fReach = vReach.Length();
          const float kMaxCollisionReach = 0.18f;
          if(fReach > kMaxCollisionReach)
            vReach = vReach * (kMaxCollisionReach / fReach);
          fInteractionReach = vReach.Length();
          interactionPose = rawPalmPose;
          interactionPose.SetTranslation(handCenter + vReach);
        }

        auto mtxTouch = interactionPose.GetRotation();
        mtxTouch = cMath::MatrixMul(
          cMath::MatrixTranslate(interactionPose.GetTranslation()), mtxTouch);

        // Area body should take priority over everything else
        bool useTraceResults = false;

        {
          // Using area requires line-of-sight
          cVector3f vStart, vEnd;

          vStart = mpPlayer->GetCamera()->GetPosition();
          auto vDir = cMath::Vector3Normalize(handCenter - vStart);

          vEnd = vStart + vDir * mpPlayer->GetPickRay()->mfMaxDistance;

          mpPlayer->GetPickRay()->Clear();
          pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);
          mpPlayer->GetPickRay()->CalculateResults();

          if (mpPlayer->GetPickRay()->mpPickedBody != NULL &&
              mpPlayer->GetPickRay()->mpPickedBody == mpPlayer->GetPickRay()->mpPickedAreaBody)
            useTraceResults = true;
        }

        if (!useTraceResults) {
          cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
          iPhysicsWorld *pPhysicsWorld = pWorld->GetPhysicsWorld();

          // Reject bodies by their cheap bounding volume before asking every
          // candidate to calculate its interaction state. This avoids doing
          // script/state work for bodies that cannot touch the hand.
          cBoundingVolume handBV = collider->GetBoundingVolume();
          handBV.SetTransform(cMath::MatrixMul(mtxTouch,
            handBV.GetTransform()));

          // Keep only the nearest candidate in each priority class while
          // iterating. The previous temporary vector allocated every frame for
          // each hand and was immediately reduced to these same two entries.
          touchedObject bestItem;
          touchedObject *bestItemTarget = NULL;
          float fBestItemDistSq = 9999.0f;
          touchedObject bestOther;
          touchedObject *bestOtherTarget = NULL;
          float fBestOtherDistSq = 9999.0f;

          cPortalContainerEntityIterator it =
            pWorld->GetPortalContainer()->GetEntityIterator(&handBV);
          cCollideData collideData;
          collideData.SetMaxSize(1);
          while (it.HasNext())
          {
            iPhysicsBody *pBody = static_cast<iPhysicsBody*>(it.Next());

            iGameEntity *pEntity = NULL;
            if (pBody->GetUserData())
              pEntity = (iGameEntity*)pBody->GetUserData();
            else
              continue;

            if (pBody->IsActive() == false) continue;
            if (cMath::CheckCollisionBV(handBV, *pBody->GetBV()) == false)
              continue;

            // Skip the object if it can't be interacted with anyway
            eCrossHairState interactState = pEntity->GetCompletePickCrosshairState(pBody);

            if (interactState == eCrossHairState_None)
              continue;

            if (pPhysicsWorld->CheckShapeCollision(pBody->GetShape(), pBody->GetLocalMatrix(),
              collider,
              mtxTouch, collideData, 1) == false)
            {
              continue;
            }

            // These hueristic rules are stolen from the normal game's trace
            if (pEntity)
            {
              //Skip stick area
              if (pEntity->GetType() == eGameEntityType_StickArea) continue;

              //Area
              if ((pEntity->GetType() == eGameEntityType_Area ||
                pEntity->GetType() == eGameEntityType_StickArea ||
                pEntity->GetType() == eGameEntityType_DamageArea ||
                pEntity->GetType() == eGameEntityType_ForceArea))
              {
                // We no longer care about areas, since they're handled with the above raytrace to the hand's center.
                // Areas only seem to be important for things that don't require precision (i.e., you never interact with them)
                // Level change triggers, non-physics objects tend to be "area bodies"

              }
              else
              {
                float planeDistance = cMath::PlaneToPointDist(cPlanef(mpPlayer->GetCamera()->GetForward(), mpPlayer->GetCamera()->GetPosition()), pBody->GetBV()->GetWorldCenter());
                const cVector3f vTouchPoint = VRHelper::CollideCenter(
                  collideData.mvContactPoints, collideData.mlNumOfPoints);
                const float fTouchDistanceSq = (vTouchPoint - handCenter).SqrLength();

                //Other entity
                touchedObject target = {
                  pBody,
                  planeDistance,
                  fTouchDistanceSq,
                  interactState,
                  vTouchPoint
                };

                const bool bIsItemClass =
                  target.crosshair == eCrossHairState_PickUp ||
                  target.crosshair == eCrossHairState_Item;
                if(bIsItemClass && target.touchDistanceSq < fBestItemDistSq)
                {
                  fBestItemDistSq = target.touchDistanceSq;
                  bestItem = target;
                  bestItemTarget = &bestItem;
                }
                else if(!bIsItemClass &&
                  target.touchDistanceSq < fBestOtherDistSq)
                {
                  fBestOtherDistSq = target.touchDistanceSq;
                  bestOther = target;
                  bestOtherTarget = &bestOther;
                }
              }
            }
          }

          float fPickedDist = 9998.0f;
          cVector3f vPickedPos;
          iPhysicsBody* pPickedBody = NULL;

          // The overlap volume is intentionally forgiving, but it must not
          // select through a thin wall. A short ray from the resolved palm to
          // the candidate preserves drawer/key interactions while rejecting
          // geometry between the hand and the object.
          auto HandHasLineOfSight = [&](touchedObject *apTarget) -> bool
          {
            if (apTarget == NULL || apTarget->physicsBody == NULL) return false;
            const cVector3f& vTarget = apTarget->touchPoint;
            // Only bypass the ray for genuine collision-resolved palm contact.
            // Once acquisition has followed the raw controller, even a nearby
            // point may actually be on the far side of a thin closed door.
            if (fInteractionReach <= 0.015f &&
              (vTarget - handCenter).Length() <= 0.055f)
              return true;

            cVector3f vHitPoint;
            return CastVRSolidSight(pPhysicsWorld, handCenter,
              apTarget->physicsBody, vTarget, vHitPoint);
          };

          bool bSelected = false;

          if (bestItemTarget != NULL) {
            // Items normally need line-of-sight, but when the hand is
            // physically touching the item (e.g. a key lying inside an open
            // drawer whose geometry occludes the camera ray) touch wins.
            cVector3f vHeadHit;
            const cVector3f vHeadOrigin = mpInit->mpGame->GetScene()
              ->GetVRWorldSpaceHeadMatrix().GetTranslation();
            bool bHasLOS = CastVRSolidSight(pPhysicsWorld, vHeadOrigin,
              bestItemTarget->physicsBody, bestItemTarget->touchPoint,
              vHeadHit);
            if (bHasLOS || HandHasLineOfSight(bestItemTarget))
            {
              mpPlayer->GetPickRay()->Clear();
              mpPlayer->GetPickRay()->mpPickedBody = bestItemTarget->physicsBody;
               mpPlayer->GetPickRay()->mfPickedDist = bestItemTarget->planeDistance;
               mpPlayer->GetPickRay()->mvPickedPos =
                 bestItemTarget->touchPoint;
              bSelected = true;
            }
          }

          if (!bSelected && !useTraceResults && bestOtherTarget != NULL &&
              HandHasLineOfSight(bestOtherTarget)) {
            vPickedPos = bestOtherTarget->touchPoint;
            fPickedDist = bestOtherTarget->planeDistance;
            pPickedBody = bestOtherTarget->physicsBody;
          }

          if (!useTraceResults && !bSelected) {
            mpPlayer->GetPickRay()->Clear();

            mpPlayer->GetPickRay()->mpPickedBody = pPickedBody;
            mpPlayer->GetPickRay()->mfPickedDist = fPickedDist;
            mpPlayer->GetPickRay()->mvPickedPos = vPickedPos;
          }
        }

        // Physical overlap remains authoritative. If it found nothing, use a
        // visible hand-aim cone only for inventory pickups.
        if (!useTraceResults && mpPlayer->GetPickedBody() == NULL)
        {
          iPhysicsBody *pMagneticBody = NULL;
          cVector3f vMagneticHit;
          float fMagneticDistance = 0.0f;
          if(FindVRMagneticTarget(mpInit, mpPlayer, handIndex,
              pMagneticBody, vMagneticHit, fMagneticDistance))
          {
            // FindVRMagneticTarget has published the winning body and hit
            // point into the legacy pick callback used immediately below.
            bMagneticTarget = true;
          }
        }

        if (mpPlayer->GetPickedBody())
        {
          iPhysicsBody *pPickedBody = mpPlayer->GetPickedBody();
          iGameEntity *pEntity = (iGameEntity*)pPickedBody->GetUserData();
          const eCrossHairState handState = pEntity->GetCompletePickCrosshairState(pPickedBody);

          mpPlayer->SetVRHandTarget(handIndex, pPickedBody,
            mpPlayer->GetPickRay()->mvPickedPos,
            mpPlayer->GetPickRay()->mfPickedDist,
            handState, bMagneticTarget);
        }

      }

      iPhysicsBody *pCurrentMagneticTarget = bMagneticTarget
        ? mpPlayer->GetVRHandTarget(handIndex).mpBody : NULL;
      if(pCurrentMagneticTarget != mpLastMagneticTargets[i])
      {
        if(pCurrentMagneticTarget != NULL)
        {
          const eVRHapticHand hapticHand = handIndex == eVRHandIndex_Left
            ? eVRHapticHand_Left : eVRHapticHand_Right;
          cVRHaptics::Play(mpInit, eVRHapticEvent_Interaction,
            hapticHand, 0.16f);
        }
        mpLastMagneticTargets[i] = pCurrentMagneticTarget;
      }
    }

    /////////////////////////////////////////////////
    // Direct hand-to-object nudge: both hands can push
    // nearby physics objects without pressing any button.
    {
      iCollideShape *nudgeShape = mpPlayer->GetVRHandNudgeShape();
      if (nudgeShape != NULL)
      {
        // Query the engine's spatial index around each active hand instead of
        // walking every body in the level. The query is deliberately kept
        // local to this fallback; native hand collision and normal grabbing
        // remain unchanged.
        iPhysicsBody *pLeftHeldBody =
          mpPlayer->GetVRHeldBody(eVRHandIndex_Left);
        iPhysicsBody *pRightHeldBody =
          mpPlayer->GetVRHeldBody(eVRHandIndex_Right);

        for (int h = 0; h < eVRHandIndex_Count; ++h)
        {
          TrackedController& trackedHand = h == eVRHandIndex_Left
            ? mpInit->mpGame->vr_left_hand
            : mpInit->mpGame->vr_right_hand;
          if (!trackedHand.IsPoseValid()) continue;

          iHudModel *pHandModel = mpInit->mpPlayerHands->GetCurrentModel(h);
          if (!pHandModel) continue;
          // Nudge follows the real controller, not the collision-constrained
          // visual hand. This lets the physical hand keep pushing a movable
          // body while the rendered hand rests against it.
          cMatrixf handPose = VRHelper::TrackingToWorldSpace(
            trackedHand.GetMatrix(), mpInit->mpGame);
          handPose = cMath::MatrixMul(handPose,
            cMath::MatrixTranslate(pHandModel->mvVrTransOffset));
          cVector3f vHandCenter = handPose.GetTranslation();

          // Skip if hand is occupied (holding an object)
          if ((h == eVRHandIndex_Left ? pLeftHeldBody : pRightHeldBody) != NULL)
            continue;

          cVector3f vWorldVel =
            mpInit->mpGame->vr_tracking.TrackingDirectionToWorld(trackedHand.GetVelocity());
          if (vWorldVel.SqrLength() <
              kVRNudgeMinHandSpeed * kVRNudgeMinHandSpeed)
            continue; // ignore micro-movements

          cMatrixf nudgeMtx = cMath::MatrixTranslate(vHandCenter);

          cBoundingVolume nudgeBV = nudgeShape->GetBoundingVolume();
          nudgeBV.SetTransform(cMath::MatrixMul(nudgeMtx,
            nudgeBV.GetTransform()));

          cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
          cPortalContainerEntityIterator bodyIt =
            pWorld->GetPortalContainer()->GetEntityIterator(&nudgeBV);

          // Check which nearby bodies overlap the compact nudge sphere.
          cCollideData collideData;
          collideData.SetMaxSize(1);
          while (bodyIt.HasNext())
          {
            iPhysicsBody *pBody = static_cast<iPhysicsBody*>(bodyIt.Next());
            if (pBody == NULL || pBody->GetUserData() == NULL) continue;
            if (pBody->IsActive() == false || pBody->GetCollide() == false)
              continue;
            const float fBodyMass = pBody->GetMass();
            if (fBodyMass <= 0.0f) continue; // skip statics
            if (pBody->IsCharacter() || pBody->IsPlayer()) continue;

            iGameEntity *pNudgeEntity = (iGameEntity*)pBody->GetUserData();
            bool bNudgeable =
              pNudgeEntity->GetType() == eGameEntityType_Object ||
              pNudgeEntity->GetType() == eGameEntityType_Item ||
              pNudgeEntity->GetType() == eGameEntityType_SwingDoor;
            if (!bNudgeable) continue;

            // Skip if any hand is already holding this body
            if (pLeftHeldBody == pBody || pRightHeldBody == pBody)
              continue;

            // The portal query is sector-level, so retain a precise cheap
            // broad-phase reject before the narrow shape test.
            cBoundingVolume *pBV = pBody->GetBV();
            const float fBroadPhaseRadius =
              pBV->GetRadius() + kVRNudgeBroadPhasePadding;
            if ((pBV->GetWorldCenter() - vHandCenter).SqrLength() >
                fBroadPhaseRadius * fBroadPhaseRadius)
              continue;

            if (!pPhysicsWorld->CheckShapeCollision(
                  pBody->GetShape(), pBody->GetLocalMatrix(),
                  nudgeShape, nudgeMtx, collideData, 1))
              continue;

            // The starting boat locker is unlocked by deactivating lock1.
            // Do not let the palm-nudge fallback move its hinged body while
            // that lock entity is still active. Keep this map lookup after
            // the geometric filters so distant objects pay no locker cost.
            if (pNudgeEntity->GetType() == eGameEntityType_Object &&
                static_cast<cGameObject*>(pNudgeEntity)->IsPadlocked())
              continue;

            // Only push when the hand is moving INTO the object
            cVector3f vContact = VRHelper::CollideCenter(
              collideData.mvContactPoints, collideData.mlNumOfPoints);
            cVector3f vToObj = vContact - vHandCenter;
            if (cMath::Vector3Dot(vWorldVel, vToObj) <= 0.0f)
              continue;

            // Project the motion onto a joint's permitted direction before
            // calculating the impulse. Otherwise most of a drawer push is
            // discarded by its slider and most of a door push fights its
            // hinge, which made nudging them feel much heavier than grabbing.
            cVector3f vPushVelocity = vWorldVel;
            bool bConstrainedJoint = false;
            if (pBody->GetJointNum() > 0)
            {
              iPhysicsJoint *pJoint = GetVRNudgeJoint(pBody);
              if (pJoint != NULL)
              {
                cVector3f vPin = cMath::Vector3Normalize(pJoint->GetPinDir());
                if (pJoint->GetType() == ePhysicsJointType_Slider)
                {
                  vPushVelocity = vPin * cMath::Vector3Dot(vWorldVel, vPin);
                  bConstrainedJoint = true;
                }
                else if (pJoint->GetType() == ePhysicsJointType_Hinge)
                {
                  cVector3f vRadial = vContact - pJoint->GetPivotPoint();
                  vRadial -= vPin * cMath::Vector3Dot(vRadial, vPin);
                  if (vRadial.Length() > 0.02f)
                  {
                    cVector3f vTangent = cMath::Vector3Normalize(
                      cMath::Vector3Cross(vPin, vRadial));
                    vPushVelocity = vTangent *
                      cMath::Vector3Dot(vWorldVel, vTangent);
                    bConstrainedJoint = true;
                  }
                }
              }
            }

            float fUsableSpeed = vPushVelocity.Length();
            if (fUsableSpeed < 0.02f) continue;
            cVector3f vPushDir = vPushVelocity * (1.0f / fUsableSpeed);
            bool bGentleGrabCandidate = false;
            bool bBreakableObject = false;
            if (pNudgeEntity->GetType() == eGameEntityType_Object)
            {
              cGameObject *pObject = (cGameObject*)pNudgeEntity;
              bGentleGrabCandidate =
                pObject->GetInteractMode() == eObjectInteractMode_Grab ||
                pObject->GetInteractMode() == eObjectInteractMode_Move;
              bBreakableObject = pObject->IsBreakable();
            }

            bool bLockedDoor = false;
            if (pNudgeEntity->GetType() == eGameEntityType_SwingDoor)
              bLockedDoor = static_cast<cGameSwingDoor*>(pNudgeEntity)->IsLocked();

            // Give compact, light props a little more displacement per touch.
            // Radius as well as mass keeps large lightweight mechanisms from
            // being treated like batteries, cans or other loose pickups.
            const bool bSmallLight =
              fBodyMass <= 3.0f && pBV->GetRadius() <= 0.30f;
            const bool bLargeHeavy =
              fBodyMass >= 12.0f || pBV->GetRadius() >= 0.75f;

            float fPushFraction;
            float fMaxPushSpeed;
            float fMaxDeltaV;
            if (bLockedDoor)
            {
              // Preserve a little physical play against the padlock without
              // hammering the locked hinge limit hard enough to pull it off-axis.
              fPushFraction = 0.30f;
              fMaxPushSpeed = 0.30f;
              fMaxDeltaV = 0.10f;
            }
            else if (bConstrainedJoint)
            {
              fPushFraction = 0.80f;
              fMaxPushSpeed = 1.35f;
              fMaxDeltaV = 0.36f;
            }
            else if (bBreakableObject)
            {
              // Bare-hand contact should roll or slide a destructible prop,
              // not cross its impact-break threshold. Weapons keep their own
              // attack path and can still destroy it normally.
              fPushFraction = 0.20f;
              fMaxPushSpeed = 0.32f;
              fMaxDeltaV = 0.08f;
            }
            else if (bSmallLight)
            {
              fPushFraction = 0.60f;
              fMaxPushSpeed = 0.90f;
              fMaxDeltaV = 0.28f;
            }
            else if (bLargeHeavy)
            {
              // A hand can start a barrel or loaded crate moving, but repeated
              // overlap frames must not behave like full-strength punches.
              fPushFraction = 0.22f;
              fMaxPushSpeed = 0.38f;
              fMaxDeltaV = 0.10f;
            }
            else if (bGentleGrabCandidate)
            {
              fPushFraction = 0.34f;
              fMaxPushSpeed = 0.65f;
              fMaxDeltaV = 0.22f;
            }
            else
            {
              fPushFraction = 0.50f;
              fMaxPushSpeed = 1.00f;
              fMaxDeltaV = 0.40f;
            }
            float fDesiredPushSpeed = cMath::Clamp(
              fUsableSpeed * fPushFraction, 0.0f, fMaxPushSpeed);
            cVector3f vContactVelocity = pBody->GetLinearVelocity() +
              cMath::Vector3Cross(pBody->GetAngularVelocity(),
                vContact - pBody->GetWorldPosition());
            float fCurrentPushSpeed = cMath::Vector3Dot(
              vContactVelocity, vPushDir);
            float fDeltaV = cMath::Clamp(
              fDesiredPushSpeed - fCurrentPushSpeed, 0.0f, fMaxDeltaV);
            if (fDeltaV <= 0.005f) continue;
            pBody->AddImpulseAtPosition(
              vPushDir * (fDeltaV * fBodyMass), vContact);

            static unsigned long slNextNudgeLogMs = 0;
            unsigned long lNudgeNowMs = GetApplicationTime();
            if (lNudgeNowMs >= slNextNudgeLogMs)
            {
              slNextNudgeLogMs = lNudgeNowMs + 1000;
              Log(" [VR nudge +%lu ms] %s hand pushed '%s' type=%d joint=%d locked=%d small=%d heavy=%d breakable=%d mass=%.2f radius=%.2f dv=%.2f.\n",
                lNudgeNowMs, h == eVRHandIndex_Left ? "left" : "right",
                pBody->GetName().c_str(), (int)pNudgeEntity->GetType(),
                bConstrainedJoint ? 1 : 0, bLockedDoor ? 1 : 0,
                bSmallLight ? 1 : 0, bLargeHeavy ? 1 : 0,
                bBreakableObject ? 1 : 0,
                fBodyMass, pBV->GetRadius(), fDeltaV);
            }

            // Subtle haptic pulse on the touching hand, scaled to the push
            eVRHapticHand hapticHand = (h == eVRHandIndex_Left)
              ? eVRHapticHand_Left : eVRHapticHand_Right;
            cVRHaptics::Play(mpInit, eVRHapticEvent_Interaction, hapticHand,
              cMath::Clamp(fDeltaV, 0.05f, 0.5f));
          }
        }
      }
    }

// Existing Penumbra entity code still reads the legacy picked-body slot.
    // Mirror the dominant target for those passive paths; explicit interaction
    // restores the target belonging to the hand whose trigger was pressed.
    const cVRHandTarget& dominantTarget = mpPlayer->GetVRHandTarget(dominantIndex);
    mpPlayer->GetPickRay()->Clear();
    if (dominantTarget.mpBody != NULL)
    {
      mpPlayer->GetPickRay()->mpPickedBody = dominantTarget.mpBody;
      mpPlayer->GetPickRay()->mvPickedPos = dominantTarget.mvPosition;
      mpPlayer->GetPickRay()->mfPickedDist = dominantTarget.mfDistance;
    }
  }

  /////////////////////////////////////////////////
  // Cast ray to see if anything could be examined.
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
}

void cPlayerState_Normal_VR::OnPostSceneDraw() {

  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();
  //pWorld->GetPhysicsWorld()->RenderDebugGeometry(mpLowGfx, cColor(1.0f, 0.0f, 1.0f, 1.0f));

  // World-only magnetic acquisition cue. This deliberately does not use or
  // modify cVRPointer / pointerDrawActive, which remain owned by inventory and
  // menu UI. Suppress it whenever a UI surface is active so the two systems
  // cannot overlap visually.
  const bool bWorldCueAllowed =
    mpInit->mpButtonHandler->GetState() == eButtonHandlerState_Game &&
    !mpInit->mpInventory->IsActive() &&
    !mpInit->mpNotebook->IsActive() &&
    !mpInit->mpNumericalPanel->IsActive() &&
    !mpInit->mpDeathMenu->IsActive();
  if(bWorldCueAllowed)
  {
    mpLowGfx->SetDepthTestActive(false);
    for(int i = 0; i < eVRHandIndex_Count; ++i)
    {
      const cVRHandTarget& target = mpPlayer->GetVRHandTarget((eVRHandIndex)i);
      if(!target.mbMagnetic || target.mpBody == NULL ||
        !target.mpBody->IsActive())
        continue;

      cVector3f vHandAnchor;
      if(!mpInit->mpPlayerHands->GetHandMiddleFingerPosition(i, vHandAnchor))
      {
        cMatrixf handPose;
        if(!mpInit->mpPlayerHands->GetHandPalmPose(i, handPose))
          continue;
        vHandAnchor = handPose.GetTranslation();
      }

      mpLowGfx->DrawLine(target.mvPosition, vHandAnchor,
        cColor(0.10f, 0.65f, 1.0f, 0.92f));
    }
    mpLowGfx->SetDepthTestActive(true);
  }

  {

    cCamera3D* pCamera3D = (cCamera3D*)mpInit->mpGame->GetScene()->GetCamera();

    for (int i = 0; i < 2; ++i) {
      auto player = mpInit->mpPlayer;
      auto crosshairState = player->GetHandCrossHairState(i);

      if (crosshairState == eCrossHairState_None)
        continue;

      auto crosshairMat = player->mvCrossHairs[crosshairState]->GetMaterial();

      mpLowGfx->SetDepthTestActive(false);
      mpLowGfx->PushMatrix(eMatrix_ModelView);

      cMatrixf iconMat = cMatrixf::Identity;

      // Rotate to face eyes
      iconMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), iconMat);

      // Move to hand's position
      TrackedController& trackedHand = i == eVRHandIndex_Left
        ? mpInit->mpGame->vr_left_hand
        : mpInit->mpGame->vr_right_hand;
      if (!trackedHand.IsPoseValid()) {
        mpLowGfx->PopMatrix(eMatrix_ModelView);
        mpLowGfx->SetDepthTestActive(true);
        continue;
      }
      cMatrixf handMat;
      iHudModel *pHandModel = mpInit->mpPlayerHands->GetCurrentModel(i);
      if (pHandModel == NULL || !pHandModel->UpdatePoseMatrix(handMat, 0.0f))
        handMat = VRHelper::TrackingToWorldSpace(trackedHand.GetMatrix(), mpInit->mpGame);
      iconMat = cMath::MatrixMul(cMath::MatrixTranslate(handMat.GetTranslation()), iconMat);

      mpLowGfx->SetMatrix(eMatrix_ModelView, cMath::MatrixMul(pCamera3D->GetViewMatrix(), iconMat));

      auto ofs = crosshairMat->GetTextureOffset(eMaterialTexture_Diffuse);

      tVertexVec vtx;
      vtx.resize(4);

      cColor color(1.0f, 1.0f);

      vtx[0] = cVertex(cVector3f(-0.035f, 0.035, 0), cVector2f(ofs.x, ofs.y), color);
      vtx[1] = cVertex(cVector3f(0.035, 0.035, 0), cVector2f(ofs.x + ofs.w, ofs.y), color);
      vtx[2] = cVertex(cVector3f(0.035, -0.035, 0), cVector2f(ofs.x + ofs.w, ofs.y + ofs.h), color);
      vtx[3] = cVertex(cVector3f(-0.035, -0.035, 0), cVector2f(ofs.x, ofs.y + ofs.h), color);

      mpLowGfx->SetTexture(0, crosshairMat->GetTexture(eMaterialTexture_Diffuse));
      mpLowGfx->SetBlendActive(true);
      mpLowGfx->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
      mpLowGfx->SetCullActive(false);

      mpLowGfx->DrawQuad(vtx);

      mpLowGfx->SetTexture(0, NULL);
      mpLowGfx->SetBlendActive(true);
      mpLowGfx->SetCullActive(true);

      mpLowGfx->PopMatrix(eMatrix_ModelView);
      mpLowGfx->SetDepthTestActive(true);
    }
  }

  // mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld()->DestroyShape(collider);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartInteract()
{
  // Prefer the hand that physically pressed interact; fall back to the other
  // hand when the pressing one has nothing in reach.  Remember which hand
  // initiated so the grab state tracks that controller.
  const eVRHandIndex preferredIndex = mpInit->mpGame->vr_input.GetInteractSourceHand() == eSteamVRHand_Left
    ? eVRHandIndex_Left : eVRHandIndex_Right;
  const eVRHandIndex fallbackIndex = preferredIndex == eVRHandIndex_Left
    ? eVRHandIndex_Right : eVRHandIndex_Left;

  eVRHandIndex chosen = preferredIndex;
  if (mpPlayer->GetVRHandTarget(preferredIndex).mpBody == NULL &&
      mpPlayer->GetVRHandTarget(fallbackIndex).mpBody != NULL)
    chosen = fallbackIndex;

  const cVRHandTarget& target = mpPlayer->GetVRHandTarget(chosen);
  if (target.mpBody)
  {
	Log(" [VR DBG +%lu ms] OnStartInteract target='%s' type=%d crosshair=%d dist=%.2f magnetic=%d\n",
	  GetApplicationTime(), target.mpBody->GetName().c_str(),
	  (int)((iGameEntity*)target.mpBody->GetUserData())->GetType(),
	  (int)target.mCrossHairState, target.mfDistance,
	  target.mbMagnetic ? 1 : 0);

    // Keep legacy entity callbacks coherent while the interaction states are
    // migrated incrementally to explicit hand ownership.
    mpPlayer->SetVRInteractHand(chosen);
    mpPlayer->GetPickRay()->mpPickedBody = target.mpBody;
    mpPlayer->GetPickRay()->mvPickedPos = target.mvPosition;
    mpPlayer->GetPickRay()->mfPickedDist = target.mfDistance;
    iGameEntity *pEntity = (iGameEntity*)target.mpBody->GetUserData();

    if (pEntity->GetType() == eGameEntityType_SwingDoor)
    {
      cGameSwingDoor *pDoor = static_cast<cGameSwingDoor*>(pEntity);
      if (pDoor->IsLocked())
      {
        Log(" [VR interaction +%lu ms] door '%s' is locked - buzz haptic, reject interact.\n",
            GetApplicationTime(), pDoor->GetName().c_str());
        eVRHapticHand hapticHand = (chosen == eVRHandIndex_Left) ? eVRHapticHand_Left : eVRHapticHand_Right;
        cVRHaptics::Play(mpInit, eVRHapticEvent_ObjectDrop, hapticHand);
        return;
      }
    }

    pEntity->PlayerInteract();
  }
else
  {
	Log(" [VR interaction +%lu ms] interact pressed with no %s-hand target.\n",
	  GetApplicationTime(), chosen == eVRHandIndex_Left ? "left" : "right");
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartExamine()
{
  //Log("Picked body: %d\n",(size_t)mpPlayer->GetPickedBody());
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartRun()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Walk ||
    mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Run);
  }
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
  {
    mpPlayer->SetPrevMoveState(ePlayerMoveState_Run);
  }
}
void cPlayerState_Normal_VR::OnStopRun()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Run)
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  else if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)
    mpPlayer->SetPrevMoveState(ePlayerMoveState_Walk);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  const ePlayerMoveState previousState = mpPlayer->GetMoveState();

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }

  Log(" [VR comfort +%lu ms] crouch pressed; move state %d -> %d.\n",
	GetApplicationTime(), (int)previousState, (int)mpPlayer->GetMoveState());
}
void cPlayerState_Normal_VR::OnStopCrouch()
{
  const ePlayerMoveState previousState = mpPlayer->GetMoveState();
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }

  if(previousState != mpPlayer->GetMoveState())
  {
	Log(" [VR comfort +%lu ms] crouch released; move state %d -> %d.\n",
	  GetApplicationTime(), (int)previousState, (int)mpPlayer->GetMoveState());
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::OnStartInteractMode()
{
  mpPlayer->ChangeState(ePlayerState_InteractMode);
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::EnterState(iPlayerState* apPrevState)
{
  mpPlayer->ResetCrossHairPos();
  for(int i = 0; i < eVRHandIndex_Count; ++i)
    mpLastMagneticTargets[i] = NULL;
}

//-----------------------------------------------------------------------

void cPlayerState_Normal_VR::LeaveState(iPlayerState* apNextState)
{
  for(int i = 0; i < eVRHandIndex_Count; ++i)
    mpLastMagneticTargets[i] = NULL;
  //Can cause crashes!!
  //mpPlayer->GetPickRay()->mpPickedBody = NULL;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// INTERACT MODE STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_InteractMode_VR::cPlayerState_InteractMode_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_InteractMode)
{
  mvLookSpeed = 0;
  mfRange = 15.0f;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnUpdate(float afTimeStep)
{
  /////////////////////////////////////////////////
  // Move viewport.
  /*if(mvLookSpeed.x != 0)
  {
  mpPlayer->GetCamera()->AddYaw(mvLookSpeed.x * afTimeStep * 1.0f);
  mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  if(mvLookSpeed.y != 0)
  {
  mpPlayer->GetCamera()->AddPitch(mvLookSpeed.y * afTimeStep * 1.0f);
  }*/

  //LogUpdate("  Casting ray\n");
  /////////////////////////////////////////////////
  // Cast ray to see if anything is picked.
  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  cVector3f vDir = mpPlayer->GetCamera()->UnProject(
    mpPlayer->GetCrossHairPos(),
    mpInit->mpGame->GetGraphics()->GetLowLevel());
  vStart = mpPlayer->GetCamera()->GetPosition();
  vEnd = vStart + vDir * mpPlayer->GetPickRay()->mfMaxDistance;

  mpPlayer->GetPickRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);

  //LogUpdate("  Calc pickray results\n");	
  mpPlayer->GetPickRay()->CalculateResults();

  //LogUpdate("  Use picked body\n");	
  if (mpPlayer->GetPickedBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

    eCrossHairState CrossState = pEntity->GetPickCrossHairState(mpPlayer->GetPickedBody());

    if (CrossState == eCrossHairState_None)
      mpPlayer->SetCrossHairState(eCrossHairState_Inactive);
    else
      mpPlayer->SetCrossHairState(CrossState);

    pEntity->PlayerPick();
  }
  else
  {
    mpPlayer->SetCrossHairState(eCrossHairState_Inactive);
  }
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartInteract()
{
  if (mpPlayer->GetPickedBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();

    pEntity->PlayerInteract();
  }
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    pEntity->PlayerExamine();
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_InteractMode_VR::OnAddYaw(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * 2.0f * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  else
  {
    if (mpPlayer->AddCrossHairPos(cVector2f(afVal * 800.0f, 0)))
    {
      mpPlayer->GetCamera()->AddYaw(-afVal * mpPlayer->GetLookSpeed());
      mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
    }

    cVector2f vBorder = mpPlayer->GetInteractMoveBorder();
    cVector2f vPos = mpPlayer->GetCrossHairPos();

    if (vPos.x < vBorder.x + mfRange || vPos.x >(799 - vBorder.x - mfRange))
    {
      float fDist;
      if (vPos.x < vBorder.x + mfRange)
      {
        fDist = (vPos.x - vBorder.x);
        mvLookSpeed.x = 1 - (fDist / mfRange) * 1;
      }
      else
      {
        fDist = ((799 - vBorder.x) - vPos.x);
        mvLookSpeed.x = (1 - (fDist / mfRange)) * -1;
      }
    }
    else
    {
      mvLookSpeed.x = 0;
    }
  }

  return false;
}

bool cPlayerState_InteractMode_VR::OnAddPitch(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    float fInvert = mpInit->mpButtonHandler->GetInvertMouseY() ? -1.0f : 1.0f;
    mpPlayer->GetCamera()->AddPitch(-afVal *2.0f*fInvert * mpPlayer->GetLookSpeed());
  }
  else
  {
    if (mpPlayer->AddCrossHairPos(cVector2f(0, afVal * 600.0f)))
    {
      mpPlayer->GetCamera()->AddPitch(-afVal * mpPlayer->GetLookSpeed());
    }

    cVector2f vBorder = mpPlayer->GetInteractMoveBorder();
    cVector2f vPos = mpPlayer->GetCrossHairPos();

    if (vPos.y < vBorder.y + mfRange || vPos.y >(599 - vBorder.y - mfRange))
    {
      float fDist;
      if (vPos.y < vBorder.y + mfRange)
      {
        fDist = (vPos.y - vBorder.y);
        mvLookSpeed.y = 1 - (fDist / mfRange) * 1;
      }
      else
      {
        fDist = ((599 - vBorder.y) - vPos.y);
        mvLookSpeed.y = (1 - (fDist / mfRange)) * -1;
      }
    }
    else
    {
      mvLookSpeed.y = 0;
    }
  }

  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartInteractMode()
{
  mpPlayer->ChangeState(ePlayerState_Normal);
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::EnterState(iPlayerState* apPrevState)
{
  mPrevMoveState = mpPlayer->GetMoveState();

  if (mpPlayer->GetMoveState() == ePlayerMoveState_Run)
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);

  mvLookSpeed = 0;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::LeaveState(iPlayerState* apNextState)
{
  /*if(mPrevMoveState != ePlayerMoveState_Run)
  mpPlayer->ChangeMoveState(mPrevMoveState);
  else
  mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);*/

  if (apNextState->mType == ePlayerState_Normal)
    mpPlayer->ResetCrossHairPos();

  //Can cause crashes!!
  //mpPlayer->GetPickRay()->mpPickedBody = NULL;
}

//-----------------------------------------------------------------------

void cPlayerState_InteractMode_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
}

void cPlayerState_InteractMode_VR::OnStopCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// USE ITEM STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_UseItem_VR::cPlayerState_UseItem_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_UseItem)
{
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnUpdate(float afTimeStep)
{
  /////////////////////////////////////////////////
// Cast ray to see if anything is picked.
  iPhysicsWorld *pPhysicsWorld = mpInit->mpGame->GetScene()->GetWorld3D()->GetPhysicsWorld();
  cVector3f vStart, vEnd;

  cMatrixf handMat = VRHelper::TrackingToWorldSpace(VRHelper::DominantAimMatrix(mpInit->mpGame), mpInit->mpGame);

  vStart = handMat.GetTranslation();
  // Rotate the view-space forward (-Z) by the aim rotation.  The former
  // inverse-rotation math mirrored the laser across orientations and pointed
  // it in wildly wrong directions.
  vEnd = vStart + cMath::MatrixMul(handMat.GetRotation(), cVector3f(0.0f, 0.0f, -1.0f));

  mvUseLineStart = vStart;
  mvUseLineEnd = vEnd;

  mpPlayer->GetPickRay()->Clear();
  pPhysicsWorld->CastRay(mpPlayer->GetPickRay(), vStart, vEnd, true, false, true);
  mpPlayer->GetPickRay()->CalculateResults();

  iPhysicsBody *pBody = mpPlayer->GetPickedBody();
  iGameEntity *pEntity = NULL;
  if (pBody) pEntity = (iGameEntity*)pBody->GetUserData();

  if (pEntity)
  {
    mvUseLineEnd = mpPlayer->GetPickedPos();
    mpInit->mpPlayer->SetItemFlash(true);
  }
  else
  {
    mpInit->mpPlayer->SetItemFlash(false);
  }
}

void cPlayerState_UseItem_VR::OnPostSceneDraw() {
  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  if (mpPlayer->GetItemFlash())
    mpLowGfx->DrawLine(mvUseLineStart, mvUseLineEnd, cColor(0, 1, 0, 0.75f));
  else
    mpLowGfx->DrawLine(mvUseLineStart, mvUseLineEnd, cColor(1, 0, 0, 0.75f));

  auto itemMat = mpPlayer->GetCurrentItem()->GetGfxObject()->GetMaterial();

  mpLowGfx->SetDepthTestActive(false);
  mpLowGfx->PushMatrix(eMatrix_ModelView);

  // Anchor the item card at the laser origin, billboarded to face the eyes,
  // so it reads as a label riding the beam instead of a decal glued to the
  // grip pose.
  cCamera3D* pCam3D = (cCamera3D*)mpInit->mpGame->GetScene()->GetCamera();
  cVector3f vBeamDir = mvUseLineEnd - mvUseLineStart;
  if (vBeamDir.Length() > 0.0001f) vBeamDir.Normalise();
  else vBeamDir = pCam3D->GetForward();

  cMatrixf iconMat = cMath::MatrixInverse(pCam3D->GetViewMatrix().GetRotation());
  iconMat = cMath::MatrixMul(
    cMath::MatrixTranslate(mvUseLineStart + vBeamDir * 0.06f), iconMat);
  iconMat = cMath::MatrixMul(pCam3D->GetViewMatrix(), iconMat);

  mpLowGfx->SetMatrix(eMatrix_ModelView, iconMat);

  auto ofs = itemMat->GetTextureOffset(eMaterialTexture_Diffuse);

  tVertexVec vtx;
  vtx.resize(4);

  cColor color = mpPlayer->GetItemFlash() ? cColor(1.0f, 1.0f) : cColor(1.0f, 0.0f, 0.0f, 1.0f);

  float scaleFactor = 0.4f;

  vtx[0] = cVertex(cVector3f(-ofs.w * scaleFactor, ofs.h * scaleFactor, 0), cVector2f(ofs.x, ofs.y), color);
  vtx[1] = cVertex(cVector3f(ofs.w * scaleFactor, ofs.h * scaleFactor, 0), cVector2f(ofs.x + ofs.w, ofs.y), cColor(1.0f, 1.0f));
  vtx[2] = cVertex(cVector3f(ofs.w * scaleFactor, -ofs.h * scaleFactor, 0), cVector2f(ofs.x + ofs.w, ofs.y + ofs.h), cColor(1.0f, 1.0f));
  vtx[3] = cVertex(cVector3f(-ofs.w * scaleFactor, -ofs.h * scaleFactor, 0), cVector2f(ofs.x, ofs.y + ofs.h), cColor(1.0f, 1.0f));

  mpLowGfx->SetTexture(0, itemMat->GetTexture(eMaterialTexture_Diffuse));
  mpLowGfx->SetBlendActive(true);
  mpLowGfx->SetCullActive(false);

  mpLowGfx->DrawQuad(vtx);

  mpLowGfx->SetTexture(0, NULL);
  mpLowGfx->SetBlendActive(false);
  mpLowGfx->SetCullActive(true);

  mpLowGfx->PopMatrix(eMatrix_ModelView);
  mpLowGfx->SetDepthTestActive(true);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartInteract()
{
  iPhysicsBody *pBody = mpPlayer->GetPickedBody();
  iGameEntity *pEntity = NULL;
  if (pBody) pEntity = (iGameEntity*)pBody->GetUserData();

  if (pEntity && mpPlayer->GetPickedDist() <= pEntity->GetMaxExamineDist())
  {
    if (mpPlayer->GetPickedDist() <= pEntity->GetMaxInteractDist())
    {
      iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetPickedBody()->GetUserData();
      cGameItemType *pType = mpInit->mpInventory->GetItemType(mpPlayer->GetCurrentItem()->GetItemType());

      if (mPrevState == ePlayerState_WeaponMelee ||
        mPrevState == ePlayerState_Throw)
      {
        mpPlayer->ChangeState(ePlayerState_Normal);
      }
      else
      {
        mpPlayer->ChangeState(mPrevState);
      }

      pType->OnUse(mpPlayer->GetCurrentItem(), pEntity);
    }
    else
    {
      mpInit->mpEffectHandler->GetSubTitle()->Add(kTranslate("Player", "UseItemTooFar"), 2.0f, true);
      return;
    }
  }
  else
  {
    if (mPrevState == ePlayerState_WeaponMelee ||
      mPrevState == ePlayerState_Throw)
    {
      mpPlayer->ChangeState(ePlayerState_Normal);
    }
    else
    {
      mpPlayer->ChangeState(mPrevState);
    }
  }
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartExamine()
{
  if (mpPlayer->GetExamineBody())
  {
    iGameEntity *pEntity = (iGameEntity*)mpPlayer->GetExamineBody()->GetUserData();

    if (mpPlayer->GetExamineDist() <= pEntity->GetMaxExamineDist())
    {
      pEntity->PlayerExamine();
    }
  }
  else
  {
    if (mPrevState == ePlayerState_WeaponMelee ||
      mPrevState == ePlayerState_Throw)
    {
      mpPlayer->ChangeState(ePlayerState_Normal);
    }
    else
    {
      mpPlayer->ChangeState(mPrevState);
    }

  }
}

//-----------------------------------------------------------------------

bool cPlayerState_UseItem_VR::OnAddYaw(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * 2.0f * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }
  else if (mpPlayer->AddCrossHairPos(cVector2f(afVal * 800.0f, 0)))
  {
    mpPlayer->GetCamera()->AddYaw(-afVal * mpPlayer->GetLookSpeed());
    mpPlayer->GetCharacterBody()->SetYaw(mpPlayer->GetCamera()->GetYaw());
  }

  return false;
}

bool cPlayerState_UseItem_VR::OnAddPitch(float afVal)
{
  cInput *pInput = mpInit->mpGame->GetInput();

  if (pInput->IsTriggerd("LookMode"))
  {
    float fInvert = mpInit->mpButtonHandler->GetInvertMouseY() ? -1.0f : 1.0f;
    mpPlayer->GetCamera()->AddPitch(-afVal *2.0f*fInvert * mpPlayer->GetLookSpeed());
  }
  else if (mpPlayer->AddCrossHairPos(cVector2f(0, afVal * 600.0f)))
  {
    mpPlayer->GetCamera()->AddPitch(-afVal * mpPlayer->GetLookSpeed());
  }

  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::EnterState(iPlayerState* apPrevState)
{
  mPrevMoveState = mpPlayer->GetMoveState();
  mPrevState = apPrevState->mType;

  mpPlayer->SetCrossHairState(eCrossHairState_Item);

  // Hide right hand
  mpPlayer->GetRightHand()->SetVisible(false);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::LeaveState(iPlayerState* apNextState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  // Show right hand
  mpPlayer->GetRightHand()->SetVisible(true);
}

//-----------------------------------------------------------------------

void cPlayerState_UseItem_VR::OnStartCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Jump)return;

  if (mpInit->mpButtonHandler->GetToggleCrouch())
  {
    if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
      mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
    else
      mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
  else
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
  }
}

void cPlayerState_UseItem_VR::OnStopCrouch()
{
  if (mpPlayer->GetMoveState() == ePlayerMoveState_Crouch &&
    mpInit->mpButtonHandler->GetToggleCrouch() == false)
  {
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
  }
}

//-----------------------------------------------------------------------

bool cPlayerState_UseItem_VR::OnStartInventory()
{
  mpPlayer->ChangeState(mPrevState);
  return true;
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// MESSAGE STATE
//////////////////////////////////////////////////////////////////////////

cPlayerState_Message_VR::cPlayerState_Message_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Message)
{
}

//-----------------------------------------------------------------------


void cPlayerState_Message_VR::OnUpdate(float afTimeStep)
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnJump()
{
  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::OnStartInteract()
{
  mpInit->mpGameMessageHandler->ShowNext();
}

void cPlayerState_Message_VR::OnStopInteract()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::OnStartExamine()
{
  mpInit->mpGameMessageHandler->ShowNext();
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnMoveForwards(float afMul, float afTimeStep) { return false; }
bool cPlayerState_Message_VR::OnMoveSideways(float afMul, float afTimeStep) { return false; }

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnAddYaw(float afVal) { return false; }
bool cPlayerState_Message_VR::OnAddPitch(float afVal) { return false; }

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::EnterState(iPlayerState* apPrevState)
{
  //Change move state so the player is still
  mPrevMoveState = mpPlayer->GetMoveState();
  //mpPlayer->ChangeMoveState(ePlayerMoveState_Still);

  mpPlayer->SetCrossHairState(eCrossHairState_None);
}

//-----------------------------------------------------------------------

void cPlayerState_Message_VR::LeaveState(iPlayerState* apNextState)
{
  if (mPrevMoveState != ePlayerMoveState_Run && mPrevMoveState != ePlayerMoveState_Jump)
    mpPlayer->ChangeMoveState(mPrevMoveState);
  else
    mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnStartInventory()
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Message_VR::OnStartInventoryShortCut(int alNum)
{
  return false;
}
//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// CLIMB STATE
//////////////////////////////////////////////////////////////////////////

static int NUM_LADDER_SOUNDS = 3;
static float LADDER_SOUND_SPACING = 0.65f;

cPlayerState_Climb_VR::cPlayerState_Climb_VR(cInit *apInit, cPlayer *apPlayer) : iPlayerState(apInit, apPlayer, ePlayerState_Climb)
{
  mpLadder = NULL;

  mfUpSpeed = mpInit->mpGameConfig->GetFloat("Movement_Climb", "UpSpeed", 0);
  mfDownSpeed = mpInit->mpGameConfig->GetFloat("Movement_Climb", "DownSpeed", 0);

  mfStepLength = mpInit->mpGameConfig->GetFloat("Movement_Climb", "StepLength", 0);

  mfStepCount = 0;

  mlState = 0;

  blackActive = false;
  blackAlpha = 0.0f;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::PlaySound(const tString &asSound)
{
  if (asSound == "") return;

  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cWorld3D *pWorld = mpInit->mpGame->GetScene()->GetWorld3D();

  cSoundEntity *pSound = pWorld->CreateSoundEntity("LadderStep", asSound, true);
  if (pSound) pSound->SetPosition(pCharBody->GetPosition());
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::EnterState(iPlayerState* apPrevState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  mvPrevPitchLimits = mpPlayer->GetCamera()->GetPitchLimits();

  mlState = 0;
  mpPlayer->GetCharacterBody()->SetGravityActive(false);
  mpPlayer->GetCharacterBody()->SetTestCollision(false);

  mvGoalPos = mvStartPosition;
  mvGoalRot.x = 0;
  mvGoalRot.y = mpLadder->GetStartRotation().y;

  //Different time if you are above the ladder.
  float fTime = 0.5f;
  if (mpPlayer->GetCharacterBody()->GetPosition().y > mpLadder->GetMaxY()) fTime = 1.2f;

  cVector3f vStartRot;
  vStartRot.x = mpPlayer->GetCamera()->GetPitch();
  vStartRot.y = mpPlayer->GetCamera()->GetYaw();

  mvPosAdd = (mvGoalPos - mpPlayer->GetCharacterBody()->GetPosition()) / fTime;

  mvRotAdd.x = cMath::GetAngleDistance(vStartRot.x, mvGoalRot.x, k2Pif) / fTime;
  mvRotAdd.y = cMath::GetAngleDistance(vStartRot.y, mvGoalRot.y, k2Pif) / fTime;

  mvCharPosition = mpPlayer->GetCharacterBody()->GetPosition();

  mfTimeCount = fTime;

  mfStepCount = 0;

  mbPlayedSound = false;

  //Play Sound
  PlaySound(mpLadder->GetAttachSound());

  // For VR
  ladderSoundsLeft = max(2, (fabs(mpLadder->GetMaxY() - mpLadder->GetMinY()) / 1.2f));
  ladderSoundCount = LADDER_SOUND_SPACING;

  // Figure out if player is moving down or up ladder
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cVector3f playerCenter = pCharBody->GetPosition() - pCharBody->GetSize().y * 0.5f;

  float topDist = cMath::Abs(playerCenter.y - mpLadder->GetMaxY());
  float bottomDist = cMath::Abs(playerCenter.y - mpLadder->GetMinY());

  if (topDist < bottomDist) {
    // Starting from top, moving to bottom of ladder
    ladderGoal = 0;

    mfLeaveAtTopCount = 0;
  }
  else {
    // Starting from bottom, moving to top of ladder
    ladderGoal = 1;

    mfLeaveAtTopCount = 2;
  }
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::LeaveState(iPlayerState* apNextState)
{
  mpPlayer->SetCrossHairState(eCrossHairState_None);

  mpPlayer->GetCharacterBody()->SetGravityActive(true);
  mpPlayer->GetCharacterBody()->SetTestCollision(true);

  mpPlayer->GetCamera()->SetPitchLimits(mvPrevPitchLimits);
  mpPlayer->GetCamera()->SetYawLimits(cVector2f(0, 0));
}


//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnUpdate(float afTimeStep)
{
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();
  cCamera3D *pCam = mpPlayer->GetCamera();

  //////////////////////////////////
  // Attach To Ladder (for VR, fade screen to black)
  if (mlState == 0)
  {
    blackActive = true;

    if (blackAlpha < 1.0f) {
      blackAlpha += afTimeStep;
    }

    if (blackAlpha >= 1.0f) {
      blackAlpha = 1.0f;

      mlState++;
    }
  }
  //////////////////////////////////
  // Move On Ladder (for VR, play ladder sounds, wait for time)
  else if (mlState == 1)
  {
    if (ladderSoundsLeft > 0) {
      if (ladderSoundCount < 0) {
        if (ladderGoal == 0) // Moving down
          PlaySound(mpLadder->GetClimbDownSound());
        else // Moving up
          PlaySound(mpLadder->GetClimbUpSound());

        ladderSoundCount = LADDER_SOUND_SPACING;
        ladderSoundsLeft--;
      }
      else
        ladderSoundCount -= afTimeStep;
    }
    else {
      // No sounds to play left, wait some time then go to exit state
      ladderSoundCount -= afTimeStep;
      if (ladderSoundCount <= 0) {
        cVector3f exitPos = mvGoalPos;

        if (ladderGoal == 0) {
          // Set player to bottom of ladder
          exitPos.y = mpLadder->GetMinY() + 0.35f;
        }
        else {
          // Set player to top of ladder
          exitPos.y = mpLadder->GetMaxY();
          exitPos += pCharBody->GetForward();
        }

        pCharBody->SetFeetPosition(exitPos);

        mlState++;
      }
    }
  }
  //////////////////////////////////
  // On the top of the ladder
  else if (mlState == 2)
  {
    if (blackAlpha > 0.0f) {
      blackAlpha -= afTimeStep;
    }

    if (blackAlpha <= 0.0f) {
      blackAlpha = 0.0f;
      blackActive = false;

      mpPlayer->ChangeState(ePlayerState_Normal);
    }
  }

}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartInteract()
{
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartExamine()
{
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnAddYaw(float afVal)
{
  return false;
}

bool cPlayerState_Climb_VR::OnAddPitch(float afVal)
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnMoveForwards(float afMul, float afTimeStep)
{
  return false;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnMoveSideways(float afMul, float afTimeStep)
{
  iCharacterBody *pCharBody = mpPlayer->GetCharacterBody();



  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnStartCrouch()
{
}

void cPlayerState_Climb_VR::OnStopCrouch()
{

}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnJump()
{
  return true;
}

//-----------------------------------------------------------------------

bool cPlayerState_Climb_VR::OnStartInventory()
{
  return false;
}

//-----------------------------------------------------------------------

void cPlayerState_Climb_VR::OnPostSceneDraw() {
  if (!blackActive) return;

  iLowLevelGraphics *mpLowGfx = mpInit->mpGame->GetGraphics()->GetLowLevel();

  auto pCamera3D = mpPlayer->GetCamera();
  cMatrixf uiViewMat = cMatrixf::Identity;

  // Translate to center of vision
  cMatrixf headInverse = cMath::MatrixInverse(pCamera3D->GetViewMatrix());
  cVector3f uiPos = headInverse.GetTranslation() + pCamera3D->GetViewMatrix().GetRotation().GetForward() * -0.75f;

  auto translateMat = cMath::MatrixTranslate(uiPos);

  auto centerTranslationMat = cMath::MatrixTranslate(cVector3f(-400.0f, -250.0f, 0.0f));
  uiViewMat = cMath::MatrixMul(centerTranslationMat, uiViewMat);

  // Rotate to face eyes
  uiViewMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), uiViewMat);

  // Translate in front of eyes
  uiViewMat = cMath::MatrixMul(translateMat, uiViewMat);

  // Project to world space
  uiViewMat = cMath::MatrixMul(pCamera3D->GetViewMatrix(), uiViewMat);

  mpLowGfx->SetMatrix(eMatrix_ModelView, uiViewMat);

  mpLowGfx->SetDepthTestActive(false);
  mpLowGfx->SetDepthWriteActive(false);

  mpLowGfx->SetTexture(0, NULL);
  mpLowGfx->SetBlendActive(true);
  mpLowGfx->SetBlendFunc(eBlendFunc_SrcAlpha, eBlendFunc_OneMinusSrcAlpha);
  mpLowGfx->SetCullActive(false);

  mpLowGfx->DrawFilledRect2D(cRect2f(-9999.0f, -9999.0f, 99999.0f, 99999.0f), 0.0f, cColor(0.0f, 0.0f, 0.0f, blackAlpha));

  mpLowGfx->SetBlendActive(false);
  mpLowGfx->SetCullActive(true);

  mpLowGfx->SetDepthTestActive(true);
  mpLowGfx->SetDepthWriteActive(true);
}


//-----------------------------------------------------------------------
