// This file can be included several times.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Tcme, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Tcme, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Tcme, ScriptName, Len, Def, Save, Desc) ;
#endif

#if defined(CONF_FAMILY_WINDOWS)
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to allow any resolution in game when zoom is allowed (buggy on Windows)")
#else
MACRO_CONFIG_INT(TcAllowAnyRes, tc_allow_any_res, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Whether to allow any resolution in game when zoom is allowed (buggy on Windows)")
#endif

MACRO_CONFIG_INT(TcShowChatClient, tc_show_chat_client, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show chat messages from the client such as echo")

MACRO_CONFIG_INT(TcShowFrozenText, tc_frozen_tees_text, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show how many tees in your team are currently frozen. (0 - off, 1 - show alive, 2 - show frozen)")
MACRO_CONFIG_INT(TcShowFrozenHud, tc_frozen_tees_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show frozen tee HUD")
MACRO_CONFIG_INT(TcShowFrozenHudSkins, tc_frozen_tees_hud_skins, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use ninja skin, or darkened skin for frozen tees on hud")

MACRO_CONFIG_INT(TcFrozenHudTeeSize, tc_frozen_tees_size, 15, 8, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of tees in frozen tee hud. (Default : 15)")
MACRO_CONFIG_INT(TcFrozenMaxRows, tc_frozen_tees_max_rows, 1, 1, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of rows in frozen tee HUD display")
MACRO_CONFIG_INT(TcFrozenHudTeamOnly, tc_frozen_tees_only_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only render frozen tee HUD display while in team")

MACRO_CONFIG_INT(TcNameplatePingCircle, tc_nameplate_ping_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows a circle to indicate ping in the nameplate")
MACRO_CONFIG_INT(TcNameplateCountry, tc_nameplate_country, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows the country flag in the nameplate")
MACRO_CONFIG_INT(TcNameplateSkins, tc_nameplate_skins, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows skin names in nameplates, good for finding missing skins")

MACRO_CONFIG_INT(TcFakeCtfFlags, tc_fake_ctf_flags, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Shows fake CTF flags on people (0 = off, 1 = red, 2 = blue)")

MACRO_CONFIG_INT(TcLimitMouseToScreen, tc_limit_mouse_to_screen, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Limit mouse to screen boundaries")
MACRO_CONFIG_INT(TcScaleMouseDistance, tc_scale_mouse_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Improve mouse precision by scaling max distance to 1000")

MACRO_CONFIG_INT(TcHammerRotatesWithCursor, tc_hammer_rotates_with_cursor, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow your hammer to rotate like other weapons")

MACRO_CONFIG_INT(TcMiniVoteHud, tc_mini_vote_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "When enabled makes the vote UI small")

// Anti Latency Tools
MACRO_CONFIG_INT(TcRemoveAnti, tc_remove_anti, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Removes some amount of antiping & player prediction in freeze")
MACRO_CONFIG_INT(TcUnfreezeLagTicks, tc_remove_anti_ticks, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "The biggest amount of prediction ticks that are removed")
MACRO_CONFIG_INT(TcUnfreezeLagDelayTicks, tc_remove_anti_delay_ticks, 25, 5, 150, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many ticks it takes to remove the maximum prediction after being frozen")

MACRO_CONFIG_INT(TcUnpredOthersInFreeze, tc_unpred_others_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Dont predict other players if you are frozen")
MACRO_CONFIG_INT(TcPredMarginInFreeze, tc_pred_margin_in_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable changing prediction margin while frozen")
MACRO_CONFIG_INT(TcPredMarginInFreezeAmount, tc_pred_margin_in_freeze_amount, 15, 0, 2000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Set what your prediction margin while frozen should be")

MACRO_CONFIG_INT(TcShowOthersGhosts, tc_show_others_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ghosts for other players in their unpredicted position")
MACRO_CONFIG_INT(TcSwapGhosts, tc_swap_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show predicted players as ghost and normal players as unpredicted")
MACRO_CONFIG_INT(TcHideFrozenGhosts, tc_hide_frozen_ghosts, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide Ghosts of other players if they are frozen")

MACRO_CONFIG_INT(TcPredGhostsAlpha, tc_pred_ghosts_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of predicted ghosts (0-100)")
MACRO_CONFIG_INT(TcUnpredGhostsAlpha, tc_unpred_ghosts_alpha, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of unpredicted ghosts (0-100)")
MACRO_CONFIG_INT(TcRenderGhostAsCircle, tc_render_ghost_as_circle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render Ghosts as circles instead of tee")

MACRO_CONFIG_INT(TcShowCenter, tc_show_center, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws lines to show the center of your screen/hitbox")
MACRO_CONFIG_INT(TcShowCenterWidth, tc_show_center_width, 0, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Center lines width (enabled by tc_show_center)")
MACRO_CONFIG_COL(TcShowCenterColor, tc_show_center_color, 1694498688, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Center lines color (enabled by tc_show_center)") // transparent red

MACRO_CONFIG_INT(TcFastInput, tc_fast_input, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Uses input for prediction before the next tick")
MACRO_CONFIG_INT(TcFastInputAmount, tc_fast_input_amount, 20, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many milliseconds fast input will apply")
MACRO_CONFIG_INT(TcFastInputOthers, tc_fast_input_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply fast input to other tees")

MACRO_CONFIG_INT(TcAntiPingImproved, tc_antiping_improved, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Different antiping smoothing algorithm, not compatible with cl_antiping_smooth")
MACRO_CONFIG_INT(TcAntiPingNegativeBuffer, tc_antiping_negative_buffer, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Helps in Gores. Allows internal certainty value to be negative which causes more conservative prediction")
MACRO_CONFIG_INT(TcAntiPingStableDirection, tc_antiping_stable_direction, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Predicts optimistically along the tees stable axis to reduce delay in gaining overall stability")
MACRO_CONFIG_INT(TcAntiPingUncertaintyScale, tc_antiping_uncertainty_scale, 150, 25, 400, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Determines uncertainty duration as a factor of ping, 100 = 1.0")

MACRO_CONFIG_INT(TcColorFreeze, tc_color_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use skin colors for frozen tees")
MACRO_CONFIG_INT(TcColorFreezeDarken, tc_color_freeze_darken, 90, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes color of tees darker when in freeze (0-100)")
MACRO_CONFIG_INT(TcColorFreezeFeet, tc_color_freeze_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also use color for frozen tee feet")

// Revert Variables
MACRO_CONFIG_INT(TcSmoothPredictionMargin, tc_prediction_margin_smooth, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Makes prediction margin transition smooth, causes worse ping jitter adjustment (reverts a DDNet change)")
MACRO_CONFIG_INT(TcFrozenKatana, tc_frozen_katana, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show katana on frozen players (reverts a DDNet change)")
MACRO_CONFIG_INT(TcOldTeamColors, tc_old_team_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use rainbow team colors (reverts a DDNet change)")
MACRO_CONFIG_INT(TcRevertHookLine, tc_revert_hook_line, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Revert hookline tip behavior")

// Outline Variables
MACRO_CONFIG_INT(TcOutline, tc_outline, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outlines")
MACRO_CONFIG_INT(TcOutlineEntities, tc_outline_in_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show outlines in entities")

MACRO_CONFIG_INT(TcOutlineSolid, tc_outline_solid, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around hook and unhook")
MACRO_CONFIG_INT(TcOutlineFreeze, tc_outline_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around freeze and deep")
MACRO_CONFIG_INT(TcOutlineUnfreeze, tc_outline_unfreeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around unfreeze and undeep")
MACRO_CONFIG_INT(TcOutlineKill, tc_outline_kill, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around kill")
MACRO_CONFIG_INT(TcOutlineTele, tc_outline_tele, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Draws outline around teleporters")

MACRO_CONFIG_INT(TcOutlineWidthSolid, tc_outline_width_solid, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around hook and unhook")
MACRO_CONFIG_INT(TcOutlineWidthFreeze, tc_outline_width_freeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around freeze and deep")
MACRO_CONFIG_INT(TcOutlineWidthUnfreeze, tc_outline_width_unfreeze, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around unfreeze and undeep")
MACRO_CONFIG_INT(TcOutlineWidthKill, tc_outline_width_kill, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around kill")
MACRO_CONFIG_INT(TcOutlineWidthTele, tc_outline_width_tele, 2, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of outline around teleporters")

MACRO_CONFIG_COL(TcOutlineColorSolid, tc_outline_color_solid, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around hook and unhook") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorFreeze, tc_outline_color_freeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around freeze and deep") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorUnfreeze, tc_outline_color_unfreeze, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around unfreeze and undeep") // 255 0 0 0
MACRO_CONFIG_COL(TcOutlineColorKill, tc_outline_color_kill, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around kill") // 0 0 0
MACRO_CONFIG_COL(TcOutlineColorTele, tc_outline_color_tele, 4294901760, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Color of outline around teleporters") // 255 0 0 0

// Indicator Variables
MACRO_CONFIG_COL(TcIndicatorAlive, tc_indicator_alive, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of alive tees in player indicator")
MACRO_CONFIG_COL(TcIndicatorFreeze, tc_indicator_freeze, 65407, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of frozen tees in player indicator")
MACRO_CONFIG_COL(TcIndicatorSaved, tc_indicator_dead, 0, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of tees who is getting saved in player indicator")
MACRO_CONFIG_INT(TcIndicatorOffset, tc_indicator_offset, 42, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Offset of indicator position")
MACRO_CONFIG_INT(TcIndicatorOffsetMax, tc_indicator_offset_max, 100, 16, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(16-128) Max indicator offset for variable offset setting")
MACRO_CONFIG_INT(TcIndicatorVariableDistance, tc_indicator_variable_distance, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Indicator circles will be further away the further the tee is")
MACRO_CONFIG_INT(TcIndicatorMaxDistance, tc_indicator_variable_max_distance, 1000, 500, 7000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum tee distance for variable offset")
MACRO_CONFIG_INT(TcIndicatorRadius, tc_indicator_radius, 4, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "(1-16) indicator circle size")
MACRO_CONFIG_INT(TcIndicatorOpacity, tc_indicator_opacity, 50, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Opacity of indicator circles")
MACRO_CONFIG_INT(TcPlayerIndicator, tc_player_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show radial indicator of other tees")
MACRO_CONFIG_INT(TcPlayerIndicatorFreeze, tc_player_indicator_freeze, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show frozen tees in indicator")
MACRO_CONFIG_INT(TcIndicatorTeamOnly, tc_indicator_inteam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show indicator while in team")
MACRO_CONFIG_INT(TcIndicatorTees, tc_indicator_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tees instead of circles")
MACRO_CONFIG_INT(TcIndicatorHideVisible, tc_indicator_hide_visible_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Don't show tees that are on your screen")

// Bind Wheel
MACRO_CONFIG_INT(TcResetBindWheelMouse, tc_reset_bindwheel_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Reset position of mouse when opening bindwheel")

// Regex chat matching
MACRO_CONFIG_STR(TcRegexChatIgnore, tc_regex_chat_ignore, 512, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Filters out chat messages based on a regular expression.")

// Misc visual
MACRO_CONFIG_INT(TcWhiteFeet, tc_white_feet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render all feet as perfectly white base color")
MACRO_CONFIG_STR(TcWhiteFeetSkin, tc_white_feet_skin, 255, "x_ninja", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Base skin for white feet")
MACRO_CONFIG_INT(TcRenderWeaponsAsGun, tc_render_weapons_as_gun, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Renders weapons as the gun sprite instead of the weapon, with the exception of hammer and ninja (1 = with hue, 2 = without hue)")
MACRO_CONFIG_INT(TcMovingTilesEntities, tc_moving_tiles_entities, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show moving tiles in entities")

// Weather particles
MACRO_CONFIG_INT(TcWeatherParticles, tc_weather_particles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render falling weather particles")
MACRO_CONFIG_INT(TcWeatherMode, tc_weather_mode, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather particle mode (0 = snow, 1 = rain, 2 = stars, 3 = mixed particles)")
MACRO_CONFIG_INT(TcWeatherAmount, tc_weather_amount, 50, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Amount of weather particles")
MACRO_CONFIG_INT(TcWeatherSpeed, tc_weather_speed, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather particle falling speed")
MACRO_CONFIG_INT(TcWeatherSize, tc_weather_size, 100, 25, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather particle size")
MACRO_CONFIG_INT(TcWeatherAlpha, tc_weather_alpha, 75, 5, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Weather particle opacity")

// Anime Love
MACRO_CONFIG_INT(TcAnimeLove, tc_anime_love, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render animated Anime Love companion")
MACRO_CONFIG_INT(TcAnimeLoveCharacter, tc_anime_love_character, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love companion skin")
MACRO_CONFIG_INT(TcAnimeLoveAnimation, tc_anime_love_animation, 2, 0, 6, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love animation (0 = wave, 1 = walk, 2 = mixed, 3 = sit, 4 = sleep, 5 = celebrate, 6 = follow)")
MACRO_CONFIG_INT(TcAnimeLovePosition, tc_anime_love_position, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love position (0 = right, 1 = left, 2 = above, 3 = below)")
MACRO_CONFIG_INT(TcAnimeLoveVisibility, tc_anime_love_visibility, 2, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love visibility (0 = both, 1 = menu only, 2 = ingame only)")
MACRO_CONFIG_INT(TcAnimeLoveSpeech, tc_anime_love_speech, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show floating Anime Love phrases")
MACRO_CONFIG_STR(TcAnimeLovePhrase, tc_anime_love_phrase, 32, "Hola", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom Anime Love greeting phrase")
MACRO_CONFIG_INT(TcAnimeLoveSize, tc_anime_love_size, 130, 40, 260, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love companion size")
MACRO_CONFIG_INT(TcAnimeLoveSpeed, tc_anime_love_speed, 100, 25, 250, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love animation speed")
MACRO_CONFIG_INT(TcAnimeLoveWalkDistance, tc_anime_love_walk_distance, 70, 0, 180, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love walking movement distance")
MACRO_CONFIG_INT(TcAnimeLoveAlpha, tc_anime_love_alpha, 95, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Anime Love opacity")

// TClient theme
MACRO_CONFIG_INT(TcTheme, tc_theme, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "TClient menu theme (0 = dark, 1 = pink anime, 2 = cyber, 3 = minimal)")
MACRO_CONFIG_INT(TcThemeCustomColors, tc_theme_custom_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use custom TClient menu colors")
MACRO_CONFIG_INT(TcThemeCustomBackground, tc_theme_custom_background, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use custom TClient settings background color")
MACRO_CONFIG_COL(TcThemeAccentColor, tc_theme_accent_color, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Custom TClient accent color")
MACRO_CONFIG_COL(TcThemePanelColor, tc_theme_panel_color, 3221225472, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Custom TClient panel color")
MACRO_CONFIG_COL(TcThemeBackgroundColor, tc_theme_background_color, 1694498815, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Custom TClient settings background color")

// TClient custom sounds
MACRO_CONFIG_INT(TcSoundPack, tc_sound_pack, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "TClient sound pack (0 = classic, 1 = anime, 2 = cyber, 3 = minimal)")
MACRO_CONFIG_INT(TcSoundFriendJoin, tc_sound_friend_join, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when a friend enters")
MACRO_CONFIG_INT(TcSoundMapFinish, tc_sound_map_finish, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when you finish a map")
MACRO_CONFIG_INT(TcSoundHighlight, tc_sound_highlight, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound when you receive a highlight")
MACRO_CONFIG_INT(TcSoundVolume, tc_sound_volume, 100, 0, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "TClient custom sound volume")

MACRO_CONFIG_INT(TcMiniDebug, tc_mini_debug, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show position and angle")

MACRO_CONFIG_INT(TcNotifyWhenLast, tc_last_notify, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Notify when you are last")
MACRO_CONFIG_STR(TcNotifyWhenLastText, tc_last_notify_text, 64, "Last!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text for last notify")
MACRO_CONFIG_COL(TcNotifyWhenLastColor, tc_last_notify_color, 256, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color for last notify")
MACRO_CONFIG_INT(TcNotifyWhenLastX, tc_last_notify_x, 20, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Horizontal position for last notify as percentage of screen width")
MACRO_CONFIG_INT(TcNotifyWhenLastY, tc_last_notify_y, 1, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Vertical position for last notify as percentage of screen height")
MACRO_CONFIG_INT(TcNotifyWhenLastSize, tc_last_notify_size, 10, 0, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Font size for last notify")

MACRO_CONFIG_INT(TcRenderCursorSpec, tc_cursor_in_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render your gun cursor when spectating in freeview")
MACRO_CONFIG_INT(TcRenderCursorSpecAlpha, tc_cursor_in_spec_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of cursor in freeview")

// MACRO_CONFIG_INT(TcRenderNameplateSpec, tc_render_nameplate_spec, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render nameplates when spectating")

MACRO_CONFIG_INT(TcTinyTees, tc_tiny_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render tees smaller")
MACRO_CONFIG_INT(TcTinyTeeSize, tc_indicator_tees_size, 100, 85, 115, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Define the Size of the Tiny Tee")
MACRO_CONFIG_INT(TcTinyTeesOthers, tc_tiny_tees_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render other tees smaller")

MACRO_CONFIG_INT(TcCursorScale, tc_cursor_scale, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Percentage to scale the in game cursor by as a percentage (50 = half, 200 = double)")

// Profiles
MACRO_CONFIG_INT(TcProfileSkin, tc_profile_skin, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply skin in profiles")
MACRO_CONFIG_INT(TcProfileName, tc_profile_name, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply name in profiles")
MACRO_CONFIG_INT(TcProfileClan, tc_profile_clan, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply clan in profiles")
MACRO_CONFIG_INT(TcProfileFlag, tc_profile_flag, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply flag in profiles")
MACRO_CONFIG_INT(TcProfileColors, tc_profile_colors, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply colors in profiles")
MACRO_CONFIG_INT(TcProfileEmote, tc_profile_emote, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Apply emote in profiles")
MACRO_CONFIG_INT(TcProfileOverwriteClanWithEmpty, tc_profile_overwrite_clan_with_empty, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Overwrite clan name even if profile has an empty clan name")

// Rainbow
MACRO_CONFIG_INT(TcRainbowTees, tc_rainbow_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side")
MACRO_CONFIG_INT(TcRainbowHook, tc_rainbow_hook, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow hook")
MACRO_CONFIG_INT(TcRainbowWeapon, tc_rainbow_weapon, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow Weapons")

MACRO_CONFIG_INT(TcRainbowOthers, tc_rainbow_others, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Turn on rainbow client side for others")
MACRO_CONFIG_INT(TcRainbowMode, tc_rainbow_mode, 1, 1, 4, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow mode (1: rainbow, 2: pulse, 3: darkness, 4: random)")
MACRO_CONFIG_INT(TcRainbowSpeed, tc_rainbow_speed, 100, 0, 10000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rainbow speed as a percentage (50 = half speed, 200 = double speed)")

// War List
MACRO_CONFIG_INT(TcWarList, tc_warlist, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggles war list visuals")
MACRO_CONFIG_INT(TcWarListShowClan, tc_warlist_show_clan_if_war, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan in nameplate if there is a war")
MACRO_CONFIG_INT(TcWarListReason, tc_warlist_reason, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war reason")
MACRO_CONFIG_INT(TcWarListChat, tc_warlist_chat, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in chat")
MACRO_CONFIG_INT(TcWarListScoreboard, tc_warlist_scoreboard, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in scoreboard")
MACRO_CONFIG_INT(TcWarListAllowDuplicates, tc_warlist_allow_duplicates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Allow duplicate war entries")
MACRO_CONFIG_INT(TcWarListSpectate, tc_warlist_spectate, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show war colors in spectator menu")

MACRO_CONFIG_INT(TcWarListIndicator, tc_warlist_indicator, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use warlist for indicator")
MACRO_CONFIG_INT(TcWarListIndicatorColors, tc_warlist_indicator_colors, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show warlist colors instead of freeze colors")
MACRO_CONFIG_INT(TcWarListIndicatorAll, tc_warlist_indicator_all, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show all groups")
MACRO_CONFIG_INT(TcWarListIndicatorEnemy, tc_warlist_indicator_enemy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from the first group")
MACRO_CONFIG_INT(TcWarListIndicatorTeam, tc_warlist_indicator_team, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show players from second group")

// Status Bar
MACRO_CONFIG_INT(TcStatusBar, tc_statusbar, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable status bar")

MACRO_CONFIG_INT(TcStatusBar12HourClock, tc_statusbar_12_hour_clock, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use 12 hour clock in local time")
MACRO_CONFIG_INT(TcStatusBarLocalTimeSeocnds, tc_statusbar_local_time_seconds, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show seconds in local time")
MACRO_CONFIG_INT(TcStatusBarHeight, tc_statusbar_height, 8, 1, 16, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Height of the status bar")

MACRO_CONFIG_COL(TcStatusBarColor, tc_statusbar_color, 3221225472, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background color")
MACRO_CONFIG_COL(TcStatusBarTextColor, tc_statusbar_text_color, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text color")
MACRO_CONFIG_INT(TcStatusBarAlpha, tc_statusbar_alpha, 75, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar background alpha")
MACRO_CONFIG_INT(TcStatusBarTextAlpha, tc_statusbar_text_alpha, 100, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Status bar text alpha")

MACRO_CONFIG_INT(TcStatusBarLabels, tc_statusbar_labels, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show labels on status bar entries")
MACRO_CONFIG_STR(TcStatusBarScheme, tc_statusbar_scheme, 128, "ac pf r", CFGFLAG_CLIENT | CFGFLAG_SAVE, "The order in which to show status bar items")

// Trails
MACRO_CONFIG_INT(TcTeeTrail, tc_tee_trail, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable Tee trails")
MACRO_CONFIG_INT(TcTeeTrailOthers, tc_tee_trail_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show tee trails for other players")
MACRO_CONFIG_INT(TcTeeTrailWidth, tc_tee_trail_width, 15, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail width")
MACRO_CONFIG_INT(TcTeeTrailLength, tc_tee_trail_length, 25, 5, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail length")
MACRO_CONFIG_INT(TcTeeTrailAlpha, tc_tee_trail_alpha, 80, 1, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail alpha")
MACRO_CONFIG_COL(TcTeeTrailColor, tc_tee_trail_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color")
MACRO_CONFIG_INT(TcTeeTrailTaper, tc_tee_trail_taper, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Taper tee trail over length")
MACRO_CONFIG_INT(TcTeeTrailFade, tc_tee_trail_fade, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fade trail alpha over length")
MACRO_CONFIG_INT(TcTeeTrailColorMode, tc_tee_trail_color_mode, 1, 1, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail color mode (1: Solid color, 2: Current Tee color, 3: Rainbow, 4: Color based on Tee speed, 5: Random)")
MACRO_CONFIG_INT(TcTeeTrailStyle, tc_tee_trail_style, 0, 0, 12, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail style (0: Default, 1: Hearts, 2: Stars, 3: Diamonds, 4: Moons, 5: Lightning, 6: Butterflies, 7: Flowers, 8: Music notes, 9: Skulls, 10: Crowns, 11: Flames, 12: Snowflakes)")
MACRO_CONFIG_INT(TcTeeTrailMovement, tc_tee_trail_movement, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Rotate shaped tee trails")
MACRO_CONFIG_INT(TcTeeTrailMovementSpeed, tc_tee_trail_movement_speed, 100, 0, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail movement speed")
MACRO_CONFIG_INT(TcTeeTrailMusicReaction, tc_tee_trail_music_reaction, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make tee trails react to system music")
MACRO_CONFIG_INT(TcTeeTrailMusicReactionStrength, tc_tee_trail_music_reaction_strength, 100, 0, 300, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tee trail music reaction strength")

// Chat Reply
MACRO_CONFIG_INT(TcAutoReplyMuted, tc_auto_reply_muted, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto reply to muted players with a message")
MACRO_CONFIG_STR(TcAutoReplyMutedMessage, tc_auto_reply_muted_message, 128, "I have muted you", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply to muted players")
MACRO_CONFIG_INT(TcAutoReplyMinimized, tc_auto_reply_minimized, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto reply when your game is minimized")
MACRO_CONFIG_STR(TcAutoReplyMinimizedMessage, tc_auto_reply_minimized_message, 128, "I am not tabbed in", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to reply when your game is minimized")

// Voting
MACRO_CONFIG_INT(TcAutoVoteWhenFar, tc_auto_vote_when_far, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto vote no if you far on a map")
MACRO_CONFIG_STR(TcAutoVoteWhenFarMessage, tc_auto_vote_when_far_message, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send when auto far vote happens, leave empty to disable")
MACRO_CONFIG_INT(TcAutoVoteWhenFarTime, tc_auto_vote_when_far_time, 5, 0, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How long until auto vote far happens")

// Font
MACRO_CONFIG_STR(TcCustomFont, tc_custom_font, 255, "DejaVu Sans", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom font face")

// Bg Draw
MACRO_CONFIG_INT(TcBgDrawWidth, tc_bg_draw_width, 5, 1, 50, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Width of background draw strokes")
MACRO_CONFIG_INT(TcBgDrawFadeTime, tc_bg_draw_fade_time, 0, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Time until strokes disappear (0 = never)")
MACRO_CONFIG_INT(TcBgDrawMaxItems, tc_bg_draw_max_items, 128, 0, 2048, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum number of strokes")
MACRO_CONFIG_COL(TcBgDrawColor, tc_bg_draw_color, 14024576, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Color of background draw strokes")
MACRO_CONFIG_INT(TcBgDrawAutoSaveLoad, tc_bg_draw_auto_save_load, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Automatically save and load background drawings")

// Translate
MACRO_CONFIG_STR(TcTranslateBackend, tc_translate_backend, 32, "google", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Translate backends (google, ftapi, libretranslate)")
MACRO_CONFIG_STR(BcTranslateIncomingSource, bc_translate_incoming_source, 16, "auto", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Source language for incoming chat translation (use auto to detect)")
MACRO_CONFIG_STR(BcTranslateIncomingIgnoreLanguages, bc_translate_incoming_ignore_languages, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Semicolon-separated source languages that should not be auto-translated, e.g. ru; en; zh")
MACRO_CONFIG_STR(TcTranslateTarget, tc_translate_target, 16, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Target language for incoming chat translation")
MACRO_CONFIG_STR(BcTranslateOutgoingSource, bc_translate_outgoing_source, 16, "auto", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Source language for your outgoing chat translation (use auto to detect)")
MACRO_CONFIG_STR(BcTranslateOutgoingTarget, bc_translate_outgoing_target, 16, "en", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Target language for your outgoing chat translation")
MACRO_CONFIG_STR(TcTranslateEndpoint, tc_translate_endpoint, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, endpoint to use (must be https)")
MACRO_CONFIG_STR(TcTranslateKey, tc_translate_key, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "For backends which need it, api key to use")
MACRO_CONFIG_INT(TcTranslateAutoIncoming, tc_translate_auto_incoming, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable automatic translation of incoming chat messages (other players)")
MACRO_CONFIG_INT(TcTranslateAutoOutgoing, tc_translate_auto_outgoing, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable automatic translation of your outgoing chat messages")
MACRO_CONFIG_INT(TcTranslateAuto, tc_translate_auto, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "DEPRECATED (migrates on startup): use tc_translate_auto_incoming and tc_translate_auto_outgoing")

// Animations
MACRO_CONFIG_INT(TcAnimateWheelTime, tc_animate_wheel_time, 80, 0, 1000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Duration of emote and bind wheel animations, in milliseconds (0 == no animation, 1000 = 1 second)")

// Pets
MACRO_CONFIG_INT(TcPetShow, tc_pet_show, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a pet")
MACRO_CONFIG_STR(TcPetSkin, tc_pet_skin, 24, "twinbop", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Pet skin")
MACRO_CONFIG_INT(TcPetSize, tc_pet_size, 60, 10, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Size of the pet as a percentage of a normal player")
MACRO_CONFIG_INT(TcPetAlpha, tc_pet_alpha, 90, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Alpha of pet (100 = fully opaque, 50 = half transparent)")

// Change name near finish
MACRO_CONFIG_INT(TcChangeNameNearFinish, tc_change_name_near_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Attempt to change your name when near finish")
MACRO_CONFIG_STR(TcFinishName, tc_finish_name, 16, "nameless tee", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Name to change to when near finish when tc_change_name_near_finish is 1")

// Flags
MACRO_CONFIG_INT(TcTClientSettingsTabs, tc_tclient_settings_tabs, 0, 0, 65536, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Bit flags to disable settings tabs")

// Volleyball
MACRO_CONFIG_INT(TcVolleyBallBetterBall, tc_volleyball_better_ball, 1, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make frozen players in volleyball look more like volleyballs (0 = disabled, 1 = in volleyball maps, 2 = always)")
MACRO_CONFIG_STR(TcVolleyBallBetterBallSkin, tc_volleyball_better_ball_skin, 24, "Volleyball", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Player skin to use for better volleyball ball")

// Mod
MACRO_CONFIG_INT(TcShowPlayerHitBoxes, tc_show_player_hit_boxes, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show player hit boxes (1 = predicted, 2 = predicted and unpredicted)")
MACRO_CONFIG_INT(TcHideChatBubbles, tc_hide_chat_bubbles, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide your own chat bubbles, only works when authed in remote console")
MACRO_CONFIG_INT(TcModWeapon, tc_mod_weapon, 0, 0, 1, CFGFLAG_CLIENT, "Run a command (default kill) when you point and shoot at someone, only works when authed in remote console")
MACRO_CONFIG_STR(TcModWeaponCommand, tc_mod_weapon_command, 256, "rcon kill_pl", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Command to run with tc_mod_weapon, id is appended to end of command")

// Run on join
MACRO_CONFIG_STR(TcExecuteOnConnect, tc_execute_on_connect, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_STR(TcExecuteOnJoin, tc_execute_on_join, 100, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(TcExecuteOnJoinDelay, tc_execute_on_join_delay, 2, 7, 50000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Tick delay before executing tc_execute_on_join")

// Custom Communities
MACRO_CONFIG_STR(TcCustomCommunitiesUrl, tc_custom_communities_url, 256, "https://raw.githubusercontent.com/SollyBunny/ddnet-custom-communities/refs/heads/main/custom-communities-ddnet-info.json", CFGFLAG_CLIENT | CFGFLAG_SAVE, "URL to fetch custom communities from (must be https), empty to disable")

// Discord RPC
MACRO_CONFIG_INT(TcDiscordRPC, tc_discord_rpc, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Toggle discord RPC (requires restart)") // broken

MACRO_CONFIG_INT(TcShowLocalTimeSeconds, tc_show_local_time_seconds, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show local time in seconds")

// Configs tab UI
MACRO_CONFIG_INT(TcUiShowDDNet, tc_ui_show_ddnet, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show DDNet domain in Configs tab")
MACRO_CONFIG_INT(TcUiShowTClient, tc_ui_show_tclient, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show TClient domain in Configs tab")
MACRO_CONFIG_INT(TcUiOnlyModified, tc_ui_only_modified, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show only modified settings in Configs tab")
MACRO_CONFIG_INT(TcUiCompactList, tc_ui_compact_list, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use compact row layout in Configs tab")

// Dummy Info
MACRO_CONFIG_INT(TcShowhudDummyPosition, tc_showhud_dummy_position, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Position)")
MACRO_CONFIG_INT(TcShowhudDummySpeed, tc_showhud_dummy_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Speed)")
MACRO_CONFIG_INT(TcShowhudDummyAngle, tc_showhud_dummy_angle, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ingame HUD (Dummy Aim Angle)")

// ===== VISUALS =====
MACRO_CONFIG_INT(TcVisualsWireframe, tc_visuals_wireframe, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Render walls in wireframe mode")
MACRO_CONFIG_INT(TcVisualsNoRenderWeapons, tc_visuals_no_render_weapons, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide weapon rendering for cleaner visuals")
MACRO_CONFIG_INT(TcVisualsTransparentTees, tc_visuals_transparent_tees, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Make other tees semi-transparent")
MACRO_CONFIG_INT(TcVisualsTransparentAmount, tc_visuals_transparent_amount, 50, 10, 90, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Transparency amount for tees (10-90)")
MACRO_CONFIG_INT(TcVisualsNoHookLine, tc_visuals_no_hook_line, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide all hook lines for cleaner visuals")
MACRO_CONFIG_INT(TcVisualsNoNameplates, tc_visuals_no_nameplates, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Hide all nameplates")
MACRO_CONFIG_INT(TcVisualsZoom, tc_visuals_zoom, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable custom zoom override")
MACRO_CONFIG_INT(TcVisualsZoomLevel, tc_visuals_zoom_level, 10, 1, 20, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom zoom level override")

// ===== KEYSTROKE HUD =====
MACRO_CONFIG_INT(TcKeystrokeHud, tc_keystroke_hud, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable keystroke HUD overlay (A, D, Space)")
MACRO_CONFIG_INT(TcKeystrokeHudPosX, tc_keystroke_hud_pos_x, 5, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keystroke HUD horizontal position (% of screen)")
MACRO_CONFIG_INT(TcKeystrokeHudPosY, tc_keystroke_hud_pos_y, 70, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keystroke HUD vertical position (% of screen)")
MACRO_CONFIG_INT(TcKeystrokeHudSize, tc_keystroke_hud_size, 100, 1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keystroke HUD size scale (1-200)")
MACRO_CONFIG_INT(TcKeystrokeHudAlpha, tc_keystroke_hud_alpha, 80, 10, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keystroke HUD opacity (10-100)")
MACRO_CONFIG_INT(TcKeystrokeHudStyle, tc_keystroke_hud_style, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keystroke HUD visual model (0=normal, 1=round, 2=diamond, 3=hexagon)")
MACRO_CONFIG_COL(TcKeystrokeHudColorPressed, tc_keystroke_hud_color_pressed, 4278190335, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Keystroke HUD key pressed color")
MACRO_CONFIG_COL(TcKeystrokeHudColorUnpressed, tc_keystroke_hud_color_unpressed, 2147483648, CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_COLALPHA, "Keystroke HUD key unpressed color")
MACRO_CONFIG_INT(TcKeystrokeHudShowText, tc_keystroke_hud_show_text, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show key labels on keystroke HUD")
MACRO_CONFIG_INT(TcKeystrokeHudShowSpace, tc_keystroke_hud_show_space, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show space bar on keystroke HUD")
MACRO_CONFIG_INT(TcKeystrokeHudOnlyOnPress, tc_keystroke_hud_only_on_press, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Only show keystroke HUD when keys are pressed")
MACRO_CONFIG_INT(TcKeystrokeHudShowMouse, tc_keystroke_hud_show_mouse, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show mouse click indicators on keystroke HUD")
MACRO_CONFIG_INT(TcKeystrokeHudMousePosX, tc_keystroke_hud_mouse_pos_x, 5, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse click HUD horizontal position (% of screen)")
MACRO_CONFIG_INT(TcKeystrokeHudMousePosY, tc_keystroke_hud_mouse_pos_y, 80, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse click HUD vertical position (% of screen)")
MACRO_CONFIG_INT(TcKeystrokeHudMouseSize, tc_keystroke_hud_mouse_size, 100, 1, 200, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse click HUD size scale (1-200)")
MACRO_CONFIG_INT(TcKeystrokeHudMouseStyle, tc_keystroke_hud_mouse_style, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Mouse click HUD visual model (0=normal, 1=round, 2=diamond, 3=hexagon)")
MACRO_CONFIG_INT(TcKeystrokeHudEditMode, tc_keystroke_hud_edit_mode, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable drag-to-move mode for keystroke HUD")

// ===== HUD =====
MACRO_CONFIG_INT(TcHudShowFps, tc_hud_show_fps, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show FPS counter on HUD")
MACRO_CONFIG_INT(TcHudShowVelocity, tc_hud_show_velocity, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show velocity on HUD")
MACRO_CONFIG_INT(TcHudShowPing, tc_hud_show_ping, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show ping value on HUD")
MACRO_CONFIG_INT(TcHudShowTime, tc_hud_show_time, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show race time on HUD")
MACRO_CONFIG_INT(TcHudShowSpeed, tc_hud_show_speed, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show speed graph on HUD")
MACRO_CONFIG_COL(TcHudFpsColor, tc_hud_fps_color, 255, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS counter color")
MACRO_CONFIG_INT(TcHudFpsPosX, tc_hud_fps_pos_x, 10, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS counter horizontal position (%)")
MACRO_CONFIG_INT(TcHudFpsPosY, tc_hud_fps_pos_y, 10, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "FPS counter vertical position (%)")
MACRO_CONFIG_INT(TcHudKillSound, tc_hud_kill_sound, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play sound on kill")
MACRO_CONFIG_INT(TcHudDeathSound, tc_hud_death_sound, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play sound on death")

// ===== MISC =====
MACRO_CONFIG_INT(TcMiscAutoJump, tc_misc_auto_jump, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto bunny hop (hold jump to auto-repeat)")
MACRO_CONFIG_INT(TcMiscNoBlood, tc_misc_no_blood, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable blood particles")
MACRO_CONFIG_INT(TcMiscNoSmoke, tc_misc_no_smoke, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Disable smoke particles")
MACRO_CONFIG_INT(TcMiscAutoRecord, tc_misc_auto_record, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto record demos on game start")
MACRO_CONFIG_INT(TcMiscSpoofCountry, tc_misc_spoof_country, -1, -1, 999, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Spoof country flag (-1 = off, ISO 3166-1 numeric)")
MACRO_CONFIG_INT(TcMiscSpoofSkin, tc_misc_spoof_skin, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Use a different skin than your actual skin")
MACRO_CONFIG_STR(TcMiscSpoofSkinName, tc_misc_spoof_skin_name, 24, "default", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Skin to show as spoof skin")
MACRO_CONFIG_INT(TcMiscChatSpam, tc_misc_chat_spam, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable chat spammer")
MACRO_CONFIG_STR(TcMiscChatSpamText, tc_misc_chat_spam_text, 256, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Text to spam in chat")
MACRO_CONFIG_INT(TcMiscChatSpamDelay, tc_misc_chat_spam_delay, 5, 1, 60, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Delay between spam messages (seconds)")

// ===== AUTO REACTIONS =====
MACRO_CONFIG_INT(TcAutoReactFinish, tc_auto_react_finish, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto-react on map finish")
MACRO_CONFIG_STR(TcAutoReactFinishMsg, tc_auto_react_finish_msg, 64, "gg", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send on finish")
MACRO_CONFIG_INT(TcAutoReactPB, tc_auto_react_pb, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto-react on personal best")
MACRO_CONFIG_STR(TcAutoReactPBMsg, tc_auto_react_pb_msg, 64, "PB!", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send on PB")
MACRO_CONFIG_INT(TcAutoReactDeath, tc_auto_react_death, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto-react on death")
MACRO_CONFIG_STR(TcAutoReactDeathMsg, tc_auto_react_death_msg, 64, "rip", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send on death")
MACRO_CONFIG_INT(TcAutoReactStart, tc_auto_react_start, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Auto-react on race start")
MACRO_CONFIG_STR(TcAutoReactStartMsg, tc_auto_react_start_msg, 64, "gl", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message to send on race start")
MACRO_CONFIG_INT(TcAutoReactEmote, tc_auto_react_emote, 2, 0, 15, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Emote to send on auto-react (0-15)")

// ===== CUSTOM ACTION SOUNDS =====
MACRO_CONFIG_INT(TcSoundHookPlayer, tc_sound_hook_player, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on hook attach to player")
MACRO_CONFIG_INT(TcSoundHookGround, tc_sound_hook_ground, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on hook attach to ground")
MACRO_CONFIG_INT(TcSoundHammerHit, tc_sound_hammer_hit, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on hammer hit")
MACRO_CONFIG_INT(TcSoundGunFire, tc_sound_gun_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on gun fire")
MACRO_CONFIG_INT(TcSoundShotgunFire, tc_sound_shotgun_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on shotgun fire")
MACRO_CONFIG_INT(TcSoundGrenadeFire, tc_sound_grenade_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on grenade fire")
MACRO_CONFIG_INT(TcSoundLaserFire, tc_sound_laser_fire, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Custom sound on laser fire")

// ===== CUSTOM SOUND FILES =====
MACRO_CONFIG_STR(TcSoundHookPlayerFile, tc_sound_hook_player_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for hook attach player (empty = use pack)")
MACRO_CONFIG_STR(TcSoundHookGroundFile, tc_sound_hook_ground_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for hook attach ground")
MACRO_CONFIG_STR(TcSoundHammerHitFile, tc_sound_hammer_hit_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for hammer hit")
MACRO_CONFIG_STR(TcSoundGunFireFile, tc_sound_gun_fire_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for gun fire")
MACRO_CONFIG_STR(TcSoundShotgunFireFile, tc_sound_shotgun_fire_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for shotgun fire")
MACRO_CONFIG_STR(TcSoundGrenadeFireFile, tc_sound_grenade_fire_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for grenade fire")
MACRO_CONFIG_STR(TcSoundLaserFireFile, tc_sound_laser_fire_file, 128, "", CFGFLAG_CLIENT | CFGFLAG_SAVE | CFGFLAG_INSENSITIVE, "Custom .opus file for laser fire")

// ===== FRIEND JOIN HUD =====
MACRO_CONFIG_INT(TcFriendHud, tc_friend_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a notification when a friend joins the server")
MACRO_CONFIG_INT(TcFriendHudDuration, tc_friend_hud_duration, 5, 1, 15, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Duration in seconds for friend join notification")
MACRO_CONFIG_INT(TcFriendHudCorner, tc_friend_hud_corner, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Corner for friend join notification (0 = top-left, 1 = top-right, 2 = bottom-left, 3 = bottom-right)")
MACRO_CONFIG_INT(TcFriendHudShowClan, tc_friend_hud_show_clan, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show clan name in friend join notification")

// ===== CHAT HISTORY =====
MACRO_CONFIG_INT(TcChatHistory, tc_chat_history, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Enable chat history scroll and search")
MACRO_CONFIG_INT(TcChatHistoryLines, tc_chat_history_lines, 200, 64, 500, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Maximum lines stored in chat history")
MACRO_CONFIG_INT(TcChatHistoryHeight, tc_chat_history_height, 60, 30, 90, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Chat history panel height as percentage of screen")
