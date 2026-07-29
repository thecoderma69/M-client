#include "stream_chat.h"

#include <base/color.h>
#include <base/math.h>
#include <base/net.h>
#include <base/str.h>
#include <base/system.h>
#include <base/thread.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/textrender.h>

#include <game/client/components/hud_layout.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <string>

namespace
{
constexpr int STREAM_PLATFORM_TWITCH = 1;
constexpr int STREAM_PLATFORM_YOUTUBE = 2;
constexpr int STREAM_PLATFORM_KICK = 3;
constexpr int STREAM_CHAT_MAX_LINES = 64;
constexpr int STREAM_CHAT_PREVIEW_LINES = 5;
constexpr int STREAM_ACTIVITY_MAX_LINES = 48;
constexpr int STREAM_ACTIVITY_PREVIEW_LINES = 3;

bool IsSafeChannelChar(char c)
{
	return std::isalnum((unsigned char)c) != 0 || c == '_';
}

std::string TrimCopy(const std::string &Input)
{
	size_t Begin = 0;
	while(Begin < Input.size() && std::isspace((unsigned char)Input[Begin]))
		Begin++;
	size_t End = Input.size();
	while(End > Begin && std::isspace((unsigned char)Input[End - 1]))
		End--;
	return Input.substr(Begin, End - Begin);
}

std::string UnescapeTwitchTagValue(const std::string &Input)
{
	std::string Out;
	Out.reserve(Input.size());
	for(size_t i = 0; i < Input.size(); i++)
	{
		if(Input[i] == '\\' && i + 1 < Input.size())
		{
			switch(Input[++i])
			{
			case 's': Out.push_back(' '); break;
			case ':': Out.push_back(';'); break;
			case 'r': Out.push_back('\r'); break;
			case 'n': Out.push_back('\n'); break;
			case '\\': Out.push_back('\\'); break;
			default: Out.push_back(Input[i]); break;
			}
		}
		else
			Out.push_back(Input[i]);
	}
	return Out;
}

std::string ExtractTwitchDisplayName(const std::string &Tags)
{
	const std::string Key = "display-name=";
	size_t Pos = Tags.find(Key);
	if(Pos == std::string::npos)
		return std::string();
	Pos += Key.size();
	size_t End = Tags.find(';', Pos);
	if(End == std::string::npos)
		End = Tags.size();
	return UnescapeTwitchTagValue(Tags.substr(Pos, End - Pos));
}

void TruncateUtf8ish(char *pText, int Size)
{
	if(Size <= 0)
		return;
	pText[Size - 1] = '\0';
	const int Len = str_length(pText);
	if(Len <= Size - 4)
		return;
	pText[maximum(0, Size - 4)] = '\0';
	str_append(pText, "...", Size);
}

void FormatDurationTicks(int64_t Since, char *pBuf, int Size)
{
	if(Size <= 0)
		return;
	if(Since <= 0)
	{
		str_copy(pBuf, "00:00", Size);
		return;
	}

	const int64_t Seconds = maximum<int64_t>(0, (time_get() - Since) / time_freq());
	const int Hours = (int)(Seconds / 3600);
	const int Minutes = (int)((Seconds / 60) % 60);
	const int Secs = (int)(Seconds % 60);
	if(Hours > 0)
		str_format(pBuf, Size, "%d:%02d:%02d", Hours, Minutes, Secs);
	else
		str_format(pBuf, Size, "%02d:%02d", Minutes, Secs);
}


bool IsUrlSafeValueChar(char c)
{
	return std::isalnum((unsigned char)c) != 0 || c == '_' || c == '-' || c == '.' || c == '~';
}

std::string UrlEncodeQuery(const char *pInput)
{
	static const char s_aHex[] = "0123456789ABCDEF";
	std::string Out;
	if(!pInput)
		return Out;
	for(const unsigned char *p = (const unsigned char *)pInput; *p; ++p)
	{
		if(IsUrlSafeValueChar((char)*p))
			Out.push_back((char)*p);
		else
		{
			Out.push_back('%');
			Out.push_back(s_aHex[*p >> 4]);
			Out.push_back(s_aHex[*p & 0x0f]);
		}
	}
	return Out;
}

bool JsonIntLike(const json_value *pValue, int *pOut)
{
	if(!pValue || pValue == &json_value_none || !pOut)
		return false;
	if(pValue->type == json_integer)
	{
		*pOut = (int)pValue->u.integer;
		return true;
	}
	if(pValue->type == json_string && pValue->u.string.ptr)
	{
		int Value = 0;
		if(str_toint(pValue->u.string.ptr, &Value))
		{
			*pOut = Value;
			return true;
		}
	}
	return false;
}

void FormatViewerCount(int Viewers, char *pBuf, int Size)
{
	if(Size <= 0)
		return;
	if(Viewers < 0)
		str_copy(pBuf, "API no configurada", Size);
	else
		str_format(pBuf, Size, "%d", Viewers);
}

} // namespace

CStreamChat::~CStreamChat()
{
	AbortStatsRequest();
	StopWorker();
}

void CStreamChat::OnInit()
{
	SetStatus("Desactivado.");
}

void CStreamChat::OnReset()
{
	AbortStatsRequest();
	ResetSessionStats();
}

void CStreamChat::OnShutdown()
{
	AbortStatsRequest();
	StopWorker();
}

void CStreamChat::RequestReconnect()
{
	m_ReconnectRequested.store(true);
}

void CStreamChat::GetStatus(char *pBuf, int Size) const
{
	CLockScope Lock(m_Lock);
	str_copy(pBuf, m_aStatus, Size);
}

void CStreamChat::SetStatus(const char *pStatus)
{
	CLockScope Lock(m_Lock);
	str_copy(m_aStatus, pStatus ? pStatus : "", sizeof(m_aStatus));
}

const char *CStreamChat::PlatformName(int Platform)
{
	switch(ClampPlatform(Platform))
	{
	case STREAM_PLATFORM_TWITCH:
		return "Twitch";
	case STREAM_PLATFORM_YOUTUBE:
		return "YouTube";
	case STREAM_PLATFORM_KICK:
		return "Kick";
	default:
		return "Stream";
	}
}

int CStreamChat::ClampPlatform(int Platform)
{
	return std::clamp(Platform, STREAM_PLATFORM_TWITCH, STREAM_PLATFORM_KICK);
}

