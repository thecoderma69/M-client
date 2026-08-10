#include "updater.h"

#include <base/math.h>
#include <base/process.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/external/json-parser/json.h>
#include <engine/shared/http.h>
#include <engine/shared/json.h>
#include <engine/storage.h>

#include <game/version.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <vector>

static constexpr const char *GITHUB_RELEASES_URL = "https://api.github.com/repos/thecoderma69/M-client/releases?per_page=10";
static constexpr const char *GITHUB_LATEST_RELEASE_URL = "https://github.com/thecoderma69/M-client/releases/latest";
static constexpr const char *UPDATE_ARCHIVE_PATH = "update/ma-client-release.zip";
static constexpr const char *UPDATE_SCRIPT_PATH = "update/ma_apply_update.ps1";

static bool StrEndsWithNoCase(const char *pStr, const char *pSuffix)
{
	if(!pStr || !pSuffix)
		return false;
	const int StrLen = str_length(pStr);
	const int SuffixLen = str_length(pSuffix);
	if(SuffixLen > StrLen)
		return false;
	return str_comp_nocase(pStr + StrLen - SuffixLen, pSuffix) == 0;
}

static void BuildGitHubReleasesUrl(char *pBuf, int BufSize)
{
	str_format(pBuf, BufSize, "%s&t=%lld", GITHUB_RELEASES_URL, (long long)time_timestamp());
}

static std::string ToLowerAscii(const char *pStr)
{
	std::string Lower;
	if(!pStr)
		return Lower;

	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pStr); *p != '\0'; ++p)
		Lower.push_back(static_cast<char>(std::tolower(*p)));
	return Lower;
}

static const char *GetReleaseVersionString(const json_value *pJson)
{
	if(!pJson || pJson->type != json_object)
		return nullptr;

	const char *pVersion = json_string_get(json_object_get(pJson, "tag_name"));
	if(!pVersion)
		pVersion = json_string_get(json_object_get(pJson, "name"));
	return pVersion;
}

static std::vector<int> ExtractVersionNumbers(const char *pVersion)
{
	std::vector<int> vNumbers;
	if(!pVersion)
		return vNumbers;

	int Current = -1;
	for(const unsigned char *p = reinterpret_cast<const unsigned char *>(pVersion); *p != '\0'; ++p)
	{
		if(std::isdigit(*p))
		{
			if(Current < 0)
				Current = 0;
			Current = Current * 10 + (*p - '0');
		}
		else if(Current >= 0)
		{
			vNumbers.push_back(Current);
			Current = -1;
		}
	}

	if(Current >= 0)
		vNumbers.push_back(Current);

	return vNumbers;
}

static int CompareVersionStrings(const char *pLeft, const char *pRight)
{
	const std::vector<int> vLeft = ExtractVersionNumbers(pLeft);
	const std::vector<int> vRight = ExtractVersionNumbers(pRight);
	const size_t Num = maximum(vLeft.size(), vRight.size());
	for(size_t i = 0; i < Num; ++i)
	{
		const int Left = i < vLeft.size() ? vLeft[i] : 0;
		const int Right = i < vRight.size() ? vRight[i] : 0;
		if(Left < Right)
			return -1;
		if(Left > Right)
			return 1;
	}

	return str_comp_nocase(pLeft ? pLeft : "", pRight ? pRight : "");
}

static void ExtractDisplayVersion(const char *pVersion, char *pBuf, int BufSize)
{
	if(BufSize <= 0)
		return;
	pBuf[0] = '\0';
	if(!pVersion)
		return;

	const char *pStart = nullptr;
	for(const char *p = pVersion; *p; ++p)
	{
		if(std::isdigit(static_cast<unsigned char>(*p)))
		{
			pStart = p;
			break;
		}
	}
	if(!pStart)
	{
		str_copy(pBuf, pVersion, BufSize);
		return;
	}

	int Write = 0;
	for(const char *p = pStart; *p && Write + 1 < BufSize; ++p)
	{
		if(!std::isdigit(static_cast<unsigned char>(*p)) && *p != '.')
			break;
		pBuf[Write++] = *p;
	}
	pBuf[Write] = '\0';
	if(pBuf[0] == '\0')
		str_copy(pBuf, pVersion, BufSize);
}

