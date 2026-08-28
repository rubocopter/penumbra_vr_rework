// Unit tests for the pure VR math: cVRTrackingSpace (tracking-to-world
// boundary) and the stick dead-zone rescale. These run on every build via
// scripts/build.ps1; a non-zero exit code fails the build.
#include "game/VRTracking.h"
#include "input/VRAnalog.hpp"
#include "math/Math.h"
#include "../../PenumbraOverture/VRHandCollisionPolicy.h"

#include <cmath>
#include <cstdio>

using namespace hpl;

namespace {

int gChecks = 0;
int gFailures = 0;

void ExpectTrue(bool condition, const char* label)
{
    ++gChecks;
    if (!condition) {
        ++gFailures;
        std::printf("FAIL %s\n", label);
    }
}

void ExpectNear(float got, float wanted, float epsilon, const char* label)
{
    ++gChecks;
    if (!(std::fabs(got - wanted) <= epsilon)) {
        ++gFailures;
        std::printf("FAIL %s (got %.6f, wanted %.6f)\n", label, got, wanted);
    }
}

void ExpectVecNear(const cVector3f& got, const cVector3f& wanted, float epsilon, const char* label)
{
    ++gChecks;
    if (!(std::fabs(got.x - wanted.x) <= epsilon &&
          std::fabs(got.y - wanted.y) <= epsilon &&
          std::fabs(got.z - wanted.z) <= epsilon)) {
        ++gFailures;
        std::printf("FAIL %s (got %.4f %.4f %.4f, wanted %.4f %.4f %.4f)\n",
                    label, got.x, got.y, got.z, wanted.x, wanted.y, wanted.z);
    }
}

cMatrixf MakeHeadPose(float x, float y, float z, float yawDegrees)
{
    return cMath::MatrixMul(cMath::MatrixRotateY(cMath::ToRad(yawDegrees)),
                            cMath::MatrixTranslate(cVector3f(x, y, z)));
}

// ---------------------------------------------------------------------------

void TestDefaultStateIsIdentity()
{
    cVRTrackingSpace tracking;
    // The legacy floor adjustment ((y-0.2)*1.065) means an uncalibrated
    // tracking-space origin sits slightly below the world floor; lock that
    // constant down so it cannot drift unnoticed.
    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().y, -0.213f, 1e-4f,
               "default head world height applies legacy floor adjustment");
}

void TestHeightCompositionAndSidewaysDiscard()
{
    // The headset's horizontal position never moves the player; only its
    // height survives calibration, then the three height offsets add up.
    cVRTrackingSpace tracking;
    tracking.SetHeadTrackingPose(MakeHeadPose(0.3f, 1.0f, -0.2f, 0.0f));
    tracking.SetHeightCalibration(0.05f);
    tracking.SetPostureOffset(0.01f);
    tracking.SetSeatedOffset(0.02f);

    const float expectedY = (1.0f - 0.2f) * 1.065f + 0.05f + 0.01f + 0.02f;
    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().y, expectedY, 1e-4f,
               "calibrated height formula");
    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().x, 0.0f, 1e-5f,
               "headset sideways motion is discarded");
    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().z, 0.0f, 1e-5f,
               "headset forward motion is discarded");
}

void TestPlayerWorldPoseCarriesPosition()
{
    cVRTrackingSpace tracking;
    tracking.SetHeadTrackingPose(MakeHeadPose(0.3f, 1.0f, -0.2f, 0.0f));
    tracking.SetPlayerWorldPose(cMath::MatrixTranslate(cVector3f(5.0f, 0.0f, 3.0f)));

    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().x, 5.0f, 1e-4f,
               "player world pose drives X");
    ExpectNear(tracking.GetHeadWorldPose().GetTranslation().z, 3.0f, 1e-4f,
               "player world pose drives Z");
}

void TestYawRotatesDirections()
{
    // Facing -Z with a 90 degree room yaw must look along world +X... or -X
    // depending on convention; both are valid as long as the transform and
    // its direction helper agree, so compare against the same rotation the
    // class applies internally.
    cVRTrackingSpace tracking;
    tracking.SetWorldYaw(cMath::ToRad(90.0f));

    cVector3f trackedForward(0.0f, 0.0f, -1.0f);
    const cVector3f worldForward = tracking.TrackingDirectionToWorld(trackedForward);

    cVector3f worldForwardCopy = worldForward;
    ExpectNear(worldForwardCopy.Length(), 1.0f, 1e-4f, "yaw preserves length");
    ExpectNear(worldForward.y, 0.0f, 1e-4f, "yaw keeps directions horizontal");
    ExpectTrue(worldForward.x != 0.0f || worldForward.z != 0.0f, "yaw actually rotated");
}

