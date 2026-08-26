#ifndef HPL_STEAM_VR_INPUT_H
#define HPL_STEAM_VR_INPUT_H

#include "openvr.h"
#include "input/TrackedController.h"
#include "system/SystemTypes.h"

namespace hpl {

  enum eSteamVRInputContext {
    eSteamVRInputContext_Gameplay,
    eSteamVRInputContext_UI
  };

  enum eSteamVRHand {
    eSteamVRHand_Left,
    eSteamVRHand_Right
  };

  struct cVRButtonState {
    cVRButtonState() : active(false), pressed(false), justPressed(false), justReleased(false) {}

    bool active;
    bool pressed;
    bool justPressed;
    bool justReleased;
  };

  // Device-independent input consumed by Penumbra gameplay. Physical controls
  // are translated into these intents either by SteamVR actions or by the
  // historical OpenVR polling fallback.
  struct cVRInputState {
    cVRInputState()
      : moveActive(false), moveX(0.0f), moveY(0.0f),
        turnActive(false), turnX(0.0f) {}

    bool moveActive;
    float moveX;
    float moveY;
    bool turnActive;
    float turnX;

    cVRButtonState sprint;
    cVRButtonState interact;
    cVRButtonState examine;
    cVRButtonState holster;
    cVRButtonState inventory;
    cVRButtonState notebook;
    cVRButtonState quickLight;
    cVRButtonState jump;
    cVRButtonState crouch;
    cVRButtonState pause;
    cVRButtonState recenter;

    cVRButtonState uiSelect;
    cVRButtonState uiDrag;
    cVRButtonState uiBack;
    cVRButtonState uiClose;
  };

  class cSteamVRInput {
  public:
    cSteamVRInput();

    bool Initialize();
    bool Update(eSteamVRInputContext context, TrackedController& leftHand, TrackedController& rightHand);
    void UpdateLegacyState(eSteamVRInputContext context,
      TrackedController& leftHand, TrackedController& rightHand);

    bool IsAvailable() const { return mbAvailable; }
    const cVRInputState& GetState() const { return mState; }
    void SetMoveDeadZone(float deadZone);
    void SetHandedness(eSteamVRHand hand);
    eSteamVRHand GetHandedness() const { return mHandedness; }

    // Physical controller that produced the last interact press edge; used by
    // grab targeting to prefer the pressing hand.
    eSteamVRHand GetInteractSourceHand() const { return mInteractSourceHand; }
    void SetInteractSourceHand(eSteamVRHand hand) { mInteractSourceHand = hand; }

    bool TriggerHaptic(eSteamVRHand hand, float durationSeconds, float frequency, float amplitude);

  private:
    struct AnalogState {
      AnalogState() : active(false), x(0.0f), y(0.0f) {}

      bool active;
      float x;
      float y;
    };

    bool ResolveActionSet(const char* path, vr::VRActionSetHandle_t& handle);
    bool ResolveAction(const char* path, vr::VRActionHandle_t& handle);
    void RetryProfileDiagnostics();
    bool LogControllerProfiles() const;
    bool ManifestListsBinding(const tString& controllerType, tString& bindingUrl) const;
    bool UpdatePoseAction(vr::VRActionHandle_t handle, TrackedController& hand,
      const char* handName, bool& stateKnown, bool& wasValid, bool isAim);
    void UpdateSkeletonSummary(vr::VRActionHandle_t action, TrackedController& hand, const char* handName);
    cVRButtonState ReadDigital(vr::VRActionHandle_t handle) const;
    AnalogState ReadAnalog(vr::VRActionHandle_t handle) const;
    void SuppressDigitalEdges(cVRButtonState& state) const;
    void SuppressAllEdges(cVRInputState& state) const;
    void ApplyMoveDeadZone(float& x, float& y) const;
    tString FindManifestPath() const;
    // Mirrored action sets (/actions/gameplay_left, /actions/ui_left) carry the
    // same intents bound to the opposite physical hand, so left-handed users
    // get a full left/right mirror of the button layout.
    vr::VRActionHandle_t ActionFor(vr::VRActionHandle_t rightAction,
      vr::VRActionHandle_t leftAction) const;

    bool mbAvailable;
    bool mbUsingActions;
    bool mbFallbackReported;
    bool mbHasActiveContext;
    bool mbLeftPoseStateKnown;
    bool mbRightPoseStateKnown;
    bool mbLeftAimStateKnown;
    bool mbRightAimStateKnown;
    bool mbLeftPoseWasValid;
    bool mbRightPoseWasValid;
    bool mbLeftAimWasValid;
    bool mbRightAimWasValid;
    eSteamVRInputContext mActiveContext;
    eSteamVRHand mActiveHandedness;
    cVRInputState mState;
    float mfMoveDeadZone;
    eSteamVRHand mHandedness;
    eSteamVRHand mInteractSourceHand;
    tString msManifestPath;

    vr::VRActionSetHandle_t mGlobalActionSet;
    vr::VRActionSetHandle_t mOffhandActionSet;
    vr::VRActionSetHandle_t mGameplayActionSet;
    vr::VRActionSetHandle_t mUIActionSet;
    vr::VRActionSetHandle_t mGameplayActionSetLeft;
    vr::VRActionSetHandle_t mUIActionSetLeft;

    vr::VRActionHandle_t mMoveAction;
    vr::VRActionHandle_t mTurnAction;
    vr::VRActionHandle_t mSprintAction;
    vr::VRActionHandle_t mInteractAction;
    vr::VRActionHandle_t mExamineAction;
    vr::VRActionHandle_t mHolsterAction;
    vr::VRActionHandle_t mInventoryAction;
    vr::VRActionHandle_t mNotebookAction;
    vr::VRActionHandle_t mQuickLightAction;
    vr::VRActionHandle_t mJumpAction;
    vr::VRActionHandle_t mCrouchAction;
    vr::VRActionHandle_t mPauseAction;
    vr::VRActionHandle_t mRecenterAction;
    vr::VRActionHandle_t mOffhandInteractAction;

    vr::VRActionHandle_t mMoveActionLeft;
    vr::VRActionHandle_t mTurnActionLeft;
    vr::VRActionHandle_t mSprintActionLeft;
    vr::VRActionHandle_t mInteractActionLeft;
    vr::VRActionHandle_t mExamineActionLeft;
    vr::VRActionHandle_t mHolsterActionLeft;
    vr::VRActionHandle_t mInventoryActionLeft;
    vr::VRActionHandle_t mNotebookActionLeft;
    vr::VRActionHandle_t mQuickLightActionLeft;
    vr::VRActionHandle_t mJumpActionLeft;
    vr::VRActionHandle_t mCrouchActionLeft;
    vr::VRActionHandle_t mPauseActionLeft;

    vr::VRActionHandle_t mUISelectAction;
    vr::VRActionHandle_t mUIDragAction;
    vr::VRActionHandle_t mUIBackAction;
    vr::VRActionHandle_t mUICloseAction;

    vr::VRActionHandle_t mUISelectActionLeft;
    vr::VRActionHandle_t mUIDragActionLeft;
    vr::VRActionHandle_t mUIBackActionLeft;
    vr::VRActionHandle_t mUICloseActionLeft;

    vr::VRActionHandle_t mLeftPoseAction;
    vr::VRActionHandle_t mRightPoseAction;
    vr::VRActionHandle_t mLeftAimAction;
    vr::VRActionHandle_t mRightAimAction;
    vr::VRActionHandle_t mLeftHapticAction;
    vr::VRActionHandle_t mRightHapticAction;

    vr::VRActionHandle_t mLeftSkeletonAction;
    vr::VRActionHandle_t mRightSkeletonAction;
    vr::VRInputValueHandle_t mLeftInputSource;
    vr::VRInputValueHandle_t mRightInputSource;
    unsigned long mlLastSkeletonErrorLog;
    int mlLastSkeletonErrorCode;
    int mlLastSkeletonFallbackCode;
    bool mbSkeletonFallbackReported;
    bool mbProfilesIdentified;
    unsigned long mlNextProfileAttempt;
    unsigned int mlProfileAttempts;
    unsigned long mlActionsIdleSince;
    unsigned long mlSticksInactiveSince;
    bool mbSticksWarningReported;
  };
}

#endif // HPL_STEAM_VR_INPUT_H
