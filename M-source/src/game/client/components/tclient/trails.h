#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_TRAILS_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_TRAILS_H

#include <base/color.h>

#include <engine/shared/protocol.h>

#include <game/client/component.h>
#include <game/client/components/ma/visualizer/service.h>

#include <cstdint>
#include <memory>

class CTrailPart
{
public:
	vec2 m_Pos = vec2(0.0f, 0.0f);
	vec2 m_UnmovedPos = vec2(0.0f, 0.0f);
	ColorRGBA m_Col;
	float m_Width = 0.0f;
	vec2 m_Normal = vec2(0.0f, 0.0f);
	vec2 m_Top = vec2(0.0f, 0.0f);
	vec2 m_Bot = vec2(0.0f, 0.0f);
	bool m_Flip = false;
	float m_Progress = 1.0f;
	int m_Tick = -1;

	bool operator==(const CTrailPart &Other) const
	{
		return m_Pos == Other.m_Pos;
	}
};

class CTrails : public CComponent
{
public:
	CTrails() = default;
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnReset() override;

	enum COLORMODES
	{
		COLORMODE_SOLID = 1,
		COLORMODE_TEE,
		COLORMODE_RAINBOW,
		COLORMODE_SPEED,
	};

	enum TRAILSTYLES
	{
		TRAILSTYLE_DEFAULT = 0,
		TRAILSTYLE_HEARTS,
		TRAILSTYLE_STARS,
		TRAILSTYLE_DIAMONDS,
		TRAILSTYLE_MOONS,
		TRAILSTYLE_LIGHTNING,
		TRAILSTYLE_BUTTERFLIES,
		TRAILSTYLE_FLOWERS,
		TRAILSTYLE_MUSIC,
		TRAILSTYLE_SKULLS,
		TRAILSTYLE_CROWNS,
		TRAILSTYLE_FLAMES,
		TRAILSTYLE_SNOWFLAKES,
	};

private:
	class CInfo
	{
	public:
		vec2 m_Pos;
		int m_Tick;
	};
	CInfo m_History[MAX_CLIENTS][200];
	bool m_HistoryValid[MAX_CLIENTS] = {};
	std::unique_ptr<BestClientVisualizer::CRealtimeMusicVisualizer> m_pMusicVisualizer;
	BestClientVisualizer::SVisualizerFrame m_LastMusicReactionFrame;
	int64_t m_LastMusicReactionPollTick = 0;
	float m_MusicReactionLevel = 0.0f;
	float m_MusicReactionKick = 0.0f;
	float m_MusicReactionRollingPeak = 0.05f;

	void ClearAllHistory();
	void ClearHistory(int ClientId);
	bool ShouldPredictPlayer(int ClientId);
	void ResetMusicReaction();
	float UpdateMusicReaction(float Delta);
};

#endif
