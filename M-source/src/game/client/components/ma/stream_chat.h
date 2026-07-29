#ifndef GAME_CLIENT_COMPONENTS_MA_STREAM_CHAT_H
#define GAME_CLIENT_COMPONENTS_MA_STREAM_CHAT_H

#include <base/lock.h>
#include <base/net.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <atomic>
#include <vector>

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
	CUIRect GetHudEditorRect(bool ForcePreview) const;
	void RenderHudEditor(bool ForcePreview);

private:
	struct SLine
	{
		char m_aUser[32];
		char m_aText[256];
		int64_t m_Time = 0;
	};

	mutable CLock m_Lock;
	std::vector<SLine> m_vLines;
	void *m_pThread = nullptr;
	std::atomic_bool m_StopThread{false};
	std::atomic_bool m_ThreadFinished{false};
	std::atomic_bool m_ReconnectRequested{false};
	int64_t m_NextReconnectTick = 0;
	int m_CurrentPlatform = 0;
	char m_aCurrentChannel[128] = "";
	char m_aStatus[128] = "Desactivado.";

	static void ThreadEntry(void *pUser);
	void WorkerMain();
	void StopWorker();
	void UpdateWorkerState();
	void StartWorker(const char *pChannel, int Platform);
	void AddLine(const char *pUser, const char *pText);
	void ClearLines();
	void SetStatus(const char *pStatus);
	void RenderPanel(bool ForcePreview);

	static bool NormalizeTwitchChannel(const char *pInput, char *pOut, int OutSize);
	static bool ParseTwitchLine(const char *pLine, char *pUser, int UserSize, char *pMessage, int MessageSize);
	static bool SendIrcLine(NETSOCKET Socket, const char *pLine);
};

#endif