/* Copyright (c) 2026 BestProject Team */
#include "cherry_gifs.h"

#include <base/log.h>
#include <base/mem.h>
#include <base/system.h>

#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/client/components/media_decoder.h>
#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>

static constexpr const char *CHERRYGIFS_DEFAULT_ENDPOINT = "https://gifs.teeworlds.xyz";
static constexpr const char *CHERRYGIFS_HOST = "gifs.teeworlds.xyz";
static constexpr const char *TENOR_DEFAULT_ENDPOINT = "https://tenor.googleapis.com/v2";
static constexpr const char *GIPHY_DEFAULT_ENDPOINT = "https://api.giphy.com/v1/gifs";
static constexpr int CHERRYGIFS_LIST_LIMIT = 50;
static constexpr float CHERRYGIFS_SEARCH_DEBOUNCE = 0.4f;
static constexpr float CHERRYGIFS_LIST_MIN_INTERVAL = 0.5f;
static constexpr int CHERRYGIFS_MAX_CONCURRENT_THUMBNAILS = 4;
// Defense in depth against a compromised/malicious API response: even though we only ever ask
// for CHERRYGIFS_LIST_LIMIT items, a hostile server could ignore that and send a huge array or
// body to burn memory/CPU on the client. These caps bound the damage regardless of what it sends.
static constexpr int64_t CHERRYGIFS_LIST_MAX_RESPONSE_BYTES = 8 * 1024 * 1024;
static constexpr int CHERRYGIFS_MAX_RESULTS_PER_RESPONSE = 100;
static constexpr int CHERRYGIFS_MAX_TAGS_PER_GIF = 16;
// Hard ceiling on accumulated in-memory results across repeated "Load more" clicks, so a server
// that just keeps claiming hasMore=true can't bait an open-ended session into unbounded growth.
static constexpr int CHERRYGIFS_MAX_TOTAL_RESULTS = 1000;
// The nsfw-by-url cache outlives individual searches (that's the point), so it needs its own,
// more generous cap to bound a very long browsing session.
static constexpr int CHERRYGIFS_MAX_NSFW_CACHE_ENTRIES = 5000;

// The API response is not trusted to only ever point back at itself - a compromised or malicious
// CherryGifs backend could return a "gif" url pointing anywhere (internal network, some exploit
// host, etc.) and we'd otherwise happily fetch and decode whatever's there. Every url that comes
// out of a parsed response is checked against this before we ever issue a follow-up request for
// it, so the only host this client will ever download gif bytes from is the one it already trusts
// with the API key.
// Downloaded bytes are never trusted just because the host/response-status checked out - format
// is verified by magic bytes before anything is handed to the image decoder. Anything that isn't
// a recognized still-image container (whatever the server claims its Content-Type is, whatever
// the url's extension is) is rejected outright rather than passed to the decoder as a "maybe".
static bool IsGifSignature(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 6 && (mem_comp(pData, "GIF87a", 6) == 0 || mem_comp(pData, "GIF89a", 6) == 0);
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

static bool IsRecognizedImageSignature(const unsigned char *pData, size_t DataSize)
{
	return IsGifSignature(pData, DataSize) || IsPngSignature(pData, DataSize) || IsJpegSignature(pData, DataSize) || IsWebpSignature(pData, DataSize) || IsBmpSignature(pData, DataSize);
}

static bool IsCherryGifsUrl(const char *pUrl)
{
	const char *pHost = str_startswith(pUrl, "https://");
	if(!pHost)
		return false;
	const char *pAfterHost = str_startswith(pHost, CHERRYGIFS_HOST);
	if(!pAfterHost)
		return false;
	return *pAfterHost == '\0' || *pAfterHost == '/' || *pAfterHost == ':' || *pAfterHost == '?';
}
static std::string ExtractHttpsHostLower(const char *pUrl)
{
	const char *pHost = str_startswith(pUrl, "https://");
	if(!pHost)
		return {};
	const char *pEnd = pHost;
	while(*pEnd && *pEnd != '/' && *pEnd != ':' && *pEnd != '?')
		++pEnd;
	std::string Host(pHost, pEnd);
	std::transform(Host.begin(), Host.end(), Host.begin(), [](unsigned char c) { return (char)std::tolower(c); });
	return Host;
}

static bool IsExactOrSubdomain(const std::string &Host, const char *pDomain)
{
	const size_t DomainLen = str_length(pDomain);
	if(Host.size() < DomainLen)
		return false;
	if(str_comp_nocase(Host.c_str() + Host.size() - DomainLen, pDomain) != 0)
		return false;
	return Host.size() == DomainLen || Host[Host.size() - DomainLen - 1] == '.';
}

static bool IsTrustedGifUrl(const char *pUrl)
{
	const std::string Host = ExtractHttpsHostLower(pUrl);
	if(Host.empty())
		return false;
	static constexpr const char *s_apDomains[] = {
		"gifs.teeworlds.xyz",
		"imgur.com",
		"giphy.com",
		"tenor.com",
		"discordapp.com",
		"discordapp.net"};
	for(const char *pDomain : s_apDomains)
	{
		if(IsExactOrSubdomain(Host, pDomain))
			return true;
	}
	return false;
}

static bool IsSafeLocalPreviewPath(const char *pPath)
{
	if(!str_startswith(pPath, "ma/gifs/"))
		return false;
	for(const char *p = pPath; *p; ++p)
	{
		if(*p == '\\' || *p == ':')
			return false;
	}
	return str_find(pPath, "..") == nullptr;
}

static std::string TrimCopy(const std::string &Text)
{
	size_t Begin = 0;
	while(Begin < Text.size() && std::isspace((unsigned char)Text[Begin]))
		++Begin;
	size_t End = Text.size();
	while(End > Begin && std::isspace((unsigned char)Text[End - 1]))
		--End;
	return Text.substr(Begin, End - Begin);
}


static std::string SanitizeFavoriteField(const char *pText)
{
	std::string Result = pText ? pText : "";
	for(char &c : Result)
	{
		if(c == '|' || c == ';' || c == '\r' || c == '\n')
			c = ' ';
	}
	return TrimCopy(Result);
}
static std::vector<std::string> SplitFields(const std::string &Line, char Separator)
{
	std::vector<std::string> vFields;
	size_t Start = 0;
	while(Start <= Line.size())
	{
		const size_t End = Line.find(Separator, Start);
		vFields.push_back(TrimCopy(Line.substr(Start, End == std::string::npos ? std::string::npos : End - Start)));
		if(End == std::string::npos)
			break;
		Start = End + 1;
	}
	return vFields;
}

static bool TextMatchesQuery(const SCherryGif &Gif, const char *pQuery)
{
	if(pQuery == nullptr || pQuery[0] == '\0')
		return true;
	if(str_find_nocase(Gif.m_aId, pQuery) || str_find_nocase(Gif.m_aUrl, pQuery) || str_find_nocase(Gif.m_aCaption, pQuery))
		return true;
	for(const std::string &Tag : Gif.m_vTags)
	{
		if(str_find_nocase(Tag.c_str(), pQuery))
			return true;
	}
	return false;
}
static const json_value *GetObjectField(const json_value *pObject, const char *pName)
{
	if(!pObject || pObject->type != json_object)
		return &json_value_none;
	return json_object_get(pObject, pName);
}

static const char *GetStringField(const json_value *pObject, const char *pName)
{
	const json_value *pField = GetObjectField(pObject, pName);
	return pField != &json_value_none && pField->type == json_string ? json_string_get(pField) : nullptr;
}

static const char *GetTenorMediaUrl(const json_value *pMediaFormats, const char *pFormat)
{
	const json_value *pMedia = GetObjectField(pMediaFormats, pFormat);
	return GetStringField(pMedia, "url");
}
static const char *GetGiphyImageUrl(const json_value *pImages, const char *pRendition)
{
	const json_value *pImage = GetObjectField(pImages, pRendition);
	return GetStringField(pImage, "url");
}

static void UrlEncodeQuery(const char *pText, char *pOut, size_t Size)
{
	if(Size == 0)
		return;
	size_t OutPos = 0;
	for(const char *p = pText; *p && OutPos < Size - 1; ++p)
	{
		const unsigned char c = (unsigned char)*p;
		if(isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
		{
			pOut[OutPos++] = (char)c;
		}
		else if(OutPos + 3 < Size)
		{
			snprintf(pOut + OutPos, 4, "%%%02X", c);
			OutPos += 3;
		}
		else
		{
			break;
		}
	}
	pOut[OutPos] = '\0';
}

void CCherryGifs::OnConsoleInit()
{
	Console()->Register("cherrygifs_test_list", "", CFGFLAG_CLIENT, ConCherryGifsTestList, this, "Debug: fetch the first page of the CherryGifs list and log it");
}

void CCherryGifs::ConCherryGifsTestList(IConsole::IResult *pResult, void *pUserData)
{
	CCherryGifs *pThis = static_cast<CCherryGifs *>(pUserData);
	log_info("cherrygifs", "Requesting first page (query='', sort=new, nsfw=exclude)...");
	pThis->StartListRequest(true);
}

void CCherryGifs::SetFilters(const char *pQuery, bool SortTop, bool IncludeNsfw)
{
	if(!m_Initialized)
	{
		m_Initialized = true;
		str_copy(m_aAppliedQuery, pQuery);
		m_AppliedSortTop = SortTop;
		m_AppliedIncludeNsfw = IncludeNsfw;
		str_copy(m_aPendingQuery, pQuery);
		m_PendingSortTop = SortTop;
		m_PendingIncludeNsfw = IncludeNsfw;
		m_PendingChangeTimer = -1.0f;
		StartListRequest(true);
		return;
	}

	if(str_comp(pQuery, m_aAppliedQuery) == 0 && SortTop == m_AppliedSortTop && IncludeNsfw == m_AppliedIncludeNsfw)
	{
		m_PendingChangeTimer = -1.0f;
		return;
	}

	// Only (re)start the debounce timer when the pending values actually change - SetFilters is
	// called every frame from the UI, so unconditionally resetting it here would mean it never
	// reaches CHERRYGIFS_SEARCH_DEBOUNCE and the filters would never actually get applied.
	if(str_comp(pQuery, m_aPendingQuery) != 0 || SortTop != m_PendingSortTop || IncludeNsfw != m_PendingIncludeNsfw)
	{
		str_copy(m_aPendingQuery, pQuery);
		m_PendingSortTop = SortTop;
		m_PendingIncludeNsfw = IncludeNsfw;
		m_PendingChangeTimer = 0.0f;
	}
}


bool CCherryGifs::DecodeThumbnailBytes(SCherryGif &Gif, const unsigned char *pData, size_t DataSize)
{
	MediaDecoder::UnloadFrames(Graphics(), Gif.m_vThumbnailFrames);
	Gif.m_ThumbnailAnimated = false;
	Gif.m_ThumbnailAnimationStart = 0;

	if(!pData || DataSize == 0 || !IsRecognizedImageSignature(pData, DataSize))
	{
		Gif.m_ThumbnailFailed = true;
		return false;
	}

	SMediaDecodedFrames DecodedFrames;
	bool DecodeOk = false;
	if(IsGifSignature(pData, DataSize) || IsWebpSignature(pData, DataSize))
	{
		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = 256;
		Limits.m_MaxFrames = 36;
		Limits.m_MaxTotalBytes = 24ull * 1024ull * 1024ull;
		Limits.m_MaxAnimationDurationMs = 2500;
		Limits.m_DecodeAllFrames = true;
		DecodeOk = MediaDecoder::DecodeAnimatedImageCpu(Graphics(), pData, DataSize, Gif.m_aId, DecodedFrames, Limits);
	}
	if(!DecodeOk)
		DecodeOk = MediaDecoder::DecodeStaticImageCpu(Graphics(), pData, DataSize, Gif.m_aId, DecodedFrames, 256);

	if(!DecodeOk)
	{
		Gif.m_ThumbnailFailed = true;
		return false;
	}

	const bool Animated = DecodedFrames.m_Animated && DecodedFrames.m_vFrames.size() > 1;
	if(!MediaDecoder::UploadFrames(Graphics(), DecodedFrames, Gif.m_vThumbnailFrames, Gif.m_aId) || Gif.m_vThumbnailFrames.empty())
	{
		Gif.m_ThumbnailFailed = true;
		return false;
	}

	Gif.m_ThumbnailAnimated = Animated && Gif.m_vThumbnailFrames.size() > 1;
	Gif.m_ThumbnailAnimationStart = time_get();
	return true;
}

bool CCherryGifs::GetThumbnailTexture(const SCherryGif &Gif, IGraphics::CTextureHandle &Texture) const
{
	return MediaDecoder::GetCurrentFrameTexture(Gif.m_vThumbnailFrames, Gif.m_ThumbnailAnimated, Gif.m_ThumbnailAnimationStart, Texture);
}
void CCherryGifs::EnsureFavorites()
{
	if(m_FavoritesLoaded)
		return;
	m_FavoritesLoaded = true;
	m_vFavorites.clear();

	std::string Data(g_Config.m_MaGifFavorites);
	size_t Start = 0;
	while(Start <= Data.size())
	{
		const size_t End = Data.find(';', Start);
		const std::string Record = Data.substr(Start, End == std::string::npos ? std::string::npos : End - Start);
		std::vector<std::string> vFields = SplitFields(Record, '|');
		if(vFields.size() >= 2 && !vFields[1].empty() && IsTrustedGifUrl(vFields[1].c_str()))
		{
			SCherryGif Gif;
			str_copy(Gif.m_aId, vFields[0].empty() ? vFields[1].c_str() : vFields[0].c_str(), sizeof(Gif.m_aId));
			str_copy(Gif.m_aUrl, vFields[1].c_str(), sizeof(Gif.m_aUrl));
			const char *pPreview = vFields.size() >= 3 && !vFields[2].empty() ? vFields[2].c_str() : Gif.m_aUrl;
			str_copy(Gif.m_aPreviewUrl, (IsTrustedGifUrl(pPreview) || IsSafeLocalPreviewPath(pPreview)) ? pPreview : Gif.m_aUrl, sizeof(Gif.m_aPreviewUrl));
			if(vFields.size() >= 4 && !vFields[3].empty())
				str_copy(Gif.m_aCaption, vFields[3].c_str(), sizeof(Gif.m_aCaption));
			if(FindFavoriteIndex(Gif.m_aUrl) < 0)
				m_vFavorites.push_back(std::move(Gif));
		}
		if(End == std::string::npos)
			break;
		Start = End + 1;
	}
}

void CCherryGifs::SaveFavorites()
{
	std::string Out;
	Out.reserve(sizeof(g_Config.m_MaGifFavorites));
	for(const SCherryGif &Gif : m_vFavorites)
	{
		if(Gif.m_aUrl[0] == '\0')
			continue;
		const std::string Id = SanitizeFavoriteField(Gif.m_aId);
		const std::string Url = SanitizeFavoriteField(Gif.m_aUrl);
		const std::string Preview = SanitizeFavoriteField(Gif.m_aPreviewUrl[0] ? Gif.m_aPreviewUrl : Gif.m_aUrl);
		const std::string Caption = SanitizeFavoriteField(Gif.m_aCaption);
		const std::string Record = Id + "|" + Url + "|" + Preview + "|" + Caption;
		if(Out.size() + Record.size() + 1 >= sizeof(g_Config.m_MaGifFavorites))
			break;
		if(!Out.empty())
			Out += ';';
		Out += Record;
	}
	str_copy(g_Config.m_MaGifFavorites, Out.c_str(), sizeof(g_Config.m_MaGifFavorites));
}

int CCherryGifs::FindFavoriteIndex(const char *pUrl) const
{
	if(!pUrl || pUrl[0] == '\0')
		return -1;
	for(size_t i = 0; i < m_vFavorites.size(); ++i)
	{
		if(str_comp(m_vFavorites[i].m_aUrl, pUrl) == 0)
			return (int)i;
	}
	return -1;
}

bool CCherryGifs::IsFavorite(const char *pUrl)
{
	EnsureFavorites();
	return FindFavoriteIndex(pUrl) >= 0;
}

void CCherryGifs::ToggleFavorite(const SCherryGif &Gif)
{
	EnsureFavorites();
	const int Existing = FindFavoriteIndex(Gif.m_aUrl);
	if(Existing >= 0)
	{
		MediaDecoder::UnloadFrames(Graphics(), m_vFavorites[Existing].m_vThumbnailFrames);
		m_vFavorites.erase(m_vFavorites.begin() + Existing);
		SaveFavorites();
		return;
	}

	SCherryGif Favorite;
	str_copy(Favorite.m_aId, Gif.m_aId[0] ? Gif.m_aId : Gif.m_aUrl, sizeof(Favorite.m_aId));
	str_copy(Favorite.m_aUrl, Gif.m_aUrl, sizeof(Favorite.m_aUrl));
	str_copy(Favorite.m_aPreviewUrl, Gif.m_aPreviewUrl[0] ? Gif.m_aPreviewUrl : Gif.m_aUrl, sizeof(Favorite.m_aPreviewUrl));
	str_copy(Favorite.m_aCaption, Gif.m_aCaption, sizeof(Favorite.m_aCaption));
	Favorite.m_Likes = Gif.m_Likes;
	Favorite.m_Nsfw = Gif.m_Nsfw;
	Favorite.m_vTags = Gif.m_vTags;
	m_vFavorites.insert(m_vFavorites.begin(), std::move(Favorite));
	SaveFavorites();
}

const std::vector<SCherryGif> &CCherryGifs::Favorites()
{
	EnsureFavorites();
	return m_vFavorites;
}
void CCherryGifs::EnsureLocalDatabase()
{
	if(m_LocalDatabaseLoaded)
		return;
	m_LocalDatabaseLoaded = true;
	m_vLocalDatabase.clear();

	void *pFileData = nullptr;
	unsigned FileSize = 0;
	if(Storage() && Storage()->ReadFile("ma/gifs/gifs.txt", IStorage::TYPE_ALL, &pFileData, &FileSize))
	{
		std::string Content(static_cast<const char *>(pFileData), FileSize);
		free(pFileData);

		size_t LineStart = 0;
		while(LineStart <= Content.size())
		{
			const size_t LineEnd = Content.find('\n', LineStart);
			std::string Line = TrimCopy(Content.substr(LineStart, LineEnd == std::string::npos ? std::string::npos : LineEnd - LineStart));
			if(!Line.empty() && Line[0] != '#')
			{
				std::vector<std::string> vFields = SplitFields(Line, '|');
				if(vFields.size() >= 2 && IsTrustedGifUrl(vFields[1].c_str()))
				{
					SCherryGif Gif;
					str_copy(Gif.m_aId, vFields[0].empty() ? vFields[1].c_str() : vFields[0].c_str(), sizeof(Gif.m_aId));
					str_copy(Gif.m_aUrl, vFields[1].c_str(), sizeof(Gif.m_aUrl));
					const char *pPreview = vFields.size() >= 3 && !vFields[2].empty() ? vFields[2].c_str() : Gif.m_aUrl;
					if(str_startswith(pPreview, "ma/gifs/previews/"))
						pPreview = Gif.m_aUrl;
					str_copy(Gif.m_aPreviewUrl, (IsTrustedGifUrl(pPreview) || IsSafeLocalPreviewPath(pPreview)) ? pPreview : Gif.m_aUrl, sizeof(Gif.m_aPreviewUrl));
					if(vFields.size() >= 4 && !vFields[3].empty())
						str_copy(Gif.m_aCaption, vFields[3].c_str(), sizeof(Gif.m_aCaption));
					if(vFields.size() >= 5 && !vFields[4].empty())
					{
						std::vector<std::string> vTags = SplitFields(vFields[4], ',');
						for(const std::string &Tag : vTags)
						{
							if(!Tag.empty())
								Gif.m_vTags.push_back(Tag);
						}
					}
					if(vFields.size() >= 6 && !vFields[5].empty())
						Gif.m_Likes = str_toint(vFields[5].c_str());
					if(vFields.size() >= 7 && !vFields[6].empty())
						Gif.m_Nsfw = str_toint(vFields[6].c_str()) != 0;
					m_vLocalDatabase.push_back(std::move(Gif));
				}
			}
			if(LineEnd == std::string::npos)
				break;
			LineStart = LineEnd + 1;
		}
	}

	std::sort(m_vLocalDatabase.begin(), m_vLocalDatabase.end(), [](const SCherryGif &Left, const SCherryGif &Right) {
		return Left.m_Likes > Right.m_Likes;
	});
	log_info("cherrygifs", "Loaded %d local gifs", (int)m_vLocalDatabase.size());
}

void CCherryGifs::AppendLocalMatches()
{
	EnsureLocalDatabase();
	for(const SCherryGif &LocalGif : m_vLocalDatabase)
	{
		if(LocalGif.m_Nsfw && !m_AppliedIncludeNsfw)
			continue;
		if(!TextMatchesQuery(LocalGif, m_aAppliedQuery))
			continue;

		bool Exists = false;
		for(const SCherryGif &Existing : m_vResults)
		{
			if(str_comp(Existing.m_aUrl, LocalGif.m_aUrl) == 0)
			{
				Exists = true;
				break;
			}
		}
		if(Exists)
			continue;

		m_vResults.push_back(LocalGif);
		if((int)m_NsfwByUrl.size() < CHERRYGIFS_MAX_NSFW_CACHE_ENTRIES)
			m_NsfwByUrl[LocalGif.m_aUrl] = LocalGif.m_Nsfw;
	}
	m_Total = std::max(m_Total, (int)m_vResults.size());
}

void CCherryGifs::ReloadLocalDatabase()
{
	m_LocalDatabaseLoaded = false;
	m_vLocalDatabase.clear();
	StartListRequest(true);
}
void CCherryGifs::ApplyFilters()
{
	str_copy(m_aAppliedQuery, m_aPendingQuery);
	m_AppliedSortTop = m_PendingSortTop;
	m_AppliedIncludeNsfw = m_PendingIncludeNsfw;
	m_PendingChangeTimer = -1.0f;
	StartListRequest(true);
}

void CCherryGifs::LoadMore()
{
	if(m_Loading || !m_HasMore || (int)m_vResults.size() >= CHERRYGIFS_MAX_TOTAL_RESULTS)
		return;
	StartListRequest(false);
}

void CCherryGifs::StartListRequest(bool Reset)
{
	if(m_pListRequest)
	{
		m_pListRequest->Abort();
		m_pListRequest = nullptr;
	}

	if(Reset)
	{
		for(SCherryGif &Gif : m_vResults)
			MediaDecoder::UnloadFrames(Graphics(), Gif.m_vThumbnailFrames);
		m_vResults.clear();
		m_vThumbnailJobs.clear();
		m_Offset = 0;
		m_Total = 0;
		m_aNextPos[0] = '\0';
		m_HasMore = false;
		m_Error = false;
		m_aErrorText[0] = '\0';
	}

	int Source = std::clamp(g_Config.m_MaGifApiSource, 0, 1);
	if(g_Config.m_MaGifApiSource != Source)
		g_Config.m_MaGifApiSource = Source;

	const bool HasGiphyKey = g_Config.m_MaGiphyApiKey[0] != '\0';
	if(Source == 0)
	{
		m_ListProvider = EListProvider::Local;
		AppendLocalMatches();
		m_Loading = false;
		m_HasMore = false;
		return;
	}
	if(!HasGiphyKey)
	{
		m_ListProvider = EListProvider::Giphy;
		AppendLocalMatches();
		m_Loading = false;
		m_HasMore = false;
		m_Error = true;
		str_copy(m_aErrorText, "Pega una API key de GIPHY. Mostrando base local.");
		return;
	}

	char aEncodedQuery[192] = "";
	UrlEncodeQuery(m_aAppliedQuery, aEncodedQuery, sizeof(aEncodedQuery));

	char aUrl[1024];
	m_ListProvider = EListProvider::Giphy;
	char aEncodedKey[384] = "";
	UrlEncodeQuery(g_Config.m_MaGiphyApiKey, aEncodedKey, sizeof(aEncodedKey));
	str_format(aUrl, sizeof(aUrl), "%s/%s?api_key=%s&limit=%d&offset=%d&rating=%s&lang=es&bundle=messaging_non_clips",
		GIPHY_DEFAULT_ENDPOINT, aEncodedQuery[0] ? "search" : "trending", aEncodedKey, CHERRYGIFS_LIST_LIMIT, m_Offset,
		m_AppliedIncludeNsfw ? "r" : "pg-13");
	if(aEncodedQuery[0] != '\0' && str_length(aUrl) + str_length(aEncodedQuery) + 4 < (int)sizeof(aUrl))
	{
		str_append(aUrl, "&q=");
		str_append(aUrl, aEncodedQuery);
	}
	std::shared_ptr<CHttpRequest> pGet = HttpGet(aUrl);
	pGet->Timeout(CTimeout{8000, 0, 500, 10});
	pGet->MaxResponseSize(CHERRYGIFS_LIST_MAX_RESPONSE_BYTES);
	pGet->FailOnErrorStatus(false);
	pGet->LogProgress(HTTPLOG::FAILURE);

	m_pListRequest = pGet;
	m_Loading = true;
	m_ListRequestCooldown = CHERRYGIFS_LIST_MIN_INTERVAL;
	Http()->Run(pGet);
}
void CCherryGifs::PollListRequest()
{
	if(!m_pListRequest)
		return;
	if(m_pListRequest->State() == EHttpState::RUNNING || m_pListRequest->State() == EHttpState::QUEUED)
		return;

	m_Loading = false;

	if(m_pListRequest->State() == EHttpState::ABORTED)
	{
		m_pListRequest = nullptr;
		return;
	}
	if(m_pListRequest->State() != EHttpState::DONE)
	{
		log_error("cherrygifs", "List request failed (state=%d)", (int)m_pListRequest->State());
		m_Error = true;
		str_copy(m_aErrorText, Localize("Connection error, see console"));
		if(m_vResults.empty() && m_ListProvider != EListProvider::Local)
		{
			AppendLocalMatches();
			m_HasMore = false;
		}
		m_pListRequest = nullptr;
		return;
	}

	json_value *pRoot = m_pListRequest->ResultJson();
	const int StatusCode = m_pListRequest->StatusCode();
	const char *pProviderName = m_ListProvider == EListProvider::Tenor ? "Tenor" : "GIPHY";
	auto FallbackToLocalResults = [&]() {
		if(m_vResults.empty() && m_ListProvider != EListProvider::Local)
		{
			AppendLocalMatches();
			m_HasMore = false;
		}
	};

	if(StatusCode != 200)
	{
		m_Error = true;
		const json_value *pErrorField = pRoot && pRoot->type == json_object ? json_object_get(pRoot, "error") : &json_value_none;
		if(pErrorField != &json_value_none && pErrorField->type == json_string)
			str_format(m_aErrorText, sizeof(m_aErrorText), "%s error %d: %s", pProviderName, StatusCode, json_string_get(pErrorField));
		else
			str_format(m_aErrorText, sizeof(m_aErrorText), "%s HTTP error %d", pProviderName, StatusCode);
		log_error("cherrygifs", "%s", m_aErrorText);
		FallbackToLocalResults();
		json_value_free(pRoot);
		m_pListRequest = nullptr;
		return;
	}

	if(!pRoot || pRoot->type != json_object)
	{
		m_Error = true;
		str_copy(m_aErrorText, Localize("Malformed response (not an object)"));
		FallbackToLocalResults();
		json_value_free(pRoot);
		m_pListRequest = nullptr;
		return;
	}

	int AddedCount = 0;
	auto AddGif = [&](SCherryGif &&Gif) {
		if((int)m_NsfwByUrl.size() < CHERRYGIFS_MAX_NSFW_CACHE_ENTRIES)
			m_NsfwByUrl[Gif.m_aUrl] = Gif.m_Nsfw;

		for(const SCherryGif &Existing : m_vResults)
		{
			if(str_comp(Existing.m_aUrl, Gif.m_aUrl) == 0)
				return;
		}

		m_vResults.push_back(std::move(Gif));
		++AddedCount;
	};

	if(m_ListProvider == EListProvider::Tenor)
	{
		const json_value *pNext = json_object_get(pRoot, "next");
		if(pNext != &json_value_none && pNext->type == json_string)
			str_copy(m_aNextPos, json_string_get(pNext), sizeof(m_aNextPos));
		else
			m_aNextPos[0] = '\0';

		const json_value *pResults = json_object_get(pRoot, "results");
		if(pResults != &json_value_none && pResults->type == json_array)
		{
			const int Count = json_array_length(pResults);
			const int ParseCount = std::min(Count, CHERRYGIFS_MAX_RESULTS_PER_RESPONSE);
			for(int i = 0; i < ParseCount; ++i)
			{
				const json_value *pItem = json_array_get(pResults, i);
				if(!pItem || pItem->type != json_object)
					continue;

				const char *pId = GetStringField(pItem, "id");
				const json_value *pMediaFormats = GetObjectField(pItem, "media_formats");
				const char *pShareUrl = GetTenorMediaUrl(pMediaFormats, "tinygif");
				if(!pShareUrl)
					pShareUrl = GetTenorMediaUrl(pMediaFormats, "gif");
				const char *pPreviewUrl = GetTenorMediaUrl(pMediaFormats, "nanogif");
				if(!pPreviewUrl)
					pPreviewUrl = pShareUrl;
				if(!pShareUrl || !IsTrustedGifUrl(pShareUrl) || !pPreviewUrl || !IsTrustedGifUrl(pPreviewUrl))
					continue;

				SCherryGif Gif;
				if(pId && pId[0] != '\0')
					str_format(Gif.m_aId, sizeof(Gif.m_aId), "tenor-%s", pId);
				else
					str_format(Gif.m_aId, sizeof(Gif.m_aId), "tenor-%d-%d", m_Offset, i);
				str_copy(Gif.m_aUrl, pShareUrl, sizeof(Gif.m_aUrl));
				str_copy(Gif.m_aPreviewUrl, pPreviewUrl, sizeof(Gif.m_aPreviewUrl));

				const char *pCaption = GetStringField(pItem, "content_description");
				if(!pCaption)
					pCaption = GetStringField(pItem, "title");
				if(pCaption && pCaption[0] != '\0')
					str_copy(Gif.m_aCaption, pCaption, sizeof(Gif.m_aCaption));
				else if(m_aAppliedQuery[0] != '\0')
					str_copy(Gif.m_aCaption, m_aAppliedQuery, sizeof(Gif.m_aCaption));
				else
					str_copy(Gif.m_aCaption, "Tenor GIF", sizeof(Gif.m_aCaption));

				const json_value *pTags = json_object_get(pItem, "tags");
				if(pTags != &json_value_none && pTags->type == json_array)
				{
					const int TagCount = std::min(json_array_length(pTags), CHERRYGIFS_MAX_TAGS_PER_GIF);
					for(int t = 0; t < TagCount; ++t)
					{
						const json_value *pTag = json_array_get(pTags, t);
						if(pTag && pTag->type == json_string)
							Gif.m_vTags.emplace_back(json_string_get(pTag));
					}
				}
				Gif.m_Likes = std::max(0, 1000 - (m_Offset + i));
				AddGif(std::move(Gif));
			}
			m_Offset += Count;
			m_Total = std::max(m_Total, m_Offset + (m_aNextPos[0] != '\0' ? CHERRYGIFS_LIST_LIMIT : 0));
		}
		m_HasMore = AddedCount > 0 && m_aNextPos[0] != '\0' && (int)m_vResults.size() < CHERRYGIFS_MAX_TOTAL_RESULTS;
		log_info("cherrygifs", "Tenor request done: +%d gifs (have=%d, hasMore=%d)", AddedCount, (int)m_vResults.size(), m_HasMore ? 1 : 0);
	}
	else if(m_ListProvider == EListProvider::Giphy)
	{
		const json_value *pPagination = json_object_get(pRoot, "pagination");
		const json_value *pTotal = GetObjectField(pPagination, "total_count");
		if(pTotal != &json_value_none && pTotal->type == json_integer)
			m_Total = json_int_get(pTotal);

		const json_value *pData = json_object_get(pRoot, "data");
		if(pData != &json_value_none && pData->type == json_array)
		{
			const int Count = json_array_length(pData);
			const int ParseCount = std::min(Count, CHERRYGIFS_MAX_RESULTS_PER_RESPONSE);
			for(int i = 0; i < ParseCount; ++i)
			{
				const json_value *pItem = json_array_get(pData, i);
				if(!pItem || pItem->type != json_object)
					continue;

				const char *pId = GetStringField(pItem, "id");
				const json_value *pImages = GetObjectField(pItem, "images");
				const char *pShareUrl = GetGiphyImageUrl(pImages, "original");
				if(!pShareUrl)
					pShareUrl = GetGiphyImageUrl(pImages, "downsized");
				if(!pShareUrl)
					pShareUrl = GetGiphyImageUrl(pImages, "fixed_width");
				const char *pPreviewUrl = GetGiphyImageUrl(pImages, "fixed_width");
				if(!pPreviewUrl)
					pPreviewUrl = GetGiphyImageUrl(pImages, "downsized");
				if(!pPreviewUrl)
					pPreviewUrl = pShareUrl;
				if(!pShareUrl || !IsTrustedGifUrl(pShareUrl) || !pPreviewUrl || !IsTrustedGifUrl(pPreviewUrl))
					continue;

				SCherryGif Gif;
				if(pId && pId[0] != '\0')
					str_format(Gif.m_aId, sizeof(Gif.m_aId), "giphy-%s", pId);
				else
					str_format(Gif.m_aId, sizeof(Gif.m_aId), "giphy-%d-%d", m_Offset, i);
				str_copy(Gif.m_aUrl, pShareUrl, sizeof(Gif.m_aUrl));
				str_copy(Gif.m_aPreviewUrl, pPreviewUrl, sizeof(Gif.m_aPreviewUrl));

				const char *pTitle = GetStringField(pItem, "title");
				if(pTitle && pTitle[0] != '\0')
					str_copy(Gif.m_aCaption, pTitle, sizeof(Gif.m_aCaption));
				else if(m_aAppliedQuery[0] != '\0')
					str_copy(Gif.m_aCaption, m_aAppliedQuery, sizeof(Gif.m_aCaption));
				else
					str_copy(Gif.m_aCaption, "GIPHY GIF", sizeof(Gif.m_aCaption));

				Gif.m_Likes = std::max(0, 1000 - (m_Offset + i));
				AddGif(std::move(Gif));
			}
			m_Offset += Count;
		}

		m_HasMore = AddedCount > 0 && m_Offset < m_Total && (int)m_vResults.size() < CHERRYGIFS_MAX_TOTAL_RESULTS;
		log_info("cherrygifs", "GIPHY request done: +%d gifs (total known=%d, have=%d, hasMore=%d)", AddedCount, m_Total, (int)m_vResults.size(), m_HasMore ? 1 : 0);
	}
	else
	{
		const json_value *pTotal = json_object_get(pRoot, "total");
		if(pTotal != &json_value_none && pTotal->type == json_integer)
			m_Total = json_int_get(pTotal);

		const json_value *pGifs = json_object_get(pRoot, "gifs");
		if(pGifs != &json_value_none && pGifs->type == json_array)
		{
			const int Count = json_array_length(pGifs);
			const int ParseCount = std::min(Count, CHERRYGIFS_MAX_RESULTS_PER_RESPONSE);
			for(int i = 0; i < ParseCount; ++i)
			{
				const json_value *pItem = json_array_get(pGifs, i);
				if(!pItem || pItem->type != json_object)
					continue;

				const json_value *pId = json_object_get(pItem, "id");
				const json_value *pUrl = json_object_get(pItem, "url");
				if(pId == &json_value_none || pId->type != json_string || pUrl == &json_value_none || pUrl->type != json_string)
					continue;
				if(!IsCherryGifsUrl(json_string_get(pUrl)))
					continue;

				SCherryGif Gif;
				str_copy(Gif.m_aId, json_string_get(pId), sizeof(Gif.m_aId));
				str_copy(Gif.m_aUrl, json_string_get(pUrl), sizeof(Gif.m_aUrl));

				const json_value *pPreviewUrl = json_object_get(pItem, "previewUrl");
				const char *pPreviewUrlStr = (pPreviewUrl != &json_value_none && pPreviewUrl->type == json_string) ? json_string_get(pPreviewUrl) : nullptr;
				str_copy(Gif.m_aPreviewUrl, (pPreviewUrlStr && IsCherryGifsUrl(pPreviewUrlStr)) ? pPreviewUrlStr : Gif.m_aUrl, sizeof(Gif.m_aPreviewUrl));

				const json_value *pCaption = json_object_get(pItem, "caption");
				if(pCaption != &json_value_none && pCaption->type == json_string)
					str_copy(Gif.m_aCaption, json_string_get(pCaption), sizeof(Gif.m_aCaption));

				const json_value *pLikes = json_object_get(pItem, "likes");
				if(pLikes != &json_value_none && pLikes->type == json_integer)
					Gif.m_Likes = json_int_get(pLikes);

				const json_value *pTags = json_object_get(pItem, "tags");
				if(pTags != &json_value_none && pTags->type == json_array)
				{
					const int TagCount = std::min(json_array_length(pTags), CHERRYGIFS_MAX_TAGS_PER_GIF);
					for(int t = 0; t < TagCount; ++t)
					{
						const json_value *pTag = json_array_get(pTags, t);
						if(pTag && pTag->type == json_string)
							Gif.m_vTags.emplace_back(json_string_get(pTag));
					}
				}

				const json_value *pNsfw = json_object_get(pItem, "nsfw");
				if(pNsfw != &json_value_none && pNsfw->type == json_boolean)
					Gif.m_Nsfw = json_boolean_get(pNsfw) != 0;

				AddGif(std::move(Gif));
			}
			m_Offset += Count;
		}

		m_HasMore = AddedCount > 0 && m_Offset < m_Total && (int)m_vResults.size() < CHERRYGIFS_MAX_TOTAL_RESULTS;
		log_info("cherrygifs", "CherryGifs request done: +%d gifs (total known=%d, have=%d, hasMore=%d)", AddedCount, m_Total, (int)m_vResults.size(), m_HasMore ? 1 : 0);
	}

	json_value_free(pRoot);
	m_pListRequest = nullptr;
}
void CCherryGifs::RequestThumbnail(int Index)
{
	if(Index < 0 || Index >= (int)m_vResults.size())
		return;
	RequestThumbnail(m_vResults[Index], false);
}

void CCherryGifs::RequestFavoriteThumbnail(int Index)
{
	EnsureFavorites();
	if(Index < 0 || Index >= (int)m_vFavorites.size())
		return;
	RequestThumbnail(m_vFavorites[Index], true);
}

void CCherryGifs::RequestThumbnail(SCherryGif &Gif, bool Favorite)
{
	if(!Gif.m_vThumbnailFrames.empty() || Gif.m_ThumbnailRequested || Gif.m_ThumbnailFailed)
		return;
	if((int)m_vThumbnailJobs.size() >= CHERRYGIFS_MAX_CONCURRENT_THUMBNAILS)
		return;
	if(Gif.m_aPreviewUrl[0] == '\0')
		return;
	// Re-check even though PollListRequest already filtered on ingest - cheap, and it means this
	// function stays safe to call on its own if that ever changes.
	if(!IsTrustedGifUrl(Gif.m_aPreviewUrl) && !IsSafeLocalPreviewPath(Gif.m_aPreviewUrl))
	{
		Gif.m_ThumbnailFailed = true;
		return;
	}

	Gif.m_ThumbnailRequested = true;

	if(IsSafeLocalPreviewPath(Gif.m_aPreviewUrl))
	{
		void *pFileData = nullptr;
		unsigned FileSize = 0;
		if(Storage() && Storage()->ReadFile(Gif.m_aPreviewUrl, IStorage::TYPE_ALL, &pFileData, &FileSize))
		{
			DecodeThumbnailBytes(Gif, static_cast<const unsigned char *>(pFileData), FileSize);
			free(pFileData);
		}
		else
		{
			Gif.m_ThumbnailFailed = true;
		}
		return;
	}

	SThumbnailJob Job;
	str_copy(Job.m_aGifId, Gif.m_aId, sizeof(Job.m_aGifId));
	str_copy(Job.m_aUrl, Gif.m_aUrl, sizeof(Job.m_aUrl));
	Job.m_Favorite = Favorite;

	std::shared_ptr<CHttpRequest> pGet = HttpGet(Gif.m_aPreviewUrl);
	pGet->Timeout(CTimeout{8000, 0, 4096, 8});
	pGet->MaxResponseSize(8 * 1024 * 1024);
	pGet->FailOnErrorStatus(false);
	pGet->LogProgress(HTTPLOG::NONE);
	Job.m_pRequest = pGet;
	Http()->Run(pGet);

	m_vThumbnailJobs.push_back(std::move(Job));
}
void CCherryGifs::PollThumbnails()
{
	for(size_t i = 0; i < m_vThumbnailJobs.size();)
	{
		SThumbnailJob &Job = m_vThumbnailJobs[i];
		if(!Job.m_pRequest->Done())
		{
			++i;
			continue;
		}

		auto FindTarget = [&](std::vector<SCherryGif> &vGifs) -> SCherryGif * {
			for(SCherryGif &Candidate : vGifs)
			{
				if((Job.m_aUrl[0] != '\0' && str_comp(Candidate.m_aUrl, Job.m_aUrl) == 0) || str_comp(Candidate.m_aId, Job.m_aGifId) == 0)
					return &Candidate;
			}
			return nullptr;
		};

		SCherryGif *pGif = Job.m_Favorite ? FindTarget(m_vFavorites) : FindTarget(m_vResults);
		if(!pGif)
			pGif = Job.m_Favorite ? FindTarget(m_vResults) : FindTarget(m_vFavorites);

		if(pGif && Job.m_pRequest->State() == EHttpState::DONE && Job.m_pRequest->StatusCode() == 200)
		{
			unsigned char *pData = nullptr;
			size_t DataSize = 0;
			Job.m_pRequest->Result(&pData, &DataSize);
			DecodeThumbnailBytes(*pGif, pData, DataSize);
		}
		else if(pGif)
		{
			pGif->m_ThumbnailFailed = true;
		}

		m_vThumbnailJobs.erase(m_vThumbnailJobs.begin() + i);
	}
}
bool CCherryGifs::TryGetNsfw(const char *pUrl, bool &OutNsfw) const
{
	const auto It = m_NsfwByUrl.find(pUrl);
	if(It == m_NsfwByUrl.end())
		return false;
	OutNsfw = It->second;
	return true;
}

void CCherryGifs::OnRender()
{
	if(m_ListRequestCooldown > 0.0f)
		m_ListRequestCooldown = std::max(0.0f, m_ListRequestCooldown - Client()->RenderFrameTime());

	PollListRequest();
	PollThumbnails();

	if(m_PendingChangeTimer >= 0.0f)
	{
		m_PendingChangeTimer += Client()->RenderFrameTime();
		if(m_PendingChangeTimer >= CHERRYGIFS_SEARCH_DEBOUNCE && !m_Loading && m_ListRequestCooldown <= 0.0f)
			ApplyFilters();
	}
}
