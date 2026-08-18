/*
 * Copyright (C) 2006-2010 - Frictional Games
 *
 * This file is part of Penumbra Overture.
 *
 * Penumbra Overture is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Penumbra Overture is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Penumbra Overture.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "ButtonHandler.h"

#include "Init.h"
#include "Player.h"
#include "Inventory.h"
#include "Notebook.h"
#include "NumericalPanel.h"
#include "IntroStory.h"
#include "SaveHandler.h"
#include "MapHandler.h"
#include "DeathMenu.h"
#include "MapLoadText.h"
#include "PreMenu.h"
#include "Credits.h"
#include "DemoEndText.h"
#include "VRHaptics.h"

#include "PlayerHelper.h"

#include "MainMenu.h"

struct cButtonHandlerAction
{
	tString msName;
	tString msType;
	int mlVal;
	bool mbConfig;
};

static tString gsLastPlayerAction = "GlowStick";
static cButtonHandlerAction gvDefaultActions[] = {
{"Forward","Keyboard",eKey_w,true},
{"Backward","Keyboard",eKey_s,true},
{"Left","Keyboard",eKey_a,true},
{"Right","Keyboard",eKey_d,true},

{"LeanLeft","Keyboard",eKey_q,true},
{"LeanRight","Keyboard",eKey_e,true},

{"Run","Keyboard",eKey_LSHIFT,true},
{"Jump","Keyboard",eKey_SPACE,true},
{"Crouch","Keyboard",eKey_LCTRL,true},

{"InteractMode","Keyboard",eKey_r,true},
{"LookMode","MouseButton",eMButton_Middle,true},

{"Holster","Keyboard",eKey_x,true},

{"Examine","MouseButton",eMButton_Right,true},
{"Interact","MouseButton",eMButton_Left,true},

{"Inventory","Keyboard",eKey_TAB,true},
{"NoteBook","Keyboard",eKey_n,true},
{"PersonalNotes","Keyboard",eKey_p,true},

{"WheelUp","MouseButton",eMButton_WheelUp,true},
{"WheelDown","MouseButton",eMButton_WheelDown,true},

{"Flashlight","Keyboard",eKey_f,true},
{"GlowStick","Keyboard",eKey_g,true},

{"Escape","Keyboard",eKey_ESCAPE,false},
{"Enter","Keyboard",eKey_RETURN,false},
{"MouseClick","MouseButton",eMButton_Left,false},
{"MouseClickRight","MouseButton",eMButton_Right,false},

{"RightClick","MouseButton",eMButton_Right,false},
{"LeftClick","MouseButton",eMButton_Left,false},

{"One","Keyboard",eKey_1,false},
{"Two","Keyboard",eKey_2,false},
{"Three","Keyboard",eKey_3,false},
{"Four","Keyboard",eKey_4,false},
{"Five","Keyboard",eKey_5,false},
{"Six","Keyboard",eKey_6,false},
{"Seven","Keyboard",eKey_7,false},
{"Eight","Keyboard",eKey_8,false},
{"Nine","Keyboard",eKey_9,false},

//Debug:
{"ResetGame","Keyboard",eKey_F1,false},
{"SaveGame","Keyboard",eKey_F4,false},
{"LoadGame","Keyboard",eKey_F5,false},
#ifdef __APPLE__
{"QuitGame","Keyboard",eKeyModifier_META | eKey_q,false},
#endif
//{"LockInput","Keyboard",eKey_k,false},
{"Screenshot","Keyboard",eKey_F12,false},

//{"Hit","Keyboard",eKey_h,false},
//{"Log","Keyboard",eKey_l,false},
//{"Taunt","Keyboard",eKey_t,false},
{"PrintLog","Keyboard",eKey_l,false},

{"","",0}
};

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cButtonHandler::cButtonHandler(cInit *apInit)  : iUpdateable("ButtonHandler")
{
	mpInit = apInit;

	mpInput = mpInit->mpGame->GetInput();
	mpLowLevelGraphics = mpInit->mpGame->GetGraphics()->GetLowLevel();
	if(mpInit->mbHasHaptics)
		mpLowLevelHaptic = mpInit->mpGame->GetHaptic()->GetLowLevel();
	else
		mpLowLevelHaptic = NULL;

	
	mState = eButtonHandlerState_Game;
	mbSnapTurnReady = false;
	mbVRCrouchButtonLatched = false;
	mbVRPhysicalCrouch = false;
	mbVRCrouchApplied = false;
	mbVRStandingHeightKnown = false;
	mfVRStandingHeight = 0.0f;

	mlNumOfActions =0;

	// INIT ALL ACTIONS USED
	cButtonHandlerAction *pBHAction = &gvDefaultActions[0];
	while(pBHAction->msName != "")
	{
		tString sName = pBHAction->msName;
		tString sType = mpInit->mpConfig->GetString("Keys",sName+"_Type",pBHAction->msType);
		tString sVal = mpInit->mpConfig->GetString("Keys",sName+"_Val",cString::ToString(pBHAction->mlVal));

		iAction *pAction = ActionFromTypeAndVal(sName, sType, sVal);
		if(pAction)
		{
			mpInput->AddAction(pAction);
		}
		else
		{
			Warning("Couldn't create action from '%s' and %d\n",pBHAction->msType.c_str(),
																pBHAction->mlVal);
		}
		
		++pBHAction;
		++mlNumOfActions;
	}


	// LOAD SETTINGS
	mfMouseSensitivity = mpInit->mpConfig->GetFloat("Controls","MouseSensitivity",1.0f);
	mbInvertMouseY = mpInit->mpConfig->GetBool("Controls","InvertMouseY",false);
	mbToggleCrouch = mpInit->mpConfig->GetBool("Controls","ToggleCrouch",true);
}

//-----------------------------------------------------------------------

cButtonHandler::~cButtonHandler(void)
{
	
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cButtonHandler::ChangeState(eButtonHandlerState aState)
{
	mState = aState;
}

//-----------------------------------------------------------------------

void cButtonHandler::OnStart()
{
	mpPlayer = mpInit->mpPlayer;
}

//-----------------------------------------------------------------------

void cButtonHandler::OnPostSceneDraw()
{
	if(mpInit->mpGame->GetGraphics()->GetRenderer3D()->GetDebugFlags() & eRendererDebugFlag_LogRendering)
	{
		Log("-------------- STOP RENDERING LOG ------------------------\n");
		mpInit->mpGame->GetGraphics()->GetRenderer3D()->SetDebugFlags(0);
	}
}

void cButtonHandler::Update(float afTimeStep)
{
	static bool bLockState = true;


  bool vrUIContext = mState != eButtonHandlerState_Game ||
    mpInit->mpDeathMenu->IsActive() ||
    mpPlayer->IsDead() ||
    mpInit->mpNumericalPanel->IsActive() ||
    mpInit->mpNotebook->IsActive() ||
    mpInit->mpInventory->IsActive();

  const eSteamVRInputContext vrInputContext =
    vrUIContext ? eSteamVRInputContext_UI : eSteamVRInputContext_Gameplay;
  bool actionInputActive = mpInit->mpGame->vr_input.Update(
    vrInputContext,
    mpInit->mpGame->vr_left_hand,
    mpInit->mpGame->vr_right_hand);

  if (!actionInputActive) {
    mpInit->mpGame->vr_right_hand.UpdateButtonState();
    mpInit->mpGame->vr_left_hand.UpdateButtonState();
    mpInit->mpGame->vr_input.UpdateLegacyState(
      vrInputContext,
      mpInit->mpGame->vr_left_hand,
      mpInit->mpGame->vr_right_hand);
  }

  const cVRInputState& vrInput = mpInit->mpGame->vr_input.GetState();

  if(vrInput.recenter.justPressed)
  {
    mpInit->mpGame->vr_tracking.RecenterOrientation();
	if(vrUIContext || !mpInit->mpGame->GetScene()->GetDrawScene())
		mpInit->mpGame->GetScene()->ResetVRMainUIAnchor();
    mpInit->mpGame->vr_old_head_tracking_pose =
      mpInit->mpGame->vr_tracking.GetHeadTrackingPose();
    Log(" [VR comfort +%lu ms] recentered orientation; world yaw %.1f deg.\n",
      GetApplicationTime(),
      cMath::ToDeg(mpInit->mpGame->vr_tracking.GetWorldYaw()));
  }

  if(!vrUIContext && mpPlayer->IsActive() && !mpPlayer->GetLookAt()->IsActive())
  {
    UpdateVRTurn(vrInput, afTimeStep);
	UpdateVRCrouch(vrInput);
  }
  else
  {
    // Returning from a menu or scripted look must require the stick to pass
    // through centre before snap turning can fire again.
    mbSnapTurnReady = false;
  }

  if(actionInputActive && vrUIContext && vrInput.uiSelect.justPressed)
  {
    const eVRHapticHand pointerHand = mpInit->mpGame->vr_right_hand.IsPoseValid()
      ? eVRHapticHand_Right : eVRHapticHand_Left;
    cVRHaptics::Play(mpInit, eVRHapticEvent_UISelect, pointerHand);
  }
	///////////////////////////////////
	// GLOBAL Key Strokes
	///////////////////////////////////
	if(mpInput->BecameTriggerd("QuitGame"))
	{
		mpInit->mpGame->Exit();
	}
	if(mpInput->BecameTriggerd("Screenshot"))
	{
		int lCount = 1;
		tString sFileName = "screenshot000.bmp";
		while(FileExists(cString::To16Char(sFileName)))
		{
			sFileName = "screenshot";
            if(lCount < 10)sFileName+= "00";
			else if(lCount < 100)sFileName+= "0";
			sFileName += cString::ToString(lCount);
			sFileName += ".bmp";
			++lCount;
		}

		mpInit->mpGame->GetGraphics()->GetLowLevel()->SaveScreenToBMP(sFileName);
	}
	if(mpInput->BecameTriggerd("LockInput"))
	{
#ifndef WIN32
		bLockState = !bLockState;
		mpInit->mpGame->GetInput()->GetLowLevel()->LockInput(bLockState);
#endif
	}
	///////////////////////////////////
	// DEMO END TEXT
	///////////////////////////////////
	if(mState == eButtonHandlerState_DemoEndText)
	{
		if(	mpInput->BecameTriggerd("Escape"))	
			mpInit->mpDemoEndText->OnButtonDown();
		if(mpInput->BecameTriggerd("LeftClick"))
			mpInit->mpDemoEndText->OnMouseDown(eMButton_Left);
		if(mpInput->BecameTriggerd("RightClick"))
			mpInit->mpDemoEndText->OnMouseDown(eMButton_Right);
	}
	///////////////////////////////////
	// CREDITS STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_Credits)
	{
		if(	mpInput->BecameTriggerd("Escape"))	
			mpInit->mpCredits->OnButtonDown();
		if(mpInput->BecameTriggerd("LeftClick"))
			mpInit->mpCredits->OnMouseDown(eMButton_Left);
		if(mpInput->BecameTriggerd("RightClick"))
			mpInit->mpCredits->OnMouseDown(eMButton_Right);
	}
	///////////////////////////////////
	// PRE MENU STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_PreMenu)
	{
		if(	mpInput->BecameTriggerd("Escape") || vrInput.uiClose.justPressed
      || vrInput.uiBack.justPressed)
			mpInit->mpPreMenu->OnButtonDown();
		if(mpInput->BecameTriggerd("LeftClick")
      || vrInput.uiSelect.justPressed)
			mpInit->mpPreMenu->OnMouseDown(eMButton_Left);
		if(mpInput->BecameTriggerd("RightClick"))
			mpInit->mpPreMenu->OnMouseDown(eMButton_Right);
	}
	///////////////////////////////////
	// MAP LOAD TEXT STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_MapLoadText)
	{
		if(	mpInput->BecameTriggerd("Escape") ||
			mpInput->BecameTriggerd("RightClick") ||
			mpInput->BecameTriggerd("LeftClick") ||
			vrInput.uiClose.justPressed ||
      vrInput.uiSelect.justPressed ||
      vrInput.uiBack.justPressed)
		{
			if(!mpInit->mpMapLoadText->IsLoading())
				mpInit->mpMapLoadText->SetActive(false);
		}
	}
	///////////////////////////////////
	// MAIN MENU BUTTON STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_MainMenu)
	{
		if(mpInput->BecameTriggerd("Escape") || vrInput.uiClose.justPressed
      || vrInput.uiBack.justPressed)
		{
			mpInit->mpMainMenu->Exit();
		}

		if(	mpInput->BecameTriggerd("RightClick") ||
			(mpInit->mbHasHaptics && mpInput->BecameTriggerd("MouseClickRight")) ||
		   vrInput.uiDrag.justPressed)
		{
			mpInit->mpMainMenu->OnMouseDown(eMButton_Right);
			mpInput->BecameTriggerd("Examine");
		}
		if(mpInput->WasTriggerd("RightClick") ||
		  vrInput.uiDrag.justReleased)
		{
			mpInit->mpMainMenu->OnMouseUp(eMButton_Right);
		}
		if(mpInput->DoubleTriggerd("RightClick",0.15f))
		{
			mpInit->mpMainMenu->OnMouseDoubleClick(eMButton_Right);
		}

		if(	mpInput->BecameTriggerd("LeftClick") || 
			(mpInit->mbHasHaptics && mpInput->BecameTriggerd("MouseClick")) ||
      vrInput.uiSelect.justPressed)
		{
			mpInit->mpMainMenu->OnMouseDown(eMButton_Left);
			mpInput->BecameTriggerd("Interact");
		}
		if(mpInput->WasTriggerd("LeftClick") ||
      vrInput.uiSelect.justReleased)
		{
			mpInit->mpMainMenu->OnMouseUp(eMButton_Left);
		}
		if(mpInput->DoubleTriggerd("LeftClick",0.15f))
		{
			mpInit->mpMainMenu->OnMouseDoubleClick(eMButton_Left);
		}

		if(mpInit->mbHasHaptics)
		{
			mpInit->mpMainMenu->AddMousePos(mpLowLevelHaptic->GetRelativeVirtualMousePos() * mfMouseSensitivity);

			cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
			mpInit->mpMainMenu->AddMousePos(vRel * mfMouseSensitivity);
		}
		else
		{
			/// Mouse Movement
			cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
			mpInit->mpMainMenu->AddMousePos(vRel * mfMouseSensitivity);
		}
		
	}
	///////////////////////////////////
	// INTRO BUTTON STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_Intro)
	{
		if(mpInput->BecameTriggerd("Escape") || vrInput.uiClose.justPressed
      || vrInput.uiBack.justPressed
      || vrInput.uiDrag.justPressed)
		{
			mpInit->mpIntroStory->Exit();
		}
	}
	///////////////////////////////////
	// GAME BUTTON STATE
	///////////////////////////////////
	else if(mState == eButtonHandlerState_Game)
	{
		///////////////////////////////////////
		// Global ////////////////////
		/*if(mpInput->BecameTriggerd("ResetGame"))
		{
			mpInit->ResetGame(true);
			mpInit->mpMapHandler->Load(	mpInit->msStartMap,mpInit->msStartLink);
		}*/
		if(mpInit->mbAllowQuickSave)
		{
			if(mpInput->BecameTriggerd("SaveGame"))
			{
				mpInit->mpSaveHandler->AutoSave(_W("auto"),5);
			}
			if(mpInput->BecameTriggerd("LoadGame"))
			{
				mpInit->mpSaveHandler->AutoLoad(_W("auto"));
			}
		}
		if(mpInput->BecameTriggerd("PrintLog"))
		{
			Log("-------------- START RENDERING LOG ------------------------\n");
			mpInit->mpGame->GetGraphics()->GetRenderer3D()->SetDebugFlags(eRendererDebugFlag_LogRendering);
		}
		//Check if no jump is pressed always.
		bool bPlayerStateIsActive = false;
		///////////////////////////////////////
		// Death menu ////////////////////
		if(mpInit->mpDeathMenu->IsActive())
		{
			if(mpInput->BecameTriggerd("Escape") || vrInput.uiClose.justPressed ||
        vrInput.uiBack.justPressed)
			{
				mpInit->mpGame->GetUpdater()->Reset();
				mpInit->mpMainMenu->SetActive(true);
			}

			if (mpInput->BecameTriggerd("RightClick"))
			{
				mpInit->mpDeathMenu->OnMouseDown(eMButton_Right);
				mpInput->BecameTriggerd("Examine");
			}

			if(mpInput->BecameTriggerd("LeftClick") ||
        vrInput.uiSelect.justPressed)
			{
				mpInit->mpDeathMenu->OnMouseDown(eMButton_Left);
				mpInput->BecameTriggerd("Interact");
			}
			if(mpInput->WasTriggerd("LeftClick") ||
        vrInput.uiSelect.justReleased)
			{
				mpInit->mpDeathMenu->OnMouseUp(eMButton_Left);
			}

			if(mpInit->mbHasHaptics)
			{
				mpInit->mpDeathMenu->AddMousePos(mpLowLevelHaptic->GetRelativeVirtualMousePos() * mfMouseSensitivity);
			}
			else
			{
				/// Mouse Movement
				cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
				mpInit->mpDeathMenu->AddMousePos(vRel * mfMouseSensitivity);
			}
		}
		///////////////////////////////////////
		// Death ////////////////////
		else if(mpPlayer->IsDead())
		{
			if(mpInput->BecameTriggerd("Escape") || vrInput.uiClose.justPressed ||
        vrInput.uiBack.justPressed)
			{
				mpInit->mpMainMenu->SetActive(true);
			}
		}
		///////////////////////////////////////
		// Numerical panel ////////////////////
		else if(mpInit->mpNumericalPanel->IsActive())
		{
			if(mpInput->BecameTriggerd("Inventory") || mpInput->BecameTriggerd("Escape") ||
        vrInput.uiClose.justPressed || vrInput.uiBack.justPressed)
			{
				mpInit->mpNumericalPanel->OnExit();
			}
			if(mpInput->BecameTriggerd("RightClick"))
			{
				mpInit->mpNumericalPanel->OnExit();
			}

			if(mpInput->BecameTriggerd("LeftClick") ||
        vrInput.uiSelect.justPressed)
			{
				mpInit->mpNumericalPanel->OnMouseDown(eMButton_Left);
				mpInput->BecameTriggerd("Interact");
			}
			if(mpInput->WasTriggerd("LeftClick") ||
        vrInput.uiSelect.justReleased)
			{
				mpInit->mpNumericalPanel->OnMouseUp(eMButton_Left);
			}

			if(mpInit->mbHasHaptics)
			{
				mpInit->mpNumericalPanel->AddMousePos(mpLowLevelHaptic->GetRelativeVirtualMousePos() * mfMouseSensitivity);
			}
			else
			{
				/// Mouse Movement
				cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
				mpInit->mpNumericalPanel->AddMousePos(vRel * mfMouseSensitivity);
			}
		}
		///////////////////////////////////////
		// Notebook ////////////////////
		else if(mpInit->mpNotebook->IsActive())
		{
			if(mpInput->BecameTriggerd("Inventory") || mpInput->BecameTriggerd("Escape") ||
        vrInput.uiClose.justPressed)
			{
				mpInit->mpNotebook->OnExit();
			}
			
			if(mpInput->BecameTriggerd("LeftClick") ||
        vrInput.uiSelect.justPressed)
			{
				mpInit->mpNotebook->OnMouseDown(eMButton_Left);

				mpInput->BecameTriggerd("Interact");
			}

			if(mpInput->BecameTriggerd("NoteBook"))
			{
				mpInit->mpNotebook->OnExit();
			}
			if(mpInput->BecameTriggerd("PersonalNotes"))
			{
				cStateMachine *pStateMachine = mpInit->mpNotebook->GetStateMachine();
				if(pStateMachine->CurrentState()->GetId() == eNotebookState_TaskList)
				{
					pStateMachine->ChangeState(eNotebookState_Front);
					mpInit->mpNotebook->OnExit();
				}
				else
				{
					pStateMachine->ChangeState(eNotebookState_TaskList);
				}
			}
			
			if(mpInit->mbHasHaptics)
			{
				mpInit->mpNotebook->AddMousePos(mpLowLevelHaptic->GetRelativeVirtualMousePos() * mfMouseSensitivity);
			}
			else
			{
				/// Mouse Movement
				cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
				mpInit->mpNotebook->AddMousePos(vRel * mfMouseSensitivity);
			}
		}
		///////////////////////////////////////
		// Inventory ////////////////////
		else if(mpInit->mpInventory->IsActive())
		{
			////////////////////////////
			//Normal Input
			if(mpInput->BecameTriggerd("Inventory") || mpInput->BecameTriggerd("Escape") ||
        vrInput.uiClose.justPressed)
			{
				mpInit->mpInventory->OnInventoryDown();
			}
			
			if(mpInput->BecameTriggerd("LeftClick") ||
        vrInput.uiDrag.justPressed)
			{
				mpInit->mpInventory->OnMouseDown(eMButton_Left);

				mpInput->BecameTriggerd("Interact");
			}
			
			if(mpInput->DoubleTriggerd("LeftClick",0.2f))
			{
				mpInit->mpInventory->OnDoubleClick(eMButton_Left);
			}
			if(vrInput.uiSelect.justPressed)
			{
				// R2 activates the highlighted context action when one is open;
				// otherwise it performs the item's default action directly.
				if(mpInit->mpInventory->GetContext()->IsActive())
					mpInit->mpInventory->OnMouseDown(eMButton_Left);
				else
					mpInit->mpInventory->OnDoubleClick(eMButton_Left);
			}
			if(mpInput->WasTriggerd("LeftClick") ||
        vrInput.uiDrag.justReleased)
			{
				mpInit->mpInventory->OnMouseUp(eMButton_Left);
			}

				
			if(mpInput->BecameTriggerd("RightClick") ||
        vrInput.uiBack.justPressed)
			{
				mpInit->mpInventory->OnMouseDown(eMButton_Right);
				
				mpInput->BecameTriggerd("Examine");
			}
			if(mpInput->WasTriggerd("RightClick") ||
        vrInput.uiBack.justReleased)
			{
				mpInit->mpInventory->OnMouseUp(eMButton_Right);
			}
			
			//////////////////////////////
			//Short cut keys
			if(mpInput->BecameTriggerd("One")) mpInit->mpInventory->OnShortcutDown(0);
			if(mpInput->BecameTriggerd("Two")) mpInit->mpInventory->OnShortcutDown(1);
			if(mpInput->BecameTriggerd("Three")) mpInit->mpInventory->OnShortcutDown(2);
			if(mpInput->BecameTriggerd("Four")) mpInit->mpInventory->OnShortcutDown(3);
			if(mpInput->BecameTriggerd("Five")) mpInit->mpInventory->OnShortcutDown(4);
			if(mpInput->BecameTriggerd("Six")) mpInit->mpInventory->OnShortcutDown(5);
			if(mpInput->BecameTriggerd("Seven")) mpInit->mpInventory->OnShortcutDown(6);
			if(mpInput->BecameTriggerd("Eight")) mpInit->mpInventory->OnShortcutDown(7);
			if(mpInput->BecameTriggerd("Nine")) mpInit->mpInventory->OnShortcutDown(8);
			
			if(mpInit->mbHasHaptics)
			{
				cVector2f vRel = mpLowLevelHaptic->GetRelativeVirtualMousePos();
				mpInit->mpInventory->AddMousePos(vRel * mfMouseSensitivity);
			}
			else
			{
				/// Mouse Movement
				cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
				mpInit->mpInventory->AddMousePos(vRel * mfMouseSensitivity);
			}
		}
		else 
		{
			bPlayerStateIsActive = true;

			if(mpInput->BecameTriggerd("Escape") || vrInput.pause.justPressed)
			{
				mpInit->mpMainMenu->SetActive(true);
			}
			if(mpInput->BecameTriggerd("Hit"))
			{
				mpInit->mpPlayer->Damage(20,ePlayerDamageType_BloodSplash);
			}
			/*if(mpInput->BecameTriggerd("Log"))
			{
				mpInit->mpGame->GetPhysics()->SetDebugLog(!mpInit->mpGame->GetPhysics()->GetDebugLog());
			}*/
			if(mpInput->BecameTriggerd("Taunt"))
			{
				for(int i=0;i<10; ++i)
					mpInit->mpGame->GetSound()->GetSoundHandler()->PlayGui("gui_notebook_add_note1",
																		false,0.01f);
				/*cVector3f vPos = mpInit->mpPlayer->GetCharacterBody()->GetPosition();
				cSoundEntity *pSound = mpInit->mpGame->GetScene()->GetWorld3D()->CreateSoundEntity("Taunt","interact_homer",true);
                if(pSound)
				{
					pSound->SetPosition(vPos);
				}*/
			}
			
			if(mpPlayer->IsActive() || mpPlayer->GetState() == ePlayerState_Message)
			{
				if(mpPlayer->IsActive())
				{
					if(mpInput->BecameTriggerd("Inventory") ||
            vrInput.inventory.justPressed)
					{
						mpPlayer->StartInventory();
					}

					if(mpInput->BecameTriggerd("NoteBook") ||
             vrInput.notebook.justPressed)
					{
						mpInit->mpNotebook->SetActive(true);
					}
					if(mpInput->BecameTriggerd("PersonalNotes"))
					{
						mpInit->mpNotebook->SetActive(true);
						mpInit->mpNotebook->GetStateMachine()->ChangeState(eNotebookState_TaskList);
					}
	                
					if(mpInput->BecameTriggerd("Flashlight"))
					{
						mpPlayer->StartFlashLightButton();
					}

					if(mpInput->BecameTriggerd("GlowStick"))
					{
						mpPlayer->StartGlowStickButton();
					}

          if (vrInput.quickLight.justPressed) {
            if (mpPlayer->GetGlowStick()->IsActive()) {
              mpPlayer->StartGlowStickButton();
              mpPlayer->StartFlashLightButton();
            } else if (mpPlayer->GetFlashLight()->IsActive()) {
              mpPlayer->StartFlashLightButton();
            }
            else {
              mpPlayer->StartGlowStickButton();
            }
          }

					///////////////////////////////////////
					// Player Movement ////////////////////
          if (vrInput.moveActive) {
            const cMatrixf headWorldRotation =
              mpInit->mpGame->vr_tracking.GetHeadWorldPose().GetRotation();
            cVector3f headForward = cMath::MatrixInverse(headWorldRotation).GetForward();
            headForward.y = 0.0f;
            headForward = cMath::Vector3Normalize(headForward);

            cVector3f headRight = cMath::MatrixInverse(headWorldRotation).GetRight();
            headRight.y = 0.0f;
            headRight = cMath::Vector3Normalize(headRight);

            mpInit->mpPlayer->vr_moveVec = headForward * -vrInput.moveY;
            mpInit->mpPlayer->vr_moveVec += headRight * vrInput.moveX;
          }

          if (vrInput.jump.justPressed) {
            mpPlayer->Jump();
          }

          if (vrInput.jump.pressed) {
            mpPlayer->SetJumpButtonDown(true);
          } else {
            mpPlayer->SetJumpButtonDown(false);
          }

					if(mpInput->IsTriggerd("Forward"))
					{
						mpPlayer->MoveForwards(1,afTimeStep);
					}
					else if(mpInput->IsTriggerd("Backward"))
					{
						mpPlayer->MoveForwards(-1,afTimeStep);
					}
					else
					{
						mpPlayer->MoveForwards(0,afTimeStep);
					}


					if(mpInput->IsTriggerd("Left"))
					{
						mpPlayer->MoveSideways(-1,afTimeStep);
					}
					else if(mpInput->IsTriggerd("Right"))
					{
						mpPlayer->MoveSideways(1,afTimeStep);
					}
					else
					{
						mpPlayer->MoveSideways(0,afTimeStep);
					}

					if(mpInput->IsTriggerd("LeanLeft"))
					{
						mpPlayer->Lean(-1,afTimeStep);
					}
					else if(mpInput->IsTriggerd("LeanRight"))
					{
						mpPlayer->Lean(1,afTimeStep);
					}
					

					if(mpInput->BecameTriggerd("Jump"))
					{
						mpPlayer->Jump();
					}
					if(mpInput->IsTriggerd("Jump"))
					{
						mpPlayer->SetJumpButtonDown(true);
					}
					
					if(mpInput->BecameTriggerd("Run"))
					{
						mpPlayer->StartRun();
					}
					if(mpInput->WasTriggerd("Run"))
					{
						mpPlayer->StopRun();	
					}

					if(mpInput->BecameTriggerd("Crouch"))
					{
						mpPlayer->StartCrouch();
					}
					if(GetToggleCrouch())
					{
						if(mpInput->WasTriggerd("Crouch"))	mpPlayer->StopCrouch();
					}
					else
					{
						if(mpInput->IsTriggerd("Crouch")==false) mpPlayer->StopCrouch();
					}

					if(mpInput->BecameTriggerd("InteractMode"))
					{
						if(mpInit->mbHasHaptics==false)
						{
							mpPlayer->StartInteractMode();
						}
						else
						{
							//DO nothing for the time being.
						}
					}
					
					//Get the mouse pos and convert it to 0 - 1
					if(mpInit->mbHasHaptics==false)
					{
						cVector2f vRel = mpInput->GetMouse()->GetRelPosition();
						vRel /= mpLowLevelGraphics->GetVirtualSize();

						mpPlayer->AddYaw(vRel.x * mfMouseSensitivity);
						mpPlayer->AddPitch(vRel.y * mfMouseSensitivity);
					}
				}

				///////////////////////////////////////
				// Player Interaction /////////////////
				if(	mpInput->BecameTriggerd("Interact") || vrInput.interact.justPressed)
				{
					mpPlayer->StartInteract();
					mpInput->BecameTriggerd("LeftClick");
				}
				if(	mpInput->WasTriggerd("Interact") || vrInput.interact.justReleased)
				{
					mpPlayer->StopInteract();
				}
				if(	mpInput->BecameTriggerd("Examine") || vrInput.examine.justPressed)
				{
					mpPlayer->StartExamine();
				}
				if(mpInput->WasTriggerd("Examine"))
				{
					mpPlayer->StopExamine();
				}
				if(mpInput->BecameTriggerd("Holster") || vrInput.holster.justPressed)
				{
					mpPlayer->StartHolster();
				}

				if(mpPlayer->IsActive())
				{
					if(mpInput->BecameTriggerd("One")) mpPlayer->StartInventoryShortCut(0);
					if(mpInput->BecameTriggerd("Two")) mpPlayer->StartInventoryShortCut(1);
					if(mpInput->BecameTriggerd("Three")) mpPlayer->StartInventoryShortCut(2);
					if(mpInput->BecameTriggerd("Four")) mpPlayer->StartInventoryShortCut(3);
					if(mpInput->BecameTriggerd("Five")) mpPlayer->StartInventoryShortCut(4);
					if(mpInput->BecameTriggerd("Six")) mpPlayer->StartInventoryShortCut(5);
					if(mpInput->BecameTriggerd("Seven")) mpPlayer->StartInventoryShortCut(6);
					if(mpInput->BecameTriggerd("Eight")) mpPlayer->StartInventoryShortCut(7);
					if(mpInput->BecameTriggerd("Nine")) mpPlayer->StartInventoryShortCut(8);
				}
			}
		}
		if(mpInput->IsTriggerd("Jump")==false || bPlayerStateIsActive==false)
		{
			mpPlayer->SetJumpButtonDown(false);
		}
	}
}