void CStreamChat::GetActivitySummary(char *pBuf, int Size) const
{
	if(Size <= 0)
		return;

	const int Source = g_Config.m_MaStreamActivityPlatform == 0 ? g_Config.m_MaStreamChatPlatform : g_Config.m_MaStreamActivityPlatform;
	const int Platform = ClampPlatform(Source);
	int aPlatformMessages[4] = {};
	int TotalMessages = 0;
	int UniqueChatters = 0;
	int ViewerCount = -1;
	int PeakViewerCount = 0;
	int64_t ConnectedSince = 0;
	{
		CLockScope Lock(m_Lock);
		for(int i = 0; i < 4; ++i)
			aPlatformMessages[i] = m_aPlatformMessages[i];
		TotalMessages = m_TotalMessages;
		UniqueChatters = (int)m_vKnownUsers.size();
		ViewerCount = m_aViewerCount[Platform];
		PeakViewerCount = m_aPeakViewerCount[Platform];
		ConnectedSince = m_ConnectedSince;
	}

	char aUptime[32];
	char aViewers[64];
	char aPeak[32];
	FormatDurationTicks(ConnectedSince, aUptime, sizeof(aUptime));
	FormatViewerCount(ViewerCount, aViewers, sizeof(aViewers));
	if(ViewerCount < 0 && PeakViewerCount <= 0)
		str_copy(aPeak, "-", sizeof(aPeak));
	else
		str_format(aPeak, sizeof(aPeak), "%d", PeakViewerCount);
	str_format(pBuf, Size, "%s | espectadores: %s | pico: %s | chatters: %d | mensajes: %d | total: %d | activo: %s",
		PlatformName(Platform), aViewers, aPeak, UniqueChatters, aPlatformMessages[Platform], TotalMessages, aUptime);
}
void CStreamChat::ResetSessionStats()
{
	CLockScope Lock(m_Lock);
	m_vLines.clear();
	m_vActivity.clear();
	m_vKnownUsers.clear();
	m_TotalMessages = 0;
	for(int &Messages : m_aPlatformMessages)
		Messages = 0;
	for(int &Viewers : m_aViewerCount)
		Viewers = -1;
	for(int &PeakViewers : m_aPeakViewerCount)
		PeakViewers = 0;
	m_ConnectedSince = 0;
	m_LastMessageTime = 0;
	m_LastStatsUpdate = 0;
	m_NextStatsTick = 0;
	m_aStatsIdentity[0] = '\0';
	str_copy(m_aStatsStatus, "Espectadores: API no configurada.", sizeof(m_aStatsStatus));
}
void CStreamChat::ClearLines()
{
	CLockScope Lock(m_Lock);
	m_vLines.clear();
}

void CStreamChat::AddActivity(int Platform, const char *pText)
{
	if(!pText || pText[0] == '\0')
		return;

	CLockScope Lock(m_Lock);
	SActivity Activity;
	str_copy(Activity.m_aPlatform, PlatformName(Platform), sizeof(Activity.m_aPlatform));
	str_copy(Activity.m_aText, pText, sizeof(Activity.m_aText));
	TruncateUtf8ish(Activity.m_aText, sizeof(Activity.m_aText));
	Activity.m_Time = time_get();
	m_vActivity.push_back(Activity);
	while((int)m_vActivity.size() > STREAM_ACTIVITY_MAX_LINES)
		m_vActivity.erase(m_vActivity.begin());
}

void CStreamChat::MarkConnected(int Platform)
{
	CLockScope Lock(m_Lock);
	m_CurrentPlatform = ClampPlatform(Platform);
	m_ConnectedSince = time_get();
}

void CStreamChat::AddLine(const char *pUser, const char *pText, bool CountStats)
{
	if(!pText || pText[0] == '\0')
		return;

	CLockScope Lock(m_Lock);
	SLine Line;
	str_copy(Line.m_aUser, pUser && pUser[0] ? pUser : "chat", sizeof(Line.m_aUser));
	str_copy(Line.m_aText, pText, sizeof(Line.m_aText));
	TruncateUtf8ish(Line.m_aText, sizeof(Line.m_aText));
	Line.m_Time = time_get();
	m_vLines.push_back(Line);
	while((int)m_vLines.size() > STREAM_CHAT_MAX_LINES)
		m_vLines.erase(m_vLines.begin());

	if(!CountStats)
		return;

	const int Platform = ClampPlatform(m_CurrentPlatform == 0 ? g_Config.m_MaStreamChatPlatform : m_CurrentPlatform);
	m_TotalMessages++;
	m_aPlatformMessages[Platform]++;
	m_LastMessageTime = Line.m_Time;

	bool KnownUser = false;
	for(const std::string &User : m_vKnownUsers)
	{
		if(str_comp(User.c_str(), Line.m_aUser) == 0)
		{
			KnownUser = true;
			break;
		}
	}
	if(!KnownUser)
		m_vKnownUsers.emplace_back(Line.m_aUser);

	SActivity Activity;
	str_copy(Activity.m_aPlatform, PlatformName(Platform), sizeof(Activity.m_aPlatform));
	str_format(Activity.m_aText, sizeof(Activity.m_aText), "Mensaje de %s", Line.m_aUser);
	Activity.m_Time = Line.m_Time;
	m_vActivity.push_back(Activity);
	while((int)m_vActivity.size() > STREAM_ACTIVITY_MAX_LINES)
		m_vActivity.erase(m_vActivity.begin());
}

bool CStreamChat::IsAllDigits(const char *pInput)
{
	if(!pInput || pInput[0] == '\0')
		return false;
	for(const char *p = pInput; *p; ++p)
	{
		if(!str_isnum(*p))
			return false;
	}
	return true;
}

bool CStreamChat::ExtractYouTubeVideoId(const char *pInput, char *pOut, int OutSize)
{
	if(OutSize <= 0)
		return false;
	pOut[0] = '\0';
	const std::string Input = TrimCopy(pInput ? pInput : "");
	if(Input.empty())
		return false;

	auto CopyId = [&](size_t Pos) {
		char aId[96] = "";
		int Len = 0;
		while(Pos < Input.size() && Len < (int)sizeof(aId) - 1)
		{
			const char c = Input[Pos++];
			if(!(std::isalnum((unsigned char)c) || c == '_' || c == '-'))
				break;
			aId[Len++] = c;
		}
		aId[Len] = '\0';
		if(Len < 6)
			return false;
		str_copy(pOut, aId, OutSize);
		return true;
	};

	const size_t WatchPos = Input.find("v=");
	if(WatchPos != std::string::npos && CopyId(WatchPos + 2))
		return true;
	const char *apMarkers[] = {"youtu.be/", "/live/", "/shorts/", "/embed/"};
	for(const char *pMarker : apMarkers)
	{
		const size_t Pos = Input.find(pMarker);
		if(Pos != std::string::npos && CopyId(Pos + str_length(pMarker)))
			return true;
	}
	return CopyId(0);
}

void CStreamChat::AbortStatsRequest()
{
	if(m_pStatsRequest && !m_pStatsRequest->Done())
		m_pStatsRequest->Abort();
	m_pStatsRequest.reset();
	m_StatsRequestPlatform = 0;
	m_aStatsRequestIdentity[0] = '\0';
}

void CStreamChat::SetStatsStatus(const char *pStatus)
{
	CLockScope Lock(m_Lock);
	str_copy(m_aStatsStatus, pStatus ? pStatus : "", sizeof(m_aStatsStatus));
}

void CStreamChat::SetViewerStats(int Platform, int Viewers, const char *pStatus)
{
	Platform = ClampPlatform(Platform);
	CLockScope Lock(m_Lock);
	m_aViewerCount[Platform] = Viewers;
	if(Viewers > m_aPeakViewerCount[Platform])
		m_aPeakViewerCount[Platform] = Viewers;
	m_LastStatsUpdate = Viewers >= 0 ? time_get() : 0;
	str_copy(m_aStatsStatus, pStatus ? pStatus : "", sizeof(m_aStatsStatus));
}

