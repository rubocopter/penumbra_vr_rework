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
#include "StdAfx.h"
#include "RadioHandler.h"

#include "Init.h"
#include "Player.h"
#include "EffectHandler.h"
#include "Notebook.h"
#include "Inventory.h"
#include "NumericalPanel.h"

//////////////////////////////////////////////////////////////////////////
// CONSTRUCTORS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cRadioHandler::cRadioHandler(cInit *apInit)  : iUpdateable("RadioHandler")
{
	mpInit = apInit;
	mpFont = mpInit->mpGame->GetResources()->GetFontManager()->CreateFontData("verdana.fnt");

	mpSoundHandler = mpInit->mpGame->GetSound()->GetSoundHandler();

	Reset();
}

//-----------------------------------------------------------------------

cRadioHandler::~cRadioHandler(void)
{
	STLDeleteAll(mlstMessages);
}

//-----------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
// GAME MESSAGE
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

cRadioMessage::cRadioMessage(const tWString &asText, const tString &asSound)
{
	msText = asText;
	msSound = asSound;
}

//-----------------------------------------------------------------------


//////////////////////////////////////////////////////////////////////////
// PUBLIC METHODS
//////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------

void cRadioHandler::Add(const tWString& asText, const tString& asSound)
{
	if(mlstMessages.empty())msPrevText = _W("");

	cRadioMessage *pMess = hplNew( cRadioMessage, (asText, asSound) );
	mlstMessages.push_back(pMess);
}

//-----------------------------------------------------------------------

void cRadioHandler::Update(float afTimeStep)
{
	if(mpInit->mpPlayer->IsDead()) {
		STLDeleteAll(mlstMessages);
		mlstMessages.clear();
		return;
	}
	
	/////////////////////////////
	//Update current message
	if(mpCurrentMessage)
	{
		if(mpSoundHandler->IsValid(mpCurrentMessage->mpChannel)==false)
		{
			msCurrentText = _W("");
			msPrevText = mpCurrentMessage->msText;
			hplDelete( mpCurrentMessage );
			mpCurrentMessage = NULL;

			if(mlstMessages.empty() && msOnEndCallback != "")
			{
				mpInit->RunScriptCommand(msOnEndCallback+"()");
				msOnEndCallback = "";
			}
		}
	}

	////////////////////////////
	// Get next message
    if(mpCurrentMessage==NULL && mlstMessages.empty()==false)
	{
		//Get newer message
		mpCurrentMessage = mlstMessages.front();
		mlstMessages.pop_front();

		msCurrentText = mpCurrentMessage->msText;

		mfAlpha =0;

        //Start newer message
		mpCurrentMessage->mpChannel = mpSoundHandler->PlayStream(mpCurrentMessage->msSound,false,1);
	}
	
	////////////////////////
	//Alpha
	if(mfAlpha <1)
	{
		mfAlpha += afTimeStep*2;
		if(mfAlpha >1){
			mfAlpha =1;
			msPrevText = msCurrentText;
		}
	}

	////////////////////////
	//VR: anchor radio subtitles to world space at the UI distance while no
	//other menu owns the overlay state. With a menu open (notebook, inventory,
	//numerical panel) the subtitles inherit that surface instead — which is
	//the behaviour testers already consider correct.
	if(mpInit->mbHasHaptics)
	{
		auto scene = mpInit->mpGame->GetScene();
		bool menuOwnsState = mpInit->mpNotebook->IsActive() ||
			mpInit->mpInventory->IsActive() ||
			mpInit->mpNumericalPanel->IsActive();

		if(IsActive() && menuOwnsState==false)
		{
			cCamera3D* pCamera3D = static_cast<cCamera3D*>(scene->GetCamera());
			float fDistance = mpInit->mVRSettings.GetUIDistance();
			cVector3f uiPos = pCamera3D->GetPosition() +
				pCamera3D->GetViewMatrix().GetRotation().GetForward() * -fDistance;

			auto translateMat = cMath::MatrixTranslate(uiPos);
			auto scaleMat = cMath::MatrixScale(cVector3f(1.0f / 750.0f, -1.0f / 750.0f, 1.0f / 750.0f));

			cMatrixf transMat = cMatrixf::Identity;
			transMat = cMath::MatrixMul(cMath::MatrixTranslate(cVector3f(-400.0f, -300.0f, 0.0f)), transMat);
			transMat = cMath::MatrixMul(scaleMat, transMat);
			transMat = cMath::MatrixMul(cMath::MatrixInverse(pCamera3D->GetViewMatrix().GetRotation()), transMat);
			transMat = cMath::MatrixMul(translateMat, transMat);

			scene->SetVRMenuState(MenuState_WorldPosition, transMat);
			mbVRAnchored = true;
		}
		else if(mbVRAnchored)
		{
			// Restore facelock only when the radio finished AND no menu took
			// over the overlay meanwhile; menus assert their own state once,
			// so writing here would clobber it.
			if(IsActive()==false && menuOwnsState==false)
			{
				scene->SetVRMenuState(MenuState_Facelock, cMatrixf::Identity);
			}
			mbVRAnchored = false;
		}
	}
}

//-----------------------------------------------------------------------

bool cRadioHandler::IsActive()
{
	return mpCurrentMessage!=NULL;
}

//-----------------------------------------------------------------------

void cRadioHandler::OnDraw()
{
	float fAlpha = mfAlpha;
	const float fTextScale = mpInit->mVRSettings.GetSubtitleScale();
	const float fLineHeight = 17.0f * fTextScale;
	const float fFontSize = 15.0f * fTextScale;
    
	if(mpInit->mbSubtitles)
	{
		if(msCurrentText !=_W(""))
		{
			mpFont->DrawWordWrap(cVector3f(40, 470,47),720,fLineHeight,fFontSize,cColor(1,fAlpha),
								eFontAlign_Left,msCurrentText);
			mpFont->DrawWordWrap(cVector3f(40, 470,46) + cVector3f(2,2,0),720,fLineHeight,fFontSize,cColor(0,fAlpha),
								eFontAlign_Left,msCurrentText);
		}

		if(msPrevText !=_W("") && msPrevText != msCurrentText && mfAlpha <1)
		{
			mpFont->DrawWordWrap(cVector3f(40, 470,47),720,fLineHeight,fFontSize,cColor(1,1-fAlpha),
				eFontAlign_Left,msPrevText);
			mpFont->DrawWordWrap(cVector3f(40, 470,46) + cVector3f(2,2,0),720,fLineHeight,fFontSize,cColor(0,1-fAlpha),
				eFontAlign_Left,msPrevText);
		}
	}
}


//-----------------------------------------------------------------------

void cRadioHandler::Reset()
{
	STLDeleteAll(mlstMessages);
	mlstMessages.clear();

	msCurrentText = _W("");
	msPrevText = _W("");
	
	mfAlpha =0;
	mpCurrentMessage = NULL;

	msOnEndCallback = "";
	mbVRAnchored = false;
}

//-----------------------------------------------------------------------
