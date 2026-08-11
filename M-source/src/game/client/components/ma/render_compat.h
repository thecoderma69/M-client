#ifndef GAME_CLIENT_COMPONENTS_MA_RENDER_COMPAT_H
#define GAME_CLIENT_COMPONENTS_MA_RENDER_COMPAT_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include <base/math.h>
#include <base/str.h>

#include <engine/shared/config.h>

namespace MaRenderCompat
{

enum class EProfile
{
	Native,
	LegacyOpenGL,
	OpenGL30,
	Vulkan,
};

inline EProfile Profile()
{
	if(!g_Config.m_MaRenderCompatibility)
		return EProfile::Native;

	if(str_comp_nocase(g_Config.m_GfxBackend, "Vulkan") == 0)
		return EProfile::Vulkan;

	const bool OpenGL = str_comp_nocase(g_Config.m_GfxBackend, "OpenGL") == 0 || str_comp_nocase(g_Config.m_GfxBackend, "GLES") == 0;
	if(OpenGL)
	{
		if(g_Config.m_GfxGLMajor <= 2)
			return EProfile::LegacyOpenGL;
		if(g_Config.m_GfxGLMajor == 3 && g_Config.m_GfxGLMinor < 3)
			return EProfile::OpenGL30;
	}

	return EProfile::Native;
}

inline bool Active()
{
	return Profile() != EProfile::Native;
}

inline float MaxLod()
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return 0.42f;
	case EProfile::OpenGL30: return 0.68f;
	case EProfile::Vulkan: return 0.86f;
	case EProfile::Native:
	default: return 1.0f;
	}
}

inline float ApplyLod(float Lod)
{
	return std::clamp(std::min(Lod, MaxLod()), 0.30f, 1.0f);
}

inline int Clamp3dParticleCount(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 36);
	case EProfile::OpenGL30: return std::min(Requested, 64);
	case EProfile::Vulkan: return std::min(Requested, 110);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int Clamp3dSpawnBudget(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 3);
	case EProfile::OpenGL30: return std::min(Requested, 5);
	case EProfile::Vulkan: return std::min(Requested, 6);
	case EProfile::Native:
	default: return Requested;
	}
}

inline bool ForceLowDetail()
{
	const EProfile CurProfile = Profile();
	return CurProfile == EProfile::LegacyOpenGL || CurProfile == EProfile::OpenGL30;
}

inline bool AllowGlow(float Lod)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL:
	case EProfile::OpenGL30:
		return false;
	case EProfile::Vulkan:
		return Lod >= 0.82f;
	case EProfile::Native:
	default:
		return true;
	}
}

inline int ClampTrailLength(int Requested, bool Local, float Lod)
{
	int MaxLength = Requested;
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: MaxLength = Local ? 58 : 24; break;
	case EProfile::OpenGL30: MaxLength = Local ? 92 : 46; break;
	case EProfile::Vulkan: MaxLength = Local ? 130 : 72; break;
	case EProfile::Native:
	default: return Requested;
	}
	MaxLength = std::clamp(round_to_int(MaxLength * (0.55f + 0.45f * Lod)), Local ? 18 : 8, MaxLength);
	return std::min(Requested, MaxLength);
}

inline int ClampTrailSymbols(int Requested, bool Local)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, Local ? 5 : 2);
	case EProfile::OpenGL30: return std::min(Requested, Local ? 8 : 3);
	case EProfile::Vulkan: return std::min(Requested, Local ? 10 : 5);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ClampWeatherSpawnBudget(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 8);
	case EProfile::OpenGL30: return std::min(Requested, 14);
	case EProfile::Vulkan: return std::min(Requested, 18);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ClampMusicVideoPoints(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 72);
	case EProfile::OpenGL30: return std::min(Requested, 112);
	case EProfile::Vulkan: return std::min(Requested, 144);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ClampMusicVideoTrailLines(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 2);
	case EProfile::OpenGL30: return std::min(Requested, 4);
	case EProfile::Vulkan: return std::min(Requested, 6);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ChatMediaMaxDimension(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 480);
	case EProfile::OpenGL30: return std::min(Requested, 640);
	case EProfile::Vulkan: return std::min(Requested, 768);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ChatMediaPreviewMaxWidth(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 180);
	case EProfile::OpenGL30: return std::min(Requested, 220);
	case EProfile::Vulkan: return std::min(Requested, 260);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ChatMediaMaxFrames(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 90);
	case EProfile::OpenGL30: return std::min(Requested, 160);
	case EProfile::Vulkan: return std::min(Requested, 240);
	case EProfile::Native:
	default: return Requested;
	}
}

inline std::size_t ChatMediaMaxAnimatedBytes(std::size_t Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min<std::size_t>(Requested, 18ull * 1024ull * 1024ull);
	case EProfile::OpenGL30: return std::min<std::size_t>(Requested, 28ull * 1024ull * 1024ull);
	case EProfile::Vulkan: return std::min<std::size_t>(Requested, 36ull * 1024ull * 1024ull);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int ChatMediaTextureUploadsPerFrame(int Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min(Requested, 1);
	case EProfile::OpenGL30: return std::min(Requested, 2);
	case EProfile::Vulkan: return std::min(Requested, 2);
	case EProfile::Native:
	default: return Requested;
	}
}

inline int64_t ChatMediaTextureUploadBudgetUs(int64_t Requested)
{
	switch(Profile())
	{
	case EProfile::LegacyOpenGL: return std::min<int64_t>(Requested, 900);
	case EProfile::OpenGL30: return std::min<int64_t>(Requested, 1500);
	case EProfile::Vulkan: return std::min<int64_t>(Requested, 1900);
	case EProfile::Native:
	default: return Requested;
	}
}

} // namespace MaRenderCompat

#endif