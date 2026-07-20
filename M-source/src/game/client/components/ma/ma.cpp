#include "ma.h"

#include "visualizer/service.h"

#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/sound.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <game/client/components/chat.h>
#include <game/client/components/hud_layout.h>
#include <game/client/components/ma/music_player.h>
#include <game/client/gameclient.h>
#include <game/client/components/sounds.h>
#include <game/localization.h>

#if defined(CONF_FAMILY_WINDOWS)
#include <base/windows.h>
#define IStorage WindowsIStorage
#include <objbase.h>
#include <wincodec.h>
#undef IStorage
#endif

#include <array>
#include <algorithm>
#include <cmath>
#include <string>

namespace
{

constexpr float MAX_EFFECT_DELTA = 0.1f;
constexpr int MAX_MUSIC_VIDEO_POINTS = 192;
constexpr float MUSIC_VIDEO_EDITOR_RECT_SCALE = 0.70f;

static float ApproachEffectValue(float Current, float Target, float Delta, float Speed)
{
	const float Amount = std::clamp(Delta * Speed, 0.0f, 1.0f);
	return Current + (Target - Current) * Amount;
}

static float AngleDistance(float A, float B)
{
	float D = A - B;
	while(D > pi)
		D -= 2.0f * pi;
	while(D < -pi)
		D += 2.0f * pi;
	return D;
}

static float GaussianAngle(float Angle, float Center, float Width)
{
	const float D = AngleDistance(Angle, Center);
	return expf(-(D * D) / maximum(0.001f, 2.0f * Width * Width));
}

static float MusicVideoBaseShape(float Angle)
{
	(void)Angle;
	return 1.0f;
}

static float PerformanceLodScale(float Delta)
{
	if(Delta <= 0.0f)
		return 1.0f;
	if(Delta > 1.0f / 180.0f)
		return 0.35f;
	if(Delta > 1.0f / 300.0f)
		return 0.50f;
	if(Delta > 1.0f / 500.0f)
		return 0.68f;
	if(Delta > 1.0f / 750.0f)
		return 0.84f;
	return 1.0f;
}

static float RoundedInset(float LocalX, float Width, float Radius)
{
	if(Radius <= 0.0f)
		return 0.0f;
	if(LocalX < Radius)
	{
		const float DeltaX = Radius - LocalX;
		return Radius - sqrtf(maximum(0.0f, Radius * Radius - DeltaX * DeltaX));
	}
	if(LocalX > Width - Radius)
	{
		const float DeltaX = LocalX - (Width - Radius);
		return Radius - sqrtf(maximum(0.0f, Radius * Radius - DeltaX * DeltaX));
	}
	return 0.0f;
}

static void DrawRoundedTextureCover(IGraphics *pGraphics, IGraphics::CTextureHandle Texture, const CUIRect &Rect, int TextureWidth, int TextureHeight, float Alpha)
{
	if(pGraphics == nullptr || !Texture.IsValid() || Rect.w <= 0.0f || Rect.h <= 0.0f || Alpha <= 0.0f)
		return;

	const float Radius = minimum(Rect.w, Rect.h) * 0.5f;
	constexpr int NumSlices = 40;
	float U0 = 0.0f;
	float U1 = 1.0f;
	float V0 = 0.0f;
	float V1 = 1.0f;
	if(TextureWidth > 0 && TextureHeight > 0)
	{
		const float TextureAspect = TextureWidth / (float)TextureHeight;
		const float RectAspect = Rect.w / maximum(Rect.h, 0.001f);
		if(TextureAspect > RectAspect)
		{
			const float Visible = RectAspect / TextureAspect;
			const float Crop = (1.0f - Visible) * 0.5f;
			U0 = Crop;
			U1 = 1.0f - Crop;
		}
		else if(TextureAspect < RectAspect)
		{
			const float Visible = TextureAspect / RectAspect;
			const float Crop = (1.0f - Visible) * 0.5f;
			V0 = Crop;
			V1 = 1.0f - Crop;
		}
	}

	pGraphics->TextureSet(Texture);
	pGraphics->QuadsBegin();
	pGraphics->SetColor(1.0f, 1.0f, 1.0f, Alpha);
	for(int i = 0; i < NumSlices; ++i)
	{
		const float SliceT0 = i / (float)NumSlices;
		const float SliceT1 = (i + 1) / (float)NumSlices;
		const float LocalX0 = Rect.w * SliceT0;
		const float LocalX1 = Rect.w * SliceT1;
		const float Inset0 = RoundedInset(LocalX0, Rect.w, Radius);
		const float Inset1 = RoundedInset(LocalX1, Rect.w, Radius);
		const float X0 = Rect.x + LocalX0;
		const float X1 = Rect.x + LocalX1;

		const vec2 TopLeft(X0, Rect.y + Inset0);
		const vec2 TopRight(X1, Rect.y + Inset1);
		const vec2 BottomLeft(X0, Rect.y + Rect.h - Inset0);
		const vec2 BottomRight(X1, Rect.y + Rect.h - Inset1);

		const float SliceU0 = mix(U0, U1, SliceT0);
		const float SliceU1 = mix(U0, U1, SliceT1);
		const float RenderV0Top = std::clamp((TopLeft.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Top = std::clamp((TopRight.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV0Bottom = std::clamp((BottomLeft.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float RenderV1Bottom = std::clamp((BottomRight.y - Rect.y) / maximum(Rect.h, 0.001f), 0.0f, 1.0f);
		const float SliceV0Top = mix(V0, V1, RenderV0Top);
		const float SliceV1Top = mix(V0, V1, RenderV1Top);
		const float SliceV0Bottom = mix(V0, V1, RenderV0Bottom);
		const float SliceV1Bottom = mix(V0, V1, RenderV1Bottom);

		pGraphics->QuadsSetSubsetFree(SliceU0, SliceV0Top, SliceU1, SliceV1Top, SliceU0, SliceV0Bottom, SliceU1, SliceV1Bottom);
		const IGraphics::CFreeformItem Item(TopLeft, TopRight, BottomLeft, BottomRight);
		pGraphics->QuadsDrawFreeform(&Item, 1);
	}
	pGraphics->QuadsEnd();
	pGraphics->TextureClear();
}

static void DrawCenteredLimitedText(ITextRender *pTextRender, const char *pText, float CenterX, float Y, float FontSize, float MaxWidth, ColorRGBA Color)
{
	if(pTextRender == nullptr || pText == nullptr || pText[0] == '\0' || MaxWidth <= 0.0f)
		return;

	float Size = FontSize;
	float Width = pTextRender->TextWidth(Size, pText, -1, -1.0f);
	if(Width > MaxWidth)
	{
		Size *= std::clamp(MaxWidth / maximum(Width, 0.001f), 0.62f, 1.0f);
		Width = pTextRender->TextWidth(Size, pText, -1, -1.0f);
	}
	pTextRender->TextColor(Color);
	pTextRender->Text(CenterX - Width * 0.5f, Y, Size, pText, -1.0f);
}

#if defined(CONF_FAMILY_WINDOWS)
static bool DecodeMusicVideoImageWithWic(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
{
	ImageOut.Free();
	if(pAbsolutePath == nullptr || pAbsolutePath[0] == '\0')
		return false;

	const HRESULT CoInitResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	const bool CoInitialized = SUCCEEDED(CoInitResult);
	if(FAILED(CoInitResult) && CoInitResult != RPC_E_CHANGED_MODE)
		return false;

	IWICImagingFactory *pFactory = nullptr;
	IWICBitmapDecoder *pDecoder = nullptr;
	IWICBitmapFrameDecode *pFrame = nullptr;
	IWICBitmapScaler *pScaler = nullptr;
	IWICFormatConverter *pConverter = nullptr;
	bool Success = false;

	auto Cleanup = [&]() {
		if(pConverter)
			pConverter->Release();
		if(pScaler)
			pScaler->Release();
		if(pFrame)
			pFrame->Release();
		if(pDecoder)
			pDecoder->Release();
		if(pFactory)
			pFactory->Release();
		if(CoInitialized)
			CoUninitialize();
	};

	do
	{
		if(FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory))))
			break;

		const std::wstring WidePath = windows_utf8_to_wide(pAbsolutePath);
		if(FAILED(pFactory->CreateDecoderFromFilename(WidePath.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder)))
			break;
		if(FAILED(pDecoder->GetFrame(0, &pFrame)))
			break;

		UINT SrcW = 0;
		UINT SrcH = 0;
		if(FAILED(pFrame->GetSize(&SrcW, &SrcH)) || SrcW == 0 || SrcH == 0)
			break;

		int DstW = (int)SrcW;
		int DstH = (int)SrcH;
		if(MaxDimension > 0 && (DstW > MaxDimension || DstH > MaxDimension))
		{
			const double Scale = minimum((double)MaxDimension / (double)DstW, (double)MaxDimension / (double)DstH);
			DstW = maximum(1, round_to_int((double)DstW * Scale));
			DstH = maximum(1, round_to_int((double)DstH * Scale));
		}

		IWICBitmapSource *pSource = pFrame;
		if(DstW != (int)SrcW || DstH != (int)SrcH)
		{
			if(FAILED(pFactory->CreateBitmapScaler(&pScaler)))
				break;
			if(FAILED(pScaler->Initialize(pFrame, (UINT)DstW, (UINT)DstH, WICBitmapInterpolationModeFant)))
				break;
			pSource = pScaler;
		}

		if(FAILED(pFactory->CreateFormatConverter(&pConverter)))
			break;
		if(FAILED(pConverter->Initialize(pSource, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
			break;

		const size_t Stride = (size_t)DstW * 4ull;
		const size_t ImageBytes = Stride * (size_t)DstH;
		if(ImageBytes == 0)
			break;
		ImageOut.m_pData = (uint8_t *)malloc(ImageBytes);
		if(!ImageOut.m_pData)
			break;
		if(FAILED(pConverter->CopyPixels(nullptr, (UINT)Stride, (UINT)ImageBytes, ImageOut.m_pData)))
		{
			ImageOut.Free();
			break;
		}

		ImageOut.m_Width = DstW;
		ImageOut.m_Height = DstH;
		ImageOut.m_Format = CImageInfo::FORMAT_RGBA;
		Success = true;
	} while(false);

	if(!Success)
		ImageOut.Free();
	Cleanup();
	return Success;
}
#else
static bool DecodeMusicVideoImageWithWic(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
{
	(void)pAbsolutePath;
	(void)MaxDimension;
	ImageOut.Free();
	return false;
}
#endif

static bool ResolveMusicVideoReadablePath(IStorage *pStorage, const char *pPath, int StorageType, char *pAbsolutePath, unsigned PathSize)
{
	if(pStorage == nullptr || pPath == nullptr || pAbsolutePath == nullptr || PathSize == 0)
		return false;

	pAbsolutePath[0] = '\0';
	if(StorageType == IStorage::TYPE_ALL)
	{
		IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_ALL, pAbsolutePath, PathSize);
		if(!File)
			return false;
		io_close(File);
		return pAbsolutePath[0] != '\0';
	}

	pStorage->GetCompletePath(StorageType, pPath, pAbsolutePath, PathSize);
	return pAbsolutePath[0] != '\0';
}

} // namespace

CMa::CMa()
{
	OnReset();
}

void CMa::OnInit()
{
	m_pGraphics = Kernel()->RequestInterface<IEngineGraphics>();
}

void CMa::OnReset()
{
	m_MusicPlaying = false;
	m_MusicInitialized = false;
	ResetMusicVideoEffect();
}

void CMa::OnShutdown()
{
	ResetMusicVideoEffect();
	UnloadMusicVideoCenterImage();
}

void CMa::OnConsoleInit()
{
	Console()->Register("ma_music_play", "", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CMa *pThis = (CMa *)pUserData;
		if(!pThis->m_MusicInitialized)
			return;
		pThis->m_MusicPlaying = true;
	}, this, "Play/resume music");

	Console()->Register("ma_music_pause", "", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CMa *pThis = (CMa *)pUserData;
		pThis->m_MusicPlaying = false;
	}, this, "Pause music");

	Console()->Register("ma_music_next", "", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CMa *pThis = (CMa *)pUserData;
		if(!pThis->m_MusicInitialized || pThis->m_vMusicTracks.empty())
			return;
		pThis->m_CurrentTrack = (pThis->m_CurrentTrack + 1) % (int)pThis->m_vMusicTracks.size();
		pThis->m_MusicPlaying = true;
	}, this, "Next track");

	Console()->Register("ma_music_prev", "", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CMa *pThis = (CMa *)pUserData;
		if(!pThis->m_MusicInitialized || pThis->m_vMusicTracks.empty())
			return;
		pThis->m_CurrentTrack = (pThis->m_CurrentTrack - 1 + (int)pThis->m_vMusicTracks.size()) % (int)pThis->m_vMusicTracks.size();
		pThis->m_MusicPlaying = true;
	}, this, "Previous track");

	Console()->Register("ma_music_stop", "", CFGFLAG_CLIENT, [](IConsole::IResult *pResult, void *pUserData) {
		CMa *pThis = (CMa *)pUserData;
		pThis->m_MusicPlaying = false;
		pThis->m_CurrentTrack = 0;
	}, this, "Stop music");
}

bool CMa::IsComponentDisabled(int Component) const
{
	(void)Component;
	return false;
}

// ===== MUSIC PLAYER =====

void CMa::UpdateMusicPlayer()
{
	if(!g_Config.m_MaMusicPlayer || IsComponentDisabled(2))
	{
		m_MusicPlaying = false;
		return;
	}

	if(!m_MusicInitialized)
	{
		m_vMusicTracks.clear();
		m_vMusicSampleIds.clear();

		Storage()->ListDirectoryInfo(IStorage::TYPE_ALL, "ma/music", [](const CFsFileInfo *pInfo, int IsDir, int DirType, void *pUser) -> int {
			CMa *pThis = (CMa *)pUser;
			if(!IsDir && str_endswith_nocase(pInfo->m_pName, ".opus"))
			{
				std::string Path = "ma/music/";
				Path += pInfo->m_pName;
				pThis->m_vMusicTracks.push_back(Path);
				int SampleId = pThis->Sound()->LoadOpus(Path.c_str(), IStorage::TYPE_ALL);
				pThis->m_vMusicSampleIds.push_back(SampleId);
			}
			return 0;
		}, this);

		m_CurrentTrack = 0;
		m_MusicInitialized = true;
	}
}

void CMa::RenderMusicPlayer()
{
	if(!g_Config.m_MaMusicPlayer || IsComponentDisabled(2))
		return;
	if(m_vMusicTracks.empty())
		return;

	if(m_MusicPlaying && !m_vMusicTracks.empty())
	{
		int Idx = m_CurrentTrack;
		if(Idx >= 0 && Idx < (int)m_vMusicSampleIds.size() && m_vMusicSampleIds[Idx] != -1)
		{
			float Vol = g_Config.m_MaMusicVolume / 100.0f;
			Sound()->Play(CSounds::CHN_MUSIC, m_vMusicSampleIds[Idx], ISound::FLAG_LOOP, Vol);
		}
	}
	else
	{
		Sound()->Stop(CSounds::CHN_MUSIC);
	}

	float SX0, SY0, SX1, SY1;
	Graphics()->GetScreen(&SX0, &SY0, &SX1, &SY1);
	float W = SX1 - SX0;
	float H = SY1 - SY0;

	const char *pTrackName = m_vMusicTracks[m_CurrentTrack].c_str();
	const char *pSlash = str_rchr(pTrackName, '/');
	if(pSlash) pTrackName = pSlash + 1;

	float FontSz = 10.0f;
	float TextW = TextRender()->TextWidth(FontSz, pTrackName);
	float BoxW = TextW + 20.0f;
	float BoxH = FontSz + 8.0f;

	float X = (W - BoxW) / 2.0f;
	float Y = g_Config.m_MaMusicPlayerSizeMode == 0 ? H - BoxH - 10.0f : H * 0.5f - BoxH / 2.0f;

	ColorRGBA Bg(0.0f, 0.0f, 0.0f, 0.6f);
	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	Graphics()->SetColor(Bg.r, Bg.g, Bg.b, Bg.a);
	IGraphics::CQuadItem BgQ(X, Y, BoxW, BoxH);
	Graphics()->QuadsDraw(&BgQ, 1);
	Graphics()->QuadsEnd();

	char aBuf[64];
	str_format(aBuf, sizeof(aBuf), "♫ %s", m_MusicPlaying ? pTrackName : TCLocalize("Paused"));
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.9f);
	TextRender()->Text(X + 10.0f, Y + 4.0f, FontSz, aBuf);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// ===== MUSIC VIDEO EFFECT =====

void CMa::ResetMusicVideoEffect()
{
	m_pMusicVideoVisualizer.reset();
	m_MusicVideoFrame = BestClientVisualizer::SVisualizerFrame();
	m_LastMusicVideoPollTick = 0;
	m_MusicVideoUsingAudio = false;
	m_MusicVideoLevel = 0.0f;
	m_MusicVideoKick = 0.0f;
	m_MusicVideoRollingPeak = 0.05f;
}

void CMa::UnloadMusicVideoCenterImage()
{
	MediaDecoder::UnloadFrames(m_pGraphics, m_vMusicVideoCenterImageFrames);
	m_MusicVideoCenterImageAnimated = false;
	m_MusicVideoCenterImageWidth = 0;
	m_MusicVideoCenterImageHeight = 0;
	m_MusicVideoCenterImageAnimationStart = 0;
	m_MusicVideoCenterImageLoaded = false;
	m_MusicVideoCenterImageHasError = false;
	m_aMusicVideoCenterImageLoadedPath[0] = '\0';
	str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("No image."), sizeof(m_aMusicVideoCenterImageStatus));
}

bool CMa::LoadMusicVideoCenterImage(const char *pPath)
{
	UnloadMusicVideoCenterImage();
	if(pPath == nullptr || pPath[0] == '\0')
		return false;

	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!Storage()->ReadFile(pPath, IStorage::TYPE_ALL, &pData, &DataSize))
	{
		m_MusicVideoCenterImageHasError = true;
		str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Failed to read image."), sizeof(m_aMusicVideoCenterImageStatus));
		return false;
	}

	const std::string Ext = MediaDecoder::ExtractExtensionLower(pPath);
	const bool AnimatedImage = MediaDecoder::IsLikelyAnimatedImageExtension(Ext);
	SMediaDecodedFrames DecodedFrames;
	bool Success = false;
	if(AnimatedImage)
	{
		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = 512;
		Limits.m_MaxFrames = 80;
		Limits.m_MaxTotalBytes = 32ull * 1024ull * 1024ull;
		Limits.m_MaxAnimationDurationMs = 12000;
		Limits.m_DecodeAllFrames = true;
		Success = MediaDecoder::DecodeAnimatedImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, Limits);
		if(!Success)
			Success = MediaDecoder::DecodeStaticImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, 512);
	}
	else
	{
		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = 512;
		Limits.m_DecodeAllFrames = false;
		Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, Limits);
		if(!Success)
			Success = MediaDecoder::DecodeStaticImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, 512);
	}
	free(pData);

	if(!Success || DecodedFrames.Empty())
	{
		DecodedFrames.Free();
		char aAbsolutePath[IO_MAX_PATH_LENGTH];
		CImageInfo FallbackImage;
		if(ResolveMusicVideoReadablePath(Storage(), pPath, IStorage::TYPE_ALL, aAbsolutePath, sizeof(aAbsolutePath)) &&
			DecodeMusicVideoImageWithWic(aAbsolutePath, FallbackImage, 1024))
		{
			SMediaFrame Frame;
			const int ImageWidth = (int)FallbackImage.m_Width;
			const int ImageHeight = (int)FallbackImage.m_Height;
			Frame.m_Texture = m_pGraphics->LoadTextureRawMove(FallbackImage, 0, pPath);
			if(Frame.m_Texture.IsValid())
			{
				m_vMusicVideoCenterImageFrames.push_back(Frame);
				m_MusicVideoCenterImageAnimated = false;
				m_MusicVideoCenterImageWidth = ImageWidth;
				m_MusicVideoCenterImageHeight = ImageHeight;
				m_MusicVideoCenterImageAnimationStart = time_get();
				m_MusicVideoCenterImageLoaded = true;
				m_MusicVideoCenterImageHasError = false;
				str_copy(m_aMusicVideoCenterImageLoadedPath, pPath, sizeof(m_aMusicVideoCenterImageLoadedPath));
				str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Image loaded."), sizeof(m_aMusicVideoCenterImageStatus));
				return true;
			}
			FallbackImage.Free();
			m_MusicVideoCenterImageHasError = true;
			str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Failed to upload image."), sizeof(m_aMusicVideoCenterImageStatus));
			return false;
		}

		m_MusicVideoCenterImageHasError = true;
		str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Failed to decode image."), sizeof(m_aMusicVideoCenterImageStatus));
		return false;
	}

	const int DecodedWidth = DecodedFrames.m_Width;
	const int DecodedHeight = DecodedFrames.m_Height;
	const bool DecodedAnimated = DecodedFrames.m_Animated;
	std::vector<SMediaFrame> vNewFrames;
	if(!MediaDecoder::UploadFrames(m_pGraphics, DecodedFrames, vNewFrames, pPath))
	{
		m_MusicVideoCenterImageHasError = true;
		str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Failed to upload image."), sizeof(m_aMusicVideoCenterImageStatus));
		return false;
	}

	m_vMusicVideoCenterImageFrames = std::move(vNewFrames);
	m_MusicVideoCenterImageAnimated = DecodedAnimated;
	m_MusicVideoCenterImageWidth = DecodedWidth;
	m_MusicVideoCenterImageHeight = DecodedHeight;
	m_MusicVideoCenterImageAnimationStart = time_get();
	m_MusicVideoCenterImageLoaded = true;
	m_MusicVideoCenterImageHasError = false;
	str_copy(m_aMusicVideoCenterImageLoadedPath, pPath, sizeof(m_aMusicVideoCenterImageLoadedPath));
	str_copy(m_aMusicVideoCenterImageStatus, TCLocalize("Image loaded."), sizeof(m_aMusicVideoCenterImageStatus));
	return true;
}

void CMa::EnsureMusicVideoCenterImageLoaded()
{
	if(str_comp(g_Config.m_MaMusicVideoEffectImagePath, m_aMusicVideoCenterImageLoadedPath) == 0)
		return;
	if(g_Config.m_MaMusicVideoEffectImagePath[0] == '\0')
	{
		UnloadMusicVideoCenterImage();
		return;
	}
	LoadMusicVideoCenterImage(g_Config.m_MaMusicVideoEffectImagePath);
}

void CMa::ReloadMusicVideoCenterImage()
{
	m_aMusicVideoCenterImageLoadedPath[0] = '\0';
	EnsureMusicVideoCenterImageLoaded();
}

CUIRect CMa::GetMusicVideoEffectRect(float Width, float Height, bool ForcePreview) const
{
	if(!ForcePreview && (!g_Config.m_MaMusicVideoEffect || !HudLayout::IsEnabled(HudLayout::MODULE_MUSIC_VIDEO_EFFECT)))
		return CUIRect{};

	const auto Layout = HudLayout::Get(HudLayout::MODULE_MUSIC_VIDEO_EFFECT, Width, Height);
	const float LayoutScale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float SizeScale = std::clamp(g_Config.m_MaMusicVideoEffectSize / 100.0f, 0.40f, 2.40f);
	const float Size = minimum(Width, Height) * 0.56f * SizeScale * LayoutScale;
	HudLayout::SModuleRect RawRect{Layout.m_X, Layout.m_Y, Size, Size, Size * 0.5f};
	const HudLayout::SModuleRect Clamped = HudLayout::ClampRectToScreen(RawRect, Width, Height);
	return {Clamped.m_X, Clamped.m_Y, Clamped.m_W, Clamped.m_H};
}

CUIRect CMa::GetMusicVideoEffectHudEditorRect(bool ForcePreview) const
{
	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * (m_pGraphics ? m_pGraphics->ScreenAspect() : 1.0f);
	const CUIRect RenderRect = GetMusicVideoEffectRect(Width, Height, ForcePreview);
	if(RenderRect.w <= 0.0f || RenderRect.h <= 0.0f)
		return RenderRect;

	const float EditorW = RenderRect.w * MUSIC_VIDEO_EDITOR_RECT_SCALE;
	const float EditorH = RenderRect.h * MUSIC_VIDEO_EDITOR_RECT_SCALE;
	return {
		RenderRect.x + (RenderRect.w - EditorW) * 0.5f,
		RenderRect.y + (RenderRect.h - EditorH) * 0.5f,
		EditorW,
		EditorH};
}

void CMa::ApplyMusicVideoEffectHudEditorRect(const CUIRect &EditorRect, float HudWidth, float HudHeight)
{
	if(EditorRect.w <= 0.0f || EditorRect.h <= 0.0f)
		return;

	const float RenderW = EditorRect.w / MUSIC_VIDEO_EDITOR_RECT_SCALE;
	const float RenderH = EditorRect.h / MUSIC_VIDEO_EDITOR_RECT_SCALE;
	const float RenderX = EditorRect.x - (RenderW - EditorRect.w) * 0.5f;
	const float RenderY = EditorRect.y - (RenderH - EditorRect.h) * 0.5f;
	const float ClampedRenderX = std::clamp(RenderX, 0.0f, maximum(0.0f, HudWidth - RenderW));
	const float ClampedRenderY = std::clamp(RenderY, 0.0f, maximum(0.0f, HudHeight - RenderH));
	HudLayout::SetPosition(
		HudLayout::MODULE_MUSIC_VIDEO_EFFECT,
		ClampedRenderX * (HudLayout::CANVAS_WIDTH / maximum(HudWidth, 1.0f)),
		ClampedRenderY);
}

void CMa::RenderMusicVideoEffectHudEditor(bool ForcePreview)
{
	RenderMusicVideoEffect(ForcePreview);
}

void CMa::RenderMusicVideoEffectBackground()
{
	if(!g_Config.m_MaEnabled || !g_Config.m_MaMusicVideoEffectBehind)
		return;
	RenderMusicVideoEffect(false);
}

float CMa::UpdateMusicVideoEffect(float Delta, bool ForcePreview, bool MusicPlaying, const char *pTitle, const char *pArtist)
{
	if(!ForcePreview && !g_Config.m_MaMusicVideoEffect)
	{
		ResetMusicVideoEffect();
		return 0.0f;
	}

	const bool ShouldUseAudio = ForcePreview || !g_Config.m_MaMusicVideoEffectMusicOnly || MusicPlaying;
	const bool ShouldWarmAudio = ForcePreview || g_Config.m_MaMusicVideoEffect != 0;
	if(ShouldUseAudio && !m_MusicVideoUsingAudio)
	{
		m_LastMusicVideoPollTick = 0;
		m_MusicVideoRollingPeak = 0.05f;
	}
	m_MusicVideoUsingAudio = ShouldUseAudio;

	if(ShouldWarmAudio && !m_pMusicVideoVisualizer)
		m_pMusicVideoVisualizer = std::make_unique<BestClientVisualizer::CRealtimeMusicVisualizer>();

	const int64_t Now = time_get();
	if(m_pMusicVideoVisualizer && (m_LastMusicVideoPollTick == 0 || Now - m_LastMusicVideoPollTick >= time_freq() / 30))
	{
		BestClientVisualizer::SVisualizerPlaybackHint Hint;
		Hint.m_Title = pTitle ? pTitle : "";
		Hint.m_Artist = pArtist ? pArtist : "";
		Hint.m_Playing = ShouldUseAudio;
		m_pMusicVideoVisualizer->SetPlaybackHint(Hint);
		m_pMusicVideoVisualizer->PollFrame(m_MusicVideoFrame);
		m_LastMusicVideoPollTick = Now;
	}

	if(!ShouldUseAudio)
	{
		m_MusicVideoLevel = ApproachEffectValue(m_MusicVideoLevel, 0.0f, Delta, 8.0f);
		m_MusicVideoKick = ApproachEffectValue(m_MusicVideoKick, 0.0f, Delta, 14.0f);
		return m_MusicVideoLevel;
	}

	const BestClientVisualizer::SVisualizerFrame &Frame = m_MusicVideoFrame;
	const bool HasLiveSignal = Frame.m_BackendStatus == BestClientVisualizer::EVisualizerBackendStatus::LIVE && Frame.m_HasRealtimeSignal;
	float Target = 0.0f;
	if(HasLiveSignal)
	{
		float Bass = 0.0f;
		for(int i = 0; i < 6; ++i)
			Bass += Frame.m_aBands[i];
		Bass /= 6.0f;

		float BandPeak = 0.0f;
		for(float Band : Frame.m_aBands)
			BandPeak = maximum(BandPeak, Band);

		const float Raw = maximum(Frame.m_Rms * 10.0f, Frame.m_Peak * 2.0f, Bass * 0.9f + BandPeak * 0.28f);
		if(Raw > m_MusicVideoRollingPeak)
			m_MusicVideoRollingPeak = ApproachEffectValue(m_MusicVideoRollingPeak, Raw, Delta, 9.0f);
		else
			m_MusicVideoRollingPeak = ApproachEffectValue(m_MusicVideoRollingPeak, maximum(Raw, 0.025f), Delta, 1.8f);

		const float Boost = std::clamp(0.72f / maximum(m_MusicVideoRollingPeak, 0.02f), 1.0f, 16.0f);
		Target = std::clamp(Raw * Boost, 0.0f, 1.0f);
		if(Target < 0.025f)
			Target = 0.0f;
	}

	const float OldLevel = m_MusicVideoLevel;
	const float Speed = Target > m_MusicVideoLevel ? 26.0f : 7.0f;
	m_MusicVideoLevel = ApproachEffectValue(m_MusicVideoLevel, Target, Delta, Speed);
	m_MusicVideoKick = ApproachEffectValue(m_MusicVideoKick, std::clamp((m_MusicVideoLevel - OldLevel) * 8.0f, 0.0f, 1.0f), Delta, 18.0f);
	return m_MusicVideoLevel;
}

void CMa::RenderMusicVideoEffect(bool ForcePreview)
{
	if(!ForcePreview && !g_Config.m_MaMusicVideoEffect)
	{
		ResetMusicVideoEffect();
		return;
	}
	if(!ForcePreview && !HudLayout::IsEnabled(HudLayout::MODULE_MUSIC_VIDEO_EFFECT))
		return;
	if(!ForcePreview && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
	{
		ResetMusicVideoEffect();
		return;
	}
	if(!ForcePreview && GameClient()->m_Menus.IsActive())
		return;

	const float Delta = ForcePreview ? 1.0f / 60.0f : std::clamp(Client()->RenderFrameTime(), 0.0f, MAX_EFFECT_DELTA);
	if(Delta <= 0.0f)
		return;

	const float Alpha = std::clamp(g_Config.m_MaMusicVideoEffectAlpha / 100.0f, 0.0f, 1.0f);
	if(Alpha <= 0.0f)
		return;

	float PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1;
	Graphics()->GetScreen(&PrevScreenX0, &PrevScreenY0, &PrevScreenX1, &PrevScreenY1);

	const float Height = HudLayout::CANVAS_HEIGHT;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	const CUIRect EffectRect = GetMusicVideoEffectRect(Width, Height, ForcePreview);
	if(EffectRect.w <= 0.0f || EffectRect.h <= 0.0f)
	{
		Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
		return;
	}

	CMusicPlayer::SNowPlayingInfo NowPlaying;
	const bool NeedNowPlaying = ForcePreview || g_Config.m_MaMusicVideoEffectMusicOnly || g_Config.m_MaMusicVideoEffectShowTrack;
	if(NeedNowPlaying)
		GameClient()->m_MusicPlayer.GetNowPlayingInfo(NowPlaying);
	if(ForcePreview)
	{
		NowPlaying.m_Valid = true;
		NowPlaying.m_Playing = true;
		NowPlaying.m_Title = "Mi musica";
		NowPlaying.m_Artist = "MΛ ツ";
	}
	const bool MusicPlaying = NowPlaying.m_Valid && NowPlaying.m_Playing;
	const float Level = UpdateMusicVideoEffect(Delta, ForcePreview, MusicPlaying, NowPlaying.m_Title.c_str(), NowPlaying.m_Artist.c_str());
	const bool AudioMotionActive = ForcePreview || !g_Config.m_MaMusicVideoEffectMusicOnly || MusicPlaying;
	const float Time = (float)(time_get() / (double)time_freq());
	const float Lod = ForcePreview ? 1.0f : PerformanceLodScale(Delta);
	const int NumPoints = std::clamp(round_to_int(std::clamp(g_Config.m_MaMusicVideoEffectPoints, 32, MAX_MUSIC_VIDEO_POINTS) * Lod), 32, MAX_MUSIC_VIDEO_POINTS);
	const int TrailLines = std::clamp(round_to_int(std::clamp(g_Config.m_MaMusicVideoEffectTrailLines, 1, 10) * (0.55f + 0.45f * Lod)), 1, 10);
	const float Intensity = std::clamp(g_Config.m_MaMusicVideoEffectIntensity / 100.0f, 0.0f, 3.0f);

	EnsureMusicVideoCenterImageLoaded();

	const vec2 Center(EffectRect.x + EffectRect.w * 0.5f, EffectRect.y + EffectRect.h * 0.5f);
	const float Radius = minimum(EffectRect.w, EffectRect.h) * 0.31f;
	const float InnerRadius = Radius * (0.72f + Level * 0.025f);
	const float RingBaseRadius = InnerRadius + maximum(1.4f, Radius * 0.035f);
	ColorRGBA EffectColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_MaMusicVideoEffectColor));
	if(EffectColor.r + EffectColor.g + EffectColor.b < 0.04f)
		EffectColor = ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f);

	Graphics()->TextureClear();
	Graphics()->BlendNormal();
	Graphics()->DrawRect(Center.x - InnerRadius, Center.y - InnerRadius, InnerRadius * 2.0f, InnerRadius * 2.0f, ColorRGBA(0.0f, 0.0f, 0.0f, Alpha * 0.62f), IGraphics::CORNER_ALL, InnerRadius);
	Graphics()->DrawRect(Center.x - InnerRadius * 0.92f, Center.y - InnerRadius * 0.92f, InnerRadius * 1.84f, InnerRadius * 1.84f, ColorRGBA(0.0f, 0.0f, 0.0f, Alpha * 0.40f), IGraphics::CORNER_ALL, InnerRadius * 0.92f);

	IGraphics::CTextureHandle CenterTexture;
	const bool HasCenterImage =
		m_MusicVideoCenterImageLoaded &&
		MediaDecoder::GetCurrentFrameTexture(m_vMusicVideoCenterImageFrames, m_MusicVideoCenterImageAnimated, m_MusicVideoCenterImageAnimationStart, CenterTexture);
	if(HasCenterImage)
	{
		const float ImageScale = std::clamp(g_Config.m_MaMusicVideoEffectImageSize / 100.0f, 0.20f, 1.0f);
		const float ImageSize = InnerRadius * 2.0f * 0.92f * ImageScale;
		const CUIRect ImageRect{Center.x - ImageSize * 0.5f, Center.y - ImageSize * 0.5f, ImageSize, ImageSize};
		DrawRoundedTextureCover(Graphics(), CenterTexture, ImageRect, m_MusicVideoCenterImageWidth, m_MusicVideoCenterImageHeight, Alpha * 0.92f);
	}
	else
	{
		const char *pLogo = "MΛ";
		const float LogoFont = maximum(10.0f, InnerRadius * 0.48f);
		const float LogoW = TextRender()->TextWidth(LogoFont, pLogo, -1, -1.0f);
		TextRender()->TextColor(EffectColor.WithAlpha(Alpha * (0.24f + Level * 0.34f)));
		TextRender()->Text(Center.x - LogoW * 0.5f, Center.y - LogoFont * 0.58f, LogoFont, pLogo, -1.0f);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	if(g_Config.m_MaMusicVideoEffectShowTrack && NowPlaying.m_Valid)
	{
		const float TitleFont = maximum(4.8f, InnerRadius * 0.145f);
		const float ArtistFont = maximum(3.8f, InnerRadius * 0.105f);
		const float MaxTextWidth = InnerRadius * 1.60f;
		const float TextBaseY = Center.y + InnerRadius * 0.43f;
		DrawCenteredLimitedText(TextRender(), NowPlaying.m_Title.empty() ? TCLocalize("No media") : NowPlaying.m_Title.c_str(), Center.x, TextBaseY, TitleFont, MaxTextWidth, ColorRGBA(1.0f, 1.0f, 1.0f, Alpha * 0.88f));
		if(!NowPlaying.m_Artist.empty())
			DrawCenteredLimitedText(TextRender(), NowPlaying.m_Artist.c_str(), Center.x, TextBaseY + TitleFont * 0.92f, ArtistFont, MaxTextWidth, ColorRGBA(0.85f, 0.85f, 0.90f, Alpha * 0.60f));
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	}

	std::array<vec2, MAX_MUSIC_VIDEO_POINTS> aPoints;
	std::array<IGraphics::CLineItem, MAX_MUSIC_VIDEO_POINTS> aLines;
	std::array<IGraphics::CLineItem, MAX_MUSIC_VIDEO_POINTS> aRadialLines;
	std::array<vec2, MAX_MUSIC_VIDEO_POINTS> aDirections;
	std::array<float, MAX_MUSIC_VIDEO_POINTS> aAngles;
	std::array<float, MAX_MUSIC_VIDEO_POINTS> aBandPower;
	const BestClientVisualizer::SVisualizerFrame &Frame = m_MusicVideoFrame;
	const bool HasLiveSignal = AudioMotionActive && Frame.m_BackendStatus == BestClientVisualizer::EVisualizerBackendStatus::LIVE && Frame.m_HasRealtimeSignal;

	for(int i = 0; i < NumPoints; ++i)
	{
		const float Angle = -pi + (2.0f * pi * i) / NumPoints;
		aAngles[i] = Angle;
		aDirections[i] = vec2(cosf(Angle), sinf(Angle));
		const int BandIndex = std::clamp((i * BestClientVisualizer::MAX_VISUALIZER_BANDS) / NumPoints, 0, BestClientVisualizer::MAX_VISUALIZER_BANDS - 1);
		float Band = HasLiveSignal ? std::clamp(Frame.m_aBands[BandIndex], 0.0f, 1.0f) : 0.0f;
		if(ForcePreview)
			Band = 0.45f + 0.45f * sinf(Time * 4.0f + Angle * 3.0f + BandIndex * 0.35f);
		aBandPower[i] = powf(std::clamp(Band, 0.0f, 1.0f), 0.62f);
	}

	auto BuildLines = [&](float RadiusOffset, float AudioScale, float TimeShift) {
		for(int i = 0; i < NumPoints; ++i)
		{
			const float Angle = aAngles[i];
			const float BandWave = aBandPower[i] * Intensity * AudioScale;
			const float IdleWave = (AudioMotionActive || ForcePreview) ?
						       0.014f * sinf(Angle * 6.0f + (Time - TimeShift) * 2.0f) +
							       0.009f * sinf(Angle * 11.0f - (Time - TimeShift) * 1.45f) :
						       0.0f;
			const float Shape = MusicVideoBaseShape(Angle);
			const float Pulse = Level * Intensity * 0.05f + m_MusicVideoKick * 0.07f;
			const float BaseRadius = RingBaseRadius * Shape + RadiusOffset;
			const float AudioOut = Radius * (BandWave * 0.24f + Pulse + IdleWave * (0.26f + Level * 0.55f));
			const float WaveRadius = BaseRadius + maximum(0.0f, AudioOut);
			const vec2 BasePoint = Center + aDirections[i] * BaseRadius;
			aPoints[i] = Center + aDirections[i] * WaveRadius;
			aRadialLines[i] = IGraphics::CLineItem(BasePoint, aPoints[i]);
		}
		for(int i = 0; i < NumPoints; ++i)
			aLines[i] = IGraphics::CLineItem(aPoints[i], aPoints[(i + 1) % NumPoints]);
	};

	Graphics()->LinesBegin();
	const float GlowPulse = 0.7f + 0.3f * Level + 0.35f * m_MusicVideoKick;
	for(int i = TrailLines; i >= 1; --i)
	{
		const float T = i / (float)maximum(1, TrailLines);
		BuildLines(i * 1.35f, 0.82f + 0.12f * (1.0f - T), i * 0.030f);
		const float PassAlpha = std::clamp(Alpha * (0.035f + 0.13f * (1.0f - T)) * GlowPulse, 0.0f, 0.40f);
		Graphics()->SetColor(EffectColor.r, EffectColor.g, EffectColor.b, PassAlpha);
		Graphics()->LinesDraw(aLines.data(), NumPoints);
	}
	BuildLines(0.0f, 1.08f, 0.0f);
	if(Lod >= 0.55f)
	{
		Graphics()->SetColor(EffectColor.r, EffectColor.g, EffectColor.b, std::clamp(Alpha * (0.18f + Level * 0.18f), 0.0f, 0.55f));
		Graphics()->LinesDraw(aRadialLines.data(), NumPoints);
	}
	Graphics()->SetColor(EffectColor.r, EffectColor.g, EffectColor.b, std::clamp(Alpha * (0.88f + Level * 0.16f), 0.0f, 1.0f));
	Graphics()->LinesDraw(aLines.data(), NumPoints);
	BuildLines(-0.6f, 0.88f, 0.0f);
	Graphics()->SetColor(EffectColor.r, EffectColor.g, EffectColor.b, std::clamp(Alpha * 0.28f * GlowPulse, 0.0f, 1.0f));
	Graphics()->LinesDraw(aLines.data(), NumPoints);
	Graphics()->LinesEnd();

	Graphics()->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
}

// ===== MAIN LOOP =====

void CMa::OnRender()
{
	if(!g_Config.m_MaEnabled)
		return;
	if(g_Config.m_MaMusicVideoEffectBehind)
		return;
	RenderMusicVideoEffect(false);
	// Music Player is now handled by the separate CMusicPlayer component
}

void CMa::OnNewSnapshot()
{
	if(!g_Config.m_MaEnabled)
		return;
	// Music Player is now handled by the separate CMusicPlayer component
}

bool CMa::OnInput(const IInput::CEvent &Event)
{
	if(!g_Config.m_MaEnabled)
		return false;
	return false;
}
