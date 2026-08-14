#include "input/SteamVRInput.h"

#include "system/LowLevelSystem.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>
#include <math.h>

namespace hpl {

  cSteamVRInput::cSteamVRInput()
    : mbAvailable(false), mbUsingActions(false), mbFallbackReported(false), mbHasActiveContext(false),
      mbLeftPoseStateKnown(false), mbRightPoseStateKnown(false),
      mbLeftPoseWasValid(false), mbRightPoseWasValid(false),
      mActiveContext(eSteamVRInputContext_Gameplay), mfMoveDeadZone(0.15f),
      mGlobalActionSet(vr::k_ulInvalidActionSetHandle),
      mGameplayActionSet(vr::k_ulInvalidActionSetHandle),
      mUIActionSet(vr::k_ulInvalidActionSetHandle),
      mMoveAction(vr::k_ulInvalidActionHandle),
      mTurnAction(vr::k_ulInvalidActionHandle),
      mSprintAction(vr::k_ulInvalidActionHandle),
      mInteractAction(vr::k_ulInvalidActionHandle),
      mExamineAction(vr::k_ulInvalidActionHandle),
      mHolsterAction(vr::k_ulInvalidActionHandle),
      mInventoryAction(vr::k_ulInvalidActionHandle),
      mNotebookAction(vr::k_ulInvalidActionHandle),
      mQuickLightAction(vr::k_ulInvalidActionHandle),
      mJumpAction(vr::k_ulInvalidActionHandle),
      mCrouchAction(vr::k_ulInvalidActionHandle),
      mPauseAction(vr::k_ulInvalidActionHandle),
      mRecenterAction(vr::k_ulInvalidActionHandle),
      mUISelectAction(vr::k_ulInvalidActionHandle),
      mUIDragAction(vr::k_ulInvalidActionHandle),
      mUIBackAction(vr::k_ulInvalidActionHandle),
      mUICloseAction(vr::k_ulInvalidActionHandle),
      mLeftPoseAction(vr::k_ulInvalidActionHandle),
      mRightPoseAction(vr::k_ulInvalidActionHandle),
      mLeftHapticAction(vr::k_ulInvalidActionHandle),
      mRightHapticAction(vr::k_ulInvalidActionHandle) {
  }

