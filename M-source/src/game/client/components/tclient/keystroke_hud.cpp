#include "keystroke_hud.h"

#include <base/str.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/hud_layout.h>
#include <game/client/gameclient.h>
#include <game/client/render.h>

#include <algorithm>

namespace
{
ColorRGBA AlphaScale(ColorRGBA Color, float Factor)
{
	Color.a = std::clamp(Color.a * Factor, 0.0f, 1.0f);
	return Color;
}

ColorRGBA Brighten(ColorRGBA Color, float Amount)
{
	Color.r = std::clamp(Color.r + Amount, 0.0f, 1.0f);
	Color.g = std::clamp(Color.g + Amount, 0.0f, 1.0f);
	Color.b = std::clamp(Color.b + Amount, 0.0f, 1.0f);
	return Color;
}

bool DrawCustomKeystrokeModel(IGraphics *pGraphics, const IGraphics::CTextureHandle &Texture, float X, float Y, float W, float H, ColorRGBA Color)
{
	if(!Texture.IsValid() || Texture.IsNullTexture() || W <= 0.0f || H <= 0.0f || Color.a <= 0.0f)
		return false;

	pGraphics->WrapClamp();
	pGraphics->TextureSet(Texture);
	pGraphics->QuadsSetSubset(0, 0, 1, 1);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(Color);
	IGraphics::CQuadItem QuadItem(X, Y, W, H);
	pGraphics->QuadsDrawTL(&QuadItem, 1);
	pGraphics->QuadsEnd();
	pGraphics->WrapNormal();
	return true;
}

void DrawKeystrokeModel(IGraphics *pGraphics, float X, float Y, float W, float H, ColorRGBA Color, int Style, float Rounding, float Scale, const IGraphics::CTextureHandle &CustomTexture = IGraphics::CTextureHandle())
{
	const int SafeStyle = std::clamp(Style, 0, 4);
	if(SafeStyle == 4 && DrawCustomKeystrokeModel(pGraphics, CustomTexture, X, Y, W, H, Color))
		return;

	const int BuiltInStyle = SafeStyle == 4 ? 0 : SafeStyle;
	const float Thin = std::max(1.0f, 1.35f * Scale);
	const float Glow = std::max(2.0f, 3.0f * Scale);

	if(BuiltInStyle == 0)
	{
		pGraphics->TextureClear();
		pGraphics->DrawRect(X, Y, W, H, Color, IGraphics::CORNER_ALL, Rounding);
		return;
	}

	if(BuiltInStyle == 1)
	{
		auto DrawCircle = [&](float CenterX, float CenterY, float Radius, ColorRGBA DrawColor) {
			pGraphics->TextureClear();
			pGraphics->QuadsBegin();
			pGraphics->SetColor(DrawColor);
			pGraphics->DrawCircle(CenterX, CenterY, Radius, 32);
			pGraphics->QuadsEnd();
		};
		auto DrawCapsule = [&](float DrawX, float DrawY, float DrawW, float DrawH, ColorRGBA DrawColor) {
			const float Radius = std::min(DrawW, DrawH) * 0.5f;
			if(DrawW > DrawH)
				pGraphics->DrawRect(DrawX + Radius, DrawY, std::max(0.0f, DrawW - Radius * 2.0f), DrawH, DrawColor, IGraphics::CORNER_NONE, 0.0f);
			DrawCircle(DrawX + Radius, DrawY + DrawH * 0.5f, Radius, DrawColor);
			DrawCircle(DrawX + DrawW - Radius, DrawY + DrawH * 0.5f, Radius, DrawColor);
		};
		DrawCapsule(X - Glow, Y - Glow, W + Glow * 2.0f, H + Glow * 2.0f, AlphaScale(Brighten(Color, 0.32f), 0.22f));
		DrawCapsule(X, Y, W, H, AlphaScale(Color, 0.92f));
		DrawCapsule(X + W * 0.25f, Y + H * 0.16f, W * 0.34f, H * 0.16f, ColorRGBA(1.0f, 1.0f, 1.0f, Color.a * 0.26f));
		return;
	}

	if(BuiltInStyle == 2)
	{
		auto DrawDiamond = [&](float DrawX, float DrawY, float DrawW, float DrawH, ColorRGBA DrawColor) {
			const float CenterX = DrawX + DrawW * 0.5f;
			const float CenterY = DrawY + DrawH * 0.5f;
			const IGraphics::CFreeformItem Item(
				CenterX, DrawY,
				DrawX, CenterY,
				DrawW + DrawX, CenterY,
				CenterX, DrawY + DrawH);
			pGraphics->TextureClear();
			pGraphics->QuadsBegin();
			pGraphics->SetColor(DrawColor);
			pGraphics->QuadsDrawFreeform(&Item, 1);
			pGraphics->QuadsEnd();
		};
		DrawDiamond(X + Thin, Y + Thin, W, H, ColorRGBA(0.0f, 0.0f, 0.0f, Color.a * 0.26f));
		DrawDiamond(X, Y, W, H, AlphaScale(Brighten(Color, 0.10f), 0.84f));
		DrawDiamond(X + W * 0.28f, Y + H * 0.13f, W * 0.44f, H * 0.30f, ColorRGBA(1.0f, 1.0f, 1.0f, Color.a * 0.18f));
		return;
	}

	auto DrawHexagon = [&](float DrawX, float DrawY, float DrawW, float DrawH, ColorRGBA DrawColor) {
		const float Cut = std::min(DrawW, DrawH) * 0.28f;
		const vec2 aPoints[] = {
			vec2(DrawX + Cut, DrawY),
			vec2(DrawX + DrawW - Cut, DrawY),
			vec2(DrawX + DrawW, DrawY + DrawH * 0.5f),
			vec2(DrawX + DrawW - Cut, DrawY + DrawH),
			vec2(DrawX + Cut, DrawY + DrawH),
			vec2(DrawX, DrawY + DrawH * 0.5f),
		};
		IGraphics::CFreeformItem aItems[6];
		const vec2 Center(DrawX + DrawW * 0.5f, DrawY + DrawH * 0.5f);
		for(int i = 0; i < 6; ++i)
			aItems[i] = IGraphics::CFreeformItem(Center, aPoints[i], aPoints[(i + 1) % 6], aPoints[(i + 1) % 6]);
		pGraphics->TextureClear();
		pGraphics->QuadsBegin();
		pGraphics->SetColor(DrawColor);
		pGraphics->QuadsDrawFreeform(aItems, 6);
		pGraphics->QuadsEnd();
	};
	DrawHexagon(X, Y, W, H, AlphaScale(Brighten(Color, 0.45f), 0.92f));
	DrawHexagon(X + Thin, Y + Thin, W - Thin * 2.0f, H - Thin * 2.0f, AlphaScale(Color, 0.42f));
	pGraphics->DrawRect(X + W * 0.22f, Y + H * 0.46f, W * 0.56f, std::max(1.0f, Thin * 0.75f), AlphaScale(Brighten(Color, 0.6f), 0.65f), IGraphics::CORNER_NONE, 0.0f);
}

ColorRGBA HudColor(unsigned ColorConfig, int AlphaPercent)
{
	return color_cast<ColorRGBA>(ColorHSLA(ColorConfig)).WithAlpha(std::clamp(AlphaPercent, 0, 100) / 100.0f);
}

void DrawCenteredLabel(ITextRender *pTextRender, float X, float Y, float W, float H, const char *pText, float FontSize, float Alpha)
{
	pTextRender->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, Alpha));
	const float TextW = pTextRender->TextWidth(FontSize, pText);
	pTextRender->Text(X + W * 0.5f - TextW * 0.5f, Y + H * 0.5f - FontSize * 0.5f, FontSize, pText);
}
} // namespace

