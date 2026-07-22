#include "hud_layout.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/config.h>
#include <engine/console.h>
#include <engine/shared/config.h>

#include <algorithm>

namespace HudLayout
{

	namespace
	{

		static const SModuleLayout gs_aModuleLayouts[MODULE_COUNT] = {
			{0.0f, 60.0f, 100, 0, true, true, 0x66000000U},
			{304.0f, 0.0f, 100, 0, true, true, 0x66000000U},
			{470.0f, 205.0f, 100, 0, true, true, 0x66000000U},
			{100.0f, 3.0f, 100, 0, true, false, 0x66000000U},
			{500.0f, 5.0f, 100, 0, true, false, 0x66000000U},
			{500.0f, 20.0f, 100, 0, true, false, 0x66000000U},
			{233.0f, 64.0f, 100, 0, true, false, 0x66000000U},
			{220.0f, 240.0f, 100, 0, true, false, 0x66000000U},
			{110.0f, 2.0f, 100, 0, true, true, 0x66000000U},
			{500.0f, 141.0f, 100, 0, true, true, 0x66000000U},
			{460.0f, 229.0f, 100, 0, true, true, 0x40000000U},
			{198.0f, 0.0f, 100, 0, true, false, 0x1E59A36BU},
			{4.0f, 122.0f, 100, 0, true, false, 0x66000000U},
			{136.0f, 0.0f, 100, 0, true, false, 0x66000000U},
			{5.0f, 278.0f, 100, 0, true, false, 0x66000000U},
			{0.0f, 60.0f, 100, 0, true, true, 0x66000000U},
			{250.0f, 200.0f, 65, 0, true, true, 0x66000000U},
			{490.0f, 5.0f, 100, 0, true, false, 0x66000000U},
			{0.0f, 156.0f, 100, 0, true, true, 0x66000000U},
			{0.0f, 182.0f, 100, 0, true, false, 0x66000000U},
			{96.0f, 182.0f, 100, 0, true, false, 0x66000000U},
			{150.0f, 50.0f, 100, 0, true, false, 0x66000000U},
			{250.0f, 11.0f, 100, 0, true, false, 0x66000000U},
			{370.0f, 72.0f, 100, 0, true, true, 0xAA050510U},
		};

		static SModuleLayout gs_aRuntimeModuleLayouts[MODULE_COUNT];
		static bool gs_RuntimeLayoutsInitialized = false;
		static bool gs_ConfigCallbackRegistered = false;
		static bool gs_ConsoleCommandRegistered = false;

		void EnsureRuntimeLayouts()
		{
			if(gs_RuntimeLayoutsInitialized)
				return;
			for(int i = 0; i < MODULE_COUNT; ++i)
				gs_aRuntimeModuleLayouts[i] = gs_aModuleLayouts[i];
			gs_RuntimeLayoutsInitialized = true;
		}

		bool HasRuntimeOverrideInternal(EModule Module)
		{
			EnsureRuntimeLayouts();
			const SModuleLayout &Runtime = gs_aRuntimeModuleLayouts[Module];
			const SModuleLayout &Default = gs_aModuleLayouts[Module];
			return Runtime.m_X != Default.m_X ||
			       Runtime.m_Y != Default.m_Y ||
			       Runtime.m_Scale != Default.m_Scale ||
			       Runtime.m_Mode != Default.m_Mode ||
			       Runtime.m_Enabled != Default.m_Enabled ||
			       Runtime.m_BackgroundEnabled != Default.m_BackgroundEnabled ||
			       Runtime.m_BackgroundColor != Default.m_BackgroundColor;
		}

		static const char *gs_apModuleNames[MODULE_COUNT] = {
			"Mini Vote",
			"Frozen HUD",
			"Movement Info",
			"Notify Last",
			"FPS",
			"Ping",
			"Game Timer",
			"Hook Combo",
			"Local Time",
			"Spectator Count",
			"Score",
			"Music Player",
			"Voice HUD",
			"Voice Mute Icons",
			"Ingame Chat",
			"Votes",
			"Lock Cam",
			"Killfeed",
			"Finish Prediction",
			"Keyboard",
			"Mouse",
			"Efecto Musica Video",
			"Contador congelados",
			"Espectadores",
		};

		SModuleLayout ConfigLayout(EModule Module)
		{
			EnsureRuntimeLayouts();
			SModuleLayout Runtime = gs_aRuntimeModuleLayouts[Module];

			// Migration: older keystrokes builds saved these modules disabled by default.
			// If the saved layout still matches that legacy default exactly, auto-enable it.
			if(Module == MODULE_KEYSTROKES_KEYBOARD &&
				!Runtime.m_Enabled &&
				Runtime.m_X == 0.0f &&
				Runtime.m_Y == 182.0f &&
				Runtime.m_Scale == 100 &&
				Runtime.m_Mode == 0 &&
				!Runtime.m_BackgroundEnabled &&
				Runtime.m_BackgroundColor == 0x66000000U)
			{
				Runtime.m_Enabled = true;
				gs_aRuntimeModuleLayouts[Module] = Runtime;
			}
			else if(Module == MODULE_KEYSTROKES_MOUSE &&
				!Runtime.m_Enabled &&
				Runtime.m_X == 96.0f &&
				Runtime.m_Y == 182.0f &&
				Runtime.m_Scale == 100 &&
				Runtime.m_Mode == 0 &&
				!Runtime.m_BackgroundEnabled &&
				Runtime.m_BackgroundColor == 0x66000000U)
			{
				Runtime.m_Enabled = true;
				gs_aRuntimeModuleLayouts[Module] = Runtime;
			}

			switch(Module)
			{
			case MODULE_MUSIC_PLAYER:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_VOICE_TALKERS:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_VOICE_STATUS:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_CHAT:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_VOTES:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_KEYSTROKES_KEYBOARD:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_KEYSTROKES_MOUSE:
				return {Runtime.m_X, Runtime.m_Y, Runtime.m_Scale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			case MODULE_MA_SPECTATORS:
				return {(float)g_Config.m_MaSpectatorPanelHudX, (float)g_Config.m_MaSpectatorPanelHudY, g_Config.m_MaSpectatorPanelHudScale, Runtime.m_Mode, Runtime.m_Enabled, Runtime.m_BackgroundEnabled, Runtime.m_BackgroundColor};
			default:
				return Runtime;
			}
		}

		void WriteConfigLayout(EModule Module, const SModuleLayout &Layout)
		{
			EnsureRuntimeLayouts();
			gs_aRuntimeModuleLayouts[Module] = Layout;

			switch(Module)
			{
			case MODULE_MUSIC_PLAYER:
				g_Config.m_MaMusicPlayerUseColorForHud = Layout.m_Mode;
				break;
			case MODULE_CHAT:
				g_Config.m_MaHudChatX = round_to_int(Layout.m_X);
				g_Config.m_MaHudChatY = round_to_int(Layout.m_Y);
				g_Config.m_MaHudChatScale = Layout.m_Scale;
				break;
			case MODULE_KEYSTROKES_KEYBOARD:
				g_Config.m_TcKeystrokeHudPosX = (int)(Layout.m_X / HudLayout::CANVAS_WIDTH * 100.0f);
				g_Config.m_TcKeystrokeHudPosY = (int)(Layout.m_Y / HudLayout::CANVAS_HEIGHT * 100.0f);
				break;
			case MODULE_KEYSTROKES_MOUSE:
				g_Config.m_TcKeystrokeHudMousePosX = (int)(Layout.m_X / HudLayout::CANVAS_WIDTH * 100.0f);
				g_Config.m_TcKeystrokeHudMousePosY = (int)(Layout.m_Y / HudLayout::CANVAS_HEIGHT * 100.0f);
				break;
			case MODULE_MA_SPECTATORS:
				g_Config.m_MaSpectatorPanelHudX = round_to_int(Layout.m_X);
				g_Config.m_MaSpectatorPanelHudY = round_to_int(Layout.m_Y);
				g_Config.m_MaSpectatorPanelHudScale = Layout.m_Scale;
				break;
			default:
				break;
			}
		}

		bool IsLegacyModule(EModule Module)
		{
			return Module >= MODULE_MINI_VOTE && Module <= MODULE_LOCAL_TIME;
		}

		SModuleLayout ResolveBaseLayout(EModule Module, float HudWidth, float HudHeight)
		{
			SModuleLayout Layout = gs_aModuleLayouts[Module];
			if(IsLegacyModule(Module) && !HasRuntimeOverrideInternal(Module))
			{
				switch(Module)
				{
				case MODULE_MINI_VOTE:
					Layout.m_X = 0.0f;
					Layout.m_Y = 60.0f;
					break;
				case MODULE_FROZEN_HUD:
					Layout.m_X = (float)round_to_int(HudWidth * 0.595f);
					Layout.m_Y = 0.0f;
					break;
				case MODULE_MOVEMENT_INFO:
					Layout.m_X = (float)round_to_int(HudWidth - 62.0f);
					Layout.m_Y = 205.0f;
					break;
				case MODULE_NOTIFY_LAST:
					Layout.m_X = (float)round_to_int(HudWidth * 0.2f);
					Layout.m_Y = (float)round_to_int(HudHeight * 0.01f);
					break;
				case MODULE_FPS:
					Layout.m_X = (float)round_to_int(HudWidth - 26.0f);
					Layout.m_Y = 5.0f;
					break;
				case MODULE_PING:
					Layout.m_X = (float)round_to_int(HudWidth - 26.0f);
					Layout.m_Y = 20.0f;
					break;
				case MODULE_GAME_TIMER:
					Layout.m_X = (float)round_to_int(HudWidth * 0.5f - 22.0f);
					Layout.m_Y = -2.0f;
					break;
				case MODULE_LOCAL_TIME:
					Layout.m_X = HudWidth / 7.0f * 3.0f;
					Layout.m_Y = 0.0f;
					break;
		case MODULE_KEYSTROKES_KEYBOARD:
				{
					float X = HudLayout::CANVAS_WIDTH * (g_Config.m_TcKeystrokeHudPosX / 100.0f);
					float Y = HudLayout::CANVAS_HEIGHT * (g_Config.m_TcKeystrokeHudPosY / 100.0f);
					if(HasRuntimeOverrideInternal(Module))
					{
						SModuleLayout Override = gs_aRuntimeModuleLayouts[Module];
						return {Override.m_X, Override.m_Y, Override.m_Scale, Override.m_Mode, Override.m_Enabled, Override.m_BackgroundEnabled, Override.m_BackgroundColor};
					}
					return {X, Y, Layout.m_Scale, Layout.m_Mode, Layout.m_Enabled, Layout.m_BackgroundEnabled, Layout.m_BackgroundColor};
				}
			case MODULE_KEYSTROKES_MOUSE:
				{
					float X = HudLayout::CANVAS_WIDTH * (g_Config.m_TcKeystrokeHudMousePosX / 100.0f);
					float Y = HudLayout::CANVAS_HEIGHT * (g_Config.m_TcKeystrokeHudMousePosY / 100.0f);
					if(HasRuntimeOverrideInternal(Module))
					{
						SModuleLayout Override = gs_aRuntimeModuleLayouts[Module];
						return {Override.m_X, Override.m_Y, Override.m_Scale, Override.m_Mode, Override.m_Enabled, Override.m_BackgroundEnabled, Override.m_BackgroundColor};
					}
					return {X, Y, Layout.m_Scale, Layout.m_Mode, Layout.m_Enabled, Layout.m_BackgroundEnabled, Layout.m_BackgroundColor};
				}
			default:
					break;
				}
			}
			else
			{
				Layout = ConfigLayout(Module);
				Layout.m_X = CanvasXToHud(Layout.m_X, HudWidth);
			}
			return Layout;
		}

		void ConHudLayoutSet(IConsole::IResult *pResult, void *pUserData)
		{
			(void)pUserData;

			const int ModuleIndex = pResult->GetInteger(0);
			if(ModuleIndex < 0 || ModuleIndex >= MODULE_COUNT)
				return;
			if(ModuleIndex == MODULE_GAME_TIMER)
				return;

			const EModule Module = (EModule)ModuleIndex;
			SModuleLayout Layout = ConfigLayout(Module);
			Layout.m_X = pResult->GetFloat(1);
			Layout.m_Y = pResult->GetFloat(2);
			Layout.m_Scale = std::clamp(pResult->GetInteger(3), 25, 300);
			Layout.m_Mode = pResult->GetInteger(4);
			Layout.m_BackgroundEnabled = pResult->GetInteger(5) != 0;
			Layout.m_BackgroundColor = (unsigned)pResult->GetInteger(6);
			if(pResult->NumArguments() > 7)
				Layout.m_Enabled = pResult->GetInteger(7) != 0;
			WriteConfigLayout(Module, Layout);
		}

		void ConfigSaveCallback(IConfigManager *pConfigManager, void *pUserData)
		{
			(void)pUserData;
			EnsureRuntimeLayouts();

			char aLine[256];
			for(int Module = 0; Module < MODULE_COUNT; ++Module)
			{
				if(Module == MODULE_GAME_TIMER)
					continue;
				const EModule ModuleId = (EModule)Module;
				const SModuleLayout Layout = ConfigLayout(ModuleId);
				str_format(
					aLine,
					sizeof(aLine),
					"hud_layout_set %d %.3f %.3f %d %d %d %u %d",
					Module,
					Layout.m_X,
					Layout.m_Y,
					Layout.m_Scale,
					Layout.m_Mode,
					Layout.m_BackgroundEnabled ? 1 : 0,
					Layout.m_BackgroundColor,
					Layout.m_Enabled ? 1 : 0);
				pConfigManager->WriteLine(aLine, ConfigDomain::MA);
			}
		}

	} // namespace

