#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TCLIENT_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TCLIENT_H

#include <engine/client/enums.h>
#include <engine/external/regex.h>
#include <engine/graphics.h>
#include <engine/shared/console.h>
#include <engine/shared/http.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <deque>

class CTClient : public CComponent
{
	std::deque<vec2> m_aAirRescuePositions[NUM_DUMMIES];
	void AirRescue();
	static void ConAirRescue(IConsole::IResult *pResult, void *pUserData);

	static void ConCalc(IConsole::IResult *pResult, void *pUserData);
	static void ConRandomTee(IConsole::IResult *pResult, void *pUserData);
	static void ConchainRandomColor(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void RandomBodyColor();
	static void RandomFeetColor();
	static void RandomSkin(void *pUserData);
	static void RandomFlag(void *pUserData);

	static void ConSpecId(IConsole::IResult *pResult, void *pUserData);
	void SpecId(int ClientId);

	int m_EmoteCycle = 0;
	static void ConEmoteCycle(IConsole::IResult *pResult, void *pUserData);

	class IEngineGraphics *m_pGraphics = nullptr;
	bool m_AspectConfigReady = false;
	bool m_ForcedAspectPending = false;
	bool m_ForcedAspectStateValid = false;
	bool m_LastForcedAspectForce = false;
	bool m_LastForcedAspectApply = false;
	int m_LastForcedAspectMode = -1;
	int m_LastForcedAspectRatio = -1;
	int m_LastForcedAspectNum = -1;
	int m_LastForcedAspectDen = -1;

	char m_PreviousOwnMessage[2048] = {};

	bool SendNonDuplicateMessage(int Team, const char *pLine);

	float m_FinishTextTimeout = 0.0f;
	void DoFinishCheck();
	float m_WeatherParticleRemainder = 0.0f;
	void RenderWeatherParticles();
	IGraphics::CTextureHandle m_AnimeLoveTexture;
	vec2 m_AnimeLoveFollowPos = vec2(0.0f, 0.0f);
	bool m_AnimeLoveFollowPosValid = false;
	void RenderAnimeLove();
	bool m_aCustomSoundFriendOnline[MAX_CLIENTS] = {};
	bool m_CustomSoundFriendStatePrimed = false;
	void PlayCustomSound(int Event);
	void PlayActionSound(int Event);
	void CheckActionSounds();
	void ResetCustomSoundTracking();
	void UpdateFriendJoinSounds();

	struct SFriendNotification
	{
		char m_aName[16];
		char m_aClan[16];
		int64_t m_JoinTime;
	};
	std::deque<SFriendNotification> m_aFriendNotifications;
	void RenderFriendNotifications();

	bool ServerCommandExists(const char *pCommand);

	bool m_AutoReactedStart = false;
	int m_PrevHookState[NUM_DUMMIES] = {-1, -1};
	int m_PrevAttackTick[NUM_DUMMIES] = {0, 0};
	int m_PrevWeapon[NUM_DUMMIES] = {0, 0};
	int m_aCustomActionSampleIds[7] = {-1, -1, -1, -1, -1, -1, -1};
	void LoadCustomActionSounds();

public:
	CTClient();
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnConsoleInit() override;
	void OnRender() override;

	void OnStateChange(int OldState, int NewState) override;
	void OnNewSnapshot() override;
	void SetForcedAspect();

	std::shared_ptr<CHttpRequest> m_pTClientInfoTask = nullptr;
	void FetchTClientInfo();
	void FinishTClientInfo();
	void ResetTClientInfoTask();
	bool NeedUpdate();

	void RenderMiniVoteHud();
	void RenderCenterLines();
	void RenderCtfFlag(vec2 Pos, float Alpha);

	bool ChatDoSpecId(const char *pInput);
	bool InfoTaskDone() { return m_pTClientInfoTask && m_pTClientInfoTask->State() == EHttpState::DONE; }
	bool m_FetchedTClientInfo = false;
	char m_aVersionStr[10] = "0";

	Regex m_RegexChatIgnore;
};

#endif