static int ScoreArchiveAsset(const char *pAssetName)
{
	if(!pAssetName)
		return -1;

	const std::string Lower = ToLowerAscii(pAssetName);
	if(Lower.find("m-client") == std::string::npos && Lower.find("mclient") == std::string::npos && Lower.find("ma-client") == std::string::npos && Lower.find("ma_client") == std::string::npos)
		return -1;

#if defined(CONF_FAMILY_WINDOWS)
	if(!StrEndsWithNoCase(pAssetName, ".zip"))
		return -1;
	if(Lower.find("windows") == std::string::npos && Lower.find("win") == std::string::npos)
		return -1;
#else
	return -1;
#endif

	if(Lower.find("debug") != std::string::npos || Lower.find("symbols") != std::string::npos || Lower.find("source") != std::string::npos)
		return -1;

	int Score = 100;
	if(Lower.find("win64") != std::string::npos || Lower.find("x64") != std::string::npos || Lower.find("64") != std::string::npos || Lower.find("amd64") != std::string::npos)
		Score += 30;
	if(Lower.find("v") != std::string::npos)
		Score += 5;
	return Score;
}

static bool ParseReleaseObject(const json_value *pJson, char *pVersion, int VersionSize, char *pArchiveName, int ArchiveNameSize, char *pArchiveUrl, int ArchiveUrlSize)
{
	if(!pJson || pJson->type != json_object)
		return false;

	const char *pReleaseVersion = GetReleaseVersionString(pJson);
	if(!pReleaseVersion)
		return false;

	const json_value *pAssets = json_object_get(pJson, "assets");
	if(!pAssets || pAssets->type != json_array)
		return false;

	int BestScore = -1;
	char aBestName[128] = "";
	char aBestUrl[2048] = "";

	for(int i = 0; i < json_array_length(pAssets); ++i)
	{
		const json_value *pAsset = json_array_get(pAssets, i);
		if(!pAsset || pAsset->type != json_object)
			continue;

		const char *pName = json_string_get(json_object_get(pAsset, "name"));
		const char *pUrl = json_string_get(json_object_get(pAsset, "browser_download_url"));
		const int Score = ScoreArchiveAsset(pName);
		if(!pName || !pUrl || Score < BestScore)
			continue;

		BestScore = Score;
		str_copy(aBestName, pName, sizeof(aBestName));
		str_copy(aBestUrl, pUrl, sizeof(aBestUrl));
	}

	if(BestScore < 0)
		return false;

	str_copy(pVersion, pReleaseVersion, VersionSize);
	str_copy(pArchiveName, aBestName, ArchiveNameSize);
	str_copy(pArchiveUrl, aBestUrl, ArchiveUrlSize);
	return true;
}

static bool ParseLatestRelease(json_value *pJson, char *pVersion, int VersionSize, char *pArchiveName, int ArchiveNameSize, char *pArchiveUrl, int ArchiveUrlSize)
{
	if(!pJson)
		return false;

	if(pJson->type == json_object)
		return ParseReleaseObject(pJson, pVersion, VersionSize, pArchiveName, ArchiveNameSize, pArchiveUrl, ArchiveUrlSize);

	if(pJson->type == json_array)
	{
		const json_value *pBestRelease = nullptr;
		char aBestVersion[64] = "";
		for(int i = 0; i < json_array_length(pJson); ++i)
		{
			const json_value *pRelease = json_array_get(pJson, i);
			const char *pReleaseVersion = GetReleaseVersionString(pRelease);
			if(!pReleaseVersion)
				continue;

			if(!pBestRelease || CompareVersionStrings(pReleaseVersion, aBestVersion) > 0)
			{
				pBestRelease = pRelease;
				str_copy(aBestVersion, pReleaseVersion, sizeof(aBestVersion));
			}
		}

		if(pBestRelease)
			return ParseReleaseObject(pBestRelease, pVersion, VersionSize, pArchiveName, ArchiveNameSize, pArchiveUrl, ArchiveUrlSize);
	}

	return false;
}

