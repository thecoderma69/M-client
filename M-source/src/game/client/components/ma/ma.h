#ifndef GAME_CLIENT_COMPONENTS_MA_MA_H
#define GAME_CLIENT_COMPONENTS_MA_MA_H

#include "visualizer/types.h"

#include <base/color.h>
#include <base/system.h>

#include <engine/client/enums.h>
#include <engine/graphics.h>
#include <engine/sound.h>
#include <engine/shared/config.h>
#include <engine/shared/protocol.h>

#include <game/gamecore.h>
#include <game/client/component.h>
#include <game/client/ui_rect.h>
#include <game/client/components/media_decoder.h>

#include <memory>
#include <string>
#include <vector>

namespace BestClientVisualizer
{
class CRealtimeMusicVisualizer;
}

class CMa : public CComponent
{
	void RenderMusicPlayer();
	void UpdateMusicPlayer();
	void UpdateStartupMusic();
	bool StartStartupMusic(bool ForceRestart);
	bool StartStartupMusicWithEngine(const char *pPath, const std::string &Ext);
	bool StartStartupMusicWithMci(const char *pAbsolutePath);
	void UpdateStartupMusicVolume();
	void ResetMusicVideoEffect();
	void UnloadMusicVideoCenterImage();
	bool LoadMusicVideoCenterImage(const char *pPath);
	void EnsureMusicVideoCenterImageLoaded();
	float UpdateMusicVideoEffect(float Delta, bool ForcePreview, bool MusicPlaying, const char *pTitle, const char *pArtist);
	CUIRect GetMusicVideoEffectRect(float Width, float Height, bool ForcePreview) const;
	CUIRect GetSpectatorPanelRect(float Width, float Height, bool ForcePreview) const;
	void RenderMusicVideoEffect(bool ForcePreview);
	void RenderSpectatorPanel(bool ForcePreview);

	class IEngineGraphics *m_pGraphics = nullptr;

	// Music player state
	std::vector<std::string> m_vMusicTracks;
	std::vector<int> m_vMusicSampleIds;
	int m_CurrentTrack = 0;
	bool m_MusicPlaying = false;
	bool m_MusicInitialized = false;

	// Startup music state
	int m_StartupMusicSampleId = -1;
	ISound::CVoiceHandle m_StartupMusicVoice;
	bool m_StartupMusicTried = false;
	bool m_StartupMusicPlaying = false;
	bool m_StartupMusicUsingMci = false;
	int m_StartupMusicAppliedVolume = -1;
	char m_aStartupMusicLoadedPath[IO_MAX_PATH_LENGTH] = "";
	char m_aStartupMusicStatus[128] = "Lista.";

	// Music video effect state
	std::unique_ptr<BestClientVisualizer::CRealtimeMusicVisualizer> m_pMusicVideoVisualizer;
	BestClientVisualizer::SVisualizerFrame m_MusicVideoFrame;
	int64_t m_LastMusicVideoPollTick = 0;
	bool m_MusicVideoUsingAudio = false;
	float m_MusicVideoLevel = 0.0f;
	float m_MusicVideoKick = 0.0f;
	float m_MusicVideoRollingPeak = 0.05f;
	std::vector<SMediaFrame> m_vMusicVideoCenterImageFrames;
	bool m_MusicVideoCenterImageAnimated = false;
	int m_MusicVideoCenterImageWidth = 0;
	int m_MusicVideoCenterImageHeight = 0;
	int64_t m_MusicVideoCenterImageAnimationStart = 0;
	bool m_MusicVideoCenterImageLoaded = false;
	bool m_MusicVideoCenterImageHasError = false;
	char m_aMusicVideoCenterImageLoadedPath[IO_MAX_PATH_LENGTH] = "";
	char m_aMusicVideoCenterImageStatus[128] = "No image.";

public:
	CMa();
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnConsoleInit() override;
	void OnRender() override;
	void OnShutdown() override;
	void OnNewSnapshot() override;
	bool OnInput(const IInput::CEvent &Event) override;
	void OnReset() override;

	bool IsComponentDisabled(int Component) const;
	CUIRect GetMusicVideoEffectHudEditorRect(bool ForcePreview) const;
	void ApplyMusicVideoEffectHudEditorRect(const CUIRect &EditorRect, float HudWidth, float HudHeight);
	void RenderMusicVideoEffectHudEditor(bool ForcePreview);
	CUIRect GetSpectatorPanelHudEditorRect(bool ForcePreview) const;
	void RenderSpectatorPanelHudEditor(bool ForcePreview);
	void RenderMusicVideoEffectBackground();
	void RestartStartupMusic();
	void StopStartupMusic();
	const char *StartupMusicStatusText() const { return m_aStartupMusicStatus; }
	void ReloadMusicVideoCenterImage();
	const char *MusicVideoCenterImageStatusText() const { return m_aMusicVideoCenterImageStatus; }
	bool MusicVideoCenterImageHasError() const { return m_MusicVideoCenterImageHasError; }
	bool MusicVideoCenterImageLoaded() const { return m_MusicVideoCenterImageLoaded; }
	enum { NUM_MA_COMPONENTS = 8 };
};

#endif
