#ifndef GAME_CLIENT_COMPONENTS_MA_NAME_EFFECTS_H
#define GAME_CLIENT_COMPONENTS_MA_NAME_EFFECTS_H

#include <base/color.h>
#include <base/math.h>

#include <algorithm>

namespace MaNameEffects
{
constexpr int STYLE_MAX = 13;

inline int ClampStyle(int Style)
{
	return std::clamp(Style, 0, STYLE_MAX);
}

inline int NormalizeLegacyStyle(int Style)
{
	if(Style == 2 || Style == 3)
		return 1;
	if(Style == 4)
		return 4;
	if(Style == 5)
		return 2;
	if(Style == 6)
		return 3;
	if(Style >= 7 && Style <= 15)
		return Style - 2;
	return ClampStyle(Style);
}

inline bool LegacyStyleHasStars(int Style)
{
	return Style == 2 || Style == 3;
}

inline float Wave01(int Value, int Period)
{
	if(Period <= 0)
		return 0.0f;
	int Phase = Value % Period;
	if(Phase < 0)
		Phase += Period;
	const int Half = maximum(1, Period / 2);
	return Phase < Half ? (float)Phase / (float)Half : (float)(Period - Phase) / (float)Half;
}

inline bool StyleAnimates(int Style)
{
	Style = ClampStyle(Style);
	return Style == 3 || Style == 4 || Style == 5 || Style == 6 || Style == 8 || Style == 9 || Style == 10 || Style == 12 || Style == 13;
}

inline bool StyleHasGlow(int Style)
{
	Style = ClampStyle(Style);
	return Style == 1 || Style == 4 || Style == 5 || Style == 6 || Style == 7 || Style == 8 || Style == 9 || Style == 10 || Style == 11 || Style == 12 || Style == 13;
}

inline void Decorations(int Style, const char **ppPrefix, const char **ppSuffix)
{
	Style = ClampStyle(Style);
	*ppPrefix = "";
	*ppSuffix = "";
	if(Style == 13)
	{
		*ppPrefix = "[";
		*ppSuffix = "]";
	}
}

inline ColorRGBA ConfigColor(unsigned ConfigColor, float Alpha)
{
	ColorHSLA HslaColor = (ConfigColor & 0xff000000U) != 0 ? ColorHSLA(ConfigColor, true) : ColorHSLA(ConfigColor);
	ColorRGBA Color = color_cast<ColorRGBA>(HslaColor);
	Color.a = std::clamp(Alpha, 0.0f, 1.0f);
	return Color;
}

inline ColorRGBA WithAlpha(ColorRGBA Color, float Alpha)
{
	Color.a = std::clamp(Alpha, 0.0f, 1.0f);
	return Color;
}

inline ColorRGBA MixColors(ColorRGBA A, ColorRGBA B, float Amount)
{
	Amount = std::clamp(Amount, 0.0f, 1.0f);
	return ColorRGBA(
		A.r + (B.r - A.r) * Amount,
		A.g + (B.g - A.g) * Amount,
		A.b + (B.b - A.b) * Amount,
		A.a + (B.a - A.a) * Amount);
}

inline ColorRGBA AccentColor(int Style, float Alpha, unsigned Color1, unsigned Color2, int MotionTick)
{
	Style = ClampStyle(Style);
	switch(Style)
	{
	case 5: return WithAlpha(Wave01(MotionTick * 7, 34) > 0.5f ? ColorRGBA(0.0f, 1.0f, 0.78f) : ColorRGBA(1.0f, 0.02f, 0.86f), Alpha);
	case 6: return WithAlpha(ColorRGBA(1.0f, 0.22f, 0.0f), Alpha);
	case 7: return WithAlpha(ColorRGBA(0.58f, 0.95f, 1.0f), Alpha);
	case 8: return WithAlpha(Wave01(MotionTick * 5, 42) > 0.58f ? ColorRGBA(1.0f, 0.96f, 0.18f) : ColorRGBA(0.18f, 0.78f, 1.0f), Alpha);
	case 9: return WithAlpha(ColorRGBA(0.95f, 0.34f, 1.0f), Alpha);
	case 10: return WithAlpha(ColorRGBA(1.0f, 0.82f, 0.12f), Alpha);
	case 11: return ConfigColor(Color2, Alpha);
	case 12: return ConfigColor(Color2, Alpha);
	case 13: return WithAlpha(ColorRGBA(0.08f, 1.0f, 0.42f), Alpha);
	default: return ConfigColor(Color2, Alpha);
	}
}

inline float GlowStrength(int Style, int Glow, int MotionTick)
{
	Style = ClampStyle(Style);
	float Strength = std::clamp(Glow / 100.0f, 0.0f, 1.0f);
	if(Style == 4 || Style == 12)
		Strength *= 0.58f + 0.42f * Wave01(MotionTick * 3, 72);
	if(Style == 5)
		Strength = maximum(Strength, 0.78f);
	if(Style == 11 || Style == 13)
		Strength = maximum(Strength, 0.65f);
	return std::clamp(Strength, 0.0f, 1.0f);
}

inline ColorRGBA LetterColor(int LetterIndex, float Alpha, int Style, unsigned Color1, unsigned Color2, bool Moving, int MotionTick)
{
	Style = ClampStyle(Style);
	const bool Animate = Moving || StyleAnimates(Style);
	const int Tick = Animate ? MotionTick : 0;
	ColorRGBA Primary = ConfigColor(Color1, Alpha);
	ColorRGBA Accent = ConfigColor(Color2, Alpha);

	switch(Style)
	{
	case 0:
	{
		const int HueOffset = Moving ? Tick * 8 : 0;
		const float Hue = (float)(((LetterIndex * 89 + HueOffset) % 360 + 360) % 360) / 360.0f;
		return WithAlpha(color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.62f)), Alpha);
	}
	case 2:
	{
		const int Phase = (LetterIndex * 31 + (Moving ? Tick * 5 : 0)) % 120;
		const float Mix = Phase < 60 ? Phase / 60.0f : (120 - Phase) / 60.0f;
		return MixColors(Primary, Accent, Mix);
	}
	case 3:
	{
		const float Hue = (float)(((LetterIndex * 54 + Tick * 12) % 360 + 360) % 360) / 360.0f;
		return WithAlpha(color_cast<ColorRGBA>(ColorHSLA(Hue, 1.0f, 0.62f)), Alpha);
	}
	case 4:
	{
		const float Pulse = Wave01(Tick * 3 + LetterIndex * 9, 80);
		return MixColors(MixColors(Primary, Accent, Pulse), ColorRGBA(1.0f, 1.0f, 1.0f, Alpha), 0.22f * Pulse);
	}
	case 5:
	{
		const int Glitch = (LetterIndex * 37 + Tick * 11) % 13;
		if(Glitch == 0 || Glitch == 6)
			return WithAlpha(ColorRGBA(1.0f, 0.02f, 0.82f), Alpha);
		if(Glitch == 1 || Glitch == 8)
			return WithAlpha(ColorRGBA(0.0f, 1.0f, 0.92f), Alpha);
		if(Glitch == 2)
			return WithAlpha(ColorRGBA(0.25f, 1.0f, 0.18f), Alpha);
		return MixColors(Primary, ColorRGBA(0.82f, 1.0f, 0.92f, Alpha), 0.35f);
	}
	case 6:
	{
		const int Phase = (LetterIndex * 29 + Tick * 4) % 90;
		if(Phase < 30)
			return WithAlpha(ColorRGBA(1.0f, 0.12f, 0.0f), Alpha);
		if(Phase < 60)
			return WithAlpha(ColorRGBA(1.0f, 0.48f, 0.03f), Alpha);
		return WithAlpha(ColorRGBA(1.0f, 0.94f, 0.20f), Alpha);
	}
	case 7:
	{
		const float Mix = Wave01(LetterIndex * 18 + (Moving ? Tick * 3 : 0), 100);
		return MixColors(ColorRGBA(0.82f, 1.0f, 1.0f, Alpha), ColorRGBA(0.18f, 0.58f, 1.0f, Alpha), Mix);
	}
	case 8:
	{
		const int Phase = (LetterIndex * 41 + Tick * 17) % 10;
		if(Phase == 0 || Phase == 5)
			return WithAlpha(ColorRGBA(1.0f, 0.96f, 0.22f), Alpha);
		if(Phase == 1)
			return WithAlpha(ColorRGBA(1.0f, 1.0f, 1.0f), Alpha);
		return WithAlpha(ColorRGBA(0.14f, 0.70f, 1.0f), Alpha);
	}
	case 9:
	{
		const int Phase = (LetterIndex * 23 + Tick * 4) % 120;
		if(Phase < 40)
			return MixColors(ColorRGBA(0.30f, 0.15f, 1.0f, Alpha), ColorRGBA(0.88f, 0.22f, 1.0f, Alpha), Phase / 40.0f);
		if(Phase < 80)
			return MixColors(ColorRGBA(0.88f, 0.22f, 1.0f, Alpha), ColorRGBA(0.10f, 0.86f, 1.0f, Alpha), (Phase - 40) / 40.0f);
		return MixColors(ColorRGBA(0.10f, 0.86f, 1.0f, Alpha), ColorRGBA(0.30f, 0.15f, 1.0f, Alpha), (Phase - 80) / 40.0f);
	}
	case 10:
	{
		const float Shine = Wave01(Tick * 6 - LetterIndex * 15, 90);
		return MixColors(ColorRGBA(1.0f, 0.58f, 0.04f, Alpha), ColorRGBA(1.0f, 1.0f, 0.72f, Alpha), Shine);
	}
	case 11:
		return Primary;
	case 12:
	{
		const float Pulse = Wave01(Tick * 4, 80);
		return MixColors(Primary, Accent, 0.20f + 0.70f * Pulse);
	}
	case 13:
	{
		const int Phase = (LetterIndex + Tick / 3) % 4;
		if(Phase == 0)
			return WithAlpha(ColorRGBA(0.08f, 1.0f, 0.42f), Alpha);
		if(Phase == 1)
			return Primary;
		if(Phase == 2)
			return WithAlpha(ColorRGBA(0.0f, 0.95f, 1.0f), Alpha);
		return Accent;
	}
	default:
		break;
	}

	if(Moving)
	{
		const int Phase = (LetterIndex * 37 + Tick * 9) % 120;
		const float Mix = Phase < 60 ? Phase / 60.0f : (120 - Phase) / 60.0f;
		return MixColors(Primary, Accent, Mix);
	}
	return Primary;
}
} // namespace MaNameEffects

#endif