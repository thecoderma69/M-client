#include "service.h"

#include "source.h"

#include <base/math.h>
#include <base/system.h>

#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>
#include <mutex>

namespace BestClientVisualizer
{

namespace
{

std::mutex s_VisualizerSourceMutex;
std::weak_ptr<IVisualizerSource> s_pSharedVisualizerSource;

static std::unique_ptr<IVisualizerSource> CreatePlatformVisualizerSource()
{
#if defined(CONF_PLATFORM_LINUX) && defined(BC_MUSICPLAYER_HAS_PULSE) && BC_MUSICPLAYER_HAS_PULSE
	return CreatePulseVisualizerSource();
#elif defined(CONF_FAMILY_WINDOWS)
	return CreateWasapiVisualizerSource();
#else
	return CreatePassiveVisualizerSource();
#endif
}

static std::shared_ptr<IVisualizerSource> GetSharedVisualizerSource()
{
	std::lock_guard<std::mutex> Lock(s_VisualizerSourceMutex);
	if(std::shared_ptr<IVisualizerSource> pSource = s_pSharedVisualizerSource.lock())
		return pSource;

	std::unique_ptr<IVisualizerSource> pNewSource = CreatePlatformVisualizerSource();
	std::shared_ptr<IVisualizerSource> pSharedSource(pNewSource.release());
	s_pSharedVisualizerSource = pSharedSource;
	return pSharedSource;
}

} // namespace

CRealtimeMusicVisualizer::CRealtimeMusicVisualizer()
{
	m_pSource = GetSharedVisualizerSource();
	RefreshConfig();
}

CRealtimeMusicVisualizer::~CRealtimeMusicVisualizer() = default;

void CRealtimeMusicVisualizer::RefreshConfig()
{
	SVisualizerConfig Config;
	Config.m_SampleRate = maximum(8000, m_ConfigInitialized ? m_Config.m_SampleRate : 48000);
	Config.m_BandCount = MAX_VISUALIZER_BANDS;
	Config.m_LowCutHz = 50;
	Config.m_HighCutHz = 10000;
	Config.m_BassSplitHz = 100;
	Config.m_NoiseReduction = std::clamp(g_Config.m_MaMusicPlayerVisualizerSmoothing / 100.0f, 0.0f, 0.99f);
	const float RawSensitivity = std::clamp(g_Config.m_MaMusicPlayerVisualizerSensitivity / 100.0f, 0.5f, 3.0f);
	Config.m_Sensitivity = powf(RawSensitivity, 1.35f);
	m_Config = Config;
	m_ConfigInitialized = true;
	if(m_pSource)
		m_pSource->SetConfig(m_Config);
}

void CRealtimeMusicVisualizer::SetPlaybackHint(const SVisualizerPlaybackHint &Hint)
{
	RefreshConfig();
	if(m_pSource)
		m_pSource->SetPlaybackHint(Hint);
}

bool CRealtimeMusicVisualizer::PollFrame(SVisualizerFrame &OutFrame)
{
	RefreshConfig();
	if(!m_pSource)
	{
		OutFrame = SVisualizerFrame();
		return false;
	}
	return m_pSource->PollFrame(OutFrame);
}

} // namespace BestClientVisualizer
