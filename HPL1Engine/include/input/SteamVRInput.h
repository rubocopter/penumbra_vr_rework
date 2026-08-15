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

  enum eVRInputSource {
    eVRInputSource_None,
    eVRInputSource_SteamVRActions,
    eVRInputSource_LegacyOpenVR
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
      : source(eVRInputSource_None), context(eSteamVRInputContext_Gameplay),
        moveActive(false), moveX(0.0f), moveY(0.0f),
        turnActive(false), turnX(0.0f) {}

    eVRInputSource source;
    eSteamVRInputContext context;

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
    bool IsUsingActions() const { return mbUsingActions; }
    const cVRInputState& GetState() const { return mState; }
    const tString& GetManifestPath() const { return msManifestPath; }
    void SetMoveDeadZone(float deadZone);
    float GetMoveDeadZone() const { return mfMoveDeadZone; }

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
    bool UpdatePoseAction(vr::VRActionHandle_t handle, TrackedController& hand,
      const char* handName, bool& stateKnown, bool& wasValid);
    void UpdateSkeletonSummary(vr::VRActionHandle_t action, TrackedController& hand, const char* handName);
    void LogHandSummary(const TrackedController& hand, const char* handName, const char* source,
      unsigned long& lastLogTime);
    cVRButtonState ReadDigital(vr::VRActionHandle_t handle) const;
    AnalogState ReadAnalog(vr::VRActionHandle_t handle) const;
    void SuppressDigitalEdges(cVRButtonState& state) const;
    void ApplyMoveDeadZone(float& x, float& y) const;
    tString FindManifestPath() const;

    bool mbAvailable;
    bool mbUsingActions;
    bool mbFallbackReported;
    bool mbHasActiveContext;
    bool mbLeftPoseStateKnown;
    bool mbRightPoseStateKnown;
    bool mbLeftPoseWasValid;
    bool mbRightPoseWasValid;
    eSteamVRInputContext mActiveContext;
    cVRInputState mState;
    float mfMoveDeadZone;
    tString msManifestPath;

    vr::VRActionSetHandle_t mGlobalActionSet;
    vr::VRActionSetHandle_t mGameplayActionSet;
    vr::VRActionSetHandle_t mUIActionSet;

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

    vr::VRActionHandle_t mUISelectAction;
    vr::VRActionHandle_t mUIDragAction;
    vr::VRActionHandle_t mUIBackAction;
    vr::VRActionHandle_t mUICloseAction;

    vr::VRActionHandle_t mLeftPoseAction;
    vr::VRActionHandle_t mRightPoseAction;
    vr::VRActionHandle_t mLeftHapticAction;
    vr::VRActionHandle_t mRightHapticAction;

    vr::VRActionHandle_t mLeftSkeletonAction;
    vr::VRActionHandle_t mRightSkeletonAction;
    unsigned long mlLastLeftHandSummaryLog;
    unsigned long mlLastRightHandSummaryLog;
    unsigned long mlLastSkeletonErrorLog;
    int mlLastSkeletonErrorCode;
    int mlLastSkeletonFallbackCode;
    bool mbSkeletonFallbackReported;
  };
}

#endif // HPL_STEAM_VR_INPUT_H
