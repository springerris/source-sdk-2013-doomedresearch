#include "cbase.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include <vgui_controls/Panel.h>
#include "hud.h"
#include "hud_suitpower.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include <vgui_controls/AnimationController.h>
#include <vgui/ISurface.h>
#include "c_basehlplayer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Shows the flashlight icon
//-----------------------------------------------------------------------------
class CHudBossbar : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE(CHudBossbar, vgui::Panel);

public:
	CHudBossbar(const char* pElementName);
	virtual void ApplySchemeSettings(vgui::IScheme* pScheme);

protected:
	virtual void Paint();

private:
	void Reset(void);
	virtual void SetVisible(bool state);

	bool	m_bFlashlightOn;
	CPanelAnimationVar(vgui::HFont, m_hFont, "TextFont", "Trebuchet24");
	CPanelAnimationVarAliasType(float, m_IconX, "icon_xpos", "0", "proportional_float");
	CPanelAnimationVarAliasType(float, m_IconY, "icon_ypos", "8", "proportional_float");

	CPanelAnimationVarAliasType(float, m_flBarInsetX, "BarInsetX", "4", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarInsetY, "BarInsetY", "36", "proportional_float");

	CPanelAnimationVarAliasType(float, m_flRowGap, "rowgap", "8", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flWide, "wide", "600", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flTall, "tall", "45", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarWidth, "BarWidth", "550", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarHeight, "BarHeight", "15", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkWidth, "BarChunkWidth", "2", "proportional_float");
	CPanelAnimationVarAliasType(float, m_flBarChunkGap, "BarChunkGap", "2", "proportional_float");
};

using namespace vgui;


DECLARE_HUDELEMENT(CHudBossbar);


//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudBossbar::CHudBossbar(const char* pElementName) : CHudElement(pElementName), BaseClass(NULL, "HudBossbar")
{
	vgui::Panel* pParent = g_pClientMode->GetViewport();
	SetParent(pParent);

	SetHiddenBits(HIDEHUD_HEALTH | HIDEHUD_PLAYERDEAD | HIDEHUD_NEEDSUIT);

}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pScheme - 
//-----------------------------------------------------------------------------
void CHudBossbar::ApplySchemeSettings(vgui::IScheme* pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);
	SetPaintBackgroundEnabled(false);
}

//-----------------------------------------------------------------------------
// Purpose: Start with our background off
//-----------------------------------------------------------------------------
void CHudBossbar::Reset(void)
{
	//g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("SuitFlashlightOn");
}

void CHudBossbar::SetVisible(bool state)
{
	BaseClass::SetVisible(state);
}


