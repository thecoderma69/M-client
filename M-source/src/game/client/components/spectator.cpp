/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "spectator.h"

#include "camera.h"

#include <base/color.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <limits>

namespace
{
struct SMaSpectatorNameEffectSettings
{
	int m_Style = 0;
	unsigned m_Color1 = 65425;
	unsigned m_Color2 = 41131;
	int m_Glow = 70;
	bool m_Moving = false;
};

static void MaSpectatorNameEffectFillOwn(SMaSpectatorNameEffectSettings &Settings)
{
	Settings.m_Style = std::clamp(g_Config.m_MaNameEffectsOwnStyle, 0, 3);
	Settings.m_Color1 = g_Config.m_MaNameEffectsOwnColor1;
	Settings.m_Color2 = g_Config.m_MaNameEffectsOwnColor2;
	Settings.m_Glow = std::clamp(g_Config.m_MaNameEffectsOwnGlow, 0, 100);
	Settings.m_Moving = g_Config.m_MaNameEffectsOwnMoving != 0;
}

static void MaSpectatorNameEffectCopyTrimmedField(const char *pStart, const char *pEnd, char *pDst, int DstSize)
{
	while(pStart < pEnd && (*pStart == ' ' || *pStart == '\t'))
		++pStart;
	while(pEnd > pStart && (*(pEnd - 1) == ' ' || *(pEnd - 1) == '\t'))
		--pEnd;

	const int CopyLen = minimum<int>((int)(pEnd - pStart), DstSize - 1);
	for(int i = 0; i < CopyLen; ++i)
		pDst[i] = pStart[i];
	pDst[CopyLen] = '\0';
}

static unsigned MaSpectatorNameEffectParseColor(const char *pText, unsigned DefaultColor)
{
	if(!pText || pText[0] == '\0')
		return DefaultColor;
	const int64_t Value = str_toint64_base(pText, 10);
	return Value < 0 ? DefaultColor : (unsigned)Value;
}

static bool MaSpectatorNameEffectParseEntryRecord(const char *pStart, const char *pEnd, const char *pName, SMaSpectatorNameEffectSettings &Settings)
{
	char aaFields[6][MAX_NAME_LENGTH] = {{0}};
	int Field = 0;
	const char *pFieldStart = pStart;
	for(const char *pCursor = pStart; pCursor <= pEnd && Field < 6; ++pCursor)
	{
		if(pCursor == pEnd || *pCursor == '|')
		{
			MaSpectatorNameEffectCopyTrimmedField(pFieldStart, pCursor, aaFields[Field], sizeof(aaFields[Field]));
			++Field;
			pFieldStart = pCursor + 1;
		}
	}

	if(aaFields[0][0] == '\0' || str_comp_nocase(aaFields[0], pName) != 0)
		return false;

	Settings.m_Style = std::clamp(aaFields[1][0] ? str_toint(aaFields[1]) : g_Config.m_MaNameEffectsStyle, 0, 3);
	Settings.m_Color1 = MaSpectatorNameEffectParseColor(aaFields[2], g_Config.m_MaNameEffectsColor1);
	Settings.m_Color2 = MaSpectatorNameEffectParseColor(aaFields[3], g_Config.m_MaNameEffectsColor2);
	Settings.m_Glow = std::clamp(aaFields[4][0] ? str_toint(aaFields[4]) : g_Config.m_MaNameEffectsGlow, 0, 100);
	Settings.m_Moving = aaFields[5][0] ? str_toint(aaFields[5]) != 0 : g_Config.m_MaNameEffectsMoving != 0;
	return true;
}

static bool MaSpectatorNameEffectFindConfiguredEntry(const char *pName, SMaSpectatorNameEffectSettings &Settings)
{
	const char *pCursor = g_Config.m_MaNameEffectsEntries;
	while(*pCursor)
	{
		while(*pCursor == ';' || *pCursor == '\n' || *pCursor == '\r')
			++pCursor;
		const char *pStart = pCursor;
		while(*pCursor && *pCursor != ';' && *pCursor != '\n' && *pCursor != '\r')
			++pCursor;
		if(pCursor > pStart && MaSpectatorNameEffectParseEntryRecord(pStart, pCursor, pName, Settings))
			return true;
	}
	return false;
}

static bool MaSpectatorNameEffectApplies(CGameClient *pGameClient, int ClientId, const char *pName, SMaSpectatorNameEffectSettings &Settings)
{
	if(!g_Config.m_MaNameEffects || !pName || pName[0] == '\0')
		return false;
	const bool Local = ClientId >= 0 && (pGameClient->m_aLocalIds[0] == ClientId || pGameClient->m_aLocalIds[1] == ClientId);
	if(g_Config.m_MaNameEffectsOwn && Local)
	{
		MaSpectatorNameEffectFillOwn(Settings);
		return true;
	}
	return MaSpectatorNameEffectFindConfiguredEntry(pName, Settings);
}

static ColorRGBA MaSpectatorNameEffectConfigColor(unsigned ConfigColor, float Alpha)
{
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ConfigColor, true));
	Color.a = std::clamp(Color.a, 0.25f, 1.0f) * Alpha;
	return Color;
}

static ColorRGBA MaSpectatorNameEffectMixColors(ColorRGBA A, ColorRGBA B, float Amount)
{
	Amount = std::clamp(Amount, 0.0f, 1.0f);
	return ColorRGBA(A.r + (B.r - A.r) * Amount, A.g + (B.g - A.g) * Amount, A.b + (B.b - A.b) * Amount, A.a + (B.a - A.a) * Amount);
}

static ColorRGBA MaSpectatorNameEffectLetterColor(int LetterIndex, float Alpha, const SMaSpectatorNameEffectSettings &Settings, int MotionTick)
{
	const int Style = std::clamp(Settings.m_Style, 0, 3);
	if(Style == 0)
	{
		const int HueOffset = Settings.m_Moving ? MotionTick * 8 : 0;
		const float Hue = (float)(((LetterIndex * 89 + HueOffset) % 360 + 360) % 360) / 360.0f;
		ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.62f));
		Color.a = Alpha;
		return Color;
	}

	ColorRGBA Primary = MaSpectatorNameEffectConfigColor(Settings.m_Color1, Alpha);
	ColorRGBA Accent = MaSpectatorNameEffectConfigColor(Settings.m_Color2, Alpha);
	if(Settings.m_Moving)
	{
		const int Phase = (LetterIndex * 37 + MotionTick * 9) % 120;
		const float Mix = Phase < 60 ? Phase / 60.0f : (120 - Phase) / 60.0f;
		return MaSpectatorNameEffectMixColors(Primary, Accent, Mix);
	}
	return Primary;
}

static void MaSpectatorRenderNameEffect(ITextRender *pTextRender, CTextCursor *pCursor, const char *pName, const SMaSpectatorNameEffectSettings &Settings, float Alpha)
{
	const int Style = std::clamp(Settings.m_Style, 0, 3);
	const bool Stars = Style == 2 || Style == 3;
	const int MotionTick = Settings.m_Moving ? (int)((time_get() * 12) / time_freq()) : 0;

	if(Stars)
	{
		pTextRender->TextColor(MaSpectatorNameEffectConfigColor(Settings.m_Color2, Alpha));
		pTextRender->TextEx(pCursor, "\xE2\x9C\xA6 ");
	}

	const char *pChar = pName;
	int LetterIndex = 0;
	while(*pChar)
	{
		const char *pNext = pChar;
		str_utf8_decode(&pNext);
		const int CharLen = maximum<int>(1, (int)(pNext - pChar));
		pTextRender->TextColor(MaSpectatorNameEffectLetterColor(LetterIndex, Alpha, Settings, MotionTick));
		pTextRender->TextEx(pCursor, pChar, CharLen);
		pChar += CharLen;
		++LetterIndex;
	}

	if(Stars)
	{
		pTextRender->TextColor(MaSpectatorNameEffectConfigColor(Settings.m_Color2, Alpha));
		pTextRender->TextEx(pCursor, " \xE2\x9C\xA6");
	}
	pTextRender->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
}
} // namespace
bool CSpectator::CanChangeSpectatorId()
{
	// don't change SpectatorId when not spectating
	if(!GameClient()->m_Snap.m_SpecInfo.m_Active)
		return false;

	// stop follow mode from changing SpectatorId
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FOLLOW)
		return false;

	return true;
}

void CSpectator::SpectateNext(bool Reverse)
{
	int CurIndex = -1;
	const CNetObj_PlayerInfo **paPlayerInfos = GameClient()->m_Snap.m_apInfoByDDTeamName;

	// m_SpectatorId may be uninitialized if m_Active is false
	if(GameClient()->m_Snap.m_SpecInfo.m_Active)
	{
		for(int i = 0; i < MAX_CLIENTS; i++)
		{
			if(paPlayerInfos[i] && paPlayerInfos[i]->m_ClientId == GameClient()->m_Snap.m_SpecInfo.m_SpectatorId)
			{
				CurIndex = i;
				break;
			}
		}
	}

	int Start;
	if(CurIndex != -1)
	{
		if(Reverse)
			Start = CurIndex - 1;
		else
			Start = CurIndex + 1;
	}
	else
	{
		if(Reverse)
			Start = -1;
		else
			Start = 0;
	}

	int Increment = Reverse ? -1 : 1;

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		int PlayerIndex = (Start + i * Increment) % MAX_CLIENTS;
		// % in C++ takes the sign of the dividend, not divisor
		if(PlayerIndex < 0)
			PlayerIndex += MAX_CLIENTS;

		const CNetObj_PlayerInfo *pPlayerInfo = paPlayerInfos[PlayerIndex];
		if(pPlayerInfo && pPlayerInfo->m_Team != TEAM_SPECTATORS)
		{
			Spectate(pPlayerInfo->m_ClientId);
			break;
		}
	}
}

void CSpectator::ConKeySpectator(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;

	if(pSelf->GameClient()->m_Scoreboard.IsActive())
		return;

	if(pSelf->GameClient()->m_Snap.m_SpecInfo.m_Active || pSelf->Client()->State() == IClient::STATE_DEMOPLAYBACK)
		pSelf->m_Active = pResult->GetInteger(0) != 0;
	else
		pSelf->m_Active = false;
}

void CSpectator::ConSpectate(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->Spectate(pResult->GetInteger(0));
}

void CSpectator::ConSpectateNext(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(false);
}

void CSpectator::ConSpectatePrevious(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	if(!pSelf->CanChangeSpectatorId())
		return;

	pSelf->SpectateNext(true);
}

void CSpectator::ConSpectateClosest(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	pSelf->SpectateClosest();
}

void CSpectator::ConMultiView(IConsole::IResult *pResult, void *pUserData)
{
	CSpectator *pSelf = (CSpectator *)pUserData;
	int Input = pResult->GetInteger(0);

	if(Input == -1)
		std::fill(std::begin(pSelf->GameClient()->m_aMultiViewId), std::end(pSelf->GameClient()->m_aMultiViewId), false); // remove everyone from multiview
	else if(Input < MAX_CLIENTS && Input >= 0)
		pSelf->GameClient()->m_aMultiViewId[Input] = !pSelf->GameClient()->m_aMultiViewId[Input]; // activate or deactivate one player from multiview
}

CSpectator::CSpectator()
{
	m_SelectorMouse = vec2(0.0f, 0.0f);
	OnReset();
}

void CSpectator::OnConsoleInit()
{
	Console()->Register("+spectate", "", CFGFLAG_CLIENT, ConKeySpectator, this, "Open spectator mode selector");
	Console()->Register("spectate", "i[spectator-id]", CFGFLAG_CLIENT, ConSpectate, this, "Switch spectator mode");
	Console()->Register("spectate_next", "", CFGFLAG_CLIENT, ConSpectateNext, this, "Spectate the next player");
	Console()->Register("spectate_previous", "", CFGFLAG_CLIENT, ConSpectatePrevious, this, "Spectate the previous player");
	Console()->Register("spectate_closest", "", CFGFLAG_CLIENT, ConSpectateClosest, this, "Spectate the closest player");
	Console()->Register("spectate_multiview", "i[id]", CFGFLAG_CLIENT, ConMultiView, this, "Add/remove Client-IDs to spectate them exclusively (-1 to reset)");
}

bool CSpectator::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_Active)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	m_SelectorMouse += vec2(x, y);
	return true;
}

bool CSpectator::OnInput(const IInput::CEvent &Event)
{
	if(IsActive() && Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		OnRelease();
		return true;
	}

	if(g_Config.m_ClSpectatorMouseclicks)
	{
		if(GameClient()->m_Snap.m_SpecInfo.m_Active && !IsActive() && !GameClient()->m_MultiViewActivated &&
			!Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive() && !GameClient()->m_Menus.IsActive())
		{
			if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_1)
			{
				if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId != SPEC_FREEVIEW)
					Spectate(SPEC_FREEVIEW);
				else
					SpectateClosest();
				return true;
			}
		}
	}

	if(GameClient()->m_Camera.SpectatingPlayer() && GameClient()->m_Camera.CanUseAutoSpecCamera())
	{
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_MOUSE_2)
		{
			GameClient()->m_Camera.ResetAutoSpecCamera();
			return true;
		}
	}

	return false;
}

void CSpectator::OnRelease()
{
	OnReset();
}

void CSpectator::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!GameClient()->m_MultiViewActivated && m_MultiViewActivateDelay != 0.0f)
	{
		if(m_MultiViewActivateDelay <= Client()->LocalTime())
		{
			m_MultiViewActivateDelay = 0.0f;
			GameClient()->m_MultiViewActivated = true;
		}
	}

	if(!m_Active)
	{
		// closing the spectator menu
		if(m_WasActive)
		{
			if(m_SelectedSpectatorId != NO_SELECTION)
			{
				if(m_SelectedSpectatorId == MULTI_VIEW)
					GameClient()->m_MultiViewActivated = true;
				else if(m_SelectedSpectatorId == SPEC_FREEVIEW || m_SelectedSpectatorId == SPEC_FOLLOW)
					GameClient()->m_MultiViewActivated = false;

				if(!GameClient()->m_MultiViewActivated)
					Spectate(m_SelectedSpectatorId);

				if(GameClient()->m_MultiViewActivated && m_SelectedSpectatorId != MULTI_VIEW && GameClient()->m_Teams.Team(m_SelectedSpectatorId) != GameClient()->m_MultiViewTeam)
				{
					GameClient()->ResetMultiView();
					Spectate(m_SelectedSpectatorId);
					m_MultiViewActivateDelay = Client()->LocalTime() + 0.3f;
				}
			}
			m_WasActive = false;
		}
		return;
	}

	if(!GameClient()->m_Snap.m_SpecInfo.m_Active && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		m_Active = false;
		m_WasActive = false;
		return;
	}

	m_WasActive = true;
	m_SelectedSpectatorId = NO_SELECTION;

	// draw background
	float Width = 400 * 3.0f * Graphics()->ScreenAspect();
	float Height = 400 * 3.0f;
	float ObjWidth = 300.0f;
	float FontSize = 20.0f;
	float BigFontSize = 20.0f;
	float StartY = -190.0f;
	float LineHeight = 60.0f;
	float TeeSizeMod = 1.0f;
	float RoundRadius = 30.0f;
	bool MultiViewSelected = false;
	int TotalPlayers = 0;
	int PerLine = 8;
	float BoxMove = -10.0f;
	float BoxOffset = 0.0f;

	for(const auto &pInfo : GameClient()->m_Snap.m_apInfoByDDTeamName)
	{
		if(!pInfo || pInfo->m_Team == TEAM_SPECTATORS)
			continue;

		++TotalPlayers;
	}

	if(TotalPlayers > 64)
	{
		FontSize = 12.0f;
		LineHeight = 15.0f;
		TeeSizeMod = 0.3f;
		PerLine = 32;
		RoundRadius = 5.0f;
		BoxMove = 3.0f;
		BoxOffset = 6.0f;
	}
	else if(TotalPlayers > 32)
	{
		FontSize = 18.0f;
		LineHeight = 30.0f;
		TeeSizeMod = 0.7f;
		PerLine = 16;
		RoundRadius = 10.0f;
		BoxMove = 3.0f;
		BoxOffset = 6.0f;
	}
	if(TotalPlayers > 16)
	{
		ObjWidth = 600.0f;
	}

	const vec2 ScreenSize = vec2(Width, Height);
	const vec2 ScreenCenter = ScreenSize / 2.0f;
	CUIRect SpectatorRect = {Width / 2.0f - ObjWidth, Height / 2.0f - 300.0f, ObjWidth * 2.0f, 600.0f};
	CUIRect SpectatorMouseRect;
	SpectatorRect.Margin(20.0f, &SpectatorMouseRect);

	const bool WasTouchPressed = m_TouchState.m_AnyPressed;
	Ui()->UpdateTouchState(m_TouchState);
	if(m_TouchState.m_AnyPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * ScreenSize;
		if(SpectatorMouseRect.Inside(ScreenCenter + TouchPos))
		{
			m_SelectorMouse = TouchPos;
		}
	}
	else if(WasTouchPressed)
	{
		const vec2 TouchPos = (m_TouchState.m_PrimaryPosition - vec2(0.5f, 0.5f)) * ScreenSize;
		if(!SpectatorRect.Inside(ScreenCenter + TouchPos))
		{
			OnRelease();
			return;
		}
	}

	Graphics()->MapScreen(0, 0, Width, Height);

	SpectatorRect.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.3f), IGraphics::CORNER_ALL, 20.0f);

	// clamp mouse position to selector area
	m_SelectorMouse.x = std::clamp(m_SelectorMouse.x, -(ObjWidth - 20.0f), ObjWidth - 20.0f);
	m_SelectorMouse.y = std::clamp(m_SelectorMouse.y, -280.0f, 280.0f);

	const bool MousePressed = Input()->KeyPress(KEY_MOUSE_1) || m_TouchState.m_PrimaryPressed;

	// draw selections
	if((Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == SPEC_FREEVIEW) ||
		(Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SPEC_FREEVIEW))
	{
		Graphics()->DrawRect(Width / 2.0f - (ObjWidth - 20.0f), Height / 2.0f - 280.0f, ((ObjWidth * 2.0f) / 3.0f) - 40.0f, 60.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);
	}

	if(GameClient()->m_MultiViewActivated)
	{
		Graphics()->DrawRect(Width / 2.0f - (ObjWidth - 20.0f) + (ObjWidth * 2.0f / 3.0f), Height / 2.0f - 280.0f, ((ObjWidth * 2.0f) / 3.0f) - 40.0f, 60.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);
	}

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_LocalClientId >= 0 && GameClient()->m_DemoSpecId == SPEC_FOLLOW)
	{
		Graphics()->DrawRect(Width / 2.0f - (ObjWidth - 20.0f) + (ObjWidth * 2.0f * 2.0f / 3.0f), Height / 2.0f - 280.0f, ((ObjWidth * 2.0f) / 3.0f) - 40.0f, 60.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, 20.0f);
	}

	bool FreeViewSelected = false;
	if(m_SelectorMouse.x >= -(ObjWidth - 20.0f) && m_SelectorMouse.x <= -(ObjWidth - 20.0f) + ((ObjWidth * 2.0f) / 3.0f) - 40.0f &&
		m_SelectorMouse.y >= -280.0f && m_SelectorMouse.y <= -220.0f)
	{
		m_SelectedSpectatorId = SPEC_FREEVIEW;
		FreeViewSelected = true;
		if(MousePressed)
		{
			GameClient()->m_MultiViewActivated = false;
			Spectate(m_SelectedSpectatorId);
		}
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, FreeViewSelected ? 1.0f : 0.5f);
	TextRender()->Text(Width / 2.0f - (ObjWidth - 40.0f), Height / 2.0f - 280.f + (60.f - BigFontSize) / 2.f, BigFontSize, Localize("Free-View"), -1.0f);

	if(m_SelectorMouse.x >= -(ObjWidth - 20.0f) + (ObjWidth * 2.0f / 3.0f) && m_SelectorMouse.x <= -(ObjWidth - 20.0f) + (ObjWidth * 2.0f / 3.0f) + ((ObjWidth * 2.0f) / 3.0f) - 40.0f &&
		m_SelectorMouse.y >= -280.0f && m_SelectorMouse.y <= -220.0f)
	{
		m_SelectedSpectatorId = MULTI_VIEW;
		MultiViewSelected = true;
		if(MousePressed)
		{
			GameClient()->m_MultiViewActivated = true;
		}
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, MultiViewSelected ? 1.0f : 0.5f);
	TextRender()->Text(Width / 2.0f - (ObjWidth - 40.0f) + (ObjWidth * 2.0f / 3.0f), Height / 2.0f - 280.f + (60.f - BigFontSize) / 2.f, BigFontSize, Localize("Multi-View"), -1.0f);

	if(Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_LocalClientId >= 0)
	{
		bool FollowSelected = false;
		if(m_SelectorMouse.x >= -(ObjWidth - 20.0f) + (ObjWidth * 2.0f * 2.0f / 3.0f) && m_SelectorMouse.x <= -(ObjWidth - 20.0f) + (ObjWidth * 2.0f * 2.0f / 3.0f) + ((ObjWidth * 2.0f) / 3.0f) - 40.0f &&
			m_SelectorMouse.y >= -280.0f && m_SelectorMouse.y <= -220.0f)
		{
			m_SelectedSpectatorId = SPEC_FOLLOW;
			FollowSelected = true;
			if(MousePressed)
			{
				GameClient()->m_MultiViewActivated = false;
				Spectate(m_SelectedSpectatorId);
			}
		}
		TextRender()->TextColor(1.0f, 1.0f, 1.0f, FollowSelected ? 1.0f : 0.5f);
		TextRender()->Text(Width / 2.0f - (ObjWidth - 40.0f) + (ObjWidth * 2.0f * 2.0f / 3.0f), Height / 2.0f - 280.0f + (60.f - BigFontSize) / 2.f, BigFontSize, Localize("Follow"), -1.0f);
	}

	float x = -(ObjWidth - 35.0f), y = StartY;

	int OldDDTeam = -1;

	for(int i = 0, Count = 0; i < MAX_CLIENTS; ++i)
	{
		if(!GameClient()->m_Snap.m_apInfoByDDTeamName[i] || GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_Team == TEAM_SPECTATORS)
			continue;

		++Count;

		if(Count == PerLine + 1 || (Count > PerLine + 1 && (Count - 1) % PerLine == 0))
		{
			x += 290.0f;
			y = StartY;
		}

		const CNetObj_PlayerInfo *pInfo = GameClient()->m_Snap.m_apInfoByDDTeamName[i];
		int DDTeam = GameClient()->m_Teams.Team(pInfo->m_ClientId);
		int NextDDTeam = 0;

		for(int j = i + 1; j < MAX_CLIENTS; j++)
		{
			const CNetObj_PlayerInfo *pInfo2 = GameClient()->m_Snap.m_apInfoByDDTeamName[j];

			if(!pInfo2 || pInfo2->m_Team == TEAM_SPECTATORS)
				continue;

			NextDDTeam = GameClient()->m_Teams.Team(pInfo2->m_ClientId);
			break;
		}

		if(OldDDTeam == -1)
		{
			for(int j = i - 1; j >= 0; j--)
			{
				const CNetObj_PlayerInfo *pInfo2 = GameClient()->m_Snap.m_apInfoByDDTeamName[j];

				if(!pInfo2 || pInfo2->m_Team == TEAM_SPECTATORS)
					continue;

				OldDDTeam = GameClient()->m_Teams.Team(pInfo2->m_ClientId);
				break;
			}
		}

		if(DDTeam != TEAM_FLOCK)
		{
			const ColorRGBA Color = GameClient()->GetDDTeamColor(DDTeam).WithAlpha(0.5f);
			int Corners = 0;
			if(OldDDTeam != DDTeam)
				Corners |= IGraphics::CORNER_TL | IGraphics::CORNER_TR;
			if(NextDDTeam != DDTeam)
				Corners |= IGraphics::CORNER_BL | IGraphics::CORNER_BR;
			Graphics()->DrawRect(Width / 2.0f + x - 10.0f + BoxOffset, Height / 2.0f + y + BoxMove, 270.0f - BoxOffset, LineHeight, Color, Corners, RoundRadius);
		}

		OldDDTeam = DDTeam;

		if((Client()->State() == IClient::STATE_DEMOPLAYBACK && GameClient()->m_DemoSpecId == GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId) || (Client()->State() != IClient::STATE_DEMOPLAYBACK && GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId))
		{
			Graphics()->DrawRect(Width / 2.0f + x - 10.0f + BoxOffset, Height / 2.0f + y + BoxMove, 270.0f - BoxOffset, LineHeight, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_ALL, RoundRadius);
		}

		bool PlayerSelected = false;
		if(m_SelectorMouse.x >= x - 10.0f && m_SelectorMouse.x < x + 260.0f &&
			m_SelectorMouse.y >= y - (LineHeight / 6.0f) && m_SelectorMouse.y < y + (LineHeight * 5.0f / 6.0f))
		{
			m_SelectedSpectatorId = GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId;
			PlayerSelected = true;
			if(MousePressed)
			{
				if(GameClient()->m_MultiViewActivated)
				{
					if(GameClient()->m_MultiViewTeam == DDTeam)
					{
						GameClient()->m_aMultiViewId[m_SelectedSpectatorId] = !GameClient()->m_aMultiViewId[m_SelectedSpectatorId];
						if(!GameClient()->m_aMultiViewId[GameClient()->m_Snap.m_SpecInfo.m_SpectatorId])
						{
							int NewClientId = GameClient()->FindFirstMultiViewId();
							if(NewClientId < MAX_CLIENTS && NewClientId >= 0)
							{
								GameClient()->CleanMultiViewId(NewClientId);
								GameClient()->m_aMultiViewId[NewClientId] = true;
								Spectate(NewClientId);
							}
						}
					}
					else
					{
						GameClient()->ResetMultiView();
						Spectate(m_SelectedSpectatorId);
						m_MultiViewActivateDelay = Client()->LocalTime() + 0.3f;
					}
				}
				else
				{
					Spectate(m_SelectedSpectatorId);
				}
			}
		}
		float TeeAlpha;
		float NameAlpha;
		if(Client()->State() == IClient::STATE_DEMOPLAYBACK &&
			!GameClient()->m_Snap.m_aCharacters[GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId].m_Active)
		{
			NameAlpha = 0.25f;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, NameAlpha);
			TeeAlpha = 0.5f;
		}
		else
		{
			NameAlpha = PlayerSelected ? 1.0f : 0.5f;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, NameAlpha);
			TeeAlpha = 1.0f;
		}
		const int ClientId = GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId;
		const char *pClientName = GameClient()->m_aClients[ClientId].m_aName;
		CTextCursor NameCursor;
		NameCursor.SetPosition(vec2(Width / 2.0f + x + 50.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f));
		NameCursor.m_FontSize = FontSize;
		NameCursor.m_Flags |= TEXTFLAG_ELLIPSIS_AT_END;
		NameCursor.m_LineWidth = 180.0f;
		if(g_Config.m_ClShowIds)
		{
			char aClientId[16];
			GameClient()->FormatClientId(ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
			TextRender()->TextEx(&NameCursor, aClientId);
		}

		SMaSpectatorNameEffectSettings NameEffectSettings;
		if(MaSpectatorNameEffectApplies(GameClient(), ClientId, pClientName, NameEffectSettings))
		{
			MaSpectatorRenderNameEffect(TextRender(), &NameCursor, pClientName, NameEffectSettings, NameAlpha);
		}
		else
		{
			// TClient
			if(pInfo && pInfo->m_ClientId > 0 && g_Config.m_TcWarList && g_Config.m_TcWarListSpectate && GameClient()->m_WarList.GetAnyWar(pInfo->m_ClientId))
			{
				TextRender()->TextColor(GameClient()->m_WarList.GetPriorityColor(pInfo->m_ClientId));
			}

			TextRender()->TextEx(&NameCursor, pClientName);
		}

		if(GameClient()->m_MultiViewActivated)
		{
			if(GameClient()->m_aMultiViewId[GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId])
			{
				TextRender()->TextColor(0.1f, 1.0f, 0.1f, PlayerSelected ? 1.0f : 0.5f);
				TextRender()->Text(Width / 2.0f + x + 50.0f + 180.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize - 3, "⬤", 220.0f);
			}
			else if(GameClient()->m_MultiViewTeam == DDTeam)
			{
				TextRender()->TextColor(1.0f, 0.1f, 0.1f, PlayerSelected ? 1.0f : 0.5f);
				TextRender()->Text(Width / 2.0f + x + 50.0f + 180.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize - 3, "◯", 220.0f);
			}
		}

		// flag
		if(GameClient()->m_Snap.m_pGameInfoObj && (GameClient()->m_Snap.m_pGameInfoObj->m_GameFlags & GAMEFLAG_FLAGS) &&
			GameClient()->m_Snap.m_pGameDataObj && (GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierRed == GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId || GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId))
		{
			Graphics()->BlendNormal();
			if(GameClient()->m_Snap.m_pGameDataObj->m_FlagCarrierBlue == GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId)
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagBlue);
			else
				Graphics()->TextureSet(GameClient()->m_GameSkin.m_SpriteFlagRed);

			Graphics()->QuadsBegin();
			Graphics()->QuadsSetSubset(1, 0, 0, 1);

			float Size = LineHeight;
			IGraphics::CQuadItem QuadItem(Width / 2.0f + x - LineHeight / 5.0f, Height / 2.0f + y - LineHeight / 3.0f, Size / 2.0f, Size);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
		}

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId].m_RenderInfo;
		TeeInfo.m_Size *= TeeSizeMod;

		const CAnimState *pIdleState = CAnimState::GetIdle();
		vec2 OffsetToMid;
		CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
		vec2 TeeRenderPos(Width / 2.0f + x + 20.0f, Height / 2.0f + y + BoxMove + LineHeight / 2.0f + OffsetToMid.y);

		RenderTools()->RenderTee(pIdleState, &TeeInfo, EMOTE_NORMAL, vec2(1.0f, 0.0f), TeeRenderPos, TeeAlpha);

		if(GameClient()->m_aClients[GameClient()->m_Snap.m_apInfoByDDTeamName[i]->m_ClientId].m_Friend)
		{
			TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)));
			TextRender()->Text(Width / 2.0f + x - TeeInfo.m_Size / 2.0f, Height / 2.0f + y + BoxMove + (LineHeight - FontSize) / 2.f, FontSize, "♥", 220.0f);
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
		}

		y += LineHeight;
	}
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);

	RenderTools()->RenderCursor(ScreenCenter + m_SelectorMouse, 48.0f);
}

void CSpectator::OnReset()
{
	m_WasActive = false;
	m_Active = false;
	m_SelectedSpectatorId = NO_SELECTION;
}

void CSpectator::Spectate(int SpectatorId)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
	{
		GameClient()->m_DemoSpecId = std::clamp(SpectatorId, (int)SPEC_FOLLOW, MAX_CLIENTS - 1);
		// The tick must be rendered for the spectator mode to be updated, so we do it manually when demo playback is paused
		// TODO: https://github.com/ddnet/ddnet/issues/11681
		if(DemoPlayer()->BaseInfo()->m_Paused)
			GameClient()->m_Menus.DemoSeekTick(IDemoPlayer::TICK_CURRENT);
		return;
	}

	if(GameClient()->m_Snap.m_SpecInfo.m_SpectatorId == SpectatorId)
		return;

	if(Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_SetSpectatorMode Msg;
		if(SpectatorId == SPEC_FREEVIEW)
		{
			Msg.m_SpecMode = protocol7::SPEC_FREEVIEW;
			Msg.m_SpectatorId = -1;
		}
		else
		{
			Msg.m_SpecMode = protocol7::SPEC_PLAYER;
			Msg.m_SpectatorId = SpectatorId;
		}
		Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL, true);
		return;
	}
	CNetMsg_Cl_SetSpectatorMode Msg;
	Msg.m_SpectatorId = SpectatorId;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CSpectator::SpectateClosest()
{
	if(!CanChangeSpectatorId())
		return;

	const CGameClient::CSnapState &Snap = GameClient()->m_Snap;
	int SpectatorId = Snap.m_SpecInfo.m_SpectatorId;

	int NewSpectatorId = -1;

	vec2 CurPosition = GameClient()->m_Camera.m_Center;
	if(SpectatorId != SPEC_FREEVIEW)
	{
		const CNetObj_Character &CurCharacter = Snap.m_aCharacters[SpectatorId].m_Cur;
		CurPosition = vec2(CurCharacter.m_X, CurCharacter.m_Y);
	}

	int ClosestDistance = std::numeric_limits<int>::max();
	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		if(ClientId == SpectatorId || !Snap.m_aCharacters[ClientId].m_Active || !Snap.m_apPlayerInfos[ClientId] || Snap.m_apPlayerInfos[ClientId]->m_Team == TEAM_SPECTATORS)
			continue;

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK && ClientId == Snap.m_LocalClientId)
			continue;

		const CNetObj_Character &MaybeClosestCharacter = Snap.m_aCharacters[ClientId].m_Cur;
		int Distance = distance(CurPosition, vec2(MaybeClosestCharacter.m_X, MaybeClosestCharacter.m_Y));
		if(NewSpectatorId == -1 || Distance < ClosestDistance)
		{
			NewSpectatorId = ClientId;
			ClosestDistance = Distance;
		}
	}
	if(NewSpectatorId > -1)
		Spectate(NewSpectatorId);
}