void CStreamChat::StartStatsRequest(int Platform, const char *pIdentity)
{
	Platform = ClampPlatform(Platform);
	if(!pIdentity || pIdentity[0] == '\0')
		return;

	std::string Url;
	std::shared_ptr<CHttpRequest> pRequest;
	if(Platform == STREAM_PLATFORM_TWITCH)
	{
		if(g_Config.m_MaStreamStatsTwitchClientId[0] == '\0' || g_Config.m_MaStreamStatsTwitchToken[0] == '\0')
		{
			SetViewerStats(Platform, -1, "Espectadores: falta Twitch Client ID/token.");
			return;
		}
		Url = "https://api.twitch.tv/helix/streams?user_login=" + UrlEncodeQuery(pIdentity);
		pRequest = HttpGet(Url.c_str());
		char aAuth[640];
		str_format(aAuth, sizeof(aAuth), "Bearer %s", g_Config.m_MaStreamStatsTwitchToken);
		pRequest->HeaderString("Client-Id", g_Config.m_MaStreamStatsTwitchClientId);
		pRequest->HeaderString("Authorization", aAuth);
	}
	else if(Platform == STREAM_PLATFORM_YOUTUBE)
	{
		if(g_Config.m_MaStreamStatsYoutubeApiKey[0] == '\0')
		{
			SetViewerStats(Platform, -1, "Espectadores: falta YouTube API key.");
			return;
		}
		Url = "https://www.googleapis.com/youtube/v3/videos?part=liveStreamingDetails&id=" + UrlEncodeQuery(pIdentity) + "&key=" + UrlEncodeQuery(g_Config.m_MaStreamStatsYoutubeApiKey);
		pRequest = HttpGet(Url.c_str());
	}
	else if(Platform == STREAM_PLATFORM_KICK)
	{
		if(g_Config.m_MaStreamStatsKickToken[0] == '\0')
		{
			SetViewerStats(Platform, -1, "Espectadores: falta Kick token.");
			return;
		}
		Url = "https://api.kick.com/public/v1/livestreams?broadcaster_user_id=" + UrlEncodeQuery(pIdentity);
		pRequest = HttpGet(Url.c_str());
		char aAuth[640];
		str_format(aAuth, sizeof(aAuth), "Bearer %s", g_Config.m_MaStreamStatsKickToken);
		pRequest->HeaderString("Authorization", aAuth);
	}

	if(!pRequest)
		return;

	pRequest->FailOnErrorStatus(false);
	pRequest->LogProgress(HTTPLOG::FAILURE);
	pRequest->Timeout(CTimeout{5000, 10000, 500, 5});
	pRequest->MaxResponseSize(1024 * 1024);
	m_pStatsRequest = pRequest;
	m_StatsRequestPlatform = Platform;
	str_copy(m_aStatsRequestIdentity, pIdentity, sizeof(m_aStatsRequestIdentity));
	SetStatsStatus("Espectadores: actualizando...");
	Http()->Run(m_pStatsRequest);
}

void CStreamChat::FinishStatsRequest()
{
	if(!m_pStatsRequest || !m_pStatsRequest->Done())
		return;

	std::shared_ptr<CHttpRequest> pFinished = m_pStatsRequest;
	const int Platform = ClampPlatform(m_StatsRequestPlatform);
	m_pStatsRequest.reset();
	m_StatsRequestPlatform = 0;
	m_aStatsRequestIdentity[0] = '\0';
	m_NextStatsTick = time_get() + (int64_t)std::clamp(g_Config.m_MaStreamViewerStatsRefresh, 10, 300) * time_freq();

	if(pFinished->State() != EHttpState::DONE)
	{
		SetViewerStats(Platform, -1, "Espectadores: error de conexion API.");
		return;
	}
	if(pFinished->StatusCode() < 200 || pFinished->StatusCode() >= 300)
	{
		char aStatus[128];
		str_format(aStatus, sizeof(aStatus), "Espectadores: API HTTP %d.", pFinished->StatusCode());
		SetViewerStats(Platform, -1, aStatus);
		return;
	}

	json_value *pRoot = pFinished->ResultJson();
	if(!pRoot)
	{
		SetViewerStats(Platform, -1, "Espectadores: respuesta API invalida.");
		return;
	}

	int Viewers = -1;
	if(Platform == STREAM_PLATFORM_TWITCH)
	{
		const json_value *pData = json_object_get(pRoot, "data");
		const json_value *pFirst = json_array_get(pData, 0);
		if(pFirst && pFirst != &json_value_none)
			JsonIntLike(json_object_get(pFirst, "viewer_count"), &Viewers);
		else
			Viewers = 0;
	}
	else if(Platform == STREAM_PLATFORM_YOUTUBE)
	{
		const json_value *pItems = json_object_get(pRoot, "items");
		const json_value *pFirst = json_array_get(pItems, 0);
		const json_value *pLive = pFirst && pFirst != &json_value_none ? json_object_get(pFirst, "liveStreamingDetails") : nullptr;
		if(pLive && pLive != &json_value_none)
			JsonIntLike(json_object_get(pLive, "concurrentViewers"), &Viewers);
		if(Viewers < 0)
			Viewers = 0;
	}
	else if(Platform == STREAM_PLATFORM_KICK)
	{
		const json_value *pData = json_object_get(pRoot, "data");
		const json_value *pSource = pData;
		if(pData && pData->type == json_array)
			pSource = json_array_get(pData, 0);
		if(pSource && pSource != &json_value_none)
		{
			if(!JsonIntLike(json_object_get(pSource, "viewer_count"), &Viewers))
				if(!JsonIntLike(json_object_get(pSource, "viewers"), &Viewers))
					JsonIntLike(json_object_get(pSource, "concurrent_viewers"), &Viewers);
		}
		if(Viewers < 0)
			Viewers = 0;
	}

	json_value_free(pRoot);
	if(Viewers < 0)
	{
		SetViewerStats(Platform, -1, "Espectadores: campo no encontrado.");
		return;
	}

	char aStatus[128];
	str_format(aStatus, sizeof(aStatus), "Espectadores: %d", Viewers);
	SetViewerStats(Platform, Viewers, aStatus);
}

