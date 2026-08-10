#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_KEYSTROKE_HUD_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_KEYSTROKE_HUD_H

#include <engine/graphics.h>

#include <game/client/component.h>

enum
{
	EDIT_DRAG_KEYS = 0,
	EDIT_DRAG_SPACE,
	EDIT_DRAG_MOUSE,
};

class CKeystrokeHud : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnRender() override;
	void OnInit() override;
	void ReloadCustomTextures();

private:
	char m_aLoadedCustomPack[64] = "";
	IGraphics::CTextureHandle m_CustomKeyTexture;
	IGraphics::CTextureHandle m_CustomSpaceTexture;
	IGraphics::CTextureHandle m_CustomMouseTexture;
	bool m_CustomTexturesDirty = true;
	void EnsureCustomTexturesLoaded();

	bool m_EditDragging = false;
	int m_EditDragTarget = EDIT_DRAG_KEYS;
	float m_EditDragOffsetX = 0.0f;
	float m_EditDragOffsetY = 0.0f;
};

#endif
