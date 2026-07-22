#include "keystroke_hud.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/components/hud_layout.h>

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

void DrawKeystrokeModel(IGraphics *pGraphics, float X, float Y, float W, float H, ColorRGBA Color, int Style, float Rounding, float Scale)
{
	const int SafeStyle = std::clamp(Style, 0, 3);
	const float Thin = std::max(1.0f, 1.35f * Scale);
	const float Glow = std::max(2.0f, 3.0f * Scale);

	if(SafeStyle == 0)
	{
		pGraphics->DrawRect(X, Y, W, H, Color, IGraphics::CORNER_ALL, Rounding);
		return;
	}

	if(SafeStyle == 1)
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

	if(SafeStyle == 2)
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
} // namespace

void CKeystrokeHud::OnInit()
{
}

void CKeystrokeHud::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	if(!g_Config.m_TcKeystrokeHud)
		return;

	bool PressedA = Input()->KeyIsPressed(KEY_A);
	bool PressedD = Input()->KeyIsPressed(KEY_D);
	bool PressedSpace = Input()->KeyIsPressed(KEY_SPACE);
	bool PressedMouse1 = Input()->KeyIsPressed(KEY_MOUSE_1);
	bool PressedMouse2 = Input()->KeyIsPressed(KEY_MOUSE_2);

	bool ShowKeys = !g_Config.m_TcKeystrokeHudOnlyOnPress || PressedA || PressedD || PressedSpace;
	bool ShowMouse = g_Config.m_TcKeystrokeHudShowMouse &&
			 (!g_Config.m_TcKeystrokeHudOnlyOnPress || PressedMouse1 || PressedMouse2);

	if(!ShowKeys && !ShowMouse)
		return;

	float KeyScale = g_Config.m_TcKeystrokeHudSize / 100.0f;
	float MouseScale = g_Config.m_TcKeystrokeHudMouseSize / 100.0f;
	float KeyW = 40.0f * KeyScale;
	float KeyH = 40.0f * KeyScale;
	float Gap = 6.0f * KeyScale;
	float SpaceH = 22.0f * KeyScale;

	float KeyTotalW = KeyW * 2.0f + Gap;
	float KeyTotalH = KeyH + Gap + SpaceH;

	float MouseW = 40.0f * MouseScale;
	float MouseH = 40.0f * MouseScale;
	float MouseGap = 6.0f * MouseScale;
	float MouseTotalW = MouseW * 2.0f + MouseGap;
	float MouseTotalH = MouseH;

	float ScreenW = 300.0f * Graphics()->ScreenAspect();
	float ScreenH = 300.0f;

	float KeyPosX;
	float KeyPosY;
	if(HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_KEYBOARD))
	{
		const auto KeyLayout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES_KEYBOARD, ScreenW, ScreenH);
		KeyPosX = KeyLayout.m_X;
		KeyPosY = KeyLayout.m_Y;
	}
	else
	{
		KeyPosX = ScreenW * (g_Config.m_TcKeystrokeHudPosX / 100.0f);
		KeyPosY = ScreenH * (g_Config.m_TcKeystrokeHudPosY / 100.0f);
	}

	float MousePosX;
	float MousePosY;
	if(HudLayout::HasRuntimeOverride(HudLayout::MODULE_KEYSTROKES_MOUSE))
	{
		const auto MouseLayout = HudLayout::Get(HudLayout::MODULE_KEYSTROKES_MOUSE, ScreenW, ScreenH);
		MousePosX = MouseLayout.m_X;
		MousePosY = MouseLayout.m_Y;
	}
	else
	{
		MousePosX = ScreenW * (g_Config.m_TcKeystrokeHudMousePosX / 100.0f);
		MousePosY = ScreenH * (g_Config.m_TcKeystrokeHudMousePosY / 100.0f);
	}

	ColorRGBA ColorPressed = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcKeystrokeHudColorPressed))
					 .WithAlpha(g_Config.m_TcKeystrokeHudAlpha / 100.0f);
	ColorRGBA ColorUnpressed = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_TcKeystrokeHudColorUnpressed))
					   .WithAlpha(g_Config.m_TcKeystrokeHudAlpha / 100.0f);

	float KeyRounding = 4.0f * KeyScale;
	float MouseRounding = 4.0f * MouseScale;

	const bool EditMode = g_Config.m_TcKeystrokeHudEditMode && !GameClient()->m_HudEditor.IsActive();
	if(!EditMode)
		m_EditDragging = false;
	if(EditMode)
	{
		vec2 NativeMouse = Input()->NativeMousePos();
		float ActualW = (float)Graphics()->ScreenWidth();
		float ActualH = (float)Graphics()->ScreenHeight();
		float VMX = NativeMouse.x / ActualW * ScreenW;
		float VMY = NativeMouse.y / ActualH * ScreenH;

		bool MouseOverKeys = ShowKeys &&
				     VMX >= KeyPosX && VMX <= KeyPosX + KeyTotalW &&
				     VMY >= KeyPosY && VMY <= KeyPosY + KeyTotalH;

		bool MouseOverMouseBtns = ShowMouse &&
					  VMX >= MousePosX && VMX <= MousePosX + MouseTotalW &&
					  VMY >= MousePosY && VMY <= MousePosY + MouseTotalH;

		bool MouseOverAny = MouseOverKeys || MouseOverMouseBtns;
		bool MouseDown = Input()->KeyIsPressed(KEY_MOUSE_1);

		if(MouseDown && MouseOverAny && !m_EditDragging)
		{
			m_EditDragging = true;
			if(MouseOverMouseBtns)
			{
				m_EditDragTarget = EDIT_DRAG_MOUSE;
				m_EditDragOffsetX = VMX - MousePosX;
				m_EditDragOffsetY = VMY - MousePosY;
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
			if(m_EditDragTarget == EDIT_DRAG_KEYS)
			{
				int NewX = (int)((VMX - m_EditDragOffsetX) / ScreenW * 100.0f);
				int NewY = (int)((VMY - m_EditDragOffsetY) / ScreenH * 100.0f);
				if(NewX < 0) NewX = 0; if(NewX > 100) NewX = 100;
				if(NewY < 0) NewY = 0; if(NewY > 100) NewY = 100;
				g_Config.m_TcKeystrokeHudPosX = NewX;
				g_Config.m_TcKeystrokeHudPosY = NewY;
				KeyPosX = ScreenW * (NewX / 100.0f);
				KeyPosY = ScreenH * (NewY / 100.0f);
			}
			else if(m_EditDragTarget == EDIT_DRAG_MOUSE)
			{
				int NewX = (int)((VMX - m_EditDragOffsetX) / ScreenW * 100.0f);
				int NewY = (int)((VMY - m_EditDragOffsetY) / ScreenH * 100.0f);
				if(NewX < 0) NewX = 0; if(NewX > 100) NewX = 100;
				if(NewY < 0) NewY = 0; if(NewY > 100) NewY = 100;
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

	if(ShowKeys)
	{
		DrawKeystrokeModel(Graphics(), KeyPosX, KeyPosY, KeyW, KeyH, PressedA ? ColorPressed : ColorUnpressed, g_Config.m_TcKeystrokeHudStyle, KeyRounding, KeyScale);
		DrawKeystrokeModel(Graphics(), KeyPosX + KeyW + Gap, KeyPosY, KeyW, KeyH, PressedD ? ColorPressed : ColorUnpressed, g_Config.m_TcKeystrokeHudStyle, KeyRounding, KeyScale);
		DrawKeystrokeModel(Graphics(), KeyPosX, KeyPosY + KeyH + Gap, KeyTotalW, SpaceH, PressedSpace ? ColorPressed : ColorUnpressed, g_Config.m_TcKeystrokeHudStyle, KeyRounding, KeyScale);

		if(g_Config.m_TcKeystrokeHudShowText)
		{
			TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, g_Config.m_TcKeystrokeHudAlpha / 100.0f));
			float TextSize = KeyH * 0.5f;

			static float s_CachedScale = 0.0f;
			static float s_TextWidthA = 0.0f;
			static float s_TextWidthD = 0.0f;
			static float s_TextWidthSpace = 0.0f;
			static float s_CachedSpaceTextSize = 0.0f;

			if(KeyScale != s_CachedScale)
			{
				s_CachedScale = KeyScale;
				s_TextWidthA = TextRender()->TextWidth(TextSize, "A");
				s_TextWidthD = TextRender()->TextWidth(TextSize, "D");
				float SpaceTextSize = SpaceH * 0.5f;
				s_TextWidthSpace = TextRender()->TextWidth(SpaceTextSize, "SPACE");
				s_CachedSpaceTextSize = SpaceTextSize;
			}

			TextRender()->Text(KeyPosX + KeyW / 2.0f - s_TextWidthA / 2.0f, KeyPosY + KeyH / 2.0f - TextSize / 2.0f, TextSize, "A");
			TextRender()->Text(KeyPosX + KeyW + Gap + KeyW / 2.0f - s_TextWidthD / 2.0f, KeyPosY + KeyH / 2.0f - TextSize / 2.0f, TextSize, "D");

			if(g_Config.m_TcKeystrokeHudShowSpace)
				TextRender()->Text(KeyPosX + KeyTotalW / 2.0f - s_TextWidthSpace / 2.0f, KeyPosY + KeyH + Gap + SpaceH / 2.0f - s_CachedSpaceTextSize / 2.0f, s_CachedSpaceTextSize, "SPACE");
		}
	}

	if(ShowMouse)
	{
		DrawKeystrokeModel(Graphics(), MousePosX, MousePosY, MouseW, MouseH, PressedMouse1 ? ColorPressed : ColorUnpressed, g_Config.m_TcKeystrokeHudMouseStyle, MouseRounding, MouseScale);
		DrawKeystrokeModel(Graphics(), MousePosX + MouseW + MouseGap, MousePosY, MouseW, MouseH, PressedMouse2 ? ColorPressed : ColorUnpressed, g_Config.m_TcKeystrokeHudMouseStyle, MouseRounding, MouseScale);

		if(g_Config.m_TcKeystrokeHudShowText)
		{
			static float s_CachedScaleM = 0.0f;
			static float s_TextWidthM1 = 0.0f;
			static float s_TextWidthM2 = 0.0f;
			static float s_CachedMouseTextSize = 0.0f;

			float MouseTextSize = MouseH * 0.45f;

			if(MouseScale != s_CachedScaleM)
			{
				s_CachedScaleM = MouseScale;
				s_TextWidthM1 = TextRender()->TextWidth(MouseTextSize, "LMB");
				s_TextWidthM2 = TextRender()->TextWidth(MouseTextSize, "RMB");
				s_CachedMouseTextSize = MouseTextSize;
			}

			TextRender()->Text(MousePosX + MouseW / 2.0f - s_TextWidthM1 / 2.0f, MousePosY + MouseH / 2.0f - s_CachedMouseTextSize / 2.0f, s_CachedMouseTextSize, "LMB");
			TextRender()->Text(MousePosX + MouseW + MouseGap + MouseW / 2.0f - s_TextWidthM2 / 2.0f, MousePosY + MouseH / 2.0f - s_CachedMouseTextSize / 2.0f, s_CachedMouseTextSize, "RMB");
		}
	}

	if(EditMode)
	{
		if(ShowKeys)
		{
			float BorderW = 2.5f * KeyScale;
			vec2 NM = Input()->NativeMousePos();
			float VX = NM.x / (float)Graphics()->ScreenWidth() * ScreenW;
			float VY = NM.y / (float)Graphics()->ScreenHeight() * ScreenH;
			bool Over = VX >= KeyPosX && VX <= KeyPosX + KeyTotalW && VY >= KeyPosY && VY <= KeyPosY + KeyTotalH;
			ColorRGBA Col = Over ? ColorRGBA(1.0f, 0.84f, 0.0f, 0.9f) : ColorRGBA(1.0f, 0.84f, 0.0f, 0.4f);

			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY - BorderW, KeyTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_T, KeyRounding);
			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY + KeyTotalH, KeyTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_B, KeyRounding);
			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY, BorderW, KeyTotalH, Col, IGraphics::CORNER_L, 0.0f);
			Graphics()->DrawRect(KeyPosX + KeyTotalW, KeyPosY, BorderW, KeyTotalH, Col, IGraphics::CORNER_R, 0.0f);
		}

		if(ShowMouse)
		{
			float BorderW = 2.5f * MouseScale;
			vec2 NM = Input()->NativeMousePos();
			float VX = NM.x / (float)Graphics()->ScreenWidth() * ScreenW;
			float VY = NM.y / (float)Graphics()->ScreenHeight() * ScreenH;
			bool Over = VX >= MousePosX && VX <= MousePosX + MouseTotalW && VY >= MousePosY && VY <= MousePosY + MouseTotalH;
			ColorRGBA Col = Over ? ColorRGBA(1.0f, 0.84f, 0.0f, 0.9f) : ColorRGBA(1.0f, 0.84f, 0.0f, 0.4f);

			Graphics()->DrawRect(MousePosX - BorderW, MousePosY - BorderW, MouseTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_T, MouseRounding);
			Graphics()->DrawRect(MousePosX - BorderW, MousePosY + MouseTotalH, MouseTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_B, MouseRounding);
			Graphics()->DrawRect(MousePosX - BorderW, MousePosY, BorderW, MouseTotalH, Col, IGraphics::CORNER_L, 0.0f);
			Graphics()->DrawRect(MousePosX + MouseTotalW, MousePosY, BorderW, MouseTotalH, Col, IGraphics::CORNER_R, 0.0f);
		}
	}
}
