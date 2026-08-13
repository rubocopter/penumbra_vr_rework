#include "input/SteamVRInput.h"

#include "system/LowLevelSystem.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>

namespace hpl {

  cSteamVRInput::cSteamVRInput()
    : mbAvailable(false), mbUsingActions(false), mbFallbackReported(false), mbPauseJustPressed(false),
      mbUICloseJustPressed(false),
      mGlobalActionSet(vr::k_ulInvalidActionSetHandle),
      mGameplayActionSet(vr::k_ulInvalidActionSetHandle),
      mUIActionSet(vr::k_ulInvalidActionSetHandle),
      mMoveAction(vr::k_ulInvalidActionHandle),
      mSprintAction(vr::k_ulInvalidActionHandle),
      mInteractAction(vr::k_ulInvalidActionHandle),
      mExamineAction(vr::k_ulInvalidActionHandle),
      mInventoryAction(vr::k_ulInvalidActionHandle),
      mNotebookAction(vr::k_ulInvalidActionHandle),
      mQuickLightAction(vr::k_ulInvalidActionHandle),
      mJumpAction(vr::k_ulInvalidActionHandle),
      mPauseAction(vr::k_ulInvalidActionHandle),
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
    resolved = ResolveAction("/actions/gameplay/in/sprint", mSprintAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/interact", mInteractAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/examine", mExamineAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/inventory", mInventoryAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/notebook", mNotebookAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/quick_light", mQuickLightAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/jump", mJumpAction) && resolved;
    resolved = ResolveAction("/actions/gameplay/in/pause", mPauseAction) && resolved;

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
    mbPauseJustPressed = false;
    mbUICloseJustPressed = false;
    if (!mbAvailable) {
      return false;
    }

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

    TrackedController::ButtonState leftState(true);
    TrackedController::ButtonState rightState(true);
    bool anyActionActive = false;

    if (context == eSteamVRInputContext_Gameplay) {
      AnalogState move = ReadAnalog(mMoveAction);
      leftState.touchX = move.x;
      leftState.touchY = move.y;
      leftState.touchContact = move.active;
      anyActionActive = move.active;

      DigitalState sprint = ReadDigital(mSprintAction);
      DigitalState interact = ReadDigital(mInteractAction);
      DigitalState examine = ReadDigital(mExamineAction);
      DigitalState inventory = ReadDigital(mInventoryAction);
      DigitalState notebook = ReadDigital(mNotebookAction);
      DigitalState quickLight = ReadDigital(mQuickLightAction);
      DigitalState jump = ReadDigital(mJumpAction);
      DigitalState pause = ReadDigital(mPauseAction);

      ApplyDigital(sprint, rightState.padPressed, rightState.padJustPressed, rightState.padJustReleased);
      ApplyDigital(interact, rightState.triggerPressed, rightState.triggerJustPressed, rightState.triggerJustReleased);
      ApplyDigital(examine, rightState.menuPressed, rightState.menuJustPressed, rightState.menuJustReleased);
      ApplyDigital(inventory, rightState.gripPressed, rightState.gripJustPressed, rightState.gripJustReleased);
      ApplyDigital(notebook, leftState.menuPressed, leftState.menuJustPressed, leftState.menuJustReleased);
      ApplyDigital(quickLight, leftState.gripPressed, leftState.gripJustPressed, leftState.gripJustReleased);
      ApplyDigital(jump, leftState.triggerPressed, leftState.triggerJustPressed, leftState.triggerJustReleased);
      mbPauseJustPressed = pause.justPressed;

      anyActionActive = anyActionActive || sprint.active || interact.active || examine.active ||
        inventory.active || notebook.active || quickLight.active || jump.active || pause.active;
    }
    else {
      DigitalState select = ReadDigital(mUISelectAction);
      DigitalState drag = ReadDigital(mUIDragAction);
      DigitalState back = ReadDigital(mUIBackAction);
      DigitalState close = ReadDigital(mUICloseAction);

      ApplyDigital(select, rightState.triggerPressed, rightState.triggerJustPressed, rightState.triggerJustReleased);
      ApplyDigital(drag, rightState.padPressed, rightState.padJustPressed, rightState.padJustReleased);
      ApplyDigital(back, rightState.menuPressed, rightState.menuJustPressed, rightState.menuJustReleased);
      ApplyDigital(close, rightState.gripPressed, rightState.gripJustPressed, rightState.gripJustReleased);
      ApplyDigital(close, leftState.menuPressed, leftState.menuJustPressed, leftState.menuJustReleased);
      mbUICloseJustPressed = close.justPressed;

      anyActionActive = select.active || drag.active || back.active || close.active;
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
    leftHand.SetButtonState(leftState);
    rightHand.SetButtonState(rightState);
    return true;
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

  cSteamVRInput::DigitalState cSteamVRInput::ReadDigital(vr::VRActionHandle_t handle) const {
    DigitalState state;
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

  void cSteamVRInput::ApplyDigital(const DigitalState& source, bool& pressed, bool& justPressed, bool& justReleased) const {
    pressed = source.pressed;
    justPressed = source.justPressed;
    justReleased = source.justReleased;
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
