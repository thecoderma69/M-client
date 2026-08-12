#include <base/log.h>
#include <base/math.h>
#include <base/system.h>
#include <base/types.h>

#include <engine/font_icons.h>
#include <engine/friends.h>
#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/shared/localization.h>
#include <engine/shared/protocol7.h>
#include <engine/storage.h>
#include <engine/textrender.h>
#include <engine/updater.h>

#include <game/client/animstate.h>
#include <game/client/components/binds.h>
#include <game/client/components/chat.h>
#include <game/client/components/countryflags.h>
#include <game/client/components/media_decoder.h>
#include <game/client/components/menu_background.h>
#include <game/client/components/menus.h>
#include <game/client/components/ma_name_effects.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/bindchat.h>
#include <game/client/components/tclient/bindwheel.h>
#include <game/client/components/tclient/trails.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/skin.h>
#include <game/client/ui.h>
#include <game/client/ui_listbox.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

enum
{
	TCLIENT_TAB_SETTINGS = 0,
	TCLIENT_TAB_BINDWHEEL,
	TCLIENT_TAB_WARLIST,
	TCLIENT_TAB_BINDCHAT,
	TCLIENT_TAB_STATUSBAR,
	TCLIENT_TAB_LLUVIA,
	TCLIENT_TAB_ANIMELOVE,
	TCLIENT_TAB_INFO,
	NUMBER_OF_TCLIENT_TABS
};

enum
{
	MA_TAB_CONFIGURACION = 0,
	MA_TAB_NICK_NAMES,
	MA_TAB_VISUAL,
	MA_TAB_GIF,
	MA_TAB_LLUVIA,
	MA_TAB_ANIMELOVE,
	MA_TAB_KEYSTROKE,
	MA_TAB_EDITOR_SKINS,
	NUMBER_OF_MA_TABS
};

enum
{
	TCLIENT_SOUND_FRIEND_JOIN = 0,
	TCLIENT_SOUND_MAP_FINISH,
	TCLIENT_SOUND_HIGHLIGHT,
};

typedef struct
{
	const char *m_pName;
	const char *m_pCommand;
	int m_KeyId;
	int m_ModifierCombination;
} CKeyInfo;

static float s_Time = 0.0f;
static bool s_StartedTime = false;

const float FontSize = 14.0f;
const float EditBoxFontSize = 12.0f;
const float LineSize = 20.0f;
const float ColorPickerLineSize = 25.0f;
const float HeadlineFontSize = 20.0f;
const float StandardFontSize = 14.0f;

const float HeadlineHeight = HeadlineFontSize + 0.0f;
const float Margin = 10.0f;
const float MarginSmall = 5.0f;
const float MarginExtraSmall = 2.5f;
const float MarginBetweenSections = 30.0f;
const float MarginBetweenViews = 30.0f;

const float ColorPickerLabelSize = 13.0f;
const float ColorPickerLineSpacing = 5.0f;

static void SetFlag(int32_t &Flags, int n, bool Value)
{
	if(Value)
		Flags |= (1 << n);
	else
		Flags &= ~(1 << n);
}

static bool IsFlagSet(int32_t Flags, int n)
{
	return (Flags & (1 << n)) != 0;
}

struct SMaNameEffectMenuEntry
{
	char m_aName[MAX_NAME_LENGTH] = "";
	int m_Style = 0;
	unsigned m_Color1 = 65425;
	unsigned m_Color2 = 41131;
	int m_Glow = 70;
	int m_Moving = 0;
	int m_Stars = 0;
};

struct SMaNameEffectPlayerRow
{
	char m_aName[MAX_NAME_LENGTH] = "";
	char m_aClan[MAX_CLAN_LENGTH] = "";
	bool m_Online = false;
	bool m_Configured = false;
	int m_ClientId = -1;
};

static void MaNameEffectSanitizeName(char *pName)
{
	for(char *pCursor = pName; *pCursor; ++pCursor)
	{
		if(*pCursor == ';' || *pCursor == '|')
			*pCursor = ' ';
	}
}

static void MaNameEffectCopyTrimmedString(char *pDst, int DstSize, const std::string &Value)
{
	size_t Start = 0;
	size_t End = Value.size();
	while(Start < End && (Value[Start] == ' ' || Value[Start] == '\t'))
		++Start;
	while(End > Start && (Value[End - 1] == ' ' || Value[End - 1] == '\t'))
		--End;
	str_copy(pDst, Value.substr(Start, End - Start).c_str(), DstSize);
	MaNameEffectSanitizeName(pDst);
}

static unsigned MaNameEffectParseMenuColor(const char *pText, unsigned DefaultColor)
{
	if(!pText || pText[0] == '\0')
		return DefaultColor;
	const int64_t Value = str_toint64_base(pText, 10);
	return Value < 0 ? DefaultColor : (unsigned)Value;
}

static bool MaNameEffectDecodeRecord(const std::string &Record, SMaNameEffectMenuEntry &Entry)
{
	std::array<std::string, 7> Fields;
	size_t Start = 0;
	for(size_t i = 0; i < Fields.size(); ++i)
	{
		const size_t End = Record.find('|', Start);
		Fields[i] = Record.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
		if(End == std::string::npos)
			break;
		Start = End + 1;
	}

	MaNameEffectCopyTrimmedString(Entry.m_aName, sizeof(Entry.m_aName), Fields[0]);
	if(Entry.m_aName[0] == '\0')
		return false;
	const int RawStyle = Fields[1].empty() ? g_Config.m_MaNameEffectsStyle : str_toint(Fields[1].c_str());
	const bool HasStarsField = !Fields[6].empty();
	Entry.m_Style = std::clamp(HasStarsField ? RawStyle : MaNameEffects::NormalizeLegacyStyle(RawStyle), 0, MaNameEffects::STYLE_MAX);
	Entry.m_Color1 = MaNameEffectParseMenuColor(Fields[2].c_str(), g_Config.m_MaNameEffectsColor1);
	Entry.m_Color2 = MaNameEffectParseMenuColor(Fields[3].c_str(), g_Config.m_MaNameEffectsColor2);
	Entry.m_Glow = std::clamp(Fields[4].empty() ? g_Config.m_MaNameEffectsGlow : str_toint(Fields[4].c_str()), 0, 100);
	Entry.m_Moving = Fields[5].empty() ? g_Config.m_MaNameEffectsMoving : (str_toint(Fields[5].c_str()) != 0);
	Entry.m_Stars = HasStarsField ? (str_toint(Fields[6].c_str()) != 0) : (g_Config.m_MaNameEffectsStars != 0 || MaNameEffects::LegacyStyleHasStars(RawStyle));
	return true;
}

static void MaNameEffectDecodeEntries(std::vector<SMaNameEffectMenuEntry> &vEntries)
{
	vEntries.clear();
	std::string Data(g_Config.m_MaNameEffectsEntries);
	size_t Start = 0;
	while(Start < Data.size())
	{
		const size_t End = Data.find(';', Start);
		const std::string Record = Data.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
		SMaNameEffectMenuEntry Entry;
		if(MaNameEffectDecodeRecord(Record, Entry))
			vEntries.push_back(Entry);
		if(End == std::string::npos)
			break;
		Start = End + 1;
	}
}

static void MaNameEffectEncodeEntries(const std::vector<SMaNameEffectMenuEntry> &vEntries)
{
	std::string Out;
	Out.reserve(sizeof(g_Config.m_MaNameEffectsEntries));
	for(const SMaNameEffectMenuEntry &Entry : vEntries)
	{
		if(Entry.m_aName[0] == '\0')
			continue;
		char aRecord[256];
		str_format(aRecord, sizeof(aRecord), "%s|%d|%u|%u|%d|%d|%d", Entry.m_aName, std::clamp(Entry.m_Style, 0, MaNameEffects::STYLE_MAX), Entry.m_Color1, Entry.m_Color2, std::clamp(Entry.m_Glow, 0, 100), Entry.m_Moving ? 1 : 0, Entry.m_Stars ? 1 : 0);
		if(!Out.empty())
			Out += ';';
		if(Out.size() + str_length(aRecord) + 1 >= sizeof(g_Config.m_MaNameEffectsEntries))
			break;
		Out += aRecord;
	}
	str_copy(g_Config.m_MaNameEffectsEntries, Out.c_str(), sizeof(g_Config.m_MaNameEffectsEntries));
}

static int MaNameEffectFindEntryIndex(const std::vector<SMaNameEffectMenuEntry> &vEntries, const char *pName)
{
	for(size_t i = 0; i < vEntries.size(); ++i)
	{
		if(str_comp_nocase(vEntries[i].m_aName, pName) == 0)
			return (int)i;
	}
	return -1;
}

static void MaNameEffectSaveEntry(std::vector<SMaNameEffectMenuEntry> &vEntries, SMaNameEffectMenuEntry Entry)
{
	MaNameEffectSanitizeName(Entry.m_aName);
	if(Entry.m_aName[0] == '\0')
		return;
	Entry.m_Style = std::clamp(Entry.m_Style, 0, MaNameEffects::STYLE_MAX);
	Entry.m_Glow = std::clamp(Entry.m_Glow, 0, 100);
	Entry.m_Moving = Entry.m_Moving ? 1 : 0;
	Entry.m_Stars = Entry.m_Stars ? 1 : 0;

	g_Config.m_MaNameEffects = 1;

	const int ExistingIndex = MaNameEffectFindEntryIndex(vEntries, Entry.m_aName);
	if(ExistingIndex >= 0)
		vEntries[ExistingIndex] = Entry;
	else
		vEntries.push_back(Entry);
	MaNameEffectEncodeEntries(vEntries);
}

static SMaNameEffectMenuEntry MaNameEffectMakeEntry(const char *pName, int Style, unsigned Color1, unsigned Color2, int Glow, int Moving, int Stars)
{
	SMaNameEffectMenuEntry Entry;
	str_copy(Entry.m_aName, pName ? pName : "", sizeof(Entry.m_aName));
	MaNameEffectSanitizeName(Entry.m_aName);
	Entry.m_Style = std::clamp(Style, 0, MaNameEffects::STYLE_MAX);
	Entry.m_Color1 = Color1;
	Entry.m_Color2 = Color2;
	Entry.m_Glow = std::clamp(Glow, 0, 100);
	Entry.m_Moving = Moving ? 1 : 0;
	Entry.m_Stars = Stars ? 1 : 0;
	return Entry;
}

static ColorHSLA MaNameEffectMenuColorHsla(unsigned ColorValue)
{
	ColorHSLA Color = (ColorValue & 0xff000000U) != 0 ? ColorHSLA(ColorValue, true) : ColorHSLA(ColorValue);
	Color.a = 1.0f;
	return Color;
}

static unsigned MaNameEffectNormalizeMenuColor(unsigned ColorValue)
{
	return MaNameEffectMenuColorHsla(ColorValue).Pack(false);
}

static unsigned MaNameEffectDefaultColor1()
{
	return MaNameEffectNormalizeMenuColor(color_cast<ColorHSLA>(ColorRGBA(1.0f, 1.0f, 1.0f)).Pack(false));
}

static unsigned MaNameEffectDefaultColor2()
{
	return MaNameEffectNormalizeMenuColor(color_cast<ColorHSLA>(ColorRGBA(1.0f, 1.0f, 1.0f)).Pack(false));
}

static ColorRGBA MaNameEffectMenuColor(unsigned ColorValue)
{
	return color_cast<ColorRGBA>(MaNameEffectMenuColorHsla(ColorValue));
}

static void MaNameEffectAddLegacyNames(std::vector<SMaNameEffectMenuEntry> &vEntries)
{
	if(g_Config.m_MaNameEffectsEntries[0] != '\0' || g_Config.m_MaNameEffectsNames[0] == '\0')
		return;

	const char *pCursor = g_Config.m_MaNameEffectsNames;
	while(*pCursor)
	{
		while(*pCursor == ';' || *pCursor == ',' || *pCursor == '|' || *pCursor == ' ' || *pCursor == '\t' || *pCursor == '\n' || *pCursor == '\r')
			++pCursor;
		const char *pStart = pCursor;
		while(*pCursor && *pCursor != ';' && *pCursor != ',' && *pCursor != '|' && *pCursor != '\n' && *pCursor != '\r')
			++pCursor;
		std::string Name(pStart, pCursor - pStart);
		SMaNameEffectMenuEntry Entry;
		MaNameEffectCopyTrimmedString(Entry.m_aName, sizeof(Entry.m_aName), Name);
		if(Entry.m_aName[0] != '\0' && MaNameEffectFindEntryIndex(vEntries, Entry.m_aName) < 0)
		{
			Entry.m_Style = std::clamp(g_Config.m_MaNameEffectsStyle, 0, MaNameEffects::STYLE_MAX);
			Entry.m_Color1 = MaNameEffectDefaultColor1();
			Entry.m_Color2 = MaNameEffectDefaultColor2();
			Entry.m_Glow = std::clamp(g_Config.m_MaNameEffectsGlow, 0, 100);
			Entry.m_Moving = g_Config.m_MaNameEffectsMoving != 0;
			Entry.m_Stars = g_Config.m_MaNameEffectsStars != 0;
			vEntries.push_back(Entry);
		}
	}
	MaNameEffectEncodeEntries(vEntries);
}
static int TClientSoundIdForPack(int Pack, int Event)
{
	static const int s_aaSoundPacks[4][3] = {
		{SOUND_PLAYER_SPAWN, SOUND_CTF_CAPTURE, SOUND_CHAT_HIGHLIGHT},
		{SOUND_CHAT_CLIENT, SOUND_CTF_GRAB_EN, SOUND_CHAT_SERVER},
		{SOUND_HOOK_ATTACH_PLAYER, SOUND_CTF_CAPTURE, SOUND_PICKUP_ARMOR},
		{SOUND_CHAT_CLIENT, SOUND_PLAYER_JUMP, SOUND_CHAT_HIGHLIGHT},
	};
	return s_aaSoundPacks[std::clamp(Pack, 0, 3)][std::clamp(Event, 0, 2)];
}

struct SMaAudioPack
{
	char m_aName[64];
	int m_GameSoundCount = 0;
};

struct SMaAudioSoundInfo
{
	const char *m_pLabel;
	const char *m_pFiles;
	int m_TestSound;
};

static const SMaAudioSoundInfo s_aMaAudioSoundInfos[] = {
	{"Gancho", "hook_attach, hook_loop, hook_noattach", SOUND_HOOK_ATTACH_GROUND},
	{"Pistola", "wp_gun_fire", SOUND_GUN_FIRE},
	{"Escopeta", "wp_shotty_fire", SOUND_SHOTGUN_FIRE},
	{"Granada", "wp_flump_launch / wp_flump_explo", SOUND_GRENADE_FIRE},
	{"Martillo", "wp_hammer_swing / wp_hammer_hit", SOUND_HAMMER_HIT},
	{"Laser", "wp_laser_fire / wp_laser_bnce", SOUND_LASER_FIRE},
	{"Cambio arma", "wp_switch", SOUND_WEAPON_SWITCH},
	{"Movimiento", "foley_foot, foley_land, foley_dbljump", SOUND_PLAYER_JUMP},
};

static const char *s_apMaAudioGameSoundFiles[] = {
	"hook_attach-01.wv",
	"hook_attach-02.wv",
	"hook_attach-03.wv",
	"hook_loop-01.wv",
	"hook_loop-02.wv",
	"hook_noattach-01.wv",
	"hook_noattach-02.wv",
	"hook_noattach-03.wv",
	"wp_gun_fire-01.wv",
	"wp_gun_fire-02.wv",
	"wp_gun_fire-03.wv",
	"wp_shotty_fire-01.wv",
	"wp_shotty_fire-02.wv",
	"wp_shotty_fire-03.wv",
	"wp_flump_launch-01.wv",
	"wp_flump_launch-02.wv",
	"wp_flump_launch-03.wv",
	"wp_flump_explo-01.wv",
	"wp_flump_explo-02.wv",
	"wp_flump_explo-03.wv",
	"wp_hammer_swing-01.wv",
	"wp_hammer_swing-02.wv",
	"wp_hammer_swing-03.wv",
	"wp_hammer_hit-01.wv",
	"wp_hammer_hit-02.wv",
	"wp_hammer_hit-03.wv",
	"wp_laser_fire-01.wv",
	"wp_laser_fire-02.wv",
	"wp_laser_fire-03.wv",
	"wp_laser_bnce-01.wv",
	"wp_laser_bnce-02.wv",
	"wp_laser_bnce-03.wv",
	"wp_switch-01.wv",
	"wp_switch-02.wv",
	"wp_switch-03.wv",
	"foley_foot_left-01.wv",
	"foley_foot_right-01.wv",
	"foley_land-01.wv",
	"foley_dbljump-01.wv",
	"sfx_pickup_gun.wv",
	"sfx_pickup_sg.wv",
	"sfx_pickup_launcher.wv",
	"sfx_pickup_ninja.wv",
};

static void MaAudioReplaceExtension(const char *pFilename, const char *pExtension, char *pBuffer, int BufferSize)
{
	str_copy(pBuffer, pFilename, BufferSize);

	int Dot = -1;
	for(int i = str_length(pBuffer) - 1; i >= 0; --i)
	{
		if(pBuffer[i] == '/' || pBuffer[i] == '\\')
			break;
		if(pBuffer[i] == '.')
		{
			Dot = i;
			break;
		}
	}
	if(Dot >= 0)
		pBuffer[Dot] = '\0';
	str_append(pBuffer, pExtension, BufferSize);
}

static bool MaAudioPackHasGameSoundFile(IStorage *pStorage, const char *pPackName, const char *pFileName)
{
	char aPath[IO_MAX_PATH_LENGTH];
	auto CheckFilename = [&](const char *pCandidateName) {
		str_format(aPath, sizeof(aPath), "audio/%s/%s", pPackName, pCandidateName);
		if(pStorage->FileExists(aPath, IStorage::TYPE_ALL))
			return true;
		str_format(aPath, sizeof(aPath), "assets/audio/%s/%s", pPackName, pCandidateName);
		return pStorage->FileExists(aPath, IStorage::TYPE_ALL);
	};

	if(CheckFilename(pFileName))
		return true;

	char aWavFilename[IO_MAX_PATH_LENGTH];
	MaAudioReplaceExtension(pFileName, ".wav", aWavFilename, sizeof(aWavFilename));
	return str_comp(aWavFilename, pFileName) != 0 && CheckFilename(aWavFilename);
}

static int MaAudioPackGameSoundCount(IStorage *pStorage, const char *pPackName)
{
	if(str_comp(pPackName, "default") == 0)
		return std::size(s_apMaAudioGameSoundFiles);

	int Count = 0;
	for(const char *pFileName : s_apMaAudioGameSoundFiles)
	{
		if(MaAudioPackHasGameSoundFile(pStorage, pPackName, pFileName))
			++Count;
	}
	return Count;
}

static bool MaAudioPackExists(const std::vector<SMaAudioPack> &vPacks, const char *pName)
{
	for(const SMaAudioPack &Pack : vPacks)
	{
		if(str_comp(Pack.m_aName, pName) == 0)
			return true;
	}
	return false;
}

static int MaAudioPackScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	auto *pPacks = static_cast<std::vector<SMaAudioPack> *>(pUser);
	if(!IsDir || pName[0] == '.' || str_comp(pName, "default") == 0 || MaAudioPackExists(*pPacks, pName))
		return 0;

	SMaAudioPack Pack;
	str_copy(Pack.m_aName, pName);
	pPacks->push_back(Pack);
	return 0;
}


static bool MaStringListContains(const std::vector<std::string> &vItems, const char *pName)
{
	for(const std::string &Item : vItems)
	{
		if(str_comp(Item.c_str(), pName) == 0)
			return true;
	}
	return false;
}

static int MaKeystrokePackScan(const char *pName, int IsDir, int DirType, void *pUser)
{
	(void)DirType;
	auto *pPacks = static_cast<std::vector<std::string> *>(pUser);
	if(!IsDir || pName[0] == '.' || MaStringListContains(*pPacks, pName))
		return 0;

	pPacks->emplace_back(pName);
	return 0;
}static const char *MaAudioPackDisplayName(const char *pName)
{
	if(str_comp(pName, "ma_space_pulse") == 0 || str_comp(pName, "ma_fx") == 0)
        return "M\316\233 \343\203\204 Space Pulse";
	if(str_comp(pName, "ma_retro_arcade") == 0)
        return "M\316\233 \343\203\204 Retro Arcade";
	if(str_comp(pName, "ma_demon_core") == 0)
        return "M\316\233 \343\203\204 Demon Core";
	if(str_comp(pName, "ma_magic_stars") == 0)
        return "M\316\233 \343\203\204 Magic Stars";
	if(str_comp(pName, "ma_dark_void") == 0)
        return "M\316\233 \343\203\204 Dark Void";
	return pName;
}

static ColorRGBA TClientThemeCustomColor(unsigned int ColorValue, float MinAlpha, float MaxAlpha)
{
	ColorRGBA Color = color_cast<ColorRGBA>(ColorHSLA(ColorValue, true));
	Color.a = std::clamp(Color.a, MinAlpha, MaxAlpha);
	return Color;
}

static ColorRGBA TClientThemePanelColor()
{
	if(g_Config.m_TcThemeCustomColors)
		return TClientThemeCustomColor(g_Config.m_TcThemePanelColor, 0.12f, 0.72f);
	switch(std::clamp(g_Config.m_TcTheme, 0, 3))
	{
	case 1:
		return ColorRGBA(0.20f, 0.04f, 0.13f, 0.38f);
	case 2:
		return ColorRGBA(0.02f, 0.12f, 0.16f, 0.42f);
	case 3:
		return ColorRGBA(0.10f, 0.10f, 0.10f, 0.18f);
	case 0:
	default:
		return ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
	}
}

static ColorRGBA TClientThemeAccentColor()
{
	if(g_Config.m_TcThemeCustomColors)
		return TClientThemeCustomColor(g_Config.m_TcThemeAccentColor, 0.35f, 1.0f);
	switch(std::clamp(g_Config.m_TcTheme, 0, 3))
	{
	case 1:
		return ColorRGBA(1.0f, 0.34f, 0.64f, 0.82f);
	case 2:
		return ColorRGBA(0.0f, 0.88f, 1.0f, 0.80f);
	case 3:
		return ColorRGBA(0.78f, 0.78f, 0.78f, 0.55f);
	case 0:
	default:
		return ColorRGBA(0.36f, 0.48f, 1.0f, 0.55f);
	}
}

static ColorRGBA TClientThemeBackgroundColor()
{
	if(g_Config.m_TcThemeCustomBackground)
		return TClientThemeCustomColor(g_Config.m_TcThemeBackgroundColor, 0.06f, 0.34f);
	switch(std::clamp(g_Config.m_TcTheme, 0, 3))
	{
	case 1:
		return ColorRGBA(0.35f, 0.03f, 0.19f, 0.16f);
	case 2:
		return ColorRGBA(0.0f, 0.23f, 0.28f, 0.16f);
	case 3:
		return ColorRGBA(0.70f, 0.70f, 0.70f, 0.08f);
	case 0:
	default:
		return ColorRGBA(0.0f, 0.0f, 0.0f, 0.07f);
	}
}

bool CMenus::DoLine_KeyReader(CUIRect &View, CButtonContainer &ReaderButton, CButtonContainer &ClearButton, const char *pName, const char *pCommand)
{
	CBindSlot Bind(0, 0);
	for(int Mod = 0; Mod < KeyModifier::COMBINATION_COUNT; Mod++)
	{
		for(int KeyId = 0; KeyId < KEY_LAST; KeyId++)
		{
			const char *pBind = GameClient()->m_Binds.Get(KeyId, Mod);
			if(!pBind[0])
				continue;

			if(str_comp(pBind, pCommand) == 0)
			{
				Bind.m_Key = KeyId;
				Bind.m_ModifierMask = Mod;
				break;
			}
		}
	}

	CUIRect KeyButton, KeyLabel;
	View.HSplitTop(LineSize, &KeyButton, &View);
	KeyButton.VSplitMid(&KeyLabel, &KeyButton);

	char aBuf[128];
	str_format(aBuf, sizeof(aBuf), "%s:", pName);
	Ui()->DoLabel(&KeyLabel, aBuf, FontSize, TEXTALIGN_ML);

	View.HSplitTop(MarginExtraSmall, nullptr, &View);

	const auto Result = GameClient()->m_KeyBinder.DoKeyReader(&ReaderButton, &ClearButton, &KeyButton, Bind, false);
	if(Result.m_Bind != Bind)
	{
		if(Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Bind.m_Key, "", false, Bind.m_ModifierMask);
		if(Result.m_Bind.m_Key != KEY_UNKNOWN)
			GameClient()->m_Binds.Bind(Result.m_Bind.m_Key, pCommand, false, Result.m_Bind.m_ModifierMask);
		return true;
	}
	return false;
}

bool CMenus::DoSliderWithScaledValue(const void *pId, int *pOption, const CUIRect *pRect, const char *pStr, int Min, int Max, int Scale, const IScrollbarScale *pScale, unsigned Flags, const char *pSuffix)
{
	const bool NoClampValue = Flags & CUi::SCROLLBAR_OPTION_NOCLAMPVALUE;

	int Value = *pOption;
	Min /= Scale;
	Max /= Scale;
	// Allow adjustment of slider options when ctrl is pressed (to avoid scrolling, or accidentally adjusting the value)
	int Increment = std::max(1, (Max - Min) / 35);
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_UP) && Ui()->MouseInside(pRect))
	{
		Value += Increment;
		Value = std::clamp(Value, Min, Max);
	}
	if(Input()->ModifierIsPressed() && Input()->KeyPress(KEY_MOUSE_WHEEL_DOWN) && Ui()->MouseInside(pRect))
	{
		Value -= Increment;
		Value = std::clamp(Value, Min, Max);
	}

	char aBuf[256];
	str_format(aBuf, sizeof(aBuf), "%s: %i%s", pStr, Value * Scale, pSuffix);

	if(NoClampValue)
	{
		// Clamp the value internally for the scrollbar
		Value = std::clamp(Value, Min, Max);
	}

	CUIRect Label, ScrollBar;
	pRect->VSplitMid(&Label, &ScrollBar, minimum(10.0f, pRect->w * 0.05f));

	const float LabelFontSize = Label.h * CUi::ms_FontmodHeight * 0.8f;
	Ui()->DoLabel(&Label, aBuf, LabelFontSize, TEXTALIGN_ML);

	Value = pScale->ToAbsolute(Ui()->DoScrollbarH(pId, &ScrollBar, pScale->ToRelative(Value, Min, Max)), Min, Max);
	if(NoClampValue && ((Value == Min && *pOption < Min) || (Value == Max && *pOption > Max)))
	{
		Value = *pOption;
	}

	if(*pOption != Value)
	{
		*pOption = Value;
		return true;
	}
	return false;
}

bool CMenus::DoEditBoxWithLabel(CLineInput *LineInput, const CUIRect *pRect, const char *pLabel, const char *pDefault, char *pBuf, size_t BufSize)
{
	CUIRect Button, Label;
	pRect->VSplitLeft(210.0f, &Label, &Button);
	Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML);
	LineInput->SetBuffer(pBuf, BufSize);
	LineInput->SetEmptyText(pDefault);
	return Ui()->DoEditBox(LineInput, &Button, EditBoxFontSize);
}

int CMenus::DoButtonLineSize_Menu(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, float ButtonLineSize, bool Fake, const char *pImageName, int Corners, float Rounding, float FontFactor, ColorRGBA Color)
{
	CUIRect Text = *pRect;

	if(Checked)
		Color = ColorRGBA(0.6f, 0.6f, 0.6f, 0.5f);
	Color.a *= Ui()->ButtonColorMul(pButtonContainer);

	if(Fake)
		Color.a *= 0.5f;

	pRect->Draw(Color, Corners, Rounding);

	Text.HMargin((Text.h - ButtonLineSize) / 2.0f, &Text);
	Text.HMargin(pRect->h >= 20.0f ? 2.0f : 1.0f, &Text);
	Text.HMargin((Text.h * FontFactor) / 2.0f, &Text);
	Ui()->DoLabel(&Text, pText, Text.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);

	if(Fake)
		return 0;

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_LEFT);
}

void CMenus::RenderDevSkin(vec2 RenderPos, float Size, const char *pSkinName, const char *pBackupSkin, bool CustomColors, int FeetColor, int BodyColor, int Emote, bool Rainbow, bool Cute, ColorRGBA ColorFeet, ColorRGBA ColorBody, ColorRGBA ColorEyeLeft, ColorRGBA ColorEyeRight)
{
	bool WhiteFeetTemp = g_Config.m_TcWhiteFeet;
	g_Config.m_TcWhiteFeet = false;

	float DefTick = std::fmod(s_Time, 1.0f);

	CTeeRenderInfo SkinInfo;
	const CSkin *pSkin = GameClient()->m_Skins.Find(pSkinName);
	if(str_comp(pSkin->GetName(), pSkinName) != 0)
		pSkin = GameClient()->m_Skins.Find(pBackupSkin);

	SkinInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	SkinInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	SkinInfo.m_SkinMetrics = pSkin->m_Metrics;
	SkinInfo.m_CustomColoredSkin = CustomColors;
	if(SkinInfo.m_CustomColoredSkin)
	{
		SkinInfo.m_ColorBody = color_cast<ColorRGBA>(ColorHSLA(BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		SkinInfo.m_ColorFeet = color_cast<ColorRGBA>(ColorHSLA(FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT));
		if(ColorFeet.a != 0.0f)
		{
			SkinInfo.m_ColorBody = ColorBody;
			SkinInfo.m_ColorFeet = ColorFeet;
		}
	}
	else
	{
		SkinInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
		SkinInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	}
	if(Rainbow)
	{
		ColorRGBA Col = color_cast<ColorRGBA>(ColorHSLA(DefTick, 1.0f, 0.5f));
		SkinInfo.m_ColorBody = Col;
		SkinInfo.m_ColorFeet = Col;
	}
	if(ColorEyeLeft.a != 0.0f || ColorEyeRight.a != 0.0f)
	{
		SkinInfo.m_UseCustomEyeColors = true;
		SkinInfo.m_ColorEyeLeft = ColorEyeLeft;
		SkinInfo.m_ColorEyeRight = ColorEyeRight;
	}
	SkinInfo.m_Size = Size;
	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &SkinInfo, OffsetToMid);
	vec2 TeeRenderPos(RenderPos.x, RenderPos.y + OffsetToMid.y);
	if(Cute)
		RenderTeeCute(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos, true);
	else
		RenderTools()->RenderTee(pIdleState, &SkinInfo, Emote, vec2(1.0f, 0.0f), TeeRenderPos);
	g_Config.m_TcWhiteFeet = WhiteFeetTemp;
}

void CMenus::RenderTeeCute(const CAnimState *pAnim, const CTeeRenderInfo *pInfo, int Emote, vec2 Dir, vec2 Pos, bool CuteEyes, float Alpha)
{
	Dir = Ui()->MousePos() - Pos;
	if(pInfo->m_Size > 0.0f)
		Dir /= pInfo->m_Size;
	const float Length = length(Dir);
	if(Length > 1.0f)
		Dir /= Length;
	if(CuteEyes && Length < 0.4f)
		Emote = 2;
	RenderTools()->RenderTee(pAnim, pInfo, Emote, Dir, Pos, Alpha);
}

void CMenus::RenderFontIcon(const CUIRect Rect, const char *pText, float Size, int Align)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	Ui()->DoLabel(&Rect, pText, Size, Align);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
}

int CMenus::DoButtonNoRect_FontIcon(CButtonContainer *pButtonContainer, const char *pText, int Checked, const CUIRect *pRect, int Corners)
{
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING);
	TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
	TextRender()->TextColor(TextRender()->DefaultTextSelectionColor());
	if(Ui()->HotItem() == pButtonContainer)
	{
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	CUIRect Temp;
	pRect->HMargin(0.0f, &Temp);
	Ui()->DoLabel(&Temp, pText, Temp.h * CUi::ms_FontmodHeight, TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);

	return Ui()->DoButtonLogic(pButtonContainer, Checked, pRect, BUTTONFLAG_LEFT);
}

void CMenus::PopupConfirmRemoveWarType()
{
	GameClient()->m_WarList.RemoveWarType(m_pRemoveWarType->m_aWarName);
	m_pRemoveWarType = nullptr;
}

void CMenus::RenderSettingsTClient(CUIRect MainView)
{
	s_Time += Client()->RenderFrameTime() * (1.0f / 100.0f);
	if(!s_StartedTime)
	{
		s_StartedTime = true;
		s_Time = (float)rand() / (float)RAND_MAX;
	}

	static int s_CurCustomTab = 0;

	CUIRect TabBar, Button;
	int TabCount = NUMBER_OF_TCLIENT_TABS - 2;

	MainView.HSplitTop(LineSize, &TabBar, &MainView);
	const float TabWidth = TabBar.w / TabCount;
	static CButtonContainer s_aPageTabs[NUMBER_OF_TCLIENT_TABS] = {};
	const char *apTabNames[] = {
		TCLocalize("Settings"),
		TCLocalize("Bind Wheel"),
		TCLocalize("War List"),
		TCLocalize("Chat Binds"),
		TCLocalize("Status Bar"),
		TCLocalize("Lluvia"),
		TCLocalize("Anime Love"),
		TCLocalize("Info")};

	for(int Tab = 0; Tab < NUMBER_OF_TCLIENT_TABS; ++Tab)
	{
		if(Tab == TCLIENT_TAB_LLUVIA || Tab == TCLIENT_TAB_ANIMELOVE)
			continue;

		if(IsFlagSet(g_Config.m_TcTClientSettingsTabs, Tab))
		{
			TabCount--;
			if(s_CurCustomTab == Tab)
				s_CurCustomTab++;
			continue;
		}

		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == NUMBER_OF_TCLIENT_TABS - 1 ? IGraphics::CORNER_R :
													 IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurCustomTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurCustomTab = Tab;
	}

	MainView.HSplitTop(Margin, nullptr, &MainView);

	CUIRect ContentFrame = MainView;
	ContentFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect ContentInner = MainView;
	ContentInner.Margin(2.0f, &ContentInner);
	ContentInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	MainView.Margin(8.0f, &MainView);

	if(s_CurCustomTab == TCLIENT_TAB_SETTINGS)
		RenderSettingsTClientSettings(MainView);
	if(s_CurCustomTab == TCLIENT_TAB_BINDCHAT)
		RenderSettingsTClientChatBinds(MainView);
	if(s_CurCustomTab == TCLIENT_TAB_BINDWHEEL)
		RenderSettingsTClientBindWheel(MainView);
	if(s_CurCustomTab == TCLIENT_TAB_WARLIST)
		RenderSettingsTClientWarList(MainView);
	if(s_CurCustomTab == TCLIENT_TAB_STATUSBAR)
		RenderSettingsTClientStatusBar(MainView);
	if(s_CurCustomTab == TCLIENT_TAB_INFO)
		RenderSettingsTClientInfo(MainView);
}

void CMenus::RenderSettingsTClientSettings(CUIRect MainView)
{
	CUIRect Column, LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	CUIRect BackgroundView = MainView;
	BackgroundView.Margin(2.0f, &BackgroundView);
	BackgroundView.Draw(TClientThemeBackgroundColor(), IGraphics::CORNER_ALL, 8.0f);

	MainView.y += ScrollOffset.y;

	MainView.VSplitRight(5.0f, &MainView, nullptr); // Padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView); // Padding for scrollbar

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ***** LeftView ***** //
	Column = LeftView;

	// ***** Visual Miscellaneous ***** //
	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Visual"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	static std::vector<const char *> s_FontDropDownNames = {};
	static CUi::SDropDownState s_FontDropDownState;
	static CScrollRegion s_FontDropDownScrollRegion;
	s_FontDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_FontDropDownScrollRegion;
	s_FontDropDownState.m_SelectionPopupContext.m_SpecialFontRenderMode = true;
	int FontSelectedOld = -1;
	for(size_t i = 0; i < TextRender()->GetCustomFaces()->size(); ++i)
	{
		if(s_FontDropDownNames.size() != TextRender()->GetCustomFaces()->size())
			s_FontDropDownNames.push_back(TextRender()->GetCustomFaces()->at(i).c_str());

		if(str_find_nocase(g_Config.m_TcCustomFont, TextRender()->GetCustomFaces()->at(i).c_str()))
			FontSelectedOld = i;
	}
	CUIRect FontDropDownRect, FontDirectory;
	Column.HSplitTop(LineSize, &FontDropDownRect, &Column);
	FontDropDownRect.VSplitLeft(100.0f, &Label, &FontDropDownRect);
	FontDropDownRect.VSplitRight(20.0f, &FontDropDownRect, &FontDirectory);
	FontDropDownRect.VSplitRight(MarginSmall, &FontDropDownRect, nullptr);

	Ui()->DoLabel(&Label, TCLocalize("Custom Font: "), FontSize, TEXTALIGN_ML);
	const int FontSelectedNew = Ui()->DoDropDown(&FontDropDownRect, FontSelectedOld, s_FontDropDownNames.data(), s_FontDropDownNames.size(), s_FontDropDownState);
	if(FontSelectedOld != FontSelectedNew)
	{
		str_copy(g_Config.m_TcCustomFont, s_FontDropDownNames[FontSelectedNew]);
		TextRender()->SetCustomFace(g_Config.m_TcCustomFont);

		// Attempt to reset all the containers
		TextRender()->OnPreWindowResize();
		GameClient()->OnWindowResize();
		GameClient()->Editor()->OnWindowResize();
		TextRender()->OnWindowResize();
		GameClient()->m_MapImages.SetTextureScale(101);
		GameClient()->m_MapImages.SetTextureScale(g_Config.m_ClTextEntitiesSize);
	}

	static CButtonContainer s_FontDirectoryId;
	if(Ui()->DoButton_FontIcon(&s_FontDirectoryId, FontIcon::FOLDER, 0, &FontDirectory, IGraphics::CORNER_ALL))
	{
		Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("tclient/fonts", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "tclient/fonts", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	CUIRect TinyTeeConfig;
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

	{
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
		static std::vector<const char *> s_DropDownNames;
		s_DropDownNames = {TCLocalize("Normal", "Hammer Mode"), TCLocalize("Rotate with cursor", "Hammer Mode"), TCLocalize("Rotate with cursor like gun", "Hammer Mode")};
		static CUi::SDropDownState s_DropDownState;
		static CScrollRegion s_DropDownScrollRegion;
		s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Hammer Mode: "), FontSize, TEXTALIGN_ML);
		g_Config.m_TcHammerRotatesWithCursor = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcHammerRotatesWithCursor, s_DropDownNames.data(), s_DropDownNames.size(), s_DropDownState);
		Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	}

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcCursorScale, &g_Config.m_TcCursorScale, &Button, TCLocalize("Ingame cursor scale"), 0, 500, &CUi::ms_LinearScrollbarScale, 0, "%");

	Column.HSplitTop(LineSize, &Button, &Column);
	if(g_Config.m_TcAnimateWheelTime > 0)
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimateWheelTime, &g_Config.m_TcAnimateWheelTime, &Button, TCLocalize("Wheel animate"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "ms");
	else
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimateWheelTime, &g_Config.m_TcAnimateWheelTime, &Button, TCLocalize("Wheel animate"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, "ms (off)");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Input ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Input"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, TCLocalize("Fast Input (reduced visual delay)"), &g_Config.m_TcFastInput, &Column, LineSize);

	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcFastInputAmount, &g_Config.m_TcFastInputAmount, &Button, TCLocalize("Amount"), 1, 40, 1, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	if(g_Config.m_TcFastInput)
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, TCLocalize("Fast Input others"), &g_Config.m_TcFastInputOthers, &Column, LineSize);
	else
		Column.HSplitTop(LineSize, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, TCLocalize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &Column, LineSize);

	// A little extra spacing because these are multi line
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Anti Latency Tools ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anti Latency Tools"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_ClPredictionMargin, &g_Config.m_ClPredictionMargin, &Button, TCLocalize("Prediction Margin"), 10, 75, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRemoveAnti, TCLocalize("Remove prediction & antiping in freeze"), &g_Config.m_TcRemoveAnti, &Column, LineSize);
	if(g_Config.m_TcRemoveAnti)
	{
		if(g_Config.m_TcUnfreezeLagDelayTicks < g_Config.m_TcUnfreezeLagTicks)
			g_Config.m_TcUnfreezeLagDelayTicks = g_Config.m_TcUnfreezeLagTicks;
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagTicks, &g_Config.m_TcUnfreezeLagTicks, &Button, TCLocalize("Amount"), 100, 300, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
		Column.HSplitTop(LineSize, &Button, &Column);
		DoSliderWithScaledValue(&g_Config.m_TcUnfreezeLagDelayTicks, &g_Config.m_TcUnfreezeLagDelayTicks, &Button, TCLocalize("Delay"), 100, 3000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	}
	else
		Column.HSplitTop(LineSize * 2, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcUnpredOthersInFreeze, TCLocalize("Dont predict other players if you are frozen"), &g_Config.m_TcUnpredOthersInFreeze, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPredMarginInFreeze, TCLocalize("Adjust your prediction margin while frozen"), &g_Config.m_TcPredMarginInFreeze, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	if(g_Config.m_TcPredMarginInFreeze)
		Ui()->DoScrollbarOption(&g_Config.m_TcPredMarginInFreezeAmount, &g_Config.m_TcPredMarginInFreezeAmount, &Button, TCLocalize("Frozen Margin"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "ms");
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Improved Anti Ping ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anti Ping Smoothing"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingImproved, TCLocalize("Use new smoothing algorithm"), &g_Config.m_TcAntiPingImproved, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingStableDirection, TCLocalize("Optimistic prediction in stable direction"), &g_Config.m_TcAntiPingStableDirection, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAntiPingNegativeBuffer, TCLocalize("Remember instability for longer"), &g_Config.m_TcAntiPingNegativeBuffer, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcAntiPingUncertaintyScale, &g_Config.m_TcAntiPingUncertaintyScale, &Button, TCLocalize("Uncertainty duration"), 50, 400, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "%");
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Execute on join ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);

	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Auto execute"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		CUIRect Box;
		Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
		Box.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, Localize("Execute before connect"), FontSize, TEXTALIGN_ML);
		static CLineInput s_LineInput(g_Config.m_TcExecuteOnConnect, sizeof(g_Config.m_TcExecuteOnConnect));
		s_LineInput.SetEmptyText(TCLocalize("Run a console command before connect"));

		Ui()->DoEditBox(&s_LineInput, &Button, EditBoxFontSize);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	{
		CUIRect Box;
		Column.HSplitTop(LineSize + MarginExtraSmall, &Box, &Column);
		Box.VSplitMid(&Label, &Button);
		Ui()->DoLabel(&Label, Localize("Execute on join"), FontSize, TEXTALIGN_ML);
		static CLineInput s_LineInput(g_Config.m_TcExecuteOnJoin, sizeof(g_Config.m_TcExecuteOnJoin));
		s_LineInput.SetEmptyText(TCLocalize("Run a console command on join"));

		Ui()->DoEditBox(&s_LineInput, &Button, EditBoxFontSize);
	}

	Column.HSplitTop(LineSize, &Button, &Column);
	DoSliderWithScaledValue(&g_Config.m_TcExecuteOnJoinDelay, &g_Config.m_TcExecuteOnJoinDelay, &Button, TCLocalize("Delay"), 140, 2000, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, "ms");
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Voting ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Voting"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoVoteWhenFar, TCLocalize("Auto vote no to map changes when far"), &g_Config.m_TcAutoVoteWhenFar, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcAutoVoteWhenFarTime, &g_Config.m_TcAutoVoteWhenFarTime, &Button, TCLocalize("Minimum Time"), 1, 20, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_NOCLAMPVALUE, " minutes");

	CUIRect VoteMessage;
	Column.HSplitTop(LineSize + MarginExtraSmall, &VoteMessage, &Column);
	VoteMessage.HSplitTop(MarginExtraSmall, nullptr, &VoteMessage);
	VoteMessage.VSplitMid(&Label, &VoteMessage);
	Ui()->DoLabel(&Label, TCLocalize("Message to send in chat:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_VoteMessage(g_Config.m_TcAutoVoteWhenFarMessage, sizeof(g_Config.m_TcAutoVoteWhenFarMessage));
	s_VoteMessage.SetEmptyText(TCLocalize("Leave empty to disable"));
	Ui()->DoEditBox(&s_VoteMessage, &VoteMessage, EditBoxFontSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Player Indicator ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Player Indicator"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPlayerIndicator, TCLocalize("Show any enabled Indicators"), &g_Config.m_TcPlayerIndicator, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorHideVisible, TCLocalize("Hide indicator for tees on your screen"), &g_Config.m_TcIndicatorHideVisible, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPlayerIndicatorFreeze, TCLocalize("Show only freeze Players"), &g_Config.m_TcPlayerIndicatorFreeze, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorTeamOnly, TCLocalize("Only show after joining a team"), &g_Config.m_TcIndicatorTeamOnly, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorTees, TCLocalize("Render tiny tees instead of circles"), &g_Config.m_TcIndicatorTees, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicator, TCLocalize("Use warlist groups for indicator"), &g_Config.m_TcWarListIndicator, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorRadius, &g_Config.m_TcIndicatorRadius, &Button, TCLocalize("Indicator size"), 1, 16);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOpacity, &g_Config.m_TcIndicatorOpacity, &Button, TCLocalize("Indicator opacity"), 0, 100);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcIndicatorVariableDistance, TCLocalize("Change indicator offset based on distance to other tees"), &g_Config.m_TcIndicatorVariableDistance, &Column, LineSize);
	if(g_Config.m_TcIndicatorVariableDistance)
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffset, &g_Config.m_TcIndicatorOffset, &Button, TCLocalize("Indicator min offset"), 16, 200);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffsetMax, &g_Config.m_TcIndicatorOffsetMax, &Button, TCLocalize("Indicator max offset"), 16, 200);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorMaxDistance, &g_Config.m_TcIndicatorMaxDistance, &Button, TCLocalize("Indicator max distance"), 500, 7000);
	}
	else
	{
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcIndicatorOffset, &g_Config.m_TcIndicatorOffset, &Button, TCLocalize("Indicator offset"), 16, 200);
		Column.HSplitTop(LineSize * 2, nullptr, &Column);
	}
	if(g_Config.m_TcWarListIndicator)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorColors, TCLocalize("Use warlist colors instead of regular colors"), &g_Config.m_TcWarListIndicatorColors, &Column, LineSize);
		char aBuf[128];
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorAll, TCLocalize("Show all warlist groups"), &g_Config.m_TcWarListIndicatorAll, &Column, LineSize);
		str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(1)->m_aWarName);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorEnemy, aBuf, &g_Config.m_TcWarListIndicatorEnemy, &Column, LineSize);
		str_format(aBuf, sizeof(aBuf), "Show %s group", GameClient()->m_WarList.m_WarTypes.at(2)->m_aWarName);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListIndicatorTeam, aBuf, &g_Config.m_TcWarListIndicatorTeam, &Column, LineSize);
	}
	if(!g_Config.m_TcWarListIndicatorColors || !g_Config.m_TcWarListIndicator)
	{
		static CButtonContainer s_IndicatorAliveColorId, s_IndicatorDeadColorId, s_IndicatorSavedColorId;
		DoLine_ColorPicker(&s_IndicatorAliveColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator alive color"), &g_Config.m_TcIndicatorAlive, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&s_IndicatorDeadColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator in freeze color"), &g_Config.m_TcIndicatorFreeze, ColorRGBA(0.0f, 0.0f, 0.0f), false);
		DoLine_ColorPicker(&s_IndicatorSavedColorId, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Indicator safe color"), &g_Config.m_TcIndicatorSaved, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Apariencia ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Appearance"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplatePingCircle, TCLocalize("Show ping colored circle in nameplates"), &g_Config.m_TcNameplatePingCircle, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateCountry, TCLocalize("Show country flags in nameplates"), &g_Config.m_TcNameplateCountry, &Column, LineSize);
	// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderNameplateSpec, TCLocalize("Hide nameplates in spec"), &g_Config.m_TcRenderNameplateSpec, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNameplateSkins, TCLocalize("Show skin names in nameplate"), &g_Config.m_TcNameplateSkins, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClFreezeStars, TCLocalize("Freeze stars"), &g_Config.m_ClFreezeStars, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcColorFreeze, TCLocalize("Colored frozen tee skins"), &g_Config.m_TcColorFreeze, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFrozenKatana, TCLocalize("Show katan on frozen players"), &g_Config.m_TcFrozenKatana, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderWeaponsAsGun, TCLocalize("Render weapons as the gun sprite"), &g_Config.m_TcRenderWeaponsAsGun, &Column, LineSize);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWhiteFeet, TCLocalize("Render all custom colored feet as white feet skin"), &g_Config.m_TcWhiteFeet, &Column, LineSize);
	CUIRect FeetBox;
	Column.HSplitTop(LineSize + MarginExtraSmall, &FeetBox, &Column);
	if(g_Config.m_TcWhiteFeet)
	{
		FeetBox.HSplitTop(MarginExtraSmall, nullptr, &FeetBox);
		FeetBox.VSplitMid(&FeetBox, nullptr);
		static CLineInput s_WhiteFeet(g_Config.m_TcWhiteFeetSkin, sizeof(g_Config.m_TcWhiteFeetSkin));
		s_WhiteFeet.SetEmptyText("x_ninja");
		Ui()->DoEditBox(&s_WhiteFeet, &FeetBox, EditBoxFontSize);
	}

	{
		static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
		int Value = g_Config.m_TcTinyTees ? (g_Config.m_TcTinyTeesOthers ? 2 : 1) : 0;
		if(DoLine_RadioMenu(Column, TCLocalize("Tiny Tees"),
			   s_vButtonContainers,
			   {Localize("None"), Localize("Own"), Localize("All")},
			   {0, 1, 2},
			   Value))
		{
			g_Config.m_TcTinyTees = Value > 0 ? 1 : 0;
			g_Config.m_TcTinyTeesOthers = Value > 1 ? 1 : 0;
		}
		Column.HSplitTop(LineSize, &TinyTeeConfig, &Column);
		if(g_Config.m_TcTinyTees > 0)
			Ui()->DoScrollbarOption(&g_Config.m_TcTinyTeeSize, &g_Config.m_TcTinyTeeSize, &TinyTeeConfig, TCLocalize("Tiny Tee Size"), 85, 115);
	}

	{
		static std::vector<CButtonContainer> s_vButtonContainers = {{}, {}, {}};
		int Value = g_Config.m_TcFakeCtfFlags;
		if(DoLine_RadioMenu(Column, TCLocalize("Fake CTF flags"),
			   s_vButtonContainers,
			   {Localize("None"), Localize("Red"), Localize("Blue")},
			   {0, 1, 2},
			   Value))
		{
			g_Config.m_TcFakeCtfFlags = Value;
		}
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMovingTilesEntities, TCLocalize("Show moving tiles in entities"), &g_Config.m_TcMovingTilesEntities, &Column, LineSize);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Pet ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Pet"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcPetShow, TCLocalize("Show the pet"), &g_Config.m_TcPetShow, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcPetSize, &g_Config.m_TcPetSize, &Button, TCLocalize("Pet size"), 10, 500, &CUi::ms_LinearScrollbarScale, 0, "%");
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcPetAlpha, &g_Config.m_TcPetAlpha, &Button, TCLocalize("Pet alpha"), 10, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
	Button.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Pet Skin:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_PetSkin(g_Config.m_TcPetSkin, sizeof(g_Config.m_TcPetSkin));
	Ui()->DoEditBox(&s_PetSkin, &Button, EditBoxFontSize);

	// Pet Preview
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	CUIRect Preview;
	Column.HSplitTop(64.0f, &Preview, &Column);

	CTeeRenderInfo TeeInfo;
	const CSkin *pSkin = GameClient()->m_Skins.Find(g_Config.m_TcPetSkin);
	if(!pSkin || str_comp(pSkin->GetName(), g_Config.m_TcPetSkin) != 0)
		pSkin = GameClient()->m_Skins.Find("default");

	TeeInfo.m_OriginalRenderSkin = pSkin->m_OriginalSkin;
	TeeInfo.m_ColorableRenderSkin = pSkin->m_ColorableSkin;
	TeeInfo.m_SkinMetrics = pSkin->m_Metrics;
	TeeInfo.m_CustomColoredSkin = false;
	TeeInfo.m_ColorBody = ColorRGBA(1.0f, 1.0f, 1.0f);
	TeeInfo.m_ColorFeet = ColorRGBA(1.0f, 1.0f, 1.0f);
	TeeInfo.m_Size = 64.0f;

	const CAnimState *pIdleState = CAnimState::GetIdle();
	vec2 OffsetToMid;
	CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeInfo, OffsetToMid);
	vec2 TeeRenderPos = Preview.Center();
	TeeRenderPos.y += OffsetToMid.y;

	vec2 Dir = Ui()->MousePos() - TeeRenderPos;
	const float Length = length(Dir);
	if(Length > 0.0f)
		Dir /= Length;
	if(Length < 0.4f * 64.0f)
	{
		Dir = vec2(1.0f, 0.0f);
	}

	int PetEmote = g_Config.m_ClPlayerDefaultEyes;
	RenderTools()->RenderTee(pIdleState, &TeeInfo, PetEmote, Dir, TeeRenderPos);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Ghost Tools ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Ghost Tools"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowOthersGhosts, TCLocalize("Show unpredicted ghosts for other players"), &g_Config.m_TcShowOthersGhosts, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSwapGhosts, TCLocalize("Swap ghosts and normal players"), &g_Config.m_TcSwapGhosts, &Column, LineSize);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcPredGhostsAlpha, &g_Config.m_TcPredGhostsAlpha, &Button, TCLocalize("Predicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcUnpredGhostsAlpha, &g_Config.m_TcUnpredGhostsAlpha, &Button, TCLocalize("Unpredicted alpha"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcHideFrozenGhosts, TCLocalize("Hide ghosts of frozen players"), &g_Config.m_TcHideFrozenGhosts, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderGhostAsCircle, TCLocalize("Render ghosts as circles"), &g_Config.m_TcRenderGhostAsCircle, &Column, LineSize);

	static CButtonContainer s_ReaderButtonGhost, s_ClearButtonGhost;
	DoLine_KeyReader(Column, s_ReaderButtonGhost, s_ClearButtonGhost, TCLocalize("Toggle ghosts key"), "toggle tc_show_others_ghosts 0 1");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** RightView ***** //
	LeftView = Column;
	Column = RightView;

	// ***** TClient Theme ***** //
	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("TClient Theme"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	{
		static std::vector<const char *> s_ThemeDropDownNames;
		s_ThemeDropDownNames = {TCLocalize("Dark"), TCLocalize("Pink Anime"), TCLocalize("Cyber"), TCLocalize("Minimal")};
		static CUi::SDropDownState s_ThemeDropDownState;
		static CScrollRegion s_ThemeDropDownScrollRegion;
		s_ThemeDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_ThemeDropDownScrollRegion;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Theme"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcTheme = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcTheme, s_ThemeDropDownNames.data(), s_ThemeDropDownNames.size(), s_ThemeDropDownState);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcThemeCustomColors, TCLocalize("Custom menu colors"), &g_Config.m_TcThemeCustomColors, &Column, LineSize);
	if(g_Config.m_TcThemeCustomColors)
	{
		static CButtonContainer s_ThemeAccentColor, s_ThemePanelColor;
		DoLine_ColorPicker(&s_ThemeAccentColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Accent color"), &g_Config.m_TcThemeAccentColor, ColorRGBA(1.0f, 0.34f, 0.64f, 0.82f), false, nullptr, true);
		DoLine_ColorPicker(&s_ThemePanelColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Panel color"), &g_Config.m_TcThemePanelColor, ColorRGBA(0.0f, 0.0f, 0.0f, 0.28f), false, nullptr, true);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcThemeCustomBackground, TCLocalize("Custom settings background"), &g_Config.m_TcThemeCustomBackground, &Column, LineSize);
	if(g_Config.m_TcThemeCustomBackground)
	{
		static CButtonContainer s_ThemeBackgroundColor;
		DoLine_ColorPicker(&s_ThemeBackgroundColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Background color"), &g_Config.m_TcThemeBackgroundColor, ColorRGBA(0.35f, 0.03f, 0.19f, 0.16f), false, nullptr, true);
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Frozen Tee Display ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Frozen Tee Display"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowFrozenHud, TCLocalize("Show frozen tee display"), &g_Config.m_TcShowFrozenHud, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowFrozenHudSkins, TCLocalize("Use skins instead of ninja tees"), &g_Config.m_TcShowFrozenHudSkins, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFrozenHudTeamOnly, TCLocalize("Only show after joining a team"), &g_Config.m_TcFrozenHudTeamOnly, &Column, LineSize);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcFrozenMaxRows, &g_Config.m_TcFrozenMaxRows, &Button, TCLocalize("Max Rows"), 1, 6);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcFrozenHudTeeSize, &g_Config.m_TcFrozenHudTeeSize, &Button, TCLocalize("Tee Size"), 8, 27);

	{
		CUIRect CheckBoxRect, CheckBoxRect2;
		Column.HSplitTop(LineSize, &CheckBoxRect, &Column);
		Column.HSplitTop(LineSize, &CheckBoxRect2, &Column);
		if(DoButton_CheckBox(&g_Config.m_TcShowFrozenText, TCLocalize("Tees left alive text"), g_Config.m_TcShowFrozenText >= 1, &CheckBoxRect))
			g_Config.m_TcShowFrozenText = g_Config.m_TcShowFrozenText >= 1 ? 0 : 1;

		if(g_Config.m_TcShowFrozenText)
		{
			static int s_CountFrozenText = 0;
			if(DoButton_CheckBox(&s_CountFrozenText, TCLocalize("Count frozen tees"), g_Config.m_TcShowFrozenText == 2, &CheckBoxRect2))
				g_Config.m_TcShowFrozenText = g_Config.m_TcShowFrozenText != 2 ? 2 : 1;
		}
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Rainbow ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Rainbow"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowTees, TCLocalize("Rainbow Tees"), &g_Config.m_TcRainbowTees, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowWeapon, TCLocalize("Rainbow weapons"), &g_Config.m_TcRainbowWeapon, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowHook, TCLocalize("Rainbow hook"), &g_Config.m_TcRainbowHook, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRainbowOthers, TCLocalize("Rainbow others"), &g_Config.m_TcRainbowOthers, &Column, LineSize);

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	static std::vector<const char *> s_RainbowDropDownNames;
	s_RainbowDropDownNames = {TCLocalize("Rainbow"), TCLocalize("Pulse"), TCLocalize("Black"), TCLocalize("Random")};
	static CUi::SDropDownState s_RainbowDropDownState;
	static CScrollRegion s_RainbowDropDownScrollRegion;
	s_RainbowDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_RainbowDropDownScrollRegion;
	int RainbowSelectedOld = g_Config.m_TcRainbowMode - 1;
	CUIRect RainbowDropDownRect;
	Column.HSplitTop(LineSize, &RainbowDropDownRect, &Column);
	const int RainbowSelectedNew = Ui()->DoDropDown(&RainbowDropDownRect, RainbowSelectedOld, s_RainbowDropDownNames.data(), s_RainbowDropDownNames.size(), s_RainbowDropDownState);
	if(RainbowSelectedOld != RainbowSelectedNew)
	{
		g_Config.m_TcRainbowMode = RainbowSelectedNew + 1;
	}
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcRainbowSpeed, &g_Config.m_TcRainbowSpeed, &Button, TCLocalize("Rainbow speed"), 0, 5000, &CUi::ms_LogarithmicScrollbarScale, 0, "%");
	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	// ***** BG Draw ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Background Draw"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	static CButtonContainer s_BgDrawColor;
	DoLine_ColorPicker(&s_BgDrawColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column, TCLocalize("Color"), &g_Config.m_TcBgDrawColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

	Column.HSplitTop(LineSize * 2.0f, &Button, &Column);
	if(g_Config.m_TcBgDrawFadeTime == 0)
		Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, TCLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, TCLocalize(" seconds (never)"));
	else
		Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawFadeTime, &g_Config.m_TcBgDrawFadeTime, &Button, TCLocalize("Time until strokes disappear"), 0, 600, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE, TCLocalize(" seconds"));

	Column.HSplitTop(LineSize * 2.0f, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcBgDrawWidth, &g_Config.m_TcBgDrawWidth, &Button, TCLocalize("Width"), 1, 50, &CUi::ms_LinearScrollbarScale, CUi::SCROLLBAR_OPTION_MULTILINE);

	static CButtonContainer s_ReaderButtonDraw, s_ClearButtonDraw;
	DoLine_KeyReader(Column, s_ReaderButtonDraw, s_ClearButtonDraw, TCLocalize("Draw where mouse is"), "+bg_draw");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	// ***** Finish Name ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Finish Name"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcChangeNameNearFinish, TCLocalize("Attempt to change your name when near finish"), &g_Config.m_TcChangeNameNearFinish, &Column, LineSize);
	Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
	Button.VSplitMid(&Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Finish Name:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_FinishName(g_Config.m_TcFinishName, sizeof(g_Config.m_TcFinishName));
	Ui()->DoEditBox(&s_FinishName, &Button, EditBoxFontSize);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** HUD ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("HUD"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMiniVoteHud, TCLocalize("Show mini vote HUD"), &g_Config.m_TcMiniVoteHud, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcMiniDebug, TCLocalize("Show position and angle (mini debug)"), &g_Config.m_TcMiniDebug, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcRenderCursorSpec, TCLocalize("Show your cursor when in free spectate"), &g_Config.m_TcRenderCursorSpec, &Column, LineSize);

	Column.HSplitTop(LineSize, &Button, &Column);
	if(g_Config.m_TcRenderCursorSpec)
	{
		Ui()->DoScrollbarOption(&g_Config.m_TcRenderCursorSpecAlpha, &g_Config.m_TcRenderCursorSpecAlpha, &Button, TCLocalize("Spectate cursor alpha"), 0, 100);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcNotifyWhenLast, TCLocalize("Show when you are the last alive"), &g_Config.m_TcNotifyWhenLast, &Column, LineSize);
	CUIRect NotificationConfig;
	Column.HSplitTop(LineSize + MarginSmall, &NotificationConfig, &Column);
	if(g_Config.m_TcNotifyWhenLast)
	{
		NotificationConfig.VSplitMid(&Button, &NotificationConfig);
		static CLineInput s_LastInput(g_Config.m_TcNotifyWhenLastText, sizeof(g_Config.m_TcNotifyWhenLastText));
		s_LastInput.SetEmptyText(TCLocalize("Last!"));
		Button.HSplitTop(MarginSmall, nullptr, &Button);
		Ui()->DoEditBox(&s_LastInput, &Button, EditBoxFontSize);
		static CButtonContainer s_ClientNotifyWhenLastColor;
		DoLine_ColorPicker(&s_ClientNotifyWhenLastColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &NotificationConfig, "", &g_Config.m_TcNotifyWhenLastColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastX, &g_Config.m_TcNotifyWhenLastX, &Button, TCLocalize("Horizontal Position"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastY, &g_Config.m_TcNotifyWhenLastY, &Button, TCLocalize("Vertical Position"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcNotifyWhenLastSize, &g_Config.m_TcNotifyWhenLastSize, &Button, TCLocalize("Font Size"), 1, 50);
	}
	else
	{
		Column.HSplitTop(LineSize * 3.0f, nullptr, &Column);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcShowCenter, TCLocalize("Show screen center lines"), &g_Config.m_TcShowCenter, &Column, LineSize);
	Column.HSplitTop(LineSize + MarginSmall, &Button, &Column);
	if(g_Config.m_TcShowCenter)
	{
		static CButtonContainer s_ShowCenterLineColor;
		DoLine_ColorPicker(&s_ShowCenterLineColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Button, TCLocalize("Screen center line color"), &g_Config.m_TcShowCenterColor, DefaultConfig::TcShowCenterColor, false, nullptr, true);
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcShowCenterWidth, &g_Config.m_TcShowCenterWidth, &Button, TCLocalize("Screen center line width"), 0, 20);
	}
	else
	{
		Column.HSplitTop(LineSize, nullptr, &Column);
	}

	Column.HSplitTop(MarginExtraSmall, nullptr, &Column);
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Tile Outlines ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Tile Outlines"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutline, TCLocalize("Show any enabled outlines"), &g_Config.m_TcOutline, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcOutlineEntities, TCLocalize("Only show outlines in entities"), &g_Config.m_TcOutlineEntities, &Column, LineSize);

	auto DoOutlineType = [&](CButtonContainer &ButtonContainer, const char *pName, int &Enable, int &Width, unsigned int &Color, const unsigned int &ColorDefault) {
		// Checkbox & Color
		DoLine_ColorPicker(&ButtonContainer, ColorPickerLineSize, ColorPickerLabelSize, 0, &Column, pName, &Color, ColorDefault, true, &Enable, true);
		// Width
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&Width, &Width, &Button, TCLocalize("Width", "Outlines"), 1, 16);
		//
		Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
	};
	Column.HSplitTop(ColorPickerLineSpacing, nullptr, &Column);
	static CButtonContainer s_aOutlineButtonContainers[5];
	DoOutlineType(s_aOutlineButtonContainers[0], TCLocalize("Unhook & hook"), g_Config.m_TcOutlineSolid, g_Config.m_TcOutlineWidthSolid, g_Config.m_TcOutlineColorSolid, DefaultConfig::TcOutlineColorSolid);
	DoOutlineType(s_aOutlineButtonContainers[1], TCLocalize("Freeze & deep"), g_Config.m_TcOutlineFreeze, g_Config.m_TcOutlineWidthFreeze, g_Config.m_TcOutlineColorFreeze, DefaultConfig::TcOutlineColorFreeze);
	DoOutlineType(s_aOutlineButtonContainers[2], TCLocalize("Unfreeze & undeep"), g_Config.m_TcOutlineUnfreeze, g_Config.m_TcOutlineWidthUnfreeze, g_Config.m_TcOutlineColorUnfreeze, DefaultConfig::TcOutlineColorUnfreeze);
	DoOutlineType(s_aOutlineButtonContainers[3], TCLocalize("Kill"), g_Config.m_TcOutlineKill, g_Config.m_TcOutlineWidthKill, g_Config.m_TcOutlineColorKill, DefaultConfig::TcOutlineColorKill);
	DoOutlineType(s_aOutlineButtonContainers[4], TCLocalize("Tele"), g_Config.m_TcOutlineTele, g_Config.m_TcOutlineWidthTele, g_Config.m_TcOutlineColorTele, DefaultConfig::TcOutlineColorTele);
	Column.h -= ColorPickerLineSpacing;

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Friend Notifications ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Friend Notifications"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFriendHud, TCLocalize("Show when a friend joins"), &g_Config.m_TcFriendHud, &Column, LineSize);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcFriendHudDuration, &g_Config.m_TcFriendHudDuration, &Button, TCLocalize("Duration (seconds)"), 1, 15);

	Column.HSplitTop(LineSize, &Button, &Column);
	static std::vector<const char *> s_FriendHudCornerNames = {
		TCLocalize("Top Left"),
		TCLocalize("Top Right"),
		TCLocalize("Bottom Left"),
		TCLocalize("Bottom Right")};
	static CUi::SDropDownState s_FriendHudCornerState;
	static CScrollRegion s_FriendHudCornerScroll;
	s_FriendHudCornerState.m_SelectionPopupContext.m_pScrollRegion = &s_FriendHudCornerScroll;
	CUIRect DropDownRect;
	Button.VSplitLeft(120.0f, &Label, &DropDownRect);
	Ui()->DoLabel(&Label, TCLocalize("Position"), FontSize, TEXTALIGN_ML);
	g_Config.m_TcFriendHudCorner = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcFriendHudCorner, s_FriendHudCornerNames.data(), s_FriendHudCornerNames.size(), s_FriendHudCornerState);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFriendHudShowClan, TCLocalize("Show clan in notification"), &g_Config.m_TcFriendHudShowClan, &Column, LineSize);

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** Chat History ***** //
	Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Chat History"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcChatHistory, TCLocalize("Enable chat history"), &g_Config.m_TcChatHistory, &Column, LineSize);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcChatHistoryLines, &g_Config.m_TcChatHistoryLines, &Button, TCLocalize("Max history lines"), 64, 500);

	Column.HSplitTop(LineSize, &Button, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_TcChatHistoryHeight, &g_Config.m_TcChatHistoryHeight, &Button, TCLocalize("Panel height"), 30, 90, &CUi::ms_LinearScrollbarScale, 0, "%");

	static CButtonContainer s_ReaderButtonHistory, s_ClearButtonHistory;
	DoLine_KeyReader(Column, s_ReaderButtonHistory, s_ClearButtonHistory, TCLocalize("Open chat history"), "+show_chat_history");

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	// ***** END OF PAGE 1 SETTINGS ***** //
	RightView = Column;

	// Scroll
	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientLluvia(CUIRect MainView)
{
	CUIRect Column, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);


	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	Column = MainView;

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Lluvia"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWeatherParticles, TCLocalize("Enable weather particles"), &g_Config.m_TcWeatherParticles, &Column, LineSize);
	if(g_Config.m_TcWeatherParticles)
	{
		static std::vector<const char *> s_WeatherDropDownNames;
		s_WeatherDropDownNames = {TCLocalize("Snow"), TCLocalize("Rain"), TCLocalize("Stars"), TCLocalize("Particles")};
		static CUi::SDropDownState s_WeatherDropDownState;
		static CScrollRegion s_WeatherDropDownScrollRegion;
		s_WeatherDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_WeatherDropDownScrollRegion;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Particle type"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcWeatherMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcWeatherMode, s_WeatherDropDownNames.data(), s_WeatherDropDownNames.size(), s_WeatherDropDownState);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherAmount, &g_Config.m_TcWeatherAmount, &Button, TCLocalize("Amount"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherSpeed, &g_Config.m_TcWeatherSpeed, &Button, TCLocalize("Speed"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherSize, &g_Config.m_TcWeatherSize, &Button, TCLocalize("Size"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherAlpha, &g_Config.m_TcWeatherAlpha, &Button, TCLocalize("Opacity"), 5, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientAnimeLove(CUIRect MainView)
{
	CUIRect Column, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);


	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	Column = MainView;

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anime Love"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAnimeLove, TCLocalize("Enable Anime Love"), &g_Config.m_TcAnimeLove, &Column, LineSize);
	if(g_Config.m_TcAnimeLove)
	{
		CUIRect DropDownRect;

		static std::vector<const char *> s_AnimeSkinNames;
		s_AnimeSkinNames = {TCLocalize("Kurumi"), TCLocalize("Crimson"), TCLocalize("Midnight"), TCLocalize("Pastel")};
		static CUi::SDropDownState s_AnimeSkinDropDownState;
		static CScrollRegion s_AnimeSkinScrollRegion;
		s_AnimeSkinDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeSkinScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Character skin"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveCharacter = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveCharacter, s_AnimeSkinNames.data(), s_AnimeSkinNames.size(), s_AnimeSkinDropDownState);

		static std::vector<const char *> s_AnimeAnimationNames;
		s_AnimeAnimationNames = {TCLocalize("Wave"), TCLocalize("Walk"), TCLocalize("Mixed"), TCLocalize("Sit"), TCLocalize("Sleep"), TCLocalize("Celebrate"), TCLocalize("Follow tee")};
		static CUi::SDropDownState s_AnimeAnimationDropDownState;
		static CScrollRegion s_AnimeAnimationScrollRegion;
		s_AnimeAnimationDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeAnimationScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Animation type"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveAnimation = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveAnimation, s_AnimeAnimationNames.data(), s_AnimeAnimationNames.size(), s_AnimeAnimationDropDownState);

		static std::vector<const char *> s_AnimeVisibilityNames;
		s_AnimeVisibilityNames = {TCLocalize("Menu and ingame"), TCLocalize("Menu only"), TCLocalize("Ingame only")};
		static CUi::SDropDownState s_AnimeVisibilityDropDownState;
		static CScrollRegion s_AnimeVisibilityScrollRegion;
		s_AnimeVisibilityDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeVisibilityScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Visibility"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveVisibility = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveVisibility, s_AnimeVisibilityNames.data(), s_AnimeVisibilityNames.size(), s_AnimeVisibilityDropDownState);

		static std::vector<const char *> s_AnimePositionNames;
		s_AnimePositionNames = {TCLocalize("Right"), TCLocalize("Left"), TCLocalize("Above"), TCLocalize("Below")};
		static CUi::SDropDownState s_AnimePositionDropDownState;
		static CScrollRegion s_AnimePositionScrollRegion;
		s_AnimePositionDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimePositionScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Position"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLovePosition = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLovePosition, s_AnimePositionNames.data(), s_AnimePositionNames.size(), s_AnimePositionDropDownState);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAnimeLoveSpeech, TCLocalize("Floating phrases"), &g_Config.m_TcAnimeLoveSpeech, &Column, LineSize);
		if(g_Config.m_TcAnimeLoveSpeech)
		{
			Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
			Button.VSplitLeft(120.0f, &Label, &Button);
			Ui()->DoLabel(&Label, TCLocalize("Greeting phrase"), FontSize, TEXTALIGN_ML);
			static CLineInput s_AnimeLovePhrase(g_Config.m_TcAnimeLovePhrase, sizeof(g_Config.m_TcAnimeLovePhrase));
			s_AnimeLovePhrase.SetEmptyText(TCLocalize("Hi!"));
			Ui()->DoEditBox(&s_AnimeLovePhrase, &Button, EditBoxFontSize);
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveSize, &g_Config.m_TcAnimeLoveSize, &Button, TCLocalize("Anime Love Size"), 40, 260, &CUi::ms_LinearScrollbarScale, 0, "px");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveSpeed, &g_Config.m_TcAnimeLoveSpeed, &Button, TCLocalize("Anime Love Speed"), 25, 250, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveWalkDistance, &g_Config.m_TcAnimeLoveWalkDistance, &Button, TCLocalize("Walk Distance"), 0, 180, &CUi::ms_LinearScrollbarScale, 0, "px");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveAlpha, &g_Config.m_TcAnimeLoveAlpha, &Button, TCLocalize("Anime Love Opacity"), 10, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CButtonContainer s_ReaderButtonAnimeLove, s_ClearButtonAnimeLove;
		DoLine_KeyReader(Column, s_ReaderButtonAnimeLove, s_ClearButtonAnimeLove, TCLocalize("Bind Anime Love"), "toggle tc_anime_love 0 1");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientBindWheel(CUIRect MainView)
{
	CUIRect LeftView, RightView, Label, Button;
	MainView.VSplitLeft(MainView.w / 2.1f, &LeftView, &RightView);

	CUIRect LeftFrame = LeftView;
	LeftFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect LeftInner = LeftView;
	LeftInner.Margin(2.0f, &LeftInner);
	LeftInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	LeftView.Margin(8.0f, &LeftView);

	CUIRect RightFrame = RightView;
	RightFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect RightInner = RightView;
	RightInner.Margin(2.0f, &RightInner);
	RightInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	RightView.Margin(8.0f, &RightView);

	const float Radius = minimum(RightView.w, RightView.h) / 2.0f;
	vec2 Center = RightView.Center();
	// Draw Circle
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Center.x, Center.y, Radius, 64);
	Graphics()->QuadsEnd();

	static char s_aBindName[BINDWHEEL_MAX_NAME];
	static char s_aBindCommand[BINDWHEEL_MAX_CMD];

	static int s_SelectedBindIndex = -1;
	int HoveringIndex = -1;

	float MouseDist = distance(Center, Ui()->MousePos());
	const int SegmentCount = GameClient()->m_BindWheel.m_vBinds.size();
	if(MouseDist < Radius && MouseDist > Radius * 0.25f && SegmentCount > 0)
	{
		float SegmentAngle = 2.0f * pi / SegmentCount;

		float HoveringAngle = angle(Ui()->MousePos() - Center) + SegmentAngle / 2.0f;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;

		HoveringIndex = (int)(HoveringAngle / (2.0f * pi) * SegmentCount);
		HoveringIndex = std::clamp(HoveringIndex, 0, SegmentCount - 1);
		if(Ui()->MouseButtonClicked(0))
		{
			s_SelectedBindIndex = HoveringIndex;
			str_copy(s_aBindName, GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aName);
			str_copy(s_aBindCommand, GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(1) && s_SelectedBindIndex >= 0 && HoveringIndex >= 0 && HoveringIndex != s_SelectedBindIndex)
		{
			CBindWheel::CBind BindA = GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex];
			CBindWheel::CBind BindB = GameClient()->m_BindWheel.m_vBinds[HoveringIndex];
			str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aName, BindB.m_aName);
			str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aCommand, BindB.m_aCommand);
			str_copy(GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aName, BindA.m_aName);
			str_copy(GameClient()->m_BindWheel.m_vBinds[HoveringIndex].m_aCommand, BindA.m_aCommand);
		}
		else if(Ui()->MouseButtonClicked(2))
		{
			s_SelectedBindIndex = HoveringIndex;
		}
	}
	else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
	{
		s_SelectedBindIndex = -1;
		str_copy(s_aBindName, "");
		str_copy(s_aBindCommand, "");
	}

	const float Theta = pi * 2.0f / std::max<float>(1.0f, GameClient()->m_BindWheel.m_vBinds.size());
	for(int i = 0; i < static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()); i++)
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

		float SegmentFontSize = FontSize * 1.1f;
		if(i == s_SelectedBindIndex)
		{
			SegmentFontSize = FontSize * 1.7f;
			TextRender()->TextColor(ColorRGBA(0.5f, 1.0f, 0.75f, 1.0f));
		}
		else if(i == HoveringIndex)
		{
			SegmentFontSize = FontSize * 1.35f;
		}

		const CBindWheel::CBind Bind = GameClient()->m_BindWheel.m_vBinds[i];
		const float Angle = Theta * i;

		const vec2 Pos = direction(Angle) * (Radius * 0.75f) + Center;
		const CUIRect Rect = CUIRect{Pos.x - 50.0f, Pos.y - 50.0f, 100.0f, 100.0f};
		Ui()->DoLabel(&Rect, Bind.m_aName, SegmentFontSize, TEXTALIGN_MC);
	}

	TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));

	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Name:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aBindName, sizeof(s_aBindName));
	s_NameInput.SetEmptyText(TCLocalize("Name"));
	Ui()->DoEditBox(&s_NameInput, &Button, EditBoxFontSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Button.VSplitLeft(100.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Command:"), FontSize, TEXTALIGN_ML);
	static CLineInput s_BindInput;
	s_BindInput.SetBuffer(s_aBindCommand, sizeof(s_aBindCommand));
	s_BindInput.SetEmptyText(TCLocalize("Command"));
	Ui()->DoEditBox(&s_BindInput, &Button, EditBoxFontSize);

	static CButtonContainer s_AddButton, s_RemoveButton, s_OverrideButton;

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	if(DoButton_Menu(&s_OverrideButton, TCLocalize("Override Selected"), 0, &Button) && s_SelectedBindIndex >= 0 && s_SelectedBindIndex < static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aName, TempBind.m_aName);
		str_copy(GameClient()->m_BindWheel.m_vBinds[s_SelectedBindIndex].m_aCommand, s_aBindCommand);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	CUIRect ButtonAdd, ButtonRemove;
	Button.VSplitMid(&ButtonRemove, &ButtonAdd, MarginSmall);
	if(DoButton_Menu(&s_AddButton, TCLocalize("Add Bind"), 0, &ButtonAdd))
	{
		CBindWheel::CBind TempBind;
		if(str_length(s_aBindName) == 0)
			str_copy(TempBind.m_aName, "*");
		else
			str_copy(TempBind.m_aName, s_aBindName);

		GameClient()->m_BindWheel.AddBind(TempBind.m_aName, s_aBindCommand);
		s_SelectedBindIndex = static_cast<int>(GameClient()->m_BindWheel.m_vBinds.size()) - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, TCLocalize("Remove Bind"), 0, &ButtonRemove) && s_SelectedBindIndex >= 0)
	{
		GameClient()->m_BindWheel.RemoveBind(s_SelectedBindIndex);
		s_SelectedBindIndex = -1;
	}

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("The command is ran in console not chat"), FontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use left mouse to select"), FontSize * 0.8f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use right mouse to swap with selected"), FontSize * 0.8f, TEXTALIGN_ML);
	LeftView.HSplitTop(LineSize * 0.8f, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Use middle mouse select without copy"), FontSize * 0.8f, TEXTALIGN_ML);

	LeftView.HSplitBottom(LineSize, &LeftView, &Label);
	static CButtonContainer s_ReaderButtonWheel, s_ClearButtonWheel;
	DoLine_KeyReader(Label, s_ReaderButtonWheel, s_ClearButtonWheel, TCLocalize("Bind Wheel Key"), "+bindwheel");

	LeftView.HSplitBottom(LineSize, &LeftView, &Label);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcResetBindWheelMouse, TCLocalize("Reset position of mouse when opening bindwheel"), &g_Config.m_TcResetBindWheelMouse, &Label, LineSize);
}

void CMenus::RenderSettingsTClientChatBinds(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;

	MainView.HSplitTop(Margin, nullptr, &MainView);
	MainView.VSplitRight(5.0f, &MainView, nullptr); // Padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView); // Padding for scrollbar

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 10.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 8.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ***** All the stuff ***** //

	auto DoBindchatDefault = [&](CUIRect &Column, CBindChat::CBindDefault &BindDefault) {
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		Column.HSplitTop(LineSize, &Button, &Column);
		CBindChat::CBind *pOldBind = GameClient()->m_BindChat.GetBind(BindDefault.m_Bind.m_aCommand);
		static char s_aTempName[BINDCHAT_MAX_NAME] = "";
		char *pName;
		if(pOldBind == nullptr)
			pName = s_aTempName;
		else
			pName = pOldBind->m_aName;
		if(DoEditBoxWithLabel(&BindDefault.m_LineInput, &Button, TCLocalize(BindDefault.m_pTitle), BindDefault.m_Bind.m_aName, pName, BINDCHAT_MAX_NAME) && BindDefault.m_LineInput.IsActive())
		{
			if(!pOldBind && pName[0] != '\0')
			{
				auto BindNew = BindDefault.m_Bind;
				str_copy(BindNew.m_aName, pName);
				GameClient()->m_BindChat.RemoveBind(pName); // Prevent duplicates
				GameClient()->m_BindChat.AddBind(BindNew);
				s_aTempName[0] = '\0';
			}
			if(pOldBind && pName[0] == '\0')
			{
				GameClient()->m_BindChat.RemoveBind(pName);
			}
		}
	};

	auto DoBindchatDefaults = [&](CUIRect &Column, const char *pTitle, std::vector<CBindChat::CBindDefault> &vBindchatDefaults) {
		s_SectionBoxes.push_back(Column);
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, pTitle, HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		for(CBindChat::CBindDefault &BindchatDefault : vBindchatDefaults)
			DoBindchatDefault(Column, BindchatDefault);
		s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
		Column.HSplitTop(MarginBetweenSections, nullptr, &Column);
	};

	float SizeL = 0.0f, SizeR = 0.0f;
	for(auto &[pTitle, vBindDefaults] : CBindChat::BIND_DEFAULTS)
	{
		float &Size = SizeL > SizeR ? SizeR : SizeL;
		CUIRect &Column = SizeL > SizeR ? RightView : LeftView;
		DoBindchatDefaults(Column, TCLocalize(pTitle), vBindDefaults);
		Size += vBindDefaults.size() * (MarginSmall + LineSize) + HeadlineHeight + HeadlineFontSize + MarginSmall * 2.0f;
	}

	// Scroll
	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientWarList(CUIRect MainView)
{
	CUIRect RightView, LeftView, Column1, Column2, Column3, Column4, Button, ButtonL, ButtonR, Label;

	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, Margin);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	// WAR LIST will have 4 columns
	//  [War entries] - [Entry Editing] - [Group Types] - [Recent Players]
	//									 [Group Editing]

	// putting this here so it can be updated by the entry list
	static char s_aEntryName[MAX_NAME_LENGTH];
	static char s_aEntryClan[MAX_CLAN_LENGTH];
	static char s_aEntryReason[MAX_WARLIST_REASON_LENGTH];
	static bool s_IsClan = false;
	static bool s_IsName = true;

	LeftView.VSplitMid(&Column1, &Column2, Margin);
	RightView.VSplitMid(&Column3, &Column4, Margin);

	CUIRect Col1Frame = Column1;
	Col1Frame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect Col1Inner = Column1;
	Col1Inner.Margin(2.0f, &Col1Inner);
	Col1Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	Column1.Margin(8.0f, &Column1);

	CUIRect Col2Frame = Column2;
	Col2Frame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect Col2Inner = Column2;
	Col2Inner.Margin(2.0f, &Col2Inner);
	Col2Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	Column2.Margin(8.0f, &Column2);

	CUIRect Col3Frame = Column3;
	Col3Frame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect Col3Inner = Column3;
	Col3Inner.Margin(2.0f, &Col3Inner);
	Col3Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	Column3.Margin(8.0f, &Column3);

	CUIRect Col4Frame = Column4;
	Col4Frame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect Col4Inner = Column4;
	Col4Inner.Margin(2.0f, &Col4Inner);
	Col4Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	Column4.Margin(8.0f, &Column4);

	// ======WAR ENTRIES======
	static CWarEntry *s_pSelectedEntry = nullptr;
	static CWarType *s_pSelectedType = GameClient()->m_WarList.m_WarTypes[0];
	{
		Column1.HSplitTop(HeadlineHeight, &Label, &Column1);
		Label.VSplitRight(25.0f, &Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("War Entries"), HeadlineFontSize, TEXTALIGN_ML);
		Column1.HSplitTop(MarginSmall, nullptr, &Column1);

		static CButtonContainer s_ReverseEntries;
		static bool s_Reversed = true;
		if(Ui()->DoButton_FontIcon(&s_ReverseEntries, s_Reversed ? FontIcon::CHEVRON_UP : FontIcon::CHEVRON_DOWN, 0, &Button, IGraphics::CORNER_ALL))
		{
			s_Reversed = !s_Reversed;
		}

		CUIRect EntriesSearch;
		Column1.HSplitBottom(25.0f, &Column1, &EntriesSearch);
		EntriesSearch.HSplitTop(MarginSmall, nullptr, &EntriesSearch);

		// Filter the list
		static CLineInputBuffered<128> s_EntriesFilterInput;
		std::vector<CWarEntry *> vpFilteredEntries;
		for(CWarEntry &Entry : GameClient()->m_WarList.m_vWarEntries)
		{
			if(str_find_nocase(Entry.m_aName, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
			else if(str_find_nocase(Entry.m_aClan, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
			else if(str_find_nocase(Entry.m_pWarType->m_aWarName, s_EntriesFilterInput.GetString()))
				vpFilteredEntries.push_back(&Entry);
		}
		if(s_Reversed)
			std::reverse(vpFilteredEntries.begin(), vpFilteredEntries.end());

		int SelectedOldEntry = -1;
		static CListBox s_EntriesListBox;
		s_EntriesListBox.DoStart(35.0f, vpFilteredEntries.size(), 1, 2, SelectedOldEntry, &Column1);

		static std::vector<unsigned char> s_vItemIds;
		static std::vector<CButtonContainer> s_vDeleteButtons;

		const int MaxEntries = GameClient()->m_WarList.m_vWarEntries.size();
		s_vItemIds.resize(MaxEntries);
		s_vDeleteButtons.resize(MaxEntries);

		for(size_t i = 0; i < vpFilteredEntries.size(); i++)
		{
			CWarEntry *pEntry = vpFilteredEntries[i];

			if(s_pSelectedEntry && pEntry == s_pSelectedEntry)
				SelectedOldEntry = i;

			const CListboxItem Item = s_EntriesListBox.DoNextItem(&s_vItemIds[i], SelectedOldEntry >= 0 && (size_t)SelectedOldEntry == i);
			if(!Item.m_Visible)
				continue;

			CUIRect EntryRect, DeleteButton, EntryTypeRect, WarType, ToolTip;
			Item.m_Rect.Margin(0.0f, &EntryRect);
			EntryRect.VSplitLeft(26.0f, &DeleteButton, &EntryRect);
			DeleteButton.HMargin(7.5f, &DeleteButton);
			DeleteButton.VSplitLeft(MarginSmall, nullptr, &DeleteButton);
			DeleteButton.VSplitRight(MarginExtraSmall, &DeleteButton, nullptr);

			if(Ui()->DoButton_FontIcon(&s_vDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				GameClient()->m_WarList.RemoveWarEntry(pEntry);

			bool IsClan = false;
			char aBuf[32];
			if(str_comp(pEntry->m_aClan, "") != 0)
			{
				str_copy(aBuf, pEntry->m_aClan);
				IsClan = true;
			}
			else
			{
				str_copy(aBuf, pEntry->m_aName);
			}
			EntryRect.VSplitLeft(35.0f, &EntryTypeRect, &EntryRect);

			if(IsClan)
			{
				RenderFontIcon(EntryTypeRect, FontIcon::ICON_USERS, 18.0f, TEXTALIGN_MC);
			}
			else
			{
				// TODO: stop misusing this function
				// TODO: render the real skin with skin remembering component (to be added)
				RenderDevSkin(EntryTypeRect.Center(), 35.0f, "default", "default", false, 0, 0, 0, false, false);
			}

			if(str_comp(pEntry->m_aReason, "") != 0)
			{
				EntryRect.VSplitRight(20.0f, &EntryRect, &ToolTip);
				RenderFontIcon(ToolTip, FontIcon::COMMENT, 18.0f, TEXTALIGN_MC);
				GameClient()->m_Tooltips.DoToolTip(&s_vItemIds[i], &ToolTip, pEntry->m_aReason);
				GameClient()->m_Tooltips.SetFadeTime(&s_vItemIds[i], 0.0f);
			}

			EntryRect.HMargin(MarginExtraSmall, &EntryRect);
			EntryRect.HSplitMid(&EntryRect, &WarType, MarginSmall);

			Ui()->DoLabel(&EntryRect, aBuf, StandardFontSize, TEXTALIGN_ML);
			TextRender()->TextColor(pEntry->m_pWarType->m_Color);
			Ui()->DoLabel(&WarType, pEntry->m_pWarType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		const int NewSelectedEntry = s_EntriesListBox.DoEnd();
		if(SelectedOldEntry != NewSelectedEntry || (SelectedOldEntry >= 0 && Ui()->HotItem() == &s_vItemIds[NewSelectedEntry] && Ui()->MouseButtonClicked(0)))
		{
			s_pSelectedEntry = vpFilteredEntries[NewSelectedEntry];
			if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
			{
				str_copy(s_aEntryName, s_pSelectedEntry->m_aName);
				str_copy(s_aEntryClan, s_pSelectedEntry->m_aClan);
				str_copy(s_aEntryReason, s_pSelectedEntry->m_aReason);
				if(str_comp(s_pSelectedEntry->m_aClan, "") != 0)
				{
					s_IsName = false;
					s_IsClan = true;
				}
				else
				{
					s_IsName = true;
					s_IsClan = false;
				}
				s_pSelectedType = s_pSelectedEntry->m_pWarType;
			}
		}

		Ui()->DoEditBox_Search(&s_EntriesFilterInput, &EntriesSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());
	}

	// ======WAR ENTRY EDITING======
	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Label.VSplitRight(25.0f, &Label, &Button);
	Ui()->DoLabel(&Label, TCLocalize("Edit Entry"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);

	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static CLineInput s_NameInput;
	s_NameInput.SetBuffer(s_aEntryName, sizeof(s_aEntryName));
	s_NameInput.SetEmptyText(TCLocalize("Name"));
	if(s_IsName)
	{
		Ui()->DoEditBox(&s_NameInput, &ButtonL, 12.0f);
	}
	else
	{
		ButtonL.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonL);
		ButtonL.VMargin(2.0f, &ButtonL);
		s_NameInput.Render(&ButtonL, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	static CLineInput s_ClanInput;
	s_ClanInput.SetBuffer(s_aEntryClan, sizeof(s_aEntryClan));
	s_ClanInput.SetEmptyText(TCLocalize("Clan"));
	if(s_IsClan)
		Ui()->DoEditBox(&s_ClanInput, &ButtonR, 12.0f);
	else
	{
		ButtonR.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), 15, 3.0f);
		Ui()->ClipEnable(&ButtonR);
		ButtonR.VMargin(2.0f, &ButtonR);
		s_ClanInput.Render(&ButtonR, 12.0f, TEXTALIGN_ML, false, -1.0f, 0.0f);
		Ui()->ClipDisable();
	}

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	static unsigned char s_NameRadio, s_ClanRadio;
	if(DoButton_CheckBox_Common(&s_NameRadio, TCLocalize("Name"), s_IsName ? "X" : "", &ButtonL, BUTTONFLAG_LEFT))
	{
		s_IsName = true;
		s_IsClan = false;
	}
	if(DoButton_CheckBox_Common(&s_ClanRadio, TCLocalize("Clan"), s_IsClan ? "X" : "", &ButtonR, BUTTONFLAG_LEFT))
	{
		s_IsName = false;
		s_IsClan = true;
	}
	if(!s_IsName)
		str_copy(s_aEntryName, "");
	if(!s_IsClan)
		str_copy(s_aEntryClan, "");

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize, &Button, &Column2);
	static CLineInput s_ReasonInput;
	s_ReasonInput.SetBuffer(s_aEntryReason, sizeof(s_aEntryReason));
	s_ReasonInput.SetEmptyText(TCLocalize("Reason"));
	Ui()->DoEditBox(&s_ReasonInput, &Button, 12.0f);

	static CButtonContainer s_AddButton, s_OverrideButton;

	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(LineSize * 2.0f, &Button, &Column2);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);

	if(DoButtonLineSize_Menu(&s_OverrideButton, TCLocalize("Override Entry"), 0, &ButtonL, LineSize) && s_pSelectedEntry)
	{
		if(s_pSelectedEntry && s_pSelectedType && (str_comp(s_aEntryName, "") != 0 || str_comp(s_aEntryClan, "") != 0))
		{
			str_copy(s_pSelectedEntry->m_aName, s_aEntryName);
			str_copy(s_pSelectedEntry->m_aClan, s_aEntryClan);
			str_copy(s_pSelectedEntry->m_aReason, s_aEntryReason);
			s_pSelectedEntry->m_pWarType = s_pSelectedType;
		}
	}
	if(DoButtonLineSize_Menu(&s_AddButton, TCLocalize("Add Entry"), 0, &ButtonR, LineSize))
	{
		if(s_pSelectedType)
			GameClient()->m_WarList.AddWarEntry(s_aEntryName, s_aEntryClan, s_aEntryReason, s_pSelectedType->m_aWarName);
	}
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);
	Column2.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column2);
	if(s_pSelectedType)
	{
		float Shade = 0.0f;
		Button.Draw(ColorRGBA(Shade, Shade, Shade, 0.25f), 15, 3.0f);
		TextRender()->TextColor(s_pSelectedType->m_Color);
		Ui()->DoLabel(&Button, s_pSelectedType->m_aWarName, HeadlineFontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	Column2.HSplitBottom(150.0f, nullptr, &Column2);

	Column2.HSplitTop(HeadlineHeight, &Label, &Column2);
	Ui()->DoLabel(&Label, TCLocalize("Settings"), HeadlineFontSize, TEXTALIGN_ML);
	Column2.HSplitTop(MarginSmall, nullptr, &Column2);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListAllowDuplicates, TCLocalize("Allow Duplicate Entries"), &g_Config.m_TcWarListAllowDuplicates, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarList, TCLocalize("Enable warlist"), &g_Config.m_TcWarList, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListChat, TCLocalize("Colors in chat"), &g_Config.m_TcWarListChat, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListScoreboard, TCLocalize("Colors in scoreboard"), &g_Config.m_TcWarListScoreboard, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListSpectate, TCLocalize("Colors in spectate select"), &g_Config.m_TcWarListSpectate, &Column2, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWarListShowClan, TCLocalize("Show clan if war"), &g_Config.m_TcWarListShowClan, &Column2, LineSize);

	// ======WAR TYPE EDITING======

	Column3.HSplitTop(HeadlineHeight, &Label, &Column3);
	Ui()->DoLabel(&Label, TCLocalize("War Groups"), HeadlineFontSize, TEXTALIGN_ML);
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);

	static char s_aTypeName[MAX_WARLIST_TYPE_LENGTH];
	static ColorRGBA s_GroupColor = ColorRGBA(1, 1, 1, 1);

	CUIRect WarTypeList;
	Column3.HSplitBottom(180.0f, &WarTypeList, &Column3);
	m_pRemoveWarType = nullptr;
	int SelectedOldType = -1;
	static CListBox s_WarTypeListBox;
	s_WarTypeListBox.DoStart(25.0f, GameClient()->m_WarList.m_WarTypes.size(), 1, 2, SelectedOldType, &WarTypeList, true, IGraphics::CORNER_ALL, true);

	static std::vector<unsigned char> s_vTypeItemIds;
	static std::vector<CButtonContainer> s_vTypeDeleteButtons;

	const int MaxTypes = GameClient()->m_WarList.m_WarTypes.size();
	s_vTypeItemIds.resize(MaxTypes);
	s_vTypeDeleteButtons.resize(MaxTypes);

	for(int i = 0; i < (int)GameClient()->m_WarList.m_WarTypes.size(); i++)
	{
		CWarType *pType = GameClient()->m_WarList.m_WarTypes[i];

		if(!pType)
			continue;

		if(s_pSelectedType && pType == s_pSelectedType)
			SelectedOldType = i;

		const CListboxItem Item = s_WarTypeListBox.DoNextItem(&s_vTypeItemIds[i], SelectedOldType >= 0 && SelectedOldType == i);
		if(!Item.m_Visible)
			continue;

		CUIRect TypeRect, DeleteButton;
		Item.m_Rect.Margin(0.0f, &TypeRect);

		if(pType->m_Removable)
		{
			TypeRect.VSplitRight(20.0f, &TypeRect, &DeleteButton);
			DeleteButton.HSplitTop(20.0f, &DeleteButton, nullptr);
			DeleteButton.Margin(2.0f, &DeleteButton);
			if(DoButtonNoRect_FontIcon(&s_vTypeDeleteButtons[i], FontIcon::TRASH, 0, &DeleteButton, IGraphics::CORNER_ALL))
				m_pRemoveWarType = pType;
		}
		TextRender()->TextColor(pType->m_Color);
		Ui()->DoLabel(&TypeRect, pType->m_aWarName, StandardFontSize, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	const int NewSelectedType = s_WarTypeListBox.DoEnd();
	if((SelectedOldType != NewSelectedType && NewSelectedType >= 0) || (NewSelectedType >= 0 && Ui()->HotItem() == &s_vTypeItemIds[NewSelectedType] && Ui()->MouseButtonClicked(0)))
	{
		s_pSelectedType = GameClient()->m_WarList.m_WarTypes[NewSelectedType];
		if(!Ui()->LastMouseButton(1) && !Ui()->LastMouseButton(2))
		{
			str_copy(s_aTypeName, s_pSelectedType->m_aWarName);
			s_GroupColor = s_pSelectedType->m_Color;
		}
	}
	if(m_pRemoveWarType != nullptr)
	{
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage),
			TCLocalize("Are you sure that you want to remove '%s' from your war groups?"),
			m_pRemoveWarType->m_aWarName);
		PopupConfirm(TCLocalize("Remove War Group"), aMessage, TCLocalize("Yes"), TCLocalize("No"), &CMenus::PopupConfirmRemoveWarType);
	}

	static CLineInput s_TypeNameInput;
	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	Column3.HSplitTop(HeadlineFontSize + MarginSmall, &Button, &Column3);
	s_TypeNameInput.SetBuffer(s_aTypeName, sizeof(s_aTypeName));
	s_TypeNameInput.SetEmptyText("Group Name");
	Ui()->DoEditBox(&s_TypeNameInput, &Button, 12.0f);
	static CButtonContainer s_AddGroupButton, s_OverrideGroupButton, s_GroupColorPicker;

	Column3.HSplitTop(MarginSmall, nullptr, &Column3);
	static unsigned int s_ColorValue = 0;
	s_ColorValue = color_cast<ColorHSLA>(s_GroupColor).Pack(false);
	ColorHSLA PickedColor = DoLine_ColorPicker(&s_GroupColorPicker, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &Column3, TCLocalize("Color"), &s_ColorValue, ColorRGBA(1.0f, 1.0f, 1.0f), true);
	s_GroupColor = color_cast<ColorRGBA>(PickedColor);

	Column3.HSplitTop(LineSize * 2.0f, &Button, &Column3);
	Button.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	bool OverrideDisabled = NewSelectedType == 0;
	if(DoButtonLineSize_Menu(&s_OverrideGroupButton, TCLocalize("Override Group"), 0, &ButtonL, LineSize, OverrideDisabled) && s_pSelectedType)
	{
		if(s_pSelectedType && str_comp(s_aTypeName, "") != 0)
		{
			str_copy(s_pSelectedType->m_aWarName, s_aTypeName);
			s_pSelectedType->m_Color = s_GroupColor;
		}
	}
	bool AddDisabled = str_comp(GameClient()->m_WarList.FindWarType(s_aTypeName)->m_aWarName, "none") != 0 || str_comp(s_aTypeName, "none") == 0;
	if(DoButtonLineSize_Menu(&s_AddGroupButton, TCLocalize("Add Group"), 0, &ButtonR, LineSize, AddDisabled))
	{
		GameClient()->m_WarList.AddWarType(s_aTypeName, s_GroupColor);
	}

	// ======ONLINE PLAYER LIST======

	Column4.HSplitTop(HeadlineHeight, &Label, &Column4);
	Ui()->DoLabel(&Label, TCLocalize("Online Players"), HeadlineFontSize, TEXTALIGN_ML);
	Column4.HSplitTop(MarginSmall, nullptr, &Column4);

	CUIRect PlayerSearch;
	Column4.HSplitBottom(25.0f, &Column4, &PlayerSearch);
	PlayerSearch.HSplitTop(MarginSmall, nullptr, &PlayerSearch);
	static CLineInputBuffered<128> s_PlayerSearchInput;
	Ui()->DoEditBox_Search(&s_PlayerSearchInput, &PlayerSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	CUIRect PlayerList;
	Column4.HSplitBottom(0.0f, &PlayerList, &Column4);
	static CListBox s_PlayerListBox;
	s_PlayerListBox.DoStart(30.0f, MAX_CLIENTS, 1, 2, -1, &PlayerList, true, IGraphics::CORNER_ALL, true);

	static std::vector<unsigned char> s_vPlayerItemIds;
	static std::vector<CButtonContainer> s_vNameButtons;
	static std::vector<CButtonContainer> s_vClanButtons;

	s_vPlayerItemIds.resize(MAX_CLIENTS);
	s_vNameButtons.resize(MAX_CLIENTS);
	s_vClanButtons.resize(MAX_CLIENTS);

	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!GameClient()->m_Snap.m_apPlayerInfos[i])
			continue;

		const auto &Client = GameClient()->m_aClients[i];

		if(!str_find_nocase(Client.m_aName, s_PlayerSearchInput.GetString()) &&
			!str_find_nocase(Client.m_aClan, s_PlayerSearchInput.GetString()))
			continue;

		const CListboxItem Item = s_PlayerListBox.DoNextItem(&s_vPlayerItemIds[i], false);
		if(!Item.m_Visible)
			continue;

		CUIRect PlayerRect, TeeRect, NameRect, ClanRect;
		Item.m_Rect.Margin(0.0f, &PlayerRect);
		PlayerRect.VSplitLeft(25.0f, &TeeRect, &PlayerRect);

		PlayerRect.VSplitMid(&NameRect, &ClanRect);
		PlayerRect = NameRect;
		PlayerRect.x = TeeRect.x;
		PlayerRect.w += TeeRect.w;
		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_NameColor);
		ColorRGBA NameButtonColor = Ui()->CheckActiveItem(&s_vNameButtons[i]) ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f) :
											(Ui()->HotItem() == &s_vNameButtons[i] ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f));
		PlayerRect.Draw(NameButtonColor, IGraphics::CORNER_L, 5.0f);
		Ui()->DoLabel(&NameRect, Client.m_aName, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vNameButtons[i], false, &PlayerRect, BUTTONFLAG_LEFT))
		{
			s_IsName = true;
			s_IsClan = false;
			str_copy(s_aEntryName, Client.m_aName);
		}

		TextRender()->TextColor(GameClient()->m_WarList.GetWarData(i).m_ClanColor);
		ColorRGBA ClanButtonColor = Ui()->CheckActiveItem(&s_vClanButtons[i]) ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.75f) :
											(Ui()->HotItem() == &s_vClanButtons[i] ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.33f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f));
		ClanRect.Draw(ClanButtonColor, IGraphics::CORNER_R, 5.0f);
		Ui()->DoLabel(&ClanRect, Client.m_aClan, StandardFontSize, TEXTALIGN_ML);
		if(Ui()->DoButtonLogic(&s_vClanButtons[i], false, &ClanRect, BUTTONFLAG_LEFT))
		{
			s_IsName = false;
			s_IsClan = true;
			str_copy(s_aEntryClan, Client.m_aClan);
		}
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		CTeeRenderInfo TeeInfo = Client.m_RenderInfo;
		TeeInfo.m_Size = 25.0f;
		RenderTeeCute(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.5f), true);
	}
	s_PlayerListBox.DoEnd();
}

void CMenus::RenderSettingsTClientStatusBar(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, StatusBar;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitBottom(100.0f, &MainView, &StatusBar);

	CUIRect StatusFrame = StatusBar;
	StatusFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect StatusInner = StatusBar;
	StatusInner.Margin(2.0f, &StatusInner);
	StatusInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	StatusBar.Margin(8.0f, &StatusBar);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	CUIRect LeftFrame = LeftView;
	LeftFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect LeftInner = LeftView;
	LeftInner.Margin(2.0f, &LeftInner);
	LeftInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	LeftView.Margin(8.0f, &LeftView);

	CUIRect RightFrame = RightView;
	RightFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect RightInner = RightView;
	RightInner.Margin(2.0f, &RightInner);
	RightInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	RightView.Margin(8.0f, &RightView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Status Bar"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBar, TCLocalize("Show status bar"), &g_Config.m_TcStatusBar, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBarLabels, TCLocalize("Show labels on status bar items"), &g_Config.m_TcStatusBarLabels, &LeftView, LineSize);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarHeight, &g_Config.m_TcStatusBarHeight, &Button, TCLocalize("Status bar height"), 1, 16);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Local Time"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBar12HourClock, TCLocalize("Use 12 hour clock"), &g_Config.m_TcStatusBar12HourClock, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcStatusBarLocalTimeSeocnds, TCLocalize("Show seconds on clock"), &g_Config.m_TcStatusBarLocalTimeSeocnds, &LeftView, LineSize);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Colors"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	static CButtonContainer s_StatusbarColor, s_StatusbarTextColor;

	DoLine_ColorPicker(&s_StatusbarColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Status bar color"), &g_Config.m_TcStatusBarColor, ColorRGBA(0.0f, 0.0f, 0.0f), false);
	DoLine_ColorPicker(&s_StatusbarTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Text color"), &g_Config.m_TcStatusBarTextColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarAlpha, &g_Config.m_TcStatusBarAlpha, &Button, TCLocalize("Status bar alpha"), 0, 100);
	LeftView.HSplitTop(LineSize, &Button, &LeftView);
	Ui()->DoScrollbarOption(&g_Config.m_TcStatusBarTextAlpha, &g_Config.m_TcStatusBarTextAlpha, &Button, TCLocalize("Text alpha"), 0, 100);

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Status Bar Codes:"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("a = Angle"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("p = Ping"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("d = Prediction"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("c = Position"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("l = Local Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("r = Race Time"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("f = FPS"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("v = Velocity"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("z = Zoom"), FontSize, TEXTALIGN_ML);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("_ or ' ' = Space"), FontSize, TEXTALIGN_ML);
	static int s_SelectedItem = -1;
	static int s_TypeSelectedOld = -1;

	CUIRect StatusScheme, StatusButtons, ItemLabel;
	static CButtonContainer s_ApplyButton, s_AddButton, s_RemoveButton;
	StatusBar.HSplitBottom(LineSize + MarginSmall, &StatusBar, &StatusScheme);
	StatusBar.HSplitTop(LineSize + MarginSmall, &ItemLabel, &StatusBar);
	StatusScheme.HSplitTop(MarginSmall, nullptr, &StatusScheme);

	if(s_TypeSelectedOld >= 0)
		Ui()->DoLabel(&ItemLabel, GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld].m_aDesc, FontSize, TEXTALIGN_ML);

	StatusScheme.VSplitMid(&StatusButtons, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&Label, &StatusScheme, MarginSmall);
	StatusScheme.VSplitMid(&StatusScheme, &Button, MarginSmall);
	if(DoButton_Menu(&s_ApplyButton, TCLocalize("Apply"), 0, &Button))
	{
		GameClient()->m_StatusBar.ApplyStatusBarScheme(g_Config.m_TcStatusBarScheme);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = -1;
	}
	Ui()->DoLabel(&Label, TCLocalize("Status Scheme:"), FontSize, TEXTALIGN_MR);
	static CLineInput s_StatusScheme(g_Config.m_TcStatusBarScheme, sizeof(g_Config.m_TcStatusBarScheme));
	s_StatusScheme.SetEmptyText("");
	Ui()->DoEditBox(&s_StatusScheme, &StatusScheme, EditBoxFontSize);

	static std::vector<const char *> s_DropDownNames = {};
	for(const CStatusItem &StatusItemType : GameClient()->m_StatusBar.m_StatusItemTypes)
		if(s_DropDownNames.size() != GameClient()->m_StatusBar.m_StatusItemTypes.size())
			s_DropDownNames.push_back(StatusItemType.m_aName);

	static CUi::SDropDownState s_DropDownState;
	static CScrollRegion s_DropDownScrollRegion;
	s_DropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_DropDownScrollRegion;
	CUIRect DropDownRect;

	StatusButtons.VSplitMid(&DropDownRect, &StatusButtons, MarginSmall);
	const int TypeSelectedNew = Ui()->DoDropDown(&DropDownRect, s_TypeSelectedOld, s_DropDownNames.data(), s_DropDownNames.size(), s_DropDownState);
	if(s_TypeSelectedOld != TypeSelectedNew)
	{
		s_TypeSelectedOld = TypeSelectedNew;
		if(s_SelectedItem >= 0)
		{
			GameClient()->m_StatusBar.m_StatusBarItems[s_SelectedItem] = &GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld];
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		}
	}
	CUIRect ButtonL, ButtonR;
	StatusButtons.VSplitMid(&ButtonL, &ButtonR, MarginSmall);
	size_t NumItems = GameClient()->m_StatusBar.m_StatusBarItems.size();
	if(DoButton_Menu(&s_AddButton, TCLocalize("Add Item"), 0, &ButtonL) && s_TypeSelectedOld >= 0 && NumItems < 128)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.push_back(&GameClient()->m_StatusBar.m_StatusItemTypes[s_TypeSelectedOld]);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = (int)GameClient()->m_StatusBar.m_StatusBarItems.size() - 1;
	}
	if(DoButton_Menu(&s_RemoveButton, TCLocalize("Remove Item"), 0, &ButtonR) && s_SelectedItem >= 0)
	{
		GameClient()->m_StatusBar.m_StatusBarItems.erase(GameClient()->m_StatusBar.m_StatusBarItems.begin() + s_SelectedItem);
		GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		s_SelectedItem = -1;
	}

	// color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcStatusBarColor)).WithAlpha(0.5f)
	StatusBar.Draw(ColorRGBA(0, 0, 0, 0.5f), IGraphics::CORNER_ALL, 5.0f);
	int ItemCount = GameClient()->m_StatusBar.m_StatusBarItems.size();
	float AvailableWidth = StatusBar.w;
	// AvailableWidth -= (ItemCount - 1) * MarginSmall;
	AvailableWidth -= MarginSmall;
	StatusBar.VSplitLeft(MarginExtraSmall, nullptr, &StatusBar);
	float ItemWidth = AvailableWidth / (float)ItemCount;
	CUIRect StatusItemButton;
	static std::vector<CButtonContainer *> s_pItemButtons;
	static std::vector<CButtonContainer> s_ItemButtons;
	static vec2 s_ActivePos = vec2(0.0f, 0.0f);
	class CSwapItem
	{
	public:
		vec2 m_InitialPosition = vec2(0.0f, 0.0f);
		float m_Duration = 0.0f;
	};

	static std::vector<CSwapItem> s_ItemSwaps;

	if((int)s_ItemButtons.size() != ItemCount)
	{
		s_ItemSwaps.resize(ItemCount);
		s_pItemButtons.resize(ItemCount);
		s_ItemButtons.resize(ItemCount);
		for(int i = 0; i < ItemCount; ++i)
		{
			s_pItemButtons[i] = &s_ItemButtons[i];
		}
	}
	bool StatusItemActive = false;
	int HotStatusIndex = 0;
	for(int i = 0; i < ItemCount; ++i)
	{
		if(Ui()->ActiveItem() == s_pItemButtons[i])
		{
			StatusItemActive = true;
			HotStatusIndex = i;
		}
	}

	for(int i = 0; i < ItemCount; ++i)
	{
		// if(i > 0)
		//	StatusBar.VSplitLeft(MarginSmall, nullptr, &StatusBar);
		StatusBar.VSplitLeft(ItemWidth, &StatusItemButton, &StatusBar);
		StatusItemButton.HMargin(MarginSmall, &StatusItemButton);
		StatusItemButton.VMargin(MarginExtraSmall, &StatusItemButton);
		CStatusItem *StatusItem = GameClient()->m_StatusBar.m_StatusBarItems[i];
		ColorRGBA Col = ColorRGBA(1.0f, 1.0f, 1.0f, 0.5f);
		if(s_SelectedItem == i)
			Col = ColorRGBA(1.0f, 0.35f, 0.35f, 0.75f);
		CUIRect TempItemButton = StatusItemButton;
		TempItemButton.y = 0, TempItemButton.h = 10000.0f;
		if(StatusItemActive && Ui()->ActiveItem() != s_pItemButtons[i] && Ui()->MouseInside(&TempItemButton))
		{
			std::swap(s_pItemButtons[i], s_pItemButtons[HotStatusIndex]);
			std::swap(GameClient()->m_StatusBar.m_StatusBarItems[i], GameClient()->m_StatusBar.m_StatusBarItems[HotStatusIndex]);
			s_SelectedItem = -2;
			s_ItemSwaps[HotStatusIndex].m_InitialPosition = vec2(StatusItemButton.x, StatusItemButton.y);
			s_ItemSwaps[HotStatusIndex].m_Duration = 0.15f;
			s_ItemSwaps[i].m_InitialPosition = vec2(s_ActivePos.x, s_ActivePos.y);
			s_ItemSwaps[i].m_Duration = 0.15f;
			GameClient()->m_StatusBar.UpdateStatusBarScheme(g_Config.m_TcStatusBarScheme);
		}
		TempItemButton = StatusItemButton;
		s_ItemSwaps[i].m_Duration = std::max(0.0f, s_ItemSwaps[i].m_Duration - Client()->RenderFrameTime());
		if(s_ItemSwaps[i].m_Duration > 0.0f)
		{
			float Progress = std::pow(2.0, -5.0 * (1.0 - s_ItemSwaps[i].m_Duration / 0.15f));
			TempItemButton.x = mix(TempItemButton.x, s_ItemSwaps[i].m_InitialPosition.x, Progress);
		}
		if(DoButtonLineSize_Menu(s_pItemButtons[i], StatusItem->m_aDisplayName, 0, &TempItemButton, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, Col))
		{
			if(s_SelectedItem == -2)
				s_SelectedItem++;
			else if(s_SelectedItem != i)
			{
				s_SelectedItem = i;
				for(int t = 0; t < (int)GameClient()->m_StatusBar.m_StatusItemTypes.size(); ++t)
					if(str_comp(GameClient()->m_StatusBar.m_StatusItemTypes[t].m_aName, StatusItem->m_aName) == 0)
						s_TypeSelectedOld = t;
			}
			else
			{
				s_SelectedItem = -1;
				s_TypeSelectedOld = -1;
			}
		}
		if(Ui()->ActiveItem() == s_pItemButtons[i])
			s_ActivePos = vec2(StatusItemButton.x, StatusItemButton.y);
	}
	if(!StatusItemActive)
		s_SelectedItem = std::max(-1, s_SelectedItem);
}

void CMenus::RenderSettingsTClientInfo(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, LowerLeftView;
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	// Save full areas for borders
	CUIRect LinksBox, ConfigBox, RightBox;

	LinksBox = LeftView;
	LeftView.HSplitMid(&LeftView, &LowerLeftView, 0.0f);
	LinksBox.h = LeftView.y + LeftView.h - LinksBox.y;

	ConfigBox = LowerLeftView;

	RightBox = RightView;

	// Draw borders
	LinksBox.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect LinksInner = LinksBox;
	LinksInner.Margin(2.0f, &LinksInner);
	LinksInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);

	ConfigBox.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect ConfigInner = ConfigBox;
	ConfigInner.Margin(2.0f, &ConfigInner);
	ConfigInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);

	RightBox.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect RightInner = RightBox;
	RightInner.Margin(2.0f, &RightInner);
	RightInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);

	LeftView.Margin(8.0f, &LeftView);
	LowerLeftView.Margin(8.0f, &LowerLeftView);
	RightView.Margin(8.0f, &RightView);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
    Ui()->DoLabel(&Label, "Enlaces M\316\233 \343\203\204", HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	static CButtonContainer s_DiscordButton, s_WebsiteButton, s_GithubButton, s_SupportButton;
	CUIRect ButtonLeft, ButtonRight;

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);
	if(DoButtonLineSize_Menu(&s_DiscordButton, TCLocalize("Discord"), 0, &ButtonLeft, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://discord.gg/fBvhH93Bt6");
	if(DoButtonLineSize_Menu(&s_WebsiteButton, TCLocalize("Website"), 0, &ButtonRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
		Client()->ViewLink("https://tclient.app/");

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&ButtonLeft, &ButtonRight, MarginSmall);

	if(DoButtonLineSize_Menu(&s_GithubButton, TCLocalize("Github"), 0, &ButtonLeft, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
        Client()->ViewLink("https://github.com/thecoderma69/M-client");
    if(DoButtonLineSize_Menu(&s_SupportButton, TCLocalize("Soporte"), 0, &ButtonRight, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
        Client()->ViewLink("https://github.com/thecoderma69");

	LeftView = LowerLeftView;
	LeftView.HSplitBottom(LineSize * 4.0f + MarginSmall * 2.0f + HeadlineFontSize, nullptr, &LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Config Files"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	char aBuf[128 + IO_MAX_PATH_LENGTH];
	CUIRect TClientConfig, ProfilesFile, WarlistFile, ChatbindsFile;

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&TClientConfig, &ProfilesFile, MarginSmall);

	static CButtonContainer s_Config, s_Profiles, s_Warlist, s_Chatbinds;
	if(DoButtonLineSize_Menu(&s_Config, TCLocalize("TClient Settings"), 0, &TClientConfig, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENT].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButtonLineSize_Menu(&s_Profiles, TCLocalize("Profiles"), 0, &ProfilesFile, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	LeftView.HSplitTop(LineSize * 2.0f, &Button, &LeftView);
	Button.VSplitMid(&WarlistFile, &ChatbindsFile, MarginSmall);

	if(DoButtonLineSize_Menu(&s_Warlist, TCLocalize("War List"), 0, &WarlistFile, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTWARLIST].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButtonLineSize_Menu(&s_Chatbinds, TCLocalize("Chat Binds"), 0, &ChatbindsFile, LineSize, false, 0, IGraphics::CORNER_ALL, 5.0f, 0.0f, ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f)))
	{
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTCHATBINDS].m_aConfigPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}

	// =======RIGHT VIEW========

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
    Ui()->DoLabel(&Label, "Desarrolladores de M\316\233 \343\203\204", HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	const float TeeSize = 34.0f;
	const float CardSize = TeeSize + MarginSmall;
	CUIRect TeeRect, DevCardRect;
	static CButtonContainer s_LinkButton6;
	{
		RightView.HSplitTop(CardSize, &DevCardRect, &RightView);
		DevCardRect.VSplitLeft(CardSize, &TeeRect, &Label);
        Label.VSplitLeft(TextRender()->TextWidth(LineSize, "M\316\233 \343\203\204"), &Label, &Button);
		Button.VSplitLeft(MarginSmall, nullptr, &Button);
		Button.w = LineSize, Button.h = LineSize, Button.y = Label.y + (Label.h / 2.0f - Button.h / 2.0f);
        Ui()->DoLabel(&Label, "M\316\233 \343\203\204", LineSize, TEXTALIGN_ML);
		if(Ui()->DoButton_FontIcon(&s_LinkButton6, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, 0, &Button, IGraphics::CORNER_ALL))
			Client()->ViewLink("https://github.com/thecoderma69");
		RenderDevSkin(TeeRect.Center(), TeeSize, "ahl_blackbop", "default", false, 0, 0, 0, false, true);
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Hide Settings Tabs"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	CUIRect LeftSettings, RightSettings;

	RightView.VSplitMid(&LeftSettings, &RightSettings, MarginSmall);
	RightView.HSplitTop(LineSize * 3.5f, nullptr, &RightView);

	const char *apTabNames[] = {
		TCLocalize("Settings"),
		TCLocalize("Bind Wheel"),
		TCLocalize("War List"),
		TCLocalize("Chat Binds"),
		TCLocalize("Status Bar"),
		TCLocalize("Lluvia"),
		TCLocalize("Anime Love"),
		TCLocalize("Info")};
	static int s_aShowTabs[NUMBER_OF_TCLIENT_TABS] = {};
	for(int i = 0; i < NUMBER_OF_TCLIENT_TABS; ++i)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&s_aShowTabs[i], apTabNames[i], &s_aShowTabs[i], i % 2 == 0 ? &LeftSettings : &RightSettings, LineSize);
		SetFlag(g_Config.m_TcTClientSettingsTabs, i, s_aShowTabs[i]);
	}

	// RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	// Ui()->DoLabel(&Label, TCLocalize("Integration"), HeadlineFontSize, TEXTALIGN_ML);
	// RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	// DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcDiscordRPC, TCLocalize("Enable Discord Integration"), &g_Config.m_TcDiscordRPC, &RightView, LineSize);
}

void CMenus::RenderSettingsTClientKeystroke(CUIRect MainView)
{
	RenderMaKeystroke(MainView);
}
void CMenus::RenderSettingsTClientProfiles(CUIRect MainView)
{
	int *pCurrentUseCustomColor = m_Dummy ? &g_Config.m_ClDummyUseCustomColor : &g_Config.m_ClPlayerUseCustomColor;

	const char *pCurrentSkinName = m_Dummy ? g_Config.m_ClDummySkin : g_Config.m_ClPlayerSkin;
	const unsigned CurrentColorBody = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorBody : g_Config.m_ClPlayerColorBody) : -1;
	const unsigned CurrentColorFeet = *pCurrentUseCustomColor == 1 ? (m_Dummy ? g_Config.m_ClDummyColorFeet : g_Config.m_ClPlayerColorFeet) : -1;
	const int CurrentFlag = m_Dummy ? g_Config.m_ClDummyCountry : g_Config.m_PlayerCountry;
	const int Emote = m_Dummy ? g_Config.m_ClDummyDefaultEyes : g_Config.m_ClPlayerDefaultEyes;
	const char *pCurrentName = m_Dummy ? g_Config.m_ClDummyName : g_Config.m_PlayerName;
	const char *pCurrentClan = m_Dummy ? g_Config.m_ClDummyClan : g_Config.m_PlayerClan;

	const CProfile CurrentProfile(
		CurrentColorBody,
		CurrentColorFeet,
		CurrentFlag,
		Emote,
		pCurrentSkinName,
		pCurrentName,
		pCurrentClan);

	static int s_SelectedProfile = -1;

	CUIRect Label, Button;

	auto RenderProfile = [&](CUIRect Rect, const CProfile &Profile, bool Main) {
		auto RenderCross = [&](CUIRect Cross, float MaxSize = 0.0f) {
			float MaxExtent = std::max(Cross.w, Cross.h);
			if(MaxSize > 0.0f && MaxExtent > MaxSize)
				MaxExtent = MaxSize;
			TextRender()->TextColor(ColorRGBA(1.0f, 0.0f, 0.0f));
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			const auto TextBoundingBox = TextRender()->TextBoundingBox(MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->Text(Cross.x + (Cross.w - TextBoundingBox.m_W) / 2.0f, Cross.y + (Cross.h - TextBoundingBox.m_H) / 2.0f, MaxExtent * 0.8f, FontIcon::XMARK);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
		};
		{
			CUIRect Skin;
			Rect.VSplitLeft(50.0f, &Skin, &Rect);
			if(!Main && Profile.m_SkinName[0] == '\0')
			{
				RenderCross(Skin, 20.0f);
			}
			else
			{
				CTeeRenderInfo TeeRenderInfo;
				TeeRenderInfo.Apply(GameClient()->m_Skins.Find(Profile.m_SkinName));
				TeeRenderInfo.ApplyColors(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0, Profile.m_BodyColor, Profile.m_FeetColor);
				TeeRenderInfo.m_Size = 50.0f;
				const vec2 Pos = Skin.Center() + vec2(0.0f, TeeRenderInfo.m_Size / 10.0f); // Prevent overflow from hats
				vec2 Dir = vec2(1.0f, 0.0f);
				if(Main)
					RenderTeeCute(CAnimState::GetIdle(), &TeeRenderInfo, std::max(0, Profile.m_Emote), Dir, Pos, false);
				else
					RenderTools()->RenderTee(CAnimState::GetIdle(), &TeeRenderInfo, std::max(0, Profile.m_Emote), Dir, Pos);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Colors;
			Rect.VSplitLeft(10.0f, &Colors, &Rect);
			CUIRect BodyColor{Colors.Center().x - 5.0f, Colors.Center().y - 11.0f, 10.0f, 10.0f};
			CUIRect FeetColor{Colors.Center().x - 5.0f, Colors.Center().y + 1.0f, 10.0f, 10.0f};
			if(Profile.m_BodyColor >= 0 && Profile.m_FeetColor > 0)
			{
				// Body Color
				Graphics()->DrawRect(BodyColor.x, BodyColor.y, BodyColor.w, BodyColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_BodyColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
				// Feet Color;
				Graphics()->DrawRect(FeetColor.x, FeetColor.y, FeetColor.w, FeetColor.h,
					color_cast<ColorRGBA>(ColorHSLA(Profile.m_FeetColor).UnclampLighting(ColorHSLA::DARKEST_LGT)).WithAlpha(1.0f),
					IGraphics::CORNER_ALL, 2.0f);
			}
			else
			{
				RenderCross(BodyColor);
				RenderCross(FeetColor);
			}
		}
		Rect.VSplitLeft(5.0f, nullptr, &Rect);
		{
			CUIRect Flag;
			Rect.VSplitRight(50.0f, &Rect, &Flag);
			Flag = {Flag.x, Flag.y + (Flag.h - 25.0f) / 2.0f, Flag.w, 25.0f};
			if(Profile.m_CountryFlag == -2)
				RenderCross(Flag, 20.0f);
			else
				GameClient()->m_CountryFlags.Render(Profile.m_CountryFlag, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), Flag.x, Flag.y, Flag.w, Flag.h);
		}
		Rect.VSplitRight(5.0f, &Rect, nullptr);
		{
			const float Height = Rect.h / 3.0f;
			if(Main)
			{
				char aBuf[256];
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Name: %s"), Profile.m_Name);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Clan: %s"), Profile.m_Clan);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				str_format(aBuf, sizeof(aBuf), TCLocalize("Skin: %s"), Profile.m_SkinName);
				Ui()->DoLabel(&Label, aBuf, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
			else
			{
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Name, Height / LineSize * FontSize, TEXTALIGN_ML);
				Rect.HSplitTop(Height, &Label, &Rect);
				Ui()->DoLabel(&Label, Profile.m_Clan, Height / LineSize * FontSize, TEXTALIGN_ML);
			}
		}
	};

	{
		CUIRect Top;
		MainView.HSplitTop(160.0f, &Top, &MainView);
		CUIRect Profiles, Settings, Actions;
		Top.VSplitLeft(300.0f, &Profiles, &Top);
		{
			CUIRect Skin;
			Profiles.HSplitTop(LineSize, &Label, &Profiles);
			Ui()->DoLabel(&Label, TCLocalize("Your profile"), FontSize, TEXTALIGN_ML);
			Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
			Profiles.HSplitTop(50.0f, &Skin, &Profiles);
			RenderProfile(Skin, CurrentProfile, true);

			// After load
			if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
			{
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(LineSize, &Label, &Profiles);
				Ui()->DoLabel(&Label, TCLocalize("After Load"), FontSize, TEXTALIGN_ML);
				Profiles.HSplitTop(MarginSmall, nullptr, &Profiles);
				Profiles.HSplitTop(50.0f, &Skin, &Profiles);

				CProfile LoadProfile = CurrentProfile;
				const CProfile &Profile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
				if(g_Config.m_TcProfileSkin && strlen(Profile.m_SkinName) != 0)
					str_copy(LoadProfile.m_SkinName, Profile.m_SkinName);
				if(g_Config.m_TcProfileColors && Profile.m_BodyColor != -1 && Profile.m_FeetColor != -1)
				{
					LoadProfile.m_BodyColor = Profile.m_BodyColor;
					LoadProfile.m_FeetColor = Profile.m_FeetColor;
				}
				if(g_Config.m_TcProfileEmote && Profile.m_Emote != -1)
					LoadProfile.m_Emote = Profile.m_Emote;
				if(g_Config.m_TcProfileName && strlen(Profile.m_Name) != 0)
					str_copy(LoadProfile.m_Name, Profile.m_Name);
				if(g_Config.m_TcProfileClan && (strlen(Profile.m_Clan) != 0 || g_Config.m_TcProfileOverwriteClanWithEmpty))
					str_copy(LoadProfile.m_Clan, Profile.m_Clan);
				if(g_Config.m_TcProfileFlag && Profile.m_CountryFlag != -2)
					LoadProfile.m_CountryFlag = Profile.m_CountryFlag;

				RenderProfile(Skin, LoadProfile, true);
			}
		}
		Top.VSplitLeft(20.0f, nullptr, &Top);
		Top.VSplitMid(&Settings, &Actions, 20.0f);
		{
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileSkin, TCLocalize("Save/Load Skin"), &g_Config.m_TcProfileSkin, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileColors, TCLocalize("Save/Load Colors"), &g_Config.m_TcProfileColors, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileEmote, TCLocalize("Save/Load Emote"), &g_Config.m_TcProfileEmote, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileName, TCLocalize("Save/Load Name"), &g_Config.m_TcProfileName, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileClan, TCLocalize("Save/Load Clan"), &g_Config.m_TcProfileClan, &Settings, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcProfileFlag, TCLocalize("Save/Load Flag"), &g_Config.m_TcProfileFlag, &Settings, LineSize);
		}
		{
			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_LoadButton;
			if(DoButton_Menu(&s_LoadButton, TCLocalize("Load"), 0, &Button))
			{
				if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
				{
					CProfile LoadProfile = GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile];
					GameClient()->m_SkinProfiles.ApplyProfile(m_Dummy, LoadProfile);
				}
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			Actions.HSplitTop(30.0f, &Button, &Actions);
			static CButtonContainer s_SaveButton;
			if(DoButton_Menu(&s_SaveButton, TCLocalize("Save"), 0, &Button))
			{
				GameClient()->m_SkinProfiles.AddProfile(
					g_Config.m_TcProfileColors ? CurrentColorBody : -1,
					g_Config.m_TcProfileColors ? CurrentColorFeet : -1,
					g_Config.m_TcProfileFlag ? CurrentFlag : -2,
					g_Config.m_TcProfileEmote ? Emote : -1,
					g_Config.m_TcProfileSkin ? pCurrentSkinName : "",
					g_Config.m_TcProfileName ? pCurrentName : "",
					g_Config.m_TcProfileClan ? pCurrentClan : "");
			}
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			static int s_AllowDelete;
			DoButton_CheckBoxAutoVMarginAndSet(&s_AllowDelete, Localizable("Enable Deleting"), &s_AllowDelete, &Actions, LineSize);
			Actions.HSplitTop(5.0f, nullptr, &Actions);

			if(s_AllowDelete)
			{
				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_DeleteButton;
				if(DoButton_Menu(&s_DeleteButton, TCLocalize("Delete"), 0, &Button))
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
						GameClient()->m_SkinProfiles.m_Profiles.erase(GameClient()->m_SkinProfiles.m_Profiles.begin() + s_SelectedProfile);
				Actions.HSplitTop(5.0f, nullptr, &Actions);

				Actions.HSplitTop(30.0f, &Button, &Actions);
				static CButtonContainer s_OverrideButton;
				if(DoButton_Menu(&s_OverrideButton, TCLocalize("Override"), 0, &Button))
				{
					if(s_SelectedProfile != -1 && s_SelectedProfile < (int)GameClient()->m_SkinProfiles.m_Profiles.size())
					{
						GameClient()->m_SkinProfiles.m_Profiles[s_SelectedProfile] = CProfile(
							g_Config.m_TcProfileColors ? CurrentColorBody : -1,
							g_Config.m_TcProfileColors ? CurrentColorFeet : -1,
							g_Config.m_TcProfileFlag ? CurrentFlag : -2,
							g_Config.m_TcProfileEmote ? Emote : -1,
							g_Config.m_TcProfileSkin ? pCurrentSkinName : "",
							g_Config.m_TcProfileName ? pCurrentName : "",
							g_Config.m_TcProfileClan ? pCurrentClan : "");
					}
				}
			}
		}
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect Options;
		MainView.HSplitTop(LineSize, &Options, &MainView);

		Options.VSplitLeft(150.0f, &Button, &Options);
		if(DoButton_CheckBox(&m_Dummy, TCLocalize("Dummy"), m_Dummy, &Button))
			m_Dummy = 1 - m_Dummy;

		Options.VSplitLeft(150.0f, &Button, &Options);
		static int s_CustomColorId = 0;
		if(DoButton_CheckBox(&s_CustomColorId, TCLocalize("Custom colors"), *pCurrentUseCustomColor, &Button))
		{
			*pCurrentUseCustomColor = *pCurrentUseCustomColor ? 0 : 1;
			SetNeedSendInfo();
		}

		Button = Options;
		if(DoButton_CheckBox(&g_Config.m_TcProfileOverwriteClanWithEmpty, TCLocalize("Overwrite clan even if empty"), g_Config.m_TcProfileOverwriteClanWithEmpty, &Button))
			g_Config.m_TcProfileOverwriteClanWithEmpty = 1 - g_Config.m_TcProfileOverwriteClanWithEmpty;
	}
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);
	{
		CUIRect SelectorRect;
		MainView.HSplitBottom(LineSize + MarginSmall, &MainView, &SelectorRect);
		SelectorRect.HSplitTop(MarginSmall, nullptr, &SelectorRect);

		static CButtonContainer s_ProfilesFile;
		SelectorRect.VSplitLeft(130.0f, &Button, &SelectorRect);
		if(DoButton_Menu(&s_ProfilesFile, TCLocalize("Profiles file"), 0, &Button))
		{
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, s_aConfigDomains[ConfigDomain::TCLIENTPROFILES].m_aConfigPath, aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
	}

	const std::vector<CProfile> &ProfileList = GameClient()->m_SkinProfiles.m_Profiles;
	static CListBox s_ListBox;
	s_ListBox.DoStart(50.0f, ProfileList.size(), MainView.w / 200.0f, 3, s_SelectedProfile, &MainView, true, IGraphics::CORNER_ALL, true);

	static bool s_Indexes[1024];

	for(size_t i = 0; i < ProfileList.size(); ++i)
	{
		CListboxItem Item = s_ListBox.DoNextItem(&s_Indexes[i], s_SelectedProfile >= 0 && (size_t)s_SelectedProfile == i);
		if(!Item.m_Visible)
			continue;

		RenderProfile(Item.m_Rect, ProfileList[i], false);
	}

	s_SelectedProfile = s_ListBox.DoEnd();
}

void CMenus::RenderSettingsTClientConfigs(CUIRect MainView)
{
	// hi hello, this is a relatively self contained mess, sorry if you're forking or need to modify this -Tater

	struct SIntStage
	{
		int m_Value;
	};
	struct SStrStage
	{
		std::string m_Value;
	};
	struct SColStage
	{
		unsigned m_Value;
	};
	static std::unordered_map<const SConfigVariable *, SIntStage> s_StagedInts;
	static std::unordered_map<const SConfigVariable *, SStrStage> s_StagedStrs;
	static std::unordered_map<const SConfigVariable *, SColStage> s_StagedCols;

	struct SIntState
	{
		CLineInputNumber m_Input;
		int m_LastValue = 0;
		bool m_Inited = false;
	};
	struct SStrState
	{
		CLineInputBuffered<512> m_Input;
		bool m_Inited = false;
	};
	struct SColState
	{
		unsigned m_LastValue = 0;
		unsigned m_Working = 0;
		bool m_Inited = false;
	};
	static std::unordered_map<const SConfigVariable *, SIntState> s_IntInputs;
	static std::unordered_map<const SConfigVariable *, SStrState> s_StrInputs;
	static std::unordered_map<const SConfigVariable *, SColState> s_ColInputs;

	auto ClearStagedAndCaches = [&]() {
		s_StagedInts.clear();
		s_StagedStrs.clear();
		s_StagedCols.clear();
		s_IntInputs.clear();
		s_StrInputs.clear();
		s_ColInputs.clear();
	};

	size_t ChangesCount = 0;

	CUIRect ApplyBar, TopBar, ListArea;
	MainView.VSplitRight(5.0f, &MainView, nullptr); // padding for scrollbar
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.HSplitTop(LineSize + MarginSmall, &ApplyBar, &MainView);
	MainView.HSplitTop(LineSize + MarginSmall, &TopBar, &ListArea);
	ListArea.HSplitTop(MarginSmall, nullptr, &ListArea);

	static CLineInputBuffered<128> s_SearchInput;

	ChangesCount = s_StagedInts.size() + s_StagedStrs.size() + s_StagedCols.size();
	{
		CUIRect LeftHalf, RightHalf;
		ApplyBar.VSplitMid(&LeftHalf, &RightHalf, 0.0f);
		CUIRect Row = LeftHalf;
		Row.HMargin(MarginSmall, &Row);
		Row.h = LineSize;
		Row.y = ApplyBar.y + (ApplyBar.h - LineSize) / 2.0f;

		const float BtnWidth = 120.0f;
		CUIRect ApplyBtn, ClearBtn, Counter;
		Row.VSplitLeft(BtnWidth, &ApplyBtn, &Row);
		Row.VSplitLeft(MarginSmall, nullptr, &Row);
		Row.VSplitLeft(BtnWidth, &ClearBtn, &Row);
		Row.VSplitLeft(MarginSmall, nullptr, &Counter);

		static CButtonContainer s_ApplyBtn, s_ClearBtn;
		int DisabledStyle = ChangesCount > 0 ? 0 : -1;
		const bool ApplyClicked = DoButton_Menu(&s_ApplyBtn, Localize("Apply Changes"), DisabledStyle, &ApplyBtn);
		if(ChangesCount > 0 && ApplyClicked)
		{
			for(const auto &It : s_StagedInts)
			{
				const SConfigVariable *pVar = It.first;
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %d", pVar->m_pScriptName, It.second.m_Value);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			for(const auto &It : s_StagedStrs)
			{
				const SConfigVariable *pVar = It.first;
				char aEsc[1024];
				aEsc[0] = '\0';
				char *pDst = aEsc;
				str_escape(&pDst, It.second.m_Value.c_str(), aEsc + sizeof(aEsc));
				char aCmd[1200];
				str_format(aCmd, sizeof(aCmd), "%s \"%s\"", pVar->m_pScriptName, aEsc);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			for(const auto &It : s_StagedCols)
			{
				const SConfigVariable *pVar = It.first;
				char aCmd[256];
				str_format(aCmd, sizeof(aCmd), "%s %u", pVar->m_pScriptName, It.second.m_Value);
				Console()->ExecuteLine(aCmd, IConsole::CLIENT_ID_UNSPECIFIED);
			}
			ClearStagedAndCaches();
		}
		const bool ClearClicked = DoButton_Menu(&s_ClearBtn, Localize("Clear Changes"), DisabledStyle, &ClearBtn);
		if(ChangesCount > 0 && ClearClicked)
		{
			ClearStagedAndCaches();
		}

		char aBuf[64];
		str_format(aBuf, sizeof(aBuf), Localize("Changes: %d"), (int)ChangesCount);
		Ui()->DoLabel(&Counter, aBuf, FontSize, TEXTALIGN_ML);

		CUIRect RightRow = RightHalf;
		RightRow.h = LineSize;
		RightRow.y = ApplyBar.y + (ApplyBar.h - LineSize) / 2.0f;
		const float RightInset = 24.0f;
		RightRow.VSplitLeft(RightInset, nullptr, &RightRow);
		CUIRect TopCol1, TopCol2;
		RightRow.VSplitMid(&TopCol1, &TopCol2, 0.0f);
		if(DoButton_CheckBox(&g_Config.m_TcUiShowTClient, Localize("TClient"), g_Config.m_TcUiShowTClient, &TopCol1))
			g_Config.m_TcUiShowTClient ^= 1;
		if(DoButton_CheckBox(&g_Config.m_TcUiCompactList, Localize("Compact List"), g_Config.m_TcUiCompactList, &TopCol2))
			g_Config.m_TcUiCompactList ^= 1;
	}

	const float SearchLabelW = 60.0f;
	{
		CUIRect SearchRow = TopBar;
		SearchRow.h = LineSize;
		SearchRow.y = TopBar.y + (TopBar.h - LineSize) / 2.0f;

		CUIRect LeftHalf, RightHalf;
		SearchRow.VSplitMid(&LeftHalf, &RightHalf, 0.0f);

		CUIRect SearchLabel, SearchEdit;
		LeftHalf.VSplitLeft(SearchLabelW, &SearchLabel, &SearchEdit);
		Ui()->DoLabel(&SearchLabel, Localize("Search"), FontSize, TEXTALIGN_ML);
		Ui()->DoClearableEditBox(&s_SearchInput, &SearchEdit, EditBoxFontSize);

		CUIRect RightCol1, RightCol2;
		const float RightInset2 = 24.0f;
		RightHalf.VSplitLeft(RightInset2, nullptr, &RightHalf);
		RightHalf.VSplitMid(&RightCol1, &RightCol2, 0.0f);
		if(DoButton_CheckBox(&g_Config.m_TcUiShowDDNet, Localize("DDNet"), g_Config.m_TcUiShowDDNet, &RightCol1))
			g_Config.m_TcUiShowDDNet ^= 1;
		if(DoButton_CheckBox(&g_Config.m_TcUiOnlyModified, Localize("Only modified"), g_Config.m_TcUiOnlyModified, &RightCol2))
			g_Config.m_TcUiOnlyModified ^= 1;
	}

	const int FlagMask = CFGFLAG_CLIENT;

	struct SEntry
	{
		const SConfigVariable *m_pVar;
	};
	std::vector<SEntry> vEntries;
	vEntries.reserve(256);

	auto Collector = [](const SConfigVariable *pVar, void *pUserData) {
		auto *pVec = static_cast<std::vector<SEntry> *>(pUserData);
		pVec->push_back({pVar});
	};
	ConfigManager()->PossibleConfigVariables("", FlagMask, Collector, &vEntries);

	auto DomainEnabled = [&](ConfigDomain Domain) {
		if(Domain == ConfigDomain::DDNET)
			return g_Config.m_TcUiShowDDNet != 0;
		if(Domain == ConfigDomain::TCLIENT)
			return g_Config.m_TcUiShowTClient != 0;
		// only show DDNet and TClient domains
		return false;
	};

	const char *pSearch = s_SearchInput.GetString();

	auto IsEffectiveDefaultVar = [&](const SConfigVariable *p) -> bool {
		if(p->m_Type == SConfigVariable::VAR_INT)
		{
			const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(p);
			auto It = s_StagedInts.find(p);
			int Value = It != s_StagedInts.end() ? It->second.m_Value : *pInt->m_pVariable;
			return Value == pInt->m_Default;
		}
		if(p->m_Type == SConfigVariable::VAR_STRING)
		{
			const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(p);
			auto It = s_StagedStrs.find(p);
			const char *pValue = It != s_StagedStrs.end() ? It->second.m_Value.c_str() : pStr->m_pStr;
			return str_comp(pValue, pStr->m_pDefault) == 0;
		}
		if(p->m_Type == SConfigVariable::VAR_COLOR)
		{
			const SColorConfigVariable *pColor = static_cast<const SColorConfigVariable *>(p);
			auto It = s_StagedCols.find(p);
			unsigned Value = It != s_StagedCols.end() ? It->second.m_Value : *pColor->m_pVariable;
			return Value == pColor->m_Default;
		}
		return true;
	};

	std::vector<const SConfigVariable *> vpFiltered;
	vpFiltered.reserve(vEntries.size());
	for(const auto &E : vEntries)
	{
		const SConfigVariable *pVar = E.m_pVar;
		if(!DomainEnabled(pVar->m_ConfigDomain))
			continue;
		if(g_Config.m_TcUiOnlyModified && IsEffectiveDefaultVar(pVar))
			continue;
		if(pSearch && pSearch[0])
		{
			const char *pName = pVar->m_pScriptName ? pVar->m_pScriptName : "";
			const char *pHelp = pVar->m_pHelp ? pVar->m_pHelp : "";
			if(!str_find_nocase(pName, pSearch) && !str_find_nocase(pHelp, pSearch))
				continue;
		}
		vpFiltered.push_back(pVar);
	}

	std::sort(vpFiltered.begin(), vpFiltered.end(), [](const SConfigVariable *a, const SConfigVariable *b) {
		if(a->m_ConfigDomain != b->m_ConfigDomain)
			return a->m_ConfigDomain < b->m_ConfigDomain;
		return str_comp(a->m_pScriptName, b->m_pScriptName) < 0;
	});

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	s_ScrollRegion.Begin(&ListArea, &ScrollOffset, &ScrollParams);

	ListArea.y += ScrollOffset.y;
	ListArea.VSplitRight(5.0f, &ListArea, nullptr);
	CUIRect Content = ListArea;

	auto DomainName = [](ConfigDomain D) {
		switch(D)
		{
		case ConfigDomain::DDNET: return "DDNet";
		case ConfigDomain::TCLIENT: return "TClient";
		default: return "Other";
		}
	};

	ConfigDomain CurrentDomain = ConfigDomain::NUM;
	for(const SConfigVariable *pVar : vpFiltered)
	{
		if(pVar->m_ConfigDomain != CurrentDomain)
		{
			CurrentDomain = pVar->m_ConfigDomain;
			CUIRect Header;
			Content.HSplitTop(HeadlineHeight, &Header, &Content);
			if(s_ScrollRegion.AddRect(Header))
				Ui()->DoLabel(&Header, DomainName(CurrentDomain), HeadlineFontSize, TEXTALIGN_ML);
			Content.HSplitTop(MarginSmall, nullptr, &Content);
		}

		CUIRect RowItem;
		const float RowHeight = g_Config.m_TcUiCompactList ? (std::max(LineSize, ColorPickerLineSize) + 5.0f) : 55.0f;
		Content.HSplitTop(RowHeight, &RowItem, &Content);
		Content.HSplitTop(MarginExtraSmall, nullptr, &Content);
		const bool Visible = s_ScrollRegion.AddRect(RowItem);
		if(!Visible)
			continue;

		const bool Modified = !IsEffectiveDefaultVar(pVar);
		const ColorRGBA BgModified = ColorRGBA(1.0f, 0.8f, 0.0f, 0.15f);
		const ColorRGBA BgNormal = ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f);
		RowItem.Draw(Modified ? BgModified : BgNormal, IGraphics::CORNER_ALL, 6.0f);

		CUIRect RowContent;
		RowItem.Margin(5.0f, &RowContent);

		CUIRect TopLine, Below;
		if(g_Config.m_TcUiCompactList)
		{
			const float UsedHeight = (pVar->m_Type == SConfigVariable::VAR_COLOR) ? ColorPickerLineSize : LineSize;
			TopLine = RowContent;
			TopLine.h = UsedHeight;
			TopLine.y = round_to_int(RowContent.y + (RowContent.h - UsedHeight) / 2.0f);
			Below = RowContent;
		}
		else
		{
			RowContent.HSplitTop(LineSize, &TopLine, &Below);
		}
		CUIRect NameLine, Right;
		TopLine.VSplitRight(320.0f, &NameLine, &Right);
		NameLine.VSplitLeft(10.0f, nullptr, &NameLine);

		Ui()->DoLabel(&NameLine, pVar->m_pScriptName, FontSize, TEXTALIGN_ML);

		CUIRect Controls, ResetRect;
		Right.VSplitRight(120.0f, &Controls, &ResetRect);
		Controls.h = LineSize;
		Controls.y = TopLine.y + (TopLine.h - LineSize) / 2.0f;
		ResetRect.h = LineSize;
		ResetRect.y = Controls.y;
		Controls.VSplitRight(MarginSmall, &Controls, nullptr);

		if(!g_Config.m_TcUiCompactList)
		{
			CUIRect Help;
			Below.HSplitTop(2.0f, nullptr, &Below);
			Help = Below;
			Help.VSplitLeft(10.0f, nullptr, &Help);
			Ui()->DoLabel(&Help, pVar->m_pHelp ? pVar->m_pHelp : "", 11.0f, TEXTALIGN_ML);
		}

		static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ResetBtns;
		if(Modified && pVar->m_Type != SConfigVariable::VAR_COLOR)
		{
			CButtonContainer &ResetBtn = s_ResetBtns[pVar];
			if(DoButton_Menu(&ResetBtn, Localize("Reset"), 0, &ResetRect))
			{
				if(pVar->m_Type == SConfigVariable::VAR_INT)
				{
					const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
					s_StagedInts[pVar] = {pInt->m_Default};
				}
				else if(pVar->m_Type == SConfigVariable::VAR_STRING)
				{
					const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
					s_StagedStrs[pVar] = {std::string(pStr->m_pDefault)};
				}
			}
		}

		if(pVar->m_Type == SConfigVariable::VAR_INT)
		{
			const SIntConfigVariable *pInt = static_cast<const SIntConfigVariable *>(pVar);
			// treat 0 1 ints as checkboxes
			if(pInt->m_Min == 0 && pInt->m_Max == 1)
			{
				const int Effective = s_StagedInts.contains(pVar) ? s_StagedInts[pVar].m_Value : *pInt->m_pVariable;
				if(DoButton_CheckBox(pVar, "", Effective, &Controls))
				{
					const int NewVal = Effective ? 0 : 1;
					if(NewVal == *pInt->m_pVariable)
						s_StagedInts.erase(pVar);
					else
						s_StagedInts[pVar] = {NewVal};
				}
			}
			else
			{
				SIntState &State = s_IntInputs[pVar];
				const int Effective = s_StagedInts.contains(pVar) ? s_StagedInts[pVar].m_Value : *pInt->m_pVariable;
				if(!State.m_Inited)
				{
					State.m_Input.SetInteger(Effective);
					State.m_LastValue = Effective;
					State.m_Inited = true;
				}
				else if(!State.m_Input.IsActive() && State.m_LastValue != Effective)
				{
					State.m_Input.SetInteger(Effective);
					State.m_LastValue = Effective;
				}

				CUIRect InputBox, Dummy;
				Controls.VSplitLeft(60.0f, &InputBox, &Dummy);

				if(Ui()->DoEditBox(&State.m_Input, &InputBox, EditBoxFontSize))
				{
					int NewVal = State.m_Input.GetInteger();
					bool InRange = true;
					if(pInt->m_Min != pInt->m_Max)
					{
						if(NewVal < pInt->m_Min)
							InRange = false;
						if(pInt->m_Max != 0 && NewVal > pInt->m_Max)
							InRange = false;
					}
					if(InRange && NewVal != State.m_LastValue)
					{
						if(NewVal == *pInt->m_pVariable)
							s_StagedInts.erase(pVar);
						else
							s_StagedInts[pVar] = {NewVal};
						State.m_LastValue = NewVal;
					}
				}
			}
		}
		else if(pVar->m_Type == SConfigVariable::VAR_STRING)
		{
			const SStringConfigVariable *pStr = static_cast<const SStringConfigVariable *>(pVar);
			SStrState &State = s_StrInputs[pVar];
			const char *Effective = s_StagedStrs.contains(pVar) ? s_StagedStrs[pVar].m_Value.c_str() : pStr->m_pStr;
			if(!State.m_Inited)
			{
				State.m_Input.Set(Effective);
				State.m_Inited = true;
			}
			else if(!State.m_Input.IsActive())
			{
				if(str_comp(State.m_Input.GetString(), Effective) != 0)
					State.m_Input.Set(Effective);
			}

			if(Ui()->DoEditBox(&State.m_Input, &Controls, EditBoxFontSize))
			{
				const char *NewVal = State.m_Input.GetString();
				if(str_comp(NewVal, pStr->m_pStr) == 0)
					s_StagedStrs.erase(pVar);
				else
					s_StagedStrs[pVar] = {std::string(NewVal)};
			}
		}
		else if(pVar->m_Type == SConfigVariable::VAR_COLOR)
		{
			const SColorConfigVariable *pCol = static_cast<const SColorConfigVariable *>(pVar);
			CUIRect ColorRect;
			ColorRect.x = Controls.x;
			ColorRect.h = ColorPickerLineSize;
			ColorRect.y = TopLine.y + (TopLine.h - ColorPickerLineSize) / 2.0f;
			ColorRect.w = ColorPickerLineSize + 8.0f + 60.0f;
			const ColorRGBA DefaultColor = color_cast<ColorRGBA>(ColorHSLA(pCol->m_Default, true).UnclampLighting(pCol->m_DarkestLighting));
			static std::unordered_map<const SConfigVariable *, CButtonContainer> s_ColorResetIds;
			CButtonContainer &ResetId = s_ColorResetIds[pVar];

			SColState &ColState = s_ColInputs[pVar];
			unsigned Effective = s_StagedCols.contains(pVar) ? s_StagedCols[pVar].m_Value : *pCol->m_pVariable;
			if(!ColState.m_Inited)
			{
				ColState.m_Working = Effective;
				ColState.m_LastValue = Effective;
				ColState.m_Inited = true;
			}
			else
			{
				const bool EditingThis = Ui()->IsPopupOpen(&m_ColorPickerPopupContext) && m_ColorPickerPopupContext.m_pHslaColor == &ColState.m_Working;
				if(!EditingThis && ColState.m_Working != Effective)
				{
					ColState.m_Working = Effective;
					ColState.m_LastValue = Effective;
				}
			}

			DoLine_ColorPicker(&ResetId, ColorPickerLineSize, ColorPickerLabelSize, 0.0f, &ColorRect, "", &ColState.m_Working, DefaultColor, false, nullptr, pCol->m_Alpha);
			if(ColState.m_Working != Effective)
			{
				if(ColState.m_Working == *pCol->m_pVariable)
					s_StagedCols.erase(pVar);
				else
					s_StagedCols[pVar] = {ColState.m_Working};
				ColState.m_LastValue = ColState.m_Working;
			}
		}
	}

	CUIRect EndPad{Content.x, Content.y, Content.w, 5.0f};
	s_ScrollRegion.AddRect(EndPad);
	s_ScrollRegion.End();
}

void CMenus::RenderSettingsTClientMa(CUIRect MainView)
{
	static int s_CurMaTab = 0;

	CUIRect TabBar, Button;
	MainView.HSplitTop(LineSize, &TabBar, &MainView);
	MainView.HSplitTop(Margin, nullptr, &MainView);

	const int TabCount = NUMBER_OF_MA_TABS;
	const float TabWidth = TabBar.w / TabCount;
	static CButtonContainer s_aPageTabs[NUMBER_OF_MA_TABS] = {};
	const char *apTabNames[] = {
		TCLocalize("Settings"),
		"Nicks Names",
		TCLocalize("Visual"),
		TCLocalize("GIF"),
		TCLocalize("Lluvia"),
		TCLocalize("Anime Love"),
		TCLocalize("Keystroke HUD"),
		TCLocalize("Editor skins")};

	for(int Tab = 0; Tab < NUMBER_OF_MA_TABS; ++Tab)
	{
		TabBar.VSplitLeft(TabWidth, &Button, &TabBar);
		const int Corners = Tab == 0 ? IGraphics::CORNER_L : Tab == NUMBER_OF_MA_TABS - 1 ? IGraphics::CORNER_R :
													 IGraphics::CORNER_NONE;
		if(DoButton_MenuTab(&s_aPageTabs[Tab], apTabNames[Tab], s_CurMaTab == Tab, &Button, Corners, nullptr, nullptr, nullptr, nullptr, 4.0f))
			s_CurMaTab = Tab;
	}

	CUIRect ContentFrame = MainView;
	ContentFrame.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.50f), IGraphics::CORNER_ALL, 8.0f);
	CUIRect ContentInner = MainView;
	ContentInner.Margin(2.0f, &ContentInner);
	ContentInner.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.25f), IGraphics::CORNER_ALL, 6.0f);
	MainView.Margin(8.0f, &MainView);

	if(s_CurMaTab == MA_TAB_CONFIGURACION)
		RenderMaConfiguracion(MainView);
	if(s_CurMaTab == MA_TAB_NICK_NAMES)
		RenderMaNickNames(MainView);
	if(s_CurMaTab == MA_TAB_VISUAL)
		RenderMaVisual(MainView);
	if(s_CurMaTab == MA_TAB_GIF)
		RenderMaGif(MainView);
	if(s_CurMaTab == MA_TAB_LLUVIA)
		RenderMaLluvia(MainView);
	if(s_CurMaTab == MA_TAB_ANIMELOVE)
		RenderMaAnimeLove(MainView);
	if(s_CurMaTab == MA_TAB_KEYSTROKE)
		RenderMaKeystroke(MainView);
	if(s_CurMaTab == MA_TAB_EDITOR_SKINS)
		RenderMaEditorSkins(MainView);
}

void CMenus::RenderMaGif(CUIRect MainView)
{
	CCherryGifs &CherryGifs = GameClient()->m_CherryGifs;
	CGifWheel &GifWheel = GameClient()->m_GifWheel;
	GifWheel.EnsureThumbnails();

	CUIRect LeftView, RightView, Panel, Label, Row, Button;
	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	auto BeginPanel = [&](CUIRect &Column, float Height, const char *pTitle, CUIRect &Content) {
		Column.HSplitTop(Margin, nullptr, &Column);
		Column.HSplitTop(Height, &Panel, &Column);
		Panel.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		Panel.Margin(2.0f, &Content);
		Content.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
		Content.Margin(8.0f, &Content);
		Content.HSplitTop(HeadlineHeight, &Label, &Content);
		Ui()->DoLabel(&Label, TCLocalize(pTitle), HeadlineFontSize, TEXTALIGN_ML);
		Content.HSplitTop(MarginSmall, nullptr, &Content);
	};

	CUIRect ChatMedia;
	BeginPanel(LeftView, 215.0f, "Chat Media / GIFs", ChatMedia);
	CChat &Chat = GameClient()->m_Chat;
	if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaChatMediaPreview, TCLocalize("Render media previews from chat links"), &g_Config.m_MaChatMediaPreview, &ChatMedia, LineSize))
		Chat.RebuildChat();
	if(g_Config.m_MaChatMediaPreview)
	{
		if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaChatMediaPhotos, TCLocalize("Show photos in chat media"), &g_Config.m_MaChatMediaPhotos, &ChatMedia, LineSize))
			Chat.RebuildChat();
		if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaChatMediaGifs, TCLocalize("Show GIFs in chat media"), &g_Config.m_MaChatMediaGifs, &ChatMedia, LineSize))
			Chat.RebuildChat();
		if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaChatMediaContentFilter, TCLocalize("Content filtering"), &g_Config.m_MaChatMediaContentFilter, &ChatMedia, LineSize))
			Chat.RebuildChat();

		if(g_Config.m_MaChatMediaContentFilter)
		{
			ChatMedia.HSplitTop(LineSize, &Row, &ChatMedia);
			Ui()->DoLabel(&Row, TCLocalize("Allowed media domains"), 12.0f, TEXTALIGN_ML);
			ChatMedia.HSplitTop(LineSize, &Row, &ChatMedia);
			static CLineInput s_ChatMediaAllowedDomains(g_Config.m_MaChatMediaAllowedDomains, sizeof(g_Config.m_MaChatMediaAllowedDomains));
			s_ChatMediaAllowedDomains.SetEmptyText("tenor.com; imgur.com; giphy.com; gifs.teeworlds.xyz");
			if(Ui()->DoClearableEditBox(&s_ChatMediaAllowedDomains, &Row, 14.0f))
				Chat.RebuildChat();
			GameClient()->m_Tooltips.DoToolTip(&s_ChatMediaAllowedDomains, &Row, TCLocalize("Semicolon-separated allowlist, for example: tenor.com; imgur.com; giphy.com; gifs.teeworlds.xyz; cdn.discordapp.com"));
		}

		ChatMedia.HSplitTop(LineSize, &Row, &ChatMedia);
		if(Ui()->DoScrollbarOption(&g_Config.m_MaChatMediaPreviewMaxWidth, &g_Config.m_MaChatMediaPreviewMaxWidth, &Row, TCLocalize("Media preview width"), 120, 400))
			Chat.RebuildChat();
	}

	CUIRect CherryPanel;
	BeginPanel(LeftView, 560.0f, "Buscador GIF", CherryPanel);
	static CLineInputBuffered<64> s_SearchInput;
	CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
	Ui()->DoEditBox_Search(&s_SearchInput, &Row, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	if(g_Config.m_MaGifApiSource != 0 && g_Config.m_MaGifApiSource != 1)
		g_Config.m_MaGifApiSource = 1;

	CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
	CUIRect SourceLocal, SourceGiphy;
	Row.VSplitMid(&SourceLocal, &SourceGiphy, MarginSmall);
	static CButtonContainer s_GifSourceLocal;
	static CButtonContainer s_GifSourceGiphy;
	if(DoButton_Menu(&s_GifSourceLocal, TCLocalize("Local"), g_Config.m_MaGifApiSource == 0, &SourceLocal, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
	{
		g_Config.m_MaGifApiSource = 0;
		CherryGifs.ReloadLocalDatabase();
	}
	if(DoButton_Menu(&s_GifSourceGiphy, "GIPHY", g_Config.m_MaGifApiSource == 1, &SourceGiphy, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
	{
		g_Config.m_MaGifApiSource = 1;
		CherryGifs.ReloadLocalDatabase();
	}
	static int s_GifBrowserSection = 0;
	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
	CUIRect SectionAll, SectionFavorites, SectionWheel, SectionRest;
	const float SectionWidth = (Row.w - MarginSmall * 2.0f) / 3.0f;
	Row.VSplitLeft(SectionWidth, &SectionAll, &SectionRest);
	SectionRest.VSplitLeft(MarginSmall, nullptr, &SectionRest);
	SectionRest.VSplitLeft(SectionWidth, &SectionFavorites, &SectionRest);
	SectionRest.VSplitLeft(MarginSmall, nullptr, &SectionWheel);
	static CButtonContainer s_GifSectionAll;
	static CButtonContainer s_GifSectionFavorites;
	static CButtonContainer s_GifSectionWheel;
	if(DoButton_Menu(&s_GifSectionAll, TCLocalize("Todos"), s_GifBrowserSection == 0, &SectionAll, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
		s_GifBrowserSection = 0;
	if(DoButton_Menu(&s_GifSectionFavorites, TCLocalize("Favoritos"), s_GifBrowserSection == 1, &SectionFavorites, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
		s_GifBrowserSection = 1;
	if(DoButton_Menu(&s_GifSectionWheel, TCLocalize("En rueda"), s_GifBrowserSection == 2, &SectionWheel, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
		s_GifBrowserSection = 2;
	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	if(g_Config.m_MaGifApiSource == 1)
	{
		CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
		Ui()->DoLabel(&Row, "GIPHY API key", 12.0f, TEXTALIGN_ML);
		CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
		static CLineInput s_GiphyApiKey(g_Config.m_MaGiphyApiKey, sizeof(g_Config.m_MaGiphyApiKey));
		s_GiphyApiKey.SetHidden(true);
		s_GiphyApiKey.SetEmptyText("Pega tu key de GIPHY");
		Ui()->DoClearableEditBox(&s_GiphyApiKey, &Row, 14.0f);
	}
	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
	CUIRect SortToggle, NsfwToggle;
	Row.VSplitMid(&SortToggle, &NsfwToggle, MarginSmall);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaCherryGifsSortTop, TCLocalize("Sort by top"), &g_Config.m_MaCherryGifsSortTop, &SortToggle, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaCherryGifsShowNsfw, TCLocalize("Show NSFW"), &g_Config.m_MaCherryGifsShowNsfw, &NsfwToggle, LineSize);

	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	CherryPanel.HSplitTop(LineSize, &Row, &CherryPanel);
	CUIRect FolderButton, ReloadButton;
	Row.VSplitMid(&FolderButton, &ReloadButton, MarginSmall);
	static CButtonContainer s_GifDatabaseFolderButton;
	static CButtonContainer s_GifDatabaseReloadButton;
	if(DoButton_Menu(&s_GifDatabaseFolderButton, TCLocalize("Carpeta base GIF"), 0, &FolderButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
	{
		Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("ma/gifs", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ma/gifs", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButton_Menu(&s_GifDatabaseReloadButton, TCLocalize("Recargar"), 0, &ReloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
		CherryGifs.ReloadLocalDatabase();

	CherryGifs.SetFilters(s_SearchInput.GetString(), g_Config.m_MaCherryGifsSortTop != 0, g_Config.m_MaCherryGifsShowNsfw != 0);

	static bool s_DragActive = false;
	static char s_aDragGifId[32] = "";
	static char s_aDragUrl[256] = "";
	static char s_aDragCaption[128] = "";
	static IGraphics::CTextureHandle s_DragThumbnail;
	static int s_DragFromWheelIndex = -1;

	CUIRect StatusRow;
	CherryPanel.HSplitBottom(LineSize, &CherryPanel, &StatusRow);
	CherryPanel.HSplitBottom(MarginSmall, &CherryPanel, nullptr);
	CherryPanel.HSplitTop(MarginSmall, nullptr, &CherryPanel);
	CUIRect Grid = CherryPanel;
	if(g_Config.m_MaGifWheelPage < 0 || g_Config.m_MaGifWheelPage >= GIFWHEEL_MAX_PAGES)
		g_Config.m_MaGifWheelPage = 0;
	const int ActiveGifWheelPage = g_Config.m_MaGifWheelPage;
	auto GifWheelSlotCountConfigForPage = [](int Page) -> int * {
		switch(std::clamp(Page, 0, (int)GIFWHEEL_MAX_PAGES - 1))
		{
		case 0: return &g_Config.m_MaGifWheelSlotsPage1;
		case 1: return &g_Config.m_MaGifWheelSlotsPage2;
		case 2: return &g_Config.m_MaGifWheelSlotsPage3;
		case 3: return &g_Config.m_MaGifWheelSlotsPage4;
		default: return &g_Config.m_MaGifWheelSlotsPage1;
		}
	};
	auto GifWheelSlotIndexForPage = [](int Page, int LocalIndex) -> int {
		if(LocalIndex < 0 || LocalIndex >= GIFWHEEL_SLOTS_PER_PAGE)
			return -1;
		const int ClampedPage = std::clamp(Page, 0, (int)GIFWHEEL_MAX_PAGES - 1);
		if(LocalIndex < GIFWHEEL_BASE_SLOTS_PER_PAGE)
			return ClampedPage * GIFWHEEL_BASE_SLOTS_PER_PAGE + LocalIndex;
		return GIFWHEEL_LEGACY_MAX_SLOTS + ClampedPage * (GIFWHEEL_SLOTS_PER_PAGE - GIFWHEEL_BASE_SLOTS_PER_PAGE) + (LocalIndex - GIFWHEEL_BASE_SLOTS_PER_PAGE);
	};
	int *pActiveGifWheelSlotCount = GifWheelSlotCountConfigForPage(ActiveGifWheelPage);
	*pActiveGifWheelSlotCount = std::clamp(*pActiveGifWheelSlotCount, 1, (int)GIFWHEEL_SLOTS_PER_PAGE);
	const int ActiveGifWheelSlotCount = *pActiveGifWheelSlotCount;

	const bool ShowFavorites = s_GifBrowserSection == 1;
	const bool ShowWheelBrowser = s_GifBrowserSection == 2;
	const std::vector<SCherryGif> &vResults = ShowFavorites ? CherryGifs.Favorites() : CherryGifs.Results();
	std::vector<const CGifWheel::CSlot *> vWheelBrowserSlots;
	if(ShowWheelBrowser)
	{
		for(int i = 0; i < ActiveGifWheelSlotCount; ++i)
		{
			const int SlotIndex = GifWheelSlotIndexForPage(ActiveGifWheelPage, i);
			if(SlotIndex >= 0 && SlotIndex < (int)GifWheel.m_vSlots.size() && !GifWheel.m_vSlots[SlotIndex].IsEmpty())
				vWheelBrowserSlots.push_back(&GifWheel.m_vSlots[SlotIndex]);
		}
	}
	const int DisplayCount = ShowWheelBrowser ? (int)vWheelBrowserSlots.size() : (int)vResults.size();
	static CListBox s_ListBox;
	static std::vector<CButtonContainer> s_vGifFavoriteButtons;
	if((int)s_vGifFavoriteButtons.size() < DisplayCount)
		s_vGifFavoriteButtons.resize(DisplayCount);
	constexpr float CardSize = 84.0f;
	constexpr float CardMargin = 8.0f;
	const int ItemsPerRow = std::max(1, (int)(Grid.w / (CardSize + CardMargin)));

	if(!ShowFavorites && !ShowWheelBrowser && vResults.empty() && CherryGifs.IsLoading())
	{
		const int PlaceholderRows = (int)std::ceil(Grid.h / (CardSize + 20.0f)) + 1;
		CUIRect PlaceholderGrid = Grid;
		for(int PlaceholderRow = 0; PlaceholderRow < PlaceholderRows; PlaceholderRow++)
		{
			CUIRect RowRect, RowRemaining;
			PlaceholderGrid.HSplitTop(CardSize + 20.0f, &RowRect, &PlaceholderGrid);
			RowRemaining = RowRect;
			for(int Col = 0; Col < ItemsPerRow; Col++)
			{
				CUIRect CardRect;
				RowRemaining.VSplitLeft(CardSize + CardMargin, &CardRect, &RowRemaining);
				CardRect.Margin(CardMargin / 2.0f, &CardRect);
				CardRect.HSplitBottom(14.0f, &CardRect, nullptr);
				Graphics()->DrawRect(CardRect.x, CardRect.y, CardRect.w, CardRect.h, ColorRGBA(0.15f, 0.15f, 0.15f, 0.35f), IGraphics::CORNER_ALL, 6.0f);
			}
		}
	}
	else if(DisplayCount == 0)
	{
		CUIRect Help = Grid;
		Help.Margin(12.0f, &Help);
		const char *pHelpText = TCLocalize("No hay GIFs para mostrar");
		if(!ShowFavorites && !ShowWheelBrowser && CherryGifs.HasError())
			pHelpText = CherryGifs.ErrorText();
		else if(ShowFavorites)
			pHelpText = TCLocalize("Marca GIFs con la estrella para verlos aca");
		else if(ShowWheelBrowser)
			pHelpText = TCLocalize("Arrastra GIFs a la rueda para verlos aca");
		Ui()->DoLabel(&Help, pHelpText, 14.0f, TEXTALIGN_MC);
	}
	else
	{
		s_ListBox.DoStart(CardSize + 20.0f, DisplayCount, ItemsPerRow, 1, -1, &Grid, false);
		bool LastItemVisible = false;
		bool StopList = false;
		for(int i = 0; i < DisplayCount; i++)
		{
			SCherryGif WheelGif;
			const CGifWheel::CSlot *pSlot = nullptr;
			const SCherryGif *pGif = nullptr;
			if(ShowWheelBrowser)
			{
				pSlot = vWheelBrowserSlots[i];
				str_copy(WheelGif.m_aId, pSlot->m_aGifId, sizeof(WheelGif.m_aId));
				str_copy(WheelGif.m_aUrl, pSlot->m_aUrl, sizeof(WheelGif.m_aUrl));
				str_copy(WheelGif.m_aPreviewUrl, pSlot->m_aUrl, sizeof(WheelGif.m_aPreviewUrl));
				str_copy(WheelGif.m_aCaption, pSlot->m_aCaption, sizeof(WheelGif.m_aCaption));
				pGif = &WheelGif;
			}
			else
			{
				pGif = &vResults[i];
			}

			const CListboxItem Item = s_ListBox.DoNextItem(ShowWheelBrowser ? (const void *)pSlot : (const void *)pGif, false);
			if(!Item.m_Visible)
				continue;
			if(i == DisplayCount - 1)
				LastItemVisible = true;

			CUIRect CardRect = Item.m_Rect;
			CardRect.Margin(CardMargin / 2.0f, &CardRect);
			CUIRect Thumb, Caption;
			CardRect.HSplitBottom(14.0f, &Thumb, &Caption);

			IGraphics::CTextureHandle ThumbnailTexture;
			if(ShowWheelBrowser)
			{
				if(pSlot && pSlot->m_Thumbnail.IsValid())
					ThumbnailTexture = pSlot->m_Thumbnail;
			}
			else
			{
				if(ShowFavorites)
					CherryGifs.RequestFavoriteThumbnail(i);
				else
					CherryGifs.RequestThumbnail(i);
				CherryGifs.GetThumbnailTexture(*pGif, ThumbnailTexture);
			}

			if(ThumbnailTexture.IsValid())
			{
				Graphics()->WrapClamp();
				Graphics()->TextureSet(ThumbnailTexture);
				Graphics()->QuadsSetSubset(0, 0, 1, 1);
				Graphics()->QuadsBegin();
				Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
				IGraphics::CQuadItem QuadItem(Thumb.x, Thumb.y, Thumb.w, Thumb.h);
				Graphics()->QuadsDrawTL(&QuadItem, 1);
				Graphics()->QuadsEnd();
				Graphics()->WrapNormal();
			}
			else
			{
				Graphics()->DrawRect(Thumb.x, Thumb.y, Thumb.w, Thumb.h, ColorRGBA(0.15f, 0.15f, 0.15f, 0.5f), IGraphics::CORNER_ALL, 6.0f);
			}

			CUIRect StarButton{Thumb.x + Thumb.w - 17.0f, Thumb.y + 2.0f, 15.0f, 15.0f};
			const bool IsFavorite = CherryGifs.IsFavorite(pGif->m_aUrl);
			const bool StarHovered = Ui()->MouseInside(&StarButton);
			Graphics()->DrawRect(StarButton.x - 1.5f, StarButton.y - 1.5f, StarButton.w + 3.0f, StarButton.h + 3.0f, ColorRGBA(0.0f, 0.0f, 0.0f, StarHovered ? 0.48f : 0.30f), IGraphics::CORNER_ALL, 5.0f);
			if(Ui()->DoButtonLogic(&s_vGifFavoriteButtons[i], IsFavorite ? 1 : 0, &StarButton, BUTTONFLAG_LEFT))
			{
				CherryGifs.ToggleFavorite(*pGif);
				if(ShowFavorites)
					StopList = true;
			}
			TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
			TextRender()->TextColor(IsFavorite ? ColorRGBA(1.0f, 0.76f, 0.12f, 1.0f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f));
			Ui()->DoLabel(&StarButton, FontIcon::STAR, 11.0f, TEXTALIGN_MC);
			TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			if(StopList)
				break;

			char aCaptionText[64];
			if(pGif->m_aCaption[0] != '\0')
				str_copy(aCaptionText, pGif->m_aCaption, sizeof(aCaptionText));
			else if(!pGif->m_vTags.empty())
				str_copy(aCaptionText, pGif->m_vTags.front().c_str(), sizeof(aCaptionText));
			else
				str_format(aCaptionText, sizeof(aCaptionText), "%d %s", pGif->m_Likes, TCLocalize("likes"));
			Ui()->DoLabel(&Caption, aCaptionText, 9.0f, TEXTALIGN_MC);

			if(!s_DragActive && Ui()->MouseButton(0) && !Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&Thumb) && !Ui()->MouseInside(&StarButton))
			{
				s_DragActive = true;
				str_copy(s_aDragGifId, pGif->m_aId, sizeof(s_aDragGifId));
				str_copy(s_aDragUrl, pGif->m_aUrl, sizeof(s_aDragUrl));
				str_copy(s_aDragCaption, pGif->m_aCaption, sizeof(s_aDragCaption));
				s_DragThumbnail = ThumbnailTexture;
			}
			if(StopList)
				break;
		}
		s_ListBox.DoEnd();

		if(!ShowFavorites && !ShowWheelBrowser && LastItemVisible && CherryGifs.HasMore() && !CherryGifs.IsLoading())
			CherryGifs.LoadMore();
	}

	if(!ShowFavorites && !ShowWheelBrowser && CherryGifs.HasError())
	{
		TextRender()->TextColor(ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		Ui()->DoLabel(&StatusRow, CherryGifs.ErrorText(), 12.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else if(!ShowFavorites && !ShowWheelBrowser && CherryGifs.IsLoading() && !vResults.empty())
	{
		Ui()->DoLabel(&StatusRow, TCLocalize("Loading..."), 12.0f, TEXTALIGN_ML);
	}
	else if(ShowFavorites)
	{
		char aStatus[96];
		str_format(aStatus, sizeof(aStatus), "%d %s", DisplayCount, TCLocalize("favoritos"));
		Ui()->DoLabel(&StatusRow, aStatus, 12.0f, TEXTALIGN_ML);
	}
	else if(ShowWheelBrowser)
	{
		char aStatus[96];
		str_format(aStatus, sizeof(aStatus), "%d %s", DisplayCount, TCLocalize("GIFs en la rueda"));
		Ui()->DoLabel(&StatusRow, aStatus, 12.0f, TEXTALIGN_ML);
	}
	else if(vResults.empty())
	{
		Ui()->DoLabel(&StatusRow, TCLocalize("Search gifs to add them to the wheel"), 12.0f, TEXTALIGN_ML);
	}
	CUIRect WheelPanel;
	BeginPanel(RightView, 475.0f, "Gif Wheel", WheelPanel);
	WheelPanel.HSplitTop(LineSize, &Row, &WheelPanel);
	Ui()->DoScrollbarOption(&g_Config.m_MaGifWheelScale, &g_Config.m_MaGifWheelScale, &Row, TCLocalize("Wheel scale"), 50, 200, &CUi::ms_LinearScrollbarScale, 0, "%");
	WheelPanel.HSplitTop(MarginSmall, nullptr, &WheelPanel);
	WheelPanel.HSplitTop(LineSize, &Row, &WheelPanel);
	CUIRect PageLabel, PageButtons;
	Row.VSplitLeft(94.0f, &PageLabel, &PageButtons);
	Ui()->DoLabel(&PageLabel, TCLocalize("Apartado"), 14.0f, TEXTALIGN_ML);
	static CButtonContainer s_aGifWheelPageButtons[GIFWHEEL_MAX_PAGES];
	for(int Page = 0; Page < GIFWHEEL_MAX_PAGES; ++Page)
	{
		CUIRect PageButton;
		const float ButtonW = PageButtons.w / (float)(GIFWHEEL_MAX_PAGES - Page);
		PageButtons.VSplitLeft(ButtonW, &PageButton, &PageButtons);
		char aPageLabel[16];
		str_format(aPageLabel, sizeof(aPageLabel), "%d", Page + 1);
		const int Corners = Page == 0 ? IGraphics::CORNER_L : (Page == GIFWHEEL_MAX_PAGES - 1 ? IGraphics::CORNER_R : IGraphics::CORNER_NONE);
		if(DoButton_Menu(&s_aGifWheelPageButtons[Page], aPageLabel, g_Config.m_MaGifWheelPage == Page, &PageButton, BUTTONFLAG_LEFT, nullptr, Corners))
			g_Config.m_MaGifWheelPage = Page;
	}
	int *pWheelPanelSlotCount = GifWheelSlotCountConfigForPage(g_Config.m_MaGifWheelPage);
	*pWheelPanelSlotCount = std::clamp(*pWheelPanelSlotCount, 1, (int)GIFWHEEL_SLOTS_PER_PAGE);
	WheelPanel.HSplitTop(MarginSmall, nullptr, &WheelPanel);
	WheelPanel.HSplitTop(LineSize, &Row, &WheelPanel);
	Ui()->DoScrollbarOption(pWheelPanelSlotCount, pWheelPanelSlotCount, &Row, TCLocalize("GIFs en este apartado"), 1, GIFWHEEL_SLOTS_PER_PAGE);
	const int WheelPanelSlotCount = *pWheelPanelSlotCount;
	auto WheelPanelSlotIndex = [&](int Index) -> int { return GifWheelSlotIndexForPage(g_Config.m_MaGifWheelPage, Index); };
	WheelPanel.HSplitTop(MarginSmall, nullptr, &WheelPanel);
	CUIRect WheelControls;
	WheelPanel.HSplitBottom(LineSize * 4.0f + MarginSmall * 3.0f, &WheelPanel, &WheelControls);
	const float Radius = std::min(WheelPanel.w, WheelPanel.h) / 2.0f;
	const vec2 Center = WheelPanel.Center();

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.3f);
	Graphics()->DrawCircle(Center.x, Center.y, Radius, 64);
	Graphics()->QuadsEnd();

	static int s_SelectedSlotIndex = -1;
	const int SegmentCount = WheelPanelSlotCount;
	const float MouseDist = distance(Center, Ui()->MousePos());
	auto SlotIndexAt = [&](vec2 Pos) -> int {
		const float Dist = distance(Center, Pos);
		if(Dist >= Radius || Dist <= Radius * 0.20f)
			return -1;
		const float SegmentAngle = 2.0f * pi / SegmentCount;
		float HoveringAngle = angle(Pos - Center) + SegmentAngle / 2.0f;
		if(HoveringAngle < 0.0f)
			HoveringAngle += 2.0f * pi;
		return std::clamp((int)(HoveringAngle / (2.0f * pi) * SegmentCount), 0, SegmentCount - 1);
	};
	auto SlotHasGif = [&](int Index) -> bool {
		const int SlotIndex = WheelPanelSlotIndex(Index);
		return Index >= 0 && Index < SegmentCount && SlotIndex >= 0 && SlotIndex < (int)GifWheel.m_vSlots.size() && !GifWheel.m_vSlots[SlotIndex].IsEmpty();
	};
	const int HoveringIndex = SlotIndexAt(Ui()->MousePos());

	if(!s_DragActive)
	{
		if(HoveringIndex >= 0)
		{
			if(Ui()->MouseButtonClicked(0))
				s_SelectedSlotIndex = HoveringIndex;
			else if(Ui()->MouseButtonClicked(1) && SlotHasGif(HoveringIndex))
			{
				GifWheel.RemoveSlot(WheelPanelSlotIndex(HoveringIndex));
				s_SelectedSlotIndex = -1;
			}
		}
		else if(MouseDist < Radius && Ui()->MouseButtonClicked(0))
		{
			s_SelectedSlotIndex = -1;
		}
	}
	if(s_SelectedSlotIndex >= SegmentCount)
		s_SelectedSlotIndex = -1;
	const bool SelectedSlotHasGif = SlotHasGif(s_SelectedSlotIndex);
	bool HasAnyWheelGif = false;
	for(int i = 0; i < SegmentCount; ++i)
		HasAnyWheelGif = HasAnyWheelGif || SlotHasGif(i);

	const float SlotDensityScale = SegmentCount > GIFWHEEL_BASE_SLOTS_PER_PAGE ? std::clamp(std::sqrt((float)GIFWHEEL_BASE_SLOTS_PER_PAGE / (float)SegmentCount), 0.52f, 1.0f) : 1.0f;
	const float SlotBaseSize = 46.0f * SlotDensityScale;
	const float SlotHoverSize = 56.0f * std::clamp(SlotDensityScale + 0.08f, 0.52f, 1.0f);
	const float SlotSelectedSize = 62.0f * std::clamp(SlotDensityScale + 0.12f, 0.52f, 1.0f);
	const float Theta = pi * 2.0f / std::max(1, SegmentCount);
	for(int i = 0; i < SegmentCount; i++)
	{
		const bool HasSlot = SlotHasGif(i);
		const int SlotIndex = WheelPanelSlotIndex(i);
		const CGifWheel::CSlot *pSlot = HasSlot && SlotIndex >= 0 ? &GifWheel.m_vSlots[SlotIndex] : nullptr;
		const float Angle = Theta * i;
		const vec2 Pos = Center + direction(Angle) * (Radius * 0.72f);
		const float Size = i == s_SelectedSlotIndex ? SlotSelectedSize : (i == HoveringIndex ? SlotHoverSize : SlotBaseSize);
		CUIRect SlotRect{Pos.x - Size / 2.0f, Pos.y - Size / 2.0f, Size, Size};

		if(i == s_SelectedSlotIndex)
			Graphics()->DrawRect(SlotRect.x - 4.0f, SlotRect.y - 4.0f, SlotRect.w + 8.0f, SlotRect.h + 8.0f, ColorRGBA(0.5f, 1.0f, 0.75f, 0.9f), IGraphics::CORNER_ALL, (Size + 8.0f) * 0.15f);
		else if(s_DragActive && i == HoveringIndex)
			Graphics()->DrawRect(SlotRect.x - 5.0f, SlotRect.y - 5.0f, SlotRect.w + 10.0f, SlotRect.h + 10.0f, ColorRGBA(1.0f, 0.35f, 0.35f, 0.9f), IGraphics::CORNER_ALL, (Size + 10.0f) * 0.15f);

		if(pSlot && pSlot->m_Thumbnail.IsValid())
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(pSlot->m_Thumbnail);
			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
			IGraphics::CQuadItem QuadItem(SlotRect.x, SlotRect.y, SlotRect.w, SlotRect.h);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
		else
		{
			Graphics()->DrawRect(SlotRect.x, SlotRect.y, SlotRect.w, SlotRect.h, ColorRGBA(0.12f, 0.12f, 0.12f, HasSlot ? 0.65f : 0.28f), IGraphics::CORNER_ALL, Size * 0.15f);
			Graphics()->DrawRect(SlotRect.x + 3.0f, SlotRect.y + 3.0f, SlotRect.w - 6.0f, SlotRect.h - 6.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.10f), IGraphics::CORNER_ALL, Size * 0.12f);
		}

		if(!s_DragActive && HasSlot && Ui()->MouseButton(0) && !Ui()->MouseButtonClicked(0) && Ui()->MouseInside(&SlotRect))
		{
			s_DragActive = true;
			str_copy(s_aDragGifId, pSlot->m_aGifId, sizeof(s_aDragGifId));
			str_copy(s_aDragUrl, pSlot->m_aUrl, sizeof(s_aDragUrl));
			str_copy(s_aDragCaption, pSlot->m_aCaption, sizeof(s_aDragCaption));
			s_DragThumbnail = pSlot->m_Thumbnail;
			s_DragFromWheelIndex = i;
		}
	}

	if(!HasAnyWheelGif)
	{
		CUIRect EmptyLabel{Center.x - 140.0f, Center.y - 8.0f, 280.0f, 16.0f};
		Ui()->DoLabel(&EmptyLabel, TCLocalize("Arrastra un GIF al hueco que quieras"), 12.0f, TEXTALIGN_MC);
	}

	if(s_DragActive && !Ui()->MouseButton(0))
	{
		const int DropIndex = SlotIndexAt(Ui()->MousePos());
		if(DropIndex >= 0)
		{
			GifWheel.SetSlot(WheelPanelSlotIndex(DropIndex), s_aDragGifId, s_aDragUrl, s_aDragCaption);
			if(s_DragFromWheelIndex >= 0 && s_DragFromWheelIndex != DropIndex)
				GifWheel.RemoveSlot(WheelPanelSlotIndex(s_DragFromWheelIndex));
			s_SelectedSlotIndex = DropIndex;
		}
		s_DragActive = false;
		s_DragFromWheelIndex = -1;
	}
	WheelControls.HSplitTop(LineSize, &Row, &WheelControls);
	Ui()->DoLabel(&Row, TCLocalize("Drag a gif onto the wheel to assign it"), 12.0f, TEXTALIGN_MC);
	WheelControls.HSplitTop(MarginSmall, nullptr, &WheelControls);
	WheelControls.HSplitTop(LineSize, &Row, &WheelControls);
	CUIRect SendButton, RemoveButton;
	Row.VSplitMid(&SendButton, &RemoveButton, MarginSmall);
	static CButtonContainer s_SendSelectedGifButton;
	static CButtonContainer s_RemoveSelectedGifButton;
	if(DoButton_Menu(&s_SendSelectedGifButton, TCLocalize("Send selected"), SelectedSlotHasGif ? 0 : -1, &SendButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && SelectedSlotHasGif)
		GifWheel.ExecuteSlot(s_SelectedSlotIndex);
	if(DoButton_Menu(&s_RemoveSelectedGifButton, TCLocalize("Remove selected"), SelectedSlotHasGif ? 0 : -1, &RemoveButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && SelectedSlotHasGif)
	{
		GifWheel.RemoveSlot(WheelPanelSlotIndex(s_SelectedSlotIndex));
		s_SelectedSlotIndex = -1;
	}
	WheelControls.HSplitTop(MarginSmall, nullptr, &WheelControls);
	WheelControls.HSplitTop(LineSize, &Row, &WheelControls);
	static CButtonContainer s_ClearGifWheelButton;
	if(DoButton_Menu(&s_ClearGifWheelButton, TCLocalize("Limpiar apartado"), HasAnyWheelGif ? 0 : -1, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && HasAnyWheelGif)
	{
		for(int i = GIFWHEEL_SLOTS_PER_PAGE - 1; i >= 0; --i)
			GifWheel.RemoveSlot(GifWheelSlotIndexForPage(g_Config.m_MaGifWheelPage, i));
		s_SelectedSlotIndex = -1;
	}

	if(s_DragActive)
	{
		constexpr float DragSize = 54.0f;
		const float DragX = Ui()->MouseX() + 12.0f;
		const float DragY = Ui()->MouseY() + 12.0f;
		if(s_DragThumbnail.IsValid())
		{
			Graphics()->WrapClamp();
			Graphics()->TextureSet(s_DragThumbnail);
			Graphics()->QuadsSetSubset(0, 0, 1, 1);
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 1.0f, 1.0f, 0.9f);
			IGraphics::CQuadItem QuadItem(DragX, DragY, DragSize, DragSize);
			Graphics()->QuadsDrawTL(&QuadItem, 1);
			Graphics()->QuadsEnd();
			Graphics()->WrapNormal();
		}
		else
		{
			Graphics()->DrawRect(DragX, DragY, DragSize, DragSize, ColorRGBA(0.2f, 0.2f, 0.2f, 0.85f), IGraphics::CORNER_ALL, 6.0f);
		}
	}

	CUIRect BubblePanel;
	BeginPanel(RightView, 220.0f, "Burbuja GIF sobre el tee", BubblePanel);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGifBubbleAboveHead, TCLocalize("Mostrar burbuja GIF sobre el tee"), &g_Config.m_MaGifBubbleAboveHead, &BubblePanel, LineSize);
	BubblePanel.HSplitTop(LineSize, &Row, &BubblePanel);
	Ui()->DoScrollbarOption(&g_Config.m_MaGifBubbleDurationMs, &g_Config.m_MaGifBubbleDurationMs, &Row, TCLocalize("Duracion"), 1000, 15000, &CUi::ms_LinearScrollbarScale, 0, "ms");
	BubblePanel.HSplitTop(LineSize, &Row, &BubblePanel);
	Ui()->DoScrollbarOption(&g_Config.m_MaGifBubbleOffsetY, &g_Config.m_MaGifBubbleOffsetY, &Row, TCLocalize("Altura sobre el tee"), 0, 300);
	BubblePanel.HSplitTop(LineSize, &Row, &BubblePanel);
	Ui()->DoLabel(&Row, TCLocalize("Dominios de burbuja"), 12.0f, TEXTALIGN_ML);
	BubblePanel.HSplitTop(LineSize, &Row, &BubblePanel);
	static CLineInput s_GifBubbleDomains(g_Config.m_MaGifBubbleDomains, sizeof(g_Config.m_MaGifBubbleDomains));
	s_GifBubbleDomains.SetEmptyText("gifs.teeworlds.xyz");
	Ui()->DoClearableEditBox(&s_GifBubbleDomains, &Row, 14.0f);

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.w = MainView.w;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}
void CMenus::RenderMaVisual(CUIRect MainView)
{
	CUIRect LeftView, RightView, Column, Button, Label, Row;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	Column = LeftView;

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Media Background"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);

	const bool MenuMediaChanged = DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMenuMediaBackground, TCLocalize("Enable to main menu"), &g_Config.m_MaMenuMediaBackground, &Column, LineSize);
	const bool GameMediaChanged = DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGameMediaBackground, TCLocalize("Enable to game background"), &g_Config.m_MaGameMediaBackground, &Column, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGameMediaBackgroundSeparate, TCLocalize("Use another background for game"), &g_Config.m_MaGameMediaBackgroundSeparate, &Column, LineSize);
	if(MenuMediaChanged || GameMediaChanged)
		m_MenuMediaBackground.ReloadFromConfig();

	struct SMenuMediaFileListContext
	{
		std::vector<std::string> *m_pLabels;
		std::vector<std::string> *m_pPaths;
	};

	auto MenuMediaFileListScan = [](const char *pName, int IsDir, int StorageType, void *pUser) {
		(void)StorageType;
		if(IsDir)
			return 0;

		auto *pContext = static_cast<SMenuMediaFileListContext *>(pUser);
		const std::string Ext = MediaDecoder::ExtractExtensionLower(pName);
		const bool SupportedImage = Ext == "png" || Ext == "jpg" || Ext == "jpeg" || Ext == "webp" || Ext == "bmp" || Ext == "avif" || Ext == "gif";
		const bool SupportedVideo = Ext == "mp4" || Ext == "webm" || Ext == "mov" || Ext == "m4v" || Ext == "mkv" || Ext == "avi";
		if(!SupportedImage && !SupportedVideo)
			return 0;

		pContext->m_pLabels->emplace_back(pName);
		pContext->m_pPaths->emplace_back(std::string("tclient/backgrounds/") + pName);
		return 0;
	};

	Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("tclient/backgrounds", IStorage::TYPE_SAVE);

	static std::vector<std::string> s_vMenuMediaFileLabels;
	static std::vector<std::string> s_vMenuMediaFilePaths;
	s_vMenuMediaFileLabels.clear();
	s_vMenuMediaFilePaths.clear();
	SMenuMediaFileListContext MenuMediaContext{&s_vMenuMediaFileLabels, &s_vMenuMediaFilePaths};
	Storage()->ListDirectory(IStorage::TYPE_ALL, "tclient/backgrounds", MenuMediaFileListScan, &MenuMediaContext);

	std::vector<int> vSortedIndices(s_vMenuMediaFileLabels.size());
	for(size_t i = 0; i < vSortedIndices.size(); ++i)
		vSortedIndices[i] = (int)i;
	std::sort(vSortedIndices.begin(), vSortedIndices.end(), [&](int Left, int Right) {
		return str_comp_nocase(s_vMenuMediaFileLabels[Left].c_str(), s_vMenuMediaFileLabels[Right].c_str()) < 0;
	});

	static std::vector<std::string> s_vMenuMediaDropDownLabels;
	static std::vector<const char *> s_vMenuMediaDropDownLabelPtrs;
	s_vMenuMediaDropDownLabels.clear();
	s_vMenuMediaDropDownLabelPtrs.clear();
	for(int SortedIndex : vSortedIndices)
		s_vMenuMediaDropDownLabels.push_back(s_vMenuMediaFileLabels[SortedIndex]);
	for(const std::string &LabelString : s_vMenuMediaDropDownLabels)
		s_vMenuMediaDropDownLabelPtrs.push_back(LabelString.c_str());

	auto FindSelectedMediaFile = [&](const char *pPath) {
		for(size_t i = 0; i < vSortedIndices.size(); ++i)
		{
			const int SortedIndex = vSortedIndices[i];
			if(str_comp(pPath, s_vMenuMediaFilePaths[SortedIndex].c_str()) == 0)
				return (int)i;
		}
		return -1;
	};

	auto RenderMediaSelector = [&](CUIRect *pColumn, const char *pLabel, char *pPath, size_t PathSize, bool ReloadMenuBackground, CUi::SDropDownState &DropDownState, CScrollRegion &DropDownScrollRegion, CButtonContainer *pEmptyButton) {
		CUIRect SelectorLabel, MediaPathRow, MediaFileDropDown, MediaReloadButton, MediaFolderButton;
		pColumn->HSplitTop(15.0f, &SelectorLabel, pColumn);
		Ui()->DoLabel(&SelectorLabel, TCLocalize(pLabel), 11.0f, TEXTALIGN_ML);

		pColumn->HSplitTop(LineSize, &MediaPathRow, pColumn);
		MediaPathRow.VSplitRight(20.0f, &MediaPathRow, &MediaFolderButton);
		MediaPathRow.VSplitRight(MarginSmall, &MediaPathRow, nullptr);
		MediaPathRow.VSplitRight(20.0f, &MediaPathRow, &MediaReloadButton);
		MediaPathRow.VSplitRight(MarginSmall, &MediaPathRow, nullptr);
		MediaFileDropDown = MediaPathRow;

		const int SelectedMediaFile = FindSelectedMediaFile(pPath);
		if(s_vMenuMediaDropDownLabelPtrs.empty())
		{
			DoButton_Menu(pEmptyButton, TCLocalize("No media files in backgrounds folder"), -1, &MediaFileDropDown);
		}
		else
		{
			DropDownState.m_SelectionPopupContext.m_pScrollRegion = &DropDownScrollRegion;
			const int NewSelectedMediaFile = Ui()->DoDropDown(&MediaFileDropDown, SelectedMediaFile, s_vMenuMediaDropDownLabelPtrs.data(), s_vMenuMediaDropDownLabelPtrs.size(), DropDownState);
			if(NewSelectedMediaFile != SelectedMediaFile && NewSelectedMediaFile >= 0 && NewSelectedMediaFile < (int)vSortedIndices.size())
			{
				const int SortedIndex = vSortedIndices[NewSelectedMediaFile];
				str_copy(pPath, s_vMenuMediaFilePaths[SortedIndex].c_str(), PathSize);
				if(ReloadMenuBackground)
					m_MenuMediaBackground.ReloadFromConfig();
			}
		}

		static CButtonContainer s_MenuMediaReloadButton;
		static CButtonContainer s_GameMediaReloadButton;
		CButtonContainer *pReloadButton = ReloadMenuBackground ? &s_MenuMediaReloadButton : &s_GameMediaReloadButton;
		if(Ui()->DoButton_FontIcon(pReloadButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &MediaReloadButton, BUTTONFLAG_LEFT))
		{
			if(ReloadMenuBackground)
				m_MenuMediaBackground.ReloadFromConfig();
			else
				GameClient()->m_Background.ReloadMediaBackground();
		}

		static CButtonContainer s_MenuMediaFolderButton;
		static CButtonContainer s_GameMediaFolderButton;
		CButtonContainer *pFolderButton = ReloadMenuBackground ? &s_MenuMediaFolderButton : &s_GameMediaFolderButton;
		if(Ui()->DoButton_FontIcon(pFolderButton, FontIcon::FOLDER, 0, &MediaFolderButton, BUTTONFLAG_LEFT))
		{
			Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
			Storage()->CreateFolder("tclient/backgrounds", IStorage::TYPE_SAVE);
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "tclient/backgrounds", aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}
	};

	Column.HSplitTop(MarginSmall, nullptr, &Column);
	static CUi::SDropDownState s_MenuMediaFileDropDownState;
	static CScrollRegion s_MenuMediaFileDropDownScrollRegion;
	static CButtonContainer s_MenuMediaEmptyButton;
	RenderMediaSelector(&Column, "Menu background", g_Config.m_MaMenuMediaBackgroundPath, sizeof(g_Config.m_MaMenuMediaBackgroundPath), true, s_MenuMediaFileDropDownState, s_MenuMediaFileDropDownScrollRegion, &s_MenuMediaEmptyButton);

	if(g_Config.m_MaGameMediaBackgroundSeparate)
	{
		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CUi::SDropDownState s_GameMediaFileDropDownState;
		static CScrollRegion s_GameMediaFileDropDownScrollRegion;
		static CButtonContainer s_GameMediaEmptyButton;
		RenderMediaSelector(&Column, "Game background", g_Config.m_MaGameMediaBackgroundPath, sizeof(g_Config.m_MaGameMediaBackgroundPath), false, s_GameMediaFileDropDownState, s_GameMediaFileDropDownScrollRegion, &s_GameMediaEmptyButton);
	}

	Column.HSplitTop(LineSize, &Row, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_MaGameMediaBackgroundOffset, &g_Config.m_MaGameMediaBackgroundOffset, &Row, TCLocalize("Map offset"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
	GameClient()->m_Tooltips.DoToolTip(&g_Config.m_MaGameMediaBackgroundOffset, &Row, TCLocalize("0 keeps the image fixed to the screen. 100 fixes it to the map for a full parallax effect."));

	Column.HSplitTop(LineSize, &Row, &Column);
	Ui()->DoScrollbarOption(&g_Config.m_MaGameMediaBackgroundOpacity, &g_Config.m_MaGameMediaBackgroundOpacity, &Row, TCLocalize("Game background opacity"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

	Column.HSplitTop(LineSize, &Row, &Column);
	if(m_MenuMediaBackground.HasError())
		TextRender()->TextColor(ColorRGBA(1.0f, 0.45f, 0.45f, 1.0f));
	else if(m_MenuMediaBackground.IsLoaded())
		TextRender()->TextColor(ColorRGBA(0.55f, 1.0f, 0.55f, 1.0f));
	Ui()->DoLabel(&Row, m_MenuMediaBackground.StatusText(), 11.0f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;
	LeftView = Column;

	// ***** Stream Chat ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Chat de stream"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	if(DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaStreamChat, TCLocalize("Activar chat de stream"), &g_Config.m_MaStreamChat, &LeftView, LineSize))
		GameClient()->m_StreamChat.RequestReconnect();

	if(g_Config.m_MaStreamChat)
	{
		static std::vector<const char *> s_StreamChatPlatformNames;
		s_StreamChatPlatformNames = {
			TCLocalize("Twitch"),
			TCLocalize("YouTube (pronto)"),
			TCLocalize("Kick (pronto)")};
		static CUi::SDropDownState s_StreamChatPlatformDropDownState;
		static CScrollRegion s_StreamChatPlatformDropDownScrollRegion;
		s_StreamChatPlatformDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_StreamChatPlatformDropDownScrollRegion;

		g_Config.m_MaStreamChatPlatform = std::clamp(g_Config.m_MaStreamChatPlatform, 1, (int)s_StreamChatPlatformNames.size());
		const int OldPlatform = g_Config.m_MaStreamChatPlatform - 1;
		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Row.VSplitLeft(120.0f, &Label, &Row);
		Ui()->DoLabel(&Label, TCLocalize("Plataforma"), FontSize, TEXTALIGN_ML);
		const int NewPlatform = Ui()->DoDropDown(&Row, OldPlatform, s_StreamChatPlatformNames.data(), s_StreamChatPlatformNames.size(), s_StreamChatPlatformDropDownState);
		if(NewPlatform != OldPlatform)
		{
			g_Config.m_MaStreamChatPlatform = NewPlatform + 1;
			GameClient()->m_StreamChat.RequestReconnect();
		}

		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Row.VSplitLeft(120.0f, &Label, &Row);
		Ui()->DoLabel(&Label, TCLocalize("Canal / URL"), FontSize, TEXTALIGN_ML);
		static CLineInput s_StreamChatChannel(g_Config.m_MaStreamChatChannel, sizeof(g_Config.m_MaStreamChatChannel));
		s_StreamChatChannel.SetEmptyText("twitch.tv/tu_canal");
		if(Ui()->DoClearableEditBox(&s_StreamChatChannel, &Row, 12.0f))
			GameClient()->m_StreamChat.RequestReconnect();

		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStreamChatMaxLines, &g_Config.m_MaStreamChatMaxLines, &Row, TCLocalize("Lineas visibles"), 1, 24);

		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStreamChatTextOpacity, &g_Config.m_MaStreamChatTextOpacity, &Row, TCLocalize("Opacidad texto"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStreamChatOpacity, &g_Config.m_MaStreamChatOpacity, &Row, TCLocalize("Opacidad fondo"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

		static CButtonContainer s_StreamChatTextColor;
		DoLine_ColorPicker(&s_StreamChatTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color texto"), &g_Config.m_MaStreamChatTextColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

		LeftView.HSplitTop(LineSize + 4.0f, &Row, &LeftView);
		Row.VSplitMid(&Button, &Row, MarginSmall);
		static CButtonContainer s_StreamChatReconnectButton;
		if(DoButton_Menu(&s_StreamChatReconnectButton, TCLocalize("Reconectar"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
			GameClient()->m_StreamChat.RequestReconnect();
		static CButtonContainer s_StreamChatHudEditorButton;
		if(DoButton_Menu(&s_StreamChatHudEditorButton, TCLocalize("Editar HUD"), 0, &Row, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
		{
			SetActive(false);
			GameClient()->m_HudEditor.Activate();
		}

		char aStreamChatStatus[128];
		GameClient()->m_StreamChat.GetStatus(aStreamChatStatus, sizeof(aStreamChatStatus));
		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		if(g_Config.m_MaStreamChatPlatform != 1)
			TextRender()->TextColor(ColorRGBA(1.0f, 0.68f, 0.35f, 1.0f));
		else
			TextRender()->TextColor(ColorRGBA(0.72f, 0.82f, 1.0f, 1.0f));
		Ui()->DoLabel(&Row, g_Config.m_MaStreamChatPlatform == 1 ? aStreamChatStatus : TCLocalize("YouTube y Kick quedan preparados para una version con API."), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
		LeftView.HSplitTop(LineSize * 2.0f, nullptr, &LeftView);

	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Stream Activity Feed ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Fuente de actividad"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaStreamActivityFeed, TCLocalize("Mostrar fuente de actividad"), &g_Config.m_MaStreamActivityFeed, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaStreamActivityStats, TCLocalize("Mostrar estadisticas"), &g_Config.m_MaStreamActivityStats, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaStreamViewerStats, TCLocalize("Mostrar espectadores reales"), &g_Config.m_MaStreamViewerStats, &LeftView, LineSize);

	static std::vector<const char *> s_StreamActivityPlatformNames;
	s_StreamActivityPlatformNames = {
		TCLocalize("Usar plataforma del chat"),
		TCLocalize("Twitch"),
		TCLocalize("YouTube"),
		TCLocalize("Kick")};
	static CUi::SDropDownState s_StreamActivityPlatformDropDownState;
	static CScrollRegion s_StreamActivityPlatformDropDownScrollRegion;
	s_StreamActivityPlatformDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_StreamActivityPlatformDropDownScrollRegion;

	g_Config.m_MaStreamActivityPlatform = std::clamp(g_Config.m_MaStreamActivityPlatform, 0, (int)s_StreamActivityPlatformNames.size() - 1);
	const int OldActivityPlatform = g_Config.m_MaStreamActivityPlatform;
	LeftView.HSplitTop(LineSize, &Row, &LeftView);
	Row.VSplitLeft(120.0f, &Label, &Row);
	Ui()->DoLabel(&Label, TCLocalize("Fuente"), FontSize, TEXTALIGN_ML);
	const int NewActivityPlatform = Ui()->DoDropDown(&Row, OldActivityPlatform, s_StreamActivityPlatformNames.data(), s_StreamActivityPlatformNames.size(), s_StreamActivityPlatformDropDownState);
	if(NewActivityPlatform != OldActivityPlatform)
		g_Config.m_MaStreamActivityPlatform = NewActivityPlatform;

	const int ActivityApiPlatform = g_Config.m_MaStreamActivityPlatform == 0 ? g_Config.m_MaStreamChatPlatform : g_Config.m_MaStreamActivityPlatform;
	if(g_Config.m_MaStreamViewerStats)
	{
		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStreamViewerStatsRefresh, &g_Config.m_MaStreamViewerStatsRefresh, &Row, TCLocalize("Actualizar espectadores cada"), 10, 300, &CUi::ms_LinearScrollbarScale, 0u, "s");

		if(ActivityApiPlatform == 1)
		{
			LeftView.HSplitTop(LineSize, &Row, &LeftView);
			Row.VSplitLeft(145.0f, &Label, &Row);
			Ui()->DoLabel(&Label, TCLocalize("Twitch Client ID"), FontSize, TEXTALIGN_ML);
			static CLineInput s_StreamStatsTwitchClientId(g_Config.m_MaStreamStatsTwitchClientId, sizeof(g_Config.m_MaStreamStatsTwitchClientId));
			s_StreamStatsTwitchClientId.SetHidden(true);
			Ui()->DoClearableEditBox(&s_StreamStatsTwitchClientId, &Row, 12.0f);

			LeftView.HSplitTop(LineSize, &Row, &LeftView);
			Row.VSplitLeft(145.0f, &Label, &Row);
			Ui()->DoLabel(&Label, TCLocalize("Twitch token"), FontSize, TEXTALIGN_ML);
			static CLineInput s_StreamStatsTwitchToken(g_Config.m_MaStreamStatsTwitchToken, sizeof(g_Config.m_MaStreamStatsTwitchToken));
			s_StreamStatsTwitchToken.SetHidden(true);
			Ui()->DoClearableEditBox(&s_StreamStatsTwitchToken, &Row, 12.0f);
		}
		else if(ActivityApiPlatform == 2)
		{
			LeftView.HSplitTop(LineSize, &Row, &LeftView);
			Row.VSplitLeft(145.0f, &Label, &Row);
			Ui()->DoLabel(&Label, TCLocalize("YouTube API key"), FontSize, TEXTALIGN_ML);
			static CLineInput s_StreamStatsYoutubeApiKey(g_Config.m_MaStreamStatsYoutubeApiKey, sizeof(g_Config.m_MaStreamStatsYoutubeApiKey));
			s_StreamStatsYoutubeApiKey.SetHidden(true);
			Ui()->DoClearableEditBox(&s_StreamStatsYoutubeApiKey, &Row, 12.0f);
		}
		else if(ActivityApiPlatform == 3)
		{
			LeftView.HSplitTop(LineSize, &Row, &LeftView);
			Row.VSplitLeft(145.0f, &Label, &Row);
			Ui()->DoLabel(&Label, TCLocalize("Kick token"), FontSize, TEXTALIGN_ML);
			static CLineInput s_StreamStatsKickToken(g_Config.m_MaStreamStatsKickToken, sizeof(g_Config.m_MaStreamStatsKickToken));
			s_StreamStatsKickToken.SetHidden(true);
			Ui()->DoClearableEditBox(&s_StreamStatsKickToken, &Row, 12.0f);

			LeftView.HSplitTop(LineSize, &Row, &LeftView);
			Row.VSplitLeft(145.0f, &Label, &Row);
			Ui()->DoLabel(&Label, TCLocalize("Kick broadcaster ID"), FontSize, TEXTALIGN_ML);
			static CLineInput s_StreamStatsKickBroadcasterId(g_Config.m_MaStreamStatsKickBroadcasterId, sizeof(g_Config.m_MaStreamStatsKickBroadcasterId));
			s_StreamStatsKickBroadcasterId.SetHidden(true);
			Ui()->DoClearableEditBox(&s_StreamStatsKickBroadcasterId, &Row, 12.0f);
		}
	}

	if(g_Config.m_MaStreamActivityFeed)
	{
		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStreamActivityMaxEvents, &g_Config.m_MaStreamActivityMaxEvents, &Row, TCLocalize("Eventos visibles"), 1, 12);
	}

	char aStreamActivitySummary[256];
	GameClient()->m_StreamChat.GetActivitySummary(aStreamActivitySummary, sizeof(aStreamActivitySummary));
	LeftView.HSplitTop(LineSize, &Row, &LeftView);
	TextRender()->TextColor(ColorRGBA(0.72f, 0.82f, 1.0f, 1.0f));
	Ui()->DoLabel(&Row, aStreamActivitySummary, 10.5f, TEXTALIGN_ML);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(g_Config.m_MaStreamActivityPlatform >= 2)
	{
		LeftView.HSplitTop(LineSize, &Row, &LeftView);
		TextRender()->TextColor(ColorRGBA(1.0f, 0.68f, 0.35f, 1.0f));
		Ui()->DoLabel(&Row, TCLocalize("YouTube y Kick requieren API/token para datos reales."), 10.5f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Team Stats ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Estadisticas de team"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaTeamStatsPanel, TCLocalize("Mostrar estadisticas de team"), &g_Config.m_MaTeamStatsPanel, &LeftView, LineSize);
	if(g_Config.m_MaTeamStatsPanel)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaTeamStatsPanelOnlyInTeam, TCLocalize("Solo al estar en team"), &g_Config.m_MaTeamStatsPanelOnlyInTeam, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaTeamStatsPanelOnlyFinish, TCLocalize("Mostrar solo al final del mapa"), &g_Config.m_MaTeamStatsPanelOnlyFinish, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaTeamStatsPanelShowSelf, TCLocalize("Mostrar mi puntuacion"), &g_Config.m_MaTeamStatsPanelShowSelf, &LeftView, LineSize);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaTeamStatsPanelMaxPlayers, &g_Config.m_MaTeamStatsPanelMaxPlayers, &Button, TCLocalize("Jugadores mostrados"), 1, 12);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaTeamStatsPanelOpacity, &g_Config.m_MaTeamStatsPanelOpacity, &Button, TCLocalize("Opacidad fondo"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaTeamStatsPanelTextOpacity, &g_Config.m_MaTeamStatsPanelTextOpacity, &Button, TCLocalize("Opacidad letras"), 0, 100, &CUi::ms_LinearScrollbarScale, 0u, "%");

		static CButtonContainer s_TeamStatsTitleColor, s_TeamStatsTextColor, s_TeamStatsHighlightColor;
		DoLine_ColorPicker(&s_TeamStatsTitleColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color titulo"), &g_Config.m_MaTeamStatsPanelTitleColor, ColorRGBA(0.58f, 0.95f, 1.0f), false);
		DoLine_ColorPicker(&s_TeamStatsTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color texto"), &g_Config.m_MaTeamStatsPanelTextColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
		DoLine_ColorPicker(&s_TeamStatsHighlightColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color destacado"), &g_Config.m_MaTeamStatsPanelHighlightColor, ColorRGBA(0.72f, 1.0f, 0.72f), false);
		LeftView.HSplitTop(LineSize + 6.0f, &Button, &LeftView);
		static CButtonContainer s_MaTeamStatsHudEditorButton;
		if(DoButton_Menu(&s_MaTeamStatsHudEditorButton, TCLocalize("Abrir editor HUD"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
		{
			SetActive(false);
			GameClient()->m_HudEditor.Activate();
		}
	}
	else
		LeftView.HSplitTop(LineSize * 2.0f, nullptr, &LeftView);

	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;
	// ***** 3D Particles ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("3D Particles"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_Ma3dParticles, TCLocalize("Enable 3D particles"), &g_Config.m_Ma3dParticles, &LeftView, LineSize);
	if(g_Config.m_Ma3dParticles)
	{
		static std::vector<const char *> s_Particles3dDropDownNames;
		s_Particles3dDropDownNames = {
			TCLocalize("Normales"),
			TCLocalize("Corazones"),
			TCLocalize("Stars"),
			TCLocalize("Diamantes"),
			TCLocalize("Lunas"),
			TCLocalize("Rayos"),
			TCLocalize("Mariposas"),
			TCLocalize("Flores"),
			TCLocalize("Notas musicales"),
			TCLocalize("Calaveras"),
			TCLocalize("Coronas"),
			TCLocalize("Llamas"),
			TCLocalize("Copos de nieve")};
		static CUi::SDropDownState s_Particles3dDropDownState;
		static CScrollRegion s_Particles3dDropDownScrollRegion;
		s_Particles3dDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_Particles3dDropDownScrollRegion;

		g_Config.m_Ma3dParticlesType = std::clamp(g_Config.m_Ma3dParticlesType, 1, (int)s_Particles3dDropDownNames.size());
		int ParticleTypeSelected = g_Config.m_Ma3dParticlesType - 1;
		CUIRect DropDownRect;
		LeftView.HSplitTop(LineSize, &DropDownRect, &LeftView);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Particle type"), FontSize, TEXTALIGN_ML);
		const int NewParticleTypeSelected = Ui()->DoDropDown(&DropDownRect, ParticleTypeSelected, s_Particles3dDropDownNames.data(), s_Particles3dDropDownNames.size(), s_Particles3dDropDownState);
		if(NewParticleTypeSelected != ParticleTypeSelected)
			g_Config.m_Ma3dParticlesType = NewParticleTypeSelected + 1;

		static CButtonContainer s_Particles3dColor;
		DoLine_ColorPicker(&s_Particles3dColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Particle color"), &g_Config.m_Ma3dParticlesColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_Ma3dParticlesCount, &g_Config.m_Ma3dParticlesCount, &Button, TCLocalize("Particle count"), 1, 200);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_Ma3dParticlesSpeed, &g_Config.m_Ma3dParticlesSpeed, &Button, TCLocalize("Speed"), 1, 500);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_Ma3dParticlesMovementSpeed, &g_Config.m_Ma3dParticlesMovementSpeed, &Button, TCLocalize("Movement speed"), 0, 2000, &CUi::ms_LinearScrollbarScale, 0, "%");
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_Ma3dParticlesMusicReaction, TCLocalize("Reaccion a la musica"), &g_Config.m_Ma3dParticlesMusicReaction, &LeftView, LineSize);
		if(g_Config.m_Ma3dParticlesMusicReaction)
		{
			LeftView.HSplitTop(LineSize, &Button, &LeftView);
			Ui()->DoScrollbarOption(&g_Config.m_Ma3dParticlesMusicReactionStrength, &g_Config.m_Ma3dParticlesMusicReactionStrength, &Button, TCLocalize("Intensidad reaccion"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		}
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_Ma3dParticlesAlpha, &g_Config.m_Ma3dParticlesAlpha, &Button, TCLocalize("Alpha"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	}
	else
		LeftView.HSplitTop(LineSize * 6.0f, nullptr, &LeftView);
	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Aspect Ratio ***** //
	RightView.HSplitTop(Margin, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Relacion de aspecto"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	g_Config.m_MaCustomAspectRatioMode = std::clamp(g_Config.m_MaCustomAspectRatioMode, 0, 2);
	g_Config.m_MaCustomAspectRatioApplyMode = std::clamp(g_Config.m_MaCustomAspectRatioApplyMode, 0, 2);
	const int AspectMode = g_Config.m_MaCustomAspectRatioMode;
	const bool IsCustomMode = AspectMode == 2;

	const auto SplitAspectRow = [](CUIRect &InRow, CUIRect &OutLabel, CUIRect &OutControl) {
		const float LabelWidth = std::clamp(InRow.w * 0.40f, 95.0f, 150.0f);
		InRow.VSplitLeft(LabelWidth, &OutLabel, &OutControl);
	};

	const char *apAspectPresetNames[] = {
		TCLocalize("Desactivado"),
		"5:4",
		"4:3",
		"3:2",
		"16:9",
		"16:10",
		"21:9",
		"32:9",
		TCLocalize("Personalizado"),
	};
	static const std::array<int, 8> s_aAspectPresetValues = {0, 125, 133, 150, 178, 160, 233, 356};
	static CUi::SDropDownState s_AspectPresetState;
	static CScrollRegion s_AspectPresetScrollRegion;
	s_AspectPresetState.m_SelectionPopupContext.m_pScrollRegion = &s_AspectPresetScrollRegion;

	auto GetAspectPresetIndex = [&]() -> int {
		const int CustomPresetIndex = (int)std::size(apAspectPresetNames) - 1;
		if(AspectMode <= 0 || g_Config.m_MaCustomAspectRatio == 0)
			return 0;
		if(AspectMode == 2)
			return CustomPresetIndex;

		for(size_t i = 1; i < s_aAspectPresetValues.size(); ++i)
		{
			if(g_Config.m_MaCustomAspectRatio == s_aAspectPresetValues[i])
				return (int)i;
		}

		int BestIndex = 1;
		int BestDiff = absolute(g_Config.m_MaCustomAspectRatio - s_aAspectPresetValues[BestIndex]);
		for(size_t i = 2; i < s_aAspectPresetValues.size(); ++i)
		{
			const int CurDiff = absolute(g_Config.m_MaCustomAspectRatio - s_aAspectPresetValues[i]);
			if(CurDiff < BestDiff)
			{
				BestDiff = CurDiff;
				BestIndex = (int)i;
			}
		}
		return BestIndex;
	};

	const int CurrentPreset = GetAspectPresetIndex();
	CUIRect PresetLabel, PresetDropDown;
	RightView.HSplitTop(LineSize, &Row, &RightView);
	SplitAspectRow(Row, PresetLabel, PresetDropDown);
	Ui()->DoLabel(&PresetLabel, TCLocalize("Preset"), FontSize, TEXTALIGN_ML);
	const int NewPreset = Ui()->DoDropDown(&PresetDropDown, CurrentPreset, apAspectPresetNames, (int)std::size(apAspectPresetNames), s_AspectPresetState);
	const int CustomPresetIndex = (int)std::size(apAspectPresetNames) - 1;
	if(NewPreset != CurrentPreset)
	{
		if(NewPreset == 0)
		{
			g_Config.m_MaCustomAspectRatioMode = 0;
			g_Config.m_MaCustomAspectRatio = 0;
		}
		else if(NewPreset == CustomPresetIndex)
		{
			g_Config.m_MaCustomAspectRatioMode = 2;
			if(g_Config.m_MaCustomAspectRatio < 100)
				g_Config.m_MaCustomAspectRatio = 178;
			if(g_Config.m_MaCustomAspectRatioNum <= 0 || g_Config.m_MaCustomAspectRatioDen <= 0)
			{
				g_Config.m_MaCustomAspectRatioNum = 16;
				g_Config.m_MaCustomAspectRatioDen = 9;
				g_Config.m_MaCustomAspectRatio = 178;
			}
		}
		else
		{
			g_Config.m_MaCustomAspectRatioMode = 1;
			g_Config.m_MaCustomAspectRatio = s_aAspectPresetValues[NewPreset];
		}
		GameClient()->m_TClient.SetForcedAspect();
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	CUIRect ApplyLabel, ApplyDropDown;
	RightView.HSplitTop(LineSize, &Row, &RightView);
	SplitAspectRow(Row, ApplyLabel, ApplyDropDown);
	Ui()->DoLabel(&ApplyLabel, TCLocalize("Aplicar"), FontSize, TEXTALIGN_ML);
	const char *apAspectApplyNames[] = {
		TCLocalize("Solo juego"),
		TCLocalize("Todo"),
		TCLocalize("Juego sin HUD"),
	};
	static CUi::SDropDownState s_AspectApplyState;
	static CScrollRegion s_AspectApplyScrollRegion;
	s_AspectApplyState.m_SelectionPopupContext.m_pScrollRegion = &s_AspectApplyScrollRegion;
	const int CurrentApplyMode = g_Config.m_MaCustomAspectRatioApplyMode;
	const int NewApplyMode = Ui()->DoDropDown(&ApplyDropDown, CurrentApplyMode, apAspectApplyNames, (int)std::size(apAspectApplyNames), s_AspectApplyState);
	if(NewApplyMode != CurrentApplyMode)
	{
		g_Config.m_MaCustomAspectRatioApplyMode = NewApplyMode;
		GameClient()->m_TClient.SetForcedAspect();
	}

	static CLineInputNumber s_CustomAspectNumeratorInput;
	static CLineInputNumber s_CustomAspectDenominatorInput;
	static bool s_CustomAspectInitialized = false;
	static int s_LastSyncedNum = -1;
	static int s_LastSyncedDen = -1;
	if(IsCustomMode)
	{
		const int CfgNum = g_Config.m_MaCustomAspectRatioNum > 0 ? g_Config.m_MaCustomAspectRatioNum : 16;
		const int CfgDen = g_Config.m_MaCustomAspectRatioDen > 0 ? g_Config.m_MaCustomAspectRatioDen : 9;
		if(!s_CustomAspectNumeratorInput.IsActive() && !s_CustomAspectDenominatorInput.IsActive() &&
			(!s_CustomAspectInitialized || s_LastSyncedNum != CfgNum || s_LastSyncedDen != CfgDen))
		{
			s_CustomAspectNumeratorInput.SetInteger(CfgNum);
			s_CustomAspectDenominatorInput.SetInteger(CfgDen);
			s_LastSyncedNum = CfgNum;
			s_LastSyncedDen = CfgDen;
			s_CustomAspectInitialized = true;
		}

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		CUIRect RatioLabel, RatioControls;
		RightView.HSplitTop(LineSize, &Row, &RightView);
		SplitAspectRow(Row, RatioLabel, RatioControls);
		Ui()->DoLabel(&RatioLabel, TCLocalize("Tamano"), FontSize, TEXTALIGN_ML);

		CUIRect NumeratorRect, SeparatorRect, DenominatorRect;
		const float Gap = minimum(6.0f, RatioControls.w * 0.08f);
		const float SeparatorWidth = minimum(12.0f, RatioControls.w * 0.18f);
		const float FieldWidth = maximum(1.0f, (RatioControls.w - SeparatorWidth - 2.0f * Gap) / 2.0f);
		RatioControls.VSplitLeft(FieldWidth, &NumeratorRect, &RatioControls);
		RatioControls.VSplitLeft(Gap, nullptr, &RatioControls);
		RatioControls.VSplitLeft(SeparatorWidth, &SeparatorRect, &RatioControls);
		RatioControls.VSplitLeft(Gap, nullptr, &RatioControls);
		RatioControls.VSplitLeft(FieldWidth, &DenominatorRect, nullptr);

		Ui()->DoEditBox(&s_CustomAspectNumeratorInput, &NumeratorRect, FontSize);
		Ui()->DoLabel(&SeparatorRect, ":", FontSize, TEXTALIGN_MC);
		Ui()->DoEditBox(&s_CustomAspectDenominatorInput, &DenominatorRect, FontSize);

		const int InputNum = maximum(1, s_CustomAspectNumeratorInput.GetInteger());
		const int InputDen = maximum(1, s_CustomAspectDenominatorInput.GetInteger());
		const bool HasPendingCustomChange = InputNum != g_Config.m_MaCustomAspectRatioNum || InputDen != g_Config.m_MaCustomAspectRatioDen;

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		CUIRect ButtonSpace, ApplyButton;
		RightView.HSplitTop(LineSize, &Row, &RightView);
		SplitAspectRow(Row, ButtonSpace, ApplyButton);
		(void)ButtonSpace;
		static CButtonContainer s_AspectApplyButton;
		if(DoButton_Menu(&s_AspectApplyButton, TCLocalize("Aplicar"), HasPendingCustomChange ? 0 : -1, &ApplyButton) && HasPendingCustomChange)
		{
			g_Config.m_MaCustomAspectRatioNum = InputNum;
			g_Config.m_MaCustomAspectRatioDen = InputDen;
			g_Config.m_MaCustomAspectRatio = std::clamp((int)std::lround((double)InputNum * 100.0 / (double)InputDen), 100, 1000);
			s_LastSyncedNum = InputNum;
			s_LastSyncedDen = InputDen;
			GameClient()->m_TClient.SetForcedAspect();
		}
	}
	else
	{
		s_CustomAspectInitialized = false;
		s_LastSyncedNum = -1;
		s_LastSyncedDen = -1;
	}
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Music Video Effect ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Efecto musica video"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicVideoEffect, TCLocalize("Activar efecto"), &g_Config.m_MaMusicVideoEffect, &RightView, LineSize);
	if(g_Config.m_MaMusicVideoEffect)
	{
		static CButtonContainer s_MusicVideoEffectColor;
		DoLine_ColorPicker(&s_MusicVideoEffectColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Color"), &g_Config.m_MaMusicVideoEffectColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicVideoEffectMusicOnly, TCLocalize("Solo musica detectada"), &g_Config.m_MaMusicVideoEffectMusicOnly, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicVideoEffectShowTrack, TCLocalize("Mostrar cancion"), &g_Config.m_MaMusicVideoEffectShowTrack, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicVideoEffectBehind, TCLocalize("Detras de todo"), &g_Config.m_MaMusicVideoEffectBehind, &RightView, LineSize);

		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectSize, &g_Config.m_MaMusicVideoEffectSize, &Button, TCLocalize("Tamano"), 40, 240, &CUi::ms_LinearScrollbarScale, 0, "%");
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectIntensity, &g_Config.m_MaMusicVideoEffectIntensity, &Button, TCLocalize("Intensidad"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectAlpha, &g_Config.m_MaMusicVideoEffectAlpha, &Button, TCLocalize("Opacidad"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectPoints, &g_Config.m_MaMusicVideoEffectPoints, &Button, TCLocalize("Puntos"), 32, 192);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectTrailLines, &g_Config.m_MaMusicVideoEffectTrailLines, &Button, TCLocalize("Lineas estela"), 1, 10);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicVideoEffectImageSize, &g_Config.m_MaMusicVideoEffectImageSize, &Button, TCLocalize("Tamano imagen"), 20, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		struct SMusicVideoImageFileListContext
		{
			std::vector<std::string> *m_pLabels;
			std::vector<std::string> *m_pPaths;
		};

		auto MusicVideoImageFileListScan = [](const char *pName, int IsDir, int StorageType, void *pUser) {
			(void)StorageType;
			if(IsDir)
				return 0;

			auto *pContext = static_cast<SMusicVideoImageFileListContext *>(pUser);
			const std::string Ext = MediaDecoder::ExtractExtensionLower(pName);
			const bool SupportedImage = Ext == "png" || Ext == "jpg" || Ext == "jpeg" || Ext == "webp" || Ext == "bmp" || Ext == "avif" || Ext == "gif";
			if(!SupportedImage)
				return 0;

			pContext->m_pLabels->emplace_back(pName);
			pContext->m_pPaths->emplace_back(std::string("tclient/music_video_effect/") + pName);
			return 0;
		};

		Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("tclient/music_video_effect", IStorage::TYPE_SAVE);

		static std::vector<std::string> s_vMusicVideoImageFileLabels;
		static std::vector<std::string> s_vMusicVideoImageFilePaths;
		s_vMusicVideoImageFileLabels.clear();
		s_vMusicVideoImageFilePaths.clear();
		SMusicVideoImageFileListContext MusicVideoImageContext{&s_vMusicVideoImageFileLabels, &s_vMusicVideoImageFilePaths};
		Storage()->ListDirectory(IStorage::TYPE_ALL, "tclient/music_video_effect", MusicVideoImageFileListScan, &MusicVideoImageContext);

		std::vector<int> vMusicVideoImageSortedIndices(s_vMusicVideoImageFileLabels.size());
		for(size_t i = 0; i < vMusicVideoImageSortedIndices.size(); ++i)
			vMusicVideoImageSortedIndices[i] = (int)i;
		std::sort(vMusicVideoImageSortedIndices.begin(), vMusicVideoImageSortedIndices.end(), [&](int Left, int Right) {
			return str_comp_nocase(s_vMusicVideoImageFileLabels[Left].c_str(), s_vMusicVideoImageFileLabels[Right].c_str()) < 0;
		});

		static std::vector<std::string> s_vMusicVideoImageDropDownLabels;
		static std::vector<const char *> s_vMusicVideoImageDropDownLabelPtrs;
		s_vMusicVideoImageDropDownLabels.clear();
		s_vMusicVideoImageDropDownLabelPtrs.clear();
		s_vMusicVideoImageDropDownLabels.emplace_back(TCLocalize("Sin imagen"));
		for(int SortedIndex : vMusicVideoImageSortedIndices)
			s_vMusicVideoImageDropDownLabels.push_back(s_vMusicVideoImageFileLabels[SortedIndex]);
		for(const std::string &LabelString : s_vMusicVideoImageDropDownLabels)
			s_vMusicVideoImageDropDownLabelPtrs.push_back(LabelString.c_str());

		int SelectedMusicVideoImageFile = 0;
		if(g_Config.m_MaMusicVideoEffectImagePath[0] != '\0')
		{
			for(size_t i = 0; i < vMusicVideoImageSortedIndices.size(); ++i)
			{
				const int SortedIndex = vMusicVideoImageSortedIndices[i];
				if(str_comp(g_Config.m_MaMusicVideoEffectImagePath, s_vMusicVideoImageFilePaths[SortedIndex].c_str()) == 0)
				{
					SelectedMusicVideoImageFile = (int)i + 1;
					break;
				}
			}
		}

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		CUIRect MusicVideoImageRow, MusicVideoImageLabel, MusicVideoImageDropDown, MusicVideoImageReloadButton, MusicVideoImageFolderButton;
		RightView.HSplitTop(LineSize, &MusicVideoImageRow, &RightView);
		MusicVideoImageRow.VSplitLeft(105.0f, &MusicVideoImageLabel, &MusicVideoImageRow);
		Ui()->DoLabel(&MusicVideoImageLabel, TCLocalize("Imagen central"), FontSize, TEXTALIGN_ML);
		MusicVideoImageRow.VSplitRight(20.0f, &MusicVideoImageRow, &MusicVideoImageFolderButton);
		MusicVideoImageRow.VSplitRight(MarginSmall, &MusicVideoImageRow, nullptr);
		MusicVideoImageRow.VSplitRight(20.0f, &MusicVideoImageRow, &MusicVideoImageReloadButton);
		MusicVideoImageRow.VSplitRight(MarginSmall, &MusicVideoImageRow, nullptr);
		MusicVideoImageDropDown = MusicVideoImageRow;

		static CUi::SDropDownState s_MusicVideoImageDropDownState;
		static CScrollRegion s_MusicVideoImageDropDownScrollRegion;
		s_MusicVideoImageDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_MusicVideoImageDropDownScrollRegion;
		const int NewSelectedMusicVideoImageFile = Ui()->DoDropDown(&MusicVideoImageDropDown, SelectedMusicVideoImageFile, s_vMusicVideoImageDropDownLabelPtrs.data(), s_vMusicVideoImageDropDownLabelPtrs.size(), s_MusicVideoImageDropDownState);
		if(NewSelectedMusicVideoImageFile != SelectedMusicVideoImageFile)
		{
			if(NewSelectedMusicVideoImageFile <= 0)
				g_Config.m_MaMusicVideoEffectImagePath[0] = '\0';
			else if(NewSelectedMusicVideoImageFile - 1 < (int)vMusicVideoImageSortedIndices.size())
			{
				const int SortedIndex = vMusicVideoImageSortedIndices[NewSelectedMusicVideoImageFile - 1];
				str_copy(g_Config.m_MaMusicVideoEffectImagePath, s_vMusicVideoImageFilePaths[SortedIndex].c_str(), sizeof(g_Config.m_MaMusicVideoEffectImagePath));
			}
			GameClient()->m_Ma.ReloadMusicVideoCenterImage();
		}

		static CButtonContainer s_MusicVideoImageReloadButton;
		if(Ui()->DoButton_FontIcon(&s_MusicVideoImageReloadButton, FontIcon::ARROW_ROTATE_RIGHT, 0, &MusicVideoImageReloadButton, BUTTONFLAG_LEFT))
			GameClient()->m_Ma.ReloadMusicVideoCenterImage();

		static CButtonContainer s_MusicVideoImageFolderButton;
		if(Ui()->DoButton_FontIcon(&s_MusicVideoImageFolderButton, FontIcon::FOLDER, 0, &MusicVideoImageFolderButton, BUTTONFLAG_LEFT))
		{
			Storage()->CreateFolder("tclient", IStorage::TYPE_SAVE);
			Storage()->CreateFolder("tclient/music_video_effect", IStorage::TYPE_SAVE);
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "tclient/music_video_effect", aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}

		if(g_Config.m_MaMusicVideoEffectImagePath[0] != '\0' && !GameClient()->m_Ma.MusicVideoCenterImageLoaded() && !GameClient()->m_Ma.MusicVideoCenterImageHasError())
			GameClient()->m_Ma.ReloadMusicVideoCenterImage();

		RightView.HSplitTop(LineSize, &Row, &RightView);
		if(GameClient()->m_Ma.MusicVideoCenterImageHasError())
			TextRender()->TextColor(ColorRGBA(1.0f, 0.45f, 0.45f, 1.0f));
		else if(GameClient()->m_Ma.MusicVideoCenterImageLoaded())
			TextRender()->TextColor(ColorRGBA(0.55f, 1.0f, 0.55f, 1.0f));
		Ui()->DoLabel(&Row, GameClient()->m_Ma.MusicVideoCenterImageStatusText(), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
		RightView.HSplitTop(LineSize * 2.0f, nullptr, &RightView);
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Startup Music ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Musica de inicio"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaStartupMusic, TCLocalize("Activar musica de inicio"), &g_Config.m_MaStartupMusic, &RightView, LineSize);
	if(g_Config.m_MaStartupMusic)
	{
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaStartupMusicVolume, &g_Config.m_MaStartupMusicVolume, &Button, TCLocalize("Volumen"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		const char *pStartupMusicDefaultPath = "ma/startup_music/welcome_to_ddnet.mp3";
		const char *pStartupMusicSpacedDefaultPath = "ma/startup_music/welcome to ddnet.mp3";
		const char *pStartupMusicOldDefaultPath = "ma/startup_music/ma_welcome_ddnet_client.mp3";
		const char *pStartupMusicLegacyPath = "ma/startup_music/MONTAGEM CEINTA (Slowed).mp3";
		const char *pStartupMusicLegacyWavPath = "ma/startup_music/ma_welcome_ddnet_client.wav";
		if(str_comp(g_Config.m_MaStartupMusicPath, pStartupMusicSpacedDefaultPath) == 0 || str_comp(g_Config.m_MaStartupMusicPath, pStartupMusicOldDefaultPath) == 0 || str_comp(g_Config.m_MaStartupMusicPath, pStartupMusicLegacyPath) == 0 || str_comp(g_Config.m_MaStartupMusicPath, pStartupMusicLegacyWavPath) == 0)
			str_copy(g_Config.m_MaStartupMusicPath, pStartupMusicDefaultPath, sizeof(g_Config.m_MaStartupMusicPath));

		struct SStartupMusicFileListContext
		{
			std::vector<std::string> *m_pLabels;
			std::vector<std::string> *m_pPaths;
			const char *m_pDefaultPath;
		};

		auto StartupMusicFileListScan = [](const char *pName, int IsDir, int StorageType, void *pUser) {
			(void)StorageType;
			if(IsDir)
				return 0;

			auto *pContext = static_cast<SStartupMusicFileListContext *>(pUser);
			const std::string Ext = MediaDecoder::ExtractExtensionLower(pName);
			const bool SupportedSong = Ext == "mp3" || Ext == "wav" || Ext == "wma" || Ext == "opus" || Ext == "ogg" || Ext == "wv";
			if(!SupportedSong)
				return 0;

			std::string Path = "ma/startup_music/";
			Path += pName;
			if(str_comp(Path.c_str(), pContext->m_pDefaultPath) == 0)
				return 0;

			pContext->m_pLabels->emplace_back(pName);
			pContext->m_pPaths->push_back(Path);
			return 0;
		};

		Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("ma/startup_music", IStorage::TYPE_SAVE);

		static std::vector<std::string> s_vStartupMusicFileLabels;
		static std::vector<std::string> s_vStartupMusicFilePaths;
		s_vStartupMusicFileLabels.clear();
		s_vStartupMusicFilePaths.clear();
		SStartupMusicFileListContext StartupMusicContext{&s_vStartupMusicFileLabels, &s_vStartupMusicFilePaths, pStartupMusicDefaultPath};
		Storage()->ListDirectory(IStorage::TYPE_ALL, "ma/startup_music", StartupMusicFileListScan, &StartupMusicContext);

		std::vector<int> vStartupMusicSortedIndices(s_vStartupMusicFileLabels.size());
		for(size_t i = 0; i < vStartupMusicSortedIndices.size(); ++i)
			vStartupMusicSortedIndices[i] = (int)i;
		std::sort(vStartupMusicSortedIndices.begin(), vStartupMusicSortedIndices.end(), [&](int Left, int Right) {
			return str_comp_nocase(s_vStartupMusicFileLabels[Left].c_str(), s_vStartupMusicFileLabels[Right].c_str()) < 0;
		});

		static std::vector<std::string> s_vStartupMusicDropDownLabels;
		static std::vector<std::string> s_vStartupMusicDropDownPaths;
		static std::vector<const char *> s_vStartupMusicDropDownLabelPtrs;
		s_vStartupMusicDropDownLabels.clear();
		s_vStartupMusicDropDownPaths.clear();
		s_vStartupMusicDropDownLabelPtrs.clear();
		s_vStartupMusicDropDownLabels.emplace_back(TCLocalize("Predeterminada"));
		s_vStartupMusicDropDownPaths.emplace_back(pStartupMusicDefaultPath);
		for(int SortedIndex : vStartupMusicSortedIndices)
		{
			s_vStartupMusicDropDownLabels.push_back(s_vStartupMusicFileLabels[SortedIndex]);
			s_vStartupMusicDropDownPaths.push_back(s_vStartupMusicFilePaths[SortedIndex]);
		}

		int SelectedStartupMusicFile = 0;
		bool CurrentStartupMusicListed = g_Config.m_MaStartupMusicPath[0] == '\0' || str_comp(g_Config.m_MaStartupMusicPath, pStartupMusicDefaultPath) == 0;
		if(!CurrentStartupMusicListed)
		{
			for(size_t i = 1; i < s_vStartupMusicDropDownPaths.size(); ++i)
			{
				if(str_comp(g_Config.m_MaStartupMusicPath, s_vStartupMusicDropDownPaths[i].c_str()) == 0)
				{
					SelectedStartupMusicFile = (int)i;
					CurrentStartupMusicListed = true;
					break;
				}
			}
		}
		if(!CurrentStartupMusicListed)
		{
			const char *pMissingLabel = g_Config.m_MaStartupMusicPath;
			if(const char *pSlash = str_rchr(pMissingLabel, '/'))
				pMissingLabel = pSlash + 1;
			s_vStartupMusicDropDownLabels.emplace_back(pMissingLabel);
			s_vStartupMusicDropDownPaths.emplace_back(g_Config.m_MaStartupMusicPath);
			SelectedStartupMusicFile = (int)s_vStartupMusicDropDownLabels.size() - 1;
		}

		for(const std::string &LabelString : s_vStartupMusicDropDownLabels)
			s_vStartupMusicDropDownLabelPtrs.push_back(LabelString.c_str());

		CUIRect StartupMusicRow, StartupMusicLabel, StartupMusicDropDown, StartupMusicPlayButton, StartupMusicFolderButton;
		RightView.HSplitTop(LineSize, &StartupMusicRow, &RightView);
		StartupMusicRow.VSplitLeft(105.0f, &StartupMusicLabel, &StartupMusicRow);
		Ui()->DoLabel(&StartupMusicLabel, TCLocalize("Cancion"), FontSize, TEXTALIGN_ML);
		StartupMusicRow.VSplitRight(20.0f, &StartupMusicRow, &StartupMusicFolderButton);
		StartupMusicRow.VSplitRight(MarginSmall, &StartupMusicRow, nullptr);
		StartupMusicRow.VSplitRight(20.0f, &StartupMusicRow, &StartupMusicPlayButton);
		StartupMusicRow.VSplitRight(MarginSmall, &StartupMusicRow, nullptr);
		StartupMusicDropDown = StartupMusicRow;

		static CUi::SDropDownState s_StartupMusicDropDownState;
		static CScrollRegion s_StartupMusicDropDownScrollRegion;
		s_StartupMusicDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_StartupMusicDropDownScrollRegion;
		const int NewSelectedStartupMusicFile = Ui()->DoDropDown(&StartupMusicDropDown, SelectedStartupMusicFile, s_vStartupMusicDropDownLabelPtrs.data(), s_vStartupMusicDropDownLabelPtrs.size(), s_StartupMusicDropDownState);
		if(NewSelectedStartupMusicFile != SelectedStartupMusicFile && NewSelectedStartupMusicFile >= 0 && NewSelectedStartupMusicFile < (int)s_vStartupMusicDropDownPaths.size())
		{
			str_copy(g_Config.m_MaStartupMusicPath, s_vStartupMusicDropDownPaths[NewSelectedStartupMusicFile].c_str(), sizeof(g_Config.m_MaStartupMusicPath));
			GameClient()->m_Ma.RestartStartupMusic();
		}

		static CButtonContainer s_StartupMusicPlayButton;
		if(Ui()->DoButton_FontIcon(&s_StartupMusicPlayButton, FontIcon::PLAY, 0, &StartupMusicPlayButton, BUTTONFLAG_LEFT))
			GameClient()->m_Ma.RestartStartupMusic();

		static CButtonContainer s_StartupMusicFolderButton;
		if(Ui()->DoButton_FontIcon(&s_StartupMusicFolderButton, FontIcon::FOLDER, 0, &StartupMusicFolderButton, BUTTONFLAG_LEFT))
		{
			Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
			Storage()->CreateFolder("ma/startup_music", IStorage::TYPE_SAVE);
			char aBuf[IO_MAX_PATH_LENGTH];
			Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ma/startup_music", aBuf, sizeof(aBuf));
			Client()->ViewFile(aBuf);
		}

		RightView.HSplitTop(LineSize, &Row, &RightView);
		const char *pStartupMusicStatus = GameClient()->m_Ma.StartupMusicStatusText();
		if(str_startswith(pStartupMusicStatus, "No se"))
			TextRender()->TextColor(ColorRGBA(1.0f, 0.45f, 0.45f, 1.0f));
		else if(str_comp(pStartupMusicStatus, TCLocalize("Sonando.")) == 0)
			TextRender()->TextColor(ColorRGBA(0.55f, 1.0f, 0.55f, 1.0f));
		Ui()->DoLabel(&Row, pStartupMusicStatus, 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}
	else
		RightView.HSplitTop(LineSize * 2.0f, nullptr, &RightView);
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Tee Trails ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Tee Trails"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrail, TCLocalize("Enable tee trails"), &g_Config.m_TcTeeTrail, &RightView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailOthers, TCLocalize("Show other tees' trails"), &g_Config.m_TcTeeTrailOthers, &RightView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailFade, TCLocalize("Fade trail alpha"), &g_Config.m_TcTeeTrailFade, &RightView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailTaper, TCLocalize("Taper trail width"), &g_Config.m_TcTeeTrailTaper, &RightView, LineSize);

	RightView.HSplitTop(MarginExtraSmall, nullptr, &RightView);
	static std::vector<const char *> s_TrailStyleDropDownNames;
	s_TrailStyleDropDownNames = {
		TCLocalize("Default"),
		TCLocalize("Corazones"),
		TCLocalize("Stars"),
		TCLocalize("Diamantes"),
		TCLocalize("Lunas"),
		TCLocalize("Rayos"),
		TCLocalize("Mariposas"),
		TCLocalize("Flores"),
		TCLocalize("Notas musicales"),
		TCLocalize("Calaveras"),
		TCLocalize("Coronas"),
		TCLocalize("Llamas"),
		TCLocalize("Copos de nieve")};
	static CUi::SDropDownState s_TrailStyleDropDownState;
	static CScrollRegion s_TrailStyleDropDownScrollRegion;
	s_TrailStyleDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailStyleDropDownScrollRegion;
	g_Config.m_TcTeeTrailStyle = std::clamp(g_Config.m_TcTeeTrailStyle, (int)CTrails::TRAILSTYLE_DEFAULT, (int)s_TrailStyleDropDownNames.size() - 1);
	CUIRect TrailStyleDropDownRect;
	RightView.HSplitTop(LineSize, &TrailStyleDropDownRect, &RightView);
	TrailStyleDropDownRect.VSplitLeft(120.0f, &Label, &TrailStyleDropDownRect);
	Ui()->DoLabel(&Label, TCLocalize("Trail style"), FontSize, TEXTALIGN_ML);
	g_Config.m_TcTeeTrailStyle = Ui()->DoDropDown(&TrailStyleDropDownRect, g_Config.m_TcTeeTrailStyle, s_TrailStyleDropDownNames.data(), s_TrailStyleDropDownNames.size(), s_TrailStyleDropDownState);

	if(g_Config.m_TcTeeTrailStyle != CTrails::TRAILSTYLE_DEFAULT)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailMovement, TCLocalize("Movement"), &g_Config.m_TcTeeTrailMovement, &RightView, LineSize);
		if(g_Config.m_TcTeeTrailMovement)
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailMovementSpeed, &g_Config.m_TcTeeTrailMovementSpeed, &Button, TCLocalize("Movement speed"), 0, 500, &CUi::ms_LinearScrollbarScale, 0, "%");
		}
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcTeeTrailMusicReaction, TCLocalize("Reaccion a la musica"), &g_Config.m_TcTeeTrailMusicReaction, &RightView, LineSize);
	if(g_Config.m_TcTeeTrailMusicReaction)
	{
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailMusicReactionStrength, &g_Config.m_TcTeeTrailMusicReactionStrength, &Button, TCLocalize("Intensidad reaccion"), 0, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
	}

	RightView.HSplitTop(MarginExtraSmall, nullptr, &RightView);
	std::vector<const char *> vTrailDropDownNames;
	vTrailDropDownNames = {TCLocalize("Solid"), TCLocalize("Tee"), TCLocalize("Rainbow"), TCLocalize("Speed")};
	static CUi::SDropDownState s_TrailDropDownState;
	static CScrollRegion s_TrailDropDownScrollRegion;
	s_TrailDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_TrailDropDownScrollRegion;
	int TrailSelectedOld = g_Config.m_TcTeeTrailColorMode - 1;
	CUIRect TrailDropDownRect;
	RightView.HSplitTop(LineSize, &TrailDropDownRect, &RightView);
	const int TrailSelectedNew = Ui()->DoDropDown(&TrailDropDownRect, TrailSelectedOld, vTrailDropDownNames.data(), vTrailDropDownNames.size(), s_TrailDropDownState);
	if(TrailSelectedOld != TrailSelectedNew)
	{
		g_Config.m_TcTeeTrailColorMode = TrailSelectedNew + 1;
	}
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	static CButtonContainer s_TeeTrailColor;
	if(g_Config.m_TcTeeTrailColorMode == CTrails::COLORMODE_SOLID)
		DoLine_ColorPicker(&s_TeeTrailColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Tee trail color"), &g_Config.m_TcTeeTrailColor, ColorRGBA(1.0f, 1.0f, 1.0f), false);
	else
		RightView.HSplitTop(ColorPickerLineSize + ColorPickerLineSpacing, &Button, &RightView);

	RightView.HSplitTop(LineSize, &Button, &RightView);
	Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailWidth, &g_Config.m_TcTeeTrailWidth, &Button, TCLocalize("Trail width"), 0, 20);
	RightView.HSplitTop(LineSize, &Button, &RightView);
	Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailLength, &g_Config.m_TcTeeTrailLength, &Button, TCLocalize("Trail length"), 0, 200);
	RightView.HSplitTop(LineSize, &Button, &RightView);
	Ui()->DoScrollbarOption(&g_Config.m_TcTeeTrailAlpha, &g_Config.m_TcTeeTrailAlpha, &Button, TCLocalize("Trail alpha"), 0, 100);
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Music Player ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Music Player"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayer, TCLocalize("Enable music player"), &g_Config.m_MaMusicPlayer, &RightView, LineSize);
	if(g_Config.m_MaMusicPlayer)
	{
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, TCLocalize("Presets"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		static std::vector<const char *> s_PresetNames;
		s_PresetNames = {TCLocalize("Classic"), TCLocalize("Compact"), TCLocalize("Custom")};
		static CUi::SDropDownState s_PresetDropDownState;
		static CScrollRegion s_PresetScrollRegion;
		s_PresetDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_PresetScrollRegion;
		static int s_PresetIndex = 2;
		int PrevPreset = s_PresetIndex;
		RightView.HSplitTop(LineSize, &Button, &RightView);
		s_PresetIndex = Ui()->DoDropDown(&Button, s_PresetIndex, s_PresetNames.data(), s_PresetNames.size(), s_PresetDropDownState);

		if(s_PresetIndex != PrevPreset && s_PresetIndex < 2)
		{
			switch(s_PresetIndex)
			{
			case 0:
				g_Config.m_MaMusicPlayerSizeMode = 0; g_Config.m_MaMusicPlayerColorMode = 3;
				g_Config.m_MaMusicPlayerShowCover = 1; g_Config.m_MaMusicPlayerVisualizer = 1;
				g_Config.m_MaMusicPlayerTextScale = 100;
				break;
			case 1:
				g_Config.m_MaMusicPlayerSizeMode = 1; g_Config.m_MaMusicPlayerColorMode = 3;
				g_Config.m_MaMusicPlayerShowCover = 0; g_Config.m_MaMusicPlayerVisualizer = 0;
				g_Config.m_MaMusicPlayerTextScale = 84;
				break;
			}
		}
		g_Config.m_MaMusicPlayerSizeMode = 1;
		g_Config.m_MaMusicPlayerColorMode = 3;

		RightView.HSplitTop(LineSize, &Button, &RightView);
		static int s_TextScale = 84;
		s_TextScale = g_Config.m_MaMusicPlayerTextScale;
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerTextScale, &g_Config.m_MaMusicPlayerTextScale, &Button, TCLocalize("Tamano numeros"), 70, 150, &CUi::ms_LinearScrollbarScale, 0, "%");
		if(g_Config.m_MaMusicPlayerTextScale != s_TextScale) s_PresetIndex = 2;

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayerShowCover, TCLocalize("Show cover art"), &g_Config.m_MaMusicPlayerShowCover, &RightView, LineSize);

		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerAnimationMs, &g_Config.m_MaMusicPlayerAnimationMs, &Button, TCLocalize("Animation speed"), 50, 1000, &CUi::ms_LinearScrollbarScale, 0, "ms");

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayerHideGameTimer, TCLocalize("No mostrar tiempo en juego"), &g_Config.m_MaMusicPlayerHideGameTimer, &RightView, LineSize);

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayerVisualizer, TCLocalize("Visualizer"), &g_Config.m_MaMusicPlayerVisualizer, &RightView, LineSize);
		if(g_Config.m_MaMusicPlayerVisualizer)
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerVisualizerColumns, &g_Config.m_MaMusicPlayerVisualizerColumns, &Button, TCLocalize("Bars"), 5, 10);
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerVisualizerSensitivity, &g_Config.m_MaMusicPlayerVisualizerSensitivity, &Button, TCLocalize("Sensitivity"), 50, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerVisualizerSmoothing, &g_Config.m_MaMusicPlayerVisualizerSmoothing, &Button, TCLocalize("Smoothing"), 0, 100);
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaMusicPlayerVisualizerMode, &g_Config.m_MaMusicPlayerVisualizerMode, &Button, TCLocalize("Mode"), 0, 2, &CUi::ms_LinearScrollbarScale, 0, " (0=Bottom/1=Center/2=Up)");
		}
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayerShowWhenPaused, TCLocalize("Show when paused"), &g_Config.m_MaMusicPlayerShowWhenPaused, &RightView, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaMusicPlayerCustomColors, TCLocalize("Custom colors"), &g_Config.m_MaMusicPlayerCustomColors, &RightView, LineSize);
		if(g_Config.m_MaMusicPlayerCustomColors)
		{
			static CButtonContainer s_MusicBgColor, s_MusicAccentColor, s_MusicTextColor;
			if(((g_Config.m_MaMusicPlayerColorBg >> 24) & 0xFF) == 0)
			{
				ColorHSLA BgColor(g_Config.m_MaMusicPlayerColorBg, false);
				BgColor.a = 0.80f;
				g_Config.m_MaMusicPlayerColorBg = BgColor.Pack(true);
			}
			DoLine_ColorPicker(&s_MusicBgColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Background"), &g_Config.m_MaMusicPlayerColorBg, ColorRGBA(0.05f, 0.05f, 0.08f, 0.8f), false, nullptr, true);
			DoLine_ColorPicker(&s_MusicAccentColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Accent"), &g_Config.m_MaMusicPlayerColorAccent, ColorRGBA(0.23f, 0.51f, 0.96f, 1.0f), false);
			DoLine_ColorPicker(&s_MusicTextColor, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Text"), &g_Config.m_MaMusicPlayerColorText, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), false);
		}
	}
	else
		RightView.HSplitTop(LineSize * 2.0f, nullptr, &RightView);
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.w = MainView.w;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderMaNickNames(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ***** Efectos de nombres por jugador ***** //
	{
		std::vector<SMaNameEffectMenuEntry> vNameEffectEntries;
		MaNameEffectDecodeEntries(vNameEffectEntries);
		MaNameEffectAddLegacyNames(vNameEffectEntries);

		static char s_aNameEffectEditName[MAX_NAME_LENGTH] = "";
		static int s_NameEffectEditStyle = 0;
		static unsigned s_NameEffectEditColor1 = 65425;
		static unsigned s_NameEffectEditColor2 = 41131;
		static int s_NameEffectEditGlow = 70;
		static int s_NameEffectEditMoving = 0;
		static int s_NameEffectEditStars = 0;
		static bool s_NameEffectEditLoaded = false;
		static bool s_NameEffectEditOwn = false;
		static char s_aNameEffectSavedName[MAX_NAME_LENGTH] = "";
		static int s_NameEffectSavedStyle = 0;
		static unsigned s_NameEffectSavedColor1 = 0;
		static unsigned s_NameEffectSavedColor2 = 0;
		static int s_NameEffectSavedGlow = 0;
		static int s_NameEffectSavedMoving = 0;
		static int s_NameEffectSavedStars = 0;
		static bool s_NameEffectSavedOwn = false;

		auto RememberNameEffectSaved = [&]() {
			str_copy(s_aNameEffectSavedName, s_aNameEffectEditName, sizeof(s_aNameEffectSavedName));
			s_NameEffectSavedStyle = s_NameEffectEditStyle;
			s_NameEffectSavedColor1 = s_NameEffectEditColor1;
			s_NameEffectSavedColor2 = s_NameEffectEditColor2;
			s_NameEffectSavedGlow = s_NameEffectEditGlow;
			s_NameEffectSavedMoving = s_NameEffectEditMoving ? 1 : 0;
			s_NameEffectSavedStars = s_NameEffectEditStars ? 1 : 0;
			s_NameEffectSavedOwn = s_NameEffectEditOwn;
		};

		auto DoNameEffectColorPicker = [&](CButtonContainer *pResetId, const char *pText, unsigned *pColorValue, ColorRGBA DefaultColor) {
			CUIRect Section, ColorPickerButton, ResetButton, TextLabel;
			RightView.HSplitTop(ColorPickerLineSize, &Section, &RightView);
			RightView.HSplitTop(ColorPickerLineSpacing, nullptr, &RightView);
			Section.VSplitRight(70.0f, &Section, &ResetButton);
			Section.VSplitRight(10.0f, &Section, nullptr);
			Section.VSplitRight(Section.h, &Section, &ColorPickerButton);
			Section.VSplitRight(10.0f, &TextLabel, nullptr);

			*pColorValue = MaNameEffectNormalizeMenuColor(*pColorValue);
			Ui()->DoLabel(&TextLabel, pText, ColorPickerLabelSize, TEXTALIGN_ML);
			const ColorHSLA PickedColor = DoButton_ColorPicker(&ColorPickerButton, pColorValue, false);
			*pColorValue = MaNameEffectNormalizeMenuColor(PickedColor.Pack(false));

			CUIRect SwatchFrame = ColorPickerButton;
			CUIRect SwatchInner;
			SwatchFrame.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.95f), IGraphics::CORNER_ALL, 5.0f);
			SwatchFrame.Margin(2.0f, &SwatchInner);
			SwatchInner.Draw(MaNameEffectMenuColor(*pColorValue), IGraphics::CORNER_ALL, 4.0f);
			CUIRect Shine = SwatchInner;
			Shine.HSplitTop(Shine.h * 0.42f, &Shine, nullptr);
			Shine.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.22f), IGraphics::CORNER_T, 4.0f);

			ResetButton.HMargin(2.0f, &ResetButton);
			if(DoButton_Menu(pResetId, TCLocalize("Reiniciar"), 0, &ResetButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 4.0f, 0.1f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f)))
				*pColorValue = MaNameEffectNormalizeMenuColor(color_cast<ColorHSLA>(DefaultColor).Pack(false));
		};

		auto FindOnlineClientId = [&](const char *pName) -> int {
			for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
			{
				if(GameClient()->m_Snap.m_apPlayerInfos[ClientId] && str_comp(GameClient()->m_aClients[ClientId].m_aName, pName) == 0)
					return ClientId;
			}
			return -1;
		};
		auto RenderNameEffectMenuLabel = [&](CUIRect Rect, const char *pName, const SMaNameEffectMenuEntry *pEntry, bool Own, float Size, ColorRGBA FallbackColor, int Align) -> bool {
			if(!pName || pName[0] == '\0')
				return false;

			int Style = 0;
			unsigned Color1 = MaNameEffectDefaultColor1();
			unsigned Color2 = MaNameEffectDefaultColor2();
			bool Moving = false;
			bool Stars = false;
			bool Active = false;

			if(Own && g_Config.m_MaNameEffectsOwn)
			{
				Style = std::clamp(g_Config.m_MaNameEffectsOwnStyle, 0, MaNameEffects::STYLE_MAX);
				Color1 = MaNameEffectNormalizeMenuColor(g_Config.m_MaNameEffectsOwnColor1);
				Color2 = MaNameEffectNormalizeMenuColor(g_Config.m_MaNameEffectsOwnColor2);
				Moving = g_Config.m_MaNameEffectsOwnMoving != 0;
				Stars = g_Config.m_MaNameEffectsOwnStars != 0;
				Active = true;
			}
			else if(pEntry)
			{
				Style = std::clamp(pEntry->m_Style, 0, MaNameEffects::STYLE_MAX);
				Color1 = MaNameEffectNormalizeMenuColor(pEntry->m_Color1);
				Color2 = MaNameEffectNormalizeMenuColor(pEntry->m_Color2);
				Moving = pEntry->m_Moving != 0;
				Stars = pEntry->m_Stars != 0;
				Active = true;
			}

			if(!Active)
			{
				TextRender()->TextColor(FallbackColor);
				Ui()->DoLabel(&Rect, pName, Size, Align);
				TextRender()->TextColor(TextRender()->DefaultTextColor());
				return false;
			}

			const int MotionTick = (Moving || MaNameEffects::StyleAnimates(Style)) ? (int)((time_get() * 12) / time_freq()) : 0;
			const ColorRGBA Accent = MaNameEffects::AccentColor(Style, 1.0f, Color1, Color2, MotionTick);
			const char *pPrefix = "";
			const char *pSuffix = "";
			MaNameEffects::Decorations(Style, &pPrefix, &pSuffix);

			float Width = TextRender()->TextWidth(Size, pName, -1, -1.0f);
			if(Stars)
				Width += TextRender()->TextWidth(Size, "\xE2\x9C\xA6  \xE2\x9C\xA6", -1, -1.0f);
			if(pPrefix[0] != '\0')
				Width += TextRender()->TextWidth(Size, pPrefix, -1, -1.0f);
			if(pSuffix[0] != '\0')
				Width += TextRender()->TextWidth(Size, pSuffix, -1, -1.0f);

			float X = Rect.x;
			if(Align == TEXTALIGN_MC)
				X = Rect.x + (Rect.w - Width) * 0.5f;
			else if(Align == TEXTALIGN_MR)
				X = Rect.x + Rect.w - Width;
			const float Y = Rect.y + (Rect.h - Size) * 0.5f;

			CTextCursor Cursor;
			Cursor.SetPosition(vec2(X, Y));
			Cursor.m_FontSize = Size;
			TextRender()->TextOutlineColor(ColorRGBA(Accent.r, Accent.g, Accent.b, Style == 11 ? 0.95f : 0.45f));

			if(Stars)
			{
				TextRender()->TextColor(Accent);
				TextRender()->TextEx(&Cursor, "\xE2\x9C\xA6 ");
			}
			if(pPrefix[0] != '\0')
			{
				TextRender()->TextColor(Accent);
				TextRender()->TextEx(&Cursor, pPrefix);
			}

			const char *pChar = pName;
			int LetterIndex = 0;
			while(*pChar)
			{
				const char *pNext = pChar;
				str_utf8_decode(&pNext);
				const int CharLen = maximum<int>(1, (int)(pNext - pChar));
				TextRender()->TextColor(MaNameEffects::LetterColor(LetterIndex, 1.0f, Style, Color1, Color2, Moving, MotionTick));
				TextRender()->TextEx(&Cursor, pChar, CharLen);
				pChar += CharLen;
				++LetterIndex;
			}

			if(pSuffix[0] != '\0')
			{
				TextRender()->TextColor(Accent);
				TextRender()->TextEx(&Cursor, pSuffix);
			}
			if(Stars)
			{
				TextRender()->TextColor(Accent);
				TextRender()->TextEx(&Cursor, " \xE2\x9C\xA6");
			}
			TextRender()->TextColor(TextRender()->DefaultTextColor());
			TextRender()->TextOutlineColor(TextRender()->DefaultTextOutlineColor());
			return true;
		};

		auto LoadNameEffectPlayer = [&](const char *pName, bool Own) {
			str_copy(s_aNameEffectEditName, pName, sizeof(s_aNameEffectEditName));
			MaNameEffectSanitizeName(s_aNameEffectEditName);
			s_NameEffectEditOwn = Own;
			if(Own)
			{
				s_NameEffectEditStyle = std::clamp(g_Config.m_MaNameEffectsOwnStyle, 0, MaNameEffects::STYLE_MAX);
				s_NameEffectEditColor1 = MaNameEffectNormalizeMenuColor(g_Config.m_MaNameEffectsOwnColor1);
				s_NameEffectEditColor2 = MaNameEffectNormalizeMenuColor(g_Config.m_MaNameEffectsOwnColor2);
				s_NameEffectEditGlow = std::clamp(g_Config.m_MaNameEffectsOwnGlow, 0, 100);
				s_NameEffectEditMoving = g_Config.m_MaNameEffectsOwnMoving != 0;
				s_NameEffectEditStars = g_Config.m_MaNameEffectsOwnStars != 0;
			}
			else
			{
				const int EntryIndex = MaNameEffectFindEntryIndex(vNameEffectEntries, s_aNameEffectEditName);
				if(EntryIndex >= 0)
				{
					const SMaNameEffectMenuEntry &Entry = vNameEffectEntries[EntryIndex];
					s_NameEffectEditStyle = std::clamp(Entry.m_Style, 0, MaNameEffects::STYLE_MAX);
					s_NameEffectEditColor1 = MaNameEffectNormalizeMenuColor(Entry.m_Color1);
					s_NameEffectEditColor2 = MaNameEffectNormalizeMenuColor(Entry.m_Color2);
					s_NameEffectEditGlow = std::clamp(Entry.m_Glow, 0, 100);
					s_NameEffectEditMoving = Entry.m_Moving ? 1 : 0;
					s_NameEffectEditStars = Entry.m_Stars ? 1 : 0;
				}
				else
				{
					s_NameEffectEditStyle = std::clamp(g_Config.m_MaNameEffectsStyle, 0, MaNameEffects::STYLE_MAX);
					s_NameEffectEditColor1 = MaNameEffectDefaultColor1();
					s_NameEffectEditColor2 = MaNameEffectDefaultColor2();
					s_NameEffectEditGlow = std::clamp(g_Config.m_MaNameEffectsGlow, 0, 100);
					s_NameEffectEditMoving = g_Config.m_MaNameEffectsMoving != 0;
					s_NameEffectEditStars = g_Config.m_MaNameEffectsStars != 0;
				}
			}
			s_NameEffectEditLoaded = true;
			RememberNameEffectSaved();
		};

		auto SaveCurrentNameEffect = [&]() {
			MaNameEffectSanitizeName(s_aNameEffectEditName);
			if(s_aNameEffectEditName[0] == '\0')
				return;
			g_Config.m_MaNameEffects = 1;
			s_NameEffectEditColor1 = MaNameEffectNormalizeMenuColor(s_NameEffectEditColor1);
			s_NameEffectEditColor2 = MaNameEffectNormalizeMenuColor(s_NameEffectEditColor2);
			if(s_NameEffectEditOwn)
			{
				g_Config.m_MaNameEffectsOwn = 1;
				g_Config.m_MaNameEffectsOwnStyle = std::clamp(s_NameEffectEditStyle, 0, MaNameEffects::STYLE_MAX);
				g_Config.m_MaNameEffectsOwnColor1 = s_NameEffectEditColor1;
				g_Config.m_MaNameEffectsOwnColor2 = s_NameEffectEditColor2;
				g_Config.m_MaNameEffectsOwnGlow = std::clamp(s_NameEffectEditGlow, 0, 100);
				g_Config.m_MaNameEffectsOwnMoving = s_NameEffectEditMoving ? 1 : 0;
				g_Config.m_MaNameEffectsOwnStars = s_NameEffectEditStars ? 1 : 0;

				const int ExistingIndex = MaNameEffectFindEntryIndex(vNameEffectEntries, s_aNameEffectEditName);
				if(ExistingIndex >= 0)
				{
					vNameEffectEntries.erase(vNameEffectEntries.begin() + ExistingIndex);
					MaNameEffectEncodeEntries(vNameEffectEntries);
				}
			}
			else
			{
				MaNameEffectSaveEntry(vNameEffectEntries, MaNameEffectMakeEntry(s_aNameEffectEditName, s_NameEffectEditStyle, s_NameEffectEditColor1, s_NameEffectEditColor2, s_NameEffectEditGlow, s_NameEffectEditMoving, s_NameEffectEditStars));
			}
			RememberNameEffectSaved();
		};

		auto NameEffectEditChangedSinceSaved = [&]() {
			return s_NameEffectSavedOwn != s_NameEffectEditOwn ||
			       str_comp(s_aNameEffectSavedName, s_aNameEffectEditName) != 0 ||
			       s_NameEffectSavedStyle != s_NameEffectEditStyle ||
			       s_NameEffectSavedColor1 != s_NameEffectEditColor1 ||
			       s_NameEffectSavedColor2 != s_NameEffectEditColor2 ||
			       s_NameEffectSavedGlow != s_NameEffectEditGlow ||
			       s_NameEffectSavedMoving != (s_NameEffectEditMoving ? 1 : 0) ||
			       s_NameEffectSavedStars != (s_NameEffectEditStars ? 1 : 0);
		};

		if(!s_NameEffectEditLoaded)
		{
			const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
			if(LocalClientId >= 0 && LocalClientId < MAX_CLIENTS && GameClient()->m_Snap.m_apPlayerInfos[LocalClientId])
				LoadNameEffectPlayer(GameClient()->m_aClients[LocalClientId].m_aName, true);
			else if(!vNameEffectEntries.empty())
				LoadNameEffectPlayer(vNameEffectEntries[0].m_aName, false);
			else
			{
				const char *pOwnName = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
				LoadNameEffectPlayer(pOwnName, true);
			}
		}

		std::vector<SMaNameEffectPlayerRow> vRows;
		auto AddPlayerRow = [&](const char *pName, const char *pClan, bool Configured) {
			if(!pName || pName[0] == '\0')
				return;
			for(SMaNameEffectPlayerRow &RowData : vRows)
			{
				if(str_comp_nocase(RowData.m_aName, pName) == 0)
				{
					RowData.m_Configured = RowData.m_Configured || Configured;
					return;
				}
			}
			SMaNameEffectPlayerRow RowData;
			str_copy(RowData.m_aName, pName, sizeof(RowData.m_aName));
			str_copy(RowData.m_aClan, pClan ? pClan : "", sizeof(RowData.m_aClan));
			RowData.m_ClientId = FindOnlineClientId(RowData.m_aName);
			RowData.m_Online = RowData.m_ClientId >= 0;
			RowData.m_Configured = Configured;
			if(RowData.m_Online && RowData.m_aClan[0] == '\0')
				str_copy(RowData.m_aClan, GameClient()->m_aClients[RowData.m_ClientId].m_aClan, sizeof(RowData.m_aClan));
			vRows.push_back(RowData);
		};

		for(const SMaNameEffectMenuEntry &Entry : vNameEffectEntries)
			AddPlayerRow(Entry.m_aName, "", true);
		if(GameClient()->Friends())
		{
			for(int FriendIndex = 0; FriendIndex < GameClient()->Friends()->NumFriends(); ++FriendIndex)
			{
				const CFriendInfo *pFriend = GameClient()->Friends()->GetFriend(FriendIndex);
				if(pFriend && pFriend->m_aName[0] != '\0')
					AddPlayerRow(pFriend->m_aName, pFriend->m_aClan, MaNameEffectFindEntryIndex(vNameEffectEntries, pFriend->m_aName) >= 0);
			}
		}
		std::stable_sort(vRows.begin(), vRows.end(), [](const SMaNameEffectPlayerRow &A, const SMaNameEffectPlayerRow &B) {
			if(A.m_Configured != B.m_Configured)
				return A.m_Configured > B.m_Configured;
			if(A.m_Online != B.m_Online)
				return A.m_Online > B.m_Online;
			return str_comp_nocase(A.m_aName, B.m_aName) < 0;
		});

		LeftView.HSplitTop(Margin, nullptr, &LeftView);
		RightView.HSplitTop(Margin, nullptr, &RightView);

		s_SectionBoxes.push_back(LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, TCLocalize("Jugadores"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaNameEffects, TCLocalize("Activar efectos de nombres"), &g_Config.m_MaNameEffects, &LeftView, LineSize);

		CUIRect PlayerList, PlayerSearch;
		LeftView.HSplitTop(220.0f, &PlayerList, &LeftView);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
		LeftView.HSplitTop(25.0f, &PlayerSearch, &LeftView);
		static CLineInputBuffered<128> s_NameEffectsPlayerSearchInput;
		Ui()->DoEditBox_Search(&s_NameEffectsPlayerSearchInput, &PlayerSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

		std::vector<SMaNameEffectPlayerRow> vFilteredRows;
		for(const SMaNameEffectPlayerRow &RowData : vRows)
		{
			const char *pSearch = s_NameEffectsPlayerSearchInput.GetString();
			if(pSearch[0] != '\0' && !str_find_nocase(RowData.m_aName, pSearch) && !str_find_nocase(RowData.m_aClan, pSearch))
				continue;
			vFilteredRows.push_back(RowData);
		}

		int SelectedOldPlayer = -1;
		for(size_t i = 0; i < vFilteredRows.size(); ++i)
		{
			if(str_comp_nocase(vFilteredRows[i].m_aName, s_aNameEffectEditName) == 0)
			{
				SelectedOldPlayer = (int)i;
				break;
			}
		}
		static CListBox s_NameEffectsPlayerListBox;
		s_NameEffectsPlayerListBox.DoStart(36.0f, vFilteredRows.size(), 1, 2, SelectedOldPlayer, &PlayerList, true, IGraphics::CORNER_ALL, true);
		static std::vector<unsigned char> s_vNameEffectPlayerItemIds;
		s_vNameEffectPlayerItemIds.resize(std::max<size_t>(1, vFilteredRows.size()));
		for(size_t i = 0; i < vFilteredRows.size(); ++i)
		{
			const SMaNameEffectPlayerRow &RowData = vFilteredRows[i];
			const CListboxItem Item = s_NameEffectsPlayerListBox.DoNextItem(&s_vNameEffectPlayerItemIds[i], SelectedOldPlayer == (int)i);
			if(!Item.m_Visible)
				continue;

			CUIRect RowRect, IconRect, TextRect, StatusRect;
			Item.m_Rect.Margin(2.0f, &RowRect);
			RowRect.VSplitLeft(28.0f, &IconRect, &RowRect);
			RowRect.VSplitRight(54.0f, &TextRect, &StatusRect);
			if(RowData.m_Online && RowData.m_ClientId >= 0)
			{
				CTeeRenderInfo TeeInfo = GameClient()->m_aClients[RowData.m_ClientId].m_RenderInfo;
				TeeInfo.m_Size = 24.0f;
				RenderTeeCute(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), IconRect.Center() + vec2(-1.0f, 2.0f), true);
			}
			else
				RenderDevSkin(IconRect.Center(), 24.0f, "default", "default", false, 0, 0, 0, false, false);

			CUIRect NameRect, ClanRect;
			TextRect.HSplitMid(&NameRect, &ClanRect, 0.0f);
			const int RowEntryIndex = MaNameEffectFindEntryIndex(vNameEffectEntries, RowData.m_aName);
			const SMaNameEffectMenuEntry *pRowEntry = RowEntryIndex >= 0 ? &vNameEffectEntries[RowEntryIndex] : nullptr;
			RenderNameEffectMenuLabel(NameRect, RowData.m_aName, pRowEntry, false, StandardFontSize, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), TEXTALIGN_ML);
			TextRender()->TextColor(RowData.m_Configured ? ColorRGBA(1.0f, 0.65f, 1.0f, 1.0f) : ColorRGBA(0.8f, 0.8f, 0.8f, 0.78f));
			Ui()->DoLabel(&ClanRect, RowData.m_Configured ? TCLocalize("personalizado") : RowData.m_aClan, 11.0f, TEXTALIGN_ML);
			TextRender()->TextColor(RowData.m_Online ? ColorRGBA(0.25f, 1.0f, 0.35f, 1.0f) : ColorRGBA(0.55f, 0.55f, 0.55f, 0.9f));
			RenderFontIcon(StatusRect, FontIcon::CIRCLE, 10.0f, TEXTALIGN_MC);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		const int NewSelectedPlayer = s_NameEffectsPlayerListBox.DoEnd();
		if(NewSelectedPlayer >= 0 && NewSelectedPlayer < (int)vFilteredRows.size() && NewSelectedPlayer != SelectedOldPlayer)
			LoadNameEffectPlayer(vFilteredRows[NewSelectedPlayer].m_aName, false);
		LeftView.HSplitTop(MarginExtraSmall, nullptr, &LeftView);
		s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

		// Bloque rapido para seleccionar el jugador local que usa el cliente.
		LeftView.HSplitTop(MarginBetweenSections * 0.45f, nullptr, &LeftView);
		s_SectionBoxes.push_back(LeftView);
		LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
		Ui()->DoLabel(&Label, TCLocalize("Yo"), HeadlineFontSize, TEXTALIGN_ML);
		LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

		CUIRect OwnRow;
		LeftView.HSplitTop(46.0f, &OwnRow, &LeftView);
		OwnRow.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.22f), IGraphics::CORNER_ALL, 6.0f);
		OwnRow.Margin(5.0f, &OwnRow);
		CUIRect OwnIcon, OwnText, OwnButton;
		OwnRow.VSplitLeft(38.0f, &OwnIcon, &OwnRow);
		OwnRow.VSplitRight(150.0f, &OwnText, &OwnButton);

		const int LocalClientId = GameClient()->m_Snap.m_LocalClientId;
		if(LocalClientId >= 0 && LocalClientId < MAX_CLIENTS && GameClient()->m_Snap.m_apPlayerInfos[LocalClientId])
		{
			CTeeRenderInfo TeeInfo = GameClient()->m_aClients[LocalClientId].m_RenderInfo;
			TeeInfo.m_Size = 30.0f;
			RenderTeeCute(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), OwnIcon.Center() + vec2(-1.0f, 2.5f), true);
		}
		else
			RenderDevSkin(OwnIcon.Center(), 30.0f, "default", "default", false, 0, 0, 0, false, false);

		CUIRect OwnName, OwnInfo;
		OwnText.HSplitMid(&OwnName, &OwnInfo, 0.0f);
		const char *pOwnName = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
		RenderNameEffectMenuLabel(OwnName, pOwnName, nullptr, true, StandardFontSize, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), TEXTALIGN_ML);
		TextRender()->TextColor(ColorRGBA(0.85f, 0.85f, 0.85f, 0.78f));
		Ui()->DoLabel(&OwnInfo, TCLocalize("jugador local"), 11.0f, TEXTALIGN_ML);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		static CButtonContainer s_NameEffectOwnSelectButton;
		if(DoButton_Menu(&s_NameEffectOwnSelectButton, TCLocalize("Seleccionar mi nick"), 0, &OwnButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
			LoadNameEffectPlayer(pOwnName, true);

		LeftView.HSplitTop(MarginExtraSmall, nullptr, &LeftView);
		s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

		s_SectionBoxes.push_back(RightView);
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, TCLocalize("Mi nombre personal"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		static CLineInput s_MaPersonalNameInput;
		s_MaPersonalNameInput.SetBuffer(g_Config.m_PlayerName, sizeof(g_Config.m_PlayerName));
		s_MaPersonalNameInput.SetEmptyText(Client()->PlayerName());
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Button.VSplitLeft(110.0f, &Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("Nombre"), FontSize, TEXTALIGN_ML);
		if(Ui()->DoEditBox(&s_MaPersonalNameInput, &Button, EditBoxFontSize))
		{
			SetNeedSendInfo();
			if(s_NameEffectEditOwn)
			{
				const char *pUpdatedOwnName = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
				str_copy(s_aNameEffectEditName, pUpdatedOwnName, sizeof(s_aNameEffectEditName));
				MaNameEffectSanitizeName(s_aNameEffectEditName);
				RememberNameEffectSaved();
			}
		}

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		static CButtonContainer s_NameEffectLoadOwnButton;
		if(DoButton_Menu(&s_NameEffectLoadOwnButton, TCLocalize("Editar efectos de mi nick"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
		{
			const char *pOwnName = g_Config.m_PlayerName[0] ? g_Config.m_PlayerName : Client()->PlayerName();
			LoadNameEffectPlayer(pOwnName, true);
		}
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaNameEffectsOwn, TCLocalize("Activar efectos en mi nick"), &g_Config.m_MaNameEffectsOwn, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClNamePlatesOwn, TCLocalize("Mostrar mi nombre sobre el tee"), &g_Config.m_ClNamePlatesOwn, &RightView, LineSize);

		RightView.HSplitTop(MarginBetweenSections * 0.45f, nullptr, &RightView);
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, TCLocalize("Editar jugador"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		static CLineInput s_NameEffectNameInput;
		s_NameEffectNameInput.SetBuffer(s_aNameEffectEditName, sizeof(s_aNameEffectEditName));
		s_NameEffectNameInput.SetEmptyText(TCLocalize("Jugador"));
		RightView.HSplitTop(LineSize, &Button, &RightView);
		bool NameEffectNameInputActive = false;
		if(s_NameEffectEditOwn)
		{
			Button.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.22f), IGraphics::CORNER_ALL, 5.0f);
			Ui()->DoLabel(&Button, s_aNameEffectEditName[0] ? s_aNameEffectEditName : TCLocalize("Mi nick local"), EditBoxFontSize, TEXTALIGN_MC);
		}
		else
		{
			Ui()->DoEditBox(&s_NameEffectNameInput, &Button, EditBoxFontSize);
			MaNameEffectSanitizeName(s_aNameEffectEditName);
			NameEffectNameInputActive = s_NameEffectNameInput.IsActive();
		}

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Button.VSplitLeft(120.0f, &Label, &Button);
		Ui()->DoLabel(&Label, TCLocalize("Estilo"), FontSize, TEXTALIGN_ML);
		static std::vector<const char *> s_NameEffectStyleNames;
		s_NameEffectStyleNames = {TCLocalize("Letras arcoiris"), TCLocalize("Neon"), TCLocalize("Degradado doble"), TCLocalize("Arcoiris en movimiento"), TCLocalize("Neon pulsante"), TCLocalize("Glitch hacker"), TCLocalize("Fuego"), TCLocalize("Hielo"), TCLocalize("Electricidad"), TCLocalize("Galaxy"), TCLocalize("Oro brillante"), TCLocalize("Sombra / outline"), TCLocalize("Latido"), TCLocalize("Pixel arcade")};
		static CUi::SDropDownState s_NameEffectStyleState;
		static CScrollRegion s_NameEffectStyleScrollRegion;
		s_NameEffectStyleState.m_SelectionPopupContext.m_pScrollRegion = &s_NameEffectStyleScrollRegion;
		s_NameEffectEditStyle = Ui()->DoDropDown(&Button, std::clamp(s_NameEffectEditStyle, 0, MaNameEffects::STYLE_MAX), s_NameEffectStyleNames.data(), s_NameEffectStyleNames.size(), s_NameEffectStyleState);
		DoButton_CheckBoxAutoVMarginAndSet(&s_NameEffectEditStars, TCLocalize("Estrellas a los lados"), &s_NameEffectEditStars, &RightView, LineSize);

		DoButton_CheckBoxAutoVMarginAndSet(&s_NameEffectEditMoving, TCLocalize("Movimiento de colores"), &s_NameEffectEditMoving, &RightView, LineSize);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&s_NameEffectEditGlow, &s_NameEffectEditGlow, &Button, TCLocalize("Intensidad neon"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		static CButtonContainer s_NameEffectEditColor1Picker, s_NameEffectEditColor2Picker;
		DoNameEffectColorPicker(&s_NameEffectEditColor1Picker, TCLocalize("Color principal"), &s_NameEffectEditColor1, ColorRGBA(0.5f, 1.0f, 1.0f));
		DoNameEffectColorPicker(&s_NameEffectEditColor2Picker, TCLocalize("Color efecto"), &s_NameEffectEditColor2, ColorRGBA(1.0f, 0.4f, 1.0f));

		if(!NameEffectNameInputActive && s_aNameEffectEditName[0] != '\0' && NameEffectEditChangedSinceSaved())
		{
			SaveCurrentNameEffect();
		}

		RightView.HSplitTop(LineSize + MarginSmall, &Button, &RightView);
		Button.VSplitMid(&Button, &Label, MarginSmall);
		static CButtonContainer s_NameEffectSaveButton, s_NameEffectDeleteButton;
		if(DoButton_Menu(&s_NameEffectSaveButton, s_NameEffectEditOwn ? TCLocalize("Guardar mi nick") : TCLocalize("Guardar jugador"), s_aNameEffectEditName[0] ? 0 : -1, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && s_aNameEffectEditName[0])
		{
			SaveCurrentNameEffect();
		}
		const int DeleteEnabled = s_NameEffectEditOwn ? g_Config.m_MaNameEffectsOwn : (MaNameEffectFindEntryIndex(vNameEffectEntries, s_aNameEffectEditName) >= 0 ? 1 : 0);
		if(DoButton_Menu(&s_NameEffectDeleteButton, s_NameEffectEditOwn ? TCLocalize("Desactivar") : TCLocalize("Eliminar"), DeleteEnabled ? 0 : -1, &Label, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
		{
			if(s_NameEffectEditOwn)
			{
				g_Config.m_MaNameEffectsOwn = 0;
				RememberNameEffectSaved();
			}
			else
			{
				const int ExistingIndex = MaNameEffectFindEntryIndex(vNameEffectEntries, s_aNameEffectEditName);
				if(ExistingIndex >= 0)
				{
					vNameEffectEntries.erase(vNameEffectEntries.begin() + ExistingIndex);
					MaNameEffectEncodeEntries(vNameEffectEntries);
					RememberNameEffectSaved();
				}
			}
		}

		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Button.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.22f), IGraphics::CORNER_ALL, 5.0f);
		TextRender()->TextColor(MaNameEffectMenuColor(s_NameEffectEditColor1));
		char aPreview[96];
		str_format(aPreview, sizeof(aPreview), "%s%s%s%s%s", s_NameEffectEditStars ? "* " : "", s_NameEffectEditStyle == 13 ? "[" : "", s_aNameEffectEditName[0] ? s_aNameEffectEditName : TCLocalize("Jugador"), s_NameEffectEditStyle == 13 ? "]" : "", s_NameEffectEditStars ? " *" : "");
		Ui()->DoLabel(&Button, aPreview, FontSize, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		RightView.HSplitTop(MarginExtraSmall, nullptr, &RightView);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		TextRender()->TextColor(ColorRGBA(1.0f, 0.45f, 0.45f, 1.0f));
		Ui()->DoLabel(&Button, TCLocalize("Para que se vea el color reinicia"), 12.0f, TEXTALIGN_MC);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		RightView.HSplitTop(MarginBetweenSections * 0.35f, nullptr, &RightView);
		RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
		Ui()->DoLabel(&Label, TCLocalize("Jugadores en linea"), HeadlineFontSize, TEXTALIGN_ML);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);

		CUIRect OnlineList, OnlineSearch;
		RightView.HSplitTop(105.0f, &OnlineList, &RightView);
		RightView.HSplitTop(MarginSmall, nullptr, &RightView);
		RightView.HSplitTop(25.0f, &OnlineSearch, &RightView);
		static CLineInputBuffered<128> s_NameEffectsOnlineSearchInput;
		Ui()->DoEditBox_Search(&s_NameEffectsOnlineSearchInput, &OnlineSearch, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

		std::vector<int> vOnlineClientIds;
		for(int ClientId = 0; ClientId < MAX_CLIENTS; ++ClientId)
		{
			if(!GameClient()->m_Snap.m_apPlayerInfos[ClientId])
				continue;
			const auto &Client = GameClient()->m_aClients[ClientId];
			const char *pSearch = s_NameEffectsOnlineSearchInput.GetString();
			if(pSearch[0] != '\0' && !str_find_nocase(Client.m_aName, pSearch) && !str_find_nocase(Client.m_aClan, pSearch))
				continue;
			vOnlineClientIds.push_back(ClientId);
		}

		static CListBox s_NameEffectsOnlineListBox;
		s_NameEffectsOnlineListBox.DoStart(30.0f, vOnlineClientIds.size(), 1, 2, -1, &OnlineList, true, IGraphics::CORNER_ALL, true);
		static std::vector<unsigned char> s_vNameEffectOnlineItemIds;
		static std::vector<CButtonContainer> s_vNameEffectOnlineButtons;
		s_vNameEffectOnlineItemIds.resize(std::max<size_t>(1, vOnlineClientIds.size()));
		s_vNameEffectOnlineButtons.resize(MAX_CLIENTS);
		for(size_t i = 0; i < vOnlineClientIds.size(); ++i)
		{
			const int ClientId = vOnlineClientIds[i];
			const auto &Client = GameClient()->m_aClients[ClientId];
			const CListboxItem Item = s_NameEffectsOnlineListBox.DoNextItem(&s_vNameEffectOnlineItemIds[i], false);
			if(!Item.m_Visible)
				continue;

			CUIRect RowRect, TeeRect, NameRect, TagRect;
			Item.m_Rect.Margin(2.0f, &RowRect);
			RowRect.VSplitLeft(28.0f, &TeeRect, &RowRect);
			RowRect.VSplitRight(80.0f, &NameRect, &TagRect);
			const ColorRGBA ButtonColor = Ui()->HotItem() == &s_vNameEffectOnlineButtons[ClientId] ? ColorRGBA(1.0f, 1.0f, 1.0f, 0.22f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.0f);
			RowRect.Draw(ButtonColor, IGraphics::CORNER_ALL, 5.0f);
			if(Ui()->DoButtonLogic(&s_vNameEffectOnlineButtons[ClientId], false, &RowRect, BUTTONFLAG_LEFT))
				LoadNameEffectPlayer(Client.m_aName, false);

			CTeeRenderInfo TeeInfo = Client.m_RenderInfo;
			TeeInfo.m_Size = 24.0f;
			RenderTeeCute(CAnimState::GetIdle(), &TeeInfo, 0, vec2(1.0f, 0.0f), TeeRect.Center() + vec2(-1.0f, 2.0f), true);
			const bool OnlineOwn = ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1];
			const int OnlineEntryIndex = MaNameEffectFindEntryIndex(vNameEffectEntries, Client.m_aName);
			const SMaNameEffectMenuEntry *pOnlineEntry = OnlineEntryIndex >= 0 ? &vNameEffectEntries[OnlineEntryIndex] : nullptr;
			RenderNameEffectMenuLabel(NameRect, Client.m_aName, pOnlineEntry, OnlineOwn, StandardFontSize, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f), TEXTALIGN_ML);
			TextRender()->TextColor(MaNameEffectFindEntryIndex(vNameEffectEntries, Client.m_aName) >= 0 ? ColorRGBA(1.0f, 0.65f, 1.0f, 1.0f) : ColorRGBA(0.75f, 0.75f, 0.75f, 0.75f));
			Ui()->DoLabel(&TagRect, MaNameEffectFindEntryIndex(vNameEffectEntries, Client.m_aName) >= 0 ? TCLocalize("listo") : TCLocalize("agregar"), 11.0f, TEXTALIGN_MR);
			TextRender()->TextColor(TextRender()->DefaultTextColor());
		}
		s_NameEffectsOnlineListBox.DoEnd();

		RightView.HSplitTop(MarginExtraSmall, nullptr, &RightView);
		s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;
	}

	CUIRect ScrollRegion;
	ScrollRegion.x = MainView.x;
	ScrollRegion.w = MainView.w;
	ScrollRegion.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}
void CMenus::RenderMaConfiguracion(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	// ===== LEFT COLUMN =====

	// ***** Optimizer ***** //
	LeftView.HSplitTop(Margin, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Optimizer"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizer, TCLocalize("Enable optimizer"), &g_Config.m_MaOptimizer, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaPerformanceGuard, TCLocalize("Proteccion automatica de FPS"), &g_Config.m_MaPerformanceGuard, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaRenderCompatibility, TCLocalize("Compatibilidad de render"), &g_Config.m_MaRenderCompatibility, &LeftView, LineSize);
	if(g_Config.m_MaPerformanceGuard)
	{
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaPerformanceGuardTargetFps, &g_Config.m_MaPerformanceGuardTargetFps, &Button, TCLocalize("Objetivo FPS"), 60, 1000);
		LeftView.HSplitTop(LineSize, &Button, &LeftView);
		Ui()->DoScrollbarOption(&g_Config.m_MaPerformanceGuardMax3dParticles, &g_Config.m_MaPerformanceGuardMax3dParticles, &Button, TCLocalize("Maximo particulas 3D"), 10, 200);
	}
	if(g_Config.m_MaOptimizer)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerFpsFog, TCLocalize("FPS Fog"), &g_Config.m_MaOptimizerFpsFog, &LeftView, LineSize);
		if(g_Config.m_MaOptimizerFpsFog)
		{
			// FPS fog mode selector
			LeftView.HSplitTop(LineSize, &Button, &LeftView);
			Ui()->DoScrollbarOption(&g_Config.m_MaOptimizerFpsFogMode, &g_Config.m_MaOptimizerFpsFogMode, &Button, TCLocalize("Fog mode"), 0, 1, &CUi::ms_LinearScrollbarScale, 0, " (0=Manual/1=Zoom)");

			if(g_Config.m_MaOptimizerFpsFogMode == 0)
			{
				LeftView.HSplitTop(LineSize, &Button, &LeftView);
				Ui()->DoScrollbarOption(&g_Config.m_MaOptimizerFpsFogRadiusTiles, &g_Config.m_MaOptimizerFpsFogRadiusTiles, &Button, TCLocalize("Radius (tiles)"), 5, 300);
			}
			else
			{
				LeftView.HSplitTop(LineSize, &Button, &LeftView);
				Ui()->DoScrollbarOption(&g_Config.m_MaOptimizerFpsFogZoomPercent, &g_Config.m_MaOptimizerFpsFogZoomPercent, &Button, TCLocalize("Zoom percent"), 10, 120);
			}
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerFpsFogRenderRect, TCLocalize("Show fog rectangle"), &g_Config.m_MaOptimizerFpsFogRenderRect, &LeftView, LineSize);
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerFpsFogCullMapTiles, TCLocalize("Cull map tiles"), &g_Config.m_MaOptimizerFpsFogCullMapTiles, &LeftView, LineSize);
		}
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerNoParticles, TCLocalize("Disable all particles"), &g_Config.m_MaOptimizerNoParticles, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerHighPriority, TCLocalize("High priority process"), &g_Config.m_MaOptimizerHighPriority, &LeftView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaOptimizerDiscordPriorityBelowNormal, TCLocalize("Discord low priority"), &g_Config.m_MaOptimizerDiscordPriorityBelowNormal, &LeftView, LineSize);
	}
	else
		LeftView.HSplitTop(LineSize * 4.0f, nullptr, &LeftView);
	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Gores Mode ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Gores Mode"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGoresMode, TCLocalize("Enable Gores Mode"), &g_Config.m_MaGoresMode, &LeftView, LineSize);
	if(g_Config.m_MaGoresMode)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGoresModeDisableIfWeapons, TCLocalize("Disable if have weapons"), &g_Config.m_MaGoresModeDisableIfWeapons, &LeftView, LineSize);
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaGoresAuto, TCLocalize("Auto-detect gores servers"), &g_Config.m_MaGoresAuto, &LeftView, LineSize);
	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Auto Reply ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Auto Reply"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReplyMuted, TCLocalize("Auto reply to muted players"), &g_Config.m_TcAutoReplyMuted, &LeftView, LineSize);
	CUIRect MutedReply;
	LeftView.HSplitTop(LineSize + MarginExtraSmall, &MutedReply, &LeftView);
	if(g_Config.m_TcAutoReplyMuted)
	{
		MutedReply.HSplitTop(MarginExtraSmall, nullptr, &MutedReply);
		static CLineInput s_MutedReply(g_Config.m_TcAutoReplyMutedMessage, sizeof(g_Config.m_TcAutoReplyMutedMessage));
		s_MutedReply.SetEmptyText("I have muted you");
		Ui()->DoEditBox(&s_MutedReply, &MutedReply, EditBoxFontSize);
	}
	LeftView.HSplitTop(MarginExtraSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReplyMinimized, TCLocalize("Auto reply when tabbed out"), &g_Config.m_TcAutoReplyMinimized, &LeftView, LineSize);
	CUIRect MinimizedReply;
	LeftView.HSplitTop(LineSize + MarginExtraSmall, &MinimizedReply, &LeftView);
	if(g_Config.m_TcAutoReplyMinimized)
	{
		MinimizedReply.HSplitTop(MarginExtraSmall, nullptr, &MinimizedReply);
		static CLineInput s_MinimizedReply(g_Config.m_TcAutoReplyMinimizedMessage, sizeof(g_Config.m_TcAutoReplyMinimizedMessage));
		s_MinimizedReply.SetEmptyText("I am not tabbed in");
		Ui()->DoEditBox(&s_MinimizedReply, &MinimizedReply, EditBoxFontSize);
	}
	LeftView.HSplitTop(MarginExtraSmall, nullptr, &LeftView);
	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;

	// ***** Auto-Reactions ***** //
	LeftView.HSplitTop(MarginBetweenSections, nullptr, &LeftView);
	s_SectionBoxes.push_back(LeftView);
	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Auto-Reactions"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReactFinish, TCLocalize("Finish"), &g_Config.m_TcAutoReactFinish, &LeftView, LineSize);
	if(g_Config.m_TcAutoReactFinish)
	{
		static CLineInput s_MaAutoReactFinishInput(g_Config.m_TcAutoReactFinishMsg, sizeof(g_Config.m_TcAutoReactFinishMsg));
		s_MaAutoReactFinishInput.SetEmptyText("gg");
		CUIRect EditBox;
		LeftView.HSplitTop(LineSize, &EditBox, &LeftView);
		EditBox.VSplitRight(100.0f, &EditBox, nullptr);
		Ui()->DoEditBox(&s_MaAutoReactFinishInput, &EditBox, EditBoxFontSize);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReactPB, TCLocalize("Personal Best"), &g_Config.m_TcAutoReactPB, &LeftView, LineSize);
	if(g_Config.m_TcAutoReactPB)
	{
		static CLineInput s_MaAutoReactPBInput(g_Config.m_TcAutoReactPBMsg, sizeof(g_Config.m_TcAutoReactPBMsg));
		s_MaAutoReactPBInput.SetEmptyText("PB!");
		CUIRect EditBox;
		LeftView.HSplitTop(LineSize, &EditBox, &LeftView);
		EditBox.VSplitRight(100.0f, &EditBox, nullptr);
		Ui()->DoEditBox(&s_MaAutoReactPBInput, &EditBox, EditBoxFontSize);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReactDeath, TCLocalize("Death"), &g_Config.m_TcAutoReactDeath, &LeftView, LineSize);
	if(g_Config.m_TcAutoReactDeath)
	{
		static CLineInput s_MaAutoReactDeathInput(g_Config.m_TcAutoReactDeathMsg, sizeof(g_Config.m_TcAutoReactDeathMsg));
		s_MaAutoReactDeathInput.SetEmptyText("rip");
		CUIRect EditBox;
		LeftView.HSplitTop(LineSize, &EditBox, &LeftView);
		EditBox.VSplitRight(100.0f, &EditBox, nullptr);
		Ui()->DoEditBox(&s_MaAutoReactDeathInput, &EditBox, EditBoxFontSize);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAutoReactStart, TCLocalize("Race start"), &g_Config.m_TcAutoReactStart, &LeftView, LineSize);
	if(g_Config.m_TcAutoReactStart)
	{
		static CLineInput s_MaAutoReactStartInput(g_Config.m_TcAutoReactStartMsg, sizeof(g_Config.m_TcAutoReactStartMsg));
		s_MaAutoReactStartInput.SetEmptyText("gl");
		CUIRect EditBox;
		LeftView.HSplitTop(LineSize, &EditBox, &LeftView);
		EditBox.VSplitRight(100.0f, &EditBox, nullptr);
		Ui()->DoEditBox(&s_MaAutoReactStartInput, &EditBox, EditBoxFontSize);
	}

	{
		static std::vector<const char *> s_MaAutoReactEmoteDropDownNames;
		s_MaAutoReactEmoteDropDownNames = {"Oop", "!", "Hearts", "Drop", "DotDot", "Music", "Sorry", "Ghost", "Sushi", "Splattee", "Deviltee", "Zomg", "Zzz", "Wtf", "Eyes", "Question"};
		static CUi::SDropDownState s_MaAutoReactEmoteDropDownState;
		static CScrollRegion s_MaAutoReactEmoteDropDownScrollRegion;
		s_MaAutoReactEmoteDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_MaAutoReactEmoteDropDownScrollRegion;
		CUIRect DropDownRect;
		LeftView.HSplitTop(LineSize, &DropDownRect, &LeftView);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Emote"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAutoReactEmote = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAutoReactEmote, s_MaAutoReactEmoteDropDownNames.data(), s_MaAutoReactEmoteDropDownNames.size(), s_MaAutoReactEmoteDropDownState);
	}

	LeftView.HSplitTop(MarginExtraSmall, nullptr, &LeftView);
	s_SectionBoxes.back().h = LeftView.y - s_SectionBoxes.back().y;


	// ===== RIGHT COLUMN =====

	// ***** Input / Snap Tap ***** //
	RightView.HSplitTop(Margin, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Input"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInput, TCLocalize("Enable Inputs"), &g_Config.m_TcFastInput, &RightView, LineSize);
	if(g_Config.m_TcFastInput)
	{
		// Mode selector
		RightView.HSplitTop(LineSize, &Button, &RightView);
		static CButtonContainer s_FastInputModeFast, s_FastInputModeBest, s_FastInputModeSaikoPlus, s_FastInputModeMa;
		const int UiFastInputMode = g_Config.m_MaFastInputMode == 2 ? 3 : g_Config.m_MaFastInputMode;
		CUIRect ModeBtn;
		const float BtnW = Button.w / 4.0f;
		Button.VSplitLeft(BtnW, &ModeBtn, &Button);
		if(DoButton_Menu(&s_FastInputModeFast, TCLocalize("Fast input"), UiFastInputMode == 0, &ModeBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
			g_Config.m_MaFastInputMode = 0;
		Button.VSplitLeft(BtnW, &ModeBtn, &Button);
		if(DoButton_Menu(&s_FastInputModeBest, TCLocalize("Best input"), UiFastInputMode == 3, &ModeBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
			g_Config.m_MaFastInputMode = 3;
		Button.VSplitLeft(BtnW, &ModeBtn, &Button);
		if(DoButton_Menu(&s_FastInputModeSaikoPlus, "Saiko+", UiFastInputMode == 4, &ModeBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
			g_Config.m_MaFastInputMode = 4;
		if(DoButton_Menu(&s_FastInputModeMa, "MA", UiFastInputMode == 5, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
			g_Config.m_MaFastInputMode = 5;

		// Per-mode settings
		if(g_Config.m_MaFastInputMode == 0)
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_TcFastInputAmount, &g_Config.m_TcFastInputAmount, &Button, TCLocalize("Amount"), 0, 40, &CUi::ms_LinearScrollbarScale, 0, "ms");
		}
		else if(g_Config.m_MaFastInputMode == 4)
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaSaikoPlusAmount, &g_Config.m_MaSaikoPlusAmount, &Button, "Saiko+ amount", 0, 500, &CUi::ms_LinearScrollbarScale, 0, " (0.01 ticks)");
		}
		else if(g_Config.m_MaFastInputMode == 5)
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			static CButtonContainer s_MaInputProfileAuto, s_MaInputProfileSmooth, s_MaInputProfileBalanced, s_MaInputProfileAggressive;
			CUIRect ProfileBtn;
			const float ProfileBtnW = Button.w / 4.0f;
			Button.VSplitLeft(ProfileBtnW, &ProfileBtn, &Button);
			if(DoButton_Menu(&s_MaInputProfileAuto, "Auto", g_Config.m_MaInputProfile == 0, &ProfileBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_L))
				g_Config.m_MaInputProfile = 0;
			Button.VSplitLeft(ProfileBtnW, &ProfileBtn, &Button);
			if(DoButton_Menu(&s_MaInputProfileSmooth, TCLocalize("Suave"), g_Config.m_MaInputProfile == 1, &ProfileBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				g_Config.m_MaInputProfile = 1;
			Button.VSplitLeft(ProfileBtnW, &ProfileBtn, &Button);
			if(DoButton_Menu(&s_MaInputProfileBalanced, TCLocalize("Medio"), g_Config.m_MaInputProfile == 2, &ProfileBtn, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_NONE))
				g_Config.m_MaInputProfile = 2;
			if(DoButton_Menu(&s_MaInputProfileAggressive, TCLocalize("Fuerte"), g_Config.m_MaInputProfile == 3, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_R))
				g_Config.m_MaInputProfile = 3;

			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaInputStrength, &g_Config.m_MaInputStrength, &Button, TCLocalize("Intensidad MA"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaInputStability, &g_Config.m_MaInputStability, &Button, TCLocalize("Estabilidad MA"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		}
		else
		{
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaBestInputOffset, &g_Config.m_MaBestInputOffset, &Button, TCLocalize("Prediction offset"), 0, 1000, &CUi::ms_LinearScrollbarScale, 0, " (0.01 ticks)");
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaBestInputSmoothing, &g_Config.m_MaBestInputSmoothing, &Button, TCLocalize("Smoothing"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaBestInputLatencyComp, &g_Config.m_MaBestInputLatencyComp, &Button, TCLocalize("Latency compensation"), 0, 50, &CUi::ms_LinearScrollbarScale, 0, "%");
			RightView.HSplitTop(LineSize, &Button, &RightView);
			Ui()->DoScrollbarOption(&g_Config.m_MaBestInputInterpolation, &g_Config.m_MaBestInputInterpolation, &Button, TCLocalize("Interpolation"), 1, 3, &CUi::ms_LinearScrollbarScale, 0, " (1=Lin/2=Cub/3=Smooth)");
		}

		// Others checkbox
		if(g_Config.m_MaFastInputMode == 0)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcFastInputOthers, TCLocalize("Fast Input others"), &g_Config.m_TcFastInputOthers, &RightView, LineSize);
		else if(g_Config.m_MaFastInputMode == 2 || g_Config.m_MaFastInputMode == 3)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaBestInputOthers, TCLocalize("Best input others"), &g_Config.m_MaBestInputOthers, &RightView, LineSize);
		else if(g_Config.m_MaFastInputMode == 4)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSaikoPlusOthers, "Saiko+ others", &g_Config.m_MaSaikoPlusOthers, &RightView, LineSize);
		else if(g_Config.m_MaFastInputMode == 5)
			DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaInputOthers, "MA others", &g_Config.m_MaInputOthers, &RightView, LineSize);
	}
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_ClSubTickAiming, TCLocalize("Sub-Tick aiming"), &g_Config.m_ClSubTickAiming, &RightView, LineSize);

	// Snap Tap subsection
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Snap Tap"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSnapTap, TCLocalize("Enable"), &g_Config.m_MaSnapTap, &RightView, LineSize);
	if(g_Config.m_MaSnapTap)
	{
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaSnapTapDelay, &g_Config.m_MaSnapTapDelay, &Button, TCLocalize("Delay"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "ms");
	}

	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Spectator Panel ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Espectador"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSpectatorPanel, TCLocalize("Ver quien te espectea"), &g_Config.m_MaSpectatorPanel, &RightView, LineSize);
	if(g_Config.m_MaSpectatorPanel)
	{
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSpectatorPanelShowNames, TCLocalize("Mostrar nombres"), &g_Config.m_MaSpectatorPanelShowNames, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSpectatorPanelOnlyWhenSpectated, TCLocalize("Solo mostrar cuando te espectean"), &g_Config.m_MaSpectatorPanelOnlyWhenSpectated, &RightView, LineSize);
		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_MaSpectatorPanelShowEmpty, TCLocalize("Mostrar aunque este vacio"), &g_Config.m_MaSpectatorPanelShowEmpty, &RightView, LineSize);

		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaSpectatorPanelMaxNames, &g_Config.m_MaSpectatorPanelMaxNames, &Button, TCLocalize("Max nombres"), 1, 64);
		RightView.HSplitTop(LineSize, &Button, &RightView);
		Ui()->DoScrollbarOption(&g_Config.m_MaSpectatorPanelOpacity, &g_Config.m_MaSpectatorPanelOpacity, &Button, TCLocalize("Opacidad"), 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		RightView.HSplitTop(LineSize + 6.0f, &Button, &RightView);
		static CButtonContainer s_MaSpectatorHudEditorButton;
		if(DoButton_Menu(&s_MaSpectatorHudEditorButton, TCLocalize("Abrir editor HUD"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
		{
			SetActive(false);
			GameClient()->m_HudEditor.Activate();
		}
	}
	else
		RightView.HSplitTop(LineSize * 3.0f, nullptr, &RightView);

	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	// ***** Custom Sounds ***** //
	RightView.HSplitTop(MarginBetweenSections, nullptr, &RightView);
	s_SectionBoxes.push_back(RightView);
	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Custom Sounds"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	{
		static std::vector<const char *> s_SoundPackDropDownNames;
		s_SoundPackDropDownNames = {TCLocalize("Classic"), TCLocalize("Anime"), TCLocalize("Cyber"), TCLocalize("Minimal")};
		static CUi::SDropDownState s_SoundPackDropDownState;
		static CScrollRegion s_SoundPackDropDownScrollRegion;
		s_SoundPackDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_SoundPackDropDownScrollRegion;
		CUIRect DropDownRect;
		RightView.HSplitTop(LineSize, &DropDownRect, &RightView);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Sound pack"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcSoundPack = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcSoundPack, s_SoundPackDropDownNames.data(), s_SoundPackDropDownNames.size(), s_SoundPackDropDownState);
	}

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSoundFriendJoin, TCLocalize("Friend join sound"), &g_Config.m_TcSoundFriendJoin, &RightView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSoundMapFinish, TCLocalize("Map finish sound"), &g_Config.m_TcSoundMapFinish, &RightView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcSoundHighlight, TCLocalize("Highlight sound"), &g_Config.m_TcSoundHighlight, &RightView, LineSize);
	RightView.HSplitTop(LineSize, &Button, &RightView);
	Ui()->DoScrollbarOption(&g_Config.m_TcSoundVolume, &g_Config.m_TcSoundVolume, &Button, TCLocalize("Sound volume"), 0, 200, &CUi::ms_LinearScrollbarScale, 0, "%");

	{
		static CButtonContainer s_TestFriendSound, s_TestFinishSound, s_TestHighlightSound;
		CUIRect Buttons, FriendButton, FinishButton, HighlightButton;
		RightView.HSplitTop(LineSize, &Buttons, &RightView);
		Buttons.VSplitMid(&FriendButton, &Buttons, MarginSmall);
		Buttons.VSplitMid(&FinishButton, &HighlightButton, MarginSmall);
		const float Volume = std::clamp(g_Config.m_TcSoundVolume / 100.0f, 0.0f, 2.0f);
		if(DoButtonLineSize_Menu(&s_TestFriendSound, TCLocalize("Test Friend"), 0, &FriendButton, LineSize))
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, TClientSoundIdForPack(g_Config.m_TcSoundPack, TCLIENT_SOUND_FRIEND_JOIN), Volume);
		if(DoButtonLineSize_Menu(&s_TestFinishSound, TCLocalize("Test Finish"), 0, &FinishButton, LineSize))
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, TClientSoundIdForPack(g_Config.m_TcSoundPack, TCLIENT_SOUND_MAP_FINISH), Volume);
		if(DoButtonLineSize_Menu(&s_TestHighlightSound, TCLocalize("Test Highlight"), 0, &HighlightButton, LineSize))
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, TClientSoundIdForPack(g_Config.m_TcSoundPack, TCLIENT_SOUND_HIGHLIGHT), Volume);
	}

	RightView.HSplitTop(MarginExtraSmall, nullptr, &RightView);
	s_SectionBoxes.back().h = RightView.y - s_SectionBoxes.back().y;

	CUIRect ScrollRect;
	ScrollRect.x = MainView.x;
	ScrollRect.y = maximum(LeftView.y, RightView.y) + MarginSmall * 2.0f;
	ScrollRect.w = MainView.w;
	ScrollRect.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRect);
	s_ScrollRegion.End();
}

void CMenus::RenderMaLluvia(CUIRect MainView)
{
	CUIRect Column, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	Column = MainView;

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Lluvia"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcWeatherParticles, TCLocalize("Enable weather particles"), &g_Config.m_TcWeatherParticles, &Column, LineSize);
	if(g_Config.m_TcWeatherParticles)
	{
		static std::vector<const char *> s_WeatherDropDownNames;
		s_WeatherDropDownNames = {TCLocalize("Snow"), TCLocalize("Rain"), TCLocalize("Stars"), TCLocalize("Particles")};
		static CUi::SDropDownState s_WeatherDropDownState;
		static CScrollRegion s_WeatherDropDownScrollRegion;
		s_WeatherDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_WeatherDropDownScrollRegion;
		CUIRect DropDownRect;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Particle type"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcWeatherMode = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcWeatherMode, s_WeatherDropDownNames.data(), s_WeatherDropDownNames.size(), s_WeatherDropDownState);

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherAmount, &g_Config.m_TcWeatherAmount, &Button, TCLocalize("Amount"), 1, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherSpeed, &g_Config.m_TcWeatherSpeed, &Button, TCLocalize("Speed"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherSize, &g_Config.m_TcWeatherSize, &Button, TCLocalize("Size"), 25, 300, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcWeatherAlpha, &g_Config.m_TcWeatherAlpha, &Button, TCLocalize("Opacity"), 5, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}

void CMenus::RenderMaAnimeLove(CUIRect MainView)
{
	CUIRect Column, Button, Label;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	Column = MainView;

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	Column.HSplitTop(Margin, nullptr, &Column);
	s_SectionBoxes.push_back(Column);
	Column.HSplitTop(HeadlineHeight, &Label, &Column);
	Ui()->DoLabel(&Label, TCLocalize("Anime Love"), HeadlineFontSize, TEXTALIGN_ML);
	Column.HSplitTop(MarginSmall, nullptr, &Column);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAnimeLove, TCLocalize("Enable Anime Love"), &g_Config.m_TcAnimeLove, &Column, LineSize);
	if(g_Config.m_TcAnimeLove)
	{
		CUIRect DropDownRect;

		static std::vector<const char *> s_AnimeSkinNames;
		s_AnimeSkinNames = {TCLocalize("Kurumi"), TCLocalize("Crimson"), TCLocalize("Midnight"), TCLocalize("Pastel")};
		static CUi::SDropDownState s_AnimeSkinDropDownState;
		static CScrollRegion s_AnimeSkinScrollRegion;
		s_AnimeSkinDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeSkinScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Character skin"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveCharacter = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveCharacter, s_AnimeSkinNames.data(), s_AnimeSkinNames.size(), s_AnimeSkinDropDownState);

		static std::vector<const char *> s_AnimeAnimationNames;
		s_AnimeAnimationNames = {TCLocalize("Wave"), TCLocalize("Walk"), TCLocalize("Mixed"), TCLocalize("Sit"), TCLocalize("Sleep"), TCLocalize("Celebrate"), TCLocalize("Follow tee")};
		static CUi::SDropDownState s_AnimeAnimationDropDownState;
		static CScrollRegion s_AnimeAnimationScrollRegion;
		s_AnimeAnimationDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeAnimationScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Animation type"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveAnimation = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveAnimation, s_AnimeAnimationNames.data(), s_AnimeAnimationNames.size(), s_AnimeAnimationDropDownState);

		static std::vector<const char *> s_AnimeVisibilityNames;
		s_AnimeVisibilityNames = {TCLocalize("Menu and ingame"), TCLocalize("Menu only"), TCLocalize("Ingame only")};
		static CUi::SDropDownState s_AnimeVisibilityDropDownState;
		static CScrollRegion s_AnimeVisibilityScrollRegion;
		s_AnimeVisibilityDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimeVisibilityScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Visibility"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLoveVisibility = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLoveVisibility, s_AnimeVisibilityNames.data(), s_AnimeVisibilityNames.size(), s_AnimeVisibilityDropDownState);

		static std::vector<const char *> s_AnimePositionNames;
		s_AnimePositionNames = {TCLocalize("Right"), TCLocalize("Left"), TCLocalize("Above"), TCLocalize("Below")};
		static CUi::SDropDownState s_AnimePositionDropDownState;
		static CScrollRegion s_AnimePositionScrollRegion;
		s_AnimePositionDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_AnimePositionScrollRegion;
		Column.HSplitTop(LineSize, &DropDownRect, &Column);
		DropDownRect.VSplitLeft(120.0f, &Label, &DropDownRect);
		Ui()->DoLabel(&Label, TCLocalize("Position"), FontSize, TEXTALIGN_ML);
		g_Config.m_TcAnimeLovePosition = Ui()->DoDropDown(&DropDownRect, g_Config.m_TcAnimeLovePosition, s_AnimePositionNames.data(), s_AnimePositionNames.size(), s_AnimePositionDropDownState);

		DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcAnimeLoveSpeech, TCLocalize("Floating phrases"), &g_Config.m_TcAnimeLoveSpeech, &Column, LineSize);
		if(g_Config.m_TcAnimeLoveSpeech)
		{
			Column.HSplitTop(LineSize + MarginExtraSmall, &Button, &Column);
			Button.VSplitLeft(120.0f, &Label, &Button);
			Ui()->DoLabel(&Label, TCLocalize("Greeting phrase"), FontSize, TEXTALIGN_ML);
			static CLineInput s_AnimeLovePhrase(g_Config.m_TcAnimeLovePhrase, sizeof(g_Config.m_TcAnimeLovePhrase));
			s_AnimeLovePhrase.SetEmptyText(TCLocalize("Hi!"));
			Ui()->DoEditBox(&s_AnimeLovePhrase, &Button, EditBoxFontSize);
		}

		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveSize, &g_Config.m_TcAnimeLoveSize, &Button, TCLocalize("Anime Love Size"), 40, 260, &CUi::ms_LinearScrollbarScale, 0, "px");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveSpeed, &g_Config.m_TcAnimeLoveSpeed, &Button, TCLocalize("Anime Love Speed"), 25, 250, &CUi::ms_LinearScrollbarScale, 0, "%");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveWalkDistance, &g_Config.m_TcAnimeLoveWalkDistance, &Button, TCLocalize("Walk Distance"), 0, 180, &CUi::ms_LinearScrollbarScale, 0, "px");
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&g_Config.m_TcAnimeLoveAlpha, &g_Config.m_TcAnimeLoveAlpha, &Button, TCLocalize("Anime Love Opacity"), 10, 100, &CUi::ms_LinearScrollbarScale, 0, "%");

		Column.HSplitTop(MarginSmall, nullptr, &Column);
		static CButtonContainer s_ReaderButtonAnimeLove, s_ClearButtonAnimeLove;
		DoLine_KeyReader(Column, s_ReaderButtonAnimeLove, s_ClearButtonAnimeLove, TCLocalize("Bind Anime Love"), "toggle tc_anime_love 0 1");
	}
	s_SectionBoxes.back().h = Column.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}


void CMenus::RenderMaKeystroke(CUIRect MainView)
{
	CUIRect LeftView, RightView, Button, Label, Row;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	CUIRect LeftFrame = LeftView;
	LeftFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect LeftInner = LeftView;
	LeftInner.Margin(2.0f, &LeftInner);
	LeftInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	LeftView.Margin(8.0f, &LeftView);

	CUIRect RightFrame = RightView;
	RightFrame.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
	CUIRect RightInner = RightView;
	RightInner.Margin(2.0f, &RightInner);
	RightInner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	RightView.Margin(8.0f, &RightView);

	auto DrawSectionTitle = [&](CUIRect &Column, const char *pTitle) {
		Column.HSplitTop(HeadlineHeight, &Label, &Column);
		Ui()->DoLabel(&Label, pTitle, HeadlineFontSize, TEXTALIGN_ML);
		Column.HSplitTop(MarginSmall, nullptr, &Column);
	};

	auto DoModelDropDown = [&](CUIRect &Column, const char *pLabel, int &Value, CUi::SDropDownState &State, CScrollRegion &DropDownScrollRegion) {
		static std::vector<const char *> s_vModelNames;
		s_vModelNames = {TCLocalize("Normal"), TCLocalize("Redondo"), TCLocalize("Diamante"), TCLocalize("Hexagonal"), TCLocalize("Personalizado")};
		State.m_SelectionPopupContext.m_pScrollRegion = &DropDownScrollRegion;
		Column.HSplitTop(LineSize, &Row, &Column);
		Row.VSplitLeft(124.0f, &Label, &Row);
		Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML);
		Value = Ui()->DoDropDown(&Row, std::clamp(Value, 0, 4), s_vModelNames.data(), s_vModelNames.size(), State);
	};

	auto DoOpacitySlider = [&](CUIRect &Column, const char *pLabel, int &Value) {
		Column.HSplitTop(LineSize, &Button, &Column);
		Ui()->DoScrollbarOption(&Value, &Value, &Button, pLabel, 0, 100, &CUi::ms_LinearScrollbarScale, 0, "%");
	};

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	{
		CUIRect TitleLabel, EditorButton;
		Label.VSplitRight(HeadlineHeight, &TitleLabel, &EditorButton);
		EditorButton.Margin(1.0f, &EditorButton);
		static CButtonContainer s_HudEditorTitleButtonMa;
		const bool CanOpenHudEditor = Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK;
		if(Ui()->DoButton_FontIcon(&s_HudEditorTitleButtonMa, FontIcon::ARROW_UP_RIGHT_FROM_SQUARE, CanOpenHudEditor ? 0 : -1, &EditorButton, BUTTONFLAG_LEFT, IGraphics::CORNER_ALL, CanOpenHudEditor) && CanOpenHudEditor)
		{
			SetActive(false);
			GameClient()->m_HudEditor.Activate();
		}
		Ui()->DoLabel(&TitleLabel, TCLocalize("HUD de teclas"), HeadlineFontSize, TEXTALIGN_ML);
	}
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokeHud, TCLocalize("Activar overlay de teclas"), &g_Config.m_TcKeystrokeHud, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokeHudOnlyOnPress, TCLocalize("Solo mostrar al presionar"), &g_Config.m_TcKeystrokeHudOnlyOnPress, &LeftView, LineSize);
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokeHudShowText, TCLocalize("Mostrar etiquetas"), &g_Config.m_TcKeystrokeHudShowText, &LeftView, LineSize);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DrawSectionTitle(LeftView, TCLocalize("Teclas A / D"));
	static CUi::SDropDownState s_KeyModelDropDownState;
	static CScrollRegion s_KeyModelDropDownScrollRegion;
	DoModelDropDown(LeftView, TCLocalize("Modelo"), g_Config.m_TcKeystrokeHudStyle, s_KeyModelDropDownState, s_KeyModelDropDownScrollRegion);
	DoOpacitySlider(LeftView, TCLocalize("Transparencia A/D"), g_Config.m_TcKeystrokeHudAlpha);
	static CButtonContainer s_KeyColorPressed, s_KeyColorUnpressed;
	DoLine_ColorPicker(&s_KeyColorPressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color presionada A/D"), &g_Config.m_TcKeystrokeHudColorPressed, ColorRGBA(1.0f, 1.0f, 1.0f), false, nullptr, true);
	DoLine_ColorPicker(&s_KeyColorUnpressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color normal A/D"), &g_Config.m_TcKeystrokeHudColorUnpressed, ColorRGBA(0.0f, 0.0f, 0.0f), false, nullptr, true);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	DrawSectionTitle(LeftView, TCLocalize("Espacio"));
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokeHudShowSpace, TCLocalize("Mostrar espacio"), &g_Config.m_TcKeystrokeHudShowSpace, &LeftView, LineSize);
	static CUi::SDropDownState s_SpaceModelDropDownState;
	static CScrollRegion s_SpaceModelDropDownScrollRegion;
	DoModelDropDown(LeftView, TCLocalize("Modelo"), g_Config.m_TcKeystrokeHudSpaceStyle, s_SpaceModelDropDownState, s_SpaceModelDropDownScrollRegion);
	DoOpacitySlider(LeftView, TCLocalize("Transparencia espacio"), g_Config.m_TcKeystrokeHudSpaceAlpha);
	static CButtonContainer s_SpaceColorPressed, s_SpaceColorUnpressed;
	DoLine_ColorPicker(&s_SpaceColorPressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color presionado espacio"), &g_Config.m_TcKeystrokeHudSpaceColorPressed, ColorRGBA(1.0f, 1.0f, 1.0f), false, nullptr, true);
	DoLine_ColorPicker(&s_SpaceColorUnpressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &LeftView, TCLocalize("Color normal espacio"), &g_Config.m_TcKeystrokeHudSpaceColorUnpressed, ColorRGBA(0.0f, 0.0f, 0.0f), false, nullptr, true);

	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);
	LeftView.HSplitTop(LineSize + 6.0f, &Button, &LeftView);
	static CButtonContainer s_HudEditorButton;
	if(DoButton_Menu(&s_HudEditorButton, TCLocalize("Abrir editor HUD"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f) && (Client()->State() == IClient::STATE_ONLINE || Client()->State() == IClient::STATE_DEMOPLAYBACK))
	{
		SetActive(false);
		GameClient()->m_HudEditor.Activate();
	}

	DrawSectionTitle(RightView, TCLocalize("Modelos personalizados"));
	Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ma/keystrokes", IStorage::TYPE_SAVE);

	static std::vector<std::string> s_vKeystrokePackValues;
	static std::vector<const char *> s_vKeystrokePackNames;
	s_vKeystrokePackValues.clear();
	s_vKeystrokePackValues.emplace_back("");
	Storage()->ListDirectory(IStorage::TYPE_ALL, "ma/keystrokes", MaKeystrokePackScan, &s_vKeystrokePackValues);
	std::sort(s_vKeystrokePackValues.begin() + 1, s_vKeystrokePackValues.end());

	int SelectedPack = 0;
	bool FoundPack = g_Config.m_TcKeystrokeHudCustomPack[0] == '\0';
	for(int i = 1; i < (int)s_vKeystrokePackValues.size(); ++i)
	{
		if(str_comp(g_Config.m_TcKeystrokeHudCustomPack, s_vKeystrokePackValues[i].c_str()) == 0)
		{
			SelectedPack = i;
			FoundPack = true;
			break;
		}
	}
	if(!FoundPack)
	{
		SelectedPack = (int)s_vKeystrokePackValues.size();
		s_vKeystrokePackValues.emplace_back(g_Config.m_TcKeystrokeHudCustomPack);
	}

	s_vKeystrokePackNames.clear();
	for(const std::string &Pack : s_vKeystrokePackValues)
		s_vKeystrokePackNames.push_back(Pack.empty() ? TCLocalize("Ninguno") : Pack.c_str());

	static CUi::SDropDownState s_CustomPackDropDownState;
	static CScrollRegion s_CustomPackDropDownScrollRegion;
	s_CustomPackDropDownState.m_SelectionPopupContext.m_pScrollRegion = &s_CustomPackDropDownScrollRegion;
	RightView.HSplitTop(LineSize, &Row, &RightView);
	Row.VSplitLeft(116.0f, &Label, &Row);
	Ui()->DoLabel(&Label, TCLocalize("Pack"), FontSize, TEXTALIGN_ML);
	const int NewSelectedPack = Ui()->DoDropDown(&Row, SelectedPack, s_vKeystrokePackNames.data(), s_vKeystrokePackNames.size(), s_CustomPackDropDownState);
	if(NewSelectedPack != SelectedPack && NewSelectedPack >= 0 && NewSelectedPack < (int)s_vKeystrokePackValues.size())
	{
		str_copy(g_Config.m_TcKeystrokeHudCustomPack, s_vKeystrokePackValues[NewSelectedPack].c_str(), sizeof(g_Config.m_TcKeystrokeHudCustomPack));
		GameClient()->m_KeystrokeHud.ReloadCustomTextures();
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize + 6.0f, &Row, &RightView);
	CUIRect FolderButton, ReloadButton;
	Row.VSplitMid(&FolderButton, &ReloadButton, MarginSmall);
	static CButtonContainer s_CustomFolderButton, s_CustomReloadButton;
	if(DoButton_Menu(&s_CustomFolderButton, TCLocalize("Carpeta modelos"), 0, &FolderButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
	{
		Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("ma/keystrokes", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ma/keystrokes", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	if(DoButton_Menu(&s_CustomReloadButton, TCLocalize("Recargar"), 0, &ReloadButton, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
	{
		GameClient()->m_KeystrokeHud.ReloadCustomTextures();
	}

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	DrawSectionTitle(RightView, TCLocalize("Mouse"));
	DoButton_CheckBoxAutoVMarginAndSet(&g_Config.m_TcKeystrokeHudShowMouse, TCLocalize("Mostrar LMB/RMB"), &g_Config.m_TcKeystrokeHudShowMouse, &RightView, LineSize);
	static CUi::SDropDownState s_MouseModelDropDownState;
	static CScrollRegion s_MouseModelDropDownScrollRegion;
	DoModelDropDown(RightView, TCLocalize("Modelo"), g_Config.m_TcKeystrokeHudMouseStyle, s_MouseModelDropDownState, s_MouseModelDropDownScrollRegion);
	DoOpacitySlider(RightView, TCLocalize("Transparencia mouse"), g_Config.m_TcKeystrokeHudMouseAlpha);
	static CButtonContainer s_MouseColorPressed, s_MouseColorUnpressed;
	DoLine_ColorPicker(&s_MouseColorPressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Color presionado mouse"), &g_Config.m_TcKeystrokeHudMouseColorPressed, ColorRGBA(1.0f, 1.0f, 1.0f), false, nullptr, true);
	DoLine_ColorPicker(&s_MouseColorUnpressed, ColorPickerLineSize, ColorPickerLabelSize, ColorPickerLineSpacing, &RightView, TCLocalize("Color normal mouse"), &g_Config.m_TcKeystrokeHudMouseColorUnpressed, ColorRGBA(0.0f, 0.0f, 0.0f), false, nullptr, true);

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	DrawSectionTitle(RightView, TCLocalize("Vista previa"));
	RightView.HSplitTop(FontSize * 2.0f, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Usa el editor HUD para mover A/D, espacio y mouse por separado."), FontSize, TEXTALIGN_ML);

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}
void CMenus::RenderMaAudio(CUIRect MainView)
{
	CUIRect LeftView, RightView, Label, List, BottomBar, SearchRow, ButtonRow, SearchBox, DirectoryButton, GeneralFolderButton, ReloadButton, PackFolderButton, TestButton, Row;

	static std::vector<SMaAudioPack> s_vAudioPacks;
	static std::vector<const SMaAudioPack *> s_vFilteredAudioPacks;
	static CLineInputBuffered<64> s_FilterInput;
	static bool s_NeedsReload = true;

	auto ReloadAudioPacks = [&]() {
		s_vAudioPacks.clear();

		SMaAudioPack DefaultPack;
		str_copy(DefaultPack.m_aName, "default");
		s_vAudioPacks.push_back(DefaultPack);

		Storage()->ListDirectory(IStorage::TYPE_ALL, "assets/audio", MaAudioPackScan, &s_vAudioPacks);
		Storage()->ListDirectory(IStorage::TYPE_ALL, "audio", MaAudioPackScan, &s_vAudioPacks);
		for(SMaAudioPack &Pack : s_vAudioPacks)
			Pack.m_GameSoundCount = MaAudioPackGameSoundCount(Storage(), Pack.m_aName);
		s_vAudioPacks.erase(std::remove_if(s_vAudioPacks.begin(), s_vAudioPacks.end(), [](const SMaAudioPack &Pack) {
			return str_comp(Pack.m_aName, "default") != 0 && Pack.m_GameSoundCount <= 0;
		}), s_vAudioPacks.end());
		if(str_comp(g_Config.m_SndPack, "ma_fx") == 0 && MaAudioPackExists(s_vAudioPacks, "ma_space_pulse"))
		{
			str_copy(g_Config.m_SndPack, "ma_space_pulse");
			GameClient()->m_Sounds.Clear();
		}
		std::sort(s_vAudioPacks.begin(), s_vAudioPacks.end(), [](const SMaAudioPack &Left, const SMaAudioPack &Right) {
			const bool LeftDefault = str_comp(Left.m_aName, "default") == 0;
			const bool RightDefault = str_comp(Right.m_aName, "default") == 0;
			if(LeftDefault != RightDefault)
				return LeftDefault;
			return str_comp_nocase(MaAudioPackDisplayName(Left.m_aName), MaAudioPackDisplayName(Right.m_aName)) < 0;
		});
	};

	if(s_NeedsReload)
	{
		ReloadAudioPacks();
		s_NeedsReload = false;
	}

	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);
	MainView.VSplitMid(&LeftView, &RightView, MarginBetweenViews);
	LeftView.VSplitLeft(MarginSmall, nullptr, &LeftView);
	RightView.VSplitRight(MarginSmall, &RightView, nullptr);

	LeftView.HSplitTop(HeadlineHeight, &Label, &LeftView);
	Ui()->DoLabel(&Label, TCLocalize("Audio"), HeadlineFontSize, TEXTALIGN_ML);
	LeftView.HSplitTop(MarginSmall, nullptr, &LeftView);

	LeftView.HSplitBottom(ms_ButtonHeight * 2.0f + 7.0f, &List, &BottomBar);
	BottomBar.HSplitTop(ms_ButtonHeight, &SearchRow, &BottomBar);
	BottomBar.HSplitTop(7.0f, nullptr, &ButtonRow);
	SearchBox = SearchRow;
	Ui()->DoEditBox_Search(&s_FilterInput, &SearchBox, 14.0f, !Ui()->IsPopupOpen() && !GameClient()->m_GameConsole.IsActive());

	const float ButtonW = ButtonRow.w / 5.0f;
	ButtonRow.VSplitLeft(ButtonW, &DirectoryButton, &ButtonRow);
	DirectoryButton.VSplitRight(4.0f, &DirectoryButton, nullptr);
	ButtonRow.VSplitLeft(ButtonW, &PackFolderButton, &ButtonRow);
	PackFolderButton.VSplitLeft(4.0f, nullptr, &PackFolderButton);
	PackFolderButton.VSplitRight(4.0f, &PackFolderButton, nullptr);
	ButtonRow.VSplitLeft(ButtonW, &GeneralFolderButton, &ButtonRow);
	GeneralFolderButton.VSplitLeft(4.0f, nullptr, &GeneralFolderButton);
	GeneralFolderButton.VSplitRight(4.0f, &GeneralFolderButton, nullptr);
	ButtonRow.VSplitLeft(ButtonW, &ReloadButton, &TestButton);
	ReloadButton.VSplitLeft(4.0f, nullptr, &ReloadButton);
	ReloadButton.VSplitRight(4.0f, &ReloadButton, nullptr);
	TestButton.VSplitLeft(4.0f, nullptr, &TestButton);

	static CButtonContainer s_AudioDirectoryButton;
	if(DoButton_Menu(&s_AudioDirectoryButton, TCLocalize("Nueva"), 0, &DirectoryButton))
	{
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/audio/mi_pack", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "assets/audio/mi_pack", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AudioDirectoryButton, &DirectoryButton, TCLocalize("Create and open assets/audio/mi_pack"));

	static CButtonContainer s_AudioPackFolderButton;
	if(DoButton_Menu(&s_AudioPackFolderButton, TCLocalize("Carpeta"), 0, &PackFolderButton))
	{
		char aPath[IO_MAX_PATH_LENGTH];
		if(str_comp(g_Config.m_SndPack, "default") == 0)
			str_copy(aPath, "assets/audio");
		else
			str_format(aPath, sizeof(aPath), "assets/audio/%s", g_Config.m_SndPack);
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(aPath, IStorage::TYPE_SAVE);

		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, aPath, aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AudioPackFolderButton, &PackFolderButton, TCLocalize("Open selected audio pack folder"));

	static CButtonContainer s_AudioGeneralFolderButton;
	if(DoButton_Menu(&s_AudioGeneralFolderButton, TCLocalize("General"), 0, &GeneralFolderButton))
	{
		Storage()->CreateFolder("assets", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("assets/audio", IStorage::TYPE_SAVE);
		char aBuf[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, "assets/audio", aBuf, sizeof(aBuf));
		Client()->ViewFile(aBuf);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_AudioGeneralFolderButton, &GeneralFolderButton, TCLocalize("Open the general audio folder to copy complete packs"));

	static CButtonContainer s_AudioReloadButton;
	if(DoButton_Menu(&s_AudioReloadButton, TCLocalize("Recargar"), 0, &ReloadButton) || Input()->KeyPress(KEY_F5) || (Input()->KeyPress(KEY_R) && Input()->ModifierIsPressed()))
	{
		ReloadAudioPacks();
		GameClient()->m_Sounds.Clear();
	}

	static CButtonContainer s_AudioTestButton;
	if(DoButton_Menu(&s_AudioTestButton, TCLocalize("Probar"), 0, &TestButton))
		GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_WEAPON_SWITCH, 1.0f);

	s_vFilteredAudioPacks.clear();
	for(const SMaAudioPack &Pack : s_vAudioPacks)
	{
		if(!s_FilterInput.IsEmpty() && !str_utf8_find_nocase(Pack.m_aName, s_FilterInput.GetString()) && !str_utf8_find_nocase(MaAudioPackDisplayName(Pack.m_aName), s_FilterInput.GetString()))
			continue;
		s_vFilteredAudioPacks.push_back(&Pack);
	}

	int OldSelected = -1;
	for(size_t i = 0; i < s_vFilteredAudioPacks.size(); ++i)
	{
		if(str_comp(s_vFilteredAudioPacks[i]->m_aName, g_Config.m_SndPack) == 0)
		{
			OldSelected = static_cast<int>(i);
			break;
		}
	}
	if(OldSelected < 0 && s_FilterInput.IsEmpty())
	{
		str_copy(g_Config.m_SndPack, "default");
		OldSelected = 0;
		GameClient()->m_Sounds.Clear();
	}

	static CListBox s_AudioPackListBox;
	s_AudioPackListBox.DoStart(28.0f, s_vFilteredAudioPacks.size(), 1, 1, OldSelected, &List, false);
	for(size_t i = 0; i < s_vFilteredAudioPacks.size(); ++i)
	{
		const SMaAudioPack *pPack = s_vFilteredAudioPacks[i];
		const CListboxItem Item = s_AudioPackListBox.DoNextItem(pPack, OldSelected >= 0 && static_cast<size_t>(OldSelected) == i);
		if(!Item.m_Visible)
			continue;

		CUIRect ItemRect = Item.m_Rect;
		ItemRect.Margin(6.0f, &ItemRect);
		CUIRect NameRect, CountRect;
		ItemRect.VSplitRight(70.0f, &NameRect, &CountRect);
		Ui()->DoLabel(&NameRect, MaAudioPackDisplayName(pPack->m_aName), FontSize, TEXTALIGN_ML);
		char aCount[32];
		str_format(aCount, sizeof(aCount), "%d/%zu", pPack->m_GameSoundCount, std::size(s_apMaAudioGameSoundFiles));
		Ui()->DoLabel(&CountRect, aCount, 11.0f, TEXTALIGN_MR);
	}

	const int NewSelected = s_AudioPackListBox.DoEnd();
	if(NewSelected >= 0 && NewSelected < static_cast<int>(s_vFilteredAudioPacks.size()) && NewSelected != OldSelected)
	{
		str_copy(g_Config.m_SndPack, s_vFilteredAudioPacks[NewSelected]->m_aName);
		GameClient()->m_Sounds.Clear();
	}

	RightView.HSplitTop(HeadlineHeight, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Sonidos del juego"), HeadlineFontSize, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	const int CurrentPackCount = MaAudioPackGameSoundCount(Storage(), g_Config.m_SndPack);
	char aStatus[128];
	str_format(aStatus, sizeof(aStatus), "%s: %s  |  %d/%zu", TCLocalize("Pack"), MaAudioPackDisplayName(g_Config.m_SndPack), CurrentPackCount, std::size(s_apMaAudioGameSoundFiles));
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, aStatus, FontSize, TEXTALIGN_ML);

	RightView.HSplitTop(MarginSmall, nullptr, &RightView);
	RightView.HSplitTop(LineSize, &Label, &RightView);
	Ui()->DoLabel(&Label, TCLocalize("Para cambiar un sonido, coloca archivos .wv o .wav con estos nombres dentro del pack."), 12.0f, TEXTALIGN_ML);
	RightView.HSplitTop(MarginSmall, nullptr, &RightView);

	for(size_t i = 0; i < std::size(s_aMaAudioSoundInfos); ++i)
	{
		const SMaAudioSoundInfo &Info = s_aMaAudioSoundInfos[i];
		static CButtonContainer s_aSoundTestButtons[std::size(s_aMaAudioSoundInfos)];
		RightView.HSplitTop(28.0f, &Row, &RightView);
		CUIRect TestRect, TextRect, NameRect, FilesRect;
		Row.VSplitRight(58.0f, &TextRect, &TestRect);
		TextRect.VSplitLeft(92.0f, &NameRect, &FilesRect);
		Ui()->DoLabel(&NameRect, TCLocalize(Info.m_pLabel), FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&FilesRect, Info.m_pFiles, 11.0f, TEXTALIGN_ML);
		if(DoButton_Menu(&s_aSoundTestButtons[i], TCLocalize("Test"), 0, &TestRect))
			GameClient()->m_Sounds.Play(CSounds::CHN_GUI, Info.m_TestSound, 1.0f);
	}
}


void CMenus::RenderMaEditorSkins(CUIRect MainView)
{
	CUIRect Label, Button;

	static CScrollRegion s_ScrollRegion;
	vec2 ScrollOffset(0.0f, 0.0f);
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	ScrollParams.m_Flags = CScrollRegionParams::FLAG_CONTENT_STATIC_WIDTH;
	ScrollParams.m_ScrollbarMargin = 5.0f;
	s_ScrollRegion.Begin(&MainView, &ScrollOffset, &ScrollParams);

	MainView.y += ScrollOffset.y;
	MainView.VSplitRight(5.0f, &MainView, nullptr);
	MainView.VSplitLeft(5.0f, nullptr, &MainView);

	static std::vector<CUIRect> s_SectionBoxes;
	static vec2 s_PrevScrollOffset(0.0f, 0.0f);

	for(CUIRect &Section : s_SectionBoxes)
	{
		float Padding = MarginBetweenViews * 0.6666f;
		Section.w += Padding;
		Section.h += Padding;
		Section.x -= Padding * 0.5f;
		Section.y -= Padding * 0.5f;
		Section.y -= s_PrevScrollOffset.y - ScrollOffset.y;
		Section.Draw(TClientThemeAccentColor(), IGraphics::CORNER_ALL, 8.0f);
		CUIRect Inner = Section;
		Inner.Margin(2.0f, &Inner);
		Inner.Draw(TClientThemePanelColor(), IGraphics::CORNER_ALL, 6.0f);
	}
	s_PrevScrollOffset = ScrollOffset;
	s_SectionBoxes.clear();

	MainView.HSplitTop(Margin, nullptr, &MainView);
	s_SectionBoxes.push_back(MainView);
	MainView.HSplitTop(HeadlineHeight, &Label, &MainView);
	Ui()->DoLabel(&Label, TCLocalize("Editor skins"), HeadlineFontSize, TEXTALIGN_ML);
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitTop(FontSize, &Label, &MainView);
	Ui()->DoLabel(&Label, TCLocalize("Create mixed assets or jump to the name plate editor."), FontSize, TEXTALIGN_ML);
	MainView.HSplitTop(MarginSmall, nullptr, &MainView);

	MainView.HSplitTop(LineSize + 6.0f, &Button, &MainView);
	static CButtonContainer s_AssetsEditorBtn;
	if(DoButton_Menu(&s_AssetsEditorBtn, TCLocalize("Assets editor"), 0, &Button, BUTTONFLAG_LEFT, nullptr, IGraphics::CORNER_ALL, 6.0f))
	{
		m_AssetsEditorState.m_VisualsEditorOpen = true;
		m_AssetsEditorState.m_FullscreenOpen = true;
		if(!m_AssetsEditorState.m_VisualsEditorInitialized)
		{
			AssetsEditorReloadAssets();
			AssetsEditorResetPartSlots();
			AssetsEditorEnsureDefaultExportNames();
			AssetsEditorSyncExportNameFromType();
			m_AssetsEditorState.m_VisualsEditorInitialized = true;
		}
	}

	s_SectionBoxes.back().h = MainView.y - s_SectionBoxes.back().y;

	CUIRect ScrollRegion;
	ScrollRegion.w = MainView.w;
	ScrollRegion.h = 0.0f;
	s_ScrollRegion.AddRect(ScrollRegion);
	s_ScrollRegion.End();
}











