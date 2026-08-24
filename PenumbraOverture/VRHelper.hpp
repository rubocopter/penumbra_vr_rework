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

  // Aim pose of any tracked controller with grip fallback: drivers or
  // bindings without /pose/aim leave the aim matrix stale, and an identity
  // matrix would cast a ray from the playspace origin.
  static inline cMatrixf ControllerAimMatrix(TrackedController& hand) {
    return hand.IsAimValid() ? hand.GetAimMatrix() : hand.GetMatrix();
  }

  // Aim pose of the dominant hand with grip fallback: drivers or bindings
  // without /pose/aim leave the aim matrix stale, and an identity matrix
  // would cast the interaction ray from the playspace origin.
  static inline cMatrixf DominantAimMatrix(cGame* game) {
    return ControllerAimMatrix(DominantHand(game));
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

  // Solves the pointer ray against the plane of the 800x600 UI surface and
  // returns the hit position in UI pixels plus the distance along the ray.
  // The projected surface is an affine rectangle, so one planar solve covers
  // the whole quad exactly where the old per-triangle tests only accepted
  // rays passing through it. Rays that miss the quad still report the point
  // where they cross the plane, letting callers pin the cursor to the panel
  // instead of hiding the laser until the wrist finds an exact angle.
  static inline bool RaycastUIPlane(const cVector3f& origin,
    const cVector3f& direction, const cVector3f& p00,
    const cVector3f& p10, const cVector3f& p01,
    float* xOut, float* yOut, float* tOut)
  {
    const cVector3f ex = (p10 - p00) / 800.0f;
    const cVector3f ey = (p01 - p00) / 600.0f;
    const cVector3f normal = cMath::Vector3Normalize(
      cMath::Vector3Cross(ex, ey));

    const float denom = cMath::Vector3Dot(normal, direction) * -1.0f;
    // Nearly parallel rays never meet the surface at a useful angle.
    if(cMath::Abs(denom) < 0.001f)
      return false;

    const cVector3f originOffset = origin - p00;
    const float distance = cMath::Vector3Dot(normal, originOffset) / denom;
    // Only surfaces ahead of the controller count as aiming at them; the
    // depth guard rejects gestures that graze the plane metres away.
    if(distance <= 0.0f || distance > 10.0f)
      return false;

    // Solve the plane hit against the surface axes. The Gram matrix is
    // orientation independent, so the flipped vertical axis of some UI
    // surfaces cannot corrupt the cursor position.
    const cVector3f planeHit = origin + direction * distance;
    const cVector3f offset = planeHit - p00;

    const float exLength2 = cMath::Vector3Dot(ex, ex);
    const float eyLength2 = cMath::Vector3Dot(ey, ey);
    const float exDotEy = cMath::Vector3Dot(ex, ey);
    const float determinant = exLength2 * eyLength2 - exDotEy * exDotEy;

    const float offsetDotEx = cMath::Vector3Dot(offset, ex);
    const float offsetDotEy = cMath::Vector3Dot(offset, ey);

    const float x = (eyLength2 * offsetDotEx - exDotEy * offsetDotEy) /
      determinant;
    const float y = (exLength2 * offsetDotEy - exDotEy * offsetDotEx) /
      determinant;

    if(xOut) *xOut = x;
    if(yOut) *yOut = y;
    if(tOut) *tOut = distance;
    return true;
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

    // Aim pose with grip fallback on either hand: the laser must originate
    // where the controller naturally points, not where its grip sits.
    if(DominantHand(game).IsPoseValid())
      handMatrix = ControllerAimMatrix(DominantHand(game));
    else if(OffHand(game).IsPoseValid())
    {
      handMatrix = ControllerAimMatrix(OffHand(game));
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

    float t = 0.0f;
    float x = 0.0f;
    float y = 0.0f;

    if(!RaycastUIPlane(handPos, handForward, p00, p10, p01, &x, &y, &t))
      return false;

    // Off-panel rays pin the cursor to the nearest edge instead of hiding
    // the laser, so pointing near the surface is enough to use it.
    x = cMath::Clamp(x, 0.0f, 800.0f);
    y = cMath::Clamp(y, 0.0f, 600.0f);

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
