#ifndef GAME_CLIENT_COMPONENTS_MA_PARTICLES3D_H
#define GAME_CLIENT_COMPONENTS_MA_PARTICLES3D_H

#include <base/color.h>
#include <base/vmath.h>

#include <game/client/component.h>

#include "visualizer/service.h"

#include <cstdint>
#include <memory>
#include <vector>

class CMa3DParticles : public CComponent
{
public:
	~CMa3DParticles() override;
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnReset() override;
	void OnRender() override;

private:
	struct SParticle
	{
		vec3 m_Pos;
		vec3 m_Vel;
		vec3 m_Rot;
		vec3 m_RotVel;
		ColorRGBA m_Color;
		float m_Size;
		vec3 m_SpawnOffset;
		vec3 m_FadeOutOffset;
		float m_SpawnTime;
		float m_FadeOutStart;
		int m_Type;
		bool m_FadingOut;
	};

	std::vector<SParticle> m_vParticles;
	float m_Time = 0.0f;
	std::unique_ptr<BestClientVisualizer::CRealtimeMusicVisualizer> m_pMusicVisualizer;
	BestClientVisualizer::SVisualizerFrame m_LastMusicReactionFrame;
	int64_t m_LastMusicReactionPollTick = 0;
	float m_MusicReactionLevel = 0.0f;
	float m_MusicReactionKick = 0.0f;
	float m_MusicReactionRollingPeak = 0.05f;
	bool m_HasConfigSnapshot = false;
	int m_LastType = 0, m_LastCount = 0, m_LastSizeMax = 0, m_LastSpeed = 0;
	int m_LastAlpha = 0, m_LastColorMode = 0, m_LastGlow = 0, m_LastGlowAlpha = 0;
	int m_LastGlowOffset = 0, m_LastDepth = 0, m_LastFadeInMs = 0, m_LastFadeOutMs = 0;
	int m_LastPushRadius = 0, m_LastPushStrength = 0, m_LastCollide = 0;
	unsigned m_LastColor = 0;
	vec2 m_LastSpawnMin, m_LastSpawnMax;
	float m_LastScreenW = 0, m_LastScreenH = 0;
	bool m_HasLastSpawnBounds = false, m_HasLastScreenSize = false;

	void ResetParticles();
	void ResetMusicReaction();
	float UpdateMusicReaction(float Delta);
	bool ShouldRender() const;
	void RenderParticles(float VMinX, float VMaxX, float VMinY, float VMaxY, float BaseAlpha, float FadeIn, float FadeOut, float Lod);

	static vec3 RotateVec3(const vec3 &V, float Cx, float Sx, float Cy, float Sy, float Cz, float Sz);
	static vec2 ProjectPoint(const vec3 &Pos, const vec2 &Center);
};

#endif