void CKeystrokeHud::ReloadCustomTextures()
{
	m_CustomTexturesDirty = true;
}

void CKeystrokeHud::EnsureCustomTexturesLoaded()
{
	if(!m_CustomTexturesDirty && str_comp(m_aLoadedCustomPack, g_Config.m_TcKeystrokeHudCustomPack) == 0)
		return;
	m_CustomTexturesDirty = false;

	if(m_CustomKeyTexture.IsValid())
		Graphics()->UnloadTexture(&m_CustomKeyTexture);
	if(m_CustomSpaceTexture.IsValid())
		Graphics()->UnloadTexture(&m_CustomSpaceTexture);
	if(m_CustomMouseTexture.IsValid())
		Graphics()->UnloadTexture(&m_CustomMouseTexture);

	m_CustomKeyTexture = IGraphics::CTextureHandle();
	m_CustomSpaceTexture = IGraphics::CTextureHandle();
	m_CustomMouseTexture = IGraphics::CTextureHandle();
	str_copy(m_aLoadedCustomPack, g_Config.m_TcKeystrokeHudCustomPack, sizeof(m_aLoadedCustomPack));

	if(g_Config.m_TcKeystrokeHudCustomPack[0] == '\0')
		return;

	char aPath[256];
	str_format(aPath, sizeof(aPath), "ma/keystrokes/%s/key.png", g_Config.m_TcKeystrokeHudCustomPack);
	m_CustomKeyTexture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
	str_format(aPath, sizeof(aPath), "ma/keystrokes/%s/space.png", g_Config.m_TcKeystrokeHudCustomPack);
	m_CustomSpaceTexture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
	str_format(aPath, sizeof(aPath), "ma/keystrokes/%s/mouse.png", g_Config.m_TcKeystrokeHudCustomPack);
	m_CustomMouseTexture = Graphics()->LoadTexture(aPath, IStorage::TYPE_ALL);
}

void CKeystrokeHud::OnInit()
{
	Storage()->CreateFolder("ma", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ma/keystrokes", IStorage::TYPE_SAVE);
}

void CKeystrokeHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!g_Config.m_TcKeystrokeHud)
		return;

	EnsureCustomTexturesLoaded();

	const bool PressedA = Input()->KeyIsPressed(KEY_A);
	const bool PressedD = Input()->KeyIsPressed(KEY_D);
	const bool PressedSpace = Input()->KeyIsPressed(KEY_SPACE);
	const bool PressedMouse1 = Input()->KeyIsPressed(KEY_MOUSE_1);
	const bool PressedMouse2 = Input()->KeyIsPressed(KEY_MOUSE_2);

	const bool ShowKeyboard = !g_Config.m_TcKeystrokeHudOnlyOnPress || PressedA || PressedD;
	const bool ShowSpace = g_Config.m_TcKeystrokeHudShowSpace && (!g_Config.m_TcKeystrokeHudOnlyOnPress || PressedSpace);
	const bool ShowMouse = g_Config.m_TcKeystrokeHudShowMouse &&
				 (!g_Config.m_TcKeystrokeHudOnlyOnPress || PressedMouse1 || PressedMouse2);

	if(!ShowKeyboard && !ShowSpace && !ShowMouse)
		return;

	float ScreenW = 300.0f * Graphics()->ScreenAspect();
	float ScreenH = 300.0f;
	const auto KeyLayout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES_KEYBOARD, ScreenW, ScreenH);
	const auto SpaceLayout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES_SPACE, ScreenW, ScreenH);
	const auto MouseLayout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES_MOUSE, ScreenW, ScreenH);

	const float KeyScale = g_Config.m_TcKeystrokeHudSize / 100.0f;
	const float SpaceScale = g_Config.m_TcKeystrokeHudSpaceSize / 100.0f;
	const float MouseScale = g_Config.m_TcKeystrokeHudMouseSize / 100.0f;
	const float KeyWidthStretch = std::clamp(KeyLayout.m_WidthScale / 100.0f, 0.20f, 4.0f);
	const float KeyHeightStretch = std::clamp(KeyLayout.m_HeightScale / 100.0f, 0.20f, 4.0f);
	const float SpaceWidthStretch = std::clamp(SpaceLayout.m_WidthScale / 100.0f, 0.20f, 4.0f);
	const float SpaceHeightStretch = std::clamp(SpaceLayout.m_HeightScale / 100.0f, 0.20f, 4.0f);
	const float MouseWidthStretch = std::clamp(MouseLayout.m_WidthScale / 100.0f, 0.20f, 4.0f);
	const float MouseHeightStretch = std::clamp(MouseLayout.m_HeightScale / 100.0f, 0.20f, 4.0f);

	const float KeyW = 40.0f * KeyScale * KeyWidthStretch;
	const float KeyH = 40.0f * KeyScale * KeyHeightStretch;
	const float Gap = 6.0f * KeyScale * KeyWidthStretch;
	const float KeyTotalW = KeyW * 2.0f + Gap;
	const float KeyTotalH = KeyH;

	const float SpaceW = 86.0f * SpaceScale * SpaceWidthStretch;
	const float SpaceH = 22.0f * SpaceScale * SpaceHeightStretch;

	const float MouseW = 40.0f * MouseScale * MouseWidthStretch;
	const float MouseH = 40.0f * MouseScale * MouseHeightStretch;
	const float MouseGap = 6.0f * MouseScale * MouseWidthStretch;
	const float MouseTotalW = MouseW * 2.0f + MouseGap;
	const float MouseTotalH = MouseH;

	float KeyPosX = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_KEYBOARD) ? KeyLayout.m_X : ScreenW * (g_Config.m_TcKeystrokeHudPosX / 100.0f);
	float KeyPosY = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_KEYBOARD) ? KeyLayout.m_Y : ScreenH * (g_Config.m_TcKeystrokeHudPosY / 100.0f);
	float SpacePosX = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_SPACE) ? SpaceLayout.m_X : ScreenW * (g_Config.m_TcKeystrokeHudSpacePosX / 100.0f);
	float SpacePosY = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_SPACE) ? SpaceLayout.m_Y : ScreenH * (g_Config.m_TcKeystrokeHudSpacePosY / 100.0f);
	float MousePosX = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_MOUSE) ? MouseLayout.m_X : ScreenW * (g_Config.m_TcKeystrokeHudMousePosX / 100.0f);
	float MousePosY = HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_MOUSE) ? MouseLayout.m_Y : ScreenH * (g_Config.m_TcKeystrokeHudMousePosY / 100.0f);

	const ColorRGBA KeyPressed = HudColor(g_Config.m_TcKeystrokeHudColorPressed, g_Config.m_TcKeystrokeHudAlpha);
	const ColorRGBA KeyUnpressed = HudColor(g_Config.m_TcKeystrokeHudColorUnpressed, g_Config.m_TcKeystrokeHudAlpha);
	const ColorRGBA SpacePressed = HudColor(g_Config.m_TcKeystrokeHudSpaceColorPressed, g_Config.m_TcKeystrokeHudSpaceAlpha);
	const ColorRGBA SpaceUnpressed = HudColor(g_Config.m_TcKeystrokeHudSpaceColorUnpressed, g_Config.m_TcKeystrokeHudSpaceAlpha);
	const ColorRGBA MousePressed = HudColor(g_Config.m_TcKeystrokeHudMouseColorPressed, g_Config.m_TcKeystrokeHudMouseAlpha);
	const ColorRGBA MouseUnpressed = HudColor(g_Config.m_TcKeystrokeHudMouseColorUnpressed, g_Config.m_TcKeystrokeHudMouseAlpha);

	const float KeyRounding = 4.0f * KeyScale;
	const float SpaceRounding = 4.0f * SpaceScale;
	const float MouseRounding = 4.0f * MouseScale;

	const bool EditMode = g_Config.m_TcKeystrokeHudEditMode && !GameClient()->m_HudEditor.IsActive();
	if(!EditMode)
		m_EditDragging = false;
	if(EditMode)
	{
		vec2 NativeMouse = Input()->NativeMousePos();
		float VMX = NativeMouse.x / (float)Graphics()->ScreenWidth() * ScreenW;
		float VMY = NativeMouse.y / (float)Graphics()->ScreenHeight() * ScreenH;

		const bool MouseOverKeys = ShowKeyboard && VMX >= KeyPosX && VMX <= KeyPosX + KeyTotalW && VMY >= KeyPosY && VMY <= KeyPosY + KeyTotalH;
		const bool MouseOverSpace = ShowSpace && VMX >= SpacePosX && VMX <= SpacePosX + SpaceW && VMY >= SpacePosY && VMY <= SpacePosY + SpaceH;
		const bool MouseOverMouseBtns = ShowMouse && VMX >= MousePosX && VMX <= MousePosX + MouseTotalW && VMY >= MousePosY && VMY <= MousePosY + MouseTotalH;
		const bool MouseOverAny = MouseOverKeys || MouseOverSpace || MouseOverMouseBtns;
		const bool MouseDown = Input()->KeyIsPressed(KEY_MOUSE_1);

		if(MouseDown && MouseOverAny && !m_EditDragging)
		{
			m_EditDragging = true;
			if(MouseOverMouseBtns)
			{
				m_EditDragTarget = EDIT_DRAG_MOUSE;
				m_EditDragOffsetX = VMX - MousePosX;
				m_EditDragOffsetY = VMY - MousePosY;
			}
			else if(MouseOverSpace)
			{
				m_EditDragTarget = EDIT_DRAG_SPACE;
				m_EditDragOffsetX = VMX - SpacePosX;
				m_EditDragOffsetY = VMY - SpacePosY;
			}
			else
			{
				m_EditDragTarget = EDIT_DRAG_KEYS;
				m_EditDragOffsetX = VMX - KeyPosX;
				m_EditDragOffsetY = VMY - KeyPosY;
			}
		}

		if(m_EditDragging && MouseDown)
		{
			int NewX = (int)((VMX - m_EditDragOffsetX) / ScreenW * 100.0f);
			int NewY = (int)((VMY - m_EditDragOffsetY) / ScreenH * 100.0f);
			NewX = std::clamp(NewX, 0, 100);
			NewY = std::clamp(NewY, 0, 100);
			if(m_EditDragTarget == EDIT_DRAG_KEYS)
			{
				g_Config.m_TcKeystrokeHudPosX = NewX;
				g_Config.m_TcKeystrokeHudPosY = NewY;
				KeyPosX = ScreenW * (NewX / 100.0f);
				KeyPosY = ScreenH * (NewY / 100.0f);
			}
			else if(m_EditDragTarget == EDIT_DRAG_SPACE)
			{
				g_Config.m_TcKeystrokeHudSpacePosX = NewX;
				g_Config.m_TcKeystrokeHudSpacePosY = NewY;
				SpacePosX = ScreenW * (NewX / 100.0f);
				SpacePosY = ScreenH * (NewY / 100.0f);
			}
			else if(m_EditDragTarget == EDIT_DRAG_MOUSE)
			{
				g_Config.m_TcKeystrokeHudMousePosX = NewX;
				g_Config.m_TcKeystrokeHudMousePosY = NewY;
				MousePosX = ScreenW * (NewX / 100.0f);
				MousePosY = ScreenH * (NewY / 100.0f);
			}
		}
		else if(!MouseDown)
		{
			m_EditDragging = false;
		}
	}

	Graphics()->MapScreen(0.0f, 0.0f, ScreenW, ScreenH);

	if(ShowKeyboard)
	{
		DrawKeystrokeModel(Graphics(), KeyPosX, KeyPosY, KeyW, KeyH, PressedA ? KeyPressed : KeyUnpressed, g_Config.m_TcKeystrokeHudStyle, KeyRounding, KeyScale, m_CustomKeyTexture);
		DrawKeystrokeModel(Graphics(), KeyPosX + KeyW + Gap, KeyPosY, KeyW, KeyH, PressedD ? KeyPressed : KeyUnpressed, g_Config.m_TcKeystrokeHudStyle, KeyRounding, KeyScale, m_CustomKeyTexture);
		if(g_Config.m_TcKeystrokeHudShowText)
		{
			const float TextSize = KeyH * 0.5f;
			DrawCenteredLabel(TextRender(), KeyPosX, KeyPosY, KeyW, KeyH, "A", TextSize, g_Config.m_TcKeystrokeHudAlpha / 100.0f);
			DrawCenteredLabel(TextRender(), KeyPosX + KeyW + Gap, KeyPosY, KeyW, KeyH, "D", TextSize, g_Config.m_TcKeystrokeHudAlpha / 100.0f);
		}
	}

	if(ShowSpace)
	{
		DrawKeystrokeModel(Graphics(), SpacePosX, SpacePosY, SpaceW, SpaceH, PressedSpace ? SpacePressed : SpaceUnpressed, g_Config.m_TcKeystrokeHudSpaceStyle, SpaceRounding, SpaceScale, m_CustomSpaceTexture);
		if(g_Config.m_TcKeystrokeHudShowText)
			DrawCenteredLabel(TextRender(), SpacePosX, SpacePosY, SpaceW, SpaceH, "SPACE", SpaceH * 0.5f, g_Config.m_TcKeystrokeHudSpaceAlpha / 100.0f);
	}

	if(ShowMouse)
	{
		DrawKeystrokeModel(Graphics(), MousePosX, MousePosY, MouseW, MouseH, PressedMouse1 ? MousePressed : MouseUnpressed, g_Config.m_TcKeystrokeHudMouseStyle, MouseRounding, MouseScale, m_CustomMouseTexture);
		DrawKeystrokeModel(Graphics(), MousePosX + MouseW + MouseGap, MousePosY, MouseW, MouseH, PressedMouse2 ? MousePressed : MouseUnpressed, g_Config.m_TcKeystrokeHudMouseStyle, MouseRounding, MouseScale, m_CustomMouseTexture);
		if(g_Config.m_TcKeystrokeHudShowText)
		{
			const float MouseTextSize = MouseH * 0.45f;
			DrawCenteredLabel(TextRender(), MousePosX, MousePosY, MouseW, MouseH, "LMB", MouseTextSize, g_Config.m_TcKeystrokeHudMouseAlpha / 100.0f);
			DrawCenteredLabel(TextRender(), MousePosX + MouseW + MouseGap, MousePosY, MouseW, MouseH, "RMB", MouseTextSize, g_Config.m_TcKeystrokeHudMouseAlpha / 100.0f);
		}
	}

	if(EditMode)
	{
		vec2 NM = Input()->NativeMousePos();
		float VX = NM.x / (float)Graphics()->ScreenWidth() * ScreenW;
		float VY = NM.y / (float)Graphics()->ScreenHeight() * ScreenH;
		auto DrawEditOutline = [&](float X, float Y, float W, float H, float Scale, float Rounding) {
			const float BorderW = 2.5f * Scale;
			const bool Over = VX >= X && VX <= X + W && VY >= Y && VY <= Y + H;
			ColorRGBA Col = Over ? ColorRGBA(1.0f, 0.84f, 0.0f, 0.9f) : ColorRGBA(1.0f, 0.84f, 0.0f, 0.4f);
			Graphics()->DrawRect(X - BorderW, Y - BorderW, W + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_T, Rounding);
			Graphics()->DrawRect(X - BorderW, Y + H, W + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_B, Rounding);
			Graphics()->DrawRect(X - BorderW, Y, BorderW, H, Col, IGraphics::CORNER_L, 0.0f);
			Graphics()->DrawRect(X + W, Y, BorderW, H, Col, IGraphics::CORNER_R, 0.0f);
		};
		if(ShowKeyboard)
			DrawEditOutline(KeyPosX, KeyPosY, KeyTotalW, KeyTotalH, KeyScale, KeyRounding);
		if(ShowSpace)
			DrawEditOutline(SpacePosX, SpacePosY, SpaceW, SpaceH, SpaceScale, SpaceRounding);
		if(ShowMouse)
			DrawEditOutline(MousePosX, MousePosY, MouseTotalW, MouseTotalH, MouseScale, MouseRounding);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
}