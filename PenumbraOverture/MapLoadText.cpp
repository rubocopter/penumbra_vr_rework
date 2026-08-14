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
#include "MapLoadText.h"

#include "Init.h"
#include "GraphicsHelper.h"
#include "ButtonHandler.h"
#include "MapHandler.h"
#include "SaveHandler.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cMapLoadText::cMapLoadText(cInit *apInit)  : iUpdateable("MapLoadText")
{
	mpInit = apInit;
	mpDrawer = mpInit->mpGame->GetGraphics()->GetDrawer();

	//Load graphics
	//mpGfxMouse = mpDrawer->CreateGfxObject("player_crosshair_pointer.bmp","diffalpha2d");
	
	//load fonts
	mpFont = mpInit->mpGame->GetResources()->GetFontManager()->CreateFontData("verdana.fnt");
	mpTextFont = mpInit->mpGame->GetResources()->GetFontManager()->CreateFontData("font_computer.fnt");

	Reset();
}

//-----------------------------------------------------------------------

cMapLoadText::~cMapLoadText(void)
{
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////


//-----------------------------------------------------------------------

void cMapLoadText::Reset()
{
	mpBack = NULL;
	mbActive = false;
	mbLoading = false;
	mbMapLoadPending = false;
	mbLoadingFramePresented = false;
	mbShowContinueText = false;
	mfAlpha = 0;
	mfAlphaAdd = 1;
}

//-----------------------------------------------------------------------

void cMapLoadText::OnPostSceneDraw()
{
	mpInit->mpGraphicsHelper->ClearScreen(cColor(0,0));
	if(mpBack)
		mpInit->mpGraphicsHelper->DrawTexture(mpBack,0,cVector3f(800,600,0),cColor(1,1));
}

//-----------------------------------------------------------------------


void cMapLoadText::OnDraw()
{
	DrawText(mbLoading);
}

//-----------------------------------------------------------------------

void cMapLoadText::DrawText(bool abLoading)
{
	//mpTextFont->DrawWordWrap(	cVector3f(25,25,25),750,17,15,cColor(0.75f, 1, 0.75f, 1),
	//							eFontAlign_Left,kTranslate(msTextCat, msTextEntry));

	cVector3f vStart(25,250.0f - 17.0f* (float)mvRows.size()/2.0f,15);
	for(size_t i=0; i< mvRows.size(); ++i)
	{
		mpTextFont->Draw(vStart + cVector3f(0,17.0f* (float)i,0),15,cColor(1,1),//cColor(0.75f, 1, 0.75f, 1),
						eFontAlign_Left,mvRows[i].c_str());
	}

	if(abLoading)
	{
		mpTextFont->Draw(cVector3f(400,550,25),17,cColor(0.75f, 0.75f, 0.75f, 1),
			eFontAlign_Center,kTranslate("LoadTexts", "Loading").c_str());
	}
	else if(mbShowContinueText)
	{
		tWString wsText = kTranslate("LoadTexts", "ClickToContinue");

		mpTextFont->Draw(cVector3f(400,550,25),
			17,cColor(0.75f*mfAlpha, 1, 0.75f*mfAlpha, 1),
			eFontAlign_Center,wsText.c_str());

		mpTextFont->Draw(cVector3f(401+ 10*sin(mfAlpha*kPi2f),551,23),
			17,cColor(0.1f, 0.1f, 0.1f, 0.7f),
			eFontAlign_Center,wsText.c_str());
		mpTextFont->Draw(cVector3f(399+ -10*sin(mfAlpha*kPi2f),549,23),
			17,cColor(0.1f, 0.1f, 0.1f, 0.7f),
			eFontAlign_Center,wsText.c_str());

	}
}

//-----------------------------------------------------------------------

void cMapLoadText::Update(float afTimeStep)
{
	if(mbMapLoadPending && mbLoadingFramePresented)
	{
		mbMapLoadPending = false;
		if(mpInit->mpSaveHandler->IsGameLoadPending())
			mpInit->mpSaveHandler->CompletePendingGameLoad();
		else
			mpInit->mpMapHandler->CompletePendingMapChange();
	}

	mfAlpha += mfAlphaAdd*afTimeStep;
	if(mfAlphaAdd <0)
	{
		if(mfAlpha <=0){
			mfAlpha =0;
			mfAlphaAdd = -mfAlphaAdd;
		}
	}
	else
	{
		if(mfAlpha >=1){
			mfAlpha =1;
			mfAlphaAdd = -mfAlphaAdd;
		}
	}
}

//-----------------------------------------------------------------------

void cMapLoadText::OnPostBufferSwap()
{
	if(mbMapLoadPending)
		mbLoadingFramePresented = true;

	mpInit->mpMapHandler->OnVRFramePresentedAfterLoad();
}

//-----------------------------------------------------------------------

void cMapLoadText::OnMouseDown(eMButton aButton)
{
}

//-----------------------------------------------------------------------

void cMapLoadText::OnMouseUp(eMButton aButton)
{
}

//-----------------------------------------------------------------------

void cMapLoadText::SetActive(bool abX)
{
	if(!abX && mbLoading) return;
	if(mbActive == abX) return;

	mbActive = abX;

	if(mbActive)
	{
		mvRows.clear();
		mbShowContinueText = msTextCat != "" && msTextEntry != "";
		if(mbShowContinueText)
			mpTextFont->GetWordWrapRows(750,17,15,kTranslate(msTextCat, msTextEntry),&mvRows);

		mpInit->mpGame->GetUpdater()->SetContainer("MapLoadText");
		mpInit->mpGame->GetScene()->SetDrawScene(false);
		mpInit->mpGame->GetScene()->ResetVRMainUIAnchor();
		mpInit->mpGame->GetScene()->SetUpdateMap(false);
		if(mpInit->mbHasHaptics)
			mpInit->mpGame->GetHaptic()->GetLowLevel()->SetUpdateShapes(false);
		mpInit->mpButtonHandler->ChangeState(eButtonHandlerState_MapLoadText);

		mpBack = mbShowContinueText
			? mpInit->mpGame->GetResources()->GetTextureManager()->Create2D("other_load_text_back.jpg",false)
			: NULL;
		
		mpInit->mpGraphicsHelper->ClearScreen(cColor(0,0));
		if(mpBack)
			mpInit->mpGraphicsHelper->DrawTexture(mpBack,0,cVector3f(800,600,0),cColor(1,1));
		DrawText(true);
		mpDrawer->DrawAll();
		mpInit->mpGraphicsHelper->SwapBuffers();

		mpInit->mpGame->GetGraphics()->GetRenderer3D()->GetRenderList()->Clear();
	}
	else
	{
		if(mpBack) mpInit->mpGame->GetResources()->GetTextureManager()->Destroy(mpBack);
		mpBack = NULL;

		mpInit->mpGame->GetUpdater()->SetContainer("Default");
		mpInit->mpGame->GetScene()->SetDrawScene(true);
		mpInit->mpGame->GetScene()->SetUpdateMap(true);
		if(mpInit->mbHasHaptics)
			mpInit->mpGame->GetHaptic()->GetLowLevel()->SetUpdateShapes(true);
		mpInit->mpButtonHandler->ChangeState(eButtonHandlerState_Game);
	}
}

//-----------------------------------------------------------------------

void cMapLoadText::BeginMapLoad()
{
	mbLoading = true;
	mbMapLoadPending = true;
	mbLoadingFramePresented = false;
}

//-----------------------------------------------------------------------

void cMapLoadText::FinishMapLoad(bool abShowContinueText)
{
	mbLoading = false;
	mbMapLoadPending = false;
	mbLoadingFramePresented = false;
	mbShowContinueText = abShowContinueText;
	if(!abShowContinueText)
	{
		// Loading a save resets every updateable, including this one, while its
		// container remains selected. Restore the active latch so SetActive can
		// perform the normal transition back to gameplay.
		if(!mbActive) mbActive = true;
		SetActive(false);
	}
}

//-----------------------------------------------------------------------

void cMapLoadText::OnExit()
{
	SetActive(false);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// PRIVATE METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------


//-----------------------------------------------------------------------