//-----------------------------------------------------------------------------
// Purpose: draws the flashlight icon
//-----------------------------------------------------------------------------
void CHudBossbar::Paint()
{
#ifdef HL2_EPISODIC
	C_BaseHLPlayer* pPlayer = (C_BaseHLPlayer*)C_BasePlayer::GetLocalPlayer();
	if (!pPlayer)
		return;

	// Only paint if we're using the new flashlight code
	if (pPlayer->m_HL2Local.m_flFlashBattery < 0.0f)
	{
		SetPaintBackgroundEnabled(false);
		return;
	};
	
	
	int ibossCount = pPlayer->GetBossCount();
#ifdef DEBUG
	if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY BOSSCOUNT: %d \n", ibossCount);
#endif // DEBUG
	if (ibossCount > 0) {
		SetVisible(true);
		int* posA = pPlayer->GetBossEncounterDrawPos();
		// some fields take half the height;
		float mulY = 0.0;
		for (int j = 0; j < ibossCount; j++) {
			switch (posA[j]) {
			case 0:
				mulY = mulY + 1;
				break;
			case 1:
				mulY = mulY + 0.5;
				break;
			default:
				mulY = mulY + 1;
				break;
			}
		}
		SetPaintBackgroundEnabled(true);
		BaseClass::SetTall(m_flTall * mulY);
		// get bar chunks
		

#ifdef DEBUG
		if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY BOSSCOUNT: %d DRAWING THE PANEL with tall %.6f \n", ibossCount, m_flTall);
#endif // DEBUG


		// Pick the right character given our current state


		for (int i = 0; i < ibossCount; i++) {

			char tempCString[64];
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY BOSSCOUNT: %d DRAWING THE PANEL %d time \n", ibossCount,i);

				switch (i)
				{
				case 0:
					V_strncpy(tempCString, pPlayer->m_bossDrawDataTitle1, sizeof(tempCString));
					break;
				case 1:
					V_strncpy(tempCString, pPlayer->m_bossDrawDataTitle2, sizeof(tempCString));
					break;
				case 2:
					V_strncpy(tempCString, pPlayer->m_bossDrawDataTitle3, sizeof(tempCString));
					break;
				case 3:
					V_strncpy(tempCString, pPlayer->m_bossDrawDataTitle4, sizeof(tempCString));
					break;
				}
				//V_strncpy(tempCString, pPlayer->m_bossDrawDataTitle[i], sizeof(tempCString));

			int textW = 0;
			int textH = 0;
			float percent = pPlayer->GetBossEncounterDrawP()[i];
			float scale = pPlayer->GetBossEncounterDrawScale()[i];
			int chunkCount = m_flBarWidth / (m_flBarChunkWidth + m_flBarChunkGap) * scale;
			int pos = posA[i];
			int enabledChunks = (int)((float)chunkCount * percent + 0.5f);
#ifdef DEBUG
			if (gpGlobals->tickcount % 66 == 0) DevMsg("Shitting out the string");
			if (gpGlobals->tickcount % 66 == 0) DevMsg("%s", pPlayer->m_bossDrawDataTitle[i]);
			if (gpGlobals->tickcount % 66 == 0) DevMsg("\n");
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY PERCENT: %.8f DRAWING THE PANEL \n", percent);
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY NEW SCALE: %.8f  DRAWING THE PANEL \n", scale);
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY NEW POS: %d DRAWING THE PANEL \n", pos);
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY TITLE: %s DRAWING THE PANEL \n", pPlayer->m_bossDrawDataTitle[i]);
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY NEW TITLE: %s DRAWING THE PANEL \n", tempCString);
#endif // DEBUG
			wchar_t tempString[64];
			V_UTF8ToUnicode(tempCString,tempString,64);
			Color clrFlashlight;
			clrFlashlight = (enabledChunks < (chunkCount / 4)) ? gHUD.m_clrCaution : gHUD.m_clrNormal;
#ifdef DEBUG
			
			if (gpGlobals->tickcount % 66 == 0) DevMsg("HI I AM THE BOSS HEALTHBAR AND THIS IS MY STRING: %s DRAWING THE PANEL \n", tempString);
#endif // DEBUG
			float mulY = 0.0;
				for (int j = 0; j < i; j++) {
					switch (posA[j]) {
					case 0:
						mulY = mulY + 1;
						break;
					case 1:
						mulY = mulY + 0.5;
						break;
					default:
						mulY = mulY + 1;
						break;
					}
				}
			int xpos = m_flBarInsetX, ypos = m_flTall * mulY;
			surface()->DrawSetTextFont(m_hFont);
			//surface()->DrawSetTextScale(32, 32);
			surface()->GetTextSize(m_hFont, tempString, textW, textH);
			surface()->DrawSetTextColor(clrFlashlight);
			
			switch (pos) {
			case 0:
				surface()->DrawSetTextPos(m_flWide / 2 - textW / 2, m_IconY + ypos);
				break;

			case 1:
				surface()->DrawSetTextPos(m_flBarInsetX, m_IconY + ypos);
				break;
			default:
				surface()->DrawSetTextPos(m_flWide / 2 - textW / 2, m_IconY + ypos);
				break;

			}
			surface()->DrawUnicodeString(tempString);


			// draw the suit power bar
			surface()->DrawSetColor(clrFlashlight);
			
			for (int i = 0; i < enabledChunks; i++)
			{
				switch (pos) {
					case 0: 
							surface()->DrawFilledRect(xpos, ypos + textH, xpos + m_flBarChunkWidth, ypos + textH + m_flBarHeight);
							break;
						
					case 1: 
						surface()->DrawFilledRect(xpos+textW, ypos + textH/2 - m_flBarHeight/2, xpos + m_flBarChunkWidth + textW, ypos + m_flBarHeight + textH / 2 - m_flBarHeight / 2);
						break;
					default:
						surface()->DrawFilledRect(xpos, ypos + textH, xpos + m_flBarChunkWidth, ypos + textH + m_flBarHeight);
						break;
				}
				
				xpos += (m_flBarChunkWidth + m_flBarChunkGap);
			}

			// Be even less transparent than we already are
			clrFlashlight[3] = clrFlashlight[3] / 8;

			// draw the exhausted portion of the bar.
			surface()->DrawSetColor(clrFlashlight);
			for (int i = enabledChunks; i < chunkCount; i++)
			{
				switch (pos) {
					case 0: 
						surface()->DrawFilledRect(xpos, ypos + textH, xpos + m_flBarChunkWidth, ypos + textH + m_flBarHeight);
						break;
					
					case 1: 
						surface()->DrawFilledRect(xpos + textW, ypos + textH / 2 - m_flBarHeight / 2, xpos + m_flBarChunkWidth + textW, ypos + textH / 2 - m_flBarHeight / 2 + m_flBarHeight);
						break;
					default:
						surface()->DrawFilledRect(xpos, ypos + textH, xpos + m_flBarChunkWidth, ypos + textH + m_flBarHeight);
						break;
					
				}
				xpos += (m_flBarChunkWidth + m_flBarChunkGap);
			}
		}

	}
	else {
		SetPaintBackgroundEnabled(false);
		SetVisible(false);
	}
#endif // HL2_EPISODIC
}