static void StripFilename(char *pPath)
{
	if(!pPath)
		return;

	for(int i = str_length(pPath) - 1; i >= 0; --i)
	{
		if(pPath[i] == '/' || pPath[i] == '\\')
		{
			pPath[i] = '\0';
			return;
		}
	}
	pPath[0] = '\0';
}

static bool WriteUpdateApplyScript(const char *pScriptPath)
{
	static const char *pScript =
		"param([int]$Pid,[string]$Archive,[string]$InstallDir,[string]$ExePath)\r\n"
		"$ErrorActionPreference = 'Stop'\r\n"
		"try { Wait-Process -Id $Pid -Timeout 60 -ErrorAction SilentlyContinue } catch {}\r\n"
		"Start-Sleep -Milliseconds 500\r\n"
		"$Temp = Join-Path $env:TEMP ('ma-client-update-' + [guid]::NewGuid().ToString())\r\n"
		"New-Item -ItemType Directory -Force -Path $Temp | Out-Null\r\n"
		"Expand-Archive -LiteralPath $Archive -DestinationPath $Temp -Force\r\n"
		"$Source = $Temp\r\n"
		"$Items = @(Get-ChildItem -LiteralPath $Temp -Force)\r\n"
		"if($Items.Count -eq 1 -and $Items[0].PSIsContainer -and (Test-Path (Join-Path $Items[0].FullName 'DDNet.exe'))) { $Source = $Items[0].FullName }\r\n"
		"$Skip = @('.git','user','dumps','screenshots','videos')\r\n"
		"Get-ChildItem -LiteralPath $Source -Force | Where-Object { $Skip -notcontains $_.Name } | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $InstallDir -Recurse -Force }\r\n"
		"Remove-Item -LiteralPath $Temp -Recurse -Force -ErrorAction SilentlyContinue\r\n"
		"Start-Process -FilePath $ExePath -WorkingDirectory $InstallDir\r\n";

	if(fs_makedir_rec_for(pScriptPath) < 0)
		return false;
	IOHANDLE File = io_open(pScriptPath, IOFLAG_WRITE);
	if(!File)
		return false;
	io_write(File, pScript, str_length(pScript));
	io_close(File);
	return true;
}

CUpdater::CUpdater()
{
	m_pClient = nullptr;
	m_pStorage = nullptr;
	m_pHttp = nullptr;
	m_State = CLEAN;
	m_aStatus[0] = '\0';
	m_Percent = 0;
	m_DownloadAfterCheck = false;
	m_aLatestVersion[0] = '\0';
	m_aArchiveName[0] = '\0';
	m_aArchiveUrl[0] = '\0';
	str_copy(m_aArchivePath, UPDATE_ARCHIVE_PATH, sizeof(m_aArchivePath));
}

void CUpdater::Init(CHttp *pHttp)
{
	m_pClient = Kernel()->RequestInterface<IClient>();
	m_pStorage = Kernel()->RequestInterface<IStorage>();
	m_pHttp = pHttp;
}

void CUpdater::SetCurrentState(EUpdaterState NewState)
{
	const CLockScope LockScope(m_Lock);
	m_State = NewState;
}

void CUpdater::SetStatus(const char *pStatus)
{
	const CLockScope LockScope(m_Lock);
	str_copy(m_aStatus, pStatus ? pStatus : "", sizeof(m_aStatus));
}

void CUpdater::SetPercent(int Percent)
{
	const CLockScope LockScope(m_Lock);
	m_Percent = std::clamp(Percent, 0, 100);
}

IUpdater::EUpdaterState CUpdater::GetCurrentState()
{
	const CLockScope LockScope(m_Lock);
	return m_State;
}

void CUpdater::GetCurrentFile(char *pBuf, int BufSize)
{
	const CLockScope LockScope(m_Lock);
	str_copy(pBuf, m_aStatus, BufSize);
}

int CUpdater::GetCurrentPercent()
{
	const CLockScope LockScope(m_Lock);
	return m_Percent;
}

const char *CUpdater::GetLatestVersionString()
{
	return m_aLatestVersion;
}

void CUpdater::ResetTask()
{
	if(m_pCurrentTask)
	{
		m_pCurrentTask->Abort();
		m_pCurrentTask = nullptr;
	}
	m_TaskKind = ETaskKind::NONE;
}