//-----------------------------------------------------------------------

void cButtonHandler::UpdateVRTurn(const cVRInputState& aInputState, float afTimeStep)
{
	const eVRTurnMode turnMode = mpInit->mVRSettings.GetTurnMode();
	if(turnMode == eVRTurnMode_Disabled || !aInputState.turnActive)
	{
		mbSnapTurnReady = false;
		return;
	}

	const float fDeadZone = mpInit->mVRSettings.GetTurnDeadZone();
	const float fAbsoluteX = std::abs(aInputState.turnX);
	if(fAbsoluteX <= fDeadZone)
	{
		mbSnapTurnReady = true;
		return;
	}

	float fTurnAmount = (fAbsoluteX - fDeadZone) / (1.0f - fDeadZone);
	if(fTurnAmount > 1.0f) fTurnAmount = 1.0f;
	if(aInputState.turnX < 0.0f) fTurnAmount = -fTurnAmount;

	if(turnMode == eVRTurnMode_Snap)
	{
		const float fActivationThreshold = 0.65f;
		if(!mbSnapTurnReady || std::abs(fTurnAmount) < fActivationThreshold) return;

		const float fDirection = fTurnAmount > 0.0f ? -1.0f : 1.0f;
		const float fYawDelta = cMath::ToRad(mpInit->mVRSettings.GetSnapTurnAngle()) * fDirection;
		mpInit->mpGame->vr_tracking.AddWorldYaw(fYawDelta);
		mbSnapTurnReady = false;
		Log(" [VR comfort +%lu ms] snap turn %.0f deg; world yaw %.1f deg.\n",
			GetApplicationTime(), cMath::ToDeg(fYawDelta),
			cMath::ToDeg(mpInit->mpGame->vr_tracking.GetWorldYaw()));
	}
	else
	{
		// Smooth turning may continue while held, but only after the stick has
		// crossed the dead zone once since entering gameplay.
		if(!mbSnapTurnReady) return;
		const float fYawDelta = -fTurnAmount *
			cMath::ToRad(mpInit->mVRSettings.GetSmoothTurnSpeed()) * afTimeStep;
		mpInit->mpGame->vr_tracking.AddWorldYaw(fYawDelta);
	}
}

//-----------------------------------------------------------------------

void cButtonHandler::UpdateVRCrouch(const cVRInputState& aInputState)
{
	const eVRCrouchMode crouchMode = mpInit->mVRSettings.GetCrouchMode();
	const bool buttonEnabled = crouchMode != eVRCrouchMode_Physical;
	const bool physicalEnabled = crouchMode != eVRCrouchMode_Button;

	if(buttonEnabled && aInputState.crouch.justPressed)
	{
		mbVRCrouchButtonLatched = !mbVRCrouchButtonLatched;
		Log(" [VR comfort +%lu ms] crouch button latch %s.\n",
			GetApplicationTime(), mbVRCrouchButtonLatched ? "on" : "off");
	}
	else if(!buttonEnabled)
	{
		mbVRCrouchButtonLatched = false;
	}

	if(physicalEnabled)
	{
		const float headHeight =
			mpInit->mpGame->vr_tracking.GetHeadTrackingPose().GetTranslation().y;
		const bool plausibleHeight = headHeight > 0.60f && headHeight < 2.50f;
		if(plausibleHeight)
		{
			if(!mbVRStandingHeightKnown)
			{
				mfVRStandingHeight = headHeight;
				mbVRStandingHeightKnown = true;
				Log(" [VR comfort +%lu ms] standing HMD height calibrated at %.2f m.\n",
					GetApplicationTime(), mfVRStandingHeight);
			}
			else if(!mbVRPhysicalCrouch && !mbVRCrouchButtonLatched &&
				headHeight > mfVRStandingHeight)
			{
				// Let an initial low sample settle upwards, but never drift down
				// into a false standing height while the player is crouching.
				mfVRStandingHeight = headHeight;
			}

			const float enterHeight = mfVRStandingHeight -
				mpInit->mVRSettings.GetPhysicalCrouchDepth();
			const float exitHeight = enterHeight + 0.08f;
			const bool wasPhysicalCrouch = mbVRPhysicalCrouch;
			if(!mbVRPhysicalCrouch && headHeight <= enterHeight)
				mbVRPhysicalCrouch = true;
			else if(mbVRPhysicalCrouch && headHeight >= exitHeight)
				mbVRPhysicalCrouch = false;

			if(wasPhysicalCrouch != mbVRPhysicalCrouch)
			{
				Log(" [VR comfort +%lu ms] physical crouch %s at %.2f m "
					"(standing %.2f m, threshold %.2f m).\n",
					GetApplicationTime(), mbVRPhysicalCrouch ? "entered" : "left",
					headHeight, mfVRStandingHeight, enterHeight);
			}
		}
	}
	else
	{
		mbVRPhysicalCrouch = false;
	}

	bool crouchWanted = false;
	if(crouchMode == eVRCrouchMode_Physical) crouchWanted = mbVRPhysicalCrouch;
	else if(crouchMode == eVRCrouchMode_Button) crouchWanted = mbVRCrouchButtonLatched;
	else crouchWanted = mbVRPhysicalCrouch || mbVRCrouchButtonLatched;

	if(crouchWanted)
	{
		if(mpPlayer->GetMoveState() != ePlayerMoveState_Jump &&
			mpPlayer->GetMoveState() != ePlayerMoveState_Crouch)
		{
			const ePlayerMoveState previousState = mpPlayer->GetMoveState();
			mpPlayer->ChangeMoveState(ePlayerMoveState_Crouch);
			Log(" [VR comfort +%lu ms] VR crouch applied; move state %d -> %d.\n",
				GetApplicationTime(), (int)previousState, (int)mpPlayer->GetMoveState());
		}
		if(mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
			mbVRCrouchApplied = true;
	}
	else if(mbVRCrouchApplied)
	{
		if(mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
		{
			mpPlayer->ChangeMoveState(ePlayerMoveState_Walk);
			if(mpPlayer->GetMoveState() == ePlayerMoveState_Crouch)
				return; // A low ceiling prevented standing; retry next frame.
		}
		mbVRCrouchApplied = false;
		Log(" [VR comfort +%lu ms] VR crouch released.\n", GetApplicationTime());
	}

// Physical crouching already moves the user's real viewpoint. A crouch
	// requested by the button lowers the eye height by the configured crouch
	// depth. The original game's move state camera drop (GetHeightAdd, about
	// -0.7 m) is a full crouch, far deeper than intended in VR and a clipping
	// hazard; the move state is still entered so the smaller collider applies.
const float postureOffset = mbVRCrouchApplied && !mbVRPhysicalCrouch
		? -mpInit->mVRSettings.GetPhysicalCrouchDepth() : 0.0f;
	mpInit->mpGame->vr_tracking.SetPostureOffset(postureOffset);

	// Temporary crouch diagnostic; remove once the crouch investigation closes.
	if(mbVRCrouchApplied || mbVRPhysicalCrouch || mbVRCrouchButtonLatched)
	{
		static unsigned long lLastCrouchDebugTime = 0;
		const unsigned long lNow = GetApplicationTime();
		if(lNow - lLastCrouchDebugTime >= 1000)
		{
			lLastCrouchDebugTime = lNow;
			const float fRawHmd = mpInit->mpGame->vr_tracking.GetHeadTrackingPose().GetTranslation().y;
			Log(" [VR crouch-debug +%lu ms] rawHmd=%.3f standing=%.3f physDelta=%.3f "
				"buttonLatched=%d physCrouch=%d applied=%d posture=%.3f "
				"trackingOriginY=%.3f feetY=%.3f headWorldY=%.3f\n",
				lNow, fRawHmd, mfVRStandingHeight, mfVRStandingHeight - fRawHmd,
				(int)mbVRCrouchButtonLatched, (int)mbVRPhysicalCrouch, (int)mbVRCrouchApplied,
				postureOffset,
				mpInit->mpGame->vr_tracking.GetPlayerWorldPose().GetTranslation().y,
				mpPlayer->GetCharacterBody()->GetFeetPosition().y,
				mpInit->mpGame->vr_tracking.GetHeadWorldPose().GetTranslation().y);
		}
	}
}

//-----------------------------------------------------------------------

void cButtonHandler::Reset()
{
	mbSnapTurnReady = false;
	mbVRCrouchButtonLatched = false;
	mbVRPhysicalCrouch = false;
	mbVRCrouchApplied = false;
	mbVRStandingHeightKnown = false;
	mfVRStandingHeight = 0.0f;
	mpInit->mpGame->vr_tracking.SetPostureOffset(0.0f);
}

//-----------------------------------------------------------------------

void cButtonHandler::OnExit()
{
	// SAVE SETTINGS
	Log("  Saving to config\n");
	mpInit->mpConfig->SetFloat("Controls","MouseSensitivity",mfMouseSensitivity);
	mpInit->mpConfig->SetBool("Controls","InvertMouseY",mbInvertMouseY);
	mpInit->mpConfig->SetBool("Controls","ToggleCrouch",mbToggleCrouch);

	// SAVE KEYS
	Log("  Saving keys\n");
	for(int i=0; i< mlNumOfActions; ++i)
	{
		//Log(" Action %s\n",gvDefaultActions[i].msName.c_str());
		
		iAction *pAction = mpInput->GetAction(gvDefaultActions[i].msName);
		tString sType="", sVal="";
		TypeAndValFromAction(pAction,&sType,&sVal);

		//Log(" type %s val: %s\n",sType.c_str(),sVal.c_str());
		
		mpInit->mpConfig->SetString("Keys",gvDefaultActions[i].msName+"_Type",sType);
		mpInit->mpConfig->SetString("Keys",gvDefaultActions[i].msName+"_Val",sVal);
	}
}

//-----------------------------------------------------------------------

void cButtonHandler::SetDefaultKeys()
{
	cButtonHandlerAction *pBHAction = &gvDefaultActions[0];
	while(pBHAction->msName != "")
	{
		tString sName = pBHAction->msName;
		tString sType = pBHAction->msType;
		tString sVal = cString::ToString(pBHAction->mlVal);
		
		iAction *pAction = ActionFromTypeAndVal(sName, sType, sVal);
		
		if(pAction)
		{
			mpInput->DestroyAction(sName);
			mpInput->AddAction(pAction);
		}
		else
		{
			Warning("Couldn't create action from '%s' and %d\n",pBHAction->msType.c_str(),
																	pBHAction->mlVal);
		}

		++pBHAction;
	}
}

//-----------------------------------------------------------------------

tString cButtonHandler::GetActionName(const tString &asInputName,const tString &asSkipAction)
{
	cButtonHandlerAction *pBHAction = &gvDefaultActions[0];
	while(pBHAction->msName != "")
	{
		tString sName = pBHAction->msName;
		tString sType = pBHAction->msType;
		tString sVal = cString::ToString(pBHAction->mlVal);

		iAction *pAction = mpInput->GetAction(sName);
		
		if(asSkipAction != sName && pAction && pAction->GetInputName() == asInputName) return sName;

		//If at last player action, skip the rest.
		if(sName == gsLastPlayerAction) return "";

		++pBHAction;
	}

	return "";
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

iAction* cButtonHandler::ActionFromTypeAndVal(const tString& asName,const tString& asType, const tString& asVal)
{
	//Log("Action %s from %s\n",asName.c_str(),asType.c_str());

	if(asType == "Keyboard")
	{
		return hplNew( cActionKeyboard, (asName,mpInit->mpGame->GetInput(),(eKey)cString::ToInt(asVal.c_str(),0)) );
	}
	else if(asType == "MouseButton" || asType == "HapticDeviceButton")
	{
		if(mpInit->mbHasHaptics && asName != "MouseClick")
		{
			int lNum = cString::ToInt(asVal.c_str(),0);
			if(lNum==2)lNum = 2; 
			else if(lNum==1)lNum = 1;
			return hplNew( cActionHaptic, (asName,mpInit->mpGame->GetHaptic(),lNum) );
		}
		else
		{
			return hplNew( cActionMouseButton, (asName,mpInit->mpGame->GetInput(),(eMButton)cString::ToInt(asVal.c_str(),0)) );
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------

void cButtonHandler::TypeAndValFromAction(iAction *apAction, tString *apType, tString *apVal)
{
	if(apAction)
	{
		*apType = apAction->GetInputType();

		if(apAction->GetInputType() == "Keyboard")
		{
			cActionKeyboard *pKeyAction = static_cast<cActionKeyboard*>(apAction);
			*apVal = cString::ToString((int)pKeyAction->GetKey() | (int)pKeyAction->GetModifier());
		}
		else if(apAction->GetInputType() == "MouseButton" ||
				apAction->GetInputType() == "HapticDeviceButton")
		{
			if(mpInit->mbHasHaptics && apAction->GetName() != "MouseClick")
			{
				cActionHaptic *pHapticAction = static_cast<cActionHaptic*>(apAction);
				*apVal = cString::ToString(pHapticAction->GetButton());
				if(*apVal=="2")*apVal = "2"; 
				else if(*apVal=="1")*apVal = "1";
			}
			else
			{
				cActionMouseButton *pMouseAction = static_cast<cActionMouseButton*>(apAction);
				*apVal = cString::ToString((int)pMouseAction->GetButton());
			}
		}
	}
	else
	{
		*apVal = "";
		*apType = "";
	}
}

//-----------------------------------------------------------------------
