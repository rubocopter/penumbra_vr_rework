#pragma once

#include "math/Math.h"
#include "openvr.h"

namespace hpl {
  class TrackedController {
  public:
    struct ButtonState {
ButtonState(bool valid = false)
        : touchX(0.0f), touchY(0.0f),
          touchContact(false),
          padPressed(false), padJustPressed(false), padJustReleased(false),
          gripPressed(false), gripJustPressed(false), gripJustReleased(false),
          triggerMargin(0.0f), triggerPressed(false), triggerJustPressed(false), triggerJustReleased(false),
          menuPressed(false), menuJustPressed(false), menuJustReleased(false),
          valid_(valid) {
      }

      float touchX;
      float touchY;

      bool touchContact;

      bool padPressed;
      bool padJustPressed;
      bool padJustReleased;

      bool gripPressed;
      bool gripJustPressed;
      bool gripJustReleased;

      float triggerMargin;

      bool triggerPressed;
      bool triggerJustPressed;
      bool triggerJustReleased;

      bool menuPressed;
      bool menuJustPressed;
      bool menuJustReleased;

      bool valid_;
    };

struct HandSummary {
      HandSummary()
        : grip(0.0f), trigger(0.0f), valid(false) {
        for (int i = 0; i < vr::VRFinger_Count; ++i) fingerCurl[i] = 0.0f;
      }

      float grip;
      float trigger;
      float fingerCurl[vr::VRFinger_Count];
      bool valid;
    };

    TrackedController();
    ~TrackedController();

    void SetMatrix(const cMatrixf& matrix);
    cMatrixf GetMatrix();

    void SetVelocity(const cVector3f& velocity);
    cVector3f GetVelocity();

    void SetAngularVelocity(const cVector3f& angular_velocity);
    cVector3f GetAngularVelocity();

    void BeginPoseFrame();
    void SetPose(const cMatrixf& matrix, const cVector3f& velocity,
      const cVector3f& angularVelocity, vr::TrackedDeviceIndex_t deviceIndex);
    void SetPoseValid(bool valid);
    bool IsPoseValid() const { return pose_valid_; }

    cMatrixf GetAimMatrix() const { return aim_matrix_; }
    void SetAimMatrix(const cMatrixf& matrix);

    void SetDeviceIndex(vr::TrackedDeviceIndex_t device_index);
    vr::TrackedDeviceIndex_t GetDeviceIndex() const { return device_index_; }

    void UpdateButtonState();
    ButtonState GetButtonState();

    // Per-hand skeletal summary. grip/trigger are normalized 0..1 and are
    // derived from the per-finger curls: trigger follows the index finger while
    // grip follows the remaining curl (middle, ring, pinky). The raw curls are
    // retained for future finger-level animation.
    void SetSkeletonSummary(const float fingerCurl[vr::VRFinger_Count]);
    void SetLegacySummary(float grip, float trigger);
    void InvalidateHandSummary();
    const HandSummary& GetHandSummary() const { return hand_summary_; }

  private:
    void InvalidateButtonState();

    ButtonState button_state_;
    vr::TrackedDeviceIndex_t device_index_;
    bool pose_valid_;
    cMatrixf matrix_;
    cMatrixf aim_matrix_;
    cVector3f velocity_;
    cVector3f angular_velocity_;
    HandSummary hand_summary_;
  };
}
