#ifndef GAME_CLIENT_COMPONENTS_MA_STREAM_CHAT_H
#define GAME_CLIENT_COMPONENTS_MA_STREAM_CHAT_H

#include <base/lock.h>
#include <base/net.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <atomic>
#include <memory>
#include <string>
#include <vector>

class CHttpRequest;

class CStreamChat : public CComponent
{
public:
	~CStreamChat() override;
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnRender() override;
	void OnShutdown() override;
	void OnReset() override;

	void RequestReconnect();
	void GetStatus(char *pBuf, int Size) const;
	void GetActivitySummary(char *pBuf, int Size) const;
	CUIRect GetHudEditorRect(bool ForcePreview) const;
	CUIRect GetActivityHudEditorRect(bool ForcePreview) const;
	void RenderHudEditor(bool ForcePreview);
	void RenderActivityHudEditor(bool ForcePreview);

private:
	struct SLine
	{
		char m_aUser[32];
		char m_aText[256];
		int64_t m_Time = 0;
	};

	struct SActivity
	{
		char m_aPlatform[16];
		char m_aText[160];
		int64_t m_Time = 0;
	};

	mutable CLock m_Lock;
	std::vector<SLine> m_vLines;
	std::vector<SActivity> m_vActivity;
	std::vector<std::string> m_vKnownUsers;
	void *m_pThread = nullptr;
	std::atomic_bool m_StopThread{false};
	std::atomic_bool m_ThreadFinished{false};
	std::atomic_bool m_ReconnectRequested{false};
	int64_t m_NextReconnectTick = 0;
	int64_t m_ConnectedSince = 0;
	int64_t m_LastMessageTime = 0;
	int64_t m_NextStatsTick = 0;
	int64_t m_LastStatsUpdate = 0;
	int m_CurrentPlatform = 0;
	int m_StatsRequestPlatform = 0;
	int m_TotalMessages = 0;
	int m_aPlatformMessages[4] = {};
	int m_aViewerCount[4] = {-1, -1, -1, -1};
	int m_aPeakViewerCount[4] = {0, 0, 0, 0};
	std::shared_ptr<CHttpRequest> m_pStatsRequest = nullptr;
	char m_aCurrentChannel[128] = "";
	char m_aStatsRequestIdentity[128] = "";
	char m_aStatsIdentity[128] = "";
	char m_aStatsStatus[128] = "Espectadores: API no configurada.";
	char m_aStatus[128] = "Desactivado.";

	static void ThreadEntry(void *pUser);
	void WorkerMain();
	void StopWorker();
	void UpdateWorkerState();
	void StartWorker(const char *pChannel, int Platform);
	void AbortStatsRequest();
	void UpdateViewerStats();
	void StartStatsRequest(int Platform, const char *pIdentity);
	void FinishStatsRequest();
	void SetViewerStats(int Platform, int Viewers, const char *pStatus);
	void SetStatsStatus(const char *pStatus);
	void AddLine(const char *pUser, const char *pText, bool CountStats = true);
	void AddActivity(int Platform, const char *pText);
	void MarkConnected(int Platform);
	void ResetSessionStats();
	void ClearLines();
	void SetStatus(const char *pStatus);
	void RenderPanel(bool ForcePreview);
	void RenderActivityPanel(bool ForcePreview);

	static const char *PlatformName(int Platform);
	static int ClampPlatform(int Platform);
	static bool NormalizeTwitchChannel(const char *pInput, char *pOut, int OutSize);
	static bool ExtractYouTubeVideoId(const char *pInput, char *pOut, int OutSize);
	static bool IsAllDigits(const char *pInput);
	static bool ParseTwitchLine(const char *pLine, char *pUser, int UserSize, char *pMessage, int MessageSize);
	static bool SendIrcLine(NETSOCKET Socket, const char *pLine);
};

#endif