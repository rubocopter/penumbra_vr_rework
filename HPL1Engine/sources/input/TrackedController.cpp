#include "input/TrackedController.h"
#include "game\Game.h"

using namespace hpl;
using namespace vr;

namespace hpl {
  extern cGame* gGame;
}

TrackedController::TrackedController()
  : button_state_(false), device_index_(vr::k_unTrackedDeviceIndexInvalid),
    pose_valid_(false), matrix_(cMatrixf::Identity),
    aim_matrix_(cMatrixf::Identity), aim_valid_(false),
    velocity_(0.0f), angular_velocity_(0.0f) {
}

TrackedController::~TrackedController() {
}

void TrackedController::SetMatrix(const cMatrixf& matrix) {
  matrix_ = matrix;
}

cMatrixf TrackedController::GetMatrix() {
  return matrix_;
}

void TrackedController::SetAimMatrix(const cMatrixf& matrix) {
  aim_matrix_ = matrix;
}

void TrackedController::SetVelocity(const cVector3f& velocity) {
  velocity_ = velocity;
}

cVector3f TrackedController::GetVelocity() {
  return velocity_;
}
void TrackedController::SetAngularVelocity(const cVector3f& angular_velocity) {
  angular_velocity_ = angular_velocity;
}

cVector3f TrackedController::GetAngularVelocity() {
  return angular_velocity_;
}

void TrackedController::BeginPoseFrame() {
  pose_valid_ = false;
  device_index_ = vr::k_unTrackedDeviceIndexInvalid;
  velocity_ = cVector3f(0.0f);
  angular_velocity_ = cVector3f(0.0f);
  hand_summary_.valid = false;
  hand_summary_.grip = 0.0f;
  hand_summary_.trigger = 0.0f;
  for (int i = 0; i < vr::VRFinger_Count; ++i) hand_summary_.fingerCurl[i] = 0.0f;
}

void TrackedController::SetPose(const cMatrixf& matrix, const cVector3f& velocity,
  const cVector3f& angularVelocity, vr::TrackedDeviceIndex_t deviceIndex) {
  SetMatrix(matrix);
  SetVelocity(velocity);
  SetAngularVelocity(angularVelocity);
  SetDeviceIndex(deviceIndex);
  pose_valid_ = true;
}

void TrackedController::SetPoseValid(bool valid) {
  pose_valid_ = valid;
  if (!valid) {
    device_index_ = vr::k_unTrackedDeviceIndexInvalid;
    velocity_ = cVector3f(0.0f);
    angular_velocity_ = cVector3f(0.0f);
  }
}

void TrackedController::SetDeviceIndex(vr::TrackedDeviceIndex_t index) {
  device_index_ = index;
}

void TrackedController::UpdateButtonState() {
  if (device_index_ == vr::k_unTrackedDeviceIndexInvalid) {
    InvalidateButtonState();
    return;
  }

  auto hmd = gGame->vr_hmd;

  vr::VRControllerState_t state;

  if (!hmd->GetControllerState(device_index_, &state, sizeof(state))) {
    InvalidateButtonState();
    return;
  }

  button_state_.valid_ = true;

  // Grip button
  button_state_.gripJustPressed = false;
  button_state_.gripJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_Grip)))
    button_state_.gripJustPressed = !button_state_.gripPressed;
  else
    button_state_.gripJustReleased = button_state_.gripPressed;

  button_state_.gripPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_Grip))) > 0;

  // Pad button
  button_state_.padJustPressed = false;
  button_state_.padJustReleased = false;

  if ((state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0)
    button_state_.padJustPressed = !button_state_.padPressed;
  else
    button_state_.padJustReleased = button_state_.padPressed;

  button_state_.padPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0;

  // Pad touched
  button_state_.touchContact = (state.ulButtonTouched & (1ULL << ((int)k_EButton_SteamVR_Touchpad))) > 0;

  // Touchpad coordinates
  button_state_.touchX = state.rAxis[0].x;
  button_state_.touchY = state.rAxis[0].y;

  // Trigger button
  button_state_.triggerJustPressed = false;
  button_state_.triggerJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Trigger)))
    button_state_.triggerJustPressed = !button_state_.triggerPressed;
  else
    button_state_.triggerJustReleased = button_state_.triggerPressed;

  button_state_.triggerPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_SteamVR_Trigger))) > 0;

  // Trigger margin
  button_state_.triggerMargin = state.rAxis[1].x;

  // Menu button
  button_state_.menuJustPressed = false;
  button_state_.menuJustReleased = false;

  if (state.ulButtonPressed & (1ULL << ((int)k_EButton_ApplicationMenu)))
    button_state_.menuJustPressed = !button_state_.menuPressed;
  else
    button_state_.menuJustReleased = button_state_.menuPressed;

  button_state_.menuPressed = (state.ulButtonPressed & (1ULL << ((int)k_EButton_ApplicationMenu))) > 0;
}

void TrackedController::InvalidateButtonState() {
  ButtonState disconnected(false);
  disconnected.padJustReleased = button_state_.padPressed;
  disconnected.gripJustReleased = button_state_.gripPressed;
  disconnected.triggerJustReleased = button_state_.triggerPressed;
  disconnected.menuJustReleased = button_state_.menuPressed;
button_state_ = disconnected;
}

TrackedController::ButtonState TrackedController::GetButtonState() {
  return button_state_;
}

void TrackedController::SetSkeletonSummary(const float fingerCurl[vr::VRFinger_Count]) {
  hand_summary_.valid = true;
  hand_summary_.grip = 0.0f;
  hand_summary_.trigger = 0.0f;

  for (int i = 0; i < vr::VRFinger_Count; ++i) {
    float value = fingerCurl[i];
    if (value < 0.0f) value = 0.0f;
    else if (value > 1.0f) value = 1.0f;
    hand_summary_.fingerCurl[i] = value;

    if (i == vr::VRFinger_Index) {
      hand_summary_.trigger = value;
    }
    else if (i != vr::VRFinger_Thumb) {
      if (value > hand_summary_.grip) hand_summary_.grip = value;
    }
  }
}
void TrackedController::SetLegacySummary(float grip, float trigger) {
  hand_summary_.valid = true;
  if (grip < 0.0f) grip = 0.0f;
  else if (grip > 1.0f) grip = 1.0f;
  if (trigger < 0.0f) trigger = 0.0f;
  else if (trigger > 1.0f) trigger = 1.0f;
  hand_summary_.grip = grip;
  hand_summary_.trigger = trigger;
  for (int i = 0; i < vr::VRFinger_Count; ++i) hand_summary_.fingerCurl[i] = 0.0f;
}

void TrackedController::InvalidateHandSummary() {
  hand_summary_.valid = false;
  hand_summary_.grip = 0.0f;
  hand_summary_.trigger = 0.0f;
  for (int i = 0; i < vr::VRFinger_Count; ++i) hand_summary_.fingerCurl[i] = 0.0f;
}