void CStreamChat::UpdateViewerStats()
{
	FinishStatsRequest();

	if(!g_Config.m_MaStreamChat || !g_Config.m_MaStreamActivityStats || !g_Config.m_MaStreamViewerStats)
	{
		AbortStatsRequest();
		return;
	}

	const int Platform = ClampPlatform(g_Config.m_MaStreamActivityPlatform == 0 ? g_Config.m_MaStreamChatPlatform : g_Config.m_MaStreamActivityPlatform);
	char aIdentity[128] = "";
	bool IdentityOk = false;
	if(Platform == STREAM_PLATFORM_TWITCH)
		IdentityOk = NormalizeTwitchChannel(g_Config.m_MaStreamChatChannel, aIdentity, sizeof(aIdentity));
	else if(Platform == STREAM_PLATFORM_YOUTUBE)
		IdentityOk = ExtractYouTubeVideoId(g_Config.m_MaStreamChatChannel, aIdentity, sizeof(aIdentity));
	else if(Platform == STREAM_PLATFORM_KICK)
	{
		if(IsAllDigits(g_Config.m_MaStreamStatsKickBroadcasterId))
		{
			str_copy(aIdentity, g_Config.m_MaStreamStatsKickBroadcasterId, sizeof(aIdentity));
			IdentityOk = true;
		}
		else if(IsAllDigits(g_Config.m_MaStreamChatChannel))
		{
			str_copy(aIdentity, g_Config.m_MaStreamChatChannel, sizeof(aIdentity));
			IdentityOk = true;
		}
	}

	if(!IdentityOk)
	{
		AbortStatsRequest();
		SetViewerStats(Platform, -1, Platform == STREAM_PLATFORM_KICK ? "Espectadores: Kick necesita broadcaster ID." : "Espectadores: canal/URL invalido.");
		m_NextStatsTick = time_get() + 5 * time_freq();
		return;
	}

	if(str_comp(m_aStatsIdentity, aIdentity) != 0)
	{
		AbortStatsRequest();
		str_copy(m_aStatsIdentity, aIdentity, sizeof(m_aStatsIdentity));
		SetViewerStats(Platform, -1, "Espectadores: esperando API.");
		m_NextStatsTick = 0;
	}

	if(m_pStatsRequest || time_get() < m_NextStatsTick)
		return;

	StartStatsRequest(Platform, aIdentity);
	if(!m_pStatsRequest)
		m_NextStatsTick = time_get() + 5 * time_freq();
}
bool CStreamChat::NormalizeTwitchChannel(const char *pInput, char *pOut, int OutSize)
{
	if(OutSize <= 0)
		return false;
	pOut[0] = '\0';
	if(!pInput || pInput[0] == '\0')
		return false;

	std::string Text = TrimCopy(pInput);
	for(char &c : Text)
	{
		if(c == '\\')
			c = '/';
	}

	std::string Lower = Text;
	std::transform(Lower.begin(), Lower.end(), Lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

	const char *apMarkers[] = {"twitch.tv/", "www.twitch.tv/", "m.twitch.tv/"};
	for(const char *pMarker : apMarkers)
	{
		size_t Pos = Lower.find(pMarker);
		if(Pos != std::string::npos)
		{
			Text = Text.substr(Pos + str_length(pMarker));
			break;
		}
	}

	if(Text.rfind("http://", 0) == 0 || Text.rfind("https://", 0) == 0)
	{
		size_t Slash = Text.find('/', Text.find("://") + 3);
		Text = Slash == std::string::npos ? std::string() : Text.substr(Slash + 1);
	}

	while(!Text.empty() && (Text[0] == '#' || Text[0] == '@' || Text[0] == '/'))
		Text.erase(Text.begin());

	size_t Cut = Text.find_first_of("/?#& ");
	if(Cut != std::string::npos)
		Text = Text.substr(0, Cut);

	std::string Channel;
	for(char c : Text)
	{
		if(IsSafeChannelChar(c))
			Channel.push_back((char)std::tolower((unsigned char)c));
	}

	if(Channel.empty())
		return false;
	str_copy(pOut, Channel.c_str(), OutSize);
	return true;
}

bool CStreamChat::SendIrcLine(NETSOCKET Socket, const char *pLine)
{
	char aBuffer[512];
	str_format(aBuffer, sizeof(aBuffer), "%s\r\n", pLine);
	const int Size = str_length(aBuffer);
	int SentTotal = 0;
	while(SentTotal < Size)
	{
		const int Sent = net_tcp_send(Socket, aBuffer + SentTotal, Size - SentTotal);
		if(Sent < 0)
			return false;
		if(Sent == 0)
		{
			thread_yield();
			continue;
		}
		SentTotal += Sent;
	}
	return true;
}

bool CStreamChat::ParseTwitchLine(const char *pLine, char *pUser, int UserSize, char *pMessage, int MessageSize)
{
	if(UserSize <= 0 || MessageSize <= 0)
		return false;
	pUser[0] = '\0';
	pMessage[0] = '\0';
	if(!pLine || pLine[0] == '\0')
		return false;

	std::string Line = pLine;
	std::string Tags;
	if(!Line.empty() && Line[0] == '@')
	{
		size_t Space = Line.find(' ');
		if(Space == std::string::npos)
			return false;
		Tags = Line.substr(1, Space - 1);
		Line = Line.substr(Space + 1);
	}

	const size_t PrivMsgPos = Line.find(" PRIVMSG ");
	if(PrivMsgPos == std::string::npos)
		return false;
	const size_t MsgPos = Line.find(" :", PrivMsgPos + 1);
	if(MsgPos == std::string::npos)
		return false;

	std::string User = ExtractTwitchDisplayName(Tags);
	if(User.empty() && !Line.empty() && Line[0] == ':')
	{
		size_t Bang = Line.find('!');
		if(Bang != std::string::npos && Bang > 1)
			User = Line.substr(1, Bang - 1);
	}
	if(User.empty())
		User = "chat";

	std::string Message = Line.substr(MsgPos + 2);
	Message.erase(std::remove(Message.begin(), Message.end(), '\r'), Message.end());
	Message.erase(std::remove(Message.begin(), Message.end(), '\n'), Message.end());
	if(Message.empty())
		return false;

	str_copy(pUser, User.c_str(), UserSize);
	str_copy(pMessage, Message.c_str(), MessageSize);
	TruncateUtf8ish(pMessage, MessageSize);
	return true;
}

void CStreamChat::ThreadEntry(void *pUser)
{
	((CStreamChat *)pUser)->WorkerMain();
}

void CStreamChat::WorkerMain()
{
	char aChannel[128];
	str_copy(aChannel, m_aCurrentChannel, sizeof(aChannel));

	NETADDR Addr = NETADDR_ZEROED;
	if(net_host_lookup("irc.chat.twitch.tv:6667", &Addr, NETTYPE_IPV4 | NETTYPE_IPV6) != 0)
	{
		SetStatus("No se pudo resolver Twitch.");
		AddActivity(STREAM_PLATFORM_TWITCH, "No se pudo resolver Twitch.");
		m_ThreadFinished.store(true);
		return;
	}

	NETADDR BindAddr = NETADDR_ZEROED;
	BindAddr.type = Addr.type & (NETTYPE_IPV4 | NETTYPE_IPV6);
	NETSOCKET Socket = net_tcp_create(BindAddr);
	if(!Socket)
	{
		SetStatus("No se pudo crear socket.");
		AddActivity(STREAM_PLATFORM_TWITCH, "No se pudo crear socket.");
		m_ThreadFinished.store(true);
		return;
	}

	if(net_tcp_connect(Socket, &Addr) != 0)
	{
		net_tcp_close(Socket);
		SetStatus("No se pudo conectar a Twitch.");
		AddActivity(STREAM_PLATFORM_TWITCH, "No se pudo conectar a Twitch.");
		m_ThreadFinished.store(true);
		return;
	}

	char aNick[32];
	str_format(aNick, sizeof(aNick), "justinfan%d", (int)(time_get() % 90000) + 10000);
	char aNickLine[64];
	str_format(aNickLine, sizeof(aNickLine), "NICK %s", aNick);

	if(!SendIrcLine(Socket, "CAP REQ :twitch.tv/tags twitch.tv/commands") ||
		!SendIrcLine(Socket, "PASS SCHMOOPIIE") ||
		!SendIrcLine(Socket, aNickLine))
	{
		net_tcp_close(Socket);
		SetStatus("No se pudo autenticar IRC.");
		AddActivity(STREAM_PLATFORM_TWITCH, "No se pudo autenticar IRC.");
		m_ThreadFinished.store(true);
		return;
	}

	char aJoin[160];
	str_format(aJoin, sizeof(aJoin), "JOIN #%s", aChannel);
	if(!SendIrcLine(Socket, aJoin))
	{
		net_tcp_close(Socket);
		SetStatus("No se pudo entrar al canal.");
		AddActivity(STREAM_PLATFORM_TWITCH, "No se pudo entrar al canal.");
		m_ThreadFinished.store(true);
		return;
	}

	net_set_non_blocking(Socket);
	SetStatus("Conectado a Twitch.");
	MarkConnected(STREAM_PLATFORM_TWITCH);
	AddActivity(STREAM_PLATFORM_TWITCH, "Conectado.");
	AddLine("MA", "Chat de stream conectado.", false);

	std::string Pending;
	char aRecv[1024];
	while(!m_StopThread.load())
	{
		net_socket_read_wait(Socket, std::chrono::milliseconds(200));
		const int Bytes = net_tcp_recv(Socket, aRecv, sizeof(aRecv) - 1);
		if(Bytes > 0)
		{
			aRecv[Bytes] = '\0';
			Pending.append(aRecv, Bytes);

			size_t LineEnd = std::string::npos;
			while((LineEnd = Pending.find('\n')) != std::string::npos)
			{
				std::string Line = Pending.substr(0, LineEnd);
				Pending.erase(0, LineEnd + 1);
				while(!Line.empty() && (Line.back() == '\r' || Line.back() == '\n'))
					Line.pop_back();

				if(Line.rfind("PING", 0) == 0)
				{
					SendIrcLine(Socket, "PONG :tmi.twitch.tv");
					continue;
				}

				char aUser[32];
				char aMessage[256];
				if(ParseTwitchLine(Line.c_str(), aUser, sizeof(aUser), aMessage, sizeof(aMessage)))
					AddLine(aUser, aMessage);
			}
		}
		else if(Bytes < 0)
		{
			if(net_would_block())
				continue;
			SetStatus("Conexion de Twitch perdida.");
			AddActivity(STREAM_PLATFORM_TWITCH, "Conexion perdida.");
			break;
		}
		else
		{
			SetStatus("Twitch cerro la conexion.");
			AddActivity(STREAM_PLATFORM_TWITCH, "Twitch cerro la conexion.");
			break;
		}
	}

	net_tcp_close(Socket);
	m_ThreadFinished.store(true);
}

void CStreamChat::StopWorker()
{
	if(!m_pThread)
		return;
	m_StopThread.store(true);
	thread_wait(m_pThread);
	m_pThread = nullptr;
	m_StopThread.store(false);
	m_ThreadFinished.store(false);
}

void CStreamChat::StartWorker(const char *pChannel, int Platform)
{
	StopWorker();
	ResetSessionStats();
	m_CurrentPlatform = ClampPlatform(Platform);
	str_copy(m_aCurrentChannel, pChannel, sizeof(m_aCurrentChannel));
	SetStatus("Conectando a Twitch...");
	AddActivity(Platform, "Conectando...");
	m_StopThread.store(false);
	m_ThreadFinished.store(false);
	m_pThread = thread_init(ThreadEntry, this, "stream chat");
}

void CStreamChat::UpdateWorkerState()
{
	if(!g_Config.m_MaStreamChat)
	{
		StopWorker();
		SetStatus("Desactivado.");
		return;
	}

	const int RequestedPlatform = ClampPlatform(g_Config.m_MaStreamChatPlatform);
	if(RequestedPlatform != STREAM_PLATFORM_TWITCH)
	{
		StopWorker();
		if(m_CurrentPlatform != RequestedPlatform)
		{
			ResetSessionStats();
			m_CurrentPlatform = RequestedPlatform;
			AddActivity(RequestedPlatform, RequestedPlatform == STREAM_PLATFORM_YOUTUBE ? "Fuente YouTube preparada." : "Fuente Kick preparada.");
		}
		SetStatus(RequestedPlatform == STREAM_PLATFORM_YOUTUBE ? "YouTube requiere API de Live Chat." : "Kick requiere API/websocket.");
		return;
	}

	char aChannel[128];
	if(!NormalizeTwitchChannel(g_Config.m_MaStreamChatChannel, aChannel, sizeof(aChannel)))
	{
		StopWorker();
		SetStatus("Coloca un canal de Twitch.");
		return;
	}

	const int64_t Now = time_get();
	if(m_pThread && m_ThreadFinished.load())
	{
		thread_wait(m_pThread);
		m_pThread = nullptr;
		m_StopThread.store(false);
		m_ThreadFinished.store(false);
		m_NextReconnectTick = Now + time_freq() * 4;
	}

	const bool ConfigChanged = m_CurrentPlatform != RequestedPlatform || str_comp(m_aCurrentChannel, aChannel) != 0;
	const bool ManualRestart = m_ReconnectRequested.exchange(false);
	if(ConfigChanged || ManualRestart)
	{
		m_NextReconnectTick = 0;
		StartWorker(aChannel, RequestedPlatform);
		return;
	}

	if(!m_pThread && Now >= m_NextReconnectTick)
		StartWorker(aChannel, RequestedPlatform);
}

CUIRect CStreamChat::GetHudEditorRect(bool ForcePreview) const
{
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_STREAM_CHAT, Width, Height);
	if(!ForcePreview && (!g_Config.m_MaStreamChat || !HudLayout::IsEnabled(HudLayout::MODULE_STREAM_CHAT)))
		return CUIRect{};

	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float WidthStretch = std::clamp(Layout.m_WidthScale / 100.0f, 0.20f, 4.0f);
	const float HeightStretch = std::clamp(Layout.m_HeightScale / 100.0f, 0.20f, 4.0f);
	const int MaxLines = ForcePreview ? STREAM_CHAT_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamChatMaxLines, 1, 24);
	const int ContentLines = MaxLines;
	const float BoxWidth = 168.0f * Scale * WidthStretch;
	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const float BoxHeight = (Padding * 2.0f + HeaderFont + 3.0f * Scale + ContentLines * LineFont + maximum(0, ContentLines - 1) * LineGap) * HeightStretch;
	HudLayout::SModuleRect RawRect{Layout.m_X, Layout.m_Y, BoxWidth, BoxHeight, 5.0f * Scale};
	const HudLayout::SModuleRect Clamped = HudLayout::ClampRectToScreen(RawRect, Width, Height);
	return {Clamped.m_X, Clamped.m_Y, Clamped.m_W, Clamped.m_H};
}