	SModuleLayout Get(EModule Module, float HudWidth, float HudHeight)
	{
		SModuleLayout Layout = ResolveBaseLayout(Module, HudWidth, HudHeight);
		if(Module == MODULE_HOOK_COMBO)
		{
			const float UserScale = 1.0f;
			Layout.m_Scale = round_to_int(Layout.m_Scale * UserScale);
		}
		return Layout;
	}

	bool HasRuntimeOverride(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT && HasRuntimeOverrideInternal(Module);
	}

	bool IsEditableModule(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT;
	}

	const char *Name(EModule Module)
	{
		return Module >= 0 && Module < MODULE_COUNT ? gs_apModuleNames[Module] : "HUD Module";
	}

	void SetPosition(EModule Module, float X, float Y)
	{
		SModuleLayout Layout = ConfigLayout(Module);
		Layout.m_X = X;
		Layout.m_Y = Y;
		WriteConfigLayout(Module, Layout);
	}

	void SetScale(EModule Module, int Scale)
	{
		SModuleLayout Layout = ConfigLayout(Module);
		Layout.m_Scale = std::clamp(Scale, 25, 300);
		WriteConfigLayout(Module, Layout);
	}

	void SetEnabled(EModule Module, bool Enabled)
	{
		SModuleLayout Layout = ConfigLayout(Module);
		Layout.m_Enabled = Enabled;
		WriteConfigLayout(Module, Layout);
	}

	bool IsEnabled(EModule Module)
	{
		return ConfigLayout(Module).m_Enabled;
	}

	void ResetPosition(EModule Module)
	{
		SModuleLayout Layout = ConfigLayout(Module);
		Layout.m_X = gs_aModuleLayouts[Module].m_X;
		Layout.m_Y = gs_aModuleLayouts[Module].m_Y;
		WriteConfigLayout(Module, Layout);
	}

	void ResetSettings(EModule Module)
	{
		WriteConfigLayout(Module, gs_aModuleLayouts[Module]);
	}

	void Reset(EModule Module)
	{
		ResetSettings(Module);
	}

	void ResetEditableModules()
	{
		for(int Module = 0; Module < MODULE_COUNT; ++Module)
		{
			if(IsEditableModule((EModule)Module))
				Reset((EModule)Module);
		}
	}

	SModuleRect ClampRectToScreen(const SModuleRect &Rect, float HudWidth, float HudHeight)
	{
		SModuleRect Result = Rect;
		Result.m_X = std::clamp(Result.m_X, 0.0f, maximum(0.0f, HudWidth - Result.m_W));
		Result.m_Y = std::clamp(Result.m_Y, 0.0f, maximum(0.0f, HudHeight - Result.m_H));
		return Result;
	}

	float CanvasXToHud(float CanvasX, float HudWidth)
	{
		return CanvasX * (HudWidth / CANVAS_WIDTH);
	}

	int BackgroundCorners(int DefaultCorners, float RectX, float RectY, float RectW, float RectH, float CanvasWidth, float CanvasHeight)
	{
		int Corners = DefaultCorners;
		const float Eps = 0.01f;
		if(RectW <= 0.0f || RectH <= 0.0f)
			return Corners;

		if(RectX <= Eps)
			Corners &= ~IGraphics::CORNER_L;
		if(RectX + RectW >= CanvasWidth - Eps)
			Corners &= ~IGraphics::CORNER_R;
		if(RectY <= Eps)
			Corners &= ~IGraphics::CORNER_T;
		if(RectY + RectH >= CanvasHeight - Eps)
			Corners &= ~IGraphics::CORNER_B;
		return Corners;
	}

	void OnConsoleInit(IConsole *pConsole, IConfigManager *pConfigManager)
	{
		if(!gs_ConsoleCommandRegistered && pConsole)
		{
			pConsole->Register(
				"hud_layout_set",
				"i[module] f[x] f[y] i[scale] i[mode] i[background_enabled] i[background_color] ?i[enabled]",
				CFGFLAG_CLIENT,
				ConHudLayoutSet,
				nullptr,
				"Set HUD module layout entry");
			gs_ConsoleCommandRegistered = true;
		}

		if(!gs_ConfigCallbackRegistered && pConfigManager)
		{
			pConfigManager->RegisterCallback(ConfigSaveCallback, nullptr, ConfigDomain::MA);
			gs_ConfigCallbackRegistered = true;
		}
	}

} // namespace HudLayout