  bool cSteamVRInput::Initialize() {
    msManifestPath = FindManifestPath();
    if (msManifestPath.empty()) {
      Log(" SteamVR Input disabled: could not resolve vr/actions.json next to the executable.\n");
      return false;
    }

    vr::EVRInputError error = vr::VRInput()->SetActionManifestPath(msManifestPath.c_str());
    if (error != vr::VRInputError_None) {
      Log(" SteamVR Input disabled: SetActionManifestPath failed with error %d for '%s'.\n",
          (int)error, msManifestPath.c_str());
      return false;
    }

    bool resolved = true;
    resolved = ResolveActionSet("/actions/global", mGlobalActionSet) && resolved;
    resolved = ResolveActionSet("/actions/gameplay", mGameplayActionSet) && resolved;
    resolved = ResolveActionSet("/actions/ui", mUIActionSet) && resolved;

    resolved = ResolveAction("/actions/gameplay/in/move", mMoveAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/turn", mTurnAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/sprint", mSprintAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/interact", mInteractAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/examine", mExamineAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/holster", mHolsterAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/inventory", mInventoryAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/notebook", mNotebookAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/quick_light", mQuickLightAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/jump", mJumpAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/crouch", mCrouchAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/pause", mPauseAction) && resolved;
    resolved = ResolveAction("/actions/global/in/recenter", mRecenterAction) && resolved;

    resolved = ResolveAction("/actions/ui/in/select", mUISelectAction) && resolved;
    resolved = ResolveAction("/actions/ui/in/drag", mUIDragAction) && resolved;
    resolved = ResolveAction("/actions/ui/in/back", mUIBackAction) && resolved;
    resolved = ResolveAction("/actions/ui/in/close", mUICloseAction) && resolved;

    resolved = ResolveAction("/actions/global/in/left_pose", mLeftPoseAction) && resolved;
    resolved = ResolveAction("/actions/global/in/right_pose", mRightPoseAction) && resolved;
    resolved = ResolveAction("/actions/global/out/left_haptic", mLeftHapticAction) && resolved;
    resolved = ResolveAction("/actions/global/out/right_haptic", mRightHapticAction) && resolved;

    if (!resolved) {
      Log(" SteamVR Input disabled: the action manifest is incomplete. Falling back to legacy controller polling.\n");
      return false;
    }

    mbAvailable = true;
    Log(" SteamVR Input initialized with '%s'.\n", msManifestPath.c_str());
    return true;
  }

  bool cSteamVRInput::Update(eSteamVRInputContext context, TrackedController& leftHand, TrackedController& rightHand) {
    const cVRInputState previousState = mState;
    mState = cVRInputState();
    if (!mbAvailable) {
      return false;
    }

    // SteamVR reports a held physical control as changed when the newly active
    // action set binds that control to another action. Without latching the
    // first state, R1/Options can open a UI and immediately close it again.
    const bool suppressContextEdges = !mbUsingActions || !mbHasActiveContext || context != mActiveContext;

    vr::VRActiveActionSet_t activeSets[2];
    memset(activeSets, 0, sizeof(activeSets));
    activeSets[0].ulActionSet = mGlobalActionSet;
    activeSets[1].ulActionSet = context == eSteamVRInputContext_UI ? mUIActionSet : mGameplayActionSet;

    vr::EVRInputError error = vr::VRInput()->UpdateActionState(activeSets, sizeof(vr::VRActiveActionSet_t), 2);
    if (error != vr::VRInputError_None) {
      if (!mbFallbackReported) {
        Log(" SteamVR Input update failed with error %d. Falling back to legacy controller polling.\n", (int)error);
        mbFallbackReported = true;
      }
      mbUsingActions = false;
      return false;
    }

    // Pose actions follow their bound hand rather than a transient tracked-device
    // index. A reconnect can assign a different index, so action origins are the
    // authoritative controller identity whenever the pose action is active.
    UpdatePoseAction(mLeftPoseAction, leftHand, "left", mbLeftPoseStateKnown, mbLeftPoseWasValid);
    UpdatePoseAction(mRightPoseAction, rightHand, "right", mbRightPoseStateKnown, mbRightPoseWasValid);

    cVRInputState nextState;
    nextState.source = eVRInputSource_SteamVRActions;
    nextState.context = context;
    nextState.recenter = ReadDigital(mRecenterAction);
    if (suppressContextEdges) SuppressDigitalEdges(nextState.recenter);
    bool anyActionActive = false;

    if (context == eSteamVRInputContext_Gameplay) {
      AnalogState move = ReadAnalog(mMoveAction);
      ApplyMoveDeadZone(move.x, move.y);
      nextState.moveX = move.x;
      nextState.moveY = move.y;
      nextState.moveActive = move.active;
      anyActionActive = move.active;

      AnalogState turn = ReadAnalog(mTurnAction);
      nextState.turnX = turn.x;
      nextState.turnActive = turn.active;
      anyActionActive = anyActionActive || turn.active;

      nextState.sprint = ReadDigital(mSprintAction);
      nextState.interact = ReadDigital(mInteractAction);
      nextState.examine = ReadDigital(mExamineAction);
      nextState.holster = ReadDigital(mHolsterAction);
      nextState.inventory = ReadDigital(mInventoryAction);
      nextState.notebook = ReadDigital(mNotebookAction);
      nextState.quickLight = ReadDigital(mQuickLightAction);
      nextState.jump = ReadDigital(mJumpAction);
      nextState.crouch = ReadDigital(mCrouchAction);
      nextState.pause = ReadDigital(mPauseAction);

      // World interaction needs a current pointing pose. Preserve non-spatial
      // controls such as pause/inventory, but release an in-progress interact
      // cleanly if tracking disappears.
      if (!rightHand.IsPoseValid()) {
        nextState.interact.pressed = false;
        nextState.interact.justPressed = false;
        nextState.interact.justReleased = previousState.interact.pressed;
      }

      if (suppressContextEdges) {
        SuppressDigitalEdges(nextState.sprint);
        SuppressDigitalEdges(nextState.interact);
        SuppressDigitalEdges(nextState.examine);
        SuppressDigitalEdges(nextState.holster);
        SuppressDigitalEdges(nextState.inventory);
        SuppressDigitalEdges(nextState.notebook);
        SuppressDigitalEdges(nextState.quickLight);
        SuppressDigitalEdges(nextState.jump);
        SuppressDigitalEdges(nextState.crouch);
        SuppressDigitalEdges(nextState.pause);
      }

      if (nextState.interact.justPressed) Log(" [VR input +%lu ms] action: interact pressed.\n", GetApplicationTime());
      if (nextState.interact.justReleased) Log(" [VR input +%lu ms] action: interact released.\n", GetApplicationTime());
      if (nextState.inventory.justPressed) Log(" [VR input +%lu ms] action: inventory pressed.\n", GetApplicationTime());
      if (nextState.holster.justPressed) Log(" [VR input +%lu ms] action: holster pressed.\n", GetApplicationTime());
      if (nextState.crouch.justPressed) Log(" [VR input +%lu ms] action: crouch pressed.\n", GetApplicationTime());

      anyActionActive = anyActionActive || nextState.sprint.active || nextState.interact.active ||
        nextState.examine.active || nextState.holster.active || nextState.inventory.active ||
        nextState.notebook.active || nextState.quickLight.active || nextState.jump.active ||
        nextState.crouch.active || nextState.pause.active;
    }
    else {
      nextState.uiSelect = ReadDigital(mUISelectAction);
      nextState.uiDrag = ReadDigital(mUIDragAction);
      nextState.uiBack = ReadDigital(mUIBackAction);
      nextState.uiClose = ReadDigital(mUICloseAction);

      // Selection and dragging are pointer operations. Closing or backing out
      // remains available even if optical hand tracking is temporarily lost.
      if (!rightHand.IsPoseValid()) {
        nextState.uiSelect.pressed = false;
        nextState.uiSelect.justPressed = false;
        nextState.uiSelect.justReleased = previousState.uiSelect.pressed;
        nextState.uiDrag.pressed = false;
        nextState.uiDrag.justPressed = false;
        nextState.uiDrag.justReleased = previousState.uiDrag.pressed;
      }

      if (suppressContextEdges) {
        SuppressDigitalEdges(nextState.uiSelect);
        SuppressDigitalEdges(nextState.uiDrag);
        SuppressDigitalEdges(nextState.uiBack);
        SuppressDigitalEdges(nextState.uiClose);
      }

      if (nextState.uiSelect.justPressed) Log(" [VR input +%lu ms] action: UI select pressed.\n", GetApplicationTime());
      if (nextState.uiClose.justPressed) Log(" [VR input +%lu ms] action: UI close pressed.\n", GetApplicationTime());

      anyActionActive = nextState.uiSelect.active || nextState.uiDrag.active ||
        nextState.uiBack.active || nextState.uiClose.active;
    }

    if (!anyActionActive) {
      if (!mbFallbackReported) {
        Log(" SteamVR Input actions became inactive. Falling back to legacy controller polling.\n");
        mbFallbackReported = true;
      }
      mbUsingActions = false;
      return false;
    }

    if (!mbUsingActions) {
      Log(" SteamVR Input actions are active.\n");
    }
    mbUsingActions = true;
    mbFallbackReported = false;
    mbHasActiveContext = true;
    mActiveContext = context;
    mState = nextState;
    return true;
  }

  void cSteamVRInput::UpdateLegacyState(eSteamVRInputContext context,
    const TrackedController::ButtonState& leftState,
    const TrackedController::ButtonState& rightState) {
    cVRInputState legacyState;
    legacyState.source = eVRInputSource_LegacyOpenVR;
    legacyState.context = context;

    if (context == eSteamVRInputContext_Gameplay) {
      legacyState.moveActive = leftState.touchContact;
      legacyState.moveX = leftState.touchX;
      legacyState.moveY = leftState.touchY;
      ApplyMoveDeadZone(legacyState.moveX, legacyState.moveY);

      legacyState.sprint.active = rightState.valid_;
      legacyState.sprint.pressed = rightState.padPressed;
      legacyState.sprint.justPressed = rightState.padJustPressed;
      legacyState.sprint.justReleased = rightState.padJustReleased;

      legacyState.interact.active = rightState.valid_;
      legacyState.interact.pressed = rightState.triggerPressed;
      legacyState.interact.justPressed = rightState.triggerJustPressed;
      legacyState.interact.justReleased = rightState.triggerJustReleased;

      legacyState.examine.active = rightState.valid_;
      legacyState.examine.pressed = rightState.menuPressed;
      legacyState.examine.justPressed = rightState.menuJustPressed;
      legacyState.examine.justReleased = rightState.menuJustReleased;

      legacyState.inventory.active = rightState.valid_;
      legacyState.inventory.pressed = rightState.gripPressed;
      legacyState.inventory.justPressed = rightState.gripJustPressed;
      legacyState.inventory.justReleased = rightState.gripJustReleased;

      legacyState.notebook.active = leftState.valid_;
      legacyState.notebook.pressed = leftState.menuPressed;
      legacyState.notebook.justPressed = leftState.menuJustPressed;
      legacyState.notebook.justReleased = leftState.menuJustReleased;

      legacyState.quickLight.active = leftState.valid_;
      legacyState.quickLight.pressed = leftState.gripPressed;
      legacyState.quickLight.justPressed = leftState.gripJustPressed;
      legacyState.quickLight.justReleased = leftState.gripJustReleased;

      legacyState.jump.active = leftState.valid_;
      legacyState.jump.pressed = leftState.triggerPressed;
      legacyState.jump.justPressed = leftState.triggerJustPressed;
      legacyState.jump.justReleased = leftState.triggerJustReleased;
    }
    else {
      legacyState.uiSelect.active = rightState.valid_;
      legacyState.uiSelect.pressed = rightState.triggerPressed;
      legacyState.uiSelect.justPressed = rightState.triggerJustPressed;
      legacyState.uiSelect.justReleased = rightState.triggerJustReleased;

      legacyState.uiDrag.active = rightState.valid_;
      legacyState.uiDrag.pressed = rightState.padPressed;
      legacyState.uiDrag.justPressed = rightState.padJustPressed;
      legacyState.uiDrag.justReleased = rightState.padJustReleased;

      legacyState.uiBack.active = rightState.valid_;
      legacyState.uiBack.pressed = rightState.menuPressed;
      legacyState.uiBack.justPressed = rightState.menuJustPressed;
      legacyState.uiBack.justReleased = rightState.menuJustReleased;

      legacyState.uiClose.active = rightState.valid_ || leftState.valid_;
      legacyState.uiClose.pressed = rightState.gripPressed || leftState.menuPressed;
      legacyState.uiClose.justPressed = rightState.gripJustPressed || leftState.menuJustPressed;
      legacyState.uiClose.justReleased = !legacyState.uiClose.pressed &&
        (rightState.gripJustReleased || leftState.menuJustReleased);
    }

    mState = legacyState;
  }

  bool cSteamVRInput::TriggerHaptic(eSteamVRHand hand, float durationSeconds, float frequency, float amplitude) {
    if (!mbAvailable) {
      return false;
    }

    vr::VRActionHandle_t action = hand == eSteamVRHand_Left ? mLeftHapticAction : mRightHapticAction;
    vr::EVRInputError error = vr::VRInput()->TriggerHapticVibrationAction(
      action, 0.0f, durationSeconds, frequency, amplitude, vr::k_ulInvalidInputValueHandle);
    return error == vr::VRInputError_None;
  }

  void cSteamVRInput::SetMoveDeadZone(float deadZone) {
    if (deadZone < 0.0f) deadZone = 0.0f;
    if (deadZone > 0.9f) deadZone = 0.9f;
    mfMoveDeadZone = deadZone;
  }

  bool cSteamVRInput::ResolveActionSet(const char* path, vr::VRActionSetHandle_t& handle) {
    vr::EVRInputError error = vr::VRInput()->GetActionSetHandle(path, &handle);
    if (error != vr::VRInputError_None) {
      Log(" SteamVR Input could not resolve action set '%s' (error %d).\n", path, (int)error);
      return false;
    }
    return true;
  }

  bool cSteamVRInput::ResolveAction(const char* path, vr::VRActionHandle_t& handle) {
    vr::EVRInputError error = vr::VRInput()->GetActionHandle(path, &handle);
    if (error != vr::VRInputError_None) {
      Log(" SteamVR Input could not resolve action '%s' (error %d).\n", path, (int)error);
      return false;
    }
    return true;
  }

  bool cSteamVRInput::UpdatePoseAction(vr::VRActionHandle_t handle, TrackedController& hand,
    const char* handName, bool& stateKnown, bool& wasValid) {
    vr::InputPoseActionData_t data;
    memset(&data, 0, sizeof(data));

    const vr::ETrackingUniverseOrigin trackingSpace = vr::VRCompositor()->GetTrackingSpace();
    vr::EVRInputError error = vr::VRInput()->GetPoseActionDataForNextFrame(
      handle, trackingSpace, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);

    // An inactive pose has no binding for the current controller. Leave the
    // compositor/device-role result in place so legacy bindings still work.
    if (error != vr::VRInputError_None || !data.bActive) {
      if (stateKnown && wasValid) {
        Log(" [VR input +%lu ms] %s pose action lost (%s, error %d).\n", GetApplicationTime(),
          handName, data.bActive ? "read failure" : "inactive", (int)error);
        wasValid = false;
      }
      return false;
    }

    const bool valid = data.pose.bPoseIsValid && data.pose.bDeviceIsConnected;
    if (!stateKnown || valid != wasValid) {
      Log(" [VR input +%lu ms] %s pose action %s.\n", GetApplicationTime(), handName,
        valid ? "acquired" : "lost");
      stateKnown = true;
      wasValid = valid;
    }

    if (!valid) {
      hand.SetPoseValid(false);
      return true;
    }

    vr::TrackedDeviceIndex_t deviceIndex = hand.GetDeviceIndex();
    if (data.activeOrigin != vr::k_ulInvalidInputValueHandle) {
      vr::InputOriginInfo_t originInfo;
      memset(&originInfo, 0, sizeof(originInfo));
      if (vr::VRInput()->GetOriginTrackedDeviceInfo(data.activeOrigin, &originInfo, sizeof(originInfo)) ==
        vr::VRInputError_None) {
        deviceIndex = originInfo.trackedDeviceIndex;
      }
    }

    static const cMatrixf heightAdd = cMath::MatrixTranslate(cVector3f(0.0f, 0.02f, 0.0f));
    const vr::HmdMatrix34_t& poseMatrix = data.pose.mDeviceToAbsoluteTracking;
    const vr::HmdVector3_t& velocity = data.pose.vVelocity;
    const vr::HmdVector3_t& angularVelocity = data.pose.vAngularVelocity;

    hand.SetPose(
      cMath::MatrixMul(heightAdd, cMatrixf::FromSteamVRMatrix34(poseMatrix)),
      cVector3f(velocity.v[0], velocity.v[1], velocity.v[2]),
      cVector3f(angularVelocity.v[0], angularVelocity.v[1], angularVelocity.v[2]),
      deviceIndex);
    return true;
  }

  cVRButtonState cSteamVRInput::ReadDigital(vr::VRActionHandle_t handle) const {
    cVRButtonState state;
    vr::InputDigitalActionData_t data;
    memset(&data, 0, sizeof(data));

    vr::EVRInputError error = vr::VRInput()->GetDigitalActionData(
      handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
    if (error != vr::VRInputError_None) {
      return state;
    }

    state.active = data.bActive;
    state.pressed = data.bActive && data.bState;
    state.justPressed = data.bActive && data.bChanged && data.bState;
    state.justReleased = data.bActive && data.bChanged && !data.bState;
    return state;
  }

  cSteamVRInput::AnalogState cSteamVRInput::ReadAnalog(vr::VRActionHandle_t handle) const {
    AnalogState state;
    vr::InputAnalogActionData_t data;
    memset(&data, 0, sizeof(data));

    vr::EVRInputError error = vr::VRInput()->GetAnalogActionData(
      handle, &data, sizeof(data), vr::k_ulInvalidInputValueHandle);
    if (error != vr::VRInputError_None || !data.bActive) {
      return state;
    }

    state.active = true;
    state.x = data.x;
    state.y = data.y;
    return state;
  }

  void cSteamVRInput::SuppressDigitalEdges(cVRButtonState& state) const {
    state.justPressed = false;
    state.justReleased = false;
  }

  void cSteamVRInput::ApplyMoveDeadZone(float& x, float& y) const {
    const float magnitude = sqrtf(x * x + y * y);
    if (magnitude <= mfMoveDeadZone || magnitude <= 0.0001f) {
      x = 0.0f;
      y = 0.0f;
      return;
    }

    float scaledMagnitude = (magnitude - mfMoveDeadZone) / (1.0f - mfMoveDeadZone);
    if (scaledMagnitude > 1.0f) scaledMagnitude = 1.0f;
    x = (x / magnitude) * scaledMagnitude;
    y = (y / magnitude) * scaledMagnitude;
  }

  tString cSteamVRInput::FindManifestPath() const {
#ifdef WIN32
    char modulePath[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) {
      return "";
    }

    tString executablePath(modulePath, length);
    tString::size_type separator = executablePath.find_last_of("\\/");
    if (separator == tString::npos) {
      return "";
    }

    tString manifestPath = executablePath.substr(0, separator + 1) + "vr\\actions.json";
    DWORD attributes = GetFileAttributesA(manifestPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      return "";
    }
    return manifestPath;
#else
    return "";
#endif
  }
}