CUIRect CStreamChat::GetActivityHudEditorRect(bool ForcePreview) const
{
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_STREAM_ACTIVITY, Width, Height);
	const bool ShowStats = ForcePreview || g_Config.m_MaStreamActivityStats;
	const bool ShowActivity = ForcePreview || g_Config.m_MaStreamActivityFeed;
	if(!ForcePreview && (!g_Config.m_MaStreamChat || (!ShowStats && !ShowActivity) || !HudLayout::IsEnabled(HudLayout::MODULE_STREAM_ACTIVITY)))
		return CUIRect{};

	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float WidthStretch = std::clamp(Layout.m_WidthScale / 100.0f, 0.20f, 4.0f);
	const float HeightStretch = std::clamp(Layout.m_HeightScale / 100.0f, 0.20f, 4.0f);
	const int ActivityLines = ShowActivity ? (ForcePreview ? STREAM_ACTIVITY_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamActivityMaxEvents, 1, 12)) : 0;
	const int StatsLines = ShowStats ? 2 : 0;
	const int ContentLines = maximum(1, ActivityLines + StatsLines);
	const float BoxWidth = 168.0f * Scale * WidthStretch;
	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const float BoxHeight = (Padding * 2.0f + HeaderFont + 3.0f * Scale + ContentLines * LineFont + maximum(0, ContentLines - 1) * LineGap) * HeightStretch;
	HudLayout::SModuleRect RawRect{Layout.m_X, Layout.m_Y, BoxWidth, BoxHeight, 5.0f * Scale};
	const HudLayout::SModuleRect Clamped = HudLayout::ClampRectToScreen(RawRect, Width, Height);
	return {Clamped.m_X, Clamped.m_Y, Clamped.m_W, Clamped.m_H};
}

void CStreamChat::RenderPanel(bool ForcePreview)
{
	if(!ForcePreview)
	{
		UpdateWorkerState();
		UpdateViewerStats();
	}

	if(!ForcePreview && (!g_Config.m_MaStreamChat || !HudLayout::IsEnabled(HudLayout::MODULE_STREAM_CHAT)))
		return;

	float PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1;
	Graphics()->GetScreen(&PrevScreenX0, &PrevScreenY0, &PrevScreenX1, &PrevScreenY1);

	const float Width = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const float Height = HudLayout::CANVAS_HEIGHT;
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const CUIRect Rect = GetHudEditorRect(ForcePreview);
	if(Rect.w <= 0.0f || Rect.h <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_STREAM_CHAT, Width, Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float BackgroundAlpha = ForcePreview ? 0.84f : std::clamp(g_Config.m_MaStreamChatOpacity / 100.0f, 0.0f, 1.0f);
	const float TextAlpha = ForcePreview ? 0.95f : std::clamp(g_Config.m_MaStreamChatTextOpacity / 100.0f, 0.0f, 1.0f);
	if(BackgroundAlpha <= 0.0f && TextAlpha <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	std::vector<SLine> vLines;
	std::vector<SActivity> vActivity;
	char aStatus[128];
	int aPlatformMessages[4] = {};
	int TotalMessages = 0;
	int UniqueChatters = 0;
	int aViewerCount[4] = {-1, -1, -1, -1};
	int aPeakViewerCount[4] = {0, 0, 0, 0};
	char aStatsStatus[128];
	int64_t ConnectedSince = 0;
	{
		CLockScope Lock(m_Lock);
		vLines = m_vLines;
		vActivity = m_vActivity;
		str_copy(aStatus, m_aStatus, sizeof(aStatus));
		str_copy(aStatsStatus, m_aStatsStatus, sizeof(aStatsStatus));
		for(int i = 0; i < 4; ++i)
		{
			aPlatformMessages[i] = m_aPlatformMessages[i];
			aViewerCount[i] = m_aViewerCount[i];
			aPeakViewerCount[i] = m_aPeakViewerCount[i];
		}
		TotalMessages = m_TotalMessages;
		UniqueChatters = (int)m_vKnownUsers.size();
		ConnectedSince = m_ConnectedSince;
	}

	if(ForcePreview)
	{
		vLines.clear();
		SLine Line;
		str_copy(Line.m_aUser, "Eimy", sizeof(Line.m_aUser));
		str_copy(Line.m_aText, "holaa chat", sizeof(Line.m_aText));
		vLines.push_back(Line);
		str_copy(Line.m_aUser, "Leo", sizeof(Line.m_aUser));
		str_copy(Line.m_aText, "se ve perfecto", sizeof(Line.m_aText));
		vLines.push_back(Line);
		str_copy(Line.m_aUser, "JoSzX", sizeof(Line.m_aUser));
		str_copy(Line.m_aText, "W", sizeof(Line.m_aText));
		vLines.push_back(Line);

		vActivity.clear();
		SActivity Activity;
		str_copy(Activity.m_aPlatform, "Twitch", sizeof(Activity.m_aPlatform));
		str_copy(Activity.m_aText, "Conectado.", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(Activity.m_aText, "Mensaje de Eimy", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(Activity.m_aText, "Mensaje de Leo", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(aStatus, "Preview Twitch", sizeof(aStatus));
		aPlatformMessages[STREAM_PLATFORM_TWITCH] = 18;
		TotalMessages = 18;
		UniqueChatters = 5;
		aViewerCount[STREAM_PLATFORM_TWITCH] = 42;
		aPeakViewerCount[STREAM_PLATFORM_TWITCH] = 57;
		str_copy(aStatsStatus, "Espectadores: 42", sizeof(aStatsStatus));
		ConnectedSince = time_get() - time_freq() * 246;
	}

	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const int MaxLines = ForcePreview ? STREAM_CHAT_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamChatMaxLines, 1, 24);
	const bool ShowStats = false;
	const bool ShowActivity = false;
	const int MaxActivityLines = 0;
	const int ActivitySource = ForcePreview ? STREAM_PLATFORM_TWITCH : (g_Config.m_MaStreamActivityPlatform == 0 ? g_Config.m_MaStreamChatPlatform : g_Config.m_MaStreamActivityPlatform);
	const int ActivityPlatform = ClampPlatform(ActivitySource);
	const int Corners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, Rect.x, Rect.y, Rect.w, Rect.h, Width, Height);

	Graphics()->TextureClear();
	if(Layout.m_BackgroundEnabled && BackgroundAlpha > 0.0f)
		Graphics()->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, ColorRGBA(0.015f, 0.017f, 0.035f, 0.78f * BackgroundAlpha), Corners, 5.0f * Scale);

	float x = Rect.x + Padding;
	float y = Rect.y + Padding;
	char aTitle[160];
	char aChannel[128];
	if(NormalizeTwitchChannel(g_Config.m_MaStreamChatChannel, aChannel, sizeof(aChannel)) && ClampPlatform(g_Config.m_MaStreamChatPlatform) == STREAM_PLATFORM_TWITCH)
		str_format(aTitle, sizeof(aTitle), "Twitch: %s", aChannel);
	else
		str_format(aTitle, sizeof(aTitle), "%s: %s", PlatformName(ClampPlatform(g_Config.m_MaStreamChatPlatform)), TCLocalize("Chat de stream"));

	ColorRGBA MessageColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MaStreamChatTextColor));
	ColorRGBA HeaderColor(0.64f, 0.44f, 1.0f, TextAlpha);
	TextRender()->TextColor(HeaderColor);
	TextRender()->Text(x, y, HeaderFont, aTitle, Rect.w - Padding * 2.0f);
	y += HeaderFont + 3.0f * Scale;

	const float TextMaxWidth = maximum(1.0f, Rect.w - Padding * 2.0f);
	const int MaxRowsPerMessage = 3;
	auto WrappedRows = [&](const char *pText, int MaxRows) {
		int LineCount = 1;
		STextSizeProperties Props;
		Props.m_pLineCount = &LineCount;
		TextRender()->TextWidth(LineFont, pText, -1, TextMaxWidth, 0, Props);
		return std::clamp(LineCount, 1, maximum(1, MaxRows));
	};
	auto RenderWrappedText = [&](const char *pText, int Rows, ColorRGBA BaseColor, float AlphaScale) {
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(x, y));
		Cursor.m_FontSize = LineFont;
		Cursor.m_LineWidth = TextMaxWidth;
		Cursor.m_LineSpacing = LineGap;
		Cursor.m_MaxLines = Rows;
		BaseColor.a = std::clamp(BaseColor.a * TextAlpha * AlphaScale, 0.0f, 1.0f);
		TextRender()->TextColor(BaseColor);
		TextRender()->TextEx(&Cursor, pText);
		y += Rows * (LineFont + LineGap);
	};

	if(ShowStats)
	{
		char aUptime[32];
		char aViewers[64];
		char aPeak[32];
		FormatDurationTicks(ConnectedSince, aUptime, sizeof(aUptime));
		FormatViewerCount(aViewerCount[ActivityPlatform], aViewers, sizeof(aViewers));
		if(aViewerCount[ActivityPlatform] < 0 && aPeakViewerCount[ActivityPlatform] <= 0)
			str_copy(aPeak, "-", sizeof(aPeak));
		else
			str_format(aPeak, sizeof(aPeak), "%d", aPeakViewerCount[ActivityPlatform]);
		char aStats[192];
		str_format(aStats, sizeof(aStats), "%s | espectadores %s | pico %s | activo %s", PlatformName(ActivityPlatform), aViewers, aPeak, aUptime);
		RenderWrappedText(aStats, 1, HeaderColor, 0.95f);
		char aStatsAll[192];
		str_format(aStatsAll, sizeof(aStatsAll), "chatters %d | mensajes %d | total mensajes %d", UniqueChatters, aPlatformMessages[ActivityPlatform], TotalMessages);
		RenderWrappedText(aStatsAll, 1, MessageColor, 0.78f);
	}
	if(ShowActivity && MaxActivityLines > 0)
	{
		if(ShowStats)
			y += 2.0f * Scale;
		ColorRGBA ActivityColor(0.56f, 0.82f, 1.0f, TextAlpha);
		struct SRenderActivity
		{
			char m_aText[192];
		};
		std::vector<SRenderActivity> vRenderActivity;
		for(int i = (int)vActivity.size() - 1; i >= 0 && (int)vRenderActivity.size() < MaxActivityLines; --i)
		{
			if(str_comp(vActivity[i].m_aPlatform, PlatformName(ActivityPlatform)) != 0)
				continue;
			SRenderActivity Entry;
			str_format(Entry.m_aText, sizeof(Entry.m_aText), "%s: %s", vActivity[i].m_aPlatform, vActivity[i].m_aText);
			vRenderActivity.push_back(Entry);
		}
		std::reverse(vRenderActivity.begin(), vRenderActivity.end());
		if(vRenderActivity.empty())
		{
			char aActivityStatus[192];
			if(g_Config.m_MaStreamViewerStats && aStatsStatus[0] != '\0')
				str_format(aActivityStatus, sizeof(aActivityStatus), "%s: %s", PlatformName(ActivityPlatform), aStatsStatus);
			else if(ActivityPlatform == STREAM_PLATFORM_TWITCH)
				str_format(aActivityStatus, sizeof(aActivityStatus), "Twitch: %s", aStatus);
			else
				str_format(aActivityStatus, sizeof(aActivityStatus), "%s: API pendiente", PlatformName(ActivityPlatform));
			RenderWrappedText(aActivityStatus, 1, ActivityColor, 0.82f);
		}
		else
		{
			for(const SRenderActivity &Entry : vRenderActivity)
				RenderWrappedText(Entry.m_aText, 1, ActivityColor, 0.86f);
		}
	}

	if(ShowStats || ShowActivity)
		y += 3.0f * Scale;

	if(vLines.empty())
	{
		const int Rows = minimum(WrappedRows(aStatus, MaxLines), MaxLines);
		RenderWrappedText(aStatus, Rows, MessageColor, 0.9f);
	}
	else
	{
		struct SRenderEntry
		{
			char m_aText[320];
			int m_Rows;
		};
		std::vector<SRenderEntry> vRenderEntries;
		int RemainingRows = MaxLines;
		for(int i = (int)vLines.size() - 1; i >= 0 && RemainingRows > 0; i--)
		{
			SRenderEntry Entry;
			str_format(Entry.m_aText, sizeof(Entry.m_aText), "%s: %s", vLines[i].m_aUser, vLines[i].m_aText);
			Entry.m_Rows = minimum(WrappedRows(Entry.m_aText, MaxRowsPerMessage), RemainingRows);
			vRenderEntries.push_back(Entry);
			RemainingRows -= Entry.m_Rows;
		}

		std::reverse(vRenderEntries.begin(), vRenderEntries.end());
		for(const SRenderEntry &Entry : vRenderEntries)
			RenderWrappedText(Entry.m_aText, Entry.m_Rows, MessageColor, 0.95f);
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
}

void CStreamChat::RenderActivityPanel(bool ForcePreview)
{
	if(!ForcePreview && (!g_Config.m_MaStreamChat || !HudLayout::IsEnabled(HudLayout::MODULE_STREAM_ACTIVITY)))
		return;

	const bool ShowStats = ForcePreview || g_Config.m_MaStreamActivityStats;
	const bool ShowActivity = ForcePreview || g_Config.m_MaStreamActivityFeed;
	if(!ForcePreview && !ShowStats && !ShowActivity)
		return;

	float PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1;
	Graphics()->GetScreen(&PrevScreenX0, &PrevScreenY0, &PrevScreenX1, &PrevScreenY1);

	const float Width = HudLayout::CANVAS_HEIGHT * Graphics()->ScreenAspect();
	const float Height = HudLayout::CANVAS_HEIGHT;
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const CUIRect Rect = GetActivityHudEditorRect(ForcePreview);
	if(Rect.w <= 0.0f || Rect.h <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	const auto Layout = HudLayout::Get(HudLayout::MODULE_STREAM_ACTIVITY, Width, Height);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float BackgroundAlpha = ForcePreview ? 0.84f : std::clamp(g_Config.m_MaStreamChatOpacity / 100.0f, 0.0f, 1.0f);
	const float TextAlpha = ForcePreview ? 0.95f : std::clamp(g_Config.m_MaStreamChatTextOpacity / 100.0f, 0.0f, 1.0f);
	if(BackgroundAlpha <= 0.0f && TextAlpha <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	std::vector<SActivity> vActivity;
	char aStatus[128];
	char aStatsStatus[128];
	int aPlatformMessages[4] = {};
	int TotalMessages = 0;
	int UniqueChatters = 0;
	int aViewerCount[4] = {-1, -1, -1, -1};
	int aPeakViewerCount[4] = {0, 0, 0, 0};
	int64_t ConnectedSince = 0;
	{
		CLockScope Lock(m_Lock);
		vActivity = m_vActivity;
		str_copy(aStatus, m_aStatus, sizeof(aStatus));
		str_copy(aStatsStatus, m_aStatsStatus, sizeof(aStatsStatus));
		for(int i = 0; i < 4; ++i)
		{
			aPlatformMessages[i] = m_aPlatformMessages[i];
			aViewerCount[i] = m_aViewerCount[i];
			aPeakViewerCount[i] = m_aPeakViewerCount[i];
		}
		TotalMessages = m_TotalMessages;
		UniqueChatters = (int)m_vKnownUsers.size();
		ConnectedSince = m_ConnectedSince;
	}

	if(ForcePreview)
	{
		vActivity.clear();
		SActivity Activity;
		str_copy(Activity.m_aPlatform, "Twitch", sizeof(Activity.m_aPlatform));
		str_copy(Activity.m_aText, "Conectado.", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(Activity.m_aText, "Mensaje de Eimy", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(Activity.m_aText, "Mensaje de Leo", sizeof(Activity.m_aText));
		vActivity.push_back(Activity);
		str_copy(aStatus, "Preview Twitch", sizeof(aStatus));
		aPlatformMessages[STREAM_PLATFORM_TWITCH] = 18;
		TotalMessages = 18;
		UniqueChatters = 5;
		aViewerCount[STREAM_PLATFORM_TWITCH] = 42;
		aPeakViewerCount[STREAM_PLATFORM_TWITCH] = 57;
		str_copy(aStatsStatus, "Espectadores: 42", sizeof(aStatsStatus));
		ConnectedSince = time_get() - time_freq() * 246;
	}

	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const int MaxActivityLines = ShowActivity ? (ForcePreview ? STREAM_ACTIVITY_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamActivityMaxEvents, 1, 12)) : 0;
	const int ActivitySource = ForcePreview ? STREAM_PLATFORM_TWITCH : (g_Config.m_MaStreamActivityPlatform == 0 ? g_Config.m_MaStreamChatPlatform : g_Config.m_MaStreamActivityPlatform);
	const int ActivityPlatform = ClampPlatform(ActivitySource);
	const int Corners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, Rect.x, Rect.y, Rect.w, Rect.h, Width, Height);

	Graphics()->TextureClear();
	if(Layout.m_BackgroundEnabled && BackgroundAlpha > 0.0f)
		Graphics()->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, ColorRGBA(0.015f, 0.017f, 0.035f, 0.78f * BackgroundAlpha), Corners, 5.0f * Scale);

	float x = Rect.x + Padding;
	float y = Rect.y + Padding;
	ColorRGBA MessageColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MaStreamChatTextColor));
	ColorRGBA HeaderColor(0.64f, 0.44f, 1.0f, TextAlpha);
	TextRender()->TextColor(HeaderColor);
	TextRender()->Text(x, y, HeaderFont, TCLocalize("Fuente de actividad"), Rect.w - Padding * 2.0f);
	y += HeaderFont + 3.0f * Scale;

	const float TextMaxWidth = maximum(1.0f, Rect.w - Padding * 2.0f);
	auto RenderWrappedText = [&](const char *pText, int Rows, ColorRGBA BaseColor, float AlphaScale) {
		CTextCursor Cursor;
		Cursor.SetPosition(vec2(x, y));
		Cursor.m_FontSize = LineFont;
		Cursor.m_LineWidth = TextMaxWidth;
		Cursor.m_LineSpacing = LineGap;
		Cursor.m_MaxLines = Rows;
		BaseColor.a = std::clamp(BaseColor.a * TextAlpha * AlphaScale, 0.0f, 1.0f);
		TextRender()->TextColor(BaseColor);
		TextRender()->TextEx(&Cursor, pText);
		y += Rows * (LineFont + LineGap);
	};

	if(ShowStats)
	{
		char aUptime[32];
		char aViewers[64];
		char aPeak[32];
		FormatDurationTicks(ConnectedSince, aUptime, sizeof(aUptime));
		FormatViewerCount(aViewerCount[ActivityPlatform], aViewers, sizeof(aViewers));
		if(aViewerCount[ActivityPlatform] < 0 && aPeakViewerCount[ActivityPlatform] <= 0)
			str_copy(aPeak, "-", sizeof(aPeak));
		else
			str_format(aPeak, sizeof(aPeak), "%d", aPeakViewerCount[ActivityPlatform]);
		char aStats[192];
		str_format(aStats, sizeof(aStats), "%s | espectadores %s | pico %s | activo %s", PlatformName(ActivityPlatform), aViewers, aPeak, aUptime);
		RenderWrappedText(aStats, 1, HeaderColor, 0.95f);
		char aStatsAll[192];
		str_format(aStatsAll, sizeof(aStatsAll), "chatters %d | mensajes %d | total mensajes %d", UniqueChatters, aPlatformMessages[ActivityPlatform], TotalMessages);
		RenderWrappedText(aStatsAll, 1, MessageColor, 0.78f);
	}
	if(ShowActivity && MaxActivityLines > 0)
	{
		if(ShowStats)
			y += 2.0f * Scale;
		ColorRGBA ActivityColor(0.56f, 0.82f, 1.0f, TextAlpha);
		struct SRenderActivity
		{
			char m_aText[192];
		};
		std::vector<SRenderActivity> vRenderActivity;
		for(int i = (int)vActivity.size() - 1; i >= 0 && (int)vRenderActivity.size() < MaxActivityLines; --i)
		{
			if(str_comp(vActivity[i].m_aPlatform, PlatformName(ActivityPlatform)) != 0)
				continue;
			SRenderActivity Entry;
			str_format(Entry.m_aText, sizeof(Entry.m_aText), "%s: %s", vActivity[i].m_aPlatform, vActivity[i].m_aText);
			vRenderActivity.push_back(Entry);
		}
		std::reverse(vRenderActivity.begin(), vRenderActivity.end());
		if(vRenderActivity.empty())
		{
			char aActivityStatus[192];
			if(g_Config.m_MaStreamViewerStats && aStatsStatus[0] != '\0')
				str_format(aActivityStatus, sizeof(aActivityStatus), "%s: %s", PlatformName(ActivityPlatform), aStatsStatus);
			else if(ActivityPlatform == STREAM_PLATFORM_TWITCH)
				str_format(aActivityStatus, sizeof(aActivityStatus), "Twitch: %s", aStatus);
			else
				str_format(aActivityStatus, sizeof(aActivityStatus), "%s: API pendiente", PlatformName(ActivityPlatform));
			RenderWrappedText(aActivityStatus, 1, ActivityColor, 0.82f);
		}
		else
		{
			for(const SRenderActivity &Entry : vRenderActivity)
				RenderWrappedText(Entry.m_aText, 1, ActivityColor, 0.86f);
		}
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
}

void CStreamChat::RenderHudEditor(bool ForcePreview)
{
	RenderPanel(ForcePreview);
}

void CStreamChat::RenderActivityHudEditor(bool ForcePreview)
{
	RenderActivityPanel(ForcePreview);
}

void CStreamChat::OnRender()
{
	RenderPanel(false);
	RenderActivityPanel(false);
}