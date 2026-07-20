/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "chat.h"

#include <base/io.h>
#include <base/log.h>
#include <base/time.h>

#include <engine/engine.h>
#include <engine/editor.h>
#include <engine/external/regex.h>
#include <engine/font_icons.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/shared/config.h>
#include <engine/shared/csv.h>
#include <engine/shared/http.h>
#include <engine/textrender.h>

#include <generated/protocol.h>
#include <generated/protocol7.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>

#include <game/client/animstate.h>
#include <game/client/components/censor.h>
#include <game/client/components/scoreboard.h>
#include <game/client/components/skins.h>
#include <game/client/components/sounds.h>
#include <game/client/components/tclient/colored_parts.h>
#include <game/client/gameclient.h>
#include <game/client/components/hud_layout.h>
#include <game/client/ui_scrollregion.h>
#include <game/localization.h>

char CChat::ms_aDisplayText[MAX_LINE_LENGTH] = "";
static constexpr int CHAT_MEDIA_MAX_CONCURRENT_DOWNLOADS = 3;
static constexpr int CHAT_MEDIA_MAX_COMPLETED_PER_FRAME = 1;
static constexpr int CHAT_MEDIA_MAX_TEXTURE_UPLOADS_PER_FRAME = 3;
static constexpr int64_t CHAT_MEDIA_TEXTURE_UPLOAD_BUDGET_US = 2500;
static constexpr int64_t CHAT_MEDIA_MAX_RESPONSE_SIZE = 64 * 1024 * 1024;
static constexpr int CHAT_MEDIA_MAX_GIF_FRAMES = 180;
static constexpr int CHAT_MEDIA_MAX_DIMENSION = 960;
static constexpr int CHAT_MEDIA_MAX_RESOLVE_DEPTH = 2;
static constexpr int CHAT_MEDIA_MAX_VIDEO_ANIMATION_MS = 15000;
static constexpr int CHAT_MEDIA_MAX_URL_LENGTH = 240;
static constexpr int CHAT_MEDIA_MAX_HTML_CANDIDATES = 32;
static constexpr size_t CHAT_MEDIA_MAX_ANIMATED_MEMORY_BYTES = 48ull * 1024ull * 1024ull;
static constexpr float CHAT_MEDIA_MAX_PREVIEW_HEIGHT = 70.0f;
static constexpr float CHAT_MEDIA_MAX_PREVIEW_HEIGHT_SCOREBOARD = 56.0f;
static constexpr float CHAT_MEDIA_PREVIEW_SIZE_SCALE = 0.9f;
static constexpr float CHAT_MEDIA_MIN_PREVIEW_SIDE = 28.0f;
static constexpr float CHAT_MEDIA_COMPACT_EXPANDED_HEIGHT = 150.0f;

CChat::CLine::CLine()
{
	m_TextContainerIndex.Reset();
	m_QuadContainerIndex = -1;
	m_MediaState = EMediaState::NONE;
	m_MediaKind = EMediaKind::UNKNOWN;
	m_aMediaUrl[0] = '\0';
	m_aMediaStatus[0] = '\0';
	m_MediaCandidateIndex = -1;
	m_MediaResolveDepth = 0;
	m_MediaUploadIndex = 0;
	m_MediaTotalDurationMs = 0;
	m_MediaAnimated = false;
	m_MediaWidth = 0;
	m_MediaHeight = 0;
	m_MediaAnimationStart = 0;
	m_aTextHeight[0] = 0.0f;
	m_aTextHeight[1] = 0.0f;
	m_aMediaPreviewWidth[0] = 0.0f;
	m_aMediaPreviewWidth[1] = 0.0f;
	m_aMediaPreviewHeight[0] = 0.0f;
	m_aMediaPreviewHeight[1] = 0.0f;
}

void CChat::CLine::Reset(CChat &This)
{
	This.TextRender()->DeleteTextContainer(m_TextContainerIndex);
	This.Graphics()->DeleteQuadContainer(m_QuadContainerIndex);
	This.ResetLineMedia(*this);
	m_Initialized = false;
	m_Time = 0;
	m_aText[0] = '\0';
	m_aName[0] = '\0';
	m_Friend = false;
	m_TimesRepeated = 0;
	m_pManagedTeeRenderInfo = nullptr;
	m_pTranslateResponse = nullptr;
}

class CChat::CMediaDecodeJob : public IJob
{
	EMediaKind m_MediaKind;
	IGraphics *m_pGraphics;
	std::vector<unsigned char> m_vData;
	char m_aContextName[512];
	SMediaDecodedFrames m_DecodedFrames;
	bool m_Success = false;

protected:
	void Run() override
	{
		if(State() == IJob::STATE_ABORTED || m_vData.empty())
			return;

		auto DecodeSingleFrameFallback = [&]() -> bool {
			CImageInfo Image;
			if(!MediaDecoder::DecodeImageToRgba(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, Image))
				return false;

			m_DecodedFrames.Free();
			m_DecodedFrames.m_Width = (int)Image.m_Width;
			m_DecodedFrames.m_Height = (int)Image.m_Height;
			m_DecodedFrames.m_Animated = false;
			m_DecodedFrames.m_AnimationStart = time_get();

			SMediaRawFrame Frame;
			Frame.m_DurationMs = 100;
			Frame.m_Image = std::move(Image);
			m_DecodedFrames.m_vFrames.push_back(std::move(Frame));
			return !m_DecodedFrames.m_vFrames.empty();
		};

		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = CHAT_MEDIA_MAX_DIMENSION;
		Limits.m_MaxFrames = CHAT_MEDIA_MAX_GIF_FRAMES;
		Limits.m_MaxTotalBytes = CHAT_MEDIA_MAX_ANIMATED_MEMORY_BYTES;
		Limits.m_MaxAnimationDurationMs = CHAT_MEDIA_MAX_VIDEO_ANIMATION_MS;

		switch(m_MediaKind)
		{
		case EMediaKind::PHOTO:
			m_Success = MediaDecoder::DecodeStaticImageCpu(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, m_DecodedFrames, CHAT_MEDIA_MAX_DIMENSION);
			if(!m_Success)
				m_Success = DecodeSingleFrameFallback();
			break;
		case EMediaKind::ANIMATED:
			Limits.m_DecodeAllFrames = true;
			m_Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, m_DecodedFrames, Limits);
			if(!m_Success)
			{
				Limits.m_DecodeAllFrames = false;
				m_Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, m_DecodedFrames, Limits);
			}
			if(!m_Success)
				m_Success = DecodeSingleFrameFallback();
			break;
		case EMediaKind::VIDEO:
			Limits.m_DecodeAllFrames = false;
			m_Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, m_DecodedFrames, Limits);
			if(!m_Success)
			{
				Limits.m_DecodeAllFrames = true;
				m_Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, m_vData.data(), m_vData.size(), m_aContextName, m_DecodedFrames, Limits);
			}
			if(!m_Success)
				m_Success = DecodeSingleFrameFallback();
			break;
		case EMediaKind::UNKNOWN:
		default:
			m_Success = false;
			break;
		}

		if(State() == IJob::STATE_ABORTED)
		{
			m_Success = false;
			m_DecodedFrames.Free();
		}
	}

public:
	CMediaDecodeJob(IGraphics *pGraphics, EMediaKind MediaKind, const unsigned char *pData, size_t DataSize, const char *pContextName) :
		m_MediaKind(MediaKind),
		m_pGraphics(pGraphics),
		m_vData(pData, pData + DataSize)
	{
		str_copy(m_aContextName, pContextName ? pContextName : "chat_media", sizeof(m_aContextName));
		Abortable(true);
	}

	~CMediaDecodeJob() override
	{
		m_DecodedFrames.Free();
	}

	bool Success() const { return m_Success; }
	SMediaDecodedFrames &DecodedFrames() { return m_DecodedFrames; }
};

namespace
{
static bool IsUrlStart(const char *pStr)
{
	return str_startswith(pStr, "http://") || str_startswith(pStr, "https://");
}

static bool IsTokenEnd(char c)
{
	return c == '\0' || c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

static bool IsTrimmedUrlChar(char c)
{
	return c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' ||
		c == ')' || c == ']' || c == '}' || c == '"' || c == '\'' || c == '>';
}

static std::string ToLowerAscii(std::string Value)
{
	std::transform(Value.begin(), Value.end(), Value.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return Value;
}

static void TrimAsciiWhitespace(std::string &Value)
{
	while(!Value.empty() && std::isspace((unsigned char)Value.front()))
		Value.erase(Value.begin());
	while(!Value.empty() && std::isspace((unsigned char)Value.back()))
		Value.pop_back();
}

static std::string TrimAsciiWhitespaceCopy(std::string Value)
{
	TrimAsciiWhitespace(Value);
	return Value;
}

static std::string ExtractUrlHostLower(const std::string &Url)
{
	const size_t SchemePos = Url.find("://");
	if(SchemePos == std::string::npos)
		return {};

	const size_t HostStart = SchemePos + 3;
	const size_t HostEnd = Url.find_first_of("/?#", HostStart);
	std::string HostPort = Url.substr(HostStart, HostEnd == std::string::npos ? std::string::npos : HostEnd - HostStart);

	const size_t AtPos = HostPort.rfind('@');
	if(AtPos != std::string::npos)
		HostPort = HostPort.substr(AtPos + 1);

	if(!HostPort.empty() && HostPort.front() == '[')
	{
		const size_t Close = HostPort.find(']');
		if(Close != std::string::npos)
			HostPort = HostPort.substr(1, Close - 1);
	}
	else
	{
		const size_t ColonPos = HostPort.find(':');
		if(ColonPos != std::string::npos)
			HostPort = HostPort.substr(0, ColonPos);
	}

	while(!HostPort.empty() && HostPort.back() == '.')
		HostPort.pop_back();

	return ToLowerAscii(HostPort);
}

static bool HostIsOrEndsWith(const std::string &HostLower, const char *pDomainLower)
{
	const std::string Domain(pDomainLower);
	if(HostLower == Domain)
		return true;
	if(HostLower.size() <= Domain.size())
		return false;
	const size_t Start = HostLower.size() - Domain.size();
	return HostLower.compare(Start, Domain.size(), Domain) == 0 && HostLower[Start - 1] == '.';
}

static std::string NormalizeAllowedMediaDomain(std::string Domain)
{
	Domain = TrimAsciiWhitespaceCopy(std::move(Domain));
	Domain = ToLowerAscii(std::move(Domain));

	const size_t SchemePos = Domain.find("://");
	if(SchemePos != std::string::npos)
		Domain = Domain.substr(SchemePos + 3);

	const size_t AtPos = Domain.rfind('@');
	if(AtPos != std::string::npos)
		Domain = Domain.substr(AtPos + 1);

	if(!Domain.empty() && Domain.front() == '[')
	{
		const size_t ClosePos = Domain.find(']');
		if(ClosePos != std::string::npos)
			Domain = Domain.substr(1, ClosePos - 1);
	}
	else
	{
		const size_t SlashPos = Domain.find_first_of("/?#");
		if(SlashPos != std::string::npos)
			Domain.resize(SlashPos);
		const size_t ColonPos = Domain.find(':');
		if(ColonPos != std::string::npos)
			Domain.resize(ColonPos);
	}

	while(!Domain.empty() && (Domain.front() == '.' || std::isspace((unsigned char)Domain.front())))
		Domain.erase(Domain.begin());
	while(!Domain.empty() && (Domain.back() == '.' || std::isspace((unsigned char)Domain.back())))
		Domain.pop_back();

	return Domain;
}

static constexpr const char *s_pDefaultChatMediaAllowedDomains = "tenor.com; tenor.googleapis.com; imgur.com; giphy.com; discordapp.com; discordapp.net";

static bool IsAllowedChatMediaHostByDomainList(const std::string &HostLower, const char *pList, bool &HasDomains)
{
	HasDomains = false;
	if(pList == nullptr || pList[0] == '\0')
		return false;

	const char *pTokenStart = pList;
	while(true)
	{
		const char *pSep = str_find(pTokenStart, ";");
		const size_t TokenLen = pSep ? (size_t)(pSep - pTokenStart) : str_length(pTokenStart);
		std::string Domain = NormalizeAllowedMediaDomain(std::string(pTokenStart, TokenLen));
		if(!Domain.empty())
		{
			HasDomains = true;
			if(HostLower == Domain)
				return true;
			if(HostLower.size() > Domain.size())
			{
				const size_t Start = HostLower.size() - Domain.size();
				if(HostLower.compare(Start, Domain.size(), Domain) == 0 && HostLower[Start - 1] == '.')
					return true;
			}
		}

		if(!pSep)
			break;
		pTokenStart = pSep + 1;
	}

	return false;
}

static bool IsAllowedChatMediaHost(const std::string &HostLower)
{
	if(!g_Config.m_MaChatMediaContentFilter)
		return true;
	if(HostLower.empty())
		return false;

	bool HasConfiguredDomains = false;
	if(IsAllowedChatMediaHostByDomainList(HostLower, g_Config.m_MaChatMediaAllowedDomains, HasConfiguredDomains))
		return true;
	if(HasConfiguredDomains)
		return false;

	bool HasDefaultDomains = false;
	return IsAllowedChatMediaHostByDomainList(HostLower, s_pDefaultChatMediaAllowedDomains, HasDefaultDomains);
}

static bool IsAllowedChatMediaUrl(const char *pUrl)
{
	if(!g_Config.m_MaChatMediaContentFilter)
		return true;
	if(pUrl == nullptr || pUrl[0] == '\0')
		return false;
	const std::string HostLower = ExtractUrlHostLower(pUrl);
	if(HostLower == "tenor.googleapis.com" || HostIsOrEndsWith(HostLower, "tenor.com"))
		return true;
	return IsAllowedChatMediaHost(HostLower);
}

static bool IsYouTubeUrl(const std::string &Url)
{
	const std::string HostLower = ExtractUrlHostLower(Url);
	if(HostLower.empty())
		return false;

	return HostIsOrEndsWith(HostLower, "youtube.com") ||
		HostIsOrEndsWith(HostLower, "youtu.be") ||
		HostIsOrEndsWith(HostLower, "youtube-nocookie.com") ||
		HostIsOrEndsWith(HostLower, "ytimg.com") ||
		HostIsOrEndsWith(HostLower, "googlevideo.com");
}

static std::string ExtractUrlPath(const std::string &Url)
{
	const size_t SchemePos = Url.find("://");
	if(SchemePos == std::string::npos)
		return {};

	const size_t PathStart = Url.find('/', SchemePos + 3);
	if(PathStart == std::string::npos)
		return "/";

	const size_t PathEnd = Url.find_first_of("?#", PathStart);
	return Url.substr(PathStart, PathEnd == std::string::npos ? std::string::npos : PathEnd - PathStart);
}

static bool ExtractGiphyMediaId(const std::string &Url, std::string &OutMediaId)
{
	OutMediaId.clear();
	const std::string HostLower = ExtractUrlHostLower(Url);
	if(!HostIsOrEndsWith(HostLower, "giphy.com"))
		return false;

	const std::string Path = ExtractUrlPath(Url);
	if(Path.empty() || Path.find("/gifs/") == std::string::npos)
		return false;

	size_t SegmentStart = Path.find_last_of('/');
	if(SegmentStart == std::string::npos || SegmentStart + 1 >= Path.size())
		return false;

	std::string LastSegment = Path.substr(SegmentStart + 1);
	const size_t DashPos = LastSegment.find_last_of('-');
	if(DashPos != std::string::npos && DashPos + 1 < LastSegment.size())
		LastSegment = LastSegment.substr(DashPos + 1);

	if(LastSegment.size() < 6 || LastSegment.size() > 64)
		return false;

	for(char c : LastSegment)
	{
		if(!std::isalnum((unsigned char)c))
			return false;
	}

	OutMediaId = LastSegment;
	return true;
}

static void AddDirectGiphyCandidates(const std::string &Url, std::vector<std::string> &vOutCandidates)
{
	std::string MediaId;
	if(!ExtractGiphyMediaId(Url, MediaId))
		return;

	const char *apHosts[] = {"https://media.giphy.com/media/", "https://media1.giphy.com/media/"};
	const char *apFormats[] = {"giphy.gif", "giphy.webp", "giphy.mp4"};
	for(const char *pHost : apHosts)
	{
		for(const char *pFormat : apFormats)
		{
			std::string Candidate = std::string(pHost) + MediaId + "/" + pFormat;
			if((int)Candidate.size() <= CHAT_MEDIA_MAX_URL_LENGTH)
				vOutCandidates.push_back(std::move(Candidate));
		}
	}
}

static bool ExtractTenorMediaId(const std::string &Url, std::string &OutMediaId)
{
	OutMediaId.clear();
	const std::string HostLower = ExtractUrlHostLower(Url);
	if(!HostIsOrEndsWith(HostLower, "tenor.com"))
		return false;

	const std::string Path = ExtractUrlPath(Url);
	if(Path.empty())
		return false;

	size_t IdStart = Path.rfind("-gif-");
	if(IdStart != std::string::npos)
		IdStart += 5;
	else
	{
		const size_t LastDash = Path.find_last_of('-');
		if(LastDash == std::string::npos)
			return false;
		IdStart = LastDash + 1;
	}

	if(IdStart >= Path.size())
		return false;

	size_t IdEnd = IdStart;
	while(IdEnd < Path.size() && std::isdigit((unsigned char)Path[IdEnd]))
		IdEnd++;

	if(IdEnd == IdStart)
		return false;

	std::string MediaId = Path.substr(IdStart, IdEnd - IdStart);
	if(MediaId.size() < 5 || MediaId.size() > 32)
		return false;

	OutMediaId = std::move(MediaId);
	return true;
}

static void AddDirectTenorCandidates(const std::string &Url, std::vector<std::string> &vOutCandidates)
{
	std::string MediaId;
	if(!ExtractTenorMediaId(Url, MediaId))
		return;

	const std::string Candidate = "https://tenor.googleapis.com/v2/posts?ids=" + MediaId + "&key=AIzaSyCZt6SSh5VgVPzD9fhyzG1DprdPRhtoaR4&client_key=tenor_web&media_filter=gif,tinygif,mp4,tinymp4";
	if((int)Candidate.size() <= CHAT_MEDIA_MAX_URL_LENGTH)
		vOutCandidates.push_back(Candidate);
}

static bool ExtractImgurMediaId(const std::string &Url, std::string &OutMediaId)
{
	OutMediaId.clear();
	const std::string HostLower = ExtractUrlHostLower(Url);
	if(!HostIsOrEndsWith(HostLower, "imgur.com"))
		return false;

	const std::string Path = ExtractUrlPath(Url);
	if(Path.empty() || Path == "/")
		return false;

	const char *apPrefixes[] = {"/a/", "/gallery/", "/t/"};
	for(const char *pPrefix : apPrefixes)
	{
		if(str_startswith(Path.c_str(), pPrefix))
			return false;
	}

	size_t SegmentStart = Path.find_last_of('/');
	if(SegmentStart == std::string::npos || SegmentStart + 1 >= Path.size())
		return false;

	std::string LastSegment = Path.substr(SegmentStart + 1);
	const size_t DotPos = LastSegment.find('.');
	if(DotPos != std::string::npos)
		LastSegment.resize(DotPos);

	if(LastSegment.size() < 4 || LastSegment.size() > 16)
		return false;

	for(char c : LastSegment)
	{
		if(!std::isalnum((unsigned char)c))
			return false;
	}

	OutMediaId = LastSegment;
	return true;
}

static void AddDirectImgurCandidates(const std::string &Url, std::vector<std::string> &vOutCandidates)
{
	std::string MediaId;
	if(!ExtractImgurMediaId(Url, MediaId))
		return;

	const char *apFormats[] = {"mp4", "gif", "webm", "jpg", "jpeg", "png", "webp"};
	for(const char *pFormat : apFormats)
	{
		std::string Candidate = "https://i.imgur.com/" + MediaId + "." + pFormat;
		if((int)Candidate.size() <= CHAT_MEDIA_MAX_URL_LENGTH)
			vOutCandidates.push_back(std::move(Candidate));
	}
}

static bool IsGifSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 6 && (mem_comp(pData, "GIF87a", 6) == 0 || mem_comp(pData, "GIF89a", 6) == 0);
}

static std::string ExtractUrlExtensionLower(const std::string &Url)
{
	const size_t QueryPos = Url.find_first_of("?#");
	const std::string Path = Url.substr(0, QueryPos);
	const size_t SlashPos = Path.find_last_of('/');
	const size_t DotPos = Path.find_last_of('.');
	if(DotPos == std::string::npos || (SlashPos != std::string::npos && DotPos < SlashPos))
		return {};

	std::string Ext = Path.substr(DotPos + 1);
	return ToLowerAscii(std::move(Ext));
}

static bool IsLikelyImageExtension(const std::string &Ext)
{
	return Ext == "png" || Ext == "jpg" || Ext == "jpeg" || Ext == "gif" || Ext == "webp" || Ext == "bmp" || Ext == "avif" || Ext == "apng";
}

static bool IsLikelyAnimatedImageExtension(const std::string &Ext)
{
	return Ext == "gif" || Ext == "webp" || Ext == "apng" || Ext == "avif";
}

static bool IsLikelyVideoExtension(const std::string &Ext)
{
	return Ext == "mp4" || Ext == "webm" || Ext == "mov" || Ext == "m4v" || Ext == "mkv" || Ext == "avi" ||
		Ext == "gifv" || Ext == "mpg" || Ext == "mpeg" || Ext == "ogv" || Ext == "3gp" || Ext == "3g2" ||
		Ext == "flv" || Ext == "wmv" || Ext == "asf" || Ext == "ts" || Ext == "m2ts" || Ext == "mts" || Ext == "f4v";
}

static bool IsBlockedMediaExtension(const std::string &Ext)
{
	return Ext == "svg" || Ext == "svgz" || Ext == "ico" || Ext == "css" || Ext == "js" || Ext == "json" || Ext == "txt" || Ext == "xml" || Ext == "pdf" || Ext == "html" || Ext == "htm";
}

static bool IsLikelyMediaExtension(const std::string &Ext)
{
	return IsLikelyImageExtension(Ext) || IsLikelyVideoExtension(Ext);
}

static bool IsPngSignature(const unsigned char *pData, size_t DataSize)
{
	static const unsigned char s_aPngSig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	return DataSize >= 8 && mem_comp(pData, s_aPngSig, 8) == 0;
}

static bool IsJpegSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 3 && pData[0] == 0xff && pData[1] == 0xd8 && pData[2] == 0xff;
}

static bool IsWebpSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 12 && mem_comp(pData, "RIFF", 4) == 0 && mem_comp(pData + 8, "WEBP", 4) == 0;
}

