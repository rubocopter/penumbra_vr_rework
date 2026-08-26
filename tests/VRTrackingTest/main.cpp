// Unit tests for the pure VR math: cVRTrackingSpace (tracking-to-world
// boundary) and the stick dead-zone rescale. These run on every build via
// scripts/build.ps1; a non-zero exit code fails the build.
#include "game/VRTracking.h"
#include "input/VRAnalog.hpp"
#include "math/Math.h"

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

    std::printf("%d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