void TestRecenterAlignsViewWithWorldForward()
{
    for (float yawDegrees = 30.0f; yawDegrees < 360.0f; yawDegrees += 70.0f) {
        cVRTrackingSpace tracking;
        tracking.SetHeadTrackingPose(MakeHeadPose(0.0f, 1.7f, 0.0f, yawDegrees));
        tracking.RecenterOrientation();

        cMatrixf inverseHead =
            cMath::MatrixInverse(tracking.GetHeadTrackingPose().GetRotation());
        cVector3f trackedForward = inverseHead.GetForward() * -1.0f;
        trackedForward.y = 0.0f;

        const cVector3f worldForward =
            tracking.TrackingDirectionToWorld(cMath::Vector3Normalize(trackedForward));
        const float alignment = cMath::Vector3Dot(
            cMath::Vector3Normalize(worldForward), cVector3f(0.0f, 0.0f, -1.0f));

        char label[128];
        std::snprintf(label, sizeof(label),
                      "recenter aligns view with world forward (head yaw %.0f)", yawDegrees);
        ExpectTrue(alignment > 0.999f, label);
    }
}

void TestTransformRoundTrip()
{
    cVRTrackingSpace tracking;
    tracking.SetHeadTrackingPose(MakeHeadPose(0.25f, 1.4f, -0.1f, 35.0f));
    tracking.SetWorldYaw(cMath::ToRad(140.0f));
    tracking.SetPlayerWorldPose(cMath::MatrixTranslate(cVector3f(-2.0f, 0.0f, 4.0f)));
    tracking.SetHeightCalibration(0.03f);

    const cMatrixf expected = cMath::MatrixMul(
        tracking.GetTrackingToWorldTransform(), tracking.GetHeadTrackingPose());
    ExpectVecNear(tracking.GetHeadWorldPose().GetTranslation(),
                  expected.GetTranslation(), 1e-4f,
                  "head world pose matches composed transform");
}

void TestWorldYawWraps()
{
    cVRTrackingSpace tracking;
    tracking.SetWorldYaw(cMath::ToRad(450.0f)); // 90 + full turn
    ExpectNear(tracking.GetWorldYaw(), cMath::ToRad(90.0f), 1e-3f,
               "world yaw wraps into [-pi, pi)");

    tracking.AddWorldYaw(cMath::ToRad(180.0f)); // 270 -> -90
    ExpectNear(tracking.GetWorldYaw(), cMath::ToRad(-90.0f), 1e-3f,
               "AddWorldYaw wraps too");
}

void TestDeadZone()
{
    const float dz = 0.15f;
    float x, y;

    x = 0.10f; y = 0.0f;
    ApplyStickDeadZone(x, y, dz);
    ExpectNear(x, 0.0f, 1e-6f, "dead zone zeroes small deflection");

    x = 1.0f; y = 0.0f;
    ApplyStickDeadZone(x, y, dz);
    ExpectNear(x, 1.0f, 1e-5f, "full deflection stays full");

    x = 0.575f; y = 0.0f; // midpoint of usable range for dz 0.15
    ApplyStickDeadZone(x, y, dz);
    ExpectNear(x, 0.5f, 1e-4f, "midpoint rescales to range midpoint");

    x = 0.6f; y = 0.6f;
    ApplyStickDeadZone(x, y, dz);
    const float diagonal = std::sqrt(x * x + y * y);
    ExpectNear(diagonal, (std::sqrt(0.72f) - dz) / (1.0f - dz), 1e-4f,
               "diagonal magnitude rescales");
    ExpectNear(x, y, 1e-5f, "diagonal keeps symmetry");

    x = 5.0f; y = 0.0f;
    ApplyStickDeadZone(x, y, dz);
    ExpectNear(x, 1.0f, 1e-5f, "oversized input clamps");

    x = 0.00005f; y = 0.0f;
    ApplyStickDeadZone(x, y, dz);
    ExpectNear(x, 0.0f, 1e-8f, "denormal guard");
}

float HandSlabPenetration(float handCenter, float handHalfExtent,
                          float slabMin, float slabMax)
{
    const float handMin = handCenter - handHalfExtent;
    const float handMax = handCenter + handHalfExtent;
    if (handMax <= slabMin || handMin >= slabMax) return 0.0f;
    return cMath::Min(handMax - slabMin, slabMax - handMin);
}

float ResolveHandAgainstSlab(float start, float goal,
                             float slabMin, float slabMax,
                             bool interactionAssist)
{
    const float halfExtent =
        VRHandCollisionPolicy::kCollisionSizeY * 0.5f;
    const float delta = goal - start;
    const int steps = VRHandCollisionPolicy::SweepStepCount(std::fabs(delta));
    float safeT = 0.0f;
    float hitT = 1.0f;
    bool hit = false;

    for (int step = 1; step <= steps; ++step) {
        const float t = (float)step / (float)steps;
        const float depth = HandSlabPenetration(
            start + delta * t, halfExtent, slabMin, slabMax);
        if (VRHandCollisionPolicy::IsBlockingPenetration(
                depth, interactionAssist)) {
            hit = true;
            hitT = t;
            break;
        }
        safeT = t;
    }

    if (!hit) return goal;
    for (int refine = 0;
         refine < VRHandCollisionPolicy::kSweepRefineIterations; ++refine) {
        const float midT = (safeT + hitT) * 0.5f;
        const float depth = HandSlabPenetration(
            start + delta * midT, halfExtent, slabMin, slabMax);
        if (VRHandCollisionPolicy::IsBlockingPenetration(
                depth, interactionAssist))
            hitT = midT;
        else
            safeT = midT;
    }
    return start + delta * safeT;
}

float RotatedHandHalfExtent(float angleRadians)
{
    const float thinHalfExtent =
        VRHandCollisionPolicy::kCollisionSizeY * 0.5f;
    const float longHalfExtent =
        VRHandCollisionPolicy::kCollisionSizeX * 0.5f;
    return std::fabs(std::cos(angleRadians)) * thinHalfExtent +
           std::fabs(std::sin(angleRadians)) * longHalfExtent;
}

float ResolveHandRotationAgainstPlane(float startAngle, float goalAngle,
                                      float handCenter,
                                      bool interactionAssist)
{
    const float delta = goalAngle - startAngle;
    const int steps = (int)(std::fabs(delta) /
        VRHandCollisionPolicy::kRotationStepRadians) + 1;
    float safeT = 0.0f;
    float hitT = 1.0f;
    bool hit = false;

    for (int step = 1; step <= steps; ++step) {
        const float t = (float)step / (float)steps;
        const float penetration = cMath::Max(0.0f,
            handCenter + RotatedHandHalfExtent(startAngle + delta * t));
        if (VRHandCollisionPolicy::IsBlockingPenetration(
                penetration, interactionAssist)) {
            hit = true;
            hitT = t;
            break;
        }
        safeT = t;
    }

    if (!hit) return goalAngle;
    for (int refine = 0;
         refine < VRHandCollisionPolicy::kRotationRefineIterations; ++refine) {
        const float midT = (safeT + hitT) * 0.5f;
        const float penetration = cMath::Max(0.0f,
            handCenter + RotatedHandHalfExtent(startAngle + delta * midT));
        if (VRHandCollisionPolicy::IsBlockingPenetration(
                penetration, interactionAssist))
            hitT = midT;
        else
            safeT = midT;
    }
    return startAngle + delta * safeT;
}

void TestVRHandCollisionPolicyKeepsFullPalm()
{
    ExpectNear(VRHandCollisionPolicy::kCollisionSizeX, 0.190f, 1e-6f,
               "VR hand physical width is not reduced");
    ExpectNear(VRHandCollisionPolicy::kCollisionSizeY, 0.052f, 1e-6f,
               "VR hand physical depth is not reduced");
    ExpectNear(VRHandCollisionPolicy::kCollisionSizeZ, 0.125f, 1e-6f,
               "VR hand physical height is not reduced");
    ExpectTrue(VRHandCollisionPolicy::kInteractionContactTolerance <
                   VRHandCollisionPolicy::kCollisionSizeY * 0.20f,
               "interaction skin stays below twenty percent of palm depth");
    ExpectTrue(VRHandCollisionPolicy::kSweepStep <
                   VRHandCollisionPolicy::kCollisionSizeY -
                       2.0f * VRHandCollisionPolicy::kInteractionContactTolerance,
               "sweep step cannot skip the thinnest blocking palm span");
    ExpectTrue(VRHandCollisionPolicy::kMaxTrackedHandDistanceFromHead >= 2.0f,
               "tracking sanity radius permits a fully extended arm");
    ExpectTrue(VRHandCollisionPolicy::SweepStepCount(
                   VRHandCollisionPolicy::kMaxTrackedHandDistanceFromHead) <= 126,
               "a plausible tracking sample has bounded sweep work");
    ExpectTrue(VRHandCollisionPolicy::kConstrainedHandDistance >
                   VRHandCollisionPolicy::kCollisionSizeY,
               "stuck recovery does not replace ordinary palm contact");
}

void TestVRHandCannotCrossWall()
{
    const float wallNear = 0.0f;
    const float wallFar = 0.25f;
    float resolved = -0.20f;

    // Sustained controller pressure used to trigger the 35 cm visual-lag
    // fallback and eventually drag the rendered hand through the wall.
    for (int frame = 0; frame < 240; ++frame) {
        resolved = ResolveHandAgainstSlab(
            resolved, 1.25f, wallNear, wallFar, true);
        ExpectTrue(resolved < wallNear,
                   "sustained hand pressure remains on near side of wall");
    }
}

void TestVRHandCannotCrossClosedDoorAtSpeed()
{
    const float doorNear = 0.0f;
    const float doorFar = 0.040f;
    const float resolved = ResolveHandAgainstSlab(
        -0.30f, 1.50f, doorNear, doorFar, true);
    ExpectTrue(resolved < doorNear,
               "fast controller sweep remains on near side of closed door");
    ExpectTrue(resolved +
                   VRHandCollisionPolicy::kCollisionSizeY * 0.5f <=
                   doorNear +
                       VRHandCollisionPolicy::kInteractionContactTolerance +
                       0.0005f,
               "closed door penetration is bounded by interaction skin");
}

void TestVRHandInteractionSkinImprovesSurfaceContact()
{
    // At this goal the full-size palm overlaps the drawer front by 6 mm:
    // ordinary collision rejects it, while interaction-aware contact accepts
    // it because the target is already valid and the 8 mm skin stays bounded.
    const float start = -0.20f;
    const float sixMillimetreContact = -0.020f;
    const float normal = ResolveHandAgainstSlab(
        start, sixMillimetreContact, 0.0f, 0.25f, false);
    const float assisted = ResolveHandAgainstSlab(
        start, sixMillimetreContact, 0.0f, 0.25f, true);

    ExpectTrue(normal < sixMillimetreContact - 0.002f,
               "ordinary contact retains the normal collision skin");
    ExpectNear(assisted, sixMillimetreContact, 1e-5f,
               "valid interaction target permits bounded soft contact");
}

void TestVRHandSweepIsFrameRateIndependent()
{
    const float start = -0.20f;
    const float goal = 1.25f;
    const float oneFrame = ResolveHandAgainstSlab(
        start, goal, 0.0f, 0.25f, true);

    float manyFrames = start;
    for (int frame = 1; frame <= 90; ++frame) {
        const float frameGoal = start + (goal - start) *
            ((float)frame / 90.0f);
        manyFrames = ResolveHandAgainstSlab(
            manyFrames, frameGoal, 0.0f, 0.25f, true);
    }
    ExpectNear(manyFrames, oneFrame, 0.001f,
               "hand sweep result is stable across frame subdivision");
}

void TestVRHandCollisionIsDirectionSymmetric()
{
    const float fromNegative = ResolveHandAgainstSlab(
        -0.20f, 1.25f, 0.0f, 0.25f, true);
    const float fromPositive = ResolveHandAgainstSlab(
        0.20f, -1.25f, -0.25f, 0.0f, true);
    ExpectTrue(fromNegative < 0.0f && fromPositive > 0.0f,
               "both approach directions remain on their near side");
    ExpectNear(fromNegative, -fromPositive, 0.001f,
               "left/right collision policy is symmetric");
}

void TestVRHandRotationCannotExpandThroughSurface()
{
    const float handCenter = -0.030f;
    const float resolvedAngle = ResolveHandRotationAgainstPlane(
        0.0f, cMath::ToRad(90.0f), handCenter, true);
    const float penetration = cMath::Max(0.0f,
        handCenter + RotatedHandHalfExtent(resolvedAngle));

    ExpectTrue(resolvedAngle < cMath::ToRad(90.0f),
               "contact rotation stops before the long palm axis crosses surface");
    ExpectTrue(penetration <=
                   VRHandCollisionPolicy::kInteractionContactTolerance + 0.0005f,
               "rotation penetration remains inside interaction skin");
}

void TestVRHandInitialCeilingOverlapRecoversBySweep()
{
    // The tracked centre begins inside an 8 cm ceiling slab after loading.
    // Starting from a body-side anchor below it must stop on the room side;
    // directly depenetrating the raw sample could choose the far side.
    const float bodySideAnchor = -0.30f;
    const float rawInsideCeiling = 0.025f;
    const float resolved = ResolveHandAgainstSlab(
        bodySideAnchor, rawInsideCeiling, 0.0f, 0.080f, false);

    ExpectTrue(resolved < 0.0f,
               "initial ceiling overlap recovers on the player side");
    ExpectTrue(resolved +
                   VRHandCollisionPolicy::kCollisionSizeY * 0.5f <=
                   VRHandCollisionPolicy::kContactTolerance + 0.0005f,
               "initial ceiling recovery keeps full-palm penetration bounded");
}

void TestVRHandRecoveryCannotCrossClosedDoor()
{
    const float bodySideAnchor = -0.30f;
    const float rawBeyondDoor = 0.20f;
    const float resolved = ResolveHandAgainstSlab(
        bodySideAnchor, rawBeyondDoor, 0.0f, 0.040f, true);

    ExpectTrue(resolved < 0.0f,
               "body-side recovery cannot teleport through a closed door");
    ExpectTrue(resolved +
                   VRHandCollisionPolicy::kCollisionSizeY * 0.5f <=
                   VRHandCollisionPolicy::kInteractionContactTolerance +
                       0.0005f,
               "recovery sweep preserves the closed-door contact bound");
}

void TestVRHandHingeRecoveryRequiresPullback()
{
    const int ready = VRHandCollisionPolicy::kConstrainedRecoveryFrames;
    ExpectTrue(!VRHandCollisionPolicy::ShouldUseRecoveryAnchor(
                   ready - 1, 0.60f, 0.70f),
               "hinge recovery waits for sustained constraint");
    ExpectTrue(!VRHandCollisionPolicy::ShouldUseRecoveryAnchor(
                   ready, 0.75f, 0.70f),
               "pushing farther into a hinge does not trigger a snap");
    ExpectTrue(VRHandCollisionPolicy::ShouldUseRecoveryAnchor(
                   ready, 0.60f, 0.70f),
               "pulling a constrained hand toward the body triggers recovery");

    const float withdrawnRaw = -0.10f;
    const float recovered = ResolveHandAgainstSlab(
        -0.30f, withdrawnRaw, 0.0f, 0.040f, false);
    ExpectNear(recovered, withdrawnRaw, 1e-5f,
               "hinge pullback returns to the controller on the player side");
}

} // namespace

int main()
{
    TestDefaultStateIsIdentity();
    TestHeightCompositionAndSidewaysDiscard();
    TestPlayerWorldPoseCarriesPosition();
    TestYawRotatesDirections();
    TestRecenterAlignsViewWithWorldForward();
    TestTransformRoundTrip();
    TestWorldYawWraps();
    TestDeadZone();
    TestVRHandCollisionPolicyKeepsFullPalm();
    TestVRHandCannotCrossWall();
    TestVRHandCannotCrossClosedDoorAtSpeed();
    TestVRHandInteractionSkinImprovesSurfaceContact();
    TestVRHandSweepIsFrameRateIndependent();
    TestVRHandCollisionIsDirectionSymmetric();
    TestVRHandRotationCannotExpandThroughSurface();
    TestVRHandInitialCeilingOverlapRecoversBySweep();
    TestVRHandRecoveryCannotCrossClosedDoor();
    TestVRHandHingeRecoveryRequiresPullback();

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