static bool IsBmpSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 2 && pData[0] == 'B' && pData[1] == 'M';
}

static bool IsMp4LikeSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 12 && mem_comp(pData + 4, "ftyp", 4) == 0;
}

static bool IsWebmSignature(const unsigned char *pData, size_t DataSize)
{
	static const unsigned char s_aWebmSig[4] = {0x1a, 0x45, 0xdf, 0xa3};
	return DataSize >= 4 && mem_comp(pData, s_aWebmSig, 4) == 0;
}

static bool IsAviSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 12 && mem_comp(pData, "RIFF", 4) == 0 && mem_comp(pData + 8, "AVI ", 4) == 0;
}

static bool IsOggSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 4 && mem_comp(pData, "OggS", 4) == 0;
}

static bool IsImagePayloadSignature(const unsigned char *pData, size_t DataSize)
{
	return IsPngSignature(pData, DataSize) || IsJpegSignature(pData, DataSize) || IsGifSignature(pData, DataSize) || IsWebpSignature(pData, DataSize) || IsBmpSignature(pData, DataSize);
}

static bool IsVideoPayloadSignature(const unsigned char *pData, size_t DataSize)
{
	return IsMp4LikeSignature(pData, DataSize) || IsWebmSignature(pData, DataSize) || IsOggSignature(pData, DataSize) || IsAviSignature(pData, DataSize);
}

static void ReplaceAll(std::string &Value, const char *pFrom, const char *pTo)
{
	const std::string From(pFrom);
	const std::string To(pTo);
	size_t Pos = 0;
	while((Pos = Value.find(From, Pos)) != std::string::npos)
	{
		Value.replace(Pos, From.size(), To);
		Pos += To.size();
	}
}

static std::string DecodeHtmlUrl(std::string Value)
{
	ReplaceAll(Value, "&amp;", "&");
	ReplaceAll(Value, "&quot;", "\"");
	ReplaceAll(Value, "&apos;", "'");
	ReplaceAll(Value, "&#39;", "'");
	ReplaceAll(Value, "&#x27;", "'");
	ReplaceAll(Value, "&lt;", "<");
	ReplaceAll(Value, "&gt;", ">");
	ReplaceAll(Value, "\\\\u002F", "/");
	ReplaceAll(Value, "\\\\u002f", "/");
	ReplaceAll(Value, "\\\\u003A", ":");
	ReplaceAll(Value, "\\\\u003a", ":");
	ReplaceAll(Value, "\\\\u0026", "&");
	ReplaceAll(Value, "\\\\u003D", "=");
	ReplaceAll(Value, "\\\\u003d", "=");
	ReplaceAll(Value, "\\\\u003F", "?");
	ReplaceAll(Value, "\\\\u003f", "?");
	ReplaceAll(Value, "\\u002F", "/");
	ReplaceAll(Value, "\\u002f", "/");
	ReplaceAll(Value, "\\u003A", ":");
	ReplaceAll(Value, "\\u003a", ":");
	ReplaceAll(Value, "\\u0026", "&");
	ReplaceAll(Value, "\\u003D", "=");
	ReplaceAll(Value, "\\u003d", "=");
	ReplaceAll(Value, "\\u003F", "?");
	ReplaceAll(Value, "\\u003f", "?");
	ReplaceAll(Value, "\\\\/", "/");
	ReplaceAll(Value, "\\/", "/");
	return Value;
}

static bool ExtractHtmlAttribute(const std::string &Tag, const std::string &TagLower, const char *pAttrName, std::string &OutValue)
{
	const std::string AttrName = ToLowerAscii(pAttrName);
	size_t Pos = 0;
	while((Pos = TagLower.find(AttrName, Pos)) != std::string::npos)
	{
		const bool LeftBoundary = Pos == 0 || std::isspace((unsigned char)TagLower[Pos - 1]) || TagLower[Pos - 1] == '<' || TagLower[Pos - 1] == '/';
		if(!LeftBoundary)
		{
			Pos += AttrName.size();
			continue;
		}

		size_t EqPos = Pos + AttrName.size();
		while(EqPos < TagLower.size() && std::isspace((unsigned char)TagLower[EqPos]))
			EqPos++;
		if(EqPos >= TagLower.size() || TagLower[EqPos] != '=')
		{
			Pos += AttrName.size();
			continue;
		}
		EqPos++;
		while(EqPos < Tag.size() && std::isspace((unsigned char)Tag[EqPos]))
			EqPos++;
		if(EqPos >= Tag.size())
			return false;

		size_t ValueBegin = EqPos;
		size_t ValueEnd = EqPos;
		if(Tag[EqPos] == '"' || Tag[EqPos] == '\'')
		{
			const char Quote = Tag[EqPos];
			ValueBegin = EqPos + 1;
			ValueEnd = Tag.find(Quote, ValueBegin);
			if(ValueEnd == std::string::npos)
				return false;
		}
		else
		{
			while(ValueEnd < Tag.size() && !std::isspace((unsigned char)Tag[ValueEnd]) && Tag[ValueEnd] != '>')
				ValueEnd++;
		}

		OutValue = DecodeHtmlUrl(Tag.substr(ValueBegin, ValueEnd - ValueBegin));
		TrimAsciiWhitespace(OutValue);
		return !OutValue.empty();
	}
	return false;
}

static bool ResolveRelativeUrl(const std::string &BaseUrl, const std::string &CandidateUrl, std::string &OutResolvedUrl)
{
	if(CandidateUrl.empty())
		return false;
	if(IsUrlStart(CandidateUrl.c_str()))
	{
		OutResolvedUrl = CandidateUrl;
		return true;
	}
	if(str_startswith(CandidateUrl.c_str(), "//"))
	{
		const size_t SchemePos = BaseUrl.find("://");
		if(SchemePos == std::string::npos)
			return false;
		OutResolvedUrl = BaseUrl.substr(0, SchemePos) + ":" + CandidateUrl;
		return true;
	}
	if(CandidateUrl[0] == '#')
		return false;

	const size_t SchemePos = BaseUrl.find("://");
	if(SchemePos == std::string::npos)
		return false;
	const size_t HostStart = SchemePos + 3;
	const size_t PathStart = BaseUrl.find('/', HostStart);
	const std::string Origin = PathStart == std::string::npos ? BaseUrl : BaseUrl.substr(0, PathStart);

	if(CandidateUrl[0] == '/')
	{
		OutResolvedUrl = Origin + CandidateUrl;
		return true;
	}

	std::string BasePath = PathStart == std::string::npos ? "/" : BaseUrl.substr(PathStart);
	const size_t QueryPos = BasePath.find_first_of("?#");
	if(QueryPos != std::string::npos)
		BasePath.resize(QueryPos);
	const size_t LastSlash = BasePath.find_last_of('/');
	if(LastSlash == std::string::npos)
		BasePath = "/";
	else
		BasePath.resize(LastSlash + 1);

	OutResolvedUrl = Origin + BasePath + CandidateUrl;
	return true;
}

static bool ResolveAndFilterCandidateUrl(const char *pBaseUrl, const std::string &RawCandidate, std::string &OutResolvedUrl, bool AllowUnknownExtensions)
{
	std::string Candidate = DecodeHtmlUrl(RawCandidate);
	TrimAsciiWhitespace(Candidate);
	if(Candidate.empty())
		return false;

	const std::string CandidateLower = ToLowerAscii(Candidate);
	if(str_startswith(CandidateLower.c_str(), "data:") || str_startswith(CandidateLower.c_str(), "blob:") ||
		str_startswith(CandidateLower.c_str(), "javascript:") || str_startswith(CandidateLower.c_str(), "mailto:") ||
		str_startswith(CandidateLower.c_str(), "about:"))
	{
		return false;
	}

	std::string Resolved;
	if(IsUrlStart(Candidate.c_str()))
		Resolved = Candidate;
	else if(!pBaseUrl || !IsUrlStart(pBaseUrl) || !ResolveRelativeUrl(pBaseUrl, Candidate, Resolved))
		return false;

	if(!IsUrlStart(Resolved.c_str()))
		return false;
	if((int)Resolved.size() > CHAT_MEDIA_MAX_URL_LENGTH)
		return false;
	for(char c : Resolved)
	{
		if((unsigned char)c < 32 || c == ' ' || c == '\t' || c == '\n' || c == '\r')
			return false;
	}

	const std::string Ext = ExtractUrlExtensionLower(Resolved);
	if(!Ext.empty() && IsBlockedMediaExtension(Ext))
		return false;
	if(!AllowUnknownExtensions && !Ext.empty() && !IsLikelyMediaExtension(Ext))
		return false;

	OutResolvedUrl = Resolved;
	return true;
}

static bool IsLikelyHtmlDocument(const unsigned char *pData, size_t DataSize)
{
	if(!pData || DataSize == 0)
		return false;

	const size_t ScanSize = minimum(DataSize, (size_t)8192);
	const std::string PrefixLower = ToLowerAscii(std::string((const char *)pData, ScanSize));
	return PrefixLower.find("<!doctype html") != std::string::npos ||
		PrefixLower.find("<html") != std::string::npos ||
		PrefixLower.find("<head") != std::string::npos ||
		PrefixLower.find("<meta") != std::string::npos;
}

static bool IsLikelyTextDocument(const unsigned char *pData, size_t DataSize)
{
	if(!pData || DataSize == 0 || IsImagePayloadSignature(pData, DataSize) || IsVideoPayloadSignature(pData, DataSize))
		return false;

	const size_t ScanSize = minimum(DataSize, (size_t)512);
	size_t Printable = 0;
	for(size_t i = 0; i < ScanSize; ++i)
	{
		const unsigned char c = pData[i];
		if(c == '\t' || c == '\n' || c == '\r' || (c >= 32 && c < 127))
			Printable++;
	}

	for(size_t i = 0; i < ScanSize; ++i)
	{
		const unsigned char c = pData[i];
		if(std::isspace(c))
			continue;
		if(c == '<' || c == '{' || c == '[')
			return true;
		break;
	}

	return ScanSize > 0 && Printable * 100 >= ScanSize * 90;
}

static void FindMetaContentsByKey(const std::string &Html, const std::string &HtmlLower, const char *pKey, std::vector<std::string> &vOutValues)
{
	const std::string KeyLower = ToLowerAscii(pKey);
	size_t Pos = 0;
	while((Pos = HtmlLower.find("<meta", Pos)) != std::string::npos)
	{
		const size_t EndPos = HtmlLower.find('>', Pos);
		if(EndPos == std::string::npos)
			break;
		if(EndPos - Pos > 3072)
		{
			Pos = EndPos + 1;
			continue;
		}

		const std::string Tag = Html.substr(Pos, EndPos - Pos + 1);
		const std::string TagLower = HtmlLower.substr(Pos, EndPos - Pos + 1);
		std::string NameOrProperty;
		const bool MatchesProperty = ExtractHtmlAttribute(Tag, TagLower, "property", NameOrProperty) && ToLowerAscii(NameOrProperty) == KeyLower;
		const bool MatchesName = ExtractHtmlAttribute(Tag, TagLower, "name", NameOrProperty) && ToLowerAscii(NameOrProperty) == KeyLower;
		const bool MatchesItemprop = ExtractHtmlAttribute(Tag, TagLower, "itemprop", NameOrProperty) && ToLowerAscii(NameOrProperty) == KeyLower;
		if(MatchesProperty || MatchesName || MatchesItemprop)
		{
			std::string Value;
			if(ExtractHtmlAttribute(Tag, TagLower, "content", Value) ||
				ExtractHtmlAttribute(Tag, TagLower, "src", Value) ||
				ExtractHtmlAttribute(Tag, TagLower, "href", Value))
			{
				vOutValues.push_back(Value);
			}
		}
		Pos = EndPos + 1;
	}
}

static void CollectTagAttributeSources(const std::string &Html, const std::string &HtmlLower, const char *pTagName, const char *const *ppAttrs, int NumAttrs, std::vector<std::string> &vOutValues)
{
	std::string TagNeedle = std::string("<") + pTagName;
	size_t Pos = 0;
	while((Pos = HtmlLower.find(TagNeedle, Pos)) != std::string::npos)
	{
		const size_t EndPos = HtmlLower.find('>', Pos);
		if(EndPos == std::string::npos)
			break;
		if(EndPos - Pos > 4096)
		{
			Pos = EndPos + 1;
			continue;
		}

		const std::string Tag = Html.substr(Pos, EndPos - Pos + 1);
		const std::string TagLower = HtmlLower.substr(Pos, EndPos - Pos + 1);
		for(int i = 0; i < NumAttrs; ++i)
		{
			std::string Value;
			if(ExtractHtmlAttribute(Tag, TagLower, ppAttrs[i], Value))
			{
				vOutValues.push_back(Value);
				break;
			}
		}
		Pos = EndPos + 1;
	}
}

static void CollectLinkMediaHrefs(const std::string &Html, const std::string &HtmlLower, std::vector<std::string> &vOutValues)
{
	size_t Pos = 0;
	while((Pos = HtmlLower.find("<link", Pos)) != std::string::npos)
	{
		const size_t EndPos = HtmlLower.find('>', Pos);
		if(EndPos == std::string::npos)
			break;

		const std::string Tag = Html.substr(Pos, EndPos - Pos + 1);
		const std::string TagLower = HtmlLower.substr(Pos, EndPos - Pos + 1);
		std::string Rel;
		if(ExtractHtmlAttribute(Tag, TagLower, "rel", Rel))
		{
			const std::string RelLower = ToLowerAscii(Rel);
			if(RelLower.find("image_src") != std::string::npos || RelLower.find("thumbnail") != std::string::npos ||
				RelLower.find("image") != std::string::npos || RelLower.find("video") != std::string::npos)
			{
				std::string Value;
				if(ExtractHtmlAttribute(Tag, TagLower, "href", Value))
					vOutValues.push_back(Value);
			}
		}

		Pos = EndPos + 1;
	}
}