void CUpdater::StartReleaseFetch()
{
	ResetTask();
	SetStatus("Buscando version");
	SetPercent(0);
	SetCurrentState(IUpdater::GETTING_MANIFEST);

	char aUrl[2304];
	BuildGitHubReleasesUrl(aUrl, sizeof(aUrl));
	m_TaskKind = ETaskKind::FETCH_RELEASE;
	m_pCurrentTask = HttpGet(aUrl);
	m_pCurrentTask->HeaderString("Accept", "application/vnd.github+json");
	m_pCurrentTask->HeaderString("User-Agent", CLIENT_NAME);
	m_pCurrentTask->HeaderString("X-GitHub-Api-Version", "2022-11-28");
	m_pCurrentTask->HeaderString("Cache-Control", "no-cache");
	m_pCurrentTask->HeaderString("Pragma", "no-cache");
	m_pCurrentTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pCurrentTask->IpResolve(IPRESOLVE::V4);
	m_pHttp->Run(m_pCurrentTask);
}

void CUpdater::ParseReleaseTask()
{
	json_value *pJson = m_pCurrentTask ? m_pCurrentTask->ResultJson() : nullptr;
	if(!pJson)
	{
		SetStatus("No se pudo leer GitHub");
		SetCurrentState(IUpdater::FAIL);
		return;
	}

	char aVersion[64] = "";
	char aDisplayVersion[64] = "";
	char aArchiveName[128] = "";
	char aArchiveUrl[2048] = "";

	const bool Parsed = ParseLatestRelease(pJson, aVersion, sizeof(aVersion), aArchiveName, sizeof(aArchiveName), aArchiveUrl, sizeof(aArchiveUrl));
	json_value_free(pJson);

	if(!Parsed || CompareVersionStrings(aVersion, CLIENT_RELEASE_VERSION) <= 0)
	{
		m_aLatestVersion[0] = '\0';
		m_aArchiveName[0] = '\0';
		m_aArchiveUrl[0] = '\0';
		SetStatus("Sin actualizacion");
		SetCurrentState(IUpdater::CLEAN);
		m_DownloadAfterCheck = false;
		return;
	}

	ExtractDisplayVersion(aVersion, aDisplayVersion, sizeof(aDisplayVersion));
	str_copy(m_aLatestVersion, aDisplayVersion[0] ? aDisplayVersion : aVersion, sizeof(m_aLatestVersion));
	str_copy(m_aArchiveName, aArchiveName, sizeof(m_aArchiveName));
	str_copy(m_aArchiveUrl, aArchiveUrl, sizeof(m_aArchiveUrl));

	if(m_DownloadAfterCheck)
	{
		StartArchiveDownload();
		return;
	}

	SetStatus("Actualizacion disponible");
	SetCurrentState(IUpdater::VERSION_AVAILABLE);
}

void CUpdater::StartArchiveDownload()
{
	ResetTask();
	str_copy(m_aArchivePath, UPDATE_ARCHIVE_PATH, sizeof(m_aArchivePath));
	m_pStorage->RemoveBinaryFile(m_aArchivePath);

	SetStatus(m_aArchiveName[0] ? m_aArchiveName : "M-Client update");
	SetPercent(0);
	SetCurrentState(IUpdater::DOWNLOADING);

	m_TaskKind = ETaskKind::DOWNLOAD_ARCHIVE;
	m_pCurrentTask = HttpGetFile(m_aArchiveUrl, m_pStorage, m_aArchivePath, IStorage::TYPE_ABSOLUTE);
	m_pCurrentTask->HeaderString("User-Agent", CLIENT_NAME);
	m_pCurrentTask->Timeout(CTimeout{10000, 0, 500, 10});
	m_pCurrentTask->IpResolve(IPRESOLVE::V4);
	m_pHttp->Run(m_pCurrentTask);
}

bool CUpdater::LaunchApplyScriptAndQuit()
{
#if defined(CONF_FAMILY_WINDOWS)
	char aArchivePath[IO_MAX_PATH_LENGTH];
	char aScriptPath[IO_MAX_PATH_LENGTH];
	char aInstallDir[IO_MAX_PATH_LENGTH];
	char aExePath[IO_MAX_PATH_LENGTH];
	char aPid[32];

	m_pStorage->GetBinaryPath(m_aArchivePath, aArchivePath, sizeof(aArchivePath));
	if(!m_pStorage->FileExists(aArchivePath, IStorage::TYPE_ABSOLUTE))
	{
		SetStatus("No se encontro el ZIP");
		return false;
	}

	m_pStorage->GetBinaryPath(UPDATE_SCRIPT_PATH, aScriptPath, sizeof(aScriptPath));
	if(!WriteUpdateApplyScript(aScriptPath))
	{
		SetStatus("No se pudo crear el aplicador");
		return false;
	}

	m_pStorage->GetBinaryPathAbsolute(PLAT_CLIENT_EXEC, aExePath, sizeof(aExePath));
	str_copy(aInstallDir, aExePath, sizeof(aInstallDir));
	StripFilename(aInstallDir);

	str_format(aPid, sizeof(aPid), "%d", process_id());
	const char *apArguments[] = {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", aScriptPath, aPid, aArchivePath, aInstallDir, aExePath};

	if(process_execute("powershell.exe", EShellExecuteWindowState::BACKGROUND, apArguments, std::size(apArguments)) == INVALID_PROCESS)
	{
		SetStatus("No se pudo iniciar PowerShell");
		return false;
	}

	m_pClient->Quit();
	return true;
#else
	if(m_pClient)
		m_pClient->ViewLink(GITHUB_LATEST_RELEASE_URL);
	SetStatus("Abre la release en GitHub");
	return false;
#endif
}

void CUpdater::CheckForUpdate()
{
	const EUpdaterState State = GetCurrentState();
	if(State == IUpdater::GETTING_MANIFEST || State == IUpdater::DOWNLOADING)
		return;

	m_DownloadAfterCheck = false;
	m_aLatestVersion[0] = '\0';
	m_aArchiveName[0] = '\0';
	m_aArchiveUrl[0] = '\0';
	StartReleaseFetch();
}

void CUpdater::InitiateUpdate()
{
	const EUpdaterState State = GetCurrentState();
	if(State == IUpdater::GETTING_MANIFEST || State == IUpdater::DOWNLOADING)
		return;

	if(State == IUpdater::NEED_RESTART)
	{
		ApplyUpdateAndRestart();
		return;
	}

	if((State == IUpdater::VERSION_AVAILABLE || State == IUpdater::FAIL) && m_aArchiveUrl[0] != '\0')
	{
		StartArchiveDownload();
		return;
	}

	m_DownloadAfterCheck = true;
	StartReleaseFetch();
}

void CUpdater::ApplyUpdateAndRestart()
{
	if(GetCurrentState() != IUpdater::NEED_RESTART)
		return;

	if(!LaunchApplyScriptAndQuit())
		SetCurrentState(IUpdater::FAIL);
}

void CUpdater::Update()
{
	if(!m_pCurrentTask)
		return;

	if(!m_pCurrentTask->Done())
	{
		if(GetCurrentState() == IUpdater::DOWNLOADING)
			SetPercent(m_pCurrentTask->Progress());
		return;
	}

	if(m_pCurrentTask->State() != EHttpState::DONE || m_pCurrentTask->StatusCode() >= 400)
	{
		ResetTask();
		SetStatus("Fallo la actualizacion");
		SetCurrentState(IUpdater::FAIL);
		m_DownloadAfterCheck = false;
		return;
	}

	if(m_TaskKind == ETaskKind::FETCH_RELEASE)
	{
		ParseReleaseTask();
		if(m_TaskKind == ETaskKind::FETCH_RELEASE)
			ResetTask();
		return;
	}

	if(m_TaskKind == ETaskKind::DOWNLOAD_ARCHIVE)
	{
		ResetTask();
		SetPercent(100);
		SetStatus(m_aArchiveName[0] != '\0' ? m_aArchiveName : "update");
		SetCurrentState(IUpdater::NEED_RESTART);
		m_DownloadAfterCheck = false;
	}
}