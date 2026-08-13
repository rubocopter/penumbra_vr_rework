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

  class cSteamVRInput {
  public:
    cSteamVRInput();

    bool Initialize();
    bool Update(eSteamVRInputContext context, TrackedController& leftHand, TrackedController& rightHand);

    bool IsAvailable() const { return mbAvailable; }
    bool WasPausePressed() const { return mbPauseJustPressed; }
    bool WasHolsterPressed() const { return mbHolsterJustPressed; }
    bool WasUIClosePressed() const { return mbUICloseJustPressed; }
    const tString& GetManifestPath() const { return msManifestPath; }

    bool TriggerHaptic(eSteamVRHand hand, float durationSeconds, float frequency, float amplitude);

  private:
    struct DigitalState {
      DigitalState() : active(false), pressed(false), justPressed(false), justReleased(false) {}

      bool active;
      bool pressed;
      bool justPressed;
      bool justReleased;
    };

    struct AnalogState {
      AnalogState() : active(false), x(0.0f), y(0.0f) {}

      bool active;
      float x;
      float y;
    };

    bool ResolveActionSet(const char* path, vr::VRActionSetHandle_t& handle);
    bool ResolveAction(const char* path, vr::VRActionHandle_t& handle);
    DigitalState ReadDigital(vr::VRActionHandle_t handle) const;
    AnalogState ReadAnalog(vr::VRActionHandle_t handle) const;
    void ApplyDigital(const DigitalState& source, bool& pressed, bool& justPressed, bool& justReleased) const;
    tString FindManifestPath() const;

    bool mbAvailable;
    bool mbUsingActions;
    bool mbFallbackReported;
    bool mbPauseJustPressed;
    bool mbHolsterJustPressed;
    bool mbUICloseJustPressed;
    tString msManifestPath;

    vr::VRActionSetHandle_t mGlobalActionSet;
    vr::VRActionSetHandle_t mGameplayActionSet;
    vr::VRActionSetHandle_t mUIActionSet;

    vr::VRActionHandle_t mMoveAction;
    vr::VRActionHandle_t mSprintAction;
    vr::VRActionHandle_t mInteractAction;
    vr::VRActionHandle_t mExamineAction;
    vr::VRActionHandle_t mHolsterAction;
    vr::VRActionHandle_t mInventoryAction;
    vr::VRActionHandle_t mNotebookAction;
    vr::VRActionHandle_t mQuickLightAction;
    vr::VRActionHandle_t mJumpAction;
    vr::VRActionHandle_t mPauseAction;

    vr::VRActionHandle_t mUISelectAction;
    vr::VRActionHandle_t mUIDragAction;
    vr::VRActionHandle_t mUIBackAction;
    vr::VRActionHandle_t mUICloseAction;

    vr::VRActionHandle_t mLeftPoseAction;
    vr::VRActionHandle_t mRightPoseAction;
    vr::VRActionHandle_t mLeftHapticAction;
    vr::VRActionHandle_t mRightHapticAction;
  };
}

#endif // HPL_STEAM_VR_INPUT_H