static int JsonHexValue(char c)
{
	if(c >= '0' && c <= '9')
		return c - '0';
	if(c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if(c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool TryParseJsonQuotedValue(const std::string &Json, size_t QuotePos, std::string &OutValue, size_t &OutEndPos)
{
	if(QuotePos >= Json.size() || (Json[QuotePos] != '"' && Json[QuotePos] != '\''))
		return false;

	const char Quote = Json[QuotePos];
	std::string Value;
	size_t Pos = QuotePos + 1;
	while(Pos < Json.size())
	{
		const char c = Json[Pos++];
		if(c == '\\')
		{
			if(Pos >= Json.size())
				break;

			const char Escape = Json[Pos++];
			if(Escape == 'u' && Pos + 4 <= Json.size())
			{
				int Codepoint = 0;
				bool Valid = true;
				for(int i = 0; i < 4; ++i)
				{
					const int Hex = JsonHexValue(Json[Pos + i]);
					if(Hex < 0)
					{
						Valid = false;
						break;
					}
					Codepoint = Codepoint * 16 + Hex;
				}
				if(Valid)
				{
					if(Codepoint > 0 && Codepoint < 128)
						Value.push_back((char)Codepoint);
					Pos += 4;
					continue;
				}
			}

			switch(Escape)
			{
			case 'n':
			case 'r':
			case 't':
				Value.push_back(' ');
				break;
			case 'b':
			case 'f':
				break;
			default:
				Value.push_back(Escape);
				break;
			}
			continue;
		}
		if(c == Quote)
		{
			OutValue = DecodeHtmlUrl(std::move(Value));
			OutEndPos = Pos;
			return true;
		}
		Value.push_back(c);
	}
	return false;
}

static void FindJsonValuesByKey(const std::string &Json, const std::string &JsonLower, const char *pKey, std::vector<std::string> &vOutValues)
{
	const std::string KeyPattern = "\"" + ToLowerAscii(pKey) + "\"";
	size_t Pos = 0;
	while((Pos = JsonLower.find(KeyPattern, Pos)) != std::string::npos)
	{
		const size_t ColonPos = JsonLower.find(':', Pos + KeyPattern.size());
		if(ColonPos == std::string::npos)
			break;

		size_t ValuePos = ColonPos + 1;
		while(ValuePos < Json.size() && std::isspace((unsigned char)Json[ValuePos]))
			ValuePos++;
		if(ValuePos >= Json.size())
			break;

		if(Json[ValuePos] == '"' || Json[ValuePos] == '\'')
		{
			std::string Value;
			size_t EndPos = ValuePos;
			if(TryParseJsonQuotedValue(Json, ValuePos, Value, EndPos))
			{
				vOutValues.push_back(Value);
				Pos = EndPos;
				continue;
			}
		}
		else
		{
			size_t EndPos = ValuePos;
			while(EndPos < Json.size() && Json[EndPos] != ',' && Json[EndPos] != '}' && Json[EndPos] != ']' && !std::isspace((unsigned char)Json[EndPos]))
				EndPos++;
			if(EndPos > ValuePos)
			{
				vOutValues.emplace_back(DecodeHtmlUrl(Json.substr(ValuePos, EndPos - ValuePos)));
				Pos = EndPos;
				continue;
			}
		}

		Pos += KeyPattern.size();
	}
}

static void CollectJsonLdMediaCandidates(const std::string &Html, const std::string &HtmlLower, std::vector<std::string> &vOutValues)
{
	size_t Pos = 0;
	while((Pos = HtmlLower.find("<script", Pos)) != std::string::npos)
	{
		const size_t TagEnd = HtmlLower.find('>', Pos);
		if(TagEnd == std::string::npos)
			break;
		const size_t ClosePos = HtmlLower.find("</script>", TagEnd + 1);
		if(ClosePos == std::string::npos)
			break;

		const std::string Tag = Html.substr(Pos, TagEnd - Pos + 1);
		const std::string TagLower = HtmlLower.substr(Pos, TagEnd - Pos + 1);
		std::string TypeValue;
		if(!ExtractHtmlAttribute(Tag, TagLower, "type", TypeValue) || ToLowerAscii(TypeValue).find("ld+json") == std::string::npos)
		{
			Pos = ClosePos + 9;
			continue;
		}

		const std::string ScriptBody = Html.substr(TagEnd + 1, ClosePos - (TagEnd + 1));
		const std::string ScriptBodyLower = ToLowerAscii(ScriptBody);
		const char *apJsonKeys[] = {"contentUrl", "thumbnailUrl", "video", "embedUrl", "url", "mp4", "srcUrl"};
		for(const char *pKey : apJsonKeys)
			FindJsonValuesByKey(ScriptBody, ScriptBodyLower, pKey, vOutValues);

		Pos = ClosePos + 9;
	}
}

static void CollectJsonMediaCandidates(const std::string &Text, const std::string &TextLower, std::vector<std::string> &vOutValues)
{
	const char *apJsonKeys[] = {
		"url",
		"contentUrl",
		"content_url",
		"mediaUrl",
		"media_url",
		"thumbnailUrl",
		"thumbnail_url",
		"gif",
		"mediumgif",
		"tinygif",
		"nanogif",
		"mp4",
		"loopedmp4",
		"tinymp4",
		"nanomp4",
		"webm",
		"webp",
		"src",
		"srcUrl",
		"embedUrl"};
	for(const char *pKey : apJsonKeys)
		FindJsonValuesByKey(Text, TextLower, pKey, vOutValues);
}

static void CollectTenorMediaFormatCandidates(const std::string &Text, const std::string &TextLower, std::vector<std::string> &vOutValues)
{
	const auto AddFirstUrlFromFormatObject = [&](const char *pFormat) {
		const std::string KeyPattern = "\"" + ToLowerAscii(pFormat) + "\"";
		size_t Pos = 0;
		while((Pos = TextLower.find(KeyPattern, Pos)) != std::string::npos)
		{
			const size_t ColonPos = TextLower.find(':', Pos + KeyPattern.size());
			if(ColonPos == std::string::npos)
				break;

			size_t ObjectStart = ColonPos + 1;
			while(ObjectStart < Text.size() && std::isspace((unsigned char)Text[ObjectStart]))
				ObjectStart++;
			if(ObjectStart >= Text.size() || Text[ObjectStart] != '{')
			{
				Pos = ColonPos + 1;
				continue;
			}

			int Depth = 0;
			size_t ObjectEnd = ObjectStart;
			bool InString = false;
			char Quote = '\0';
			while(ObjectEnd < Text.size() && ObjectEnd - ObjectStart < 8192)
			{
				const char c = Text[ObjectEnd++];
				if(InString)
				{
					if(c == '\\' && ObjectEnd < Text.size())
					{
						ObjectEnd++;
						continue;
					}
					if(c == Quote)
					{
						InString = false;
						Quote = '\0';
					}
					continue;
				}
				if(c == '"' || c == '\'')
				{
					InString = true;
					Quote = c;
					continue;
				}
				if(c == '{')
					Depth++;
				else if(c == '}')
				{
					Depth--;
					if(Depth <= 0)
						break;
				}
			}

			if(ObjectEnd <= ObjectStart || ObjectEnd > Text.size())
			{
				Pos = ColonPos + 1;
				continue;
			}

			const std::string Object = Text.substr(ObjectStart, ObjectEnd - ObjectStart);
			const std::string ObjectLower = ToLowerAscii(Object);
			std::vector<std::string> vUrls;
			FindJsonValuesByKey(Object, ObjectLower, "url", vUrls);
			for(const std::string &Url : vUrls)
			{
				const std::string UrlHostLower = ExtractUrlHostLower(Url);
				if(HostIsOrEndsWith(UrlHostLower, "tenor.com"))
				{
					vOutValues.push_back(Url);
					return;
				}
			}

			Pos = ObjectEnd;
		}
	};

	const char *apPreferredFormats[] = {"tinygif", "gif", "mp4", "tinymp4", "mediumgif", "webp", "tinywebp"};
	for(const char *pFormat : apPreferredFormats)
		AddFirstUrlFromFormatObject(pFormat);
}

static void CollectScriptMediaCandidates(const std::string &Html, const std::string &HtmlLower, std::vector<std::string> &vOutValues)
{
	size_t Pos = 0;
	while((Pos = HtmlLower.find("<script", Pos)) != std::string::npos)
	{
		const size_t TagEnd = HtmlLower.find('>', Pos);
		if(TagEnd == std::string::npos)
			break;
		const size_t ClosePos = HtmlLower.find("</script>", TagEnd + 1);
		if(ClosePos == std::string::npos)
			break;

		const std::string ScriptBody = DecodeHtmlUrl(Html.substr(TagEnd + 1, ClosePos - (TagEnd + 1)));
		const std::string ScriptBodyLower = ToLowerAscii(ScriptBody);
		if(ScriptBodyLower.find("media.tenor.com") == std::string::npos &&
			ScriptBodyLower.find("media1.tenor.com") == std::string::npos &&
			ScriptBodyLower.find("c.tenor.com") == std::string::npos &&
			ScriptBodyLower.find("media.giphy.com") == std::string::npos &&
			ScriptBodyLower.find("giphy.mp4") == std::string::npos &&
			ScriptBodyLower.find(".gif") == std::string::npos &&
			ScriptBodyLower.find(".mp4") == std::string::npos &&
			ScriptBodyLower.find(".webp") == std::string::npos)
		{
			Pos = ClosePos + 9;
			continue;
		}

		CollectJsonMediaCandidates(ScriptBody, ScriptBodyLower, vOutValues);

		Pos = ClosePos + 9;
	}
}

static void CollectInlineMediaUrls(const std::string &Html, std::vector<std::string> &vOutValues)
{
	const std::string DecodedHtml = DecodeHtmlUrl(Html);
	const std::string DecodedHtmlLower = ToLowerAscii(DecodedHtml);
	const auto AddCandidateAt = [&](size_t Pos) {
		size_t EndPos = Pos;
		while(EndPos < DecodedHtml.size())
		{
			const char c = DecodedHtml[EndPos];
			if(std::isspace((unsigned char)c) || c == '"' || c == '\'' || c == '<' || c == '>' || c == '\\' || c == ')' || c == ']' || c == '}')
				break;
			EndPos++;
		}

		std::string Candidate = DecodedHtml.substr(Pos, EndPos - Pos);
		while(!Candidate.empty() && IsTrimmedUrlChar(Candidate.back()))
			Candidate.pop_back();
		const std::string Ext = ExtractUrlExtensionLower(Candidate);
		if(!Candidate.empty() && IsLikelyMediaExtension(Ext))
		{
			vOutValues.push_back(std::move(Candidate));
			return true;
		}
		return false;
	};

	size_t SearchPos = 0;
	while(SearchPos < DecodedHtmlLower.size())
	{
		const size_t HttpPos = DecodedHtmlLower.find("http://", SearchPos);
		const size_t HttpsPos = DecodedHtmlLower.find("https://", SearchPos);
		size_t Pos = std::string::npos;
		if(HttpPos == std::string::npos)
			Pos = HttpsPos;
		else if(HttpsPos == std::string::npos)
			Pos = HttpPos;
		else
			Pos = minimum(HttpPos, HttpsPos);
		if(Pos == std::string::npos)
			break;

		AddCandidateAt(Pos);
		if((int)vOutValues.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
			return;

		SearchPos = Pos + 1;
	}

	const char *apProtocolRelativeHosts[] = {
		"//media.tenor.com",
		"//media1.tenor.com",
		"//c.tenor.com",
		"//media.giphy.com",
		"//media1.giphy.com",
		"//i.imgur.com",
		"//cdn.discordapp.com",
		"//media.discordapp.net"};
	for(const char *pNeedle : apProtocolRelativeHosts)
	{
		SearchPos = 0;
		while((SearchPos = DecodedHtmlLower.find(pNeedle, SearchPos)) != std::string::npos)
		{
			AddCandidateAt(SearchPos);
			if((int)vOutValues.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
				return;
			SearchPos++;
		}
	}
}

static bool LooksLikeDecorativeMediaUrl(const std::string &UrlLower)
{
	const char *apNeedles[] = {
		"/favicon",
		"favicon.",
		"apple-touch-icon",
		"/apple-touch",
		"/logo",
		"logo.",
		"-logo",
		"_logo",
		"/icon/",
		"/icons/",
		"/sprite",
		"sprite.",
		"/brand/",
		"/branding/",
		"/avatar",
		"placeholder"};
	for(const char *pNeedle : apNeedles)
	{
		if(UrlLower.find(pNeedle) != std::string::npos)
			return true;
	}
	return false;
}

static int ScoreHtmlMediaCandidate(int Priority, const std::string &ResolvedUrl)
{
	const std::string UrlLower = ToLowerAscii(ResolvedUrl);
	const std::string HostLower = ExtractUrlHostLower(ResolvedUrl);
	const std::string Ext = ExtractUrlExtensionLower(UrlLower);

	int Score = Priority * 10;
	if(IsLikelyAnimatedImageExtension(Ext))
		Score -= 70;
	else if(IsLikelyVideoExtension(Ext))
		Score -= 55;
	else if(IsLikelyImageExtension(Ext))
		Score += 10;
	else if(Ext.empty())
		Score += 15;

	if(HostLower == "media.tenor.com" || HostLower == "media1.tenor.com" || HostLower == "c.tenor.com" ||
		HostLower == "media.giphy.com" || HostLower == "media1.giphy.com" ||
		HostLower == "i.imgur.com" || HostLower == "media.discordapp.net" ||
		HostLower == "cdn.discordapp.com")
	{
		Score -= 25;
	}

	if(LooksLikeDecorativeMediaUrl(UrlLower))
		Score += 1000;

	return Score;
}

static bool IsAnimatedProviderUrl(const char *pUrl)
{
	if(pUrl == nullptr || pUrl[0] == '\0')
		return false;
	const std::string HostLower = ExtractUrlHostLower(pUrl);
	return HostIsOrEndsWith(HostLower, "tenor.com") || HostIsOrEndsWith(HostLower, "giphy.com");
}

static bool IsAnimatedOrVideoMediaUrl(const std::string &Url)
{
	const std::string Ext = ExtractUrlExtensionLower(Url);
	return IsLikelyVideoExtension(Ext) || IsLikelyAnimatedImageExtension(Ext);
}

static bool IsStaticImageMediaUrl(const std::string &Url)
{
	const std::string Ext = ExtractUrlExtensionLower(Url);
	return IsLikelyImageExtension(Ext) && !IsLikelyAnimatedImageExtension(Ext);
}

static void ExtractMediaUrlsFromHtmlDocument(const unsigned char *pData, size_t DataSize, const char *pBaseUrl, std::vector<std::string> &vOutUrls)
{
	vOutUrls.clear();
	if(!pData || DataSize == 0 || !pBaseUrl || !IsLikelyTextDocument(pData, DataSize))
		return;

	const size_t HtmlSize = minimum(DataSize, (size_t)(256 * 1024));
	const std::string Html = DecodeHtmlUrl(std::string((const char *)pData, HtmlSize));
	const std::string HtmlLower = ToLowerAscii(Html);
	const bool LikelyHtml = IsLikelyHtmlDocument(pData, DataSize);
	const std::string BaseHostLower = ExtractUrlHostLower(pBaseUrl);
	const bool TenorDocument = HostIsOrEndsWith(BaseHostLower, "tenor.com") || BaseHostLower == "tenor.googleapis.com";

	struct SPrioritizedCandidate
	{
		int m_Priority = 0;
		std::string m_Value;
	};

	std::vector<SPrioritizedCandidate> vRawCandidates;
	const auto AddCandidates = [&](int Priority, const std::vector<std::string> &vValues) {
		for(const std::string &Value : vValues)
		{
			vRawCandidates.push_back({Priority, Value});
			if((int)vRawCandidates.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES * 6)
				return;
		}
	};

	if(TenorDocument)
	{
		std::vector<std::string> vValues;
		CollectTenorMediaFormatCandidates(Html, HtmlLower, vValues);
		AddCandidates(-5, vValues);
	}

	{
		std::vector<std::string> vValues;
		CollectJsonMediaCandidates(Html, HtmlLower, vValues);
		AddCandidates(0, vValues);
	}

	if(!LikelyHtml)
	{
		std::vector<std::string> vValues;
		CollectInlineMediaUrls(Html, vValues);
		AddCandidates(1, vValues);
	}

	const char *apMetaVideoKeys[] = {"og:video", "og:video:url", "og:video:secure_url", "twitter:video", "twitter:video:src", "twitter:player:stream", "contenturl", "contentUrl", "mediaurl", "mediaUrl", "embedurl", "embedUrl", "video"};
	for(const char *pKey : apMetaVideoKeys)
	{
		std::vector<std::string> vValues;
		FindMetaContentsByKey(Html, HtmlLower, pKey, vValues);
		AddCandidates(0, vValues);
	}

	const char *apMetaImageKeys[] = {"og:image", "og:image:url", "og:image:secure_url", "twitter:image", "twitter:image:src"};
	for(const char *pKey : apMetaImageKeys)
	{
		std::vector<std::string> vValues;
		FindMetaContentsByKey(Html, HtmlLower, pKey, vValues);
		AddCandidates(1, vValues);
	}

	{
		std::vector<std::string> vValues;
		const char *apAttrs[] = {"src", "poster", "data-src"};
		CollectTagAttributeSources(Html, HtmlLower, "video", apAttrs, sizeof(apAttrs) / sizeof(apAttrs[0]), vValues);
		AddCandidates(1, vValues);
	}
	{
		std::vector<std::string> vValues;
		const char *apAttrs[] = {"src"};
		CollectTagAttributeSources(Html, HtmlLower, "source", apAttrs, sizeof(apAttrs) / sizeof(apAttrs[0]), vValues);
		AddCandidates(1, vValues);
	}
	{
		std::vector<std::string> vValues;
		CollectLinkMediaHrefs(Html, HtmlLower, vValues);
		AddCandidates(2, vValues);
	}
	{
		std::vector<std::string> vValues;
		CollectJsonLdMediaCandidates(Html, HtmlLower, vValues);
		AddCandidates(2, vValues);
	}
	{
		std::vector<std::string> vValues;
		CollectScriptMediaCandidates(Html, HtmlLower, vValues);
		AddCandidates(2, vValues);
	}
	{
		std::vector<std::string> vValues;
		CollectInlineMediaUrls(Html, vValues);
		AddCandidates(2, vValues);
	}
	{
		std::vector<std::string> vValues;
		const char *apAttrs[] = {"src", "data-src", "data-original"};
		CollectTagAttributeSources(Html, HtmlLower, "img", apAttrs, sizeof(apAttrs) / sizeof(apAttrs[0]), vValues);
		AddCandidates(3, vValues);
	}

	std::vector<std::pair<int, std::string>> vResolvedCandidates;
	for(const auto &Candidate : vRawCandidates)
	{
		if((int)vResolvedCandidates.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES * 4)
			break;
		std::string Resolved;
		if(!ResolveAndFilterCandidateUrl(pBaseUrl, Candidate.m_Value, Resolved, true))
			continue;
		if(str_comp(Resolved.c_str(), pBaseUrl) == 0)
			continue;
		const std::string ResolvedLower = ToLowerAscii(Resolved);
		if(LooksLikeDecorativeMediaUrl(ResolvedLower))
			continue;

		bool Exists = false;
		for(const auto &Entry : vResolvedCandidates)
		{
			if(str_comp(Entry.second.c_str(), Resolved.c_str()) == 0)
			{
				Exists = true;
				break;
			}
		}
		if(!Exists)
			vResolvedCandidates.emplace_back(ScoreHtmlMediaCandidate(Candidate.m_Priority, Resolved), std::move(Resolved));
	}

	std::stable_sort(vResolvedCandidates.begin(), vResolvedCandidates.end(), [](const auto &A, const auto &B) {
		return A.first < B.first;
	});

	bool HasAnimatedOrVideoCandidate = false;
	for(const auto &Entry : vResolvedCandidates)
	{
		if(IsAnimatedOrVideoMediaUrl(Entry.second))
		{
			HasAnimatedOrVideoCandidate = true;
			break;
		}
	}

	for(const auto &Entry : vResolvedCandidates)
	{
		if((int)vOutUrls.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
			break;
		if(HasAnimatedOrVideoCandidate && IsStaticImageMediaUrl(Entry.second))
			continue;
		vOutUrls.push_back(Entry.second);
	}
}
} // namespace

CChat::CChat()
{
	m_Mode = MODE_NONE;

	m_Input.SetClipboardLineCallback([this](const char *pStr) { SendChatQueued(pStr); });
	m_Input.SetCalculateOffsetCallback([this]() { return m_IsInputCensored; });
	m_Input.SetDisplayTextCallback([this](char *pStr, size_t NumChars) {
		m_IsInputCensored = false;
		if(
			g_Config.m_ClStreamerMode &&
			(str_startswith(pStr, "/login ") ||
				str_startswith(pStr, "/register ") ||
				str_startswith(pStr, "/code ") ||
				str_startswith(pStr, "/timeout ") ||
				str_startswith(pStr, "/save ") ||
				str_startswith(pStr, "/load ")))
		{
			bool Censor = false;
			const size_t NumLetters = minimum(NumChars, sizeof(ms_aDisplayText) - 1);
			for(size_t i = 0; i < NumLetters; ++i)
			{
				if(Censor)
					ms_aDisplayText[i] = '*';
				else
					ms_aDisplayText[i] = pStr[i];
				if(pStr[i] == ' ')
				{
					Censor = true;
					m_IsInputCensored = true;
				}
			}
			ms_aDisplayText[NumLetters] = '\0';
			return ms_aDisplayText;
		}
		return pStr;
	});
}

void CChat::RegisterCommand(const char *pName, const char *pParams, const char *pHelpText)
{
	// Don't allow duplicate commands.
	for(const auto &Command : m_vServerCommands)
		if(str_comp(Command.m_aName, pName) == 0)
			return;

	m_vServerCommands.emplace_back(pName, pParams, pHelpText);
	m_ServerCommandsNeedSorting = true;
}

void CChat::UnregisterCommand(const char *pName)
{
	m_vServerCommands.erase(std::remove_if(m_vServerCommands.begin(), m_vServerCommands.end(), [pName](const CCommand &Command) { return str_comp(Command.m_aName, pName) == 0; }), m_vServerCommands.end());
}

void CChat::RebuildChat()
{
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized)
			continue;
		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);
		// recalculate sizes
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
	}
}

void CChat::ClearLines()
{
	for(auto &Line : m_aLines)
		Line.Reset(*this);
	m_PrevScoreBoardShowed = false;
	m_PrevShowChat = false;
}

void CChat::OnWindowResize()
{
	RebuildChat();
}

void CChat::Reset()
{
	ClearLines();

	m_Show = false;
	m_CompletionUsed = false;
	m_CompletionChosen = -1;
	m_aCompletionBuffer[0] = 0;
	m_PlaceholderOffset = 0;
	m_PlaceholderLength = 0;
	m_pHistoryEntry = nullptr;
	m_vPendingChatQueue.clear();
	m_LastChatSend = 0;
	m_CurrentLine = 0;
	m_IsInputCensored = false;
	m_EditingNewLine = true;
	m_ServerSupportsCommandInfo = false;
	m_ServerCommandsNeedSorting = false;
	m_aCurrentInputText[0] = '\0';
	DisableMode();
	m_vServerCommands.clear();
	m_HistoryMode = false;
	m_HistoryScrollOffset = 0;
	m_HistoryCurrent = 0;
	m_HistoryCount = 0;
	m_SearchActive = false;
	m_aSearchText[0] = '\0';
	m_vSearchMatches.clear();
	m_CurrentSearchMatch = 0;
	m_TranslateButtonPressed = false;
	m_TranslateButtonRectValid = false;

	for(int64_t &LastSoundPlayed : m_aLastSoundPlayed)
		LastSoundPlayed = 0;
}

void CChat::OnRelease()
{
	m_Show = false;
}

void CChat::OnStateChange(int NewState, int OldState)
{
	if(OldState <= IClient::STATE_CONNECTING)
		Reset();
}

void CChat::ConSay(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(0, pResult->GetString(0));
}

void CChat::ConSayTeam(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->SendChat(1, pResult->GetString(0));
}

void CChat::ConChat(IConsole::IResult *pResult, void *pUserData)
{
	const char *pMode = pResult->GetString(0);
	if(str_comp(pMode, "all") == 0)
		((CChat *)pUserData)->EnableMode(0);
	else if(str_comp(pMode, "team") == 0)
		((CChat *)pUserData)->EnableMode(1);
	else
		((CChat *)pUserData)->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "console", "expected all or team as mode");

	if(pResult->GetString(1)[0] || g_Config.m_ClChatReset)
		((CChat *)pUserData)->m_Input.Set(pResult->GetString(1));
}

void CChat::ConShowChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->m_Show = pResult->GetInteger(0) != 0;
}

void CChat::ConEcho(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->Echo(pResult->GetString(0));
}

void CChat::ConClearChat(IConsole::IResult *pResult, void *pUserData)
{
	((CChat *)pUserData)->ClearLines();
}

void CChat::ConShowChatHistory(IConsole::IResult *pResult, void *pUserData)
{
	CChat *pChat = (CChat *)pUserData;
	if(!g_Config.m_TcChatHistory)
		return;
	pChat->m_HistoryMode = !pChat->m_HistoryMode;
	if(pChat->m_HistoryMode)
	{
		pChat->m_HistoryScrollOffset = 0;
		pChat->m_SearchActive = false;
		pChat->m_aSearchText[0] = '\0';
		pChat->m_vSearchMatches.clear();
		pChat->m_CurrentSearchMatch = 0;
	}
}

void CChat::ConchainChatOld(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	((CChat *)pUserData)->RebuildChat();
}

void CChat::ConchainChatFontSize(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentWidth();
	pChat->RebuildChat();
}

void CChat::ConchainChatWidth(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData)
{
	pfnCallback(pResult, pCallbackUserData);
	CChat *pChat = (CChat *)pUserData;
	pChat->EnsureCoherentFontSize();
	pChat->RebuildChat();
}

void CChat::Echo(const char *pString)
{
	AddLine(CLIENT_MSG, 0, pString);
}

void CChat::ExtractMediaUrlsFromText(const char *pText, std::vector<std::string> &vOutUrls)
{
	vOutUrls.clear();
	if(!pText)
		return;

	const char *pCur = pText;
	while(*pCur)
	{
		if(!IsUrlStart(pCur))
		{
			++pCur;
			continue;
		}

		const char *pEnd = pCur;
		while(!IsTokenEnd(*pEnd))
			++pEnd;

		std::string Url(pCur, pEnd - pCur);
		while(!Url.empty() && IsTrimmedUrlChar(Url.back()))
			Url.pop_back();

		if(IsYouTubeUrl(Url))
		{
			pCur = pEnd;
			continue;
		}

		std::vector<std::string> vExpandedUrls;
		AddDirectTenorCandidates(Url, vExpandedUrls);
		AddDirectGiphyCandidates(Url, vExpandedUrls);
		AddDirectImgurCandidates(Url, vExpandedUrls);
		vExpandedUrls.push_back(Url);

		for(const std::string &ExpandedUrl : vExpandedUrls)
		{
			if(!IsUrlStart(ExpandedUrl.c_str()) || (int)ExpandedUrl.size() > CHAT_MEDIA_MAX_URL_LENGTH)
				continue;

			bool Exists = false;
			for(const auto &ExistingUrl : vOutUrls)
			{
				if(str_comp(ExistingUrl.c_str(), ExpandedUrl.c_str()) == 0)
				{
					Exists = true;
					break;
				}
			}
			if(!Exists)
			{
				vOutUrls.push_back(ExpandedUrl);
				if((int)vOutUrls.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
					return;
			}
		}

		pCur = pEnd;
	}
}

CChat::EMediaKind CChat::MediaKindFromUrl(const char *pUrl)
{
	if(!pUrl)
		return EMediaKind::UNKNOWN;

	const std::string Ext = ExtractUrlExtensionLower(pUrl);
	if(IsLikelyVideoExtension(Ext))
		return EMediaKind::VIDEO;
	if(IsLikelyAnimatedImageExtension(Ext))
		return EMediaKind::ANIMATED;
	if(IsLikelyImageExtension(Ext))
		return EMediaKind::PHOTO;
	return EMediaKind::UNKNOWN;
}

void CChat::ResetLineMedia(CLine &Line)
{
	if(Line.m_pMediaRequest)
	{
		Line.m_pMediaRequest->Abort();
		Line.m_pMediaRequest = nullptr;
	}
	if(Line.m_pMediaDecodeJob)
	{
		Line.m_pMediaDecodeJob->Abort();
		Line.m_pMediaDecodeJob = nullptr;
	}

	MediaDecoder::UnloadFrames(Graphics(), Line.m_vMediaFrames);
	Line.m_OptMediaDecodedFrames.reset();
	Line.m_MediaState = EMediaState::NONE;
	Line.m_MediaKind = EMediaKind::UNKNOWN;
	Line.m_aMediaUrl[0] = '\0';
	Line.m_aMediaStatus[0] = '\0';
	Line.m_vMediaCandidates.clear();
	Line.m_MediaCandidateIndex = -1;
	Line.m_MediaResolveDepth = 0;
	Line.m_MediaUploadIndex = 0;
	Line.m_vMediaFrameEndMs.clear();
	Line.m_MediaTotalDurationMs = 0;
	Line.m_MediaAnimated = false;
	Line.m_MediaWidth = 0;
	Line.m_MediaHeight = 0;
	Line.m_MediaAnimationStart = 0;
	Line.m_aTextHeight[0] = 0.0f;
	Line.m_aTextHeight[1] = 0.0f;
	Line.m_aMediaPreviewWidth[0] = 0.0f;
	Line.m_aMediaPreviewWidth[1] = 0.0f;
	Line.m_aMediaPreviewHeight[0] = 0.0f;
	Line.m_aMediaPreviewHeight[1] = 0.0f;
}

void CChat::SetMediaCandidates(CLine &Line, const std::vector<std::string> &vCandidates)
{
	Line.m_vMediaCandidates.clear();
	for(const std::string &Candidate : vCandidates)
	{
		if(!IsUrlStart(Candidate.c_str()) || (int)Candidate.size() > CHAT_MEDIA_MAX_URL_LENGTH)
			continue;

		bool Exists = false;
		for(const std::string &Existing : Line.m_vMediaCandidates)
		{
			if(str_comp(Existing.c_str(), Candidate.c_str()) == 0)
			{
				Exists = true;
				break;
			}
		}
		if(!Exists)
		{
			Line.m_vMediaCandidates.push_back(Candidate);
			if((int)Line.m_vMediaCandidates.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
				break;
		}
	}

	Line.m_MediaCandidateIndex = -1;
	Line.m_MediaKind = EMediaKind::UNKNOWN;
	Line.m_aMediaUrl[0] = '\0';
	Line.m_aMediaStatus[0] = '\0';
	if(!Line.m_vMediaCandidates.empty())
	{
		Line.m_MediaCandidateIndex = 0;
		str_copy(Line.m_aMediaUrl, Line.m_vMediaCandidates.front().c_str(), sizeof(Line.m_aMediaUrl));
		Line.m_MediaKind = MediaKindFromUrl(Line.m_aMediaUrl);
		str_copy(Line.m_aMediaStatus, "Queued media...", sizeof(Line.m_aMediaStatus));
	}
}

void CChat::InsertMediaCandidates(CLine &Line, const std::vector<std::string> &vCandidates, int InsertIndex)
{
	if(vCandidates.empty())
		return;

	int InsertPos = std::clamp(InsertIndex, 0, (int)Line.m_vMediaCandidates.size());
	for(const std::string &Candidate : vCandidates)
	{
		if(!IsUrlStart(Candidate.c_str()) || (int)Candidate.size() > CHAT_MEDIA_MAX_URL_LENGTH)
			continue;

		bool Exists = false;
		for(const std::string &Existing : Line.m_vMediaCandidates)
		{
			if(str_comp(Existing.c_str(), Candidate.c_str()) == 0)
			{
				Exists = true;
				break;
			}
		}
		if(Exists)
			continue;

		Line.m_vMediaCandidates.insert(Line.m_vMediaCandidates.begin() + InsertPos, Candidate);
		InsertPos++;
		if((int)Line.m_vMediaCandidates.size() >= CHAT_MEDIA_MAX_HTML_CANDIDATES)
			break;
	}
}

bool CChat::QueueNextMediaCandidate(CLine &Line, const char *pReason)
{
	MediaDecoder::UnloadFrames(Graphics(), Line.m_vMediaFrames);
	Line.m_OptMediaDecodedFrames.reset();
	Line.m_MediaUploadIndex = 0;
	Line.m_vMediaFrameEndMs.clear();
	Line.m_MediaTotalDurationMs = 0;
	Line.m_MediaAnimated = false;
	Line.m_MediaWidth = 0;
	Line.m_MediaHeight = 0;
	Line.m_MediaAnimationStart = 0;

	const int NextIndex = Line.m_MediaCandidateIndex + 1;
	for(int CandidateIndex = maximum(0, NextIndex); CandidateIndex < (int)Line.m_vMediaCandidates.size(); ++CandidateIndex)
	{
		if(!IsUrlStart(Line.m_vMediaCandidates[CandidateIndex].c_str()))
			continue;
		if(!IsMediaUrlAllowed(Line.m_vMediaCandidates[CandidateIndex].c_str()))
			continue;

		Line.m_MediaCandidateIndex = CandidateIndex;
		str_copy(Line.m_aMediaUrl, Line.m_vMediaCandidates[CandidateIndex].c_str(), sizeof(Line.m_aMediaUrl));
		Line.m_MediaState = EMediaState::QUEUED;
		Line.m_MediaKind = MediaKindFromUrl(Line.m_aMediaUrl);
		const std::string HostLower = ExtractUrlHostLower(Line.m_aMediaUrl);
		if(HostLower == "tenor.googleapis.com" || HostIsOrEndsWith(HostLower, "tenor.com"))
			str_copy(Line.m_aMediaStatus, "Resolving Tenor...", sizeof(Line.m_aMediaStatus));
		else
			str_copy(Line.m_aMediaStatus, "Queued media...", sizeof(Line.m_aMediaStatus));
		Line.m_pMediaRequest = nullptr;
		Line.m_pMediaDecodeJob = nullptr;
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
		if(g_Config.m_Debug)
			log_debug("chat/media", "Trying media candidate (%d/%d): %s (%s)", CandidateIndex + 1, (int)Line.m_vMediaCandidates.size(), Line.m_aMediaUrl, pReason ? pReason : "unknown");
		return true;
	}

	return false;
}

void CChat::QueueMediaDownload(CLine &Line)
{
	if(!g_Config.m_MaChatMediaPreview || !AnyMediaAllowed() || Line.m_vMediaCandidates.empty())
		return;
	if(Line.m_MediaCandidateIndex < 0 || Line.m_MediaCandidateIndex >= (int)Line.m_vMediaCandidates.size())
	{
		if(!QueueNextMediaCandidate(Line, "initial candidate"))
		{
			Line.m_MediaState = EMediaState::NONE;
			return;
		}
	}
	if(Line.m_aMediaUrl[0] == '\0')
		return;
	if(!IsMediaUrlAllowed(Line.m_aMediaUrl))
	{
		if(!QueueNextMediaCandidate(Line, "media disabled or blocked"))
			Line.m_MediaState = EMediaState::NONE;
		return;
	}
	Line.m_MediaState = EMediaState::QUEUED;
}

void CChat::StartMediaDownload(CLine &Line)
{
	if(Line.m_MediaState != EMediaState::QUEUED || Line.m_aMediaUrl[0] == '\0')
		return;
	if(!IsMediaUrlAllowed(Line.m_aMediaUrl))
	{
		if(!QueueNextMediaCandidate(Line, "media disabled or blocked"))
			Line.m_MediaState = EMediaState::NONE;
		return;
	}
	if((int)str_length(Line.m_aMediaUrl) > CHAT_MEDIA_MAX_URL_LENGTH)
	{
		if(!QueueNextMediaCandidate(Line, "overlong URL"))
			Line.m_MediaState = EMediaState::FAILED;
		return;
	}
	for(const char *p = Line.m_aMediaUrl; *p; ++p)
	{
		if((unsigned char)*p < 32 || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
		{
			if(!QueueNextMediaCandidate(Line, "invalid URL characters"))
				Line.m_MediaState = EMediaState::FAILED;
			return;
		}
	}

	std::shared_ptr<CHttpRequest> pGet = HttpGet(Line.m_aMediaUrl);
	pGet->Timeout(CTimeout{8000, 0, 4096, 8});
	pGet->MaxResponseSize(CHAT_MEDIA_MAX_RESPONSE_SIZE);
	pGet->FailOnErrorStatus(false);
	pGet->LogProgress(HTTPLOG::NONE);
	Line.m_pMediaRequest = pGet;
	Line.m_MediaState = EMediaState::LOADING;
	const std::string HostLower = ExtractUrlHostLower(Line.m_aMediaUrl);
	if(HostLower == "tenor.googleapis.com" || HostIsOrEndsWith(HostLower, "tenor.com"))
		str_copy(Line.m_aMediaStatus, "Resolving Tenor...", sizeof(Line.m_aMediaStatus));
	else
		str_copy(Line.m_aMediaStatus, "Downloading media...", sizeof(Line.m_aMediaStatus));
	Http()->Run(pGet);
}

bool CChat::StartMediaDecode(CLine &Line, EMediaKind MediaKind, const unsigned char *pData, size_t DataSize)
{
	if(!pData || DataSize == 0 || DataSize > (size_t)CHAT_MEDIA_MAX_RESPONSE_SIZE)
		return false;
	if(Line.m_pMediaDecodeJob)
	{
		Line.m_pMediaDecodeJob->Abort();
		Line.m_pMediaDecodeJob = nullptr;
	}

	MediaDecoder::UnloadFrames(Graphics(), Line.m_vMediaFrames);
	Line.m_OptMediaDecodedFrames.reset();
	Line.m_MediaUploadIndex = 0;
	Line.m_vMediaFrameEndMs.clear();
	Line.m_MediaTotalDurationMs = 0;
	Line.m_MediaAnimated = false;
	Line.m_MediaWidth = 0;
	Line.m_MediaHeight = 0;
	Line.m_MediaAnimationStart = 0;

	Line.m_pMediaDecodeJob = std::make_shared<CMediaDecodeJob>(Graphics(), MediaKind, pData, DataSize, Line.m_aMediaUrl);
	Engine()->AddJob(Line.m_pMediaDecodeJob);
	Line.m_MediaState = EMediaState::DECODING;
	str_copy(Line.m_aMediaStatus, "Decoding media...", sizeof(Line.m_aMediaStatus));
	return true;
}

bool CChat::AnyMediaAllowed() const
{
	return g_Config.m_MaChatMediaPhotos || g_Config.m_MaChatMediaGifs;
}

bool CChat::IsMediaKindAllowed(EMediaKind Kind) const
{
	switch(Kind)
	{
	case EMediaKind::PHOTO:
		return g_Config.m_MaChatMediaPhotos;
	case EMediaKind::ANIMATED:
	case EMediaKind::VIDEO:
		return g_Config.m_MaChatMediaGifs;
	case EMediaKind::UNKNOWN:
	default:
		return AnyMediaAllowed();
	}
}

bool CChat::IsMediaUrlAllowed(const char *pUrl) const
{
	return IsMediaKindAllowed(MediaKindFromUrl(pUrl)) && IsAllowedChatMediaUrl(pUrl);
}

bool CChat::HasAllowedMediaCandidates(const CLine &Line) const
{
	for(const std::string &Candidate : Line.m_vMediaCandidates)
	{
		if(IsMediaUrlAllowed(Candidate.c_str()))
			return true;
	}
	return Line.m_aMediaUrl[0] != '\0' && IsMediaUrlAllowed(Line.m_aMediaUrl);
}

bool CChat::ShouldDisplayMediaSlot(const CLine &Line) const
{
	if(!g_Config.m_MaChatMediaPreview || !AnyMediaAllowed())
		return false;
	if(Line.m_MediaState == EMediaState::FAILED)
		return Line.m_aMediaStatus[0] != '\0';
	if((Line.m_MediaState == EMediaState::READY || Line.m_MediaState == EMediaState::LOADING || Line.m_MediaState == EMediaState::DECODING || Line.m_MediaState == EMediaState::QUEUED) && Line.m_MediaKind != EMediaKind::UNKNOWN)
		return IsMediaKindAllowed(Line.m_MediaKind);
	return HasAllowedMediaCandidates(Line);
}

bool CChat::GetCurrentFrameTexture(CLine &Line, IGraphics::CTextureHandle &Texture) const
{
	if(Line.m_vMediaFrames.empty())
		return false;
	if(!Line.m_MediaAnimated || Line.m_vMediaFrames.size() == 1)
	{
		Texture = Line.m_vMediaFrames.front().m_Texture;
		return Texture.IsValid();
	}
	if(Line.m_MediaTotalDurationMs <= 0 || (int)Line.m_vMediaFrameEndMs.size() != (int)Line.m_vMediaFrames.size())
	{
		// Fallback for old/incomplete media state.
		return MediaDecoder::GetCurrentFrameTexture(Line.m_vMediaFrames, Line.m_MediaAnimated, Line.m_MediaAnimationStart, Texture);
	}

	const int64_t ElapsedMs = ((time_get() - Line.m_MediaAnimationStart) * 1000) / time_freq();
	const int Offset = (int)(ElapsedMs % (int64_t)Line.m_MediaTotalDurationMs);
	const auto It = std::upper_bound(Line.m_vMediaFrameEndMs.begin(), Line.m_vMediaFrameEndMs.end(), Offset);
	const int Index = It == Line.m_vMediaFrameEndMs.end() ? 0 : (int)(It - Line.m_vMediaFrameEndMs.begin());
	Texture = Line.m_vMediaFrames[Index].m_Texture;
	return Texture.IsValid();
}

void CChat::UpdateMediaDownloads()
{
	if(!g_Config.m_MaChatMediaPreview || !AnyMediaAllowed())
	{
		for(auto &Line : m_aLines)
		{
			if(Line.m_MediaState != EMediaState::NONE)
			{
				ResetLineMedia(Line);
				Line.m_aYOffset[0] = -1.0f;
				Line.m_aYOffset[1] = -1.0f;
			}
		}
		return;
	}

	int ActiveDownloads = 0;
	for(auto &Line : m_aLines)
	{
		if(Line.m_MediaState == EMediaState::LOADING && Line.m_pMediaRequest && !Line.m_pMediaRequest->Done())
			ActiveDownloads++;
	}

	const auto FailLine = [this](CLine &Line, bool SuppressedBySettings, const char *pReason) {
		Line.m_OptMediaDecodedFrames.reset();
		Line.m_MediaUploadIndex = 0;
		Line.m_vMediaFrameEndMs.clear();
		Line.m_MediaTotalDurationMs = 0;
		MediaDecoder::UnloadFrames(Graphics(), Line.m_vMediaFrames);
		Line.m_MediaState = SuppressedBySettings ? EMediaState::NONE : EMediaState::FAILED;
		Line.m_MediaAnimated = false;
		Line.m_MediaWidth = 0;
		Line.m_MediaHeight = 0;
		Line.m_MediaAnimationStart = 0;
		if(SuppressedBySettings)
			Line.m_aMediaStatus[0] = '\0';
		else
			str_format(Line.m_aMediaStatus, sizeof(Line.m_aMediaStatus), "Media failed: %s", pReason ? pReason : "unknown");
		Line.m_aYOffset[0] = -1.0f;
		Line.m_aYOffset[1] = -1.0f;
	};

	int CompletedRequestsThisFrame = 0;
	for(auto &Line : m_aLines)
	{
		if(CompletedRequestsThisFrame >= CHAT_MEDIA_MAX_COMPLETED_PER_FRAME)
			break;
		if(Line.m_MediaState != EMediaState::LOADING || !Line.m_pMediaRequest || !Line.m_pMediaRequest->Done())
			continue;

		bool StartedDecode = false;
		bool SuppressedBySettings = false;
		const char *pFailureReason = "download failed";
		const bool HttpDone = Line.m_pMediaRequest->State() == EHttpState::DONE;
		const int StatusCode = HttpDone ? Line.m_pMediaRequest->StatusCode() : -1;
		if(HttpDone && StatusCode >= 200 && StatusCode < 400)
		{
			unsigned char *pResult = nullptr;
			size_t ResultSize = 0;
			Line.m_pMediaRequest->Result(&pResult, &ResultSize);
			if(pResult && ResultSize > 0)
			{
				if(Line.m_MediaResolveDepth < CHAT_MEDIA_MAX_RESOLVE_DEPTH)
				{
					std::vector<std::string> vExtractedUrls;
					ExtractMediaUrlsFromHtmlDocument(pResult, ResultSize, Line.m_aMediaUrl, vExtractedUrls);
					const int CandidateCountBefore = (int)Line.m_vMediaCandidates.size();
					InsertMediaCandidates(Line, vExtractedUrls, Line.m_MediaCandidateIndex + 1);
					if((int)Line.m_vMediaCandidates.size() > CandidateCountBefore)
					{
						Line.m_MediaResolveDepth++;
						str_copy(Line.m_aMediaStatus, "Found media link...", sizeof(Line.m_aMediaStatus));
					}
				}

				if(IsLikelyTextDocument(pResult, ResultSize))
				{
					pFailureReason = "text response";
				}
				else
				{
					const std::string Ext = ExtractUrlExtensionLower(Line.m_aMediaUrl);
					const bool IsGif = IsGifSignature(pResult, ResultSize) || Ext == "gif";
					const bool IsVideoCandidate = IsLikelyVideoExtension(Ext) || IsVideoPayloadSignature(pResult, ResultSize);
					const bool IsImageCandidate = IsLikelyImageExtension(Ext) || IsImagePayloadSignature(pResult, ResultSize);
					const bool IsAnimatedImageCandidate = IsLikelyAnimatedImageExtension(Ext) && !IsVideoCandidate;
					EMediaKind MediaKind = EMediaKind::UNKNOWN;
					if(IsGif || IsAnimatedImageCandidate)
						MediaKind = EMediaKind::ANIMATED;
					else if(IsVideoCandidate || (!IsImageCandidate && Ext.empty()))
						MediaKind = EMediaKind::VIDEO;
					else if(IsImageCandidate)
						MediaKind = EMediaKind::PHOTO;
					if(MediaKind == EMediaKind::UNKNOWN)
						MediaKind = EMediaKind::VIDEO;
					Line.m_MediaKind = MediaKind;

					if(!Ext.empty() && IsBlockedMediaExtension(Ext))
					{
						pFailureReason = "blocked extension";
					}
					else if(ResultSize < 16)
					{
						pFailureReason = "payload too small";
					}
					else if(MediaKind == EMediaKind::PHOTO && LooksLikeDecorativeMediaUrl(ToLowerAscii(Line.m_aMediaUrl)))
					{
						pFailureReason = "decorative image";
					}
					else if(!IsMediaKindAllowed(MediaKind))
					{
						SuppressedBySettings = true;
						pFailureReason = "media type disabled";
					}
					else
					{
						StartedDecode = StartMediaDecode(Line, MediaKind, pResult, ResultSize);
						if(!StartedDecode)
							pFailureReason = "decode job failed";
					}
				}
			}
			else
			{
				pFailureReason = "empty response";
			}
		}
		else if(g_Config.m_Debug)
		{
			log_debug("chat/media", "HTTP request failed for media URL (state=%d, status=%d): %s", (int)Line.m_pMediaRequest->State(), StatusCode, Line.m_aMediaUrl);
		}

		Line.m_pMediaRequest = nullptr;
		ActiveDownloads = maximum(0, ActiveDownloads - 1);
		CompletedRequestsThisFrame++;

		if(StartedDecode)
			continue;
		if(QueueNextMediaCandidate(Line, pFailureReason))
			continue;
		FailLine(Line, SuppressedBySettings, pFailureReason);
	}

	int CompletedDecodesThisFrame = 0;
	for(auto &Line : m_aLines)
	{
		if(CompletedDecodesThisFrame >= CHAT_MEDIA_MAX_COMPLETED_PER_FRAME)
			break;
		if(Line.m_MediaState != EMediaState::DECODING || !Line.m_pMediaDecodeJob || !Line.m_pMediaDecodeJob->Done())
			continue;

		bool Success = false;
		const char *pFailureReason = "decode failed";
		if(Line.m_pMediaDecodeJob->State() == IJob::STATE_DONE && Line.m_pMediaDecodeJob->Success() && !Line.m_pMediaDecodeJob->DecodedFrames().Empty())
		{
			const int Width = Line.m_pMediaDecodeJob->DecodedFrames().m_Width;
			const int Height = Line.m_pMediaDecodeJob->DecodedFrames().m_Height;
			Line.m_OptMediaDecodedFrames.emplace(std::move(Line.m_pMediaDecodeJob->DecodedFrames()));
			Line.m_MediaUploadIndex = 0;
			Line.m_vMediaFrameEndMs.clear();
			Line.m_MediaTotalDurationMs = 0;
			Line.m_MediaWidth = Width;
			Line.m_MediaHeight = Height;
			Line.m_MediaAnimated = false;
			Line.m_MediaAnimationStart = 0;
			str_copy(Line.m_aMediaStatus, "Preparing media...", sizeof(Line.m_aMediaStatus));
			Success = true;
		}
		else if(g_Config.m_Debug)
		{
			log_debug("chat/media", "Media decode job failed: %s", Line.m_aMediaUrl);
		}

		Line.m_pMediaDecodeJob = nullptr;
		CompletedDecodesThisFrame++;

		if(Success)
			continue;
		Line.m_OptMediaDecodedFrames.reset();
		Line.m_MediaUploadIndex = 0;
		if(QueueNextMediaCandidate(Line, pFailureReason))
			continue;
		FailLine(Line, false, pFailureReason);
	}

	const auto ClampFrameDurationMs = [](int DurationMs) -> int {
		constexpr int MediaFpsCap = 120;
		constexpr int MediaMinFrameMs = (1000 + MediaFpsCap - 1) / MediaFpsCap;
		constexpr int MediaMaxFrameMs = 10000;
		return std::clamp(DurationMs, MediaMinFrameMs, MediaMaxFrameMs);
	};

	auto UploadDecodedFramesStep = [&](CLine &Line, int MaxFramesToUpload, int64_t TimeBudgetUs, int &UploadedFramesOut, bool &FinishedOut) -> bool {
		UploadedFramesOut = 0;
		FinishedOut = false;
		if(!Line.m_OptMediaDecodedFrames.has_value())
			return true;
		SMediaDecodedFrames &DecodedFrames = *Line.m_OptMediaDecodedFrames;
		if(DecodedFrames.m_vFrames.empty())
		{
			FinishedOut = true;
			return false;
		}

		const int64_t Start = time_get();
		while(Line.m_MediaUploadIndex < (int)DecodedFrames.m_vFrames.size())
		{
			if(UploadedFramesOut >= MaxFramesToUpload)
				break;
			if(TimeBudgetUs > 0)
			{
				const int64_t ElapsedUs = ((time_get() - Start) * 1000000) / time_freq();
				if(ElapsedUs >= TimeBudgetUs)
					break;
			}

			SMediaRawFrame &RawFrame = DecodedFrames.m_vFrames[Line.m_MediaUploadIndex];
			SMediaFrame Frame;
			Frame.m_DurationMs = RawFrame.m_DurationMs;
			Frame.m_Texture = Graphics()->LoadTextureRawMove(RawFrame.m_Image, 0, Line.m_aMediaUrl);
			if(!Frame.m_Texture.IsValid())
				return false;
			Line.m_vMediaFrames.push_back(Frame);
			Line.m_MediaUploadIndex++;
			UploadedFramesOut++;
		}

		FinishedOut = Line.m_MediaUploadIndex >= (int)DecodedFrames.m_vFrames.size();
		return true;
	};

	int UploadedTexturesThisFrame = 0;
	const int64_t UploadStart = time_get();
	for(auto &Line : m_aLines)
	{
		if(!Line.m_Initialized || !Line.m_OptMediaDecodedFrames.has_value())
			continue;
		if(UploadedTexturesThisFrame >= CHAT_MEDIA_MAX_TEXTURE_UPLOADS_PER_FRAME)
			break;

		const int64_t ElapsedUs = ((time_get() - UploadStart) * 1000000) / time_freq();
		const int64_t RemainingUs = CHAT_MEDIA_TEXTURE_UPLOAD_BUDGET_US - ElapsedUs;
		if(RemainingUs <= 0)
			break;

		const int FramesBudget = CHAT_MEDIA_MAX_TEXTURE_UPLOADS_PER_FRAME - UploadedTexturesThisFrame;
		int UploadedNow = 0;
		bool Finished = false;
		const bool Success = UploadDecodedFramesStep(Line, FramesBudget, RemainingUs, UploadedNow, Finished);
		UploadedTexturesThisFrame += UploadedNow;

		if(!Success)
		{
			Line.m_OptMediaDecodedFrames.reset();
			Line.m_MediaUploadIndex = 0;
			Line.m_vMediaFrameEndMs.clear();
			Line.m_MediaTotalDurationMs = 0;
			MediaDecoder::UnloadFrames(Graphics(), Line.m_vMediaFrames);
			if(QueueNextMediaCandidate(Line, "upload failed"))
				continue;
			FailLine(Line, false, "upload failed");
			continue;
		}

		if(!Line.m_vMediaFrames.empty() && Line.m_MediaState != EMediaState::READY)
		{
			Line.m_MediaState = EMediaState::READY;
			Line.m_aMediaStatus[0] = '\0';
			Line.m_aYOffset[0] = -1.0f;
			Line.m_aYOffset[1] = -1.0f;
		}

		if(Finished)
		{
			Line.m_OptMediaDecodedFrames.reset();
			Line.m_MediaUploadIndex = 0;
			Line.m_vMediaFrameEndMs.clear();
			Line.m_MediaTotalDurationMs = 0;
			if(Line.m_vMediaFrames.size() > 1)
			{
				Line.m_vMediaFrameEndMs.reserve(Line.m_vMediaFrames.size());
				int TotalDuration = 0;
				for(const auto &Frame : Line.m_vMediaFrames)
				{
					TotalDuration += ClampFrameDurationMs(Frame.m_DurationMs);
					Line.m_vMediaFrameEndMs.push_back(TotalDuration);
				}
				Line.m_MediaTotalDurationMs = TotalDuration;
				Line.m_MediaAnimated = TotalDuration > 0;
				if(Line.m_MediaAnimated)
					Line.m_MediaAnimationStart = time_get();
			}
			else
			{
				Line.m_MediaAnimated = false;
				Line.m_MediaAnimationStart = 0;
			}
		}
	}

	for(auto &Line : m_aLines)
	{
		if(ActiveDownloads >= CHAT_MEDIA_MAX_CONCURRENT_DOWNLOADS)
			break;
		if(Line.m_MediaState == EMediaState::QUEUED)
		{
			StartMediaDownload(Line);
			if(Line.m_MediaState == EMediaState::LOADING)
				ActiveDownloads++;
		}
	}
}

void CChat::OnConsoleInit()
{
	Console()->Register("say", "r[message]", CFGFLAG_CLIENT, ConSay, this, "Say in chat");
	Console()->Register("say_team", "r[message]", CFGFLAG_CLIENT, ConSayTeam, this, "Say in team chat");
	Console()->Register("chat", "s['team'|'all'] ?r[message]", CFGFLAG_CLIENT, ConChat, this, "Enable chat with all/team mode");
	Console()->Register("+show_chat", "", CFGFLAG_CLIENT, ConShowChat, this, "Show chat");
	Console()->Register("echo", "r[message]", CFGFLAG_CLIENT | CFGFLAG_STORE, ConEcho, this, "Echo the text in chat window");
	Console()->Register("clear_chat", "", CFGFLAG_CLIENT | CFGFLAG_STORE, ConClearChat, this, "Clear chat messages");
	Console()->Register("+show_chat_history", "", CFGFLAG_CLIENT, ConShowChatHistory, this, "Show chat history");
}

void CChat::OnInit()
{
	Reset();
	Console()->Chain("cl_chat_old", ConchainChatOld, this);
	Console()->Chain("cl_chat_size", ConchainChatFontSize, this);
	Console()->Chain("cl_chat_width", ConchainChatWidth, this);
}

namespace
{
struct STranslateLanguageOption
{
	const char *m_pCode;
	const char *m_pLabel;
};

constexpr STranslateLanguageOption gs_aTranslateSourceOptions[] = {
	{"auto", "Auto"},
	{"ru", "Ruso"},
	{"en", "Ingles"},
	{"de", "Aleman"},
	{"fr", "Frances"},
	{"es", "Espanol"},
	{"zh", "Chino"},
	{"pt", "Portugues"},
	{"tr", "Turco"},
};

constexpr STranslateLanguageOption gs_aTranslateTargetOptions[] = {
	{"ru", "Ruso"},
	{"en", "Ingles"},
	{"de", "Aleman"},
	{"fr", "Frances"},
	{"es", "Espanol"},
	{"zh", "Chino"},
	{"pt", "Portugues"},
	{"tr", "Turco"},
};

template<size_t N>
int TranslateLanguageIndex(const char *pCode, const STranslateLanguageOption (&aOptions)[N])
{
	for(size_t i = 0; i < N; ++i)
	{
		if(str_comp_nocase(pCode, aOptions[i].m_pCode) == 0)
			return (int)i;
	}
	return 0;
}

template<size_t N>
void ApplyTranslateLanguage(char *pConfig, size_t ConfigSize, int Index, const STranslateLanguageOption (&aOptions)[N])
{
	Index = std::clamp(Index, 0, (int)N - 1);
	str_copy(pConfig, aOptions[Index].m_pCode, ConfigSize);
}
}

vec2 CChat::ChatMousePos() const
{
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	const vec2 WindowSize(maximum(1.0f, (float)Graphics()->WindowWidth()), maximum(1.0f, (float)Graphics()->WindowHeight()));
	const CUIRect *pUiScreen = Ui()->Screen();
	const vec2 UiMousePos = Ui()->UpdatedMousePos() * vec2(pUiScreen->w, pUiScreen->h) / WindowSize;
	return UiMousePos * vec2(Width / pUiScreen->w, Height / pUiScreen->h);
}

void CChat::OpenTranslateSettingsPopup(const CUIRect &ButtonRect)
{
	Ui()->DoPopupMenu(&m_TranslateSettingsPopupId, ButtonRect.x, ButtonRect.y, 300.0f, 283.0f, this, PopupTranslateSettings);
}

CUi::EPopupMenuFunctionResult CChat::PopupTranslateSettings(void *pContext, CUIRect View, bool Active)
{
	CChat *pChat = static_cast<CChat *>(pContext);
	(void)Active;
	const float Spacing = 5.0f;
	const float RowHeight = 20.0f;
	const float FontSize = 11.0f;
	static CUi::SDropDownState s_IncomingSourceDropDown;
	static CUi::SDropDownState s_IncomingTargetDropDown;
	static CUi::SDropDownState s_OutgoingSourceDropDown;
	static CUi::SDropDownState s_OutgoingTargetDropDown;
	static CLineInput s_IncomingIgnoreLanguagesInput(g_Config.m_BcTranslateIncomingIgnoreLanguages, sizeof(g_Config.m_BcTranslateIncomingIgnoreLanguages));
	static CScrollRegion s_IncomingSourceScroll;
	static CScrollRegion s_IncomingTargetScroll;
	static CScrollRegion s_OutgoingSourceScroll;
	static CScrollRegion s_OutgoingTargetScroll;

	s_IncomingSourceDropDown.m_SelectionPopupContext.m_pScrollRegion = &s_IncomingSourceScroll;
	s_IncomingTargetDropDown.m_SelectionPopupContext.m_pScrollRegion = &s_IncomingTargetScroll;
	s_OutgoingSourceDropDown.m_SelectionPopupContext.m_pScrollRegion = &s_OutgoingSourceScroll;
	s_OutgoingTargetDropDown.m_SelectionPopupContext.m_pScrollRegion = &s_OutgoingTargetScroll;

	CUIRect Row;
	View.HSplitTop(14.0f, &Row, &View);
	pChat->Ui()->DoLabel(&Row, "Traductor de chat", 12.0f, TEXTALIGN_ML);

	View.HSplitTop(Spacing, nullptr, &View);
	View.HSplitTop(18.0f, &Row, &View);
	if(pChat->GameClient()->m_Menus.DoButton_CheckBox(&pChat->m_TranslateSettingsEnableButton, "Traducir mensajes de otros", g_Config.m_TcTranslateAutoIncoming, &Row))
		g_Config.m_TcTranslateAutoIncoming ^= 1;

	View.HSplitTop(Spacing, nullptr, &View);
	View.HSplitTop(18.0f, &Row, &View);
	if(pChat->GameClient()->m_Menus.DoButton_CheckBox(&pChat->m_TranslateSettingsEnableOutgoingButton, "Traducir mis mensajes", g_Config.m_TcTranslateAutoOutgoing, &Row))
		g_Config.m_TcTranslateAutoOutgoing ^= 1;

	const auto RenderLanguageField = [&](const char *pLabel, int CurrentIndex, const char **ppLabels, int LabelCount, CUi::SDropDownState &DropDownState) {
		View.HSplitTop(Spacing, nullptr, &View);
		View.HSplitTop(RowHeight, &Row, &View);
		CUIRect Label, DropDown;
		Row.VSplitLeft(145.0f, &Label, &DropDown);
		pChat->Ui()->DoLabel(&Label, pLabel, FontSize, TEXTALIGN_ML);
		return pChat->Ui()->DoDropDown(&DropDown, CurrentIndex, ppLabels, LabelCount, DropDownState);
	};

	static const char *s_apSourceLabels[] = {
		"Auto", "Ruso", "Ingles", "Aleman", "Frances", "Espanol", "Chino", "Portugues", "Turco"};
	static const char *s_apTargetLabels[] = {
		"Ruso", "Ingles", "Aleman", "Frances", "Espanol", "Chino", "Portugues", "Turco"};

	const int IncomingSourceIndex = TranslateLanguageIndex(g_Config.m_BcTranslateIncomingSource, gs_aTranslateSourceOptions);
	const int NewIncomingSourceIndex = RenderLanguageField("Recibidos desde", IncomingSourceIndex, s_apSourceLabels, std::size(s_apSourceLabels), s_IncomingSourceDropDown);
	if(NewIncomingSourceIndex != IncomingSourceIndex)
		ApplyTranslateLanguage(g_Config.m_BcTranslateIncomingSource, sizeof(g_Config.m_BcTranslateIncomingSource), NewIncomingSourceIndex, gs_aTranslateSourceOptions);

	const int IncomingTargetIndex = TranslateLanguageIndex(g_Config.m_TcTranslateTarget, gs_aTranslateTargetOptions);
	const int NewIncomingTargetIndex = RenderLanguageField("Recibidos a", IncomingTargetIndex, s_apTargetLabels, std::size(s_apTargetLabels), s_IncomingTargetDropDown);
	if(NewIncomingTargetIndex != IncomingTargetIndex)
		ApplyTranslateLanguage(g_Config.m_TcTranslateTarget, sizeof(g_Config.m_TcTranslateTarget), NewIncomingTargetIndex, gs_aTranslateTargetOptions);

	const int OutgoingSourceIndex = TranslateLanguageIndex(g_Config.m_BcTranslateOutgoingSource, gs_aTranslateSourceOptions);
	const int NewOutgoingSourceIndex = RenderLanguageField("Mis mensajes desde", OutgoingSourceIndex, s_apSourceLabels, std::size(s_apSourceLabels), s_OutgoingSourceDropDown);
	if(NewOutgoingSourceIndex != OutgoingSourceIndex)
		ApplyTranslateLanguage(g_Config.m_BcTranslateOutgoingSource, sizeof(g_Config.m_BcTranslateOutgoingSource), NewOutgoingSourceIndex, gs_aTranslateSourceOptions);

	const int OutgoingTargetIndex = TranslateLanguageIndex(g_Config.m_BcTranslateOutgoingTarget, gs_aTranslateTargetOptions);
	const int NewOutgoingTargetIndex = RenderLanguageField("Mis mensajes a", OutgoingTargetIndex, s_apTargetLabels, std::size(s_apTargetLabels), s_OutgoingTargetDropDown);
	if(NewOutgoingTargetIndex != OutgoingTargetIndex)
		ApplyTranslateLanguage(g_Config.m_BcTranslateOutgoingTarget, sizeof(g_Config.m_BcTranslateOutgoingTarget), NewOutgoingTargetIndex, gs_aTranslateTargetOptions);

	View.HSplitTop(Spacing, nullptr, &View);
	View.HSplitTop(RowHeight, &Row, &View);
	CUIRect IgnoreLabel, IgnoreEditBox;
	Row.VSplitLeft(145.0f, &IgnoreLabel, &IgnoreEditBox);
	pChat->Ui()->DoLabel(&IgnoreLabel, "No traducir desde", FontSize, TEXTALIGN_ML);
	s_IncomingIgnoreLanguagesInput.SetEmptyText("ru; en; zh");
	pChat->Ui()->DoClearableEditBox(&s_IncomingIgnoreLanguagesInput, &IgnoreEditBox, 14.0f);
	pChat->GameClient()->m_Tooltips.DoToolTip(&s_IncomingIgnoreLanguagesInput, &IgnoreEditBox, "Idiomas separados por punto y coma, por ejemplo: ru; en; zh");

	View.HSplitTop(Spacing, nullptr, &View);
	static CButtonContainer s_TranslateKeyReader;
	static CButtonContainer s_TranslateKeyClear;
	pChat->GameClient()->m_Menus.DoLine_KeyReader(View, s_TranslateKeyReader, s_TranslateKeyClear, "Activar traductor", "toggle_translate");

	return CUi::POPUP_KEEP_OPEN;
}

void CChat::RenderTranslateSettingsButton(const CUIRect &ButtonRect)
{
	m_TranslateButtonRect.m_X = ButtonRect.x;
	m_TranslateButtonRect.m_Y = ButtonRect.y;
	m_TranslateButtonRect.m_W = ButtonRect.w;
	m_TranslateButtonRect.m_H = ButtonRect.h;
	m_TranslateButtonRectValid = true;

	const vec2 MousePos = ChatMousePos();
	const bool Hovered = MousePos.x >= ButtonRect.x && MousePos.x <= ButtonRect.x + ButtonRect.w &&
		MousePos.y >= ButtonRect.y && MousePos.y <= ButtonRect.y + ButtonRect.h;
	const bool IsOpen = Ui()->IsPopupOpen(&m_TranslateSettingsPopupId);
	const bool IsTranslateActive = g_Config.m_TcTranslateAutoIncoming || g_Config.m_TcTranslateAutoOutgoing;
	const ColorRGBA ButtonColor = IsOpen ? ColorRGBA(0.09f, 0.55f, 0.23f, 0.98f) :
		(IsTranslateActive ? (Hovered ? ColorRGBA(0.10f, 0.72f, 0.30f, 0.98f) : ColorRGBA(0.07f, 0.55f, 0.23f, 0.96f)) :
		(Hovered ? ColorRGBA(0.11f, 0.62f, 0.27f, 0.98f) : ColorRGBA(0.06f, 0.44f, 0.19f, 0.94f)));
	const float ButtonRounding = maximum(3.0f, ButtonRect.h * 0.22f);

	ButtonRect.Draw(ButtonColor, IGraphics::CORNER_ALL, ButtonRounding);

	CUIRect IconRect;
	ButtonRect.Margin(maximum(0.5f, ButtonRect.h * 0.05f), &IconRect);
	CUIRect LeftIcon, RightIcon;
	IconRect.VSplitMid(&LeftIcon, &RightIcon, 0.5f);
	TextRender()->TextColor(0.92f, 1.0f, 0.94f, 1.0f);
	Ui()->DoLabel(&LeftIcon, "A", maximum(4.0f, LeftIcon.h * 0.56f), TEXTALIGN_MC);
	TextRender()->SetFontPreset(EFontPreset::ICON_FONT);
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_ONLY_ADVANCE_WIDTH | ETextRenderFlags::TEXT_RENDER_FLAG_NO_X_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_Y_BEARING | ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT | ETextRenderFlags::TEXT_RENDER_FLAG_NO_OVERSIZE);
	Ui()->DoLabel(&RightIcon, FontIcon::LANGUAGE, maximum(4.0f, RightIcon.h * 0.46f), TEXTALIGN_MC);
	TextRender()->SetRenderFlags(0);
	TextRender()->SetFontPreset(EFontPreset::DEFAULT_FONT);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	if(Hovered)
		Ui()->SetHotItem(&m_TranslateSettingsButton);
	GameClient()->m_Tooltips.DoToolTip(&m_TranslateSettingsButton, &ButtonRect, "Traductor de chat");
}

bool CChat::OnInput(const IInput::CEvent &Event)
{
	if(m_Mode == MODE_NONE && !m_HistoryMode)
		return false;

	if(m_HistoryMode)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
		{
			if(m_SearchActive)
			{
				m_SearchActive = false;
				m_aSearchText[0] = '\0';
				m_vSearchMatches.clear();
				m_CurrentSearchMatch = 0;
			}
			else
			{
				m_HistoryMode = false;
				m_HistoryScrollOffset = 0;
			}
			return true;
		}

		if(m_SearchActive)
		{
			if(Event.m_Flags & IInput::FLAG_PRESS)
			{
				if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
				{
					m_SearchActive = false;
					return true;
				}
				if(Event.m_Key == KEY_TAB)
				{
					if(!m_vSearchMatches.empty())
					{
						m_CurrentSearchMatch = (m_CurrentSearchMatch + 1) % (int)m_vSearchMatches.size();
						UpdateSearch();
					}
					return true;
				}
			}
			if(Event.m_Flags & IInput::FLAG_TEXT)
			{
				if(str_length(m_aSearchText) < (int)sizeof(m_aSearchText) - 1)
				{
					char aNew[64];
					str_copy(aNew, m_aSearchText);
					str_append(aNew, Event.m_aText);
					str_copy(m_aSearchText, aNew);
					UpdateSearch();
					return true;
				}
			}
			if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_BACKSPACE)
			{
				int Len = str_length(m_aSearchText);
				if(Len > 0)
					m_aSearchText[Len - 1] = '\0';
				UpdateSearch();
				return true;
			}
			return true;
		}

		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			if(Input()->ModifierIsPressed() && Event.m_Key == KEY_F)
			{
				m_SearchActive = true;
				m_aSearchText[0] = '\0';
				m_vSearchMatches.clear();
				m_CurrentSearchMatch = 0;
				UpdateSearch();
				return true;
			}
			if(Event.m_Key == KEY_PAGEUP)
			{
				m_HistoryScrollOffset += 10;
				return true;
			}
			if(Event.m_Key == KEY_PAGEDOWN)
			{
				m_HistoryScrollOffset = maximum(m_HistoryScrollOffset - 10, 0);
				return true;
			}
			if(Event.m_Key == KEY_HOME)
			{
				m_HistoryScrollOffset = m_HistoryCount - 1;
				return true;
			}
			if(Event.m_Key == KEY_END)
			{
				m_HistoryScrollOffset = 0;
				return true;
			}
		}
		if(Event.m_Flags == IInput::FLAG_RELEASE && Event.m_Key == KEY_MOUSE_WHEEL_UP)
		{
			m_HistoryScrollOffset = minimum(m_HistoryScrollOffset + 1, m_HistoryCount - 1);
			return true;
		}
		if(Event.m_Flags == IInput::FLAG_RELEASE && Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
		{
			m_HistoryScrollOffset = maximum(m_HistoryScrollOffset - 1, 0);
			return true;
		}
		return true;
	}

	if(m_Mode == MODE_NONE)
		return false;

	if((Event.m_Flags & IInput::FLAG_PRESS) && Event.m_Key == KEY_ESCAPE && Ui()->IsPopupOpen(&m_TranslateSettingsPopupId))
	{
		Ui()->ClosePopupMenu(&m_TranslateSettingsPopupId);
		return true;
	}

	if(Ui()->IsPopupOpen(&m_TranslateSettingsPopupId) && Ui()->OnInput(Event))
		return true;

	if(Event.m_Key == KEY_MOUSE_1 && m_TranslateButtonRectValid)
	{
		const vec2 MousePos = ChatMousePos();
		const bool InsideTranslateButton =
			MousePos.x >= m_TranslateButtonRect.m_X && MousePos.x <= m_TranslateButtonRect.m_X + m_TranslateButtonRect.m_W &&
			MousePos.y >= m_TranslateButtonRect.m_Y && MousePos.y <= m_TranslateButtonRect.m_Y + m_TranslateButtonRect.m_H;

		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			m_TranslateButtonPressed = InsideTranslateButton;
			if(InsideTranslateButton)
				return true;
		}
		else if(Event.m_Flags & IInput::FLAG_RELEASE)
		{
			const bool ActivateButton = m_TranslateButtonPressed && InsideTranslateButton;
			m_TranslateButtonPressed = false;
			if(ActivateButton)
			{
				CUIRect ButtonRect = {m_TranslateButtonRect.m_X, m_TranslateButtonRect.m_Y, m_TranslateButtonRect.m_W, m_TranslateButtonRect.m_H};
				if(Ui()->IsPopupOpen(&m_TranslateSettingsPopupId))
					Ui()->ClosePopupMenu(&m_TranslateSettingsPopupId);
				else
					OpenTranslateSettingsPopup(ButtonRect);
				return true;
			}
		}
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_ESCAPE)
	{
		DisableMode();
		GameClient()->OnRelease();
		if(g_Config.m_ClChatReset)
		{
			m_Input.Clear();
			m_pHistoryEntry = nullptr;
		}
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && (Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER))
	{
		if(m_ServerCommandsNeedSorting)
		{
			std::sort(m_vServerCommands.begin(), m_vServerCommands.end());
			m_ServerCommandsNeedSorting = false;
		}

		if(GameClient()->m_BindChat.ChatDoBinds(m_Input.GetString()))
			; // Do nothing as bindchat was executed
		else if(GameClient()->m_TClient.ChatDoSpecId(m_Input.GetString()))
			; // Do nothing as specid was executed
		else
			SendChatQueued(m_Input.GetString());
		m_pHistoryEntry = nullptr;
		DisableMode();
		GameClient()->OnRelease();
		m_Input.Clear();
	}
	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_TAB)
	{
		const bool ShiftPressed = Input()->ShiftIsPressed();

		// fill the completion buffer
		if(!m_CompletionUsed)
		{
			const char *pCursor = m_Input.GetString() + m_Input.GetCursorOffset();
			for(size_t Count = 0; Count < m_Input.GetCursorOffset() && *(pCursor - 1) != ' '; --pCursor, ++Count)
				;
			m_PlaceholderOffset = pCursor - m_Input.GetString();

			for(m_PlaceholderLength = 0; *pCursor && *pCursor != ' '; ++pCursor)
				++m_PlaceholderLength;

			str_truncate(m_aCompletionBuffer, sizeof(m_aCompletionBuffer), m_Input.GetString() + m_PlaceholderOffset, m_PlaceholderLength);
		}

		if(!m_CompletionUsed && m_aCompletionBuffer[0] != '/')
		{
			// Create the completion list of player names through which the player can iterate
			const char *PlayerName, *FoundInput;
			m_PlayerCompletionListLength = 0;
			for(auto &PlayerInfo : GameClient()->m_Snap.m_apInfoByName)
			{
				if(PlayerInfo)
				{
					PlayerName = GameClient()->m_aClients[PlayerInfo->m_ClientId].m_aName;
					FoundInput = str_utf8_find_nocase(PlayerName, m_aCompletionBuffer);
					if(FoundInput != nullptr)
					{
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_ClientId = PlayerInfo->m_ClientId;
						// The score for suggesting a player name is determined by the distance of the search input to the beginning of the player name
						m_aPlayerCompletionList[m_PlayerCompletionListLength].m_Score = (int)(FoundInput - PlayerName);
						m_PlayerCompletionListLength++;
					}
				}
			}
			std::stable_sort(m_aPlayerCompletionList, m_aPlayerCompletionList + m_PlayerCompletionListLength,
				[](const CRateablePlayer &Player1, const CRateablePlayer &Player2) -> bool {
					return Player1.m_Score < Player2.m_Score;
				});
		}

		if(GameClient()->m_BindChat.ChatDoAutocomplete(ShiftPressed))
		{
		}
		else if(m_aCompletionBuffer[0] == '/' && !m_vServerCommands.empty())
		{
			CCommand *pCompletionCommand = nullptr;

			const size_t NumCommands = m_vServerCommands.size();

			if(ShiftPressed && m_CompletionUsed)
				m_CompletionChosen--;
			else if(!ShiftPressed)
				m_CompletionChosen++;
			m_CompletionChosen = (m_CompletionChosen + 2 * NumCommands) % (2 * NumCommands);

			m_CompletionUsed = true;

			const char *pCommandStart = m_aCompletionBuffer + 1;
			for(size_t i = 0; i < 2 * NumCommands; ++i)
			{
				int SearchType;
				int Index;

				if(ShiftPressed)
				{
					SearchType = ((m_CompletionChosen - i + 2 * NumCommands) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen - i + NumCommands) % NumCommands;
				}
				else
				{
					SearchType = ((m_CompletionChosen + i) % (2 * NumCommands)) / NumCommands;
					Index = (m_CompletionChosen + i) % NumCommands;
				}

				auto &Command = m_vServerCommands[Index];

				if(str_startswith_nocase(Command.m_aName, pCommandStart))
				{
					pCompletionCommand = &Command;
					m_CompletionChosen = Index + SearchType * NumCommands;
					break;
				}
			}

			// insert the command
			if(pCompletionCommand)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// add the command
				str_append(aBuf, "/");
				str_append(aBuf, pCompletionCommand->m_aName);

				// add separator
				const char *pSeparator = pCompletionCommand->m_aParams[0] == '\0' ? "" : " ";
				str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionCommand->m_aName) + 1;
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
		else
		{
			// find next possible name
			const char *pCompletionString = nullptr;
			if(m_PlayerCompletionListLength > 0)
			{
				// We do this in a loop, if a player left the game during the repeated pressing of Tab, they are skipped
				CGameClient::CClientData *pCompletionClientData;
				for(int i = 0; i < m_PlayerCompletionListLength; ++i)
				{
					if(ShiftPressed && m_CompletionUsed)
					{
						m_CompletionChosen--;
					}
					else if(!ShiftPressed)
					{
						m_CompletionChosen++;
					}
					if(m_CompletionChosen < 0)
					{
						m_CompletionChosen += m_PlayerCompletionListLength;
					}
					m_CompletionChosen %= m_PlayerCompletionListLength;
					m_CompletionUsed = true;

					pCompletionClientData = &GameClient()->m_aClients[m_aPlayerCompletionList[m_CompletionChosen].m_ClientId];
					if(!pCompletionClientData->m_Active)
					{
						continue;
					}

					pCompletionString = pCompletionClientData->m_aName;
					break;
				}
			}

			// insert the name
			if(pCompletionString)
			{
				char aBuf[MAX_LINE_LENGTH];
				// add part before the name
				str_truncate(aBuf, sizeof(aBuf), m_Input.GetString(), m_PlaceholderOffset);

				// quote the name
				char aQuoted[128];
				if((m_Input.GetString()[0] == '/' || GameClient()->m_BindChat.CheckBindChat(m_Input.GetString())) && (str_find(pCompletionString, " ") || str_find(pCompletionString, "\"")))
				{
					// escape the name
					str_copy(aQuoted, "\"");
					char *pDst = aQuoted + str_length(aQuoted);
					str_escape(&pDst, pCompletionString, aQuoted + sizeof(aQuoted));
					str_append(aQuoted, "\"");

					pCompletionString = aQuoted;
				}

				// add the name
				str_append(aBuf, pCompletionString);

				// add separator
				const char *pSeparator = "";
				if(*(m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength) != ' ')
					pSeparator = m_PlaceholderOffset == 0 ? ": " : " ";
				else if(m_PlaceholderOffset == 0)
					pSeparator = ":";
				if(*pSeparator)
					str_append(aBuf, pSeparator);

				// add part after the name
				str_append(aBuf, m_Input.GetString() + m_PlaceholderOffset + m_PlaceholderLength);

				m_PlaceholderLength = str_length(pSeparator) + str_length(pCompletionString);
				m_Input.Set(aBuf);
				m_Input.SetCursorOffset(m_PlaceholderOffset + m_PlaceholderLength);
			}
		}
	}
	else
	{
		// reset name completion process
		if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key != KEY_TAB && Event.m_Key != KEY_LSHIFT && Event.m_Key != KEY_RSHIFT)
		{
			m_CompletionChosen = -1;
			m_CompletionUsed = false;
		}

		m_Input.ProcessInput(Event);
	}

	if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_UP)
	{
		if(m_EditingNewLine)
		{
			str_copy(m_aCurrentInputText, m_Input.GetString());
			m_EditingNewLine = false;
		}

		if(m_pHistoryEntry)
		{
			CHistoryEntry *pTest = m_History.Prev(m_pHistoryEntry);

			if(pTest)
				m_pHistoryEntry = pTest;
		}
		else
			m_pHistoryEntry = m_History.Last();

		if(m_pHistoryEntry)
			m_Input.Set(m_pHistoryEntry->m_aText);
	}
	else if(Event.m_Flags & IInput::FLAG_PRESS && Event.m_Key == KEY_DOWN)
	{
		if(m_pHistoryEntry)
			m_pHistoryEntry = m_History.Next(m_pHistoryEntry);

		if(m_pHistoryEntry)
		{
			m_Input.Set(m_pHistoryEntry->m_aText);
		}
		else if(!m_EditingNewLine)
		{
			m_Input.Set(m_aCurrentInputText);
			m_EditingNewLine = true;
		}
	}

	return true;
}

bool CChat::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(m_Mode == MODE_NONE)
		return false;

	Ui()->ConvertMouseMove(&x, &y, CursorType);
	Ui()->OnCursorMove(x, y);
	return true;
}

void CChat::EnableMode(int Team)
{
	if(Client()->State() == IClient::STATE_DEMOPLAYBACK)
		return;

	m_HistoryMode = false;

	if(m_Mode == MODE_NONE)
	{
		if(Team)
			m_Mode = MODE_TEAM;
		else
			m_Mode = MODE_ALL;

		Input()->Clear();
		m_CompletionChosen = -1;
		m_CompletionUsed = false;
		m_Input.Activate(EInputPriority::CHAT);
	}
}

void CChat::DisableMode()
{
	if(m_Mode != MODE_NONE)
	{
		m_Mode = MODE_NONE;
		m_Input.Deactivate();
	}
}

void CChat::OnMessage(int MsgType, void *pRawMsg)
{
	if(GameClient()->m_SuppressEvents)
		return;

	if(MsgType == NETMSGTYPE_SV_CHAT)
	{
		CNetMsg_Sv_Chat *pMsg = (CNetMsg_Sv_Chat *)pRawMsg;

		auto &Re = GameClient()->m_TClient.m_RegexChatIgnore;
		if(Re.error().empty() && Re.test(pMsg->m_pMessage))
			return;

		/*
		if(g_Config.m_ClCensorChat)
		{
			char aMessage[MAX_LINE_LENGTH];
			str_copy(aMessage, pMsg->m_pMessage);
			GameClient()->m_Censor.CensorMessage(aMessage);
			AddLine(pMsg->m_ClientId, pMsg->m_Team, aMessage);
		}
		else
			AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);
		*/

		AddLine(pMsg->m_ClientId, pMsg->m_Team, pMsg->m_pMessage);

		if(Client()->State() != IClient::STATE_DEMOPLAYBACK &&
			pMsg->m_ClientId == SERVER_MSG)
		{
			StoreSave(pMsg->m_pMessage);
		}
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFO)
	{
		CNetMsg_Sv_CommandInfo *pMsg = (CNetMsg_Sv_CommandInfo *)pRawMsg;
		if(!m_ServerSupportsCommandInfo)
		{
			m_vServerCommands.clear();
			m_ServerSupportsCommandInfo = true;
		}
		RegisterCommand(pMsg->m_pName, pMsg->m_pArgsFormat, pMsg->m_pHelpText);
	}
	else if(MsgType == NETMSGTYPE_SV_COMMANDINFOREMOVE)
	{
		CNetMsg_Sv_CommandInfoRemove *pMsg = (CNetMsg_Sv_CommandInfoRemove *)pRawMsg;
		UnregisterCommand(pMsg->m_pName);
	}
}

bool CChat::LineShouldHighlight(const char *pLine, const char *pName)
{
	const char *pHit = str_utf8_find_nocase(pLine, pName);

	while(pHit)
	{
		int Length = str_length(pName);

		if(Length > 0 && (pLine == pHit || pHit[-1] == ' ') && (pHit[Length] == 0 || pHit[Length] == ' ' || pHit[Length] == '.' || pHit[Length] == '!' || pHit[Length] == ',' || pHit[Length] == '?' || pHit[Length] == ':'))
			return true;

		pHit = str_utf8_find_nocase(pHit + 1, pName);
	}

	return false;
}

static constexpr const char *SAVES_HEADER[] = {
	"Time",
	"Player",
	"Map",
	"Code",
};

// TODO: remove this in a few releases (in 2027 or later)
//       it got deprecated by CGameClient::StoreSave
void CChat::StoreSave(const char *pText)
{
	const char *pStart = str_find(pText, "Team successfully saved by ");
	const char *pMid = str_find(pText, ". Use '/load ");
	const char *pOn = str_find(pText, "' on ");
	const char *pEnd = str_find(pText, pOn ? " to continue" : "' to continue");

	if(!pStart || !pMid || !pEnd || pMid < pStart || pEnd < pMid || (pOn && (pOn < pMid || pEnd < pOn)))
		return;

	char aName[16];
	str_truncate(aName, sizeof(aName), pStart + 27, pMid - pStart - 27);

	char aSaveCode[64];

	str_truncate(aSaveCode, sizeof(aSaveCode), pMid + 13, (pOn ? pOn : pEnd) - pMid - 13);

	char aTimestamp[20];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::SPACE);

	const bool SavesFileExists = Storage()->FileExists(SAVES_FILE, IStorage::TYPE_SAVE);
	IOHANDLE File = Storage()->OpenFile(SAVES_FILE, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
		return;

	const char *apColumns[4] = {
		aTimestamp,
		aName,
		GameClient()->Map()->BaseName(),
		aSaveCode,
	};

	if(!SavesFileExists)
	{
		CsvWrite(File, 4, SAVES_HEADER);
	}
	CsvWrite(File, 4, apColumns);
	io_close(File);
}

void CChat::AddLine(int ClientId, int Team, const char *pLine)
{
	if(*pLine == 0 ||
		(ClientId == SERVER_MSG && !g_Config.m_ClShowChatSystem) ||
		(ClientId >= 0 && (GameClient()->m_aClients[ClientId].m_aName[0] == '\0' || // unknown client
					  GameClient()->m_aClients[ClientId].m_ChatIgnore ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatFriends && !GameClient()->m_aClients[ClientId].m_Friend) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && g_Config.m_ClShowChatTeamMembersOnly && GameClient()->IsOtherTeam(ClientId) && GameClient()->m_Teams.Team(GameClient()->m_Snap.m_LocalClientId) != TEAM_FLOCK) ||
					  (GameClient()->m_Snap.m_LocalClientId != ClientId && GameClient()->m_aClients[ClientId].m_Foe))))
		return;

	// TClient
	if(ClientId == CLIENT_MSG && !g_Config.m_TcShowChatClient)
		return;

	// trim right and set maximum length to 256 utf8-characters
	int Length = 0;
	const char *pStr = pLine;
	const char *pEnd = nullptr;
	while(*pStr)
	{
		const char *pStrOld = pStr;
		int Code = str_utf8_decode(&pStr);

		// check if unicode is not empty
		if(!str_utf8_isspace(Code))
		{
			pEnd = nullptr;
		}
		else if(pEnd == nullptr)
			pEnd = pStrOld;

		if(++Length >= MAX_LINE_LENGTH)
		{
			*(const_cast<char *>(pStr)) = '\0';
			break;
		}
	}
	if(pEnd != nullptr)
		*(const_cast<char *>(pEnd)) = '\0';

	if(*pLine == 0)
		return;

	bool Highlighted = false;

	auto &&FChatMsgCheckAndPrint = [this](const CLine &Line) {
		char aBuf[1024];
		str_format(aBuf, sizeof(aBuf), "%s%s%s", Line.m_aName, Line.m_ClientId >= 0 ? ": " : "", Line.m_aText);

		ColorRGBA ChatLogColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);
		if(Line.m_Highlighted)
		{
			ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		}
		else
		{
			if(Line.m_Friend && g_Config.m_ClMessageFriend)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor));
			else if(Line.m_Team)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
			else if(Line.m_ClientId == SERVER_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
			else if(Line.m_ClientId == CLIENT_MSG)
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
			else // regular message
				ChatLogColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		}

		const char *pFrom;
		if(Line.m_Whisper)
			pFrom = "chat/whisper";
		else if(Line.m_Team)
			pFrom = "chat/team";
		else if(Line.m_ClientId == SERVER_MSG)
			pFrom = "chat/server";
		else if(Line.m_ClientId == CLIENT_MSG)
			pFrom = "chat/client";
		else
			pFrom = "chat/all";

		Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, pFrom, aBuf, ChatLogColor);
	};

	// Custom color for new line
	std::optional<ColorRGBA> CustomColor = std::nullopt;
	if(ClientId == CLIENT_MSG)
		CustomColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));

	CLine &PreviousLine = m_aLines[m_CurrentLine];

	// Team Number:
	// 0 = global; 1 = team; 2 = sending whisper; 3 = receiving whisper

	// If it's a client message, m_aText will have ": " prepended so we have to work around it.
	if(PreviousLine.m_Initialized &&
		PreviousLine.m_TeamNumber == Team &&
		PreviousLine.m_ClientId == ClientId &&
		str_comp(PreviousLine.m_aText, pLine) == 0 &&
		PreviousLine.m_CustomColor == CustomColor)
	{
		PreviousLine.m_TimesRepeated++;
		TextRender()->DeleteTextContainer(PreviousLine.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(PreviousLine.m_QuadContainerIndex);
		PreviousLine.m_Time = time();
		PreviousLine.m_aYOffset[0] = -1.0f;
		PreviousLine.m_aYOffset[1] = -1.0f;

		FChatMsgCheckAndPrint(PreviousLine);
		return;
	}

	m_CurrentLine = (m_CurrentLine + 1) % MAX_LINES;

	CLine &CurrentLine = m_aLines[m_CurrentLine];
	CurrentLine.Reset(*this);
	CurrentLine.m_Initialized = true;
	CurrentLine.m_Time = time();
	CurrentLine.m_aYOffset[0] = -1.0f;
	CurrentLine.m_aYOffset[1] = -1.0f;
	CurrentLine.m_ClientId = ClientId;
	CurrentLine.m_TeamNumber = Team;
	CurrentLine.m_Team = Team == 1;
	CurrentLine.m_Whisper = Team >= 2;
	CurrentLine.m_NameColor = -2;
	CurrentLine.m_CustomColor = CustomColor;

	// check for highlighted name
	if(Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(ClientId >= 0 && ClientId != GameClient()->m_aLocalIds[0] && ClientId != GameClient()->m_aLocalIds[1])
		{
			for(int LocalId : GameClient()->m_aLocalIds)
			{
				Highlighted |= LocalId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[LocalId].m_aName);
			}
		}
	}
	else
	{
		// on demo playback use local id from snap directly,
		// since m_aLocalIds isn't valid there
		Highlighted |= GameClient()->m_Snap.m_LocalClientId >= 0 && LineShouldHighlight(pLine, GameClient()->m_aClients[GameClient()->m_Snap.m_LocalClientId].m_aName);
	}
	CurrentLine.m_Highlighted = Highlighted;

	str_copy(CurrentLine.m_aText, pLine);

	if(CurrentLine.m_ClientId == SERVER_MSG)
	{
		str_copy(CurrentLine.m_aName, "*** ");
	}
	else if(CurrentLine.m_ClientId == CLIENT_MSG)
	{
		str_copy(CurrentLine.m_aName, "— ");
	}
	else
	{
		const auto &LineAuthor = GameClient()->m_aClients[CurrentLine.m_ClientId];

		if(LineAuthor.m_Active)
		{
			if(LineAuthor.m_Team == TEAM_SPECTATORS)
				CurrentLine.m_NameColor = TEAM_SPECTATORS;

			if(GameClient()->IsTeamPlay())
			{
				if(LineAuthor.m_Team == TEAM_RED)
					CurrentLine.m_NameColor = TEAM_RED;
				else if(LineAuthor.m_Team == TEAM_BLUE)
					CurrentLine.m_NameColor = TEAM_BLUE;
			}
		}

		if(Team == TEAM_WHISPER_SEND)
		{
			str_copy(CurrentLine.m_aName, "→");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_BLUE;
			CurrentLine.m_Highlighted = false;
			Highlighted = false;
		}
		else if(Team == TEAM_WHISPER_RECV)
		{
			str_copy(CurrentLine.m_aName, "←");
			if(LineAuthor.m_Active)
			{
				str_append(CurrentLine.m_aName, " ");
				str_append(CurrentLine.m_aName, LineAuthor.m_aName);
			}
			CurrentLine.m_NameColor = TEAM_RED;
			CurrentLine.m_Highlighted = true;
			Highlighted = true;
		}
		else
		{
			str_copy(CurrentLine.m_aName, LineAuthor.m_aName);
		}

		if(LineAuthor.m_Active)
		{
			CurrentLine.m_Friend = LineAuthor.m_Friend;
			CurrentLine.m_pManagedTeeRenderInfo = GameClient()->CreateManagedTeeRenderInfo(LineAuthor);
		}
	}

	FChatMsgCheckAndPrint(CurrentLine);

	if(g_Config.m_MaChatMediaPreview && AnyMediaAllowed())
	{
		std::vector<std::string> vMediaUrls;
		ExtractMediaUrlsFromText(CurrentLine.m_aText, vMediaUrls);
		if(!vMediaUrls.empty())
		{
			SetMediaCandidates(CurrentLine, vMediaUrls);
			QueueMediaDownload(CurrentLine);
		}
	}

	// TClient: Store in chat history
	if(g_Config.m_TcChatHistory)
	{
		m_HistoryCurrent = (m_HistoryCurrent + 1) % MAX_HISTORY;
		CLine &HistLine = m_aHistoryLines[m_HistoryCurrent];
		HistLine.Reset(*this);
		HistLine.m_Time = CurrentLine.m_Time;
		HistLine.m_ClientId = CurrentLine.m_ClientId;
		HistLine.m_TeamNumber = CurrentLine.m_TeamNumber;
		HistLine.m_Team = CurrentLine.m_Team;
		HistLine.m_Whisper = CurrentLine.m_Whisper;
		HistLine.m_NameColor = CurrentLine.m_NameColor;
		HistLine.m_Highlighted = CurrentLine.m_Highlighted;
		str_copy(HistLine.m_aName, CurrentLine.m_aName);
		str_copy(HistLine.m_aText, CurrentLine.m_aText);
		if(m_HistoryCount < MAX_HISTORY)
			m_HistoryCount++;
	}

	// play sound
	int64_t Now = time();
	if(ClientId == SERVER_MSG)
	{
		if(Now - m_aLastSoundPlayed[CHAT_SERVER] >= time_freq() * 3 / 10)
		{
			if(g_Config.m_SndServerMessage)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_SERVER, 1.0f);
				m_aLastSoundPlayed[CHAT_SERVER] = Now;
			}
		}
	}
	else if(ClientId == CLIENT_MSG)
	{
		// No sound yet
	}
	else if(Highlighted && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		if(Now - m_aLastSoundPlayed[CHAT_HIGHLIGHT] >= time_freq() * 3 / 10)
		{
			char aBuf[1024];
			str_format(aBuf, sizeof(aBuf), "%s: %s", CurrentLine.m_aName, CurrentLine.m_aText);
			Client()->Notify("DDNet Chat", aBuf);
			if(g_Config.m_SndHighlight)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 1.0f);
				m_aLastSoundPlayed[CHAT_HIGHLIGHT] = Now;
			}

			if(g_Config.m_ClEditor)
			{
				GameClient()->Editor()->UpdateMentions();
			}
		}
	}
	else if(Team != TEAM_WHISPER_SEND)
	{
		if(Now - m_aLastSoundPlayed[CHAT_CLIENT] >= time_freq() * 3 / 10)
		{
			bool PlaySound = CurrentLine.m_Team ? g_Config.m_SndTeamChat : g_Config.m_SndChat;
#if defined(CONF_VIDEORECORDER)
			if(IVideo::Current())
			{
				PlaySound &= (bool)g_Config.m_ClVideoShowChat;
			}
#endif
			if(PlaySound)
			{
				GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_CLIENT, 1.0f);
				m_aLastSoundPlayed[CHAT_CLIENT] = Now;
			}
		}
	}

	// TClient
	GameClient()->m_Translate.AutoTranslate(CurrentLine);
}

void CChat::OnPrepareLines(float x, float y)
{
	float FontSize = this->FontSize();

	const bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;
	static float s_LastPrepareX = -100000.0f;
	static int s_LastMediaPreview = -1;
	static int s_LastMediaPhotos = -1;
	static int s_LastMediaGifs = -1;
	static int s_LastMediaMaxWidth = -1;
	const bool MediaConfigChanged =
		s_LastMediaPreview != g_Config.m_MaChatMediaPreview ||
		s_LastMediaPhotos != g_Config.m_MaChatMediaPhotos ||
		s_LastMediaGifs != g_Config.m_MaChatMediaGifs ||
		s_LastMediaMaxWidth != g_Config.m_MaChatMediaPreviewMaxWidth;
	bool ForceRecreate = IsScoreBoardOpen != m_PrevScoreBoardShowed || ShowLargeArea != m_PrevShowChat || std::fabs(s_LastPrepareX - x) > 0.01f || MediaConfigChanged;
	s_LastPrepareX = x;
	s_LastMediaPreview = g_Config.m_MaChatMediaPreview;
	s_LastMediaPhotos = g_Config.m_MaChatMediaPhotos;
	s_LastMediaGifs = g_Config.m_MaChatMediaGifs;
	s_LastMediaMaxWidth = g_Config.m_MaChatMediaPreviewMaxWidth;
	m_PrevScoreBoardShowed = IsScoreBoardOpen;
	m_PrevShowChat = ShowLargeArea;

	const int TeeSize = MessageTeeSize();
	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();
	float RealMsgPaddingTee = TeeSize + MESSAGE_TEE_PADDING_RIGHT;

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
		RealMsgPaddingTee = 0;
	}

	int64_t Now = time();
	float LineWidth = (IsScoreBoardOpen ? maximum(85.0f, (FontSize * 85.0f / 6.0f)) : g_Config.m_ClChatWidth) - (RealMsgPaddingX * 1.5f) - RealMsgPaddingTee;

	const auto ShouldExpandCompactAreaForMedia = [&]() {
		if(IsScoreBoardOpen || ShowLargeArea || !g_Config.m_MaChatMediaPreview || !AnyMediaAllowed())
			return false;
		for(int i = 0; i < 3; ++i)
		{
			const CLine &RecentLine = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
			if(!RecentLine.m_Initialized)
				break;
			if(ShouldDisplayMediaSlot(RecentLine))
				return true;
		}
		return false;
	};
	const bool ExpandCompactAreaForMedia = ShouldExpandCompactAreaForMedia();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (ShowLargeArea ? 50.0f : (ExpandCompactAreaForMedia ? y - CHAT_MEDIA_COMPACT_EXPANDED_HEIGHT : 200.0f));
	float Begin = x;
	float TextBegin = Begin + RealMsgPaddingX / 2.0f;
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		if(Line.m_TextContainerIndex.Valid() && Line.m_aYOffset[OffsetType] >= 0.0f && !ForceRecreate)
			continue;

		TextRender()->DeleteTextContainer(Line.m_TextContainerIndex);
		Graphics()->DeleteQuadContainer(Line.m_QuadContainerIndex);

		char aClientId[16] = "";
		if(g_Config.m_ClShowIds && Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			GameClient()->FormatClientId(Line.m_ClientId, aClientId, EClientIdFormat::INDENT_AUTO);
		}

		char aCount[12];
		if(Line.m_ClientId < 0)
			str_format(aCount, sizeof(aCount), "[%d] ", Line.m_TimesRepeated + 1);
		else
			str_format(aCount, sizeof(aCount), " [%d]", Line.m_TimesRepeated + 1);

		const char *pText = Line.m_aText;
		if(Config()->m_ClStreamerMode && Line.m_ClientId == SERVER_MSG)
		{
			if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load ") && str_endswith(Line.m_aText, "'"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***'";
			}
			else if(str_startswith(Line.m_aText, "Team save in progress. You'll be able to load with '/load") && str_endswith(Line.m_aText, "if it fails"))
			{
				pText = "Team save in progress. You'll be able to load with '/load *** *** ***' if save is successful or with '/load *** *** ***' if it fails";
			}
			else if(str_startswith(Line.m_aText, "Team successfully saved by ") && str_endswith(Line.m_aText, " to continue"))
			{
				pText = "Team successfully saved by ***. Use '/load *** *** ***' to continue";
			}
		}

		const CColoredParts ColoredParts(pText, Line.m_ClientId == CLIENT_MSG);
		if(!ColoredParts.Colors().empty() && ColoredParts.Colors()[0].m_Index == 0)
			Line.m_CustomColor = ColoredParts.Colors()[0].m_Color;
		pText = ColoredParts.Text();

		const char *pTranslatedError = nullptr;
		const char *pTranslatedText = nullptr;
		const char *pTranslatedLanguage = nullptr;
		if(Line.m_pTranslateResponse != nullptr && Line.m_pTranslateResponse->m_Text[0])
		{
			// If hidden and there is translated text
			if(pText != Line.m_aText)
			{
				pTranslatedError = TCLocalize("Translated text hidden due to streamer mode");
			}
			else if(Line.m_pTranslateResponse->m_Error)
			{
				pTranslatedError = Line.m_pTranslateResponse->m_Text;
			}
			else
			{
				pTranslatedText = Line.m_pTranslateResponse->m_Text;
				if(Line.m_pTranslateResponse->m_Language[0] != '\0')
					pTranslatedLanguage = Line.m_pTranslateResponse->m_Language;
			}
		}

		// get the y offset (calculate it if we haven't done that yet)
		if(Line.m_aYOffset[OffsetType] < 0.0f)
		{
			CTextCursor MeasureCursor;
			MeasureCursor.SetPosition(vec2(TextBegin, 0.0f));
			MeasureCursor.m_FontSize = FontSize;
			MeasureCursor.m_Flags = 0;
			MeasureCursor.m_LineWidth = LineWidth;

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				MeasureCursor.m_X += RealMsgPaddingTee;

				if(Line.m_Friend && g_Config.m_ClMessageFriend)
				{
					TextRender()->TextEx(&MeasureCursor, "♥ ");
				}
			}

			TextRender()->TextEx(&MeasureCursor, aClientId);
			TextRender()->TextEx(&MeasureCursor, Line.m_aName);
			if(Line.m_TimesRepeated > 0)
				TextRender()->TextEx(&MeasureCursor, aCount);

			if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			{
				TextRender()->TextEx(&MeasureCursor, ": ");
			}

			CTextCursor AppendCursor = MeasureCursor;
			AppendCursor.m_LongestLineWidth = 0.0f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				AppendCursor.m_StartX = MeasureCursor.m_X;
				AppendCursor.m_LineWidth -= MeasureCursor.m_LongestLineWidth;
			}

			if(pTranslatedText)
			{
				TextRender()->TextEx(&AppendCursor, pTranslatedText);
				if(pTranslatedLanguage)
				{
					TextRender()->TextEx(&AppendCursor, " [");
					TextRender()->TextEx(&AppendCursor, pTranslatedLanguage);
					TextRender()->TextEx(&AppendCursor, "]");
				}
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pText);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else if(pTranslatedError)
			{
				TextRender()->TextEx(&AppendCursor, pText);
				TextRender()->TextEx(&AppendCursor, "\n");
				AppendCursor.m_FontSize *= 0.8f;
				TextRender()->TextEx(&AppendCursor, pTranslatedError);
				AppendCursor.m_FontSize /= 0.8f;
			}
			else
			{
				TextRender()->TextEx(&AppendCursor, pText);
			}

			Line.m_aTextHeight[OffsetType] = AppendCursor.Height();
			Line.m_aMediaPreviewWidth[OffsetType] = 0.0f;
			Line.m_aMediaPreviewHeight[OffsetType] = 0.0f;

			float TotalHeight = Line.m_aTextHeight[OffsetType] + RealMsgPaddingY;
			if(ShouldDisplayMediaSlot(Line))
			{
				const float MaxPreviewWidth = minimum(LineWidth, (float)g_Config.m_MaChatMediaPreviewMaxWidth) * CHAT_MEDIA_PREVIEW_SIZE_SCALE;
				const float MaxPreviewHeight = (IsScoreBoardOpen ? CHAT_MEDIA_MAX_PREVIEW_HEIGHT_SCOREBOARD : CHAT_MEDIA_MAX_PREVIEW_HEIGHT) * CHAT_MEDIA_PREVIEW_SIZE_SCALE;
				if(Line.m_MediaState == EMediaState::READY && Line.m_MediaWidth > 0 && Line.m_MediaHeight > 0 && !Line.m_vMediaFrames.empty())
				{
					const float ScaleByWidth = MaxPreviewWidth / (float)Line.m_MediaWidth;
					const float ScaleByHeight = MaxPreviewHeight / (float)Line.m_MediaHeight;
					float Scale = minimum(1.0f, minimum(ScaleByWidth, ScaleByHeight));
					float PreviewW = maximum(1.0f, (float)Line.m_MediaWidth * Scale);
					float PreviewH = maximum(1.0f, (float)Line.m_MediaHeight * Scale);
					if(PreviewW < CHAT_MEDIA_MIN_PREVIEW_SIDE || PreviewH < CHAT_MEDIA_MIN_PREVIEW_SIDE)
					{
						const float Upscale = maximum(CHAT_MEDIA_MIN_PREVIEW_SIDE / PreviewW, CHAT_MEDIA_MIN_PREVIEW_SIDE / PreviewH);
						const float MaxUpscale = minimum(MaxPreviewWidth / PreviewW, MaxPreviewHeight / PreviewH);
						if(MaxUpscale > 1.0f)
						{
							const float UseUpscale = minimum(Upscale, MaxUpscale);
							PreviewW *= UseUpscale;
							PreviewH *= UseUpscale;
						}
					}
					Line.m_aMediaPreviewWidth[OffsetType] = PreviewW;
					Line.m_aMediaPreviewHeight[OffsetType] = PreviewH;
				}
				else if(Line.m_MediaState == EMediaState::QUEUED || Line.m_MediaState == EMediaState::LOADING || Line.m_MediaState == EMediaState::DECODING)
				{
					Line.m_aMediaPreviewWidth[OffsetType] = MaxPreviewWidth;
					Line.m_aMediaPreviewHeight[OffsetType] = maximum(FontSize * 1.5f, 16.0f) * CHAT_MEDIA_PREVIEW_SIZE_SCALE;
				}

				if(Line.m_aMediaPreviewWidth[OffsetType] > 0.0f && Line.m_aMediaPreviewHeight[OffsetType] > 0.0f)
					TotalHeight += FontSize * 0.45f + Line.m_aMediaPreviewHeight[OffsetType];
			}

			Line.m_aYOffset[OffsetType] = TotalHeight;
		}

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit && i != 0)
			break;

		// the position the text was created
		Line.m_TextYOffset = y + RealMsgPaddingY / 2.0f;

		int CurRenderFlags = TextRender()->GetRenderFlags();
		TextRender()->SetRenderFlags(CurRenderFlags | ETextRenderFlags::TEXT_RENDER_FLAG_NO_AUTOMATIC_QUAD_UPLOAD);

		// reset the cursor
		CTextCursor LineCursor;
		LineCursor.SetPosition(vec2(TextBegin, Line.m_TextYOffset));
		LineCursor.m_FontSize = FontSize;
		LineCursor.m_LineWidth = LineWidth;

		// Message is from valid player
		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			LineCursor.m_X += RealMsgPaddingTee;

			if(Line.m_Friend && g_Config.m_ClMessageFriend)
			{
				TextRender()->TextColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageFriendColor)).WithAlpha(1.0f));
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, "♥ ");
			}
		}

		// render name
		ColorRGBA NameColor;
		if(Line.m_CustomColor)
			NameColor = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			NameColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_ClientId >= 0 && g_Config.m_TcWarList && g_Config.m_TcWarListChat && GameClient()->m_WarList.GetAnyWar(Line.m_ClientId)) // TClient
			NameColor = GameClient()->m_WarList.GetPriorityColor(Line.m_ClientId);
		else if(Line.m_Team)
			NameColor = CalculateNameColor(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else if(Line.m_NameColor == TEAM_RED)
			NameColor = ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f);
		else if(Line.m_NameColor == TEAM_BLUE)
			NameColor = ColorRGBA(0.7f, 0.7f, 1.0f, 1.0f);
		else if(Line.m_NameColor == TEAM_SPECTATORS)
			NameColor = ColorRGBA(0.75f, 0.5f, 0.75f, 1.0f);
		else if(Line.m_ClientId >= 0 && g_Config.m_ClChatTeamColors && GameClient()->m_Teams.Team(Line.m_ClientId))
			NameColor = GameClient()->GetDDTeamColor(GameClient()->m_Teams.Team(Line.m_ClientId), 0.75f);
		else
			NameColor = ColorRGBA(0.8f, 0.8f, 0.8f, 1.0f);

		TextRender()->TextColor(NameColor);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aClientId);
		TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, Line.m_aName);

		if(Line.m_TimesRepeated > 0)
		{
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.3f);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, aCount);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
		{
			TextRender()->TextColor(NameColor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &LineCursor, ": ");
		}

		ColorRGBA Color;
		if(Line.m_CustomColor)
			Color = *Line.m_CustomColor;
		else if(Line.m_ClientId == SERVER_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageSystemColor));
		else if(Line.m_ClientId == CLIENT_MSG)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageClientColor));
		else if(Line.m_Highlighted)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageHighlightColor));
		else if(Line.m_Team)
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageTeamColor));
		else // regular message
			Color = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClMessageColor));
		TextRender()->TextColor(Color);

		CTextCursor AppendCursor = LineCursor;
		AppendCursor.m_LongestLineWidth = 0.0f;
		if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
		{
			AppendCursor.m_StartX = LineCursor.m_X;
			AppendCursor.m_LineWidth -= LineCursor.m_LongestLineWidth;
		}

		if(pTranslatedText)
		{
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedText);
			if(pTranslatedLanguage)
			{
				ColorRGBA ColorLang = Color;
				ColorLang.r *= 0.8f;
				ColorLang.g *= 0.8f;
				ColorLang.b *= 0.8f;
				TextRender()->TextColor(ColorLang);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, " [");
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedLanguage);
				TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "]");
			}
			ColorRGBA ColorSub = Color;
			ColorSub.r *= 0.7f;
			ColorSub.g *= 0.7f;
			ColorSub.b *= 0.7f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else if(pTranslatedError)
		{
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			ColorRGBA ColorSub = Color;
			ColorSub.r = 0.7f;
			ColorSub.g = 0.6f;
			ColorSub.b = 0.6f;
			TextRender()->TextColor(ColorSub);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, "\n");
			AppendCursor.m_FontSize *= 0.8f;
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pTranslatedError);
			AppendCursor.m_FontSize /= 0.8f;
			TextRender()->TextColor(Color);
		}
		else
		{
			ColoredParts.AddSplitsToCursor(AppendCursor);
			TextRender()->CreateOrAppendTextContainer(Line.m_TextContainerIndex, &AppendCursor, pText);
			AppendCursor.m_vColorSplits.clear();
		}

		if(!g_Config.m_ClChatOld && (Line.m_aText[0] != '\0' || Line.m_aName[0] != '\0'))
		{
			float FullWidth = RealMsgPaddingX * 1.5f;
			if(!IsScoreBoardOpen && !g_Config.m_ClChatOld)
			{
				FullWidth += LineCursor.m_LongestLineWidth + AppendCursor.m_LongestLineWidth;
			}
			else
			{
				FullWidth += maximum(LineCursor.m_LongestLineWidth, AppendCursor.m_LongestLineWidth);
			}
			Graphics()->SetColor(1, 1, 1, 1);
			Line.m_QuadContainerIndex = Graphics()->CreateRectQuadContainer(Begin, y, FullWidth, Line.m_aYOffset[OffsetType], MessageRounding(), IGraphics::CORNER_ALL);
		}

		TextRender()->SetRenderFlags(CurRenderFlags);
		if(Line.m_TextContainerIndex.Valid())
			TextRender()->UploadTextContainer(Line.m_TextContainerIndex);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CChat::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	UpdateMediaDownloads();

	// send pending chat messages
	if(!m_vPendingChatQueue.empty() && m_LastChatSend + time_freq() < time())
	{
		const CPendingChatEntry Entry = m_vPendingChatQueue.front();
		m_vPendingChatQueue.erase(m_vPendingChatQueue.begin());
		SendChat(Entry.m_Team, Entry.m_aText);
	}

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	m_TranslateButtonRectValid = false;

	// TClient
	const auto ChatLayout = HudLayout::Get(HudLayout::MODULE_CHAT, Width, Height);
	const float ChatScale = std::clamp(ChatLayout.m_Scale / 100.0f, 0.25f, 3.0f);
	float y = ChatLayout.m_Y;
	float x = ChatLayout.m_X;
	if(!HudLayout::HasRuntimeOverride(HudLayout::MODULE_CHAT))
		y = 300.0f - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));

	if(!HudLayout::IsEnabled(HudLayout::MODULE_CHAT))
		return;

	float ScaledFontSize = FontSize() * (8.0f / 6.0f);
	if(m_Mode != MODE_NONE)
	{
		// render chat input
		CTextCursor InputCursor;
		InputCursor.SetPosition(vec2(x, y));
		InputCursor.m_FontSize = ScaledFontSize;
		const float AvailableChatWidth = maximum(80.0f, Width - x - 5.0f);
		const float DesiredChatWidth = maximum(80.0f, g_Config.m_ClChatWidth * ChatScale);
		InputCursor.m_LineWidth = minimum(DesiredChatWidth, AvailableChatWidth);

		if(m_Mode == MODE_ALL)
			TextRender()->TextEx(&InputCursor, Localize("All"));
		else if(m_Mode == MODE_TEAM)
			TextRender()->TextEx(&InputCursor, Localize("Team"));
		else
			TextRender()->TextEx(&InputCursor, Localize("Chat"));

		TextRender()->TextEx(&InputCursor, ": ");

		const float TranslateButtonSize = maximum(10.0f, ScaledFontSize * 0.72f);
		const float TranslateButtonGap = 2.0f;
		const float MessageMaxWidth = maximum(40.0f, InputCursor.m_LineWidth - (InputCursor.m_X - InputCursor.m_StartX) - TranslateButtonSize - TranslateButtonGap);
		const CUIRect ClippingRect = {InputCursor.m_X, InputCursor.m_Y, MessageMaxWidth, 2.25f * InputCursor.m_FontSize};
		const float XScale = Graphics()->ScreenWidth() / Width;
		const float YScale = Graphics()->ScreenHeight() / Height;
		Graphics()->ClipEnable((int)(ClippingRect.x * XScale), (int)(ClippingRect.y * YScale), (int)(ClippingRect.w * XScale), (int)(ClippingRect.h * YScale));

		float ScrollOffset = m_Input.GetScrollOffset();
		float ScrollOffsetChange = m_Input.GetScrollOffsetChange();

		m_Input.Activate(EInputPriority::CHAT); // Ensure that the input is active
		const CUIRect InputCursorRect = {InputCursor.m_X, InputCursor.m_Y - ScrollOffset, 0.0f, 0.0f};
		const bool WasChanged = m_Input.WasChanged();
		const bool WasCursorChanged = m_Input.WasCursorChanged();
		const bool Changed = WasChanged || WasCursorChanged;
		const STextBoundingBox BoundingBox = m_Input.Render(&InputCursorRect, InputCursor.m_FontSize, TEXTALIGN_TL, Changed, MessageMaxWidth, 0.0f);

		Graphics()->ClipDisable();

		CUIRect TranslateButtonRect = {minimum(ClippingRect.x + ClippingRect.w + TranslateButtonGap, Width - TranslateButtonSize - 5.0f), ClippingRect.y, TranslateButtonSize, TranslateButtonSize};
		RenderTranslateSettingsButton(TranslateButtonRect);

		// Scroll up or down to keep the caret inside the clipping rect
		const float CaretPositionY = m_Input.GetCaretPosition().y - ScrollOffsetChange;
		if(CaretPositionY < ClippingRect.y)
			ScrollOffsetChange -= ClippingRect.y - CaretPositionY;
		else if(CaretPositionY + InputCursor.m_FontSize > ClippingRect.y + ClippingRect.h)
			ScrollOffsetChange += CaretPositionY + InputCursor.m_FontSize - (ClippingRect.y + ClippingRect.h);

		Ui()->DoSmoothScrollLogic(&ScrollOffset, &ScrollOffsetChange, ClippingRect.h, BoundingBox.m_H);

		m_Input.SetScrollOffset(ScrollOffset);
		m_Input.SetScrollOffsetChange(ScrollOffsetChange);

		// Autocompletion hint
		if(m_Input.GetString()[0] == '/' && m_Input.GetString()[1] != '\0' && !m_vServerCommands.empty())
		{
			for(const auto &Command : m_vServerCommands)
			{
				if(str_startswith_nocase(Command.m_aName, m_Input.GetString() + 1))
				{
					InputCursor.m_X = InputCursor.m_X + TextRender()->TextWidth(InputCursor.m_FontSize, m_Input.GetString(), -1, InputCursor.m_LineWidth);
					InputCursor.m_Y = m_Input.GetCaretPosition().y;
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.5f);
					TextRender()->TextEx(&InputCursor, Command.m_aName + str_length(m_Input.GetString() + 1));
					TextRender()->TextColor(TextRender()->DefaultTextColor());
					break;
				}
			}
		}
	}

	if(m_Mode != MODE_NONE && Ui()->IsPopupOpen())
	{
		Ui()->StartCheck();
		Ui()->Update();
		Ui()->MapScreen();
		Ui()->RenderPopupMenus();
		Ui()->FinishCheck();
		Ui()->ClearHotkeys();
		Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	}

	if(m_Mode != MODE_NONE)
		RenderTools()->RenderCursor(ChatMousePos(), 12.0f);

#if defined(CONF_VIDEORECORDER)
	if(!((g_Config.m_ClShowChat && !IVideo::Current()) || (g_Config.m_ClVideoShowChat && IVideo::Current())))
#else
	if(!g_Config.m_ClShowChat)
#endif
		return;

	y -= ScaledFontSize;

	OnPrepareLines(x, y);

	bool IsScoreBoardOpen = GameClient()->m_Scoreboard.IsActive() && (Graphics()->ScreenAspect() > 1.7f); // only assume scoreboard when screen ratio is widescreen(something around 16:9)
	const bool ShowLargeArea = m_Show || (m_Mode != MODE_NONE && g_Config.m_ClShowChat == 1) || g_Config.m_ClShowChat == 2;

	int64_t Now = time();
	const auto ShouldExpandCompactAreaForMedia = [&]() {
		if(IsScoreBoardOpen || ShowLargeArea || !g_Config.m_MaChatMediaPreview || !AnyMediaAllowed())
			return false;
		for(int i = 0; i < 3; ++i)
		{
			const CLine &RecentLine = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
			if(!RecentLine.m_Initialized)
				break;
			if(ShouldDisplayMediaSlot(RecentLine))
				return true;
		}
		return false;
	};
	const bool ExpandCompactAreaForMedia = ShouldExpandCompactAreaForMedia();
	float HeightLimit = IsScoreBoardOpen ? 180.0f : (ShowLargeArea ? 50.0f : (ExpandCompactAreaForMedia ? y - CHAT_MEDIA_COMPACT_EXPANDED_HEIGHT : 200.0f));
	int OffsetType = IsScoreBoardOpen ? 1 : 0;

	float RealMsgPaddingX = MessagePaddingX();
	float RealMsgPaddingY = MessagePaddingY();

	if(g_Config.m_ClChatOld)
	{
		RealMsgPaddingX = 0;
		RealMsgPaddingY = 0;
	}

	for(int i = 0; i < MAX_LINES; i++)
	{
		CLine &Line = m_aLines[((m_CurrentLine - i) + MAX_LINES) % MAX_LINES];
		if(!Line.m_Initialized)
			break;
		if(Now > Line.m_Time + 16 * time_freq() && !m_PrevShowChat)
			break;

		y -= Line.m_aYOffset[OffsetType];

		// cut off if msgs waste too much space
		if(y < HeightLimit && i != 0)
			break;

		float Blend = Now > Line.m_Time + 14 * time_freq() && !m_PrevShowChat ? 1.0f - (Now - Line.m_Time - 14 * time_freq()) / (2.0f * time_freq()) : 1.0f;

		// Draw backgrounds for messages in one batch
		if(!g_Config.m_ClChatOld)
		{
			Graphics()->TextureClear();
			if(Line.m_QuadContainerIndex != -1)
			{
				Graphics()->SetColor(color_cast<ColorRGBA>(ColorHSLA(g_Config.m_ClChatBackgroundColor, true)).WithMultipliedAlpha(Blend));
				Graphics()->RenderQuadContainerEx(Line.m_QuadContainerIndex, 0, -1, 0, ((y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset));
			}
		}

		if(Line.m_TextContainerIndex.Valid())
		{
			if(!g_Config.m_ClChatOld && Line.m_pManagedTeeRenderInfo != nullptr)
			{
				CTeeRenderInfo &TeeRenderInfo = Line.m_pManagedTeeRenderInfo->TeeRenderInfo();
				const int TeeSize = MessageTeeSize();
				TeeRenderInfo.m_Size = TeeSize;

				float RowHeight = FontSize() + RealMsgPaddingY;
				float OffsetTeeY = TeeSize / 2.0f;
				float FullHeightMinusTee = RowHeight - TeeSize;

				const CAnimState *pIdleState = CAnimState::GetIdle();
				vec2 OffsetToMid;
				CRenderTools::GetRenderTeeOffsetToRenderedTee(pIdleState, &TeeRenderInfo, OffsetToMid);
				vec2 TeeRenderPos(x + (RealMsgPaddingX + TeeSize) / 2.0f, y + OffsetTeeY + FullHeightMinusTee / 2.0f + OffsetToMid.y);
				RenderTools()->RenderTee(pIdleState, &TeeRenderInfo, EMOTE_NORMAL, vec2(1, 0.1f), TeeRenderPos, Blend);
			}

			const ColorRGBA TextColor = TextRender()->DefaultTextColor().WithMultipliedAlpha(Blend);
			const ColorRGBA TextOutlineColor = TextRender()->DefaultTextOutlineColor().WithMultipliedAlpha(Blend);
			const float TextOffsetY = (y + RealMsgPaddingY / 2.0f) - Line.m_TextYOffset;
			TextRender()->RenderTextContainer(Line.m_TextContainerIndex, TextColor, TextOutlineColor, 0, TextOffsetY);

			const bool ShowMediaSlot = ShouldDisplayMediaSlot(Line);
			const bool HasMediaPreview = Line.m_aMediaPreviewWidth[OffsetType] > 0.0f && Line.m_aMediaPreviewHeight[OffsetType] > 0.0f;
			if(ShowMediaSlot && HasMediaPreview)
			{
				const float PreviewX = x + RealMsgPaddingX / 2.0f;
				const float PreviewY = Line.m_TextYOffset + TextOffsetY + Line.m_aTextHeight[OffsetType] + FontSize() * 0.45f;
				const float PreviewW = Line.m_aMediaPreviewWidth[OffsetType];
				const float PreviewH = Line.m_aMediaPreviewHeight[OffsetType];
				const float PreviewRounding = minimum(minimum(PreviewW, PreviewH) / 2.0f, maximum(2.0f, FontSize() * 0.45f));

				Graphics()->TextureClear();
				Graphics()->DrawRect(PreviewX, PreviewY, PreviewW, PreviewH, ColorRGBA(0.08f, 0.08f, 0.10f, 0.75f * Blend), IGraphics::CORNER_ALL, PreviewRounding);

				if(Line.m_MediaState == EMediaState::READY)
				{
					IGraphics::CTextureHandle MediaTexture;
					if(GetCurrentFrameTexture(Line, MediaTexture))
					{
						const float Border = maximum(0.35f, FontSize() * 0.025f);
						const float InnerX = PreviewX + Border;
						const float InnerY = PreviewY + Border;
						const float InnerW = maximum(1.0f, PreviewW - Border * 2.0f);
						const float InnerH = maximum(1.0f, PreviewH - Border * 2.0f);

						Graphics()->WrapClamp();
						Graphics()->TextureSet(MediaTexture);
						Graphics()->QuadsBegin();
						Graphics()->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
						Graphics()->SetColor(1.0f, 1.0f, 1.0f, Blend);
						const IGraphics::CQuadItem PreviewQuad(InnerX, InnerY, InnerW, InnerH);
						Graphics()->QuadsDrawTL(&PreviewQuad, 1);
						Graphics()->QuadsEnd();
						Graphics()->WrapNormal();
						Graphics()->TextureClear();
					}
				}
				else
				{
					CTextCursor LoadingCursor;
					LoadingCursor.SetPosition(vec2(PreviewX + FontSize() * 0.45f, PreviewY + maximum(FontSize() * 0.25f, (PreviewH - FontSize() * 0.75f) / 2.0f)));
					LoadingCursor.m_FontSize = FontSize() * 0.72f;
					TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.8f * Blend);
					TextRender()->TextEx(&LoadingCursor, Line.m_aMediaStatus[0] != '\0' ? Line.m_aMediaStatus : "Loading media...");
					TextRender()->TextColor(TextRender()->DefaultTextColor());
				}
			}
		}
	}

	// TClient: Render chat history panel
	if(m_HistoryMode)
		RenderHistoryPanel();
}

void CChat::EnsureCoherentFontSize() const
{
	// Adjust font size based on width
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatFontSize = g_Config.m_ClChatWidth / CHAT_FONTSIZE_WIDTH_RATIO;
}

void CChat::EnsureCoherentWidth() const
{
	// Adjust width based on font size
	if(g_Config.m_ClChatWidth / (float)g_Config.m_ClChatFontSize >= CHAT_FONTSIZE_WIDTH_RATIO)
		return;

	// We want to keep a ration between font size and font width so that we don't have a weird rendering
	g_Config.m_ClChatWidth = CHAT_FONTSIZE_WIDTH_RATIO * g_Config.m_ClChatFontSize;
}

// ----- send functions -----

void CChat::SendChat(int Team, const char *pLine)
{
	// don't send empty messages
	if(*str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	m_LastChatSend = time();

	if(GameClient()->Client()->IsSixup())
	{
		protocol7::CNetMsg_Cl_Say Msg7;
		Msg7.m_Mode = Team == 1 ? protocol7::CHAT_TEAM : protocol7::CHAT_ALL;
		Msg7.m_Target = -1;
		Msg7.m_pMessage = pLine;
		Client()->SendPackMsgActive(&Msg7, MSGFLAG_VITAL, true);
		return;
	}

	// send chat message
	CNetMsg_Cl_Say Msg;
	Msg.m_Team = Team;
	Msg.m_pMessage = pLine;
	Client()->SendPackMsgActive(&Msg, MSGFLAG_VITAL);
}

void CChat::UpdateSearch()
{
	m_vSearchMatches.clear();
	if(m_aSearchText[0] == '\0')
		return;

	for(int i = 0; i < m_HistoryCount; i++)
	{
		int Idx = ((m_HistoryCurrent - i) + MAX_HISTORY) % MAX_HISTORY;
		if(str_find_nocase(m_aHistoryLines[Idx].m_aText, m_aSearchText) ||
			str_find_nocase(m_aHistoryLines[Idx].m_aName, m_aSearchText))
			m_vSearchMatches.push_back(Idx);
	}
	if(m_CurrentSearchMatch >= (int)m_vSearchMatches.size())
		m_CurrentSearchMatch = 0;
}

void CChat::AddHistoryLine(const CLine &Line)
{
}

void CChat::RenderHistoryPanel()
{
	if(!m_HistoryMode || m_HistoryCount == 0)
		return;

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float ScreenH = ScreenY1 - ScreenY0;
	const float ScreenW = ScreenX1 - ScreenX0;

	const float PanelH = ScreenH * (g_Config.m_TcChatHistoryHeight / 100.0f);
	const float PanelY = ScreenH - PanelH;
	const float PanelW = ScreenW * 0.7f;
	const float PanelX = (ScreenW - PanelW) / 2.0f;

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(0.0f, 0.0f, 0.0f, 0.75f);
	IGraphics::CQuadItem BgQuad(PanelX, PanelY, PanelW, PanelH);
	Graphics()->QuadsDraw(&BgQuad, 1);
	Graphics()->QuadsEnd();

	const float FontSz = 10.0f;
	const float LineH = FontSz + 4.0f;
	float Y = PanelY + PanelH - LineH;

	char aBuf[256];

	// Title
	str_copy(aBuf, TCLocalize("Chat History"));
	float TitleW = TextRender()->TextWidth(FontSz + 2.0f, aBuf);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.9f);
	TextRender()->Text(PanelX + PanelW / 2.0f - TitleW / 2.0f, PanelY + 4.0f, FontSz + 2.0f, aBuf, -1.0f);
	TextRender()->TextColor(TextRender()->DefaultTextColor());

	Y -= LineH;

	// Search bar
	if(m_SearchActive)
	{
		char aSearchDisplay[128];
		str_format(aSearchDisplay, sizeof(aSearchDisplay), "%s: %s", TCLocalize("Search"), m_aSearchText);
		if(m_aSearchText[0] != '\0')
		{
			char aMatch[32];
			str_format(aMatch, sizeof(aMatch), "  (%d %s)", (int)m_vSearchMatches.size(), TCLocalize("matches"));
			str_append(aSearchDisplay, aMatch);
		}
		TextRender()->TextColor(1.0f, 0.8f, 0.2f, 0.9f);
		TextRender()->Text(PanelX + 8.0f, Y, FontSz, aSearchDisplay, -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
		Y -= LineH;
	}

	// Lines
	for(int i = m_HistoryScrollOffset; i < m_HistoryCount; i++)
	{
		if(Y < PanelY + 24.0f)
			break;

		int Idx = ((m_HistoryCurrent - i) + MAX_HISTORY) % MAX_HISTORY;
		CLine &Line = m_aHistoryLines[Idx];
		if(!Line.m_Initialized)
			continue;

		bool IsMatch = m_aSearchText[0] != '\0' && std::find(m_vSearchMatches.begin(), m_vSearchMatches.end(), Idx) != m_vSearchMatches.end();
		if(m_SearchActive && m_aSearchText[0] != '\0' && !IsMatch)
			continue;

		char aTimestamp[16];
		{
			int TotalSecs = (int)(Line.m_Time % 86400);
			int Hours = TotalSecs / 3600;
			int Mins = (TotalSecs % 3600) / 60;
			int Secs = TotalSecs % 60;
			str_format(aTimestamp, sizeof(aTimestamp), "[%02d:%02d:%02d] ", Hours, Mins, Secs);
		}

		if(Line.m_ClientId >= 0 && Line.m_aName[0] != '\0')
			str_format(aBuf, sizeof(aBuf), "%s%s: %s", aTimestamp, Line.m_aName, Line.m_aText);
		else
			str_format(aBuf, sizeof(aBuf), "%s%s", aTimestamp, Line.m_aText);

		if(IsMatch)
		{
			Graphics()->TextureClear();
			Graphics()->QuadsBegin();
			Graphics()->SetColor(1.0f, 0.85f, 0.0f, 0.2f);
			IGraphics::CQuadItem MatchQuad(PanelX + 2.0f, Y - 1.0f, PanelW - 4.0f, LineH);
			Graphics()->QuadsDraw(&MatchQuad, 1);
			Graphics()->QuadsEnd();
			TextRender()->TextColor(1.0f, 0.9f, 0.4f, 1.0f);
		}
		else
		{
			TextRender()->TextColor(0.7f, 0.7f, 0.7f, 1.0f);
		}

		TextRender()->Text(PanelX + 8.0f, Y, FontSz, aBuf, -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());

		Y -= LineH;
	}
}

void CChat::SendChatPayloadQueued(int Team, const char *pLine)
{
	if(!pLine || str_length(pLine) < 1)
		return;

	if(m_LastChatSend + time_freq() < time())
	{
		SendChat(Team, pLine);
	}
	else if(m_vPendingChatQueue.size() < 3)
	{
		CPendingChatEntry Entry;
		Entry.m_Team = Team;
		str_copy(Entry.m_aText, pLine, sizeof(Entry.m_aText));
		m_vPendingChatQueue.emplace_back(Entry);
	}
}

void CChat::SendChatQueued(const char *pLine)
{
	if(!pLine || *str_utf8_skip_whitespaces(pLine) == '\0')
		return;

	const int Team = m_Mode == MODE_ALL ? 0 : 1;

	const int Length = str_length(pLine);
	CHistoryEntry *pEntry = m_History.Allocate(sizeof(CHistoryEntry) + Length);
	pEntry->m_Team = Team;
	str_copy(pEntry->m_aText, pLine, Length + 1);

	if(GameClient()->m_Translate.TryTranslateOutgoingChat(Team, pLine))
		return;
	SendChatPayloadQueued(Team, pLine);
}

void CChat::SendTranslatedChatQueued(int Team, const char *pLine)
{
	SendChatPayloadQueued(Team, pLine);
}

CUIRect CChat::GetHudRect(float Width, float Height, bool ForcePreview) const
{
	const auto Layout = HudLayout::Get(HudLayout::MODULE_CHAT, Width, Height);
	float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float ScaledFontSize = FontSize() * (8.0f / 6.0f);
	const float InputHeight = maximum(2.25f * ScaledFontSize, maximum(ScaledFontSize + 4.0f, 16.0f));
	const float RectHeight = (ForcePreview ? 80.0f : (m_PrevShowChat ? 50.0f : 80.0f)) * Scale + InputHeight;
	const float RectWidth = minimum(g_Config.m_ClChatWidth * Scale, maximum(80.0f, Width - Layout.m_X - 5.0f));
	float AnchorY = Layout.m_Y;
	if(!HudLayout::HasRuntimeOverride(HudLayout::MODULE_CHAT))
		AnchorY = Height - (20.0f * FontSize() / 6.0f + (g_Config.m_TcStatusBar ? g_Config.m_TcStatusBarHeight : 0.0f));
	CUIRect Rect = {Layout.m_X, AnchorY - RectHeight + InputHeight, RectWidth, RectHeight};
	Rect.x = std::clamp(Rect.x, 0.0f, maximum(0.0f, Width - Rect.w));
	Rect.y = std::clamp(Rect.y, 0.0f, maximum(0.0f, Height - Rect.h));
	return Rect;
}

void CChat::RenderHud(bool ForcePreview)
{
	float Height = HudLayout::CANVAS_HEIGHT;
	float Width = Height * Graphics()->ScreenAspect();
	CUIRect Rect = GetHudRect(Width, Height, ForcePreview);
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);
	Graphics()->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, ColorRGBA(0.15f, 0.15f, 0.15f, 0.6f), IGraphics::CORNER_ALL, 5.0f);
}
