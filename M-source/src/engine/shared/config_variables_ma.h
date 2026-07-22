// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(MaName, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(MaName, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(MaName, ScriptName, Len, Def, Save, Desc) ;
#endif

MACRO_CONFIG_INT(MaEnabled, ma_enabled, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable MΛ ツ features")

// ===== ASPECT RATIO =====
MACRO_CONFIG_INT(MaCustomAspectRatioMode, ma_custom_aspect_ratio_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aspect ratio mode (0=off, 1=preset, 2=custom)")
MACRO_CONFIG_INT(MaCustomAspectRatioApplyMode, ma_custom_aspect_ratio_apply_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aspect ratio apply mode (0=game only, 1=full, 2=game no hud)")
MACRO_CONFIG_INT(MaCustomAspectRatio, ma_custom_aspect_ratio, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Aspect ratio value x100 (0=off, presets: 125=5:4, 133=4:3, 150=3:2, 178=16:9, custom: 100-1000)")
MACRO_CONFIG_INT(MaCustomAspectRatioNum, ma_custom_aspect_ratio_num, 16, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio numerator (width), 0=unset")
MACRO_CONFIG_INT(MaCustomAspectRatioDen, ma_custom_aspect_ratio_den, 9, 0, 100000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom aspect ratio denominator (height), 0=unset")

// ===== OPTIMIZER =====
MACRO_CONFIG_INT(MaOptimizer, ma_optimizer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable optimizer for better FPS")
MACRO_CONFIG_INT(MaOptimizerFpsFog, ma_optimizer_fps_fog, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cull non-map rendering outside a distance limit around the camera")
MACRO_CONFIG_INT(MaOptimizerFpsFogMode, ma_optimizer_fps_fog_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS fog mode (0=manual radius tiles, 1=by zoom percent)")
MACRO_CONFIG_INT(MaOptimizerFpsFogRadiusTiles, ma_optimizer_fps_fog_radius_tiles, 40, 5, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS fog manual radius in tiles (tile=32 units)")
MACRO_CONFIG_INT(MaOptimizerFpsFogRadius, ma_optimizer_fps_fog_radius, 50, 10, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS fog radius in tiles (legacy)")
MACRO_CONFIG_INT(MaOptimizerFpsFogZoomPercent, ma_optimizer_fps_fog_zoom_percent, 70, 10, 120, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS fog visible area percent in zoom mode")
MACRO_CONFIG_INT(MaOptimizerFpsFogRenderRect, ma_optimizer_fps_fog_render_rect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render an outline rectangle showing the FPS fog area")
MACRO_CONFIG_INT(MaOptimizerFpsFogCullMapTiles, ma_optimizer_fps_fog_cull_map_tiles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Cull map tile rendering outside the FPS fog area")
MACRO_CONFIG_INT(MaOptimizerNoParticles, ma_optimizer_no_particles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable all particles")
MACRO_CONFIG_INT(MaOptimizerHighPriority, ma_optimizer_high_priority, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set DDNet process priority to high")
MACRO_CONFIG_INT(MaOptimizerDiscordPriorityBelowNormal, ma_optimizer_discord_priority_below_normal, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set Discord process priority to Below Normal while enabled")

// ===== GORES MODE =====
MACRO_CONFIG_INT(MaGoresMode, ma_gores_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Gores Mode (hammer + hook only)")
MACRO_CONFIG_INT(MaGoresAuto, ma_gores_auto, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto-detect gores servers")
MACRO_CONFIG_INT(MaGoresModeDisableIfWeapons, ma_gores_mode_disable_weapons, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable gores mode when holding shotgun, grenade or laser")

// ===== 3D PARTICLES =====
MACRO_CONFIG_INT(Ma3dParticles, ma_3d_particles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle 3D particles")
MACRO_CONFIG_INT(Ma3dParticlesType, ma_3d_particles_type, 1, 1, 13, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Type (1=Normal cubes, 2=Hearts, 3=Stars, 4=Diamonds, 5=Moons, 6=Lightning, 7=Butterflies, 8=Flowers, 9=Music notes, 10=Skulls, 11=Crowns, 12=Flames, 13=Snowflakes)")
MACRO_CONFIG_INT(Ma3dParticlesCount, ma_3d_particles_count, 60, 1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Count of 3D particles")
MACRO_CONFIG_INT(Ma3dParticlesSizeMin, ma_3d_particles_size_min, 3, 2, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Minimum size")
MACRO_CONFIG_INT(Ma3dParticlesSizeMax, ma_3d_particles_size_max, 8, 2, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum size")
MACRO_CONFIG_INT(Ma3dParticlesSpeed, ma_3d_particles_speed, 18, 1, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base speed")
MACRO_CONFIG_INT(Ma3dParticlesMovementSpeed, ma_3d_particles_movement_speed, 300, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "3D particle rotation speed")
MACRO_CONFIG_INT(Ma3dParticlesMusicReaction, ma_3d_particles_music_reaction, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make 3D particles react to system music")
MACRO_CONFIG_INT(Ma3dParticlesMusicReactionStrength, ma_3d_particles_music_reaction_strength, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "3D particle music reaction strength")
MACRO_CONFIG_INT(Ma3dParticlesDepth, ma_3d_particles_depth, 300, 10, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Depth range")
MACRO_CONFIG_INT(Ma3dParticlesAlpha, ma_3d_particles_alpha, 35, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha (1-100)")
MACRO_CONFIG_INT(Ma3dParticlesFadeInMs, ma_3d_particles_fade_in_ms, 400, 1, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade-in time in ms")
MACRO_CONFIG_INT(Ma3dParticlesFadeOutMs, ma_3d_particles_fade_out_ms, 400, 1, 5000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade-out time in ms")
MACRO_CONFIG_INT(Ma3dParticlesPushRadius, ma_3d_particles_push_radius, 120, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player push radius")
MACRO_CONFIG_INT(Ma3dParticlesPushStrength, ma_3d_particles_push_strength, 120, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Player push strength")
MACRO_CONFIG_INT(Ma3dParticlesCollide, ma_3d_particles_collide, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Particles collide with each other")
MACRO_CONFIG_INT(Ma3dParticlesColorMode, ma_3d_particles_color_mode, 1, 1, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color mode (1=Custom, 2=Random)")
MACRO_CONFIG_COL(Ma3dParticlesColor, ma_3d_particles_color, 4294967295, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of 3D particles")
MACRO_CONFIG_INT(Ma3dParticlesGlow, ma_3d_particles_glow, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable glow")
MACRO_CONFIG_INT(Ma3dParticlesGlowAlpha, ma_3d_particles_glow_alpha, 35, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Glow alpha (1-100)")
MACRO_CONFIG_INT(Ma3dParticlesGlowOffset, ma_3d_particles_glow_offset, 2, 1, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Glow offset")

// ===== MEDIA BACKGROUND =====
MACRO_CONFIG_INT(MaMenuMediaBackground, ma_menu_media_background, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable custom media background in offline menus")
MACRO_CONFIG_INT(MaGameMediaBackground, ma_game_media_background, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable custom media background in game background rendering")
MACRO_CONFIG_INT(MaGameMediaBackgroundSeparate, ma_game_media_background_separate, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use a different media background file while playing")
MACRO_CONFIG_STR(MaMenuMediaBackgroundPath, ma_menu_media_background_path, IO_MAX_PATH_LENGTH, "tclient/backgrounds/thumb-1920-735980.png", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Path to the custom menu media background file")
MACRO_CONFIG_STR(MaGameMediaBackgroundPath, ma_game_media_background_path, IO_MAX_PATH_LENGTH, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Path to the custom in-game media background file")
MACRO_CONFIG_INT(MaGameMediaBackgroundOffset, ma_game_media_background_offset, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How much the custom media background is fixed to the map when rendering the in-game background")
MACRO_CONFIG_INT(MaGameMediaBackgroundOpacity, ma_game_media_background_opacity, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of the custom media background while playing")

// ===== CHAT MEDIA PREVIEWS =====
MACRO_CONFIG_INT(MaChatMediaPreview, ma_chat_media_preview, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render media previews from chat links")
MACRO_CONFIG_INT(MaChatMediaPhotos, ma_chat_media_photos, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render photo previews from chat links")
MACRO_CONFIG_INT(MaChatMediaGifs, ma_chat_media_gifs, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render GIF and animated media previews from chat links")
MACRO_CONFIG_INT(MaChatMediaContentFilter, ma_chat_media_content_filter, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow chat media previews only from configured domains")
MACRO_CONFIG_STR(MaChatMediaAllowedDomains, ma_chat_media_allowed_domains, 512, "tenor.com; tenor.googleapis.com; imgur.com; giphy.com; discordapp.com; discordapp.net", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Semicolon-separated allowlist for chat media domains")
MACRO_CONFIG_INT(MaChatMediaPreviewMaxWidth, ma_chat_media_preview_max_width, 220, 120, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum width of chat media previews")

// ===== MUSIC VIDEO EFFECT =====
MACRO_CONFIG_INT(MaMusicVideoEffect, ma_music_video_effect, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable circular music video effect")
MACRO_CONFIG_INT(MaMusicVideoEffectSize, ma_music_video_effect_size, 120, 40, 240, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect size in percent")
MACRO_CONFIG_INT(MaMusicVideoEffectIntensity, ma_music_video_effect_intensity, 120, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect reaction intensity in percent")
MACRO_CONFIG_INT(MaMusicVideoEffectAlpha, ma_music_video_effect_alpha, 78, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect opacity in percent")
MACRO_CONFIG_INT(MaMusicVideoEffectPoints, ma_music_video_effect_points, 96, 32, 192, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect waveform point count")
MACRO_CONFIG_INT(MaMusicVideoEffectTrailLines, ma_music_video_effect_trail_lines, 6, 1, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect trail line count")
MACRO_CONFIG_INT(MaMusicVideoEffectMusicOnly, ma_music_video_effect_music_only, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Move the music video effect only while a media session is playing")
MACRO_CONFIG_INT(MaMusicVideoEffectShowTrack, ma_music_video_effect_show_track, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show current song title in the music video effect")
MACRO_CONFIG_INT(MaMusicVideoEffectBehind, ma_music_video_effect_behind, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render the music video effect behind gameplay")
MACRO_CONFIG_INT(MaMusicVideoEffectImageSize, ma_music_video_effect_image_size, 72, 20, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect center image size in percent")
MACRO_CONFIG_COL(MaMusicVideoEffectColor, ma_music_video_effect_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music video effect color")
MACRO_CONFIG_STR(MaMusicVideoEffectImagePath, ma_music_video_effect_image_path, IO_MAX_PATH_LENGTH, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Path to the music video effect center image")

// ===== STARTUP MUSIC =====
MACRO_CONFIG_INT(MaStartupMusic, ma_startup_music, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a custom song once when opening the client")
MACRO_CONFIG_INT(MaStartupMusicVolume, ma_startup_music_volume, 55, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Startup music volume")
MACRO_CONFIG_STR(MaStartupMusicPath, ma_startup_music_path, IO_MAX_PATH_LENGTH, "ma/startup_music/welcome to ddnet.mp3", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Path to the custom startup music file")

// ===== MUSIC PLAYER =====
MACRO_CONFIG_INT(MaMusicPlayer, ma_music_player, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Music Player HUD element")
MACRO_CONFIG_INT(MaMusicPlayerShowWhenPaused, ma_music_player_show_when_paused, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep Music Player visible while playback is paused")
MACRO_CONFIG_INT(MaMusicPlayerHideGameTimer, ma_music_player_hide_game_timer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide the original game timer while the music player timer is visible")
MACRO_CONFIG_INT(MaMusicPlayerVisualizer, ma_music_player_visualizer, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable music player visualizer")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerMode, ma_music_player_visualizer_mode, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer mode (0=bottom, 1=center, 2=up)")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerSensitivity, ma_music_player_visualizer_sensitivity, 300, 50, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer sensitivity in percent")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerSmoothing, ma_music_player_visualizer_smoothing, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer smoothing (0-100)")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerRounding, ma_music_player_visualizer_rounding, 0, 0, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer rounding in percent")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerColumns, ma_music_player_visualizer_columns, 5, 5, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer bar count")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerColumnWidth, ma_music_player_visualizer_column_width, 100, 50, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer column width in percent")
MACRO_CONFIG_INT(MaMusicPlayerVisualizerGap, ma_music_player_visualizer_gap, 100, 0, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player visualizer gap in percent")
MACRO_CONFIG_INT(MaMusicPlayerColorMode, ma_music_player_color_mode, 3, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player color mode (0=static, 1=cover accent, 2=dominant cover, 3=translucent)")
MACRO_CONFIG_COL(MaMusicPlayerStaticColor, ma_music_player_static_color, 128, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Static color for the music player when static color mode is selected")
MACRO_CONFIG_INT(MaMusicPlayerSizeMode, ma_music_player_size_mode, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player size mode (0=normal, 1=mini)")
MACRO_CONFIG_INT(MaMusicPlayerTextScale, ma_music_player_text_scale, 84, 70, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player text scale in percent")
MACRO_CONFIG_INT(MaMusicPlayerAnimationMs, ma_music_player_animation_ms, 180, 50, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player animation duration in ms")
MACRO_CONFIG_INT(MaMusicPlayerShowCover, ma_music_player_show_cover, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show cover art in the music player")
MACRO_CONFIG_INT(MaMusicPlayerUseColorForHud, ma_music_player_use_color_for_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use the Music Player color for HUD rectangles")
MACRO_CONFIG_INT(MaMusicPlayerHudColorAlpha, ma_music_player_hud_color_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha multiplier for the Music Player and HUD colors")
MACRO_CONFIG_INT(MaDbgMusicPlayer, ma_dbg_music_player, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Debug logging for music player (0=off, 1=state changes, 2=verbose)")
MACRO_CONFIG_INT(MaMusicVolume, ma_music_volume, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music volume %")

// ===== MUSIC PLAYER STYLE =====
MACRO_CONFIG_INT(MaMusicPlayerStyle, ma_music_player_style, 0, 0, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player style (0=Card, 1=Bar, 2=Minimal, 3=Disc, 4=Banner)")
MACRO_CONFIG_INT(MaMusicPlayerCustomColors, ma_music_player_custom_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use custom colors for music player")
MACRO_CONFIG_COL(MaMusicPlayerColorBg, ma_music_player_color_bg, 0xCC0A0A14, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Music player background color")
MACRO_CONFIG_COL(MaMusicPlayerColorAccent, ma_music_player_color_accent, 0xFF3B82F6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player accent color")
MACRO_CONFIG_COL(MaMusicPlayerColorText, ma_music_player_color_text, 0xFFFFFFFF, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Music player text color")

// ===== FAST INPUT / BEST INPUT / SAIKO+ / MA INPUT =====
MACRO_CONFIG_INT(MaFastInputMode, ma_fast_input_mode, 0, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fast input mode (0=fast input, 3=best input, 4=saiko+, 5=MA input)")
MACRO_CONFIG_INT(MaBestInputOffset, ma_best_input_offset, 0, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input prediction offset in 0.01 ticks (0-10.00 ticks)")
MACRO_CONFIG_INT(MaBestInputSmoothing, ma_best_input_smoothing, 0, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input smoothing amount (0-100%)")
MACRO_CONFIG_INT(MaBestInputLatencyComp, ma_best_input_latency_comp, 0, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input latency compensation (0-50%)")
MACRO_CONFIG_INT(MaBestInputInterpolation, ma_best_input_interpolation, 1, 1, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best input interpolation mode (1=linear, 2=cubic, 3=smooth)")
MACRO_CONFIG_INT(MaSaikoPlusAmount, ma_saiko_plus_amount, 0, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Saiko+ input amount in 0.01 ticks")
MACRO_CONFIG_INT(MaSaikoPlusOthers, ma_saiko_plus_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply Saiko+ input to other tees")
MACRO_CONFIG_INT(MaBestInputOthers, ma_best_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply best input to other tees")
MACRO_CONFIG_INT(MaInputProfile, ma_input_profile, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "MA input profile (0=auto, 1=smooth, 2=balanced, 3=aggressive)")
MACRO_CONFIG_INT(MaInputStrength, ma_input_strength, 55, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "MA input strength")
MACRO_CONFIG_INT(MaInputStability, ma_input_stability, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "MA input stability")
MACRO_CONFIG_INT(MaInputOthers, ma_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply MA input to other tees")

// ===== SNAP TAP =====
MACRO_CONFIG_INT(MaSnapTap, ma_snap_tap, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Snap Tap for opposite left/right inputs")
MACRO_CONFIG_INT(MaSnapTapDelay, ma_snap_tap_delay, 0, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Snap Tap direction switch delay in ms (0=off)")

// ===== SPECTATOR PANEL =====
MACRO_CONFIG_INT(MaSpectatorPanel, ma_spectator_panel, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show who is spectating you when spectator data is available")
MACRO_CONFIG_INT(MaSpectatorPanelShowNames, ma_spectator_panel_show_names, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show spectator names when available")
MACRO_CONFIG_INT(MaSpectatorPanelShowEmpty, ma_spectator_panel_show_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep spectator panel visible with zero spectators")
MACRO_CONFIG_INT(MaSpectatorPanelMaxNames, ma_spectator_panel_max_names, 5, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum spectator names shown in the panel")
MACRO_CONFIG_INT(MaSpectatorPanelOpacity, ma_spectator_panel_opacity, 78, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spectator panel opacity")
MACRO_CONFIG_INT(MaSpectatorPanelHudX, ma_spectator_panel_hud_x, 370, -1000, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor X position for spectator panel")
MACRO_CONFIG_INT(MaSpectatorPanelHudY, ma_spectator_panel_hud_y, 72, -1000, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor Y position for spectator panel")
MACRO_CONFIG_INT(MaSpectatorPanelHudScale, ma_spectator_panel_hud_scale, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor scale for spectator panel")

// ===== COMPONENT EDITOR =====
MACRO_CONFIG_INT(MaComponentsMask, ma_components_mask, 0, 0, 65535, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bitmask for disabled MΛ ツ components")

// ===== HUD POSITIONS =====
MACRO_CONFIG_INT(MaHudChatX, ma_hud_chat_x, 5, -1000, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor X position for chat")
MACRO_CONFIG_INT(MaHudChatY, ma_hud_chat_y, 278, -1000, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor Y position for chat")
MACRO_CONFIG_INT(MaHudChatScale, ma_hud_chat_scale, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "HUD editor scale for chat")
