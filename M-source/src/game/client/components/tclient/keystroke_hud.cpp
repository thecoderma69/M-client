#include "keystroke_hud.h"

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/client/render.h>
#include <game/client/components/hud_layout.h>

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

	float Scale = g_Config.m_TcKeystrokeHudSize / 100.0f;
	float KeyW = 40.0f * Scale;
	float KeyH = 40.0f * Scale;
	float Gap = 6.0f * Scale;
	float SpaceH = 22.0f * Scale;

	float KeyTotalW = KeyW * 2.0f + Gap;
	float KeyTotalH = KeyH + Gap + SpaceH;

	float MouseW = KeyW;
	float MouseH = KeyH;
	float MouseGap = Gap;
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

	float Rounding = 4.0f * Scale;
	int Corners = IGraphics::CORNER_ALL;

	bool EditMode = g_Config.m_TcKeystrokeHudEditMode;
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
		Graphics()->DrawRect(KeyPosX, KeyPosY, KeyW, KeyH, PressedA ? ColorPressed : ColorUnpressed, Corners, Rounding);
		Graphics()->DrawRect(KeyPosX + KeyW + Gap, KeyPosY, KeyW, KeyH, PressedD ? ColorPressed : ColorUnpressed, Corners, Rounding);
		Graphics()->DrawRect(KeyPosX, KeyPosY + KeyH + Gap, KeyTotalW, SpaceH, PressedSpace ? ColorPressed : ColorUnpressed, Corners, Rounding);

		if(g_Config.m_TcKeystrokeHudShowText)
		{
			TextRender()->TextColor(ColorRGBA(1.0f, 1.0f, 1.0f, g_Config.m_TcKeystrokeHudAlpha / 100.0f));
			float TextSize = KeyH * 0.5f;

			static float s_CachedScale = 0.0f;
			static float s_TextWidthA = 0.0f;
			static float s_TextWidthD = 0.0f;
			static float s_TextWidthSpace = 0.0f;
			static float s_CachedSpaceTextSize = 0.0f;

			if(Scale != s_CachedScale)
			{
				s_CachedScale = Scale;
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
		Graphics()->DrawRect(MousePosX, MousePosY, MouseW, MouseH, PressedMouse1 ? ColorPressed : ColorUnpressed, Corners, Rounding);
		Graphics()->DrawRect(MousePosX + MouseW + MouseGap, MousePosY, MouseW, MouseH, PressedMouse2 ? ColorPressed : ColorUnpressed, Corners, Rounding);

		if(g_Config.m_TcKeystrokeHudShowText)
		{
			static float s_CachedScaleM = 0.0f;
			static float s_TextWidthM1 = 0.0f;
			static float s_TextWidthM2 = 0.0f;
			static float s_CachedMouseTextSize = 0.0f;

			float MouseTextSize = MouseH * 0.45f;

			if(Scale != s_CachedScaleM)
			{
				s_CachedScaleM = Scale;
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
		float BorderW = 2.5f * Scale;

		if(ShowKeys)
		{
			vec2 NM = Input()->NativeMousePos();
			float VX = NM.x / (float)Graphics()->ScreenWidth() * ScreenW;
			float VY = NM.y / (float)Graphics()->ScreenHeight() * ScreenH;
			bool Over = VX >= KeyPosX && VX <= KeyPosX + KeyTotalW && VY >= KeyPosY && VY <= KeyPosY + KeyTotalH;
			ColorRGBA Col = Over ? ColorRGBA(1.0f, 0.84f, 0.0f, 0.9f) : ColorRGBA(1.0f, 0.84f, 0.0f, 0.4f);

			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY - BorderW, KeyTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_T, Rounding);
			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY + KeyTotalH, KeyTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_B, Rounding);
			Graphics()->DrawRect(KeyPosX - BorderW, KeyPosY, BorderW, KeyTotalH, Col, IGraphics::CORNER_L, 0.0f);
			Graphics()->DrawRect(KeyPosX + KeyTotalW, KeyPosY, BorderW, KeyTotalH, Col, IGraphics::CORNER_R, 0.0f);
		}

		if(ShowMouse)
		{
			vec2 NM = Input()->NativeMousePos();
			float VX = NM.x / (float)Graphics()->ScreenWidth() * ScreenW;
			float VY = NM.y / (float)Graphics()->ScreenHeight() * ScreenH;
			bool Over = VX >= MousePosX && VX <= MousePosX + MouseTotalW && VY >= MousePosY && VY <= MousePosY + MouseTotalH;
			ColorRGBA Col = Over ? ColorRGBA(1.0f, 0.84f, 0.0f, 0.9f) : ColorRGBA(1.0f, 0.84f, 0.0f, 0.4f);

			Graphics()->DrawRect(MousePosX - BorderW, MousePosY - BorderW, MouseTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_T, Rounding);
			Graphics()->DrawRect(MousePosX - BorderW, MousePosY + MouseTotalH, MouseTotalW + BorderW * 2.0f, BorderW, Col, IGraphics::CORNER_B, Rounding);
			Graphics()->DrawRect(MousePosX - BorderW, MousePosY, BorderW, MouseTotalH, Col, IGraphics::CORNER_L, 0.0f);
			Graphics()->DrawRect(MousePosX + MouseTotalW, MousePosY, BorderW, MouseTotalH, Col, IGraphics::CORNER_R, 0.0f);
		}
	}
}
