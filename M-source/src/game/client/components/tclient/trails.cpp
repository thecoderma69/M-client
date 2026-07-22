#include "trails.h"

#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/animstate.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

static float ApproachTrailValue(float Current, float Target, float Delta, float Speed)
{
	const float Amount = std::clamp(Delta * Speed, 0.0f, 1.0f);
	return mix(Current, Target, Amount);
}

static float TrailPerformanceLodScale(float Delta)
{
	float Lod = 1.0f;
	if(Delta <= 0.0f)
		return Lod;
	if(Delta > 1.0f / 180.0f)
		Lod = 0.35f;
	else if(Delta > 1.0f / 300.0f)
		Lod = 0.50f;
	else if(Delta > 1.0f / 500.0f)
		Lod = 0.68f;
	else if(Delta > 1.0f / 750.0f)
		Lod = 0.84f;

	if(g_Config.m_MaPerformanceGuard)
	{
		const int TargetFps = std::clamp(g_Config.m_MaPerformanceGuardTargetFps, 60, 1000);
		const float TargetDelta = 1.0f / (float)TargetFps;
		if(Delta > TargetDelta)
			Lod = std::min(Lod, std::clamp(TargetDelta / Delta, 0.35f, 1.0f));
	}
	return Lod;
}

template<std::size_t NumPoints>
static void DrawTrailShape(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle, const std::array<vec2, NumPoints> &Shape, bool Closed = true)
{
	const std::size_t NumLines = Closed ? Shape.size() : Shape.size() - 1;
	const float C = std::cos(Angle);
	const float S = std::sin(Angle);
	std::array<IGraphics::CLineItem, NumPoints> aLines;
	for(std::size_t i = 0; i < NumLines; ++i)
	{
		const vec2 FromPoint = vec2(Shape[i].x * C - Shape[i].y * S, Shape[i].x * S + Shape[i].y * C);
		const vec2 ToPoint = vec2(Shape[(i + 1) % Shape.size()].x * C - Shape[(i + 1) % Shape.size()].y * S, Shape[(i + 1) % Shape.size()].x * S + Shape[(i + 1) % Shape.size()].y * C);
		const vec2 From = Center + FromPoint * Size;
		const vec2 To = Center + ToPoint * Size;
		aLines[i] = IGraphics::CLineItem(From.x, From.y, To.x, To.y);
	}
	pGraphics->SetColor(Color);
	pGraphics->LinesDraw(aLines.data(), NumLines);
}

static void DrawTrailHeart(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 12> s_aHeartPoints = {
		vec2(0.0f, 0.95f),
		vec2(-0.48f, 0.52f),
		vec2(-0.86f, 0.08f),
		vec2(-1.0f, -0.43f),
		vec2(-0.82f, -0.88f),
		vec2(-0.36f, -1.02f),
		vec2(0.0f, -0.78f),
		vec2(0.36f, -1.02f),
		vec2(0.82f, -0.88f),
		vec2(1.0f, -0.43f),
		vec2(0.86f, 0.08f),
		vec2(0.48f, 0.52f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aHeartPoints);
}

static void DrawTrailStar(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 10> s_aStarPoints = {
		vec2(0.0f, -1.0f),
		vec2(0.22f, -0.31f),
		vec2(0.95f, -0.31f),
		vec2(0.36f, 0.12f),
		vec2(0.59f, 0.81f),
		vec2(0.0f, 0.38f),
		vec2(-0.59f, 0.81f),
		vec2(-0.36f, 0.12f),
		vec2(-0.95f, -0.31f),
		vec2(-0.22f, -0.31f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aStarPoints);
}

static void DrawTrailDiamond(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 4> s_aDiamondPoints = {
		vec2(0.0f, -1.0f),
		vec2(0.72f, 0.0f),
		vec2(0.0f, 1.0f),
		vec2(-0.72f, 0.0f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aDiamondPoints);

	static const std::array<vec2, 2> s_aVertical = {vec2(0.0f, -1.0f), vec2(0.0f, 1.0f)};
	static const std::array<vec2, 2> s_aHorizontal = {vec2(-0.72f, 0.0f), vec2(0.72f, 0.0f)};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aVertical, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aHorizontal, false);
}

static void DrawTrailMoon(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 15> s_aMoonPoints = {
		vec2(0.42f, -0.96f),
		vec2(0.05f, -0.92f),
		vec2(-0.30f, -0.72f),
		vec2(-0.56f, -0.40f),
		vec2(-0.68f, 0.00f),
		vec2(-0.56f, 0.40f),
		vec2(-0.30f, 0.72f),
		vec2(0.05f, 0.92f),
		vec2(0.42f, 0.96f),
		vec2(0.24f, 0.66f),
		vec2(0.18f, 0.34f),
		vec2(0.22f, 0.02f),
		vec2(0.36f, -0.30f),
		vec2(0.56f, -0.58f),
		vec2(0.74f, -0.78f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aMoonPoints);
}

static void DrawTrailLightning(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 7> s_aLightningPoints = {
		vec2(0.06f, -1.00f),
		vec2(0.62f, -0.18f),
		vec2(0.24f, -0.18f),
		vec2(0.54f, 1.00f),
		vec2(-0.62f, 0.08f),
		vec2(-0.18f, 0.08f),
		vec2(-0.42f, -1.00f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aLightningPoints);
}

static void DrawTrailButterfly(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 14> s_aButterflyPoints = {
		vec2(0.00f, -0.84f),
		vec2(-0.20f, -0.48f),
		vec2(-0.88f, -0.78f),
		vec2(-0.74f, -0.04f),
		vec2(-0.94f, 0.58f),
		vec2(-0.24f, 0.36f),
		vec2(0.00f, 0.90f),
		vec2(0.24f, 0.36f),
		vec2(0.94f, 0.58f),
		vec2(0.74f, -0.04f),
		vec2(0.88f, -0.78f),
		vec2(0.20f, -0.48f),
		vec2(0.00f, -0.84f),
		vec2(0.00f, 0.90f),
	};
	static const std::array<vec2, 3> s_aLeftAntenna = {vec2(0.0f, -0.84f), vec2(-0.18f, -1.08f), vec2(-0.36f, -1.02f)};
	static const std::array<vec2, 3> s_aRightAntenna = {vec2(0.0f, -0.84f), vec2(0.18f, -1.08f), vec2(0.36f, -1.02f)};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aButterflyPoints, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aLeftAntenna, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aRightAntenna, false);
}

static const std::array<vec2, 48> &TrailFlowerPoints()
{
	static std::array<vec2, 48> s_aFlowerPoints;
	static bool s_Initialized = false;
	if(!s_Initialized)
	{
		for(int i = 0; i < (int)s_aFlowerPoints.size(); i++)
		{
			const float T = -0.5f * pi + 2.0f * pi * (float)i / (float)s_aFlowerPoints.size();
			const float R = 0.64f + 0.28f * std::sin(5.0f * T);
			s_aFlowerPoints[i] = vec2(std::cos(T) * R, std::sin(T) * R);
		}
		s_Initialized = true;
	}
	return s_aFlowerPoints;
}

static void DrawTrailFlower(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, TrailFlowerPoints());

	std::array<vec2, 12> aCenterPoints;
	for(int i = 0; i < (int)aCenterPoints.size(); i++)
	{
		const float T = 2.0f * pi * (float)i / (float)aCenterPoints.size();
		aCenterPoints[i] = vec2(std::cos(T) * 0.18f, std::sin(T) * 0.18f);
	}
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, aCenterPoints);
}

static void DrawTrailMusic(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	std::array<vec2, 14> aLeftHead;
	std::array<vec2, 14> aRightHead;
	for(int i = 0; i < (int)aLeftHead.size(); i++)
	{
		const float T = 2.0f * pi * (float)i / (float)aLeftHead.size();
		aLeftHead[i] = vec2(-0.34f + std::cos(T) * 0.24f, 0.48f + std::sin(T) * 0.16f);
		aRightHead[i] = vec2(0.42f + std::cos(T) * 0.24f, 0.26f + std::sin(T) * 0.16f);
	}
	static const std::array<vec2, 2> s_aLeftStem = {vec2(-0.12f, 0.48f), vec2(-0.12f, -0.78f)};
	static const std::array<vec2, 2> s_aRightStem = {vec2(0.64f, 0.26f), vec2(0.64f, -0.56f)};
	static const std::array<vec2, 2> s_aBeam = {vec2(-0.12f, -0.78f), vec2(0.64f, -0.56f)};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, aLeftHead);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, aRightHead);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aLeftStem, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aRightStem, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aBeam, false);
}

static void DrawTrailSkull(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 18> s_aSkullPoints = {
		vec2(0.00f, -0.96f),
		vec2(0.48f, -0.82f),
		vec2(0.78f, -0.48f),
		vec2(0.82f, -0.04f),
		vec2(0.66f, 0.34f),
		vec2(0.36f, 0.52f),
		vec2(0.34f, 0.88f),
		vec2(0.12f, 0.88f),
		vec2(0.12f, 0.62f),
		vec2(-0.12f, 0.62f),
		vec2(-0.12f, 0.88f),
		vec2(-0.34f, 0.88f),
		vec2(-0.36f, 0.52f),
		vec2(-0.66f, 0.34f),
		vec2(-0.82f, -0.04f),
		vec2(-0.78f, -0.48f),
		vec2(-0.48f, -0.82f),
		vec2(0.00f, -0.96f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aSkullPoints, false);

	std::array<vec2, 10> aLeftEye;
	std::array<vec2, 10> aRightEye;
	for(int i = 0; i < (int)aLeftEye.size(); i++)
	{
		const float T = 2.0f * pi * (float)i / (float)aLeftEye.size();
		aLeftEye[i] = vec2(-0.28f + std::cos(T) * 0.16f, -0.14f + std::sin(T) * 0.18f);
		aRightEye[i] = vec2(0.28f + std::cos(T) * 0.16f, -0.14f + std::sin(T) * 0.18f);
	}
	static const std::array<vec2, 3> s_aNose = {vec2(0.0f, 0.06f), vec2(-0.12f, 0.30f), vec2(0.12f, 0.30f)};
	static const std::array<vec2, 2> s_aMouth = {vec2(-0.26f, 0.48f), vec2(0.26f, 0.48f)};
	static const std::array<vec2, 2> s_aTooth1 = {vec2(-0.10f, 0.40f), vec2(-0.10f, 0.58f)};
	static const std::array<vec2, 2> s_aTooth2 = {vec2(0.10f, 0.40f), vec2(0.10f, 0.58f)};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, aLeftEye);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, aRightEye);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aNose);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aMouth, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aTooth1, false);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aTooth2, false);
}

static void DrawTrailCrown(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 9> s_aCrownPoints = {
		vec2(-0.94f, 0.78f),
		vec2(0.94f, 0.78f),
		vec2(0.82f, -0.28f),
		vec2(0.46f, 0.12f),
		vec2(0.25f, -0.84f),
		vec2(0.00f, -0.18f),
		vec2(-0.25f, -0.84f),
		vec2(-0.46f, 0.12f),
		vec2(-0.82f, -0.28f),
	};
	static const std::array<vec2, 2> s_aBase = {vec2(-0.70f, 0.52f), vec2(0.70f, 0.52f)};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aCrownPoints);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aBase, false);
}

static void DrawTrailFire(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	static const std::array<vec2, 12> s_aFirePoints = {
		vec2(0.00f, -1.00f),
		vec2(0.36f, -0.62f),
		vec2(0.26f, -0.28f),
		vec2(0.65f, 0.04f),
		vec2(0.56f, 0.52f),
		vec2(0.25f, 0.88f),
		vec2(0.00f, 1.00f),
		vec2(-0.38f, 0.82f),
		vec2(-0.62f, 0.42f),
		vec2(-0.54f, 0.02f),
		vec2(-0.28f, -0.30f),
		vec2(-0.34f, -0.65f),
	};
	static const std::array<vec2, 6> s_aInnerFirePoints = {
		vec2(0.05f, -0.30f),
		vec2(0.28f, 0.16f),
		vec2(0.18f, 0.58f),
		vec2(0.00f, 0.82f),
		vec2(-0.20f, 0.55f),
		vec2(-0.12f, 0.15f),
	};
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aFirePoints);
	DrawTrailShape(pGraphics, Center, Size, Color, Angle, s_aInnerFirePoints);
}

static void DrawTrailSnowflake(IGraphics *pGraphics, vec2 Center, float Size, ColorRGBA Color, float Angle)
{
	auto Rotate2D = [](const vec2 &Point, float A) {
		const float C = std::cos(A);
		const float S = std::sin(A);
		return vec2(Point.x * C - Point.y * S, Point.x * S + Point.y * C);
	};

	for(int Arm = 0; Arm < 6; Arm++)
	{
		const float A = Angle + (2.0f * pi * (float)Arm) / 6.0f;
		const vec2 End = Rotate2D(vec2(0.0f, -1.0f), A);
		const std::array<vec2, 2> aMain = {vec2(0.0f, 0.0f), End};
		DrawTrailShape(pGraphics, Center, Size, Color, 0.0f, aMain, false);

		const vec2 Base = Rotate2D(vec2(0.0f, -0.56f), A);
		const vec2 Left = Base + Rotate2D(vec2(-0.22f, -0.20f), A);
		const vec2 Right = Base + Rotate2D(vec2(0.22f, -0.20f), A);
		const std::array<vec2, 2> aLeft = {Base, Left};
		const std::array<vec2, 2> aRight = {Base, Right};
		DrawTrailShape(pGraphics, Center, Size, Color, 0.0f, aLeft, false);
		DrawTrailShape(pGraphics, Center, Size, Color, 0.0f, aRight, false);
	}
}

bool CTrails::ShouldPredictPlayer(int ClientId)
{
	if(!GameClient()->Predict())
		return false;
	CCharacter *pChar = GameClient()->m_PredictedWorld.GetCharacterById(ClientId);
	if(GameClient()->Predict() && (ClientId == GameClient()->m_Snap.m_LocalClientId || (GameClient()->AntiPingPlayers() && !GameClient()->IsOtherTeam(ClientId))) && pChar)
		return true;
	return false;
}

void CTrails::ClearAllHistory()
{
	for(int i = 0; i < MAX_CLIENTS; ++i)
		ClearHistory(i);
}
void CTrails::ClearHistory(int ClientId)
{
	for(int i = 0; i < 200; ++i)
		m_History[ClientId][i] = {{}, -1};
	m_HistoryValid[ClientId] = false;
}
void CTrails::OnReset()
{
	ClearAllHistory();
	ResetMusicReaction();
}

void CTrails::ResetMusicReaction()
{
	m_pMusicVisualizer.reset();
	m_LastMusicReactionFrame = BestClientVisualizer::SVisualizerFrame();
	m_LastMusicReactionPollTick = 0;
	m_MusicReactionLevel = 0.0f;
	m_MusicReactionKick = 0.0f;
	m_MusicReactionRollingPeak = 0.05f;
}

float CTrails::UpdateMusicReaction(float Delta)
{
	if(!g_Config.m_TcTeeTrailMusicReaction)
	{
		ResetMusicReaction();
		return 0.0f;
	}

	if(!m_pMusicVisualizer)
		m_pMusicVisualizer = std::make_unique<BestClientVisualizer::CRealtimeMusicVisualizer>();

	const int64_t Now = time_get();
	if(m_LastMusicReactionPollTick == 0 || Now - m_LastMusicReactionPollTick >= time_freq() / 15)
	{
		m_pMusicVisualizer->PollFrame(m_LastMusicReactionFrame);
		m_LastMusicReactionPollTick = Now;
	}
	const BestClientVisualizer::SVisualizerFrame &Frame = m_LastMusicReactionFrame;

	float Target = 0.0f;
	const bool HasLiveSignal = Frame.m_BackendStatus == BestClientVisualizer::EVisualizerBackendStatus::LIVE && Frame.m_HasRealtimeSignal;
	if(HasLiveSignal)
	{
		float Bass = 0.0f;
		for(int i = 0; i < 6; i++)
			Bass += Frame.m_aBands[i];
		Bass /= 6.0f;

		float BandPeak = 0.0f;
		for(float Band : Frame.m_aBands)
			BandPeak = maximum(BandPeak, Band);

		const float Raw = maximum(Frame.m_Rms * 11.0f, Frame.m_Peak * 2.2f, Bass * 0.9f + BandPeak * 0.25f);
		if(Raw > m_MusicReactionRollingPeak)
			m_MusicReactionRollingPeak = ApproachTrailValue(m_MusicReactionRollingPeak, Raw, Delta, 9.0f);
		else
			m_MusicReactionRollingPeak = ApproachTrailValue(m_MusicReactionRollingPeak, maximum(Raw, 0.025f), Delta, 1.8f);

		const float Boost = std::clamp(0.72f / maximum(m_MusicReactionRollingPeak, 0.02f), 1.0f, 16.0f);
		Target = std::clamp(Raw * Boost, 0.0f, 1.0f);
		if(Target < 0.025f)
			Target = 0.0f;
	}

	const float OldLevel = m_MusicReactionLevel;
	const float Speed = Target > m_MusicReactionLevel ? 24.0f : 7.0f;
	m_MusicReactionLevel = ApproachTrailValue(m_MusicReactionLevel, Target, Delta, Speed);
	m_MusicReactionKick = ApproachTrailValue(m_MusicReactionKick, std::clamp((m_MusicReactionLevel - OldLevel) * 8.0f, 0.0f, 1.0f), Delta, 18.0f);
	return m_MusicReactionLevel;
}

void CTrails::OnRender()
{
	if(!g_Config.m_TcTeeTrail)
	{
		ResetMusicReaction();
		return;
	}
	if(GameClient()->OptimizerDisableParticles())
	{
		ResetMusicReaction();
		return;
	}

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		ResetMusicReaction();
		return;
	}

	if(!GameClient()->m_Snap.m_pGameInfoObj)
	{
		ResetMusicReaction();
		return;
	}

	const float Delta = std::clamp(Client()->RenderFrameTime(), 0.0f, 0.1f);
	const float Lod = TrailPerformanceLodScale(Delta);
	const float MusicReaction = UpdateMusicReaction(Delta);
	const float MusicReactionStrength = std::clamp(g_Config.m_TcTeeTrailMusicReactionStrength / 100.0f, 0.0f, 3.0f);
	const float MusicPulse = MusicReaction * MusicReactionStrength;
	const float MusicKick = m_MusicReactionKick * MusicReactionStrength;
	const float MusicAlpha = 1.0f + MusicPulse * 0.35f + MusicKick * 0.28f;
	const float MusicWidth = 1.0f + MusicPulse * 0.55f + MusicKick * 0.45f;
	const float MusicSymbolPulse = 1.0f + MusicPulse * 0.22f + MusicKick * 0.48f;
	const float MusicMovementBoost = 1.0f + MusicPulse * 1.8f + MusicKick * 1.2f;

	Graphics()->TextureClear();

	float ScreenX0, ScreenY0, ScreenX1, ScreenY1;
	Graphics()->GetScreen(&ScreenX0, &ScreenY0, &ScreenX1, &ScreenY1);
	const float ScreenMinX = std::min(ScreenX0, ScreenX1);
	const float ScreenMaxX = std::max(ScreenX0, ScreenX1);
	const float ScreenMinY = std::min(ScreenY0, ScreenY1);
	const float ScreenMaxY = std::max(ScreenY0, ScreenY1);
	const float TrailScreenMargin = 256.0f + (float)std::clamp(g_Config.m_TcTeeTrailLength, 5, 200) * 8.0f;
	const auto IsTrailNearScreen = [&](vec2 Pos) {
		return Pos.x >= ScreenMinX - TrailScreenMargin && Pos.x <= ScreenMaxX + TrailScreenMargin &&
		       Pos.y >= ScreenMinY - TrailScreenMargin && Pos.y <= ScreenMaxY + TrailScreenMargin;
	};

	for(int ClientId = 0; ClientId < MAX_CLIENTS; ClientId++)
	{
		const bool Local = GameClient()->m_Snap.m_LocalClientId == ClientId;
		if(!Local && g_Config.m_MaPerformanceGuard && Lod < 0.55f)
			continue;

		const bool ZoomAllowed = GameClient()->m_Camera.ZoomAllowed();
		if(!g_Config.m_TcTeeTrailOthers && !Local)
			continue;

		if(!Local && !ZoomAllowed)
			continue;

		if(!GameClient()->m_Snap.m_aCharacters[ClientId].m_Active)
		{
			if(m_HistoryValid[ClientId])
				ClearHistory(ClientId);
			continue;
		}
		else
			m_HistoryValid[ClientId] = true;

		CTeeRenderInfo TeeInfo = GameClient()->m_aClients[ClientId].m_RenderInfo;

		const bool PredictPlayer = ShouldPredictPlayer(ClientId);
		int StartTick;
		const int GameTick = Client()->GameTick(g_Config.m_ClDummy);
		const int PredTick = Client()->PredGameTick(g_Config.m_ClDummy);
		float IntraTick;
		if(PredictPlayer)
		{
			StartTick = PredTick;
			IntraTick = Client()->PredIntraGameTick(g_Config.m_ClDummy);
			if(g_Config.m_TcRemoveAnti)
			{
				StartTick = GameClient()->m_SmoothTick;
				IntraTick = GameClient()->m_SmoothIntraTick;
			}
			if(g_Config.m_TcUnpredOthersInFreeze && !Local && Client()->m_IsLocalFrozen)
			{
				StartTick = GameTick;
			}
		}
		else
		{
			StartTick = GameTick;
			IntraTick = Client()->IntraGameTick(g_Config.m_ClDummy);
		}

		const vec2 CurServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Cur.m_Y);
		const vec2 PrevServerPos = vec2(GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_X, GameClient()->m_Snap.m_aCharacters[ClientId].m_Prev.m_Y);
		m_History[ClientId][GameTick % 200] = {
			mix(PrevServerPos, CurServerPos, IntraTick),
			GameTick,
		};

		if(!Local)
		{
			if(!IsTrailNearScreen(CurServerPos) && !IsTrailNearScreen(PrevServerPos))
				continue;
			if(GameClient()->OptimizerFpsFogEnabled() &&
				!GameClient()->OptimizerAllowRenderPos(CurServerPos) &&
				!GameClient()->OptimizerAllowRenderPos(PrevServerPos))
				continue;
		}

		// // NOTE: this is kind of a hack to fix 25tps. This fixes flickering when using the speed mode
		// m_History[ClientId][(GameTick + 1) % 200] = m_History[ClientId][GameTick % 200];
		// m_History[ClientId][(GameTick + 2) % 200] = m_History[ClientId][GameTick % 200];

		IGraphics::CLineItem LineItem;
		bool LineMode = g_Config.m_TcTeeTrailWidth == 0;

		float Alpha = g_Config.m_TcTeeTrailAlpha / 100.0f;
		// Taken from players.cpp
		if(ClientId == -2)
			Alpha *= g_Config.m_ClRaceGhostAlpha / 100.0f;
		else if(ClientId < 0 || GameClient()->IsOtherTeam(ClientId))
			Alpha *= g_Config.m_ClShowOthersAlpha / 100.0f;

		int TrailLength = std::clamp(g_Config.m_TcTeeTrailLength, 5, 200);
		if(!Local)
			TrailLength = std::clamp(round_to_int(TrailLength * (0.38f + 0.62f * Lod)), 5, TrailLength);
		else if(Lod < 0.55f)
			TrailLength = std::min(TrailLength, 120);
		if(g_Config.m_MaPerformanceGuard)
		{
			const int GuardMaxLength = Local ? std::clamp(round_to_int(90.0f * Lod), 18, 110) : std::clamp(round_to_int(45.0f * Lod), 8, 60);
			TrailLength = std::min(TrailLength, GuardMaxLength);
		}
		float Width = g_Config.m_TcTeeTrailWidth;

		static std::vector<CTrailPart> s_Trail;
		s_Trail.clear();

		// TODO: figure out why this is required
		if(!PredictPlayer)
			TrailLength += 2;
		bool TrailFull = false;
		// Fill trail list with initial positions
		for(int i = 0; i < TrailLength; i++)
		{
			CTrailPart Part;
			int PosTick = StartTick - i;
			if(PredictPlayer)
			{
				if(GameClient()->m_aClients[ClientId].m_aPredTick[PosTick % 200] != PosTick)
					continue;
				Part.m_Pos = GameClient()->m_aClients[ClientId].m_aPredPos[PosTick % 200];
				if(i == TrailLength - 1)
					TrailFull = true;
			}
			else
			{
				if(m_History[ClientId][PosTick % 200].m_Tick != PosTick)
					continue;
				Part.m_Pos = m_History[ClientId][PosTick % 200].m_Pos;
				if(i == TrailLength - 2 || i == TrailLength - 3)
					TrailFull = true;
			}
			Part.m_UnmovedPos = Part.m_Pos;
			Part.m_Tick = PosTick;
			s_Trail.push_back(Part);
		}

		// Trim the ends if intratick is too big
		// this was not trivial to figure out
		int TrimTicks = (int)IntraTick;
		for(int i = 0; i < TrimTicks; i++)
			if((int)s_Trail.size() > 0)
				s_Trail.pop_back();

		// Stuff breaks if we have less than 3 points because we cannot calculate an angle between segments to preserve constant width
		// TODO: Pad the list with generated entries in the same direction as before
		if((int)s_Trail.size() < 3)
			continue;

		if(PredictPlayer)
			s_Trail.at(0).m_Pos = GameClient()->m_aClients[ClientId].m_RenderPos;
		else
			s_Trail.at(0).m_Pos = mix(PrevServerPos, CurServerPos, IntraTick);

		if(TrailFull)
			s_Trail.at(s_Trail.size() - 1).m_Pos = mix(s_Trail.at(s_Trail.size() - 1).m_Pos, s_Trail.at(s_Trail.size() - 2).m_Pos, std::fmod(IntraTick, 1.0f));

		// Set progress
		for(int i = 0; i < (int)s_Trail.size(); i++)
		{
			float Size = float(s_Trail.size() - 1 + TrimTicks);
			CTrailPart &Part = s_Trail.at(i);
			if(i == 0)
				Part.m_Progress = 0.0f;
			else if(i == (int)s_Trail.size() - 1)
				Part.m_Progress = 1.0f;
			else
				Part.m_Progress = ((float)i + IntraTick - 1.0f) / (Size - 1.0f);

			switch(g_Config.m_TcTeeTrailColorMode)
			{
			case COLORMODE_SOLID:
				Part.m_Col = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcTeeTrailColor));
				break;
			case COLORMODE_TEE:
				if(TeeInfo.m_CustomColoredSkin)
					Part.m_Col = TeeInfo.m_ColorBody;
				else
					Part.m_Col = TeeInfo.m_BloodColor;
				break;
			case COLORMODE_RAINBOW:
			{
				float Cycle = (1.0f / TrailLength) * 0.5f;
				float Hue = std::fmod(((Part.m_Tick + 6361 * ClientId) % 1000000) * Cycle, 1.0f);
				Part.m_Col = color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.5f));
				break;
			}
			case COLORMODE_SPEED:
			{
				float Speed = 0.0f;
				if(s_Trail.size() > 3)
				{
					if(i < 2)
						Speed = distance(s_Trail.at(i + 2).m_UnmovedPos, Part.m_UnmovedPos) / std::abs(s_Trail.at(i + 2).m_Tick - Part.m_Tick);
					else
						Speed = distance(Part.m_UnmovedPos, s_Trail.at(i - 2).m_UnmovedPos) / std::abs(Part.m_Tick - s_Trail.at(i - 2).m_Tick);
				}
				Part.m_Col = color_cast<ColorRGBA>(ColorHSLA(65280 * ((int)(Speed * Speed / 12.5f) + 1)).UnclampLighting(ColorHSLA::DARKEST_LGT));
				break;
			}
			default:
				dbg_assert(false, "Invalid value for g_Config.m_TcTeeTrailColorMode");
				dbg_break();
			}

			Part.m_Col.a = std::clamp(Alpha * MusicAlpha, 0.0f, 1.0f);
			if(g_Config.m_TcTeeTrailFade)
				Part.m_Col.a *= 1.0 - Part.m_Progress;

			Part.m_Width = Width * MusicWidth;
			if(g_Config.m_TcTeeTrailTaper)
				Part.m_Width = Width * MusicWidth * (1.0 - Part.m_Progress);
		}

		// Remove duplicate elements (those with same Pos)
		auto NewEnd = std::unique(s_Trail.begin(), s_Trail.end());
		s_Trail.erase(NewEnd, s_Trail.end());

		if((int)s_Trail.size() < 3)
			continue;

		const int TrailStyle = std::clamp(g_Config.m_TcTeeTrailStyle, (int)TRAILSTYLE_DEFAULT, (int)TRAILSTYLE_SNOWFLAKES);
		if(TrailStyle != TRAILSTYLE_DEFAULT)
		{
			const int MaxSymbols = std::clamp(round_to_int((Local ? 14.0f : 7.0f) * (0.35f + 0.65f * Lod)), Local ? 5 : 2, Local ? 14 : 7);
			const int Step = std::max(2, (int)s_Trail.size() / maximum(1, MaxSymbols));
			const bool MovementEnabled = g_Config.m_TcTeeTrailMovement != 0;
			const float MovementSpeed = std::clamp(g_Config.m_TcTeeTrailMovementSpeed, 0, 500) / 100.0f;
			const float MovementAngle = MovementEnabled ? Client()->LocalTime() * MovementSpeed * MusicMovementBoost * 6.28318530718f : 0.0f;
			Graphics()->LinesBegin();
			for(int i = Step; i < (int)s_Trail.size(); i += Step)
			{
				const CTrailPart &Part = s_Trail.at(i);
				if(Part.m_Col.a <= 0.0f)
					continue;

				const float SymbolSize = std::clamp(std::max(Part.m_Width, 8.0f) * 0.75f * MusicSymbolPulse, 5.0f, 24.0f);
				const float SymbolAngle = MovementEnabled ? MovementAngle + Part.m_Progress * 1.57079632679f : 0.0f;
				switch(TrailStyle)
				{
				case TRAILSTYLE_HEARTS: DrawTrailHeart(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_STARS: DrawTrailStar(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_DIAMONDS: DrawTrailDiamond(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_MOONS: DrawTrailMoon(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_LIGHTNING: DrawTrailLightning(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_BUTTERFLIES: DrawTrailButterfly(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_FLOWERS: DrawTrailFlower(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_MUSIC: DrawTrailMusic(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_SKULLS: DrawTrailSkull(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_CROWNS: DrawTrailCrown(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_FLAMES: DrawTrailFire(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				case TRAILSTYLE_SNOWFLAKES: DrawTrailSnowflake(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				default: DrawTrailStar(Graphics(), Part.m_Pos, SymbolSize, Part.m_Col, SymbolAngle); break;
				}
			}
			Graphics()->LinesEnd();
			continue;
		}

		// Calculate the widths
		for(int i = 0; i < (int)s_Trail.size(); i++)
		{
			CTrailPart &Part = s_Trail.at(i);
			vec2 PrevPos;
			vec2 Pos = s_Trail.at(i).m_Pos;
			vec2 NextPos;

			if(i == 0)
			{
				vec2 Direction = normalize(s_Trail.at(i + 1).m_Pos - Pos);
				PrevPos = Pos - Direction;
			}
			else
				PrevPos = s_Trail.at(i - 1).m_Pos;

			if(i == (int)s_Trail.size() - 1)
			{
				vec2 Direction = normalize(Pos - s_Trail.at(i - 1).m_Pos);
				NextPos = Pos + Direction;
			}
			else
				NextPos = s_Trail.at(i + 1).m_Pos;

			vec2 NextDirection = normalize(NextPos - Pos);
			vec2 PrevDirection = normalize(Pos - PrevPos);

			vec2 Normal = vec2(-PrevDirection.y, PrevDirection.x);
			Part.m_Normal = Normal;
			vec2 Tangent = normalize(NextDirection + PrevDirection);
			if(Tangent == vec2(0.0f, 0.0f))
				Tangent = Normal;

			vec2 PerpVec = vec2(-Tangent.y, Tangent.x);
			Width = Part.m_Width;
			float ScaledWidth = Width / dot(Normal, PerpVec);
			float TopScaled = ScaledWidth;
			float BotScaled = ScaledWidth;
			if(dot(PrevDirection, Tangent) > 0.0f)
				TopScaled = std::min(Width * 3.0f, TopScaled);
			else
				BotScaled = std::min(Width * 3.0f, BotScaled);

			vec2 Top = Pos + PerpVec * TopScaled;
			vec2 Bot = Pos - PerpVec * BotScaled;
			Part.m_Top = Top;
			Part.m_Bot = Bot;

			// Bevel Cap
			if(dot(PrevDirection, NextDirection) < -0.25f)
			{
				Top = Pos + Tangent * Width;
				Bot = Pos - Tangent * Width;

				float Det = PrevDirection.x * NextDirection.y - PrevDirection.y * NextDirection.x;
				if(Det >= 0.0f)
				{
					Part.m_Top = Top;
					Part.m_Bot = Bot;
					if(i > 0)
						s_Trail.at(i).m_Flip = true;
				}
				else // <-Left Direction
				{
					Part.m_Top = Bot;
					Part.m_Bot = Top;
					if(i > 0)
						s_Trail.at(i).m_Flip = true;
				}
			}
		}

		if(LineMode)
			Graphics()->LinesBegin();
		else
			Graphics()->QuadsBegin();

		// Draw the trail
		for(int i = 0; i < (int)s_Trail.size() - 1; i++)
		{
			const CTrailPart &Part = s_Trail.at(i);
			const CTrailPart &NextPart = s_Trail.at(i + 1);
			const float Dist = distance(Part.m_UnmovedPos, NextPart.m_UnmovedPos);

			const float MaxDiff = 120.0f;
			if(i > 0)
			{
				const CTrailPart &PrevPart = s_Trail.at(i - 1);
				float PrevDist = distance(PrevPart.m_UnmovedPos, Part.m_UnmovedPos);
				if(std::abs(Dist - PrevDist) > MaxDiff)
					continue;
			}
			if(i < (int)s_Trail.size() - 2)
			{
				const CTrailPart &NextNextPart = s_Trail.at(i + 2);
				float NextDist = distance(NextPart.m_UnmovedPos, NextNextPart.m_UnmovedPos);
				if(std::abs(Dist - NextDist) > MaxDiff)
					continue;
			}

			if(LineMode)
			{
				Graphics()->SetColor(Part.m_Col);
				LineItem = IGraphics::CLineItem(Part.m_Pos.x, Part.m_Pos.y, NextPart.m_Pos.x, NextPart.m_Pos.y);
				Graphics()->LinesDraw(&LineItem, 1);
			}
			else
			{
				vec2 Top, Bot;
				if(Part.m_Flip)
				{
					Top = Part.m_Bot;
					Bot = Part.m_Top;
				}
				else
				{
					Top = Part.m_Top;
					Bot = Part.m_Bot;
				}

				Graphics()->SetColor4(NextPart.m_Col, NextPart.m_Col, Part.m_Col, Part.m_Col);
				// IGraphics::CFreeformItem FreeformItem(Top, Bot, NextPart.m_Top, NextPart.m_Bot);
				IGraphics::CFreeformItem FreeformItem(NextPart.m_Top, NextPart.m_Bot, Top, Bot);

				Graphics()->QuadsDrawFreeform(&FreeformItem, 1);
			}
		}
		if(LineMode)
			Graphics()->LinesEnd();
		else
			Graphics()->QuadsEnd();
	}
}
