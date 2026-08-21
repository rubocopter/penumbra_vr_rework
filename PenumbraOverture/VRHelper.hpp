#pragma once

#include "openvr.h"
#include "math/Math.h"
#include "game/Game.h"
#include "scene/Scene.h"
#include "VRHaptics.h"

using namespace hpl;

namespace VRHelper {
  static inline cMatrixf TrackingToWorldSpace(const cMatrixf& trackingSpaceMtx, cGame* game) {
    return game->vr_tracking.TrackingToWorld(trackingSpaceMtx);
  }

  // The preferred hand is an application preference owned by the VR layer.
  // Gameplay and UI must query these helpers instead of assuming that the
  // right hand is the primary interaction hand.
  static inline TrackedController& DominantHand(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left
      ? game->vr_left_hand : game->vr_right_hand;
  }

  static inline cMatrixf DominantAimMatrix(cGame* game) {
    return VRHelper::DominantHand(game).GetAimMatrix();
  }

  static inline TrackedController& OffHand(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left
      ? game->vr_right_hand : game->vr_left_hand;
  }

  // Haptic feedback follows the same hand ownership as the interaction it
  // reports (held object, used item, melee impact).
  static inline eVRHapticHand DominantHapticHand(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left
      ? eVRHapticHand_Left : eVRHapticHand_Right;
  }

  static inline eVRHapticHand OffHapticHand(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left
      ? eVRHapticHand_Right : eVRHapticHand_Left;
  }

  // cPlayerHands model slots: 0 = left hand, 1 = right hand. Equipment
  // (weapons, lights) must be attached to the slot that owns the object, not
  // to a fixed side, so handedness keeps working symmetrically.
  static inline int DominantHandSlot(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left ? 0 : 1;
  }

  static inline int OffHandSlot(cGame* game) {
    return game->vr_dominant_hand == eSteamVRHand_Left ? 1 : 0;
  }

  static inline cVector3f CollideCenter(tCollidePointVec collidePoints, int numContactPoints) {
    cVector3f sum;

    for (int i = 0; i < numContactPoints; ++i)
      sum += collidePoints[i].mvPoint;

    return sum / numContactPoints;
  }

  #define EPSILON 0.000001
  #define CROSS(dest,v1,v2) \
          dest[0]=v1[1]*v2[2]-v1[2]*v2[1]; \
          dest[1]=v1[2]*v2[0]-v1[0]*v2[2]; \
          dest[2]=v1[0]*v2[1]-v1[1]*v2[0];
  #define DOT(v1,v2) (v1[0]*v2[0]+v1[1]*v2[1]+v1[2]*v2[2])
  #define SUB(dest,v1,v2) \
          dest[0]=v1[0]-v2[0]; \
          dest[1]=v1[1]-v2[1]; \
          dest[2]=v1[2]-v2[2]; 

  /* the original jgt code */
  static inline int intersect_triangle(float orig[3], float dir[3],
    float vert0[3], float vert1[3], float vert2[3],
    float *t, float *u, float *v)
  {
    float edge1[3], edge2[3], tvec[3], pvec[3], qvec[3];
    float det, inv_det;

    /* find vectors for two edges sharing vert0 */
    SUB(edge1, vert1, vert0);
    SUB(edge2, vert2, vert0);

    /* begin calculating determinant - also used to calculate U parameter */
    CROSS(pvec, dir, edge2);

    /* if determinant is near zero, ray lies in plane of triangle */
    det = DOT(edge1, pvec);

    if (det > -EPSILON && det < EPSILON)
      return 0;
    inv_det = 1.0 / det;

    /* calculate distance from vert0 to ray origin */
    SUB(tvec, orig, vert0);

    /* calculate U parameter and test bounds */
    *u = DOT(tvec, pvec) * inv_det;
    if (*u < 0.0 || *u > 1.0)
      return 0;

    /* prepare to test V parameter */
    CROSS(qvec, tvec, edge1);

    /* calculate V parameter and test bounds */
    *v = DOT(dir, qvec) * inv_det;
    if (*v < 0.0 || *u + *v > 1.0)
      return 0;

    /* calculate t, ray intersects triangle */
    *t = DOT(edge2, qvec) * inv_det;

    return 1;
  }

// Projects the best available controller pose onto a complete 800x600 UI
  // surface. The dominant hand remains the primary pointer, while the other
  // hand takes over automatically if SteamVR temporarily loses its pose.
  static inline bool TraceUIPointer(cGame* game, const cMatrixf& uiMatrix,
    bool worldSpace, cVector2f* screenPos, cVector3f* rayStart,
    cVector3f* rayEnd, bool* usingLeftHand = NULL)
  {
    cMatrixf handMatrix;
    bool leftHand = false;

    if(DominantHand(game).IsPoseValid())
      handMatrix = DominantHand(game).GetMatrix();
    else if(OffHand(game).IsPoseValid())
    {
      handMatrix = OffHand(game).GetMatrix();
      leftHand = game->vr_dominant_hand != eSteamVRHand_Left;
    }
    else
      return false;

    if(worldSpace)
      handMatrix = TrackingToWorldSpace(handMatrix, game);

    cVector3f handPos = handMatrix.GetTranslation();
    cVector3f handForward = cMath::Vector3Normalize(
      cMath::MatrixInverse(handMatrix.GetRotation()).GetForward()) * -1.0f;

    cVector3f p00 = cMath::MatrixMul(uiMatrix, cVector3f(0.0f, 0.0f, 0.0f));
    cVector3f p01 = cMath::MatrixMul(uiMatrix, cVector3f(0.0f, 600.0f, 0.0f));
    cVector3f p10 = cMath::MatrixMul(uiMatrix, cVector3f(800.0f, 0.0f, 0.0f));
    cVector3f p11 = cMath::MatrixMul(uiMatrix, cVector3f(800.0f, 600.0f, 0.0f));

    float t = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    if(intersect_triangle(&handPos.v[0], &handForward.v[0],
      &p00.v[0], &p01.v[0], &p10.v[0], &t, &u, &v) && t > 0.0f)
    {
      x = v * 800.0f;
      y = u * 600.0f;
    }
    else if(intersect_triangle(&handPos.v[0], &handForward.v[0],
      &p11.v[0], &p10.v[0], &p01.v[0], &t, &u, &v) && t > 0.0f)
    {
      x = (1.0f - v) * 800.0f;
      y = (1.0f - u) * 600.0f;
    }
    else
      return false;

    if(screenPos) *screenPos = cVector2f(x, y);
    if(rayStart) *rayStart = handPos + handForward * 0.03f;
    if(rayEnd) *rayEnd = handPos + handForward * t;
    if(usingLeftHand) *usingLeftHand = leftHand;
    return true;
  }

  // Applies light smoothing to the screen cursor and publishes a matching
  // world/tracking-space laser for the stereo renderer.
  static inline bool UpdateUIPointer(cGame* game, const cMatrixf& uiMatrix,
    bool worldSpace, const cVector2f& currentPos, cVector2f* newPos,
    float smoothing = 0.40f)
  {
    cVector2f hitPos;
    cVector3f rayStart;
    cVector3f rayEnd;
    if(!TraceUIPointer(game, uiMatrix, worldSpace, &hitPos, &rayStart, &rayEnd))
    {
      game->pointerDrawActive = false;
      return false;
    }

    hitPos.x = currentPos.x + (hitPos.x - currentPos.x) * smoothing;
    hitPos.y = currentPos.y + (hitPos.y - currentPos.y) * smoothing;

    game->pointerDrawStart = rayStart;
    game->pointerDrawEnd = cMath::MatrixMul(uiMatrix,
      cVector3f(hitPos.x, hitPos.y, 0.0f));
    game->pointerDrawActive = true;
    if(newPos) *newPos = hitPos;
    return true;
  }
}
