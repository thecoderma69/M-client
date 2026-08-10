#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <objbase.h>
#include <wincodec.h>
#include <shlwapi.h>

static bool IsGifSig(const unsigned char *pData, size_t DataSize)
{
	return DataSize >= 6 && (memcmp(pData, "GIF87a", 6) == 0 || memcmp(pData, "GIF89a", 6) == 0);
}

struct WicRawFrame
{
	uint8_t *m_pData;
	int m_Width;
	int m_Height;
	int m_DurationMs;
};

struct WicDecodedFrames
{
	WicRawFrame *m_pFrames;
	int m_FrameCount;
	int m_Width;
	int m_Height;
};

static int MetadataInt(IWICMetadataQueryReader *pReader, const wchar_t *pName, int DefaultVal)
{
	if(!pReader) return DefaultVal;
	PROPVARIANT v;
	PropVariantInit(&v);
	int val = DefaultVal;
	if(SUCCEEDED(pReader->GetMetadataByName(pName, &v)))
	{
		if(v.vt == VT_UI2) val = (int)v.uiVal;
		else if(v.vt == VT_UI4) val = (int)v.ulVal;
	}
	PropVariantClear(&v);
	return val;
}

extern "C" {

bool WicDecodeGif(const unsigned char *pData, size_t DataSize, WicDecodedFrames *pOut, int MaxDimension, int MaxFrames, bool DecodeAllFrames)
{
	if(!IsGifSig(pData, DataSize))
		return false;

	IWICImagingFactory *pFactory = NULL;
	if(FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (void **)&pFactory)))
		return false;

	IStream *pStream = SHCreateMemStream(pData, (UINT)DataSize);
	if(!pStream) { pFactory->Release(); return false; }

	IWICBitmapDecoder *pDecoder = NULL;
	HRESULT hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
	pStream->Release();
	if(FAILED(hr)) { pFactory->Release(); return false; }

	UINT FrameCount = 0;
	hr = pDecoder->GetFrameCount(&FrameCount);
	if(FAILED(hr) || FrameCount == 0) { pDecoder->Release(); pFactory->Release(); return false; }

	if(!DecodeAllFrames) FrameCount = 1;
	if(MaxFrames > 0 && (int)FrameCount > MaxFrames) FrameCount = (UINT)MaxFrames;

	int MaxDim = MaxDimension > 0 ? MaxDimension : 960;

	// Get global GIF dimensions from first frame's global metadata
	int GlobalW = 0, GlobalH = 0;
	{
		IWICBitmapFrameDecode *pFirstFrame = NULL;
		if(SUCCEEDED(pDecoder->GetFrame(0, &pFirstFrame)))
		{
			IWICMetadataQueryReader *pMeta = NULL;
			if(SUCCEEDED(pFirstFrame->GetMetadataQueryReader(&pMeta)))
			{
				GlobalW = MetadataInt(pMeta, L"/logscrdesc/Width", 0);
				GlobalH = MetadataInt(pMeta, L"/logscrdesc/Height", 0);
				pMeta->Release();
			}
			pFirstFrame->Release();
		}
	}
	if(GlobalW <= 0 || GlobalH <= 0)
	{
		// Fallback: use first frame size
		IWICBitmapFrameDecode *pFF = NULL;
		if(SUCCEEDED(pDecoder->GetFrame(0, &pFF)))
		{
			UINT fw = 0, fh = 0;
			pFF->GetSize(&fw, &fh);
			GlobalW = (int)fw;
			GlobalH = (int)fh;
			pFF->Release();
		}
	}
	if(GlobalW <= 0 || GlobalH <= 0) { pDecoder->Release(); pFactory->Release(); return false; }

	// Clamp global dimensions to MaxDim
	int OutW = GlobalW, OutH = GlobalH;
	if(OutW > MaxDim || OutH > MaxDim)
	{
		float scale = (float)MaxDim / (float)(OutW > OutH ? OutW : OutH);
		OutW = (int)((float)GlobalW * scale);
		OutH = (int)((float)GlobalH * scale);
		if(OutW < 1) OutW = 1;
		if(OutH < 1) OutH = 1;
	}

	size_t CanvasBytes = (size_t)OutW * OutH * 4;
	uint8_t *pCanvas = (uint8_t *)calloc(1, CanvasBytes);
	if(!pCanvas) { pDecoder->Release(); pFactory->Release(); return false; }

	WicRawFrame *pFrames = (WicRawFrame *)calloc(FrameCount, sizeof(WicRawFrame));
	if(!pFrames) { free(pCanvas); pDecoder->Release(); pFactory->Release(); return false; }

	UINT ActualFrames = 0;
	UINT i;

	for(i = 0; i < FrameCount; i++)
	{
		IWICBitmapFrameDecode *pFrameDecode = NULL;
		hr = pDecoder->GetFrame(i, &pFrameDecode);
		if(FAILED(hr)) { if(i > 0) break; goto cleanup_fail; }

		UINT fw = 0, fh = 0;
		pFrameDecode->GetSize(&fw, &fh);
		if(fw == 0 || fh == 0) { pFrameDecode->Release(); if(i > 0) break; goto cleanup_fail; }

		// Get frame metadata
		int FrameX = 0, FrameY = 0, FrameW = (int)fw, FrameH = (int)fh;
		IWICMetadataQueryReader *pMeta = NULL;
		if(SUCCEEDED(pFrameDecode->GetMetadataQueryReader(&pMeta)))
		{
			FrameX = MetadataInt(pMeta, L"/imgdesc/Left", 0);
			FrameY = MetadataInt(pMeta, L"/imgdesc/Top", 0);
			int imgW = MetadataInt(pMeta, L"/imgdesc/Width", (int)fw);
			int imgH = MetadataInt(pMeta, L"/imgdesc/Height", (int)fh);
			if(imgW > 0) FrameW = imgW;
			if(imgH > 0) FrameH = imgH;
			pMeta->Release();
		}

		// Scale frame position to output canvas
		float WScale = (float)OutW / (float)GlobalW;
		float HScale = (float)OutH / (float)GlobalH;
		int OutX = (int)((float)FrameX * WScale);
		int OutY = (int)((float)FrameY * HScale);
		int OutFw = (int)((float)FrameW * WScale);
		int OutFh = (int)((float)FrameH * HScale);
		if(OutFw < 1) OutFw = 1;
		if(OutFh < 1) OutFh = 1;

		// Clamp to canvas bounds
		if(OutX < 0) { OutFw += OutX; OutX = 0; }
		if(OutY < 0) { OutFh += OutY; OutY = 0; }
		if(OutX + OutFw > OutW) OutFw = OutW - OutX;
		if(OutY + OutFh > OutH) OutFh = OutH - OutY;
		if(OutFw <= 0 || OutFh <= 0) { pFrameDecode->Release(); if(i > 0) break; goto cleanup_fail; }

		// Decode frame to RGBA at target size
		IWICBitmapScaler *pScaler = NULL;
		IWICBitmapSource *pSource = (IWICBitmapSource *)pFrameDecode;
		if(OutFw != (int)fw || OutFh != (int)fh)
		{
			if(SUCCEEDED(pFactory->CreateBitmapScaler(&pScaler)))
				pScaler->Initialize((IWICBitmapSource *)pFrameDecode, OutFw, OutFh, WICBitmapInterpolationModeFant);
			if(pScaler) pSource = (IWICBitmapSource *)pScaler;
		}

		IWICFormatConverter *pConverter = NULL;
		if(FAILED(pFactory->CreateFormatConverter(&pConverter)))
		{
			if(pScaler) pScaler->Release();
			pFrameDecode->Release();
			if(i > 0) break;
			goto cleanup_fail;
		}

		hr = pConverter->Initialize(pSource, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeCustom);
		if(FAILED(hr))
		{
			pConverter->Release();
			if(pScaler) pScaler->Release();
			pFrameDecode->Release();
			if(i > 0) break;
			goto cleanup_fail;
		}

		size_t FrameBytes = (size_t)OutFw * OutFh * 4;
		uint8_t *pFramePixels = (uint8_t *)malloc(FrameBytes);
		if(!pFramePixels)
		{
			pConverter->Release();
			if(pScaler) pScaler->Release();
			pFrameDecode->Release();
			if(i > 0) break;
			goto cleanup_fail;
		}

		WICRect rc = {0, 0, OutFw, OutFh};
		hr = pConverter->CopyPixels(&rc, OutFw * 4, (UINT)FrameBytes, pFramePixels);
		pConverter->Release();
		if(pScaler) pScaler->Release();
		pFrameDecode->Release();

		if(FAILED(hr))
		{
			free(pFramePixels);
			if(i > 0) break;
			goto cleanup_fail;
		}

		// Get frame delay
		int delayMs = 100;
		IWICMetadataQueryReader *pDelayMeta = NULL;
		if(SUCCEEDED(pDecoder->GetFrame(i, &pFrameDecode)))
		{
			if(SUCCEEDED(pFrameDecode->GetMetadataQueryReader(&pDelayMeta)))
			{
				delayMs = MetadataInt(pDelayMeta, L"/grctlext/Delay", 100);
				if(delayMs > 0) delayMs *= 10;
				else delayMs = 100;
				pDelayMeta->Release();
			}
			pFrameDecode->Release();
		}
		if(delayMs < 8) delayMs = 8;
		if(delayMs > 10000) delayMs = 10000;

		// Get disposal - for frame 0 or "restore to background", clear canvas first
		int disposal = 0;
		IWICBitmapFrameDecode *pDF = NULL;
		if(SUCCEEDED(pDecoder->GetFrame(i, &pDF)))
		{
			IWICMetadataQueryReader *pDMeta = NULL;
			if(SUCCEEDED(pDF->GetMetadataQueryReader(&pDMeta)))
			{
				disposal = MetadataInt(pDMeta, L"/grctlext/Disposal", 0);
				pDMeta->Release();
			}
			pDF->Release();
		}
		if(i == 0 || disposal == 2) // Restore to background
			memset(pCanvas, 0, CanvasBytes);

		// Composite frame onto canvas with alpha blending
		for(int y = 0; y < OutFh; y++)
		{
			for(int x = 0; x < OutFw; x++)
			{
				size_t srcOff = (size_t)(y * OutFw + x) * 4;
				size_t dstOff = (size_t)((OutY + y) * OutW + (OutX + x)) * 4;
				uint8_t srcA = pFramePixels[srcOff + 3];
				if(srcA == 0) continue;
				if(srcA == 255)
				{
					memcpy(pCanvas + dstOff, pFramePixels + srcOff, 4);
					continue;
				}
				// Alpha blend: dst = src * a + dst * (1-a)
				uint8_t srcR = pFramePixels[srcOff];
				uint8_t srcG = pFramePixels[srcOff + 1];
				uint8_t srcB = pFramePixels[srcOff + 2];
				uint8_t dstR = pCanvas[dstOff];
				uint8_t dstG = pCanvas[dstOff + 1];
				uint8_t dstB = pCanvas[dstOff + 2];
				uint8_t dstA = pCanvas[dstOff + 3];
				int invA = 255 - srcA;
				pCanvas[dstOff] = (uint8_t)((srcR * srcA + dstR * invA) / 255);
				pCanvas[dstOff + 1] = (uint8_t)((srcG * srcA + dstG * invA) / 255);
				pCanvas[dstOff + 2] = (uint8_t)((srcB * srcA + dstB * invA) / 255);
				pCanvas[dstOff + 3] = (uint8_t)(srcA + (dstA * invA) / 255);
			}
		}

		free(pFramePixels);

		// Copy canvas to output frame
		pFrames[i].m_pData = (uint8_t *)malloc(CanvasBytes);
		if(!pFrames[i].m_pData)
		{
			if(i > 0) break;
			goto cleanup_fail;
		}
		memcpy(pFrames[i].m_pData, pCanvas, CanvasBytes);
		pFrames[i].m_Width = OutW;
		pFrames[i].m_Height = OutH;
		pFrames[i].m_DurationMs = delayMs;
		ActualFrames++;
	}

	pDecoder->Release();
	pFactory->Release();
	free(pCanvas);

	if(ActualFrames == 0) { free(pFrames); return false; }

	pOut->m_pFrames = pFrames;
	pOut->m_FrameCount = (int)ActualFrames;
	pOut->m_Width = OutW;
	pOut->m_Height = OutH;
	return true;

cleanup_fail:
	for(UINT j = 0; j < i; j++) free(pFrames[j].m_pData);
	free(pFrames);
	free(pCanvas);
	pDecoder->Release();
	pFactory->Release();
	return false;
}

void WicFreeFrames(WicDecodedFrames *pFrames)
{
	if(!pFrames || !pFrames->m_pFrames) return;
	for(int i = 0; i < pFrames->m_FrameCount; i++)
		free(pFrames->m_pFrames[i].m_pData);
	free(pFrames->m_pFrames);
	pFrames->m_pFrames = NULL;
	pFrames->m_FrameCount = 0;
}

} // extern "C"
