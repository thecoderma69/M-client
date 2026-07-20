#include "particles3d.h"

#include "visualizer/service.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>

#include <game/client/gameclient.h>

#include <algorithm>
#include <array>
#include <cmath>

static constexpr float PROJ_DIST = 600.0f;
static constexpr int PARTICLE_MAX = 200;
static constexpr float MAX_DELTA = 0.1f;

static constexpr int SHAPE_CUBE = 1;
static constexpr int SHAPE_HEART = 2;
static constexpr int SHAPE_STAR = 3;
static constexpr int SHAPE_DIAMOND = 4;
static constexpr int SHAPE_MOON = 5;
static constexpr int SHAPE_LIGHTNING = 6;
static constexpr int SHAPE_BUTTERFLY = 7;
static constexpr int SHAPE_FLOWER = 8;
static constexpr int SHAPE_MUSIC = 9;
static constexpr int SHAPE_SKULL = 10;
static constexpr int SHAPE_CROWN = 11;
static constexpr int SHAPE_FIRE = 12;
static constexpr int SHAPE_SNOWFLAKE = 13;
static constexpr int SHAPE_LAST = SHAPE_SNOWFLAKE;

struct SRotation
{
	float m_Cx;
	float m_Sx;
	float m_Cy;
	float m_Sy;
	float m_Cz;
	float m_Sz;
};

static SRotation MakeRotation(const vec3 &Rot)
{
	return SRotation{
		std::cos(Rot.x), std::sin(Rot.x),
		std::cos(Rot.y), std::sin(Rot.y),
		std::cos(Rot.z), std::sin(Rot.z)};
}

static float ApproachValue(float Current, float Target, float Delta, float Speed)
{
	const float Amount = std::clamp(Delta * Speed, 0.0f, 1.0f);
	return mix(Current, Target, Amount);
}

static const std::array<vec3, 8> g_aCubeVertices = { {
	vec3(-1.0f, -1.0f, -1.0f),
	vec3(1.0f, -1.0f, -1.0f),
	vec3(1.0f, 1.0f, -1.0f),
	vec3(-1.0f, 1.0f, -1.0f),
	vec3(-1.0f, -1.0f, 1.0f),
	vec3(1.0f, -1.0f, 1.0f),
	vec3(1.0f, 1.0f, 1.0f),
	vec3(-1.0f, 1.0f, 1.0f),
} };

static const std::array<std::array<int, 2>, 12> g_aCubeEdges = { {
	{ {0, 1} },
	{ {1, 2} },
	{ {2, 3} },
	{ {3, 0} },
	{ {4, 5} },
	{ {5, 6} },
	{ {6, 7} },
	{ {7, 4} },
	{ {0, 4} },
	{ {1, 5} },
	{ {2, 6} },
	{ {3, 7} },
} };

static constexpr int HEART_POINTS = 96;
static constexpr int HEART_LOW_POINTS = 14;
static constexpr int SHAPE_LAYERS = 2;
static constexpr int MAX_SHAPE_POINTS = 64;
static constexpr float HEART_THICKNESS = 0.35f;
static constexpr float STAR_THICKNESS = 0.32f;

static constexpr int STAR_POINTS = 10;
static const std::array<vec3, STAR_POINTS> g_aStarVertices = { {
	vec3(0.0f, -1.0f, 0.0f),
	vec3(0.22f, -0.31f, 0.0f),
	vec3(0.95f, -0.31f, 0.0f),
	vec3(0.36f, 0.12f, 0.0f),
	vec3(0.59f, 0.81f, 0.0f),
	vec3(0.0f, 0.38f, 0.0f),
	vec3(-0.59f, 0.81f, 0.0f),
	vec3(-0.36f, 0.12f, 0.0f),
	vec3(-0.95f, -0.31f, 0.0f),
	vec3(-0.22f, -0.31f, 0.0f),
} };

static constexpr int MOON_POINTS = 15;
static const std::array<vec3, MOON_POINTS> g_aMoonVertices = { {
	vec3(0.42f, -0.96f, 0.0f),
	vec3(0.05f, -0.92f, 0.0f),
	vec3(-0.30f, -0.72f, 0.0f),
	vec3(-0.56f, -0.40f, 0.0f),
	vec3(-0.68f, 0.00f, 0.0f),
	vec3(-0.56f, 0.40f, 0.0f),
	vec3(-0.30f, 0.72f, 0.0f),
	vec3(0.05f, 0.92f, 0.0f),
	vec3(0.42f, 0.96f, 0.0f),
	vec3(0.24f, 0.66f, 0.0f),
	vec3(0.18f, 0.34f, 0.0f),
	vec3(0.22f, 0.02f, 0.0f),
	vec3(0.36f, -0.30f, 0.0f),
	vec3(0.56f, -0.58f, 0.0f),
	vec3(0.74f, -0.78f, 0.0f),
} };

static constexpr int LIGHTNING_POINTS = 7;
static const std::array<vec3, LIGHTNING_POINTS> g_aLightningVertices = { {
	vec3(0.06f, -1.00f, 0.0f),
	vec3(0.62f, -0.18f, 0.0f),
	vec3(0.24f, -0.18f, 0.0f),
	vec3(0.54f, 1.00f, 0.0f),
	vec3(-0.62f, 0.08f, 0.0f),
	vec3(-0.18f, 0.08f, 0.0f),
	vec3(-0.42f, -1.00f, 0.0f),
} };

static constexpr int BUTTERFLY_POINTS = 14;
static const std::array<vec3, BUTTERFLY_POINTS> g_aButterflyVertices = { {
	vec3(0.00f, -0.84f, 0.0f),
	vec3(-0.20f, -0.48f, 0.0f),
	vec3(-0.88f, -0.78f, 0.0f),
	vec3(-0.74f, -0.04f, 0.0f),
	vec3(-0.94f, 0.58f, 0.0f),
	vec3(-0.24f, 0.36f, 0.0f),
	vec3(0.00f, 0.90f, 0.0f),
	vec3(0.24f, 0.36f, 0.0f),
	vec3(0.94f, 0.58f, 0.0f),
	vec3(0.74f, -0.04f, 0.0f),
	vec3(0.88f, -0.78f, 0.0f),
	vec3(0.20f, -0.48f, 0.0f),
	vec3(0.00f, -0.84f, 0.0f),
	vec3(0.00f, 0.90f, 0.0f),
} };

static constexpr int FLOWER_POINTS = 32;

static constexpr int CROWN_POINTS = 9;
static const std::array<vec3, CROWN_POINTS> g_aCrownVertices = { {
	vec3(-0.94f, 0.78f, 0.0f),
	vec3(0.94f, 0.78f, 0.0f),
	vec3(0.82f, -0.28f, 0.0f),
	vec3(0.46f, 0.12f, 0.0f),
	vec3(0.25f, -0.84f, 0.0f),
	vec3(0.00f, -0.18f, 0.0f),
	vec3(-0.25f, -0.84f, 0.0f),
	vec3(-0.46f, 0.12f, 0.0f),
	vec3(-0.82f, -0.28f, 0.0f),
} };

static constexpr int FIRE_POINTS = 12;
static const std::array<vec3, FIRE_POINTS> g_aFireVertices = { {
	vec3(0.00f, -1.00f, 0.0f),
	vec3(0.36f, -0.62f, 0.0f),
	vec3(0.26f, -0.28f, 0.0f),
	vec3(0.65f, 0.04f, 0.0f),
	vec3(0.56f, 0.52f, 0.0f),
	vec3(0.25f, 0.88f, 0.0f),
	vec3(0.00f, 1.00f, 0.0f),
	vec3(-0.38f, 0.82f, 0.0f),
	vec3(-0.62f, 0.42f, 0.0f),
	vec3(-0.54f, 0.02f, 0.0f),
	vec3(-0.28f, -0.30f, 0.0f),
	vec3(-0.34f, -0.65f, 0.0f),
} };

static constexpr int FIRE_INNER_POINTS = 6;
static const std::array<vec3, FIRE_INNER_POINTS> g_aFireInnerVertices = { {
	vec3(0.05f, -0.30f, 0.0f),
	vec3(0.28f, 0.16f, 0.0f),
	vec3(0.18f, 0.58f, 0.0f),
	vec3(0.00f, 0.82f, 0.0f),
	vec3(-0.20f, 0.55f, 0.0f),
	vec3(-0.12f, 0.15f, 0.0f),
} };

static constexpr int SKULL_POINTS = 18;
static const std::array<vec3, SKULL_POINTS> g_aSkullVertices = { {
	vec3(0.00f, -0.96f, 0.0f),
	vec3(0.48f, -0.82f, 0.0f),
	vec3(0.78f, -0.48f, 0.0f),
	vec3(0.82f, -0.04f, 0.0f),
	vec3(0.66f, 0.34f, 0.0f),
	vec3(0.36f, 0.52f, 0.0f),
	vec3(0.34f, 0.88f, 0.0f),
	vec3(0.12f, 0.88f, 0.0f),
	vec3(0.12f, 0.62f, 0.0f),
	vec3(-0.12f, 0.62f, 0.0f),
	vec3(-0.12f, 0.88f, 0.0f),
	vec3(-0.34f, 0.88f, 0.0f),
	vec3(-0.36f, 0.52f, 0.0f),
	vec3(-0.66f, 0.34f, 0.0f),
	vec3(-0.82f, -0.04f, 0.0f),
	vec3(-0.78f, -0.48f, 0.0f),
	vec3(-0.48f, -0.82f, 0.0f),
	vec3(0.00f, -0.96f, 0.0f),
} };

static const std::array<vec3, HEART_POINTS> &HeartVertices()
{
	static std::array<vec3, HEART_POINTS> s_aVerts;
	static bool s_Initialized = false;
	if(!s_Initialized)
	{
		for(int i = 0; i < HEART_POINTS; i++)
		{
			const float T = 2.0f * pi * (float)i / (float)HEART_POINTS;
			const float X = 16.0f * std::pow(std::sin(T), 3.0f);
			const float Y = 13.0f * std::cos(T) - 5.0f * std::cos(2.0f * T) - 2.0f * std::cos(3.0f * T) - std::cos(4.0f * T);
			s_aVerts[i] = vec3(X, -Y, 0.0f);
		}
		s_Initialized = true;
	}
	return s_aVerts;
}

static const std::array<vec3, HEART_LOW_POINTS> &HeartLowVertices()
{
	static std::array<vec3, HEART_LOW_POINTS> s_aVerts;
	static bool s_Initialized = false;
	if(!s_Initialized)
	{
		const auto &HighRes = HeartVertices();
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			const int Src = std::clamp((i * HEART_POINTS) / HEART_LOW_POINTS, 0, HEART_POINTS - 1);
			s_aVerts[i] = HighRes[Src];
		}
		s_Initialized = true;
	}
	return s_aVerts;
}

static const std::array<vec3, FLOWER_POINTS> &FlowerVertices()
{
	static std::array<vec3, FLOWER_POINTS> s_aVerts;
	static bool s_Initialized = false;
	if(!s_Initialized)
	{
		for(int i = 0; i < FLOWER_POINTS; i++)
		{
			const float T = -0.5f * pi + 2.0f * pi * (float)i / (float)FLOWER_POINTS;
			const float R = 0.64f + 0.28f * std::sin(5.0f * T);
			s_aVerts[i] = vec3(std::cos(T) * R, std::sin(T) * R, 0.0f);
		}
		s_Initialized = true;
	}
	return s_aVerts;
}

static vec3 RotateWith(const vec3 &V, const SRotation &Rot)
{
	vec3 R = vec3(V.x * Rot.m_Cz - V.y * Rot.m_Sz, V.x * Rot.m_Sz + V.y * Rot.m_Cz, V.z);
	R = vec3(R.x, R.y * Rot.m_Cx - R.z * Rot.m_Sx, R.y * Rot.m_Sx + R.z * Rot.m_Cx);
	R = vec3(R.x * Rot.m_Cy + R.z * Rot.m_Sy, R.y, -R.x * Rot.m_Sy + R.z * Rot.m_Cy);
	return R;
}

CMa3DParticles::~CMa3DParticles() = default;

vec3 CMa3DParticles::RotateVec3(const vec3 &V, float Cx, float Sx, float Cy, float Sy, float Cz, float Sz)
{
	vec3 R = vec3(V.x * Cz - V.y * Sz, V.x * Sz + V.y * Cz, V.z);
	R = vec3(R.x, R.y * Cx - R.z * Sx, R.y * Sx + R.z * Cx);
	R = vec3(R.x * Cy + R.z * Sy, R.y, -R.x * Sy + R.z * Cy);
	return R;
}

vec2 CMa3DParticles::ProjectPoint(const vec3 &Pos, const vec2 &Center)
{
	float Scale = std::clamp(PROJ_DIST / (PROJ_DIST + Pos.z), 0.5f, 1.6f);
	vec2 Rel = vec2(Pos.x - Center.x, Pos.y - Center.y);
	return Center + Rel * Scale;
}

void CMa3DParticles::OnInit()
{
	m_HasConfigSnapshot = false;
	ResetParticles();
}

void CMa3DParticles::OnReset()
{
	m_HasConfigSnapshot = false;
	ResetParticles();
}

void CMa3DParticles::ResetParticles()
{
	m_vParticles.clear();
	m_Time = 0.0f;
	m_HasLastSpawnBounds = false;
	m_HasLastScreenSize = false;
	m_LastSpawnMin = vec2(0,0);
	m_LastSpawnMax = vec2(0,0);
	m_LastScreenW = 0;
	m_LastScreenH = 0;
}

void CMa3DParticles::ResetMusicReaction()
{
	m_pMusicVisualizer.reset();
	m_LastMusicReactionFrame = BestClientVisualizer::SVisualizerFrame();
	m_LastMusicReactionPollTick = 0;
	m_MusicReactionLevel = 0.0f;
	m_MusicReactionKick = 0.0f;
	m_MusicReactionRollingPeak = 0.05f;
}

float CMa3DParticles::UpdateMusicReaction(float Delta)
{
	if(!g_Config.m_Ma3dParticlesMusicReaction)
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
			m_MusicReactionRollingPeak = ApproachValue(m_MusicReactionRollingPeak, Raw, Delta, 9.0f);
		else
			m_MusicReactionRollingPeak = ApproachValue(m_MusicReactionRollingPeak, maximum(Raw, 0.025f), Delta, 1.8f);

		const float Boost = std::clamp(0.72f / maximum(m_MusicReactionRollingPeak, 0.02f), 1.0f, 16.0f);
		Target = std::clamp(Raw * Boost, 0.0f, 1.0f);
		if(Target < 0.025f)
			Target = 0.0f;
	}

	const float OldLevel = m_MusicReactionLevel;
	const float Speed = Target > m_MusicReactionLevel ? 24.0f : 7.0f;
	m_MusicReactionLevel = ApproachValue(m_MusicReactionLevel, Target, Delta, Speed);
	m_MusicReactionKick = ApproachValue(m_MusicReactionKick, std::clamp((m_MusicReactionLevel - OldLevel) * 8.0f, 0.0f, 1.0f), Delta, 18.0f);
	return m_MusicReactionLevel;
}

bool CMa3DParticles::ShouldRender() const
{
	if(!g_Config.m_Ma3dParticles)
		return false;
	if(GameClient()->OptimizerDisableParticles())
		return false;
	return true;
}

void CMa3DParticles::OnRender()
{
	if(!ShouldRender())
	{
		if(!m_vParticles.empty())
			ResetParticles();
		ResetMusicReaction();
		return;
	}

	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		ResetParticles();
		ResetMusicReaction();
		return;
	}

	float Delta = std::clamp(Client()->RenderFrameTime(), 0.0f, MAX_DELTA);
	if(Delta <= 0.0f)
		return;
	m_Time += Delta;

	vec2 LocalPos = GameClient()->m_Camera.m_Center;

	float Depth = (float)g_Config.m_Ma3dParticlesDepth;
	float FadeIn = g_Config.m_Ma3dParticlesFadeInMs / 1000.0f;
	float FadeOut = g_Config.m_Ma3dParticlesFadeOutMs / 1000.0f;
	float BaseAlpha = g_Config.m_Ma3dParticlesAlpha / 100.0f;

	float SX0, SY0, SX1, SY1;
	Graphics()->GetScreen(&SX0, &SY0, &SX1, &SY1);

	float VMinX = SX0, VMaxX = SX1, VMinY = SY0, VMaxY = SY1;
	float SpawnMinX = SX0, SpawnMaxX = SX1, SpawnMinY = SY0, SpawnMaxY = SY1;

	int TargetCount = std::clamp(g_Config.m_Ma3dParticlesCount, 0, PARTICLE_MAX);
	if((int)m_vParticles.size() > TargetCount)
		m_vParticles.resize(TargetCount);

	float RotationSpeed = std::clamp(g_Config.m_Ma3dParticlesMovementSpeed, 0, 2000) / 100.0f;
	float PushRadius = (float)g_Config.m_Ma3dParticlesPushRadius;
	float PushStrength = (float)g_Config.m_Ma3dParticlesPushStrength;
	const float MusicReaction = UpdateMusicReaction(Delta);
	const float MusicReactionStrength = std::clamp(g_Config.m_Ma3dParticlesMusicReactionStrength / 100.0f, 0.0f, 3.0f);
	const float MusicSpinBoost = 1.0f + MusicReaction * MusicReactionStrength * 2.2f;
	const float MusicPush = (MusicReaction * 0.35f + m_MusicReactionKick * 0.65f) * MusicReactionStrength;

	for(auto &P : m_vParticles)
	{
		P.m_Pos += P.m_Vel * Delta;
		P.m_Rot += P.m_RotVel * Delta * RotationSpeed * MusicSpinBoost;

		if(MusicPush > 0.0f)
		{
			vec3 Diff = P.m_Pos - vec3(LocalPos.x, LocalPos.y, 0);
			const float Dist = maximum(length(Diff), 0.001f);
			P.m_Vel += (Diff / Dist) * (140.0f * MusicPush) * Delta;
		}

		if(PushStrength > 0 && PushRadius > 0)
		{
			vec3 Diff = P.m_Pos - vec3(LocalPos.x, LocalPos.y, 0);
			float DistSq = dot(Diff, Diff);
			if(DistSq > 0.0001f && DistSq < PushRadius * PushRadius)
			{
				float Dist = sqrtf(DistSq);
				vec3 Dir = Diff / Dist;
				P.m_Vel += Dir * (PushStrength * (1.0f - Dist / PushRadius)) * Delta;
			}
		}

		float Speed = length(P.m_Vel);
		if(Speed > 500.0f)
			P.m_Vel = P.m_Vel / Speed * 500.0f;
		P.m_Vel *= 0.995f;
		P.m_Pos.z = std::clamp(P.m_Pos.z, -Depth, Depth);

		if((P.m_Pos.x < VMinX || P.m_Pos.x > VMaxX || P.m_Pos.y < VMinY || P.m_Pos.y > VMaxY) && !P.m_FadingOut)
		{
			P.m_FadingOut = true;
			P.m_FadeOutStart = m_Time;
		}
	}

	for(size_t i = 0; i < m_vParticles.size();)
	{
		if(m_vParticles[i].m_FadingOut)
		{
			float T = FadeOut > 0 ? (m_Time - m_vParticles[i].m_FadeOutStart) / FadeOut : 1.0f;
			if(T >= 1.0f)
			{
				m_vParticles[i] = m_vParticles.back();
				m_vParticles.pop_back();
				continue;
			}
		}
		i++;
	}

	int ConfigType = std::clamp(g_Config.m_Ma3dParticlesType, SHAPE_CUBE, SHAPE_LAST);
	if(!m_HasConfigSnapshot || m_LastType != ConfigType)
	{
		ResetParticles();
		m_HasConfigSnapshot = true;
		m_LastType = ConfigType;
	}

	int Missing = TargetCount - (int)m_vParticles.size();
	int SpawnNow = std::min(Missing, 8);
	float SizeMin = (float)g_Config.m_Ma3dParticlesSizeMin;
	float SizeMax = (float)g_Config.m_Ma3dParticlesSizeMax;
	float Speed = (float)g_Config.m_Ma3dParticlesSpeed;

	for(int i = 0; i < SpawnNow; i++)
	{
		SParticle P;
		P.m_Type = ConfigType;
		P.m_Size = random_float(SizeMin, SizeMax);

		ColorRGBA BaseColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_Ma3dParticlesColor));
		if(g_Config.m_Ma3dParticlesColorMode == 2)
			BaseColor = color_cast<ColorRGBA>(ColorHSLA(random_float(), 0.85f, 0.65f, BaseColor.a));
		P.m_Color = BaseColor;

		P.m_Pos = vec3(random_float(SpawnMinX, SpawnMaxX), random_float(SpawnMinY, SpawnMaxY), random_float(-Depth, Depth));
		vec3 Dir(random_float(-1,1), random_float(-1,1), 0);
		float Dlen = length(Dir);
		P.m_Vel = (Dlen > 0.001f ? Dir / Dlen : vec3(1,0,0)) * Speed;
		P.m_Rot = vec3(random_float(-0.35f,0.35f), random_float(-0.35f,0.35f), random_float(0, pi*2));
		P.m_RotVel = vec3(random_float(-0.08f,0.08f), random_float(-0.08f,0.08f), random_float(-0.2f,0.2f));
		const vec3 SpawnDir = normalize(vec3(random_float(-1.0f, 1.0f), random_float(-1.0f, 1.0f), random_float(-0.5f, 0.5f)) + vec3(0.001f, 0.0f, 0.0f));
		const vec3 FadeDir = normalize(vec3(random_float(-1.0f, 1.0f), random_float(-1.0f, 1.0f), random_float(-0.5f, 0.5f)) + vec3(0.001f, 0.0f, 0.0f));
		P.m_SpawnOffset = SpawnDir * (P.m_Size * random_float(0.35f, 0.85f));
		P.m_FadeOutOffset = FadeDir * (P.m_Size * random_float(0.55f, 1.1f));
		P.m_SpawnTime = m_Time;
		P.m_FadeOutStart = 0;
		P.m_FadingOut = false;

		m_vParticles.push_back(P);
	}

	m_LastSpawnMin = vec2(SpawnMinX, SpawnMinY);
	m_LastSpawnMax = vec2(SpawnMaxX, SpawnMaxY);
	m_HasLastSpawnBounds = true;
	m_LastScreenW = SX1 - SX0;
	m_LastScreenH = SY1 - SY0;
	m_HasLastScreenSize = true;

	RenderParticles(VMinX, VMaxX, VMinY, VMaxY, BaseAlpha, FadeIn, FadeOut);
}

void CMa3DParticles::RenderParticles(float VMinX, float VMaxX, float VMinY, float VMaxY, float BaseAlpha, float FadeIn, float FadeOut)
{
	if(m_vParticles.empty())
		return;

	Graphics()->TextureClear();

	const bool GlowEnabled = g_Config.m_Ma3dParticlesGlow != 0;
	const float GlowAlpha = std::clamp(g_Config.m_Ma3dParticlesGlowAlpha / 100.0f, 0.0f, 1.0f);
	const float GlowOffset = (float)g_Config.m_Ma3dParticlesGlowOffset;
	const vec3 GlowOffsetVec(-GlowOffset, -GlowOffset, 0.0f);
	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float MusicReactionStrength = std::clamp(g_Config.m_Ma3dParticlesMusicReactionStrength / 100.0f, 0.0f, 3.0f);
	const float MusicScale = 1.0f + m_MusicReactionLevel * MusicReactionStrength * 0.42f;
	const float MusicAlpha = 1.0f + m_MusicReactionLevel * MusicReactionStrength * 0.28f;
	const float MusicKickOffset = m_MusicReactionKick * MusicReactionStrength;

	auto GetRenderParams = [&](const SParticle &Part, float AlphaMul, const vec3 &ExtraOffset, vec3 &OutPos, float &OutSize, float &OutAlpha) {
		if(Part.m_Pos.x < VMinX || Part.m_Pos.x > VMaxX || Part.m_Pos.y < VMinY || Part.m_Pos.y > VMaxY)
			return false;

		const float InT = FadeIn > 0.0f ? std::clamp((m_Time - Part.m_SpawnTime) / FadeIn, 0.0f, 1.0f) : 1.0f;
		const float OutT = Part.m_FadingOut ? (FadeOut > 0.0f ? std::clamp((m_Time - Part.m_FadeOutStart) / FadeOut, 0.0f, 1.0f) : 1.0f) : 0.0f;
		const float Out = Part.m_FadingOut ? (1.0f - OutT) : 1.0f;
		const float Alpha = std::clamp(BaseAlpha * InT * Out * AlphaMul * MusicAlpha, 0.0f, 1.0f);
		if(Alpha <= 0.0f)
			return false;

		const float InEase = InT * InT * (3.0f - 2.0f * InT);
		const float OutEase = OutT * OutT * (3.0f - 2.0f * OutT);

		float Scale = 1.0f;
		vec3 Offset(0.0f, 0.0f, 0.0f);

		const float Pop = 1.0f + 0.2f * std::sin(InEase * pi);
		Scale *= mix(0.55f, 1.0f, InEase) * Pop * MusicScale;
		Offset += Part.m_SpawnOffset * (1.0f - InEase);

		if(MusicKickOffset > 0.0f)
		{
			vec3 Diff = Part.m_Pos - vec3(Center.x, Center.y, 0.0f);
			const float Dist = maximum(length(Diff), 0.001f);
			Offset += (Diff / Dist) * (Part.m_Size * 1.6f * MusicKickOffset);
		}

		if(Part.m_FadingOut)
		{
			Offset += Part.m_FadeOutOffset * OutEase;
			Scale *= std::pow(Out, 1.25f);
		}

		OutPos = Part.m_Pos + Offset + ExtraOffset;
		OutSize = Part.m_Size * Scale;
		OutAlpha = Alpha;
		return OutAlpha > 0.0f && OutSize > 0.01f;
	};

	auto DrawExtrudedPolyline = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha, const vec3 *pVerts, int NumPoints, bool Closed, float ScaleMul, float Thickness, bool CenterSpokes) {
		if(NumPoints < 2 || NumPoints > MAX_SHAPE_POINTS)
			return;

		Graphics()->SetColor(ColorRGBA(Part.m_Color.r, Part.m_Color.g, Part.m_Color.b, Part.m_Color.a * FinalAlpha));

		const SRotation Rot = MakeRotation(Part.m_Rot);
		const float Scale = RenderSize * ScaleMul;
		const float LayerStep = SHAPE_LAYERS > 1 ? 2.0f / (float)(SHAPE_LAYERS - 1) : 0.0f;
		std::array<std::array<vec2, MAX_SHAPE_POINTS>, SHAPE_LAYERS> aProjected;
		std::array<float, SHAPE_LAYERS> aLayerZ;
		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			const float LayerT = -1.0f + LayerStep * (float)L;
			const float Z = LayerT * (RenderSize * Thickness);
			aLayerZ[L] = Z;
			const float LayerScale = 1.0f - std::abs(LayerT) * 0.08f;
			for(int i = 0; i < NumPoints; i++)
			{
				const vec3 Local = vec3(pVerts[i].x * Scale * LayerScale, pVerts[i].y * Scale * LayerScale, pVerts[i].z * Scale + Z);
				const vec3 V = RotateWith(Local, Rot) + RenderPos;
				aProjected[L][i] = ProjectPoint(V, Center);
			}
		}

		const int RingLineCount = Closed ? NumPoints : NumPoints - 1;
		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			std::array<IGraphics::CLineItem, MAX_SHAPE_POINTS> aRingLines;
			for(int i = 0; i < RingLineCount; i++)
			{
				const int Next = (i + 1) % NumPoints;
				aRingLines[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L][Next]);
			}
			Graphics()->LinesDraw(aRingLines.data(), RingLineCount);
		}

		for(int L = 0; L < SHAPE_LAYERS - 1; L++)
		{
			std::array<IGraphics::CLineItem, MAX_SHAPE_POINTS> aVertical;
			for(int i = 0; i < NumPoints; i++)
				aVertical[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][i]);
			Graphics()->LinesDraw(aVertical.data(), NumPoints);

			std::array<IGraphics::CLineItem, MAX_SHAPE_POINTS> aDiagonal;
			for(int i = 0; i < RingLineCount; i++)
			{
				const int Next = (i + 1) % NumPoints;
				aDiagonal[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][Next]);
			}
			Graphics()->LinesDraw(aDiagonal.data(), RingLineCount);
		}

		if(CenterSpokes && Closed)
		{
			const int Front = 0;
			const int Back = SHAPE_LAYERS - 1;
			const vec2 CenterFront = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Front]), Rot) + RenderPos, Center);
			const vec2 CenterBack = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Back]), Rot) + RenderPos, Center);
			std::array<IGraphics::CLineItem, MAX_SHAPE_POINTS> aFront;
			std::array<IGraphics::CLineItem, MAX_SHAPE_POINTS> aBack;
			for(int i = 0; i < NumPoints; i++)
			{
				aFront[i] = IGraphics::CLineItem(CenterFront, aProjected[Front][i]);
				aBack[i] = IGraphics::CLineItem(CenterBack, aProjected[Back][i]);
			}
			Graphics()->LinesDraw(aFront.data(), NumPoints);
			Graphics()->LinesDraw(aBack.data(), NumPoints);
		}
	};

	auto DrawCube = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		Graphics()->SetColor(ColorRGBA(Part.m_Color.r, Part.m_Color.g, Part.m_Color.b, Part.m_Color.a * FinalAlpha));

		const SRotation Rot = MakeRotation(Part.m_Rot);
		std::array<vec2, g_aCubeVertices.size()> aProjected;
		for(size_t i = 0; i < g_aCubeVertices.size(); i++)
		{
			const vec3 Local = g_aCubeVertices[i] * RenderSize;
			const vec3 V = RotateWith(Local, Rot) + RenderPos;
			aProjected[i] = ProjectPoint(V, Center);
		}

		std::array<IGraphics::CLineItem, g_aCubeEdges.size()> aLines;
		for(size_t i = 0; i < g_aCubeEdges.size(); i++)
		{
			const auto &Edge = g_aCubeEdges[i];
			aLines[i] = IGraphics::CLineItem(aProjected[Edge[0]], aProjected[Edge[1]]);
		}
		Graphics()->LinesDraw(aLines.data(), aLines.size());
	};

	auto DrawHeart = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		Graphics()->SetColor(ColorRGBA(Part.m_Color.r, Part.m_Color.g, Part.m_Color.b, Part.m_Color.a * FinalAlpha));

		const SRotation Rot = MakeRotation(Part.m_Rot);
		const auto &Verts = HeartLowVertices();
		const float Scale = RenderSize * 0.055f;
		const float LayerStep = SHAPE_LAYERS > 1 ? 2.0f / (float)(SHAPE_LAYERS - 1) : 0.0f;
		std::array<std::array<vec2, HEART_LOW_POINTS>, SHAPE_LAYERS> aProjected;
		std::array<float, SHAPE_LAYERS> aLayerZ;
		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			const float LayerT = -1.0f + LayerStep * (float)L;
			const float Z = LayerT * (RenderSize * HEART_THICKNESS);
			aLayerZ[L] = Z;
			const float LayerScale = 1.0f - std::abs(LayerT) * 0.08f;
			for(int i = 0; i < HEART_LOW_POINTS; i++)
			{
				const vec3 Local = vec3(Verts[i].x * Scale * LayerScale, Verts[i].y * Scale * LayerScale, Z);
				const vec3 V = RotateWith(Local, Rot) + RenderPos;
				aProjected[L][i] = ProjectPoint(V, Center);
			}
		}

		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aRingLines;
			for(int i = 0; i < HEART_LOW_POINTS; i++)
			{
				const int Next = (i + 1) % HEART_LOW_POINTS;
				aRingLines[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L][Next]);
			}
			Graphics()->LinesDraw(aRingLines.data(), aRingLines.size());
		}

		for(int L = 0; L < SHAPE_LAYERS - 1; L++)
		{
			std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aVertical;
			std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aDiagonal;
			for(int i = 0; i < HEART_LOW_POINTS; i++)
			{
				const int Next = (i + 1) % HEART_LOW_POINTS;
				aVertical[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][i]);
				aDiagonal[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][Next]);
			}
			Graphics()->LinesDraw(aVertical.data(), aVertical.size());
			Graphics()->LinesDraw(aDiagonal.data(), aDiagonal.size());
		}

		const int Front = 0;
		const int Back = SHAPE_LAYERS - 1;
		const vec2 CenterFront = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Front]), Rot) + RenderPos, Center);
		const vec2 CenterBack = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Back]), Rot) + RenderPos, Center);
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aFront;
		std::array<IGraphics::CLineItem, HEART_LOW_POINTS> aBack;
		for(int i = 0; i < HEART_LOW_POINTS; i++)
		{
			aFront[i] = IGraphics::CLineItem(CenterFront, aProjected[Front][i]);
			aBack[i] = IGraphics::CLineItem(CenterBack, aProjected[Back][i]);
		}
		Graphics()->LinesDraw(aFront.data(), aFront.size());
		Graphics()->LinesDraw(aBack.data(), aBack.size());
	};

	auto DrawStar = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		Graphics()->SetColor(ColorRGBA(Part.m_Color.r, Part.m_Color.g, Part.m_Color.b, Part.m_Color.a * FinalAlpha));

		const SRotation Rot = MakeRotation(Part.m_Rot);
		const float Scale = RenderSize * 0.98f;
		const float LayerStep = SHAPE_LAYERS > 1 ? 2.0f / (float)(SHAPE_LAYERS - 1) : 0.0f;
		std::array<std::array<vec2, STAR_POINTS>, SHAPE_LAYERS> aProjected;
		std::array<float, SHAPE_LAYERS> aLayerZ;
		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			const float LayerT = -1.0f + LayerStep * (float)L;
			const float Z = LayerT * (RenderSize * STAR_THICKNESS);
			aLayerZ[L] = Z;
			const float LayerScale = 1.0f - std::abs(LayerT) * 0.08f;
			for(int i = 0; i < STAR_POINTS; i++)
			{
				const vec3 Local = vec3(g_aStarVertices[i].x * Scale * LayerScale, g_aStarVertices[i].y * Scale * LayerScale, Z);
				const vec3 V = RotateWith(Local, Rot) + RenderPos;
				aProjected[L][i] = ProjectPoint(V, Center);
			}
		}

		for(int L = 0; L < SHAPE_LAYERS; L++)
		{
			std::array<IGraphics::CLineItem, STAR_POINTS> aRingLines;
			for(int i = 0; i < STAR_POINTS; i++)
			{
				const int Next = (i + 1) % STAR_POINTS;
				aRingLines[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L][Next]);
			}
			Graphics()->LinesDraw(aRingLines.data(), aRingLines.size());
		}

		for(int L = 0; L < SHAPE_LAYERS - 1; L++)
		{
			std::array<IGraphics::CLineItem, STAR_POINTS> aVertical;
			std::array<IGraphics::CLineItem, STAR_POINTS> aDiagonal;
			for(int i = 0; i < STAR_POINTS; i++)
			{
				const int Next = (i + 1) % STAR_POINTS;
				aVertical[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][i]);
				aDiagonal[i] = IGraphics::CLineItem(aProjected[L][i], aProjected[L + 1][Next]);
			}
			Graphics()->LinesDraw(aVertical.data(), aVertical.size());
			Graphics()->LinesDraw(aDiagonal.data(), aDiagonal.size());
		}

		const int Front = 0;
		const int Back = SHAPE_LAYERS - 1;
		const vec2 CenterFront = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Front]), Rot) + RenderPos, Center);
		const vec2 CenterBack = ProjectPoint(RotateWith(vec3(0.0f, 0.0f, aLayerZ[Back]), Rot) + RenderPos, Center);
		std::array<IGraphics::CLineItem, STAR_POINTS> aFront;
		std::array<IGraphics::CLineItem, STAR_POINTS> aBack;
		for(int i = 0; i < STAR_POINTS; i++)
		{
			aFront[i] = IGraphics::CLineItem(CenterFront, aProjected[Front][i]);
			aBack[i] = IGraphics::CLineItem(CenterBack, aProjected[Back][i]);
		}
		Graphics()->LinesDraw(aFront.data(), aFront.size());
		Graphics()->LinesDraw(aBack.data(), aBack.size());
	};

	auto DrawDiamond = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		Graphics()->SetColor(ColorRGBA(Part.m_Color.r, Part.m_Color.g, Part.m_Color.b, Part.m_Color.a * FinalAlpha));

		const SRotation Rot = MakeRotation(Part.m_Rot);
		const std::array<vec3, 6> aVerts = { {
			vec3(0.0f, -1.05f, 0.0f),
			vec3(0.78f, 0.0f, 0.0f),
			vec3(0.0f, 1.05f, 0.0f),
			vec3(-0.78f, 0.0f, 0.0f),
			vec3(0.0f, 0.0f, 0.72f),
			vec3(0.0f, 0.0f, -0.72f),
		} };
		const std::array<std::array<int, 2>, 16> aEdges = { {
			{ {0, 1} }, { {1, 2} }, { {2, 3} }, { {3, 0} },
			{ {0, 4} }, { {1, 4} }, { {2, 4} }, { {3, 4} },
			{ {0, 5} }, { {1, 5} }, { {2, 5} }, { {3, 5} },
			{ {4, 1} }, { {4, 3} }, { {5, 0} }, { {5, 2} },
		} };

		std::array<vec2, aVerts.size()> aProjected;
		for(size_t i = 0; i < aVerts.size(); i++)
			aProjected[i] = ProjectPoint(RotateWith(aVerts[i] * RenderSize, Rot) + RenderPos, Center);

		std::array<IGraphics::CLineItem, aEdges.size()> aLines;
		for(size_t i = 0; i < aEdges.size(); i++)
			aLines[i] = IGraphics::CLineItem(aProjected[aEdges[i][0]], aProjected[aEdges[i][1]]);
		Graphics()->LinesDraw(aLines.data(), aLines.size());
	};

	auto DrawMoon = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aMoonVertices.data(), MOON_POINTS, true, 0.92f, 0.24f, false);
	};

	auto DrawLightning = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aLightningVertices.data(), LIGHTNING_POINTS, true, 0.96f, 0.28f, true);
	};

	auto DrawButterfly = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aButterflyVertices.data(), BUTTERFLY_POINTS, false, 0.92f, 0.22f, false);
		const std::array<vec3, 3> aLeftAntenna = { {vec3(0.0f, -0.84f, 0.0f), vec3(-0.18f, -1.08f, 0.0f), vec3(-0.36f, -1.02f, 0.0f)} };
		const std::array<vec3, 3> aRightAntenna = { {vec3(0.0f, -0.84f, 0.0f), vec3(0.18f, -1.08f, 0.0f), vec3(0.36f, -1.02f, 0.0f)} };
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aLeftAntenna.data(), (int)aLeftAntenna.size(), false, 0.92f, 0.12f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aRightAntenna.data(), (int)aRightAntenna.size(), false, 0.92f, 0.12f, false);
	};

	auto DrawFlower = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		const auto &Verts = FlowerVertices();
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, Verts.data(), FLOWER_POINTS, true, 0.98f, 0.24f, true);

		constexpr int CenterPoints = 12;
		std::array<vec3, CenterPoints> aCenter;
		for(int i = 0; i < CenterPoints; i++)
		{
			const float T = 2.0f * pi * (float)i / (float)CenterPoints;
			aCenter[i] = vec3(std::cos(T) * 0.18f, std::sin(T) * 0.18f, 0.0f);
		}
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aCenter.data(), CenterPoints, true, 0.98f, 0.12f, false);
	};

	auto DrawMusic = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		constexpr int NotePoints = 14;
		std::array<vec3, NotePoints> aLeftHead;
		std::array<vec3, NotePoints> aRightHead;
		for(int i = 0; i < NotePoints; i++)
		{
			const float T = 2.0f * pi * (float)i / (float)NotePoints;
			aLeftHead[i] = vec3(-0.34f + std::cos(T) * 0.24f, 0.48f + std::sin(T) * 0.16f, 0.0f);
			aRightHead[i] = vec3(0.42f + std::cos(T) * 0.24f, 0.26f + std::sin(T) * 0.16f, 0.0f);
		}
		const std::array<vec3, 2> aLeftStem = { {vec3(-0.12f, 0.48f, 0.0f), vec3(-0.12f, -0.78f, 0.0f)} };
		const std::array<vec3, 2> aRightStem = { {vec3(0.64f, 0.26f, 0.0f), vec3(0.64f, -0.56f, 0.0f)} };
		const std::array<vec3, 2> aBeam = { {vec3(-0.12f, -0.78f, 0.0f), vec3(0.64f, -0.56f, 0.0f)} };
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aLeftHead.data(), NotePoints, true, 0.95f, 0.16f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aRightHead.data(), NotePoints, true, 0.95f, 0.16f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aLeftStem.data(), (int)aLeftStem.size(), false, 0.95f, 0.16f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aRightStem.data(), (int)aRightStem.size(), false, 0.95f, 0.16f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aBeam.data(), (int)aBeam.size(), false, 0.95f, 0.16f, false);
	};

	auto DrawSkull = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aSkullVertices.data(), SKULL_POINTS, false, 0.92f, 0.24f, false);

		constexpr int EyePoints = 10;
		std::array<vec3, EyePoints> aLeftEye;
		std::array<vec3, EyePoints> aRightEye;
		for(int i = 0; i < EyePoints; i++)
		{
			const float T = 2.0f * pi * (float)i / (float)EyePoints;
			aLeftEye[i] = vec3(-0.28f + std::cos(T) * 0.16f, -0.14f + std::sin(T) * 0.18f, 0.0f);
			aRightEye[i] = vec3(0.28f + std::cos(T) * 0.16f, -0.14f + std::sin(T) * 0.18f, 0.0f);
		}
		const std::array<vec3, 3> aNose = { {vec3(0.0f, 0.06f, 0.0f), vec3(-0.12f, 0.30f, 0.0f), vec3(0.12f, 0.30f, 0.0f)} };
		const std::array<vec3, 2> aMouth = { {vec3(-0.26f, 0.48f, 0.0f), vec3(0.26f, 0.48f, 0.0f)} };
		const std::array<vec3, 2> aTooth1 = { {vec3(-0.10f, 0.40f, 0.0f), vec3(-0.10f, 0.58f, 0.0f)} };
		const std::array<vec3, 2> aTooth2 = { {vec3(0.10f, 0.40f, 0.0f), vec3(0.10f, 0.58f, 0.0f)} };
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aLeftEye.data(), EyePoints, true, 0.92f, 0.10f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aRightEye.data(), EyePoints, true, 0.92f, 0.10f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aNose.data(), (int)aNose.size(), true, 0.92f, 0.10f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aMouth.data(), (int)aMouth.size(), false, 0.92f, 0.10f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aTooth1.data(), (int)aTooth1.size(), false, 0.92f, 0.10f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aTooth2.data(), (int)aTooth2.size(), false, 0.92f, 0.10f, false);
	};

	auto DrawCrown = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aCrownVertices.data(), CROWN_POINTS, true, 0.95f, 0.24f, false);

		const std::array<vec3, 2> aBase = { {vec3(-0.70f, 0.52f, 0.0f), vec3(0.70f, 0.52f, 0.0f)} };
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aBase.data(), (int)aBase.size(), false, 0.95f, 0.10f, false);
	};

	auto DrawFire = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aFireVertices.data(), FIRE_POINTS, true, 0.94f, 0.25f, false);
		DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, g_aFireInnerVertices.data(), FIRE_INNER_POINTS, true, 0.94f, 0.14f, false);
	};

	auto DrawSnowflake = [&](const SParticle &Part, const vec3 &RenderPos, float RenderSize, float FinalAlpha) {
		auto Rotate2D = [](const vec2 &P, float Angle) {
			const float C = std::cos(Angle);
			const float S = std::sin(Angle);
			return vec2(P.x * C - P.y * S, P.x * S + P.y * C);
		};

		for(int Arm = 0; Arm < 6; Arm++)
		{
			const float A = (2.0f * pi * (float)Arm) / 6.0f;
			const vec2 End = Rotate2D(vec2(0.0f, -1.0f), A);
			const std::array<vec3, 2> aMain = { {vec3(0.0f, 0.0f, 0.0f), vec3(End.x, End.y, 0.0f)} };
			DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aMain.data(), (int)aMain.size(), false, 0.92f, 0.08f, false);

			const vec2 BaseA = Rotate2D(vec2(0.0f, -0.56f), A);
			const vec2 Left = BaseA + Rotate2D(vec2(-0.22f, -0.20f), A);
			const vec2 Right = BaseA + Rotate2D(vec2(0.22f, -0.20f), A);
			const std::array<vec3, 2> aLeft = { {vec3(BaseA.x, BaseA.y, 0.0f), vec3(Left.x, Left.y, 0.0f)} };
			const std::array<vec3, 2> aRight = { {vec3(BaseA.x, BaseA.y, 0.0f), vec3(Right.x, Right.y, 0.0f)} };
			DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aLeft.data(), (int)aLeft.size(), false, 0.92f, 0.08f, false);
			DrawExtrudedPolyline(Part, RenderPos, RenderSize, FinalAlpha, aRight.data(), (int)aRight.size(), false, 0.92f, 0.08f, false);
		}
	};

	const int ParticleCount = (int)m_vParticles.size();

	Graphics()->LinesBegin();
	for(int ParticleIndex = 0; ParticleIndex < ParticleCount; ParticleIndex++)
	{
		const auto &Part = m_vParticles[ParticleIndex];
		vec3 RenderPos;
		float RenderSize;
		float FinalAlpha;

		auto DrawByType = [&](const vec3 &Pos, float Size, float Alpha) {
			switch(Part.m_Type)
			{
			case SHAPE_HEART: DrawHeart(Part, Pos, Size, Alpha); break;
			case SHAPE_STAR: DrawStar(Part, Pos, Size, Alpha); break;
			case SHAPE_DIAMOND: DrawDiamond(Part, Pos, Size, Alpha); break;
			case SHAPE_MOON: DrawMoon(Part, Pos, Size, Alpha); break;
			case SHAPE_LIGHTNING: DrawLightning(Part, Pos, Size, Alpha); break;
			case SHAPE_BUTTERFLY: DrawButterfly(Part, Pos, Size, Alpha); break;
			case SHAPE_FLOWER: DrawFlower(Part, Pos, Size, Alpha); break;
			case SHAPE_MUSIC: DrawMusic(Part, Pos, Size, Alpha); break;
			case SHAPE_SKULL: DrawSkull(Part, Pos, Size, Alpha); break;
			case SHAPE_CROWN: DrawCrown(Part, Pos, Size, Alpha); break;
			case SHAPE_FIRE: DrawFire(Part, Pos, Size, Alpha); break;
			case SHAPE_SNOWFLAKE: DrawSnowflake(Part, Pos, Size, Alpha); break;
			default: DrawCube(Part, Pos, Size, Alpha); break;
			}
		};

		if(GlowEnabled && GlowAlpha > 0.0f && GlowOffset > 0.0f)
		{
			if(GetRenderParams(Part, GlowAlpha, GlowOffsetVec, RenderPos, RenderSize, FinalAlpha))
				DrawByType(RenderPos, RenderSize, FinalAlpha);
		}

		if(GetRenderParams(Part, 1.0f, vec3(0.0f, 0.0f, 0.0f), RenderPos, RenderSize, FinalAlpha))
			DrawByType(RenderPos, RenderSize, FinalAlpha);
	}
	Graphics()->LinesEnd();
}
