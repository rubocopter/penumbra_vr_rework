#include "input/SteamVRInput.h"

#include "system/LowLevelSystem.h"

#ifdef WIN32
#include <windows.h>
#endif

#include <string.h>
#include <stdio.h>
#include <math.h>

namespace hpl {

  cSteamVRInput::cSteamVRInput()
    : mbAvailable(false), mbUsingActions(false), mbFallbackReported(false), mbHasActiveContext(false),
      mbLeftPoseStateKnown(false), mbRightPoseStateKnown(false),
      mbLeftPoseWasValid(false), mbRightPoseWasValid(false),
      mActiveContext(eSteamVRInputContext_Gameplay),
      mActiveHandedness(eSteamVRHand_Right), mfMoveDeadZone(0.15f),
      mHandedness(eSteamVRHand_Right),
      mGlobalActionSet(vr::k_ulInvalidActionSetHandle),
      mGameplayActionSet(vr::k_ulInvalidActionSetHandle),
      mUIActionSet(vr::k_ulInvalidActionSetHandle),
      mGameplayActionSetLeft(vr::k_ulInvalidActionSetHandle),
      mUIActionSetLeft(vr::k_ulInvalidActionSetHandle),
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
      mMoveActionLeft(vr::k_ulInvalidActionHandle),
      mTurnActionLeft(vr::k_ulInvalidActionHandle),
      mSprintActionLeft(vr::k_ulInvalidActionHandle),
      mInteractActionLeft(vr::k_ulInvalidActionHandle),
      mExamineActionLeft(vr::k_ulInvalidActionHandle),
      mHolsterActionLeft(vr::k_ulInvalidActionHandle),
      mInventoryActionLeft(vr::k_ulInvalidActionHandle),
      mNotebookActionLeft(vr::k_ulInvalidActionHandle),
      mQuickLightActionLeft(vr::k_ulInvalidActionHandle),
      mJumpActionLeft(vr::k_ulInvalidActionHandle),
      mCrouchActionLeft(vr::k_ulInvalidActionHandle),
      mPauseActionLeft(vr::k_ulInvalidActionHandle),
      mUISelectAction(vr::k_ulInvalidActionHandle),
      mUIDragAction(vr::k_ulInvalidActionHandle),
      mUIBackAction(vr::k_ulInvalidActionHandle),
      mUICloseAction(vr::k_ulInvalidActionHandle),
      mUISelectActionLeft(vr::k_ulInvalidActionHandle),
      mUIDragActionLeft(vr::k_ulInvalidActionHandle),
      mUIBackActionLeft(vr::k_ulInvalidActionHandle),
      mUICloseActionLeft(vr::k_ulInvalidActionHandle),
      mLeftPoseAction(vr::k_ulInvalidActionHandle),
      mRightPoseAction(vr::k_ulInvalidActionHandle),
      mLeftHapticAction(vr::k_ulInvalidActionHandle),
      mRightHapticAction(vr::k_ulInvalidActionHandle),
      mLeftSkeletonAction(vr::k_ulInvalidActionHandle),
      mRightSkeletonAction(vr::k_ulInvalidActionHandle),
      mlLastSkeletonErrorLog(0),
      mlLastSkeletonErrorCode(0), mlLastSkeletonFallbackCode(0),
      mbSkeletonFallbackReported(false),
      mbProfilesIdentified(false), mlNextProfileAttempt(0), mlProfileAttempts(0),
      mlActionsIdleSince(0) {
  }

  bool cSteamVRInput::Initialize() {
    msManifestPath = FindManifestPath();
    RetryProfileDiagnostics();
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
    resolved = ResolveActionSet("/actions/gameplay_left", mGameplayActionSetLeft) && resolved;
    resolved = ResolveActionSet("/actions/ui_left", mUIActionSetLeft) && resolved;

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

    // Mirrored (left-handed) action sets. Same intents, same action names,
    // bound to the opposite physical hand.
    resolved = ResolveAction("/actions/gameplay_left/in/move", mMoveActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/turn", mTurnActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/sprint", mSprintActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/interact", mInteractActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/examine", mExamineActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/holster", mHolsterActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/inventory", mInventoryActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/notebook", mNotebookActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/quick_light", mQuickLightActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/jump", mJumpActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/crouch", mCrouchActionLeft) && resolved;
    resolved = ResolveAction("/actions/gameplay_left/in/pause", mPauseActionLeft) && resolved;

    resolved = ResolveAction("/actions/ui_left/in/select", mUISelectActionLeft) && resolved;
    resolved = ResolveAction("/actions/ui_left/in/drag", mUIDragActionLeft) && resolved;
    resolved = ResolveAction("/actions/ui_left/in/back", mUIBackActionLeft) && resolved;
    resolved = ResolveAction("/actions/ui_left/in/close", mUICloseActionLeft) && resolved;

    resolved = ResolveAction("/actions/global/in/left_pose", mLeftPoseAction) && resolved;
    resolved = ResolveAction("/actions/global/in/right_pose", mRightPoseAction) && resolved;
    resolved = ResolveAction("/actions/global/in/left_aim", mLeftAimAction) && resolved;
    resolved = ResolveAction("/actions/global/in/right_aim", mRightAimAction) && resolved;
    resolved = ResolveAction("/actions/global/out/left_haptic", mLeftHapticAction) && resolved;
    resolved = ResolveAction("/actions/global/out/right_haptic", mRightHapticAction) && resolved;

    resolved = ResolveAction("/actions/global/in/left_skeleton", mLeftSkeletonAction) && resolved;
    resolved = ResolveAction("/actions/global/in/right_skeleton", mRightSkeletonAction) && resolved;

    if (!resolved) {
      Log(" SteamVR Input disabled: the action manifest is incomplete. Falling back to legacy controller polling.\n");
      return false;
    }

    mbAvailable = true;
    Log(" SteamVR Input initialized with '%s'.\n", msManifestPath.c_str());
    return true;
  }

  bool cSteamVRInput::Update(eSteamVRInputContext context, TrackedController& leftHand, TrackedController& rightHand) {
    // Controller roles and their property strings can appear seconds after
    // VR_Init; keep retrying briefly so the profile log reflects real hardware.
    RetryProfileDiagnostics();

    const cVRInputState previousState = mState;
    mState = cVRInputState();
    if (!mbAvailable) {
      return false;
    }

    // SteamVR reports a held physical control as changed when the newly active
    // action set binds that control to another action. Without latching the
    // first state, R1/Options can open a UI and immediately close it again.
    // The same latch applies when the dominant hand changes and the mirrored
    // action set takes over.
    const bool suppressContextEdges = !mbUsingActions || !mbHasActiveContext ||
      context != mActiveContext || mHandedness != mActiveHandedness;

    vr::VRActiveActionSet_t activeSets[2];
    memset(activeSets, 0, sizeof(activeSets));
    activeSets[0].ulActionSet = mGlobalActionSet;
    activeSets[1].ulActionSet = mHandedness == eSteamVRHand_Left
      ? (context == eSteamVRInputContext_UI ? mUIActionSetLeft : mGameplayActionSetLeft)
      : (context == eSteamVRInputContext_UI ? mUIActionSet : mGameplayActionSet);

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
    UpdatePoseAction(mLeftPoseAction, leftHand, "left", mbLeftPoseStateKnown, mbLeftPoseWasValid, false);
    UpdatePoseAction(mRightPoseAction, rightHand, "right", mbRightPoseStateKnown, mbRightPoseWasValid, false);
    UpdatePoseAction(mLeftAimAction, leftHand, "left aim", mbLeftAimStateKnown, mbLeftAimWasValid, true);
    UpdatePoseAction(mRightAimAction, rightHand, "right aim", mbRightAimStateKnown, mbRightAimWasValid, true);

    // Per-hand skeletal summary drives future hand-pose animation (open, grab,
    // trigger). Devices without skeletal support leave the summary invalid and
    // keep their previous handling; the legacy path fills it from buttons.
    UpdateSkeletonSummary(mLeftSkeletonAction, leftHand, "left");
    UpdateSkeletonSummary(mRightSkeletonAction, rightHand, "right");

    cVRInputState nextState;
    nextState.recenter = ReadDigital(mRecenterAction);
    // The mirror transition can re-report a held create as a fresh press, so
    // latch that edge only when the dominant hand changes. A context switch or
    // a return from the legacy fallback must not swallow a real recenter press:
    // the legacy path has no create button to fall back on.
    if (mHandedness != mActiveHandedness) {
      SuppressDigitalEdges(nextState.recenter);
    }
    // Recenter is frequently the only active input (the player stands still to
    // re-aim). Count it as an active action, otherwise an idle frame drops to
    // legacy polling below and the press is lost.
    bool anyActionActive = nextState.recenter.active;

    if (context == eSteamVRInputContext_Gameplay) {
      AnalogState move = ReadAnalog(ActionFor(mMoveAction, mMoveActionLeft));
      ApplyMoveDeadZone(move.x, move.y);
      nextState.moveX = move.x;
      nextState.moveY = move.y;
      nextState.moveActive = move.active;
      // OR, never overwrite: recenter was counted above and is frequently the
      // only active input while standing still.
      anyActionActive = anyActionActive || move.active;

      AnalogState turn = ReadAnalog(ActionFor(mTurnAction, mTurnActionLeft));
      nextState.turnX = turn.x;
      nextState.turnActive = turn.active;
      anyActionActive = anyActionActive || turn.active;

      nextState.sprint = ReadDigital(ActionFor(mSprintAction, mSprintActionLeft));
      nextState.interact = ReadDigital(ActionFor(mInteractAction, mInteractActionLeft));
      nextState.examine = ReadDigital(ActionFor(mExamineAction, mExamineActionLeft));
      nextState.holster = ReadDigital(ActionFor(mHolsterAction, mHolsterActionLeft));
      nextState.inventory = ReadDigital(ActionFor(mInventoryAction, mInventoryActionLeft));
      nextState.notebook = ReadDigital(ActionFor(mNotebookAction, mNotebookActionLeft));
      nextState.quickLight = ReadDigital(ActionFor(mQuickLightAction, mQuickLightActionLeft));
      nextState.jump = ReadDigital(ActionFor(mJumpAction, mJumpActionLeft));
      nextState.crouch = ReadDigital(ActionFor(mCrouchAction, mCrouchActionLeft));
      nextState.pause = ReadDigital(ActionFor(mPauseAction, mPauseActionLeft));

      // World interaction needs a current pointing pose. Preserve non-spatial
      // controls such as pause/inventory, but release an in-progress interact
      // cleanly if the dominant hand's tracking disappears.
      TrackedController& pointerHand = mHandedness == eSteamVRHand_Left ? leftHand : rightHand;
      if (!pointerHand.IsPoseValid()) {
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

      anyActionActive = anyActionActive || nextState.sprint.active || nextState.interact.active ||
        nextState.examine.active || nextState.holster.active || nextState.inventory.active ||
        nextState.notebook.active || nextState.quickLight.active || nextState.jump.active ||
        nextState.crouch.active || nextState.pause.active;
    }
    else {
      nextState.uiSelect = ReadDigital(ActionFor(mUISelectAction, mUISelectActionLeft));
      nextState.uiDrag = ReadDigital(ActionFor(mUIDragAction, mUIDragActionLeft));
      nextState.uiBack = ReadDigital(ActionFor(mUIBackAction, mUIBackActionLeft));
      nextState.uiClose = ReadDigital(ActionFor(mUICloseAction, mUICloseActionLeft));

      // Selection and dragging are pointer operations. Closing or backing out
      // remains available even if optical hand tracking is temporarily lost.
      TrackedController& pointerHand = mHandedness == eSteamVRHand_Left ? leftHand : rightHand;
      if (!pointerHand.IsPoseValid()) {
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

      anyActionActive = anyActionActive || nextState.uiSelect.active ||
        nextState.uiDrag.active || nextState.uiBack.active || nextState.uiClose.active;
    }

    if (anyActionActive) {
      if (!mbUsingActions) {
        Log(" SteamVR Input actions are active.\n");
      }
      mbUsingActions = true;
      mbFallbackReported = false;
      mbHasActiveContext = true;
      mActiveContext = context;
      mActiveHandedness = mHandedness;
      mState = nextState;
      mlActionsIdleSince = 0;
      return true;
    }

    // SteamVR intermittently reports every action origin as inactive for a
    // handful of frames while it refreshes bindings or re-evaluates devices
    // (observed on PS VR2 and Index). Dropping to legacy polling on the first
    // idle frame swallowed held inputs and, on controllers whose sticks do not
    // assert the legacy touch bit, dead-ended movement for whole minutes.
    // Hold the last good state through a short grace window before falling back.
    const unsigned long now = GetApplicationTime();
    if (mlActionsIdleSince == 0) {
      mlActionsIdleSince = now;
    }

    if (!mbUsingActions || now - mlActionsIdleSince < 500) {
      if (mbUsingActions) {
        mState = previousState;
        SuppressAllEdges(mState);
        return true;
      }
      return false;
    }

    if (!mbFallbackReported) {
      Log(" SteamVR Input actions became inactive. Falling back to legacy controller polling.\n");
      mbFallbackReported = true;
    }
    mbUsingActions = false;
    mlActionsIdleSince = 0;
    return false;
  }

  vr::VRActionHandle_t cSteamVRInput::ActionFor(vr::VRActionHandle_t rightAction,
    vr::VRActionHandle_t leftAction) const {
    return mHandedness == eSteamVRHand_Left ? leftAction : rightAction;
  }

  void cSteamVRInput::UpdateLegacyState(eSteamVRInputContext context,
    TrackedController& leftHand, TrackedController& rightHand) {
    const TrackedController::ButtonState& leftState = leftHand.GetButtonState();
    const TrackedController::ButtonState& rightState = rightHand.GetButtonState();
    const TrackedController::ButtonState& dominantState =
      mHandedness == eSteamVRHand_Left ? leftState : rightState;
    // The mirror of the hard-coded right-handed layout: everything that used
    // the right hand moves to the dominant hand and vice versa.
    const TrackedController::ButtonState& offState =
      mHandedness == eSteamVRHand_Left ? rightState : leftState;

    // Skeletal data is not available on the fallback path: grip becomes a
    // binary 0/1 button value while the trigger keeps its analog margin.
    if (leftState.valid_) {
      leftHand.SetLegacySummary(leftState.gripPressed ? 1.0f : 0.0f, leftState.triggerMargin);
    }
    else {
      leftHand.InvalidateHandSummary();
    }
    if (rightState.valid_) {
      rightHand.SetLegacySummary(rightState.gripPressed ? 1.0f : 0.0f, rightState.triggerMargin);
    }
    else {
      rightHand.InvalidateHandSummary();
    }

    cVRInputState legacyState;

    if (context == eSteamVRInputContext_Gameplay) {
      legacyState.moveActive = offState.touchContact;
      legacyState.moveX = offState.touchX;
      legacyState.moveY = offState.touchY;
      ApplyMoveDeadZone(legacyState.moveX, legacyState.moveY);

      legacyState.sprint.active = dominantState.valid_;
      legacyState.sprint.pressed = dominantState.padPressed;
      legacyState.sprint.justPressed = dominantState.padJustPressed;
      legacyState.sprint.justReleased = dominantState.padJustReleased;

      legacyState.interact.active = dominantState.valid_;
      legacyState.interact.pressed = dominantState.triggerPressed;
      legacyState.interact.justPressed = dominantState.triggerJustPressed;
      legacyState.interact.justReleased = dominantState.triggerJustReleased;

      legacyState.examine.active = dominantState.valid_;
      legacyState.examine.pressed = dominantState.menuPressed;
      legacyState.examine.justPressed = dominantState.menuJustPressed;
      legacyState.examine.justReleased = dominantState.menuJustReleased;

      legacyState.inventory.active = dominantState.valid_;
      legacyState.inventory.pressed = dominantState.gripPressed;
      legacyState.inventory.justPressed = dominantState.gripJustPressed;
      legacyState.inventory.justReleased = dominantState.gripJustReleased;

      legacyState.notebook.active = offState.valid_;
      legacyState.notebook.pressed = offState.menuPressed;
      legacyState.notebook.justPressed = offState.menuJustPressed;
      legacyState.notebook.justReleased = offState.menuJustReleased;

      legacyState.quickLight.active = offState.valid_;
      legacyState.quickLight.pressed = offState.gripPressed;
      legacyState.quickLight.justPressed = offState.gripJustPressed;
      legacyState.quickLight.justReleased = offState.gripJustReleased;

      legacyState.jump.active = offState.valid_;
      legacyState.jump.pressed = offState.triggerPressed;
      legacyState.jump.justPressed = offState.triggerJustPressed;
      legacyState.jump.justReleased = offState.triggerJustReleased;
    }
    else {
      legacyState.uiSelect.active = dominantState.valid_;
      legacyState.uiSelect.pressed = dominantState.triggerPressed;
      legacyState.uiSelect.justPressed = dominantState.triggerJustPressed;
      legacyState.uiSelect.justReleased = dominantState.triggerJustReleased;

      legacyState.uiDrag.active = dominantState.valid_;
      legacyState.uiDrag.pressed = dominantState.padPressed;
      legacyState.uiDrag.justPressed = dominantState.padJustPressed;
      legacyState.uiDrag.justReleased = dominantState.padJustReleased;

      legacyState.uiBack.active = dominantState.valid_;
      legacyState.uiBack.pressed = dominantState.menuPressed;
      legacyState.uiBack.justPressed = dominantState.menuJustPressed;
      legacyState.uiBack.justReleased = dominantState.menuJustReleased;

      legacyState.uiClose.active = dominantState.valid_ || offState.valid_;
      legacyState.uiClose.pressed = dominantState.gripPressed || offState.menuPressed;
      legacyState.uiClose.justPressed = dominantState.gripJustPressed || offState.menuJustPressed;
      legacyState.uiClose.justReleased = !legacyState.uiClose.pressed &&
        (dominantState.gripJustReleased || offState.menuJustReleased);
    }

    mState = legacyState;
  }

  void cSteamVRInput::UpdateSkeletonSummary(vr::VRActionHandle_t action, TrackedController& hand,
    const char* handName) {
    if (action == vr::k_ulInvalidActionHandle) {
      hand.InvalidateHandSummary();
      return;
    }

    vr::VRSkeletalSummaryData_t summary;
    memset(&summary, 0, sizeof(summary));

    const vr::EVRInputError error = vr::VRInput()->GetSkeletalSummaryData(
      action, vr::VRSummaryType_FromDevice, &summary);
    if (error == vr::VRInputError_None) {
      mbSkeletonFallbackReported = false;
      hand.SetSkeletonSummary(summary.flFingerCurl);
      return;
    }

    // Some drivers expose the estimated hand pose only through the animation
    // layer (e.g. controllers without finger tracking). Fall back to it so the
    // hand summary still follows the physical buttons.
    vr::VRSkeletalSummaryData_t animationSummary;
    memset(&animationSummary, 0, sizeof(animationSummary));
    const vr::EVRInputError animationError = vr::VRInput()->GetSkeletalSummaryData(
      action, vr::VRSummaryType_FromAnimation, &animationSummary);
    if (animationError == vr::VRInputError_None) {
      if (!mbSkeletonFallbackReported || GetApplicationTime() - mlLastSkeletonErrorLog >= 5000) {
        Log(" [VR input +%lu ms] %s skeleton summary via animation fallback (device error %d).\n",
          GetApplicationTime(), handName, (int)error);
        mlLastSkeletonErrorLog = GetApplicationTime();
        mlLastSkeletonErrorCode = (int)error;
        mbSkeletonFallbackReported = true;
      }
      hand.SetSkeletonSummary(animationSummary.flFingerCurl);
      return;
    }

    if (GetApplicationTime() - mlLastSkeletonErrorLog >= 5000 &&
      ((int)error != mlLastSkeletonErrorCode || (int)animationError != mlLastSkeletonFallbackCode)) {
      Log(" [VR input +%lu ms] %s skeleton summary unavailable: device error %d, animation error %d.\n",
        GetApplicationTime(), handName, (int)error, (int)animationError);
      mlLastSkeletonErrorLog = GetApplicationTime();
      mlLastSkeletonErrorCode = (int)error;
      mlLastSkeletonFallbackCode = (int)animationError;
      mbSkeletonFallbackReported = false;
    }
    hand.InvalidateHandSummary();
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

  void cSteamVRInput::SetHandedness(eSteamVRHand hand) {
    mHandedness = hand;
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

  void cSteamVRInput::RetryProfileDiagnostics() {
    if (mbProfilesIdentified || mlProfileAttempts >= 5) {
      return;
    }

    const unsigned long now = GetApplicationTime();
    if (mlProfileAttempts > 0 && now < mlNextProfileAttempt) {
      return;
    }

    mlProfileAttempts++;
    mlNextProfileAttempt = now + 5000;
    mbProfilesIdentified = LogControllerProfiles();
  }

  bool cSteamVRInput::LogControllerProfiles() const {
    vr::IVRSystem* system = vr::VRSystem();
    if (system == NULL) {
      return false;
    }

    bool identified = false;
    char text[vr::k_unMaxPropertyStringSize];
    vr::ETrackedPropertyError propertyError = vr::TrackedProp_Success;

    memset(text, 0, sizeof(text));
    system->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd,
      vr::Prop_TrackingSystemName_String, text, sizeof(text), &propertyError);
    if (propertyError == vr::TrackedProp_Success && text[0] != '\0') {
      Log(" [VR input] headset driver '%s'.\n", text);
    }

    const vr::TrackedDeviceIndex_t handDevices[2] = {
      system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand),
      system->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand)
    };
    const char* handNames[2] = { "left", "right" };

    for (int i = 0; i < 2; ++i) {
      if (handDevices[i] == vr::k_unTrackedDeviceIndexInvalid) {
        Log(" [VR input] %s hand controller not detected yet.\n", handNames[i]);
        continue;
      }

      memset(text, 0, sizeof(text));
      system->GetStringTrackedDeviceProperty(handDevices[i], vr::Prop_ControllerType_String,
        text, sizeof(text), &propertyError);
      if (propertyError != vr::TrackedProp_Success || text[0] == '\0') {
        Log(" [VR input] %s hand controller type unknown (property error %d).\n",
          handNames[i], (int)propertyError);
        continue;
      }

      identified = true;
      tString bindingUrl;
      if (ManifestListsBinding(text, bindingUrl)) {
        Log(" [VR input] %s hand controller type '%s' matches bundled default profile '%s'.\n",
          handNames[i], text, bindingUrl.c_str());
      }
      else {
        Log(" WARNING [VR input]: %s hand controller type '%s' has no bundled default profile in actions.json."
          " SteamVR applies its generic binding, unmatched actions drop to legacy polling and controls may be partial."
          " See docs/INPUT.md for the supported controller types.\n", handNames[i], text);
      }
    }

    return identified;
  }

  bool cSteamVRInput::ManifestListsBinding(const tString& controllerType, tString& bindingUrl) const {
    bindingUrl.clear();
    if (msManifestPath.empty() || controllerType.empty()) {
      return false;
    }

    FILE* manifest = fopen(msManifestPath.c_str(), "rb");
    if (manifest == NULL) {
      return false;
    }

    tString text;
    char buffer[4096];
    size_t bytesRead = 0;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), manifest)) > 0) {
      text.append(buffer, bytesRead);
    }
    fclose(manifest);

    tString typeKey = "\"controller_type\": \"" + controllerType + "\"";
    tString::size_type typeAt = text.find(typeKey);
    if (typeAt == tString::npos) {
      typeKey = "\"controller_type\":\"" + controllerType + "\"";
      typeAt = text.find(typeKey);
    }
    if (typeAt == tString::npos) {
      return false;
    }

    const tString::size_type nextTypeAt = text.find("\"controller_type\"", typeAt + 1);
    const char* urlKeys[2] = { "\"binding_url\": \"", "\"binding_url\":\"" };
    for (int i = 0; i < 2; ++i) {
      const tString::size_type urlAt = text.find(urlKeys[i], typeAt);
      if (urlAt == tString::npos) {
        continue;
      }
      if (nextTypeAt != tString::npos && urlAt > nextTypeAt) {
        return false;
      }

      const tString::size_type start = urlAt + strlen(urlKeys[i]);
      const tString::size_type end = text.find('"', start);
      if (end == tString::npos) {
        return false;
      }
      bindingUrl = text.substr(start, end - start);
      return true;
    }

    return false;
  }

bool cSteamVRInput::UpdatePoseAction(vr::VRActionHandle_t handle, TrackedController& hand,
    const char* handName, bool& stateKnown, bool& wasValid, bool isAim) {
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
    // The aim pose is auxiliary: never invalidate the shared grip pose,
    // otherwise a missing aim binding would hide the hands and drop the
    // whole input path to legacy polling.
    if (!isAim) {
      hand.SetPoseValid(false);
      hand.SetAimValid(false);
    } else {
      hand.SetAimValid(false);
    }
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

  if (isAim) {
    hand.SetAimMatrix(
      cMath::MatrixMul(heightAdd, cMatrixf::FromSteamVRMatrix34(poseMatrix)));
    hand.SetAimValid(true);
  } else {
    hand.SetPose(
      cMath::MatrixMul(heightAdd, cMatrixf::FromSteamVRMatrix34(poseMatrix)),
      cVector3f(velocity.v[0], velocity.v[1], velocity.v[2]),
      cVector3f(angularVelocity.v[0], angularVelocity.v[1], angularVelocity.v[2]),
      deviceIndex);
  }
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

  void cSteamVRInput::SuppressAllEdges(cVRInputState& state) const {
    SuppressDigitalEdges(state.sprint);
    SuppressDigitalEdges(state.interact);
    SuppressDigitalEdges(state.examine);
    SuppressDigitalEdges(state.holster);
    SuppressDigitalEdges(state.inventory);
    SuppressDigitalEdges(state.notebook);
    SuppressDigitalEdges(state.quickLight);
    SuppressDigitalEdges(state.jump);
    SuppressDigitalEdges(state.crouch);
    SuppressDigitalEdges(state.pause);
    SuppressDigitalEdges(state.recenter);
    SuppressDigitalEdges(state.uiSelect);
    SuppressDigitalEdges(state.uiDrag);
    SuppressDigitalEdges(state.uiBack);
    SuppressDigitalEdges(state.uiClose);
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
