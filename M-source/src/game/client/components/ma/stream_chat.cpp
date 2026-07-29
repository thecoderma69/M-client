#include "stream_chat.h"

#include <base/math.h>
#include <base/net.h>
#include <base/str.h>
#include <base/system.h>
#include <base/thread.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
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
constexpr int STREAM_CHAT_MAX_LINES = 64;
constexpr int STREAM_CHAT_PREVIEW_LINES = 5;

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

} // namespace

CStreamChat::~CStreamChat()
{
	StopWorker();
}

void CStreamChat::OnInit()
{
	SetStatus("Desactivado.");
}

void CStreamChat::OnReset()
{
	ClearLines();
}

void CStreamChat::OnShutdown()
{
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

void CStreamChat::ClearLines()
{
	CLockScope Lock(m_Lock);
	m_vLines.clear();
}

void CStreamChat::AddLine(const char *pUser, const char *pText)
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
		m_ThreadFinished.store(true);
		return;
	}

	NETADDR BindAddr = NETADDR_ZEROED;
	BindAddr.type = Addr.type & (NETTYPE_IPV4 | NETTYPE_IPV6);
	NETSOCKET Socket = net_tcp_create(BindAddr);
	if(!Socket)
	{
		SetStatus("No se pudo crear socket.");
		m_ThreadFinished.store(true);
		return;
	}

	if(net_tcp_connect(Socket, &Addr) != 0)
	{
		net_tcp_close(Socket);
		SetStatus("No se pudo conectar a Twitch.");
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
		m_ThreadFinished.store(true);
		return;
	}

	char aJoin[160];
	str_format(aJoin, sizeof(aJoin), "JOIN #%s", aChannel);
	if(!SendIrcLine(Socket, aJoin))
	{
		net_tcp_close(Socket);
		SetStatus("No se pudo entrar al canal.");
		m_ThreadFinished.store(true);
		return;
	}

	net_set_non_blocking(Socket);
	SetStatus("Conectado a Twitch.");
	AddLine("MΛ ツ", "Chat de stream conectado.");

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
			break;
		}
		else
		{
			SetStatus("Twitch cerro la conexion.");
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
	ClearLines();
	m_CurrentPlatform = Platform;
	str_copy(m_aCurrentChannel, pChannel, sizeof(m_aCurrentChannel));
	SetStatus("Conectando a Twitch...");
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

	if(g_Config.m_MaStreamChatPlatform != STREAM_PLATFORM_TWITCH)
	{
		StopWorker();
		SetStatus("Solo Twitch en esta version.");
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

	const bool ConfigChanged = m_CurrentPlatform != g_Config.m_MaStreamChatPlatform || str_comp(m_aCurrentChannel, aChannel) != 0;
	const bool ManualRestart = m_ReconnectRequested.exchange(false);
	if(ConfigChanged || ManualRestart)
	{
		m_NextReconnectTick = 0;
		StartWorker(aChannel, g_Config.m_MaStreamChatPlatform);
		return;
	}

	if(!m_pThread && Now >= m_NextReconnectTick)
		StartWorker(aChannel, g_Config.m_MaStreamChatPlatform);
}

CUIRect CStreamChat::GetHudEditorRect(bool ForcePreview) const
{
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	const auto Layout = HudLayout::Get(HudLayout::MODULE_STREAM_CHAT, Width, Height);
	if(!ForcePreview && (!g_Config.m_MaStreamChat || !HudLayout::IsEnabled(HudLayout::MODULE_STREAM_CHAT)))
		return CUIRect{};

	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const int MaxLines = ForcePreview ? STREAM_CHAT_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamChatMaxLines, 1, 24);
	const float BoxWidth = 168.0f * Scale;
	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const float BoxHeight = Padding * 2.0f + HeaderFont + 3.0f * Scale + MaxLines * LineFont + maximum(0, MaxLines - 1) * LineGap;
	HudLayout::SModuleRect RawRect{Layout.m_X, Layout.m_Y, BoxWidth, BoxHeight, 5.0f * Scale};
	const HudLayout::SModuleRect Clamped = HudLayout::ClampRectToScreen(RawRect, Width, Height);
	return {Clamped.m_X, Clamped.m_Y, Clamped.m_W, Clamped.m_H};
}

void CStreamChat::RenderPanel(bool ForcePreview)
{
	if(!ForcePreview)
		UpdateWorkerState();

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
	const float Alpha = ForcePreview ? 0.84f : std::clamp(g_Config.m_MaStreamChatOpacity / 100.0f, 0.0f, 1.0f);
	if(Alpha <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	std::vector<SLine> vLines;
	char aStatus[128];
	{
		CLockScope Lock(m_Lock);
		vLines = m_vLines;
		str_copy(aStatus, m_aStatus, sizeof(aStatus));
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
		str_copy(aStatus, "Preview Twitch", sizeof(aStatus));
	}

	const float Padding = 5.0f * Scale;
	const float HeaderFont = 6.8f * Scale;
	const float LineFont = 5.2f * Scale;
	const float LineGap = 1.3f * Scale;
	const int MaxLines = ForcePreview ? STREAM_CHAT_PREVIEW_LINES : std::clamp(g_Config.m_MaStreamChatMaxLines, 1, 24);
	const int Corners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, Rect.x, Rect.y, Rect.w, Rect.h, Width, Height);

	Graphics()->TextureClear();
	if(Layout.m_BackgroundEnabled)
		Graphics()->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, ColorRGBA(0.015f, 0.017f, 0.035f, 0.78f * Alpha), Corners, 5.0f * Scale);

	float x = Rect.x + Padding;
	float y = Rect.y + Padding;
	char aTitle[160];
	char aChannel[128];
	if(NormalizeTwitchChannel(g_Config.m_MaStreamChatChannel, aChannel, sizeof(aChannel)))
		str_format(aTitle, sizeof(aTitle), "Twitch: %s", aChannel);
	else
		str_copy(aTitle, TCLocalize("Chat de stream"), sizeof(aTitle));

	TextRender()->TextColor(ColorRGBA(0.64f, 0.44f, 1.0f, Alpha));
	TextRender()->Text(x, y, HeaderFont, aTitle, Rect.w - Padding * 2.0f);
	y += HeaderFont + 3.0f * Scale;

	int Start = maximum(0, (int)vLines.size() - MaxLines);
	if(vLines.empty())
	{
		TextRender()->TextColor(ColorRGBA(0.82f, 0.84f, 0.92f, Alpha * 0.9f));
		TextRender()->Text(x, y, LineFont, aStatus, Rect.w - Padding * 2.0f);
	}
	else
	{
		for(int i = Start; i < (int)vLines.size(); i++)
		{
			char aLine[320];
			str_format(aLine, sizeof(aLine), "%s: %s", vLines[i].m_aUser, vLines[i].m_aText);
			TextRender()->TextColor(ColorRGBA(0.92f, 0.93f, 0.98f, Alpha * 0.95f));
			TextRender()->Text(x, y, LineFont, aLine, Rect.w - Padding * 2.0f);
			y += LineFont + LineGap;
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
	Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
}

void CStreamChat::RenderHudEditor(bool ForcePreview)
{
	RenderPanel(ForcePreview);
}

void CStreamChat::OnRender()
{
	RenderPanel(false);
}