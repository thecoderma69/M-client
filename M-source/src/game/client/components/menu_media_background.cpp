#include "menu_media_background.h"

#include <base/io.h>
#include <base/system.h>
#if defined(CONF_FAMILY_WINDOWS)
#include <base/windows.h>
#endif

#include <engine/shared/config.h>

#include <game/localization.h>

#include <algorithm>
#include <cmath>
#include <limits>

#if defined(CONF_FAMILY_WINDOWS)
#define IStorage WindowsIStorage
#include <objbase.h>
#include <wincodec.h>
#undef IStorage
#endif

#if defined(CONF_VIDEORECORDER)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}
#endif

namespace
{
	constexpr int MENU_MEDIA_MAX_VIDEO_FRAME_MS = 250;
	constexpr int MENU_MEDIA_DEFAULT_VIDEO_FRAME_MS = 33;
	constexpr int MENU_MEDIA_MAX_IMAGE_DIMENSION = 2048;
	constexpr int MENU_MEDIA_MAX_ANIMATION_FRAMES = 90;
	constexpr size_t MENU_MEDIA_MAX_ANIMATION_BYTES = 96ull * 1024ull * 1024ull;
	constexpr int64_t MENU_MEDIA_PTS_UNSET = std::numeric_limits<int64_t>::min();

#if defined(CONF_VIDEORECORDER)
	bool DecodeFirstFrameFromFile(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
	{
		ImageOut.Free();

		if(pAbsolutePath == nullptr || pAbsolutePath[0] == '\0')
			return false;

		AVFormatContext *pFormatCtx = nullptr;
		AVCodecContext *pCodecCtx = nullptr;
		SwsContext *pSwsCtx = nullptr;
		AVPacket *pPacket = nullptr;
		AVFrame *pFrame = nullptr;
		AVFrame *pFrameRgba = nullptr;
		int VideoStream = -1;
		bool Success = false;
		int SrcW = 0;
		int SrcH = 0;
		int DstW = 0;
		int DstH = 0;
		size_t FrameBytes = 0;

		auto PrepareFrameTarget = [&]() -> bool {
			if(!pFrame || !pCodecCtx || pFrame->width <= 0 || pFrame->height <= 0)
				return false;

			const int FrameW = pFrame->width;
			const int FrameH = pFrame->height;
			if(pSwsCtx && pFrameRgba && FrameW == SrcW && FrameH == SrcH)
				return true;

			if(pSwsCtx)
			{
				sws_freeContext(pSwsCtx);
				pSwsCtx = nullptr;
			}
			if(pFrameRgba)
				av_frame_free(&pFrameRgba);

			SrcW = FrameW;
			SrcH = FrameH;
			DstW = SrcW;
			DstH = SrcH;
			if(MaxDimension > 0 && (DstW > MaxDimension || DstH > MaxDimension))
			{
				const double Scale = std::min((double)MaxDimension / (double)DstW, (double)MaxDimension / (double)DstH);
				DstW = std::max(1, (int)((double)DstW * Scale + 0.5));
				DstH = std::max(1, (int)((double)DstH * Scale + 0.5));
			}
			if((size_t)DstH > std::numeric_limits<size_t>::max() / 4ull ||
				(size_t)DstW > std::numeric_limits<size_t>::max() / ((size_t)DstH * 4ull))
				return false;
			FrameBytes = (size_t)DstW * (size_t)DstH * 4ull;
			if(FrameBytes == 0)
				return false;

			const AVPixelFormat SrcFormat = pFrame->format >= 0 ? (AVPixelFormat)pFrame->format : pCodecCtx->pix_fmt;
			pSwsCtx = sws_getContext(SrcW, SrcH, SrcFormat, DstW, DstH, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
			if(!pSwsCtx)
				return false;

			pFrameRgba = av_frame_alloc();
			if(!pFrameRgba)
				return false;

			pFrameRgba->format = AV_PIX_FMT_RGBA;
			pFrameRgba->width = DstW;
			pFrameRgba->height = DstH;
			if(av_frame_get_buffer(pFrameRgba, 1) < 0)
				return false;

			return true;
		};

		auto CopyFrame = [&]() -> bool {
			if(!PrepareFrameTarget() || !pFrameRgba || !pSwsCtx || FrameBytes == 0)
				return false;
			if(av_frame_make_writable(pFrameRgba) < 0)
				return false;
			const int ScaledLines = sws_scale(pSwsCtx, pFrame->data, pFrame->linesize, 0, SrcH, pFrameRgba->data, pFrameRgba->linesize);
			if(ScaledLines <= 0 || pFrameRgba->linesize[0] <= 0 || (size_t)pFrameRgba->linesize[0] < (size_t)DstW * 4ull)
				return false;

			ImageOut.Free();
			ImageOut.m_Width = DstW;
			ImageOut.m_Height = DstH;
			ImageOut.m_Format = CImageInfo::FORMAT_RGBA;
			ImageOut.m_pData = (uint8_t *)malloc(FrameBytes);
			if(!ImageOut.m_pData)
				return false;

			for(int y = 0; y < DstH; ++y)
			{
				mem_copy(
					ImageOut.m_pData + (size_t)y * (size_t)DstW * 4ull,
					pFrameRgba->data[0] + (size_t)y * (size_t)pFrameRgba->linesize[0],
					(size_t)DstW * 4ull);
			}
			return true;
		};

		do
		{
			if(avformat_open_input(&pFormatCtx, pAbsolutePath, nullptr, nullptr) != 0)
				break;
			if(avformat_find_stream_info(pFormatCtx, nullptr) < 0)
				break;

			VideoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
			if(VideoStream < 0)
				break;

			const AVStream *pStream = pFormatCtx->streams[VideoStream];
			const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
			if(!pCodec)
				break;

			pCodecCtx = avcodec_alloc_context3(pCodec);
			if(!pCodecCtx || avcodec_parameters_to_context(pCodecCtx, pStream->codecpar) < 0 || avcodec_open2(pCodecCtx, pCodec, nullptr) < 0)
				break;

			pPacket = av_packet_alloc();
			pFrame = av_frame_alloc();
			if(!pPacket || !pFrame)
				break;

			while(av_read_frame(pFormatCtx, pPacket) >= 0)
			{
				if(pPacket->stream_index == VideoStream)
				{
					if(avcodec_send_packet(pCodecCtx, pPacket) < 0)
					{
						av_packet_unref(pPacket);
						break;
					}
					while(avcodec_receive_frame(pCodecCtx, pFrame) == 0)
					{
						if(CopyFrame())
						{
							Success = true;
							break;
						}
					}
				}
				av_packet_unref(pPacket);
				if(Success)
					break;
			}

			if(!Success && avcodec_send_packet(pCodecCtx, nullptr) >= 0)
			{
				while(avcodec_receive_frame(pCodecCtx, pFrame) == 0)
				{
					if(CopyFrame())
					{
						Success = true;
						break;
					}
				}
			}
		} while(false);

		if(!Success)
			ImageOut.Free();

		if(pFrameRgba)
			av_frame_free(&pFrameRgba);
		if(pFrame)
			av_frame_free(&pFrame);
		if(pPacket)
			av_packet_free(&pPacket);
		if(pSwsCtx)
			sws_freeContext(pSwsCtx);
		if(pCodecCtx)
			avcodec_free_context(&pCodecCtx);
		if(pFormatCtx)
			avformat_close_input(&pFormatCtx);

		return Success;
	}
#else
	bool DecodeFirstFrameFromFile(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
	{
		(void)pAbsolutePath;
		(void)MaxDimension;
		ImageOut.Free();
		return false;
	}
#endif

#if defined(CONF_FAMILY_WINDOWS)
	bool DecodeImageWithWic(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
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
				const double Scale = std::min((double)MaxDimension / (double)DstW, (double)MaxDimension / (double)DstH);
				DstW = std::max(1, (int)((double)DstW * Scale + 0.5));
				DstH = std::max(1, (int)((double)DstH * Scale + 0.5));
			}
			if((size_t)DstH > std::numeric_limits<size_t>::max() / 4ull ||
				(size_t)DstW > std::numeric_limits<size_t>::max() / ((size_t)DstH * 4ull))
				break;

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
	bool DecodeImageWithWic(const char *pAbsolutePath, CImageInfo &ImageOut, int MaxDimension)
	{
		(void)pAbsolutePath;
		(void)MaxDimension;
		ImageOut.Free();
		return false;
	}
#endif

	bool ResolveReadablePath(IStorage *pStorage, const char *pPath, int StorageType, char *pAbsolutePath, unsigned PathSize)
	{
		if(pStorage == nullptr || pPath == nullptr || pAbsolutePath == nullptr || PathSize == 0)
			return false;

		pAbsolutePath[0] = '\0';
		if(StorageType == IStorage::TYPE_ALL)
		{
			IOHANDLE File = pStorage->OpenFile(pPath, IOFLAG_READ, IStorage::TYPE_ALL, pAbsolutePath, PathSize);
			if(!File)
			{
				pAbsolutePath[0] = '\0';
				return false;
			}
			io_close(File);
			return pAbsolutePath[0] != '\0';
		}

		pStorage->GetCompletePath(StorageType, pPath, pAbsolutePath, PathSize);
		return pAbsolutePath[0] != '\0';
	}
}

CMenuMediaBackground::~CMenuMediaBackground()
{
	Unload();
}

void CMenuMediaBackground::Init(IGraphics *pGraphics, IStorage *pStorage)
{
	m_pGraphics = pGraphics;
	m_pStorage = pStorage;
	SetStatus(Localize("Disabled."));
}

void CMenuMediaBackground::SetStatus(const char *pText)
{
	m_HasError = false;
	str_copy(m_aStatus, pText, sizeof(m_aStatus));
}

void CMenuMediaBackground::SetError(const char *pText)
{
	m_HasError = true;
	str_copy(m_aStatus, pText, sizeof(m_aStatus));
}

void CMenuMediaBackground::ClearVideoState()
{
#if defined(CONF_VIDEORECORDER)
	if(m_pGraphics != nullptr)
		m_pGraphics->UnloadTexture(&m_VideoTexture);
	if(m_pPacket)
		av_packet_free(&m_pPacket);
	if(m_pFrameRgba)
		av_frame_free(&m_pFrameRgba);
	if(m_pFrame)
		av_frame_free(&m_pFrame);
	if(m_pSwsCtx)
		sws_freeContext(m_pSwsCtx);
	if(m_pCodecCtx)
		avcodec_free_context(&m_pCodecCtx);
	if(m_pFormatCtx)
		avformat_close_input(&m_pFormatCtx);

	m_pFormatCtx = nullptr;
	m_pCodecCtx = nullptr;
	m_pFrame = nullptr;
	m_pFrameRgba = nullptr;
	m_pPacket = nullptr;
	m_pSwsCtx = nullptr;
	m_VideoStream = -1;
	m_LastVideoPts = MENU_MEDIA_PTS_UNSET;
	m_NextFrameTime = std::chrono::nanoseconds::zero();
	m_vVideoUploadBuffer.clear();
#else
	if(m_pGraphics != nullptr)
		m_pGraphics->UnloadTexture(&m_VideoTexture);
	m_pFormatCtx = nullptr;
	m_pCodecCtx = nullptr;
	m_pFrame = nullptr;
	m_pFrameRgba = nullptr;
	m_pPacket = nullptr;
	m_pSwsCtx = nullptr;
	m_VideoStream = -1;
	m_LastVideoPts = MENU_MEDIA_PTS_UNSET;
	m_NextFrameTime = std::chrono::nanoseconds::zero();
	m_vVideoUploadBuffer.clear();
#endif
}

void CMenuMediaBackground::Unload()
{
	MediaDecoder::UnloadFrames(m_pGraphics, m_vFrames);
	ClearVideoState();

	m_Animated = false;
	m_Width = 0;
	m_Height = 0;
	m_AnimationStart = 0;
	m_IsVideo = false;
	m_IsLoaded = false;
	m_HasError = false;
	m_aLoadedPath[0] = '\0';
	m_LastConfigEnabled = -1;
	m_aLastConfigPath[0] = '\0';
	m_LastDecodeStepTick = 0;
	m_LastPerfReportTick = 0;
	m_LastUpdateCostTick = 0;
	m_MaxUpdateCostTick = 0;
	m_TotalUpdateCostTick = 0;
	m_UpdateSamples = 0;
	str_copy(m_aStatus, Localize("Disabled."), sizeof(m_aStatus));
}

void CMenuMediaBackground::Shutdown()
{
	Unload();
	m_pGraphics = nullptr;
	m_pStorage = nullptr;
}

bool CMenuMediaBackground::LoadStaticMedia(const char *pPath, int StorageType)
{
	m_IsVideo = false;

	const std::string Ext = MediaDecoder::ExtractExtensionLower(pPath);
	void *pData = nullptr;
	unsigned DataSize = 0;
	if(!m_pStorage->ReadFile(pPath, StorageType, &pData, &DataSize))
	{
		SetError(Localize("Failed to read file."));
		return false;
	}

	const bool AnimatedImage = MediaDecoder::IsLikelyAnimatedImageExtension(Ext);
	SMediaDecodedFrames DecodedFrames;
	bool Success = false;
	if(AnimatedImage)
	{
		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = MENU_MEDIA_MAX_IMAGE_DIMENSION;
		Limits.m_MaxFrames = MENU_MEDIA_MAX_ANIMATION_FRAMES;
		Limits.m_MaxTotalBytes = MENU_MEDIA_MAX_ANIMATION_BYTES;
		Limits.m_MaxAnimationDurationMs = 15000;
		Limits.m_DecodeAllFrames = true;
		Success = MediaDecoder::DecodeAnimatedImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, Limits);
		if(!Success)
			Success = MediaDecoder::DecodeStaticImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, MENU_MEDIA_MAX_IMAGE_DIMENSION);
	}
	else
	{
		SMediaDecodeLimits Limits;
		Limits.m_MaxDimension = MENU_MEDIA_MAX_IMAGE_DIMENSION;
		Limits.m_DecodeAllFrames = false;
		Success = MediaDecoder::DecodeImageWithFfmpegCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, Limits);
		if(!Success)
			Success = MediaDecoder::DecodeStaticImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, MENU_MEDIA_MAX_IMAGE_DIMENSION);
		if(!Success)
		{
			SMediaDecodeLimits Limits;
			Limits.m_MaxDimension = MENU_MEDIA_MAX_IMAGE_DIMENSION;
			Limits.m_MaxFrames = MENU_MEDIA_MAX_ANIMATION_FRAMES;
			Limits.m_MaxTotalBytes = MENU_MEDIA_MAX_ANIMATION_BYTES;
			Limits.m_MaxAnimationDurationMs = 15000;
			Limits.m_DecodeAllFrames = true;
			Success = MediaDecoder::DecodeAnimatedImageCpu(m_pGraphics, static_cast<unsigned char *>(pData), DataSize, pPath, DecodedFrames, Limits);
		}
	}
	free(pData);

	if(!Success)
	{
		char aAbsolutePath[IO_MAX_PATH_LENGTH];
		CImageInfo FallbackImage;
		if(ResolveReadablePath(m_pStorage, pPath, StorageType, aAbsolutePath, sizeof(aAbsolutePath)) &&
			(DecodeImageWithWic(aAbsolutePath, FallbackImage, MENU_MEDIA_MAX_IMAGE_DIMENSION) ||
				DecodeFirstFrameFromFile(aAbsolutePath, FallbackImage, MENU_MEDIA_MAX_IMAGE_DIMENSION)))
		{
			MediaDecoder::UnloadFrames(m_pGraphics, m_vFrames);
			const int ImageWidth = (int)FallbackImage.m_Width;
			const int ImageHeight = (int)FallbackImage.m_Height;
			SMediaFrame Frame;
			Frame.m_Texture = m_pGraphics->LoadTextureRawMove(FallbackImage, 0, pPath);
			if(!Frame.m_Texture.IsValid())
			{
				FallbackImage.Free();
				SetError(Localize("Failed to upload image."));
				return false;
			}

			m_vFrames.push_back(Frame);
			m_Animated = false;
			m_Width = ImageWidth;
			m_Height = ImageHeight;
			m_AnimationStart = time_get();
			m_IsLoaded = true;
			SetStatus(Localize("Loaded image."));
			return true;
		}

		SetError(Localize("Failed to decode image."));
		return false;
	}

	const int DecodedWidth = DecodedFrames.m_Width;
	const int DecodedHeight = DecodedFrames.m_Height;
	const bool DecodedAnimated = DecodedFrames.m_Animated;
	std::vector<SMediaFrame> vNewFrames;
	if(!MediaDecoder::UploadFrames(m_pGraphics, DecodedFrames, vNewFrames, pPath))
	{
		SetError(Localize("Failed to upload image."));
		return false;
	}

	MediaDecoder::UnloadFrames(m_pGraphics, m_vFrames);
	m_vFrames = std::move(vNewFrames);
	m_Animated = DecodedAnimated && m_vFrames.size() > 1;
	m_Width = DecodedWidth;
	m_Height = DecodedHeight;
	m_AnimationStart = time_get();
	m_IsLoaded = true;
	SetStatus(m_Animated ? "Loaded animated image." : "Loaded image.");
	return true;
}

bool CMenuMediaBackground::UploadCurrentVideoFrame(const char *pContextName, int DurationMs)
{
#if !defined(CONF_VIDEORECORDER)
	(void)pContextName;
	(void)DurationMs;
	return false;
#else
	if(m_pGraphics == nullptr || m_pFrame == nullptr || m_pFrameRgba == nullptr || m_pSwsCtx == nullptr || m_Width <= 0 || m_Height <= 0)
		return false;

	const size_t FrameBytes = (size_t)m_Width * (size_t)m_Height * 4ull;
	if(m_vVideoUploadBuffer.size() != FrameBytes)
		m_vVideoUploadBuffer.resize(FrameBytes);
	CImageInfo Image;
	Image.m_Width = m_Width;
	Image.m_Height = m_Height;
	Image.m_Format = CImageInfo::FORMAT_RGBA;

	if(av_frame_make_writable(m_pFrameRgba) < 0)
		return false;

	sws_scale(m_pSwsCtx, m_pFrame->data, m_pFrame->linesize, 0, m_Height, m_pFrameRgba->data, m_pFrameRgba->linesize);
	for(int y = 0; y < m_Height; ++y)
		mem_copy(m_vVideoUploadBuffer.data() + (size_t)y * (size_t)m_Width * 4ull, m_pFrameRgba->data[0] + (size_t)y * (size_t)m_pFrameRgba->linesize[0], (size_t)m_Width * 4ull);
	Image.m_pData = m_vVideoUploadBuffer.data();
	if(!m_VideoTexture.IsValid())
	{
		m_VideoTexture = m_pGraphics->LoadTextureRaw(Image, 0, pContextName);
		if(!m_VideoTexture.IsValid())
			return false;
	}
	else
	{
		m_pGraphics->UnloadTexture(&m_VideoTexture);
		m_VideoTexture = m_pGraphics->LoadTextureRaw(Image, 0, pContextName);
		if(!m_VideoTexture.IsValid())
			return false;
	}

	m_LastVideoPts = m_pFrame->best_effort_timestamp;
	m_NextFrameTime = time_get_nanoseconds() + std::chrono::milliseconds(std::clamp(DurationMs, 1, MENU_MEDIA_MAX_VIDEO_FRAME_MS));
	return true;
#endif
}

bool CMenuMediaBackground::DecodeNextVideoFrame(bool LoopOnEof)
{
#if !defined(CONF_VIDEORECORDER)
	(void)LoopOnEof;
	return false;
#else
	if(!m_pFormatCtx || !m_pCodecCtx || !m_pPacket || !m_pFrame)
		return false;

	while(true)
	{
		while(avcodec_receive_frame(m_pCodecCtx, m_pFrame) == 0)
		{
			int DurationMs = MENU_MEDIA_DEFAULT_VIDEO_FRAME_MS;
			int64_t DurationTs = 0;
			if(m_LastVideoPts != MENU_MEDIA_PTS_UNSET && m_pFrame->best_effort_timestamp != AV_NOPTS_VALUE)
				DurationTs = m_pFrame->best_effort_timestamp - m_LastVideoPts;
			if(DurationTs > 0)
			{
				const int64_t Rescaled = av_rescale_q(DurationTs, m_pFormatCtx->streams[m_VideoStream]->time_base, AVRational{1, 1000});
				if(Rescaled > 0)
					DurationMs = (int)Rescaled;
			}
			return UploadCurrentVideoFrame(m_aLoadedPath, DurationMs);
		}

		const int ReadResult = av_read_frame(m_pFormatCtx, m_pPacket);
		if(ReadResult < 0)
		{
			if(!LoopOnEof)
				return false;

			av_seek_frame(m_pFormatCtx, m_VideoStream, 0, AVSEEK_FLAG_BACKWARD);
			avcodec_flush_buffers(m_pCodecCtx);
			m_LastVideoPts = MENU_MEDIA_PTS_UNSET;
			continue;
		}

		if(m_pPacket->stream_index != m_VideoStream)
		{
			av_packet_unref(m_pPacket);
			continue;
		}

		const int SendResult = avcodec_send_packet(m_pCodecCtx, m_pPacket);
		av_packet_unref(m_pPacket);
		if(SendResult < 0)
			return false;
	}
#endif
}

bool CMenuMediaBackground::LoadVideo(const char *pPath, int StorageType)
{
#if !defined(CONF_VIDEORECORDER)
	(void)pPath;
	(void)StorageType;
	m_IsVideo = true;
	SetError(Localize("Video backgrounds are unavailable in this build."));
	return false;
#else
	m_IsVideo = true;

	char aPath[IO_MAX_PATH_LENGTH];
	if(!ResolveReadablePath(m_pStorage, pPath, StorageType, aPath, sizeof(aPath)))
	{
		SetError(Localize("Failed to read file."));
		return false;
	}

	if(avformat_open_input(&m_pFormatCtx, aPath, nullptr, nullptr) != 0)
	{
		SetError(Localize("Failed to open video."));
		return false;
	}
	if(avformat_find_stream_info(m_pFormatCtx, nullptr) < 0)
	{
		SetError(Localize("Failed to read video info."));
		return false;
	}

	m_VideoStream = av_find_best_stream(m_pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if(m_VideoStream < 0)
	{
		SetError(Localize("No video stream found."));
		return false;
	}

	const AVStream *pStream = m_pFormatCtx->streams[m_VideoStream];
	const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
	if(!pCodec)
	{
		SetError(Localize("Unsupported video codec."));
		ClearVideoState();
		return false;
	}

	m_pCodecCtx = avcodec_alloc_context3(pCodec);
	if(!m_pCodecCtx || avcodec_parameters_to_context(m_pCodecCtx, pStream->codecpar) < 0 || avcodec_open2(m_pCodecCtx, pCodec, nullptr) < 0)
	{
		SetError(Localize("Failed to initialize video decoder."));
		ClearVideoState();
		return false;
	}

	m_Width = m_pCodecCtx->width;
	m_Height = m_pCodecCtx->height;
	if(m_Width <= 0 || m_Height <= 0)
	{
		SetError(Localize("Invalid video dimensions."));
		ClearVideoState();
		return false;
	}

	m_pFrame = av_frame_alloc();
	m_pFrameRgba = av_frame_alloc();
	m_pPacket = av_packet_alloc();
	if(!m_pFrame || !m_pFrameRgba || !m_pPacket)
	{
		SetError(Localize("Failed to allocate video frames."));
		ClearVideoState();
		return false;
	}

	m_pFrameRgba->format = AV_PIX_FMT_RGBA;
	m_pFrameRgba->width = m_Width;
	m_pFrameRgba->height = m_Height;
	if(av_frame_get_buffer(m_pFrameRgba, 1) < 0)
	{
		SetError(Localize("Failed to allocate RGBA frame."));
		ClearVideoState();
		return false;
	}

	m_pSwsCtx = sws_getContext(m_Width, m_Height, m_pCodecCtx->pix_fmt, m_Width, m_Height, AV_PIX_FMT_RGBA, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if(!m_pSwsCtx)
	{
		SetError(Localize("Failed to initialize video scaler."));
		ClearVideoState();
		return false;
	}

	if(!DecodeNextVideoFrame(true))
	{
		SetError(Localize("Failed to decode first video frame."));
		ClearVideoState();
		return false;
	}

	m_IsLoaded = true;
	SetStatus(Localize("Loaded video."));
	return true;
#endif
}

void CMenuMediaBackground::ReloadFromConfig(int Enabled, const char *pPath)
{
	char aPath[IO_MAX_PATH_LENGTH];
	str_copy(aPath, pPath, sizeof(aPath));

	Unload();

	m_LastConfigEnabled = Enabled;
	str_copy(m_aLastConfigPath, aPath, sizeof(m_aLastConfigPath));

	if(!Enabled)
	{
		SetStatus(Localize("Disabled."));
		return;
	}
	if(aPath[0] == '\0')
	{
		SetError(Localize("No media selected."));
		return;
	}

	const int StorageType = fs_is_relative_path(aPath) ? IStorage::TYPE_ALL : IStorage::TYPE_ABSOLUTE;
	const std::string Ext = MediaDecoder::ExtractExtensionLower(aPath);

	bool Success = false;
	if(MediaDecoder::IsLikelyVideoExtension(Ext))
		Success = LoadVideo(aPath, StorageType);
	else
		Success = LoadStaticMedia(aPath, StorageType);

	if(Success)
		str_copy(m_aLoadedPath, aPath, sizeof(m_aLoadedPath));
}

void CMenuMediaBackground::SyncFromConfig(int Enabled, const char *pPath)
{
	if(m_LastConfigEnabled != Enabled || str_comp(m_aLastConfigPath, pPath) != 0)
		ReloadFromConfig(Enabled, pPath);
}

void CMenuMediaBackground::ReloadFromConfig()
{
	ReloadFromConfig(g_Config.m_MaMenuMediaBackground, g_Config.m_MaMenuMediaBackgroundPath);
}

void CMenuMediaBackground::SyncFromConfig()
{
	SyncFromConfig(g_Config.m_MaMenuMediaBackground, g_Config.m_MaMenuMediaBackgroundPath);
}

void CMenuMediaBackground::OnWindowResize()
{
	if(m_IsLoaded || m_HasError)
		Unload();
}

void CMenuMediaBackground::Update()
{
	if(!m_IsLoaded || !m_IsVideo)
	{
		return;
	}

	const int64_t PerfStart = time_get();
	const auto Now = time_get_nanoseconds();
	const int64_t DecodeStepTick = time_get();
	if(m_LastDecodeStepTick != 0 && DecodeStepTick - m_LastDecodeStepTick < time_freq() / 60)
		return;
	m_LastDecodeStepTick = DecodeStepTick;

	int Guard = 0;
	while(Now >= m_NextFrameTime && Guard < 1)
	{
		if(!DecodeNextVideoFrame(true))
		{
			SetError(Localize("Failed while decoding video."));
			m_IsLoaded = false;
			ClearVideoState();
			break;
		}
		++Guard;
	}
	if(Guard == 1 && Now >= m_NextFrameTime)
	{
		m_NextFrameTime = Now + std::chrono::milliseconds(MENU_MEDIA_DEFAULT_VIDEO_FRAME_MS);
	}

	m_LastUpdateCostTick = time_get() - PerfStart;
	m_MaxUpdateCostTick = maximum(m_MaxUpdateCostTick, m_LastUpdateCostTick);
	m_TotalUpdateCostTick += m_LastUpdateCostTick;
	++m_UpdateSamples;
	if(g_Config.m_Debug)
	{
		const int64_t PerfNow = time_get();
		if(m_LastPerfReportTick == 0 || PerfNow - m_LastPerfReportTick >= time_freq())
		{
			dbg_msg("menumedia/perf", "update last=%.3fms avg=%.3fms max=%.3fms samples=%lld loaded=%d video=%d",
				m_LastUpdateCostTick * 1000.0 / (double)time_freq(),
				m_UpdateSamples > 0 ? (m_TotalUpdateCostTick * 1000.0 / (double)time_freq()) / (double)m_UpdateSamples : 0.0,
				m_MaxUpdateCostTick * 1000.0 / (double)time_freq(),
				(long long)m_UpdateSamples,
				m_IsLoaded ? 1 : 0,
				m_IsVideo ? 1 : 0);
			m_LastPerfReportTick = PerfNow;
			m_TotalUpdateCostTick = 0;
			m_UpdateSamples = 0;
			m_MaxUpdateCostTick = 0;
		}
	}
}

void CMenuMediaBackground::RenderTextureCover(IGraphics::CTextureHandle Texture, int Width, int Height, float TargetCenterX, float TargetCenterY, float TargetWidth, float TargetHeight, float Alpha)
{
	if(TargetWidth <= 1.0f || TargetHeight <= 1.0f ||
		!std::isfinite(TargetCenterX) || !std::isfinite(TargetCenterY) ||
		!std::isfinite(TargetWidth) || !std::isfinite(TargetHeight))
		return;

	const float TextureAspect = Width > 0 && Height > 0 ? (float)Width / (float)Height : 1.0f;
	const float TargetAspect = TargetHeight > 0.0f ? TargetWidth / TargetHeight : 1.0f;

	float DrawW = TargetWidth;
	float DrawH = TargetHeight;

	if(TextureAspect > TargetAspect)
	{
		DrawH = TargetHeight;
		DrawW = DrawH * TextureAspect;
	}
	else
	{
		DrawW = TargetWidth;
		DrawH = DrawW / std::max(TextureAspect, 0.001f);
	}
	const float DrawX = TargetCenterX - DrawW * 0.5f;
	const float DrawY = TargetCenterY - DrawH * 0.5f;

	m_pGraphics->TextureSet(Texture);
	m_pGraphics->WrapClamp();
	m_pGraphics->QuadsBegin();
	m_pGraphics->QuadsSetSubset(0.0f, 0.0f, 1.0f, 1.0f);
	m_pGraphics->SetColor(1.0f, 1.0f, 1.0f, std::clamp(Alpha, 0.0f, 1.0f));
	const IGraphics::CQuadItem Quad(DrawX, DrawY, DrawW, DrawH);
	m_pGraphics->QuadsDrawTL(&Quad, 1);
	m_pGraphics->QuadsEnd();
	m_pGraphics->SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	m_pGraphics->WrapNormal();
}

bool CMenuMediaBackground::Render(float ScreenWidth, float ScreenHeight, const SRenderContext *pRenderContext)
{
	if(!m_IsLoaded || m_pGraphics == nullptr)
		return false;
	if(ScreenWidth <= 1.0f || ScreenHeight <= 1.0f || !std::isfinite(ScreenWidth) || !std::isfinite(ScreenHeight))
		return false;

	IGraphics::CTextureHandle Texture;
	if(m_IsVideo)
		Texture = m_VideoTexture;
	else if(!MediaDecoder::GetCurrentFrameTexture(m_vFrames, m_Animated, m_AnimationStart, Texture))
		return false;

	if(!Texture.IsValid())
		return false;

	float PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1;
	m_pGraphics->GetScreen(&PrevScreenX0, &PrevScreenY0, &PrevScreenX1, &PrevScreenY1);
	m_pGraphics->BlendNormal();

	const bool UseWorldOffset = pRenderContext != nullptr &&
				    pRenderContext->m_WorldOffset > 0.0f &&
				    pRenderContext->m_ViewWidth > 0.0f &&
				    pRenderContext->m_ViewHeight > 0.0f &&
				    std::isfinite(pRenderContext->m_CameraCenterX) &&
				    std::isfinite(pRenderContext->m_CameraCenterY) &&
				    std::isfinite(pRenderContext->m_ViewWidth) &&
				    std::isfinite(pRenderContext->m_ViewHeight) &&
				    std::isfinite(pRenderContext->m_MapWidth) &&
				    std::isfinite(pRenderContext->m_MapHeight);
	const float Alpha = pRenderContext != nullptr ? std::clamp(pRenderContext->m_Alpha, 0.0f, 1.0f) : 1.0f;

	if(UseWorldOffset)
	{
		const float WorldOffset = std::clamp(pRenderContext->m_WorldOffset, 0.0f, 1.0f);
		const float ViewWidth = pRenderContext->m_ViewWidth;
		const float ViewHeight = pRenderContext->m_ViewHeight;
		const float MapWidth = std::max(pRenderContext->m_MapWidth, ViewWidth);
		const float MapHeight = std::max(pRenderContext->m_MapHeight, ViewHeight);
		const float MapCenterX = MapWidth * 0.5f;
		const float MapCenterY = MapHeight * 0.5f;
		const float TargetCenterX = pRenderContext->m_CameraCenterX + (MapCenterX - pRenderContext->m_CameraCenterX) * WorldOffset;
		const float TargetCenterY = pRenderContext->m_CameraCenterY + (MapCenterY - pRenderContext->m_CameraCenterY) * WorldOffset;
		const float TargetWidth = ViewWidth + (MapWidth - ViewWidth) * WorldOffset;
		const float TargetHeight = ViewHeight + (MapHeight - ViewHeight) * WorldOffset;

		m_pGraphics->MapScreen(
			pRenderContext->m_CameraCenterX - ViewWidth * 0.5f,
			pRenderContext->m_CameraCenterY - ViewHeight * 0.5f,
			pRenderContext->m_CameraCenterX + ViewWidth * 0.5f,
			pRenderContext->m_CameraCenterY + ViewHeight * 0.5f);
		RenderTextureCover(Texture, m_Width, m_Height, TargetCenterX, TargetCenterY, TargetWidth, TargetHeight, Alpha);
	}
	else
	{
		m_pGraphics->MapScreen(0.0f, 0.0f, ScreenWidth, ScreenHeight);
		RenderTextureCover(Texture, m_Width, m_Height, ScreenWidth * 0.5f, ScreenHeight * 0.5f, ScreenWidth, ScreenHeight, Alpha);
	}

	m_pGraphics->TextureClear();
	m_pGraphics->MapScreen(PrevScreenX0, PrevScreenY0, PrevScreenX1, PrevScreenY1);
	return true;
}
