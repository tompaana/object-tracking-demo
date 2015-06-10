#include "pch.h"

#include "BufferLock.h"
#include "ImageProcessingUtils.h"
#include "MessengerInterface.h"
#include "ObjectDetails.h"
#include "ObjectFinderEffect.h"

#include <wrl.h>
#include <robuffer.h>
#include <collection.h>
#include <vector>
#include <map>
#include <sstream>		
#include <algorithm>  //for str.remove
#include <wrl\module.h>

#pragma comment(lib, "d2d1")

using namespace Microsoft::WRL;
using namespace Platform::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;



inline void GetColorFilteredValues(
	BYTE* py, BYTE* pu, BYTE* pv,
	const BYTE& targetY, const BYTE& targetU, const BYTE& targetV, const BYTE& threshold, const bool &dimmUnselected)
{
	if (abs(*py - targetY) > threshold
		|| abs(*pu - targetU) > threshold
		|| abs(*pv - targetV) > threshold)
	{
		if (*py == SelectedPixelValue)
		{
			*py = 254;
		}
		else if (dimmUnselected)
		{
			*py = 0;
		}
	}
	else
	{
		*py = SelectedPixelValue;
	}

    // Uncomment the following to remove color
    /*
	*pu = 128;
	*pv = 128;
    */
}

// Convert UYVY image.

/*void TransformImage_UYVY(
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels)
{
	DWORD y = 0;
	const DWORD y0 = min(rcDest.bottom, dwHeightInPixels);

	// Lines above the destination rectangle.
	for (; y < rcDest.top; y++)
	{
		memcpy(pDest, pSrc, dwWidthInPixels * 2);
		pSrc += lSrcStride;
		pDest += lDestStride;
	}

	// Lines within the destination rectangle.
	for (; y < y0; y++)
	{
		WORD *pSrc_Pixel = (WORD*)pSrc;
		WORD *pDest_Pixel = (WORD*)pDest;

		for (DWORD x = 0; (x + 1) < dwWidthInPixels; x += 2)
		{
			// Byte order is U0 Y0 V0 Y1
			// Each WORD is a byte pair (U/V, Y)
			// Windows is little-endian so the order appears reversed.

			if (x >= rcDest.left && x < rcDest.right)
			{
				BYTE u = pSrc_Pixel[x] & 0x00FF;
				BYTE v = pSrc_Pixel[x + 1] & 0x00FF;

				TransformChroma(mat, &u, &v);

				pDest_Pixel[x] = (pSrc_Pixel[x] & 0xFF00) | u;
				pDest_Pixel[x + 1] = (pSrc_Pixel[x + 1] & 0xFF00) | v;
			}
			else
			{
#pragma warning(push)
#pragma warning(disable: 6385) 
#pragma warning(disable: 6386) 
				pDest_Pixel[x] = pSrc_Pixel[x];
				pDest_Pixel[x + 1] = pSrc_Pixel[x + 1];
#pragma warning(pop)
			}
		}

		pDest += lDestStride;
		pSrc += lSrcStride;
	}

	// Lines below the destination rectangle.
	for (; y < dwHeightInPixels; y++)
	{
		memcpy(pDest, pSrc, dwWidthInPixels * 2);
		pSrc += lSrcStride;
		pDest += lDestStride;
	}
}*/


//-------------------------------------------------------------------
// ChromaFilterTransformImageYUY2
//
// Converts YUY2 image. This mostly used by tablets and desktop 
// webcams.
//-------------------------------------------------------------------
void ChromaFilterTransformImageYUY2(
	ObjectDetails& objectDetails,
	const float* pTargetYUV,
	const BYTE& threshold,
    const bool& dimmFilteredPixels,
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels)
{
	DWORD y = 0;
	const DWORD y0 = min(rcDest.bottom, dwHeightInPixels);

	// Lines above the destination rectangle.
	for (; y < rcDest.top; y++)
	{
		memcpy(pDest, pSrc, dwWidthInPixels * 2);
		pSrc += lSrcStride;
		pDest += lDestStride;
	}

	const BYTE targetY = static_cast<BYTE>(pTargetYUV[0]);
	const BYTE targetU = static_cast<BYTE>(pTargetYUV[1]);
	const BYTE targetV = static_cast<BYTE>(pTargetYUV[2]);

	int greatestSelectedPixelsCountY = 0;
	int mostSelectedPixelsY = -1;
	int numberOfSelectedPixelsY = 0;
	int *numberOfSelectedPixelsX = new int[dwWidthInPixels];

	memset(numberOfSelectedPixelsX, 0, dwWidthInPixels * sizeof(int));

	// Lines within the destination rectangle.
	for (; y < y0; y++)
	{
		numberOfSelectedPixelsY = 0;

		WORD *pSrc_Pixel = (WORD*)pSrc;
		WORD *pDest_Pixel = (WORD*)pDest;

		for (UINT32 x = 0; (x + 1) < dwWidthInPixels; x += 2)
		{
			// Byte order is Y0 U0 Y1 V0
			// Each WORD is a byte pair (Y, U/V)
			// Windows is little-endian so the order appears reversed.

			if (x >= rcDest.left && x < rcDest.right)
			{
				BYTE yy = pSrc_Pixel[x] & 0x00FF;
				BYTE u = pSrc_Pixel[x] >> 8;
				BYTE v = pSrc_Pixel[x + 1] >> 8;

                GetColorFilteredValues(&yy, &u, &v, targetY, targetU, targetV, threshold, dimmFilteredPixels);

				pDest_Pixel[x] = (yy & 0x00FF) | (u << 8);
				pDest_Pixel[x + 1] = (yy & 0x00FF) | (v << 8);

				if (yy == SelectedPixelValue)
				{
					numberOfSelectedPixelsY++;
					numberOfSelectedPixelsX[x]++;
				}
			}
			else
			{
#pragma warning(push)
#pragma warning(disable: 6385) 
#pragma warning(disable: 6386) 
				pDest_Pixel[x] = pSrc_Pixel[x];
				pDest_Pixel[x + 1] = pSrc_Pixel[x + 1];
#pragma warning(pop)
			}
		}

		pDest += lDestStride;
		pSrc += lSrcStride;

		if (numberOfSelectedPixelsY > greatestSelectedPixelsCountY)
		{
			greatestSelectedPixelsCountY = numberOfSelectedPixelsY;
#pragma warning(push)
#pragma warning(disable: 4258)
			mostSelectedPixelsY = y;
#pragma warning(pop)
		}
	}

	// Lines below the destination rectangle.
	for (; y < dwHeightInPixels; y++)
	{
		memcpy(pDest, pSrc, dwWidthInPixels * 2);
		pSrc += lSrcStride;
		pDest += lDestStride;
	}

	int mostSelectedPixelsX = -1;
	int greatestSelectedPixelsCountX = 0;

	for (uint32 x = 0; x < dwWidthInPixels; ++x)
	{
		if (numberOfSelectedPixelsX[x] > greatestSelectedPixelsCountX)
		{
			greatestSelectedPixelsCountX = numberOfSelectedPixelsX[x];
			mostSelectedPixelsX = x;
		}
	}

	if (mostSelectedPixelsX >= 0 && mostSelectedPixelsY >= 0)
	{
		objectDetails._centerX = static_cast<UINT32>(mostSelectedPixelsX);
		objectDetails._centerY = static_cast<UINT32>(mostSelectedPixelsY);
		objectDetails._width = greatestSelectedPixelsCountY * 2;
		objectDetails._height = greatestSelectedPixelsCountX * 2;
	}

    delete[] numberOfSelectedPixelsX;
}


//-------------------------------------------------------------------
// ChromaFilterTransformImageNV12
//
// Converts NV12 image. This mostly used by phones.
//-------------------------------------------------------------------
void ChromaFilterTransformImageNV12(
	ObjectDetails& objectDetails,
	const float* pTargetYUV,
	const BYTE& threshold,
    const bool& dimmFilteredPixels,
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(2 * lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(2 * lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels)
{
	// NV12 is planar: Y plane, followed by packed U-V plane.

	BYTE* destBegin = pDest; // Keep the first index stored for later

	CopyMemory(pDest, pSrc, dwWidthInPixels * dwHeightInPixels);
	pDest += lDestStride * dwHeightInPixels;
	pSrc += lSrcStride * dwHeightInPixels;

	// U-V plane

	// NOTE: The U-V plane has 1/2 the number of lines as the Y plane.

	// Lines above the destination rectangle.
	const UINT32 numberOfLinesAboveDestRect = rcDest.top / 2;
	BYTE* yPlaneDest = destBegin;
	DWORD y = 0;

	if (numberOfLinesAboveDestRect > 0)
	{
		memcpy(pDest, pSrc, dwWidthInPixels * numberOfLinesAboveDestRect);
		pSrc += lSrcStride * numberOfLinesAboveDestRect;
		const LONG temp = lDestStride * numberOfLinesAboveDestRect;
		pDest += temp;
		yPlaneDest += temp * 2;
		y += numberOfLinesAboveDestRect - 1;
	}

	const BYTE targetY = static_cast<BYTE>(pTargetYUV[0]);
	const BYTE targetU = static_cast<BYTE>(pTargetYUV[1]);
	const BYTE targetV = static_cast<BYTE>(pTargetYUV[2]);

	int numberOfSelectedPixelsY = 0;
	int *numberOfSelectedPixelsX = new int[dwWidthInPixels];

	for (uint32 i = 0; i < dwWidthInPixels; ++i)
	{
		numberOfSelectedPixelsX[i] = 0;
	}

	int greatestSelectedPixelsCountY = 0;
	int mostSelectedPixelsY = -1;

	
	// Lines within the destination rectangle.
	const DWORD y0 = min(rcDest.bottom, dwHeightInPixels);
	const DWORD endOfDestinationRectangleY = y0 / 2;

	const DWORD rectWidth = (rcDest.right > dwWidthInPixels) ? 0 : rcDest.right - rcDest.left;
	const DWORD endOfDestinationRectangleX = (rectWidth == 0) ? dwWidthInPixels : rectWidth;

	BYTE yy = 0;
	BYTE u = 0;
	BYTE v = 0;

	for (; y < endOfDestinationRectangleY; ++y)
	{
		numberOfSelectedPixelsY = 0;

		if (rectWidth > 0 && rcDest.left > 0)
		{
			memcpy(pDest, pSrc, rcDest.left);
			pDest += rcDest.left;
			pSrc += rcDest.left;
			yPlaneDest += rcDest.left;
		}

		for (DWORD x = 0; (x + 1) < endOfDestinationRectangleX; x += 2)
		{
			/*BYTE y0 = yPlaneDest[x];
			BYTE y1 = yPlaneDest[x + 1];
			BYTE y2 = yPlaneDest[x + dwWidthInPixels];
			BYTE y3 = yPlaneDest[x + 1 + dwWidthInPixels];
			BYTE yy = (y0 + y1 + y2 + y3) / 4;*/
			yy = yPlaneDest[x];
			u = pSrc[x];
			v = pSrc[x + 1];

			GetColorFilteredValues(&yy, &u, &v, targetY, targetU, targetV, threshold, dimmFilteredPixels);

			pDest[x] = u;
			pDest[x + 1] = v;

			yPlaneDest[x] = yy;
			yPlaneDest[x + 1] = yy;
			yPlaneDest[x + dwWidthInPixels] = yy;
			yPlaneDest[x + 1 + dwWidthInPixels] = yy;

			if (yy == SelectedPixelValue)
			{
				numberOfSelectedPixelsY++;
				numberOfSelectedPixelsX[x]++;
			}
		}

		if (rectWidth > 0)
		{
			pDest += rectWidth;
			pSrc += rectWidth;
			yPlaneDest += rectWidth;
			memcpy(pDest, pSrc, dwWidthInPixels - rcDest.right);
			pDest += dwWidthInPixels - rcDest.right;
			pSrc += dwWidthInPixels - rcDest.right;
			yPlaneDest += dwWidthInPixels - rcDest.right + lDestStride;
		}
		else
		{
			pDest += lDestStride;
			pSrc += lSrcStride;
			yPlaneDest += lDestStride * 2;
		}

		if (numberOfSelectedPixelsY > greatestSelectedPixelsCountY)
		{
			greatestSelectedPixelsCountY = numberOfSelectedPixelsY;
#pragma warning(push)
#pragma warning(disable: 4258)
			mostSelectedPixelsY = y * 2;
#pragma warning(pop)
		}
	}

	// Lines below the destination rectangle.
	for (; y < dwHeightInPixels / 2; y++)
	{
		memcpy(pDest, pSrc, dwWidthInPixels);
		pSrc += lSrcStride;
		pDest += lDestStride;
	}

	int mostSelectedPixelsX = -1;
	int greatestSelectedPixelsCountX = 0;

	for (uint32 x = 0; x < dwWidthInPixels; ++x)
	{
		if (numberOfSelectedPixelsX[x] > greatestSelectedPixelsCountX)
		{
			greatestSelectedPixelsCountX = numberOfSelectedPixelsX[x];
			mostSelectedPixelsX = x + rcDest.left;
		}
	}

	if (mostSelectedPixelsX >= 0 && mostSelectedPixelsY >= 0)
	{
		objectDetails._centerX = static_cast<UINT32>(mostSelectedPixelsX);
		objectDetails._centerY = static_cast<UINT32>(mostSelectedPixelsY);
		objectDetails._width = greatestSelectedPixelsCountY * 2;
		objectDetails._height = greatestSelectedPixelsCountX * 2;
	}

    delete numberOfSelectedPixelsX;
}



//-------------------------------------------------------------------
// ImageProcessingUtils class
//-------------------------------------------------------------------


ImageProcessingUtils::ImageProcessingUtils()
{
}


ImageProcessingUtils::~ImageProcessingUtils()
{
}


//-------------------------------------------------------------------
// drawLine
//
//
//-------------------------------------------------------------------
void ImageProcessingUtils::drawLine(
    BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight,
    const D2D1_POINT_2U& point1, D2D1_POINT_2U& point2,
	const IMAGE_TRANSFORM_FUNCTION &pTransformFn,
    const UINT32& thickness,
    const BYTE& yy, const BYTE& u, const BYTE& v)
{
	// subtype = MFVideoFormat_NV12
	if (pTransformFn == ChromaFilterTransformImageNV12)
	{
		double a = 0;
		int b = 0;

		bool lineIsNotVertical = calculateLineEquation(point1, point2, &a, &b);
		D2D_RECT_U lineBoundingBox = this->lineBoundingBox(point1, point2, true);

		const UINT32 uvPlaneStart = imageWidth * imageHeight - 1;

		int yIndex1 = 0;
		int yIndex2 = 0;
		int yIndexForUVPlane = 0;

		UINT32 trueThickness = thickness * 2;
		int thicknessIndexReduction = ((int)ceil((double)thickness / 2) - 1) * 2;
		UINT32 xStartForThickness = 0;

		if (lineIsNotVertical)
		{
			int x = 0;
			int y = 0;

			for (x = lineBoundingBox.left; x < (int)lineBoundingBox.right && x < (int)imageWidth; x += 2)
			{
				y = (int)floor(a * x + b);

				if (y < 0)
				{
					continue;
				}

				yIndex1 = y * imageWidth;
				yIndex2 = (y + 1) * imageWidth;
				yIndexForUVPlane = uvPlaneStart + y / 2 * imageWidth;

				xStartForThickness = x - thicknessIndexReduction;

				for (UINT32 xx = xStartForThickness; xx < xStartForThickness + trueThickness && xx + 1 < imageWidth; xx += 2)
				{
					image[yIndex1 + xx] = yy;
					image[yIndex2 + xx] = yy;
					image[yIndexForUVPlane + xx] = v;
					image[yIndex1 + xx + 1] = yy;
					image[yIndex2 + xx + 1] = yy;
					image[yIndexForUVPlane + xx + 1] = u;
				}
			}

			for (y = lineBoundingBox.top; y < (int)lineBoundingBox.bottom && y < (int)imageHeight; ++y)
			{
				x = (int)round((y - b) / a);

				if (x % 2 != 0)
				{
					x -= 1;
				}

				yIndex1 = y * imageWidth;
				yIndex2 = (y + 1) * imageWidth;
				yIndexForUVPlane = uvPlaneStart + y / 2 * imageWidth;

				xStartForThickness = x - thicknessIndexReduction;

				for (UINT32 xx = xStartForThickness; xx < xStartForThickness + trueThickness && xx + 1 < imageWidth; xx += 2)
				{
					image[yIndex1 + xx] = yy;
					image[yIndex2 + xx] = yy;
					image[yIndexForUVPlane + xx] = v;
					image[yIndex1 + xx + 1] = yy;
					image[yIndex2 + xx + 1] = yy;
					image[yIndexForUVPlane + xx + 1] = u;
				}
			}
		}
		else
		{
			int x = lineBoundingBox.left;

			for (UINT32 y = lineBoundingBox.top; y < lineBoundingBox.bottom && y < imageHeight; ++y)
			{
				yIndex1 = y * imageWidth;
				yIndex2 = (y + 1) * imageWidth;
				yIndexForUVPlane = uvPlaneStart + y / 2 * imageWidth;

				for (UINT32 xx = (x - trueThickness >= 0) ? x - trueThickness : 0;
				xx < x + trueThickness && xx + 1 < imageWidth;
					xx += 2)
				{
					image[yIndex1 + xx] = yy;
					image[yIndex2 + xx] = yy;
					image[yIndexForUVPlane + xx] = v;
					image[yIndex1 + xx + 1] = yy;
					image[yIndex2 + xx + 1] = yy;
					image[yIndexForUVPlane + xx + 1] = u;
				}
			}
		}
	}
	// subtype = MFVideoFormat_YUY2
	else if (pTransformFn == ChromaFilterTransformImageYUY2)
	{
		double a = 0;
		int b = 0;
		D2D_RECT_U lineBoundingBox = this->lineBoundingBox(point1, point2, true);

#pragma warning(push)
#pragma warning(disable: 4018) 
		// Not vertical line
		if (calculateLineEquation(point1, point2, &a, &b))
		{
			if (lineBoundingBox.right - lineBoundingBox.left > lineBoundingBox.bottom - lineBoundingBox.top)
			{
				for (int x = lineBoundingBox.left; x < lineBoundingBox.right - 1 && x < imageWidth - 1; x += 2)
				{
					int y = (int)floor(a * x + b);

					if (y < 0 || y >= (int)imageHeight)
					{
						continue;
					}

					*(int*)(image + (((y * imageWidth) + x) << 1)) = (v << 24) | (yy << 16) | (u << 8) | (yy);
				}
			}
			else
			{
				for (int y = lineBoundingBox.top; y < lineBoundingBox.bottom && y < imageHeight; ++y)
				{
					int x = (int)round((y - b) / a);

					if (x % 2 != 0)
					{
						x -= 1;
					}

					*(int*)(image + (((y * imageWidth) + x) << 1)) = (v << 24) | (yy << 16) | (u << 8) | (yy);
				}
			}
		}
		else // vertical line
		{
			for (int y = lineBoundingBox.top; y < lineBoundingBox.bottom && y < imageHeight; ++y)
			{
				*(int*)(image + (((y * imageWidth) + lineBoundingBox.left) << 1)) = (v << 24) | (yy << 16) | (u << 8) | (yy);
			}
		}
#pragma warning(pop)
	}
}


//-------------------------------------------------------------------
// getImageSize
//
// Calculate the size of the buffer needed to store the image.
// fcc: The FOURCC code of the video format.
//-------------------------------------------------------------------
HRESULT ImageProcessingUtils::getImageSize(DWORD fcc, UINT32 width, UINT32 height, DWORD* cbImage)
{
	HRESULT hr = S_OK;

	switch (fcc)
	{
	case FOURCC_YUY2:
	case FOURCC_UYVY:
		// check overflow
		if ((width > MAXDWORD / 2) || (width * 2 > MAXDWORD / height))
		{
			hr = E_INVALIDARG;
		}
		else
		{
			// 16 bpp
			*cbImage = width * height * 2;
		}
		break;

	case FOURCC_NV12:
		// check overflow
		if ((height / 2 > MAXDWORD - height) || ((height + height / 2) > MAXDWORD / width))
		{
			hr = E_INVALIDARG;
		}
		else
		{
			// 12 bpp
			*cbImage = width * (height + (height / 2));
		}
		break;

	default:
		hr = E_FAIL;    // Unsupported type.
	}
	return hr;
}


//-------------------------------------------------------------------
// getDefaultStride
//
// Get the default stride for a video format. 
//-------------------------------------------------------------------
HRESULT ImageProcessingUtils::getDefaultStride(IMFMediaType* mediaType, LONG* stride)
{
	LONG lStride = 0;

	// Try to get the default stride from the media type.
    HRESULT hr = mediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride);
	if (FAILED(hr))
	{
		// Attribute not set. Try to calculate the default stride.
		GUID subtype = GUID_NULL;

		UINT32 width = 0;
		UINT32 height = 0;

		// Get the subtype and the image size.
        hr = mediaType->GetGUID(MF_MT_SUBTYPE, &subtype);

		if (SUCCEEDED(hr))
		{
            hr = MFGetAttributeSize(mediaType, MF_MT_FRAME_SIZE, &width, &height);
		}
		if (SUCCEEDED(hr))
		{
			if (subtype == MFVideoFormat_NV12)
			{
				lStride = width;
			}
			else if (subtype == MFVideoFormat_YUY2 || subtype == MFVideoFormat_UYVY)
			{
				lStride = ((width * 2) + 3) & ~3;
			}
			else
			{
				hr = E_INVALIDARG;
			}
		}

		// Set the attribute for later reference.
		if (SUCCEEDED(hr))
		{
            (void)mediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(lStride));
		}
	}
	if (SUCCEEDED(hr))
	{
		*stride = lStride;
	}
	return hr;
}


//-------------------------------------------------------------------
// Validate that a rectangle meets the following criteria:
//
//  - All coordinates are non-negative.
//  - The rectangle is not flipped (top > bottom, left > right)
//
// These are the requirements for the destination rectangle.
//-------------------------------------------------------------------
bool ImageProcessingUtils::validateRect(const RECT& rc)
{
	if (rc.left < 0 || rc.top < 0)
	{
		return false;
	}
	if (rc.left > rc.right || rc.top > rc.bottom)
	{
		return false;
	}
	return true;
}


bool ImageProcessingUtils::validateRect(const D2D_RECT_U &rect, const UINT32& totalWidth, const UINT32& totalHeight)
{
    if (rect.left < 0 || rect.left >= totalWidth
        || rect.right > totalWidth || rect.right <= rect.left
        || rect.top < 0 || rect.top >= totalHeight
        || rect.bottom > totalHeight || rect.bottom <= rect.top)
    {
        return false;
    }

    return true;
}


//------------------------------------------------------------------
// cropImage
//
// 
//------------------------------------------------------------------
BYTE* ImageProcessingUtils::cropImage(BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U &cropRect)
{
    if (!validateRect(cropRect, imageWidth, imageHeight))
    {
        // Invalid rect
        return NULL;
    }

    const UINT32 firstXIndex = cropRect.left;
    const UINT32 numberOfXLines = cropRect.right - firstXIndex;
    const UINT32 numberOfYLines = cropRect.bottom - cropRect.top;

    BYTE* croppedImage = new BYTE[numberOfXLines * numberOfYLines];
    BYTE* croppedImageBegin = croppedImage;

    image += imageWidth * cropRect.top;

    for (UINT32 y = 0; y < numberOfYLines; ++y)
    {
        image += firstXIndex;
        CopyMemory(croppedImage, image, numberOfXLines);
        croppedImage += numberOfXLines;
        image += imageWidth - firstXIndex;
    }

    return croppedImageBegin;
}


//------------------------------------------------------------------
// copyFrame
//
// Copies the given frame and returns a newly created byte array
// containing the frame data.
//------------------------------------------------------------------
BYTE* ImageProcessingUtils::copyFrame(BYTE* source, const size_t size)
{
    BYTE* destination = (BYTE*)malloc(size);
    memcpy(destination, source, size);
    return destination;
}


//------------------------------------------------------------------
// mergeFramesNV12
//
// Merges the two given frame into one at the given X coordinate.
//------------------------------------------------------------------
BYTE* ImageProcessingUtils::mergeFramesNV12(
    BYTE* leftFrame, BYTE* rightFrame, const UINT32& frameWidth, const UINT32& frameHeight, UINT32& joinX)
{
    if (joinX % 2 != 0)
    {
        joinX++;
    }

    if (joinX == 0 || joinX >= frameWidth)
    {
        return NULL;
    }

    const UINT32 frameSizeNV12 = (UINT32)(frameWidth * frameHeight * 1.5f);
	BYTE* mergedFrame = (BYTE*)malloc(frameSizeNV12 * sizeof(BYTE));
    BYTE* mergedFrameBegin = mergedFrame;
    const UINT32 leftSideLength = joinX;
    const UINT32 rightSideLength = frameWidth - joinX;

	for (UINT32 i = 0; i < frameSizeNV12; i += frameWidth)
    {
        CopyMemory(mergedFrame, leftFrame, leftSideLength);
        mergedFrame += leftSideLength;
		leftFrame += frameWidth;
		rightFrame += leftSideLength;

        CopyMemory(mergedFrame, rightFrame, rightSideLength);
        mergedFrame += rightSideLength;
        rightFrame += rightSideLength;
    }
    
    return mergedFrameBegin;
}


//------------------------------------------------------------------
// createObjectMap
//
//
// 
//------------------------------------------------------------------
UINT16* ImageProcessingUtils::createObjectMap(BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight, const IMAGE_TRANSFORM_FUNCTION &pTransformFn)
{
	BYTE* yyImage = NULL;

	// Create temporary array to pack all yy values used in the algorithm
	// TODO: optimization
	if (pTransformFn == ChromaFilterTransformImageNV12)
	{
		yyImage = image;
	}
	else if (pTransformFn == ChromaFilterTransformImageYUY2)
	{
		int len = imageWidth * imageHeight;
		yyImage = new BYTE[len];

		for (int i = 0; i < len; i++)
		{
			yyImage[i] = image[i * 2];
		}
	}
	else
	{
		return NULL;
	}

    UINT16* objectMap = new UINT16[imageWidth * imageHeight];
	memset(objectMap, 0, imageWidth * imageHeight * sizeof(UINT16));
    int currentIndex = 0;
	int cornerCase = 0;
    int pixelAboveIndex = 0;
    UINT16 highestObjectIndex = 0;
    UINT16 currentObjectIndex = 0;
    UINT16 bridgedObjectIndex = 0;
    bool previousPixelWasEmpty = true;

    for (UINT32 y = 0; y < imageHeight; ++y)
    {
		previousPixelWasEmpty = true;

        for (UINT32 x = 0; x < imageWidth; ++x)
        {
            if (yyImage[currentIndex] == SelectedPixelValue)
            {
                if (previousPixelWasEmpty)
                {
                    highestObjectIndex++;
                    currentObjectIndex = highestObjectIndex;
                }

                bridgedObjectIndex = objectIdOfPixelAbove(yyImage, objectMap, currentIndex, imageWidth);

                if (bridgedObjectIndex != 0 && bridgedObjectIndex != currentObjectIndex)
                {
                    UINT32 i = currentIndex;
                    bool needToCheckALineAbove = true;

                    do
                    {
                        --i;

                        if ((i + 1) % imageWidth == 0)
                        {
                            // We are at most right on Y axis
                            if (!needToCheckALineAbove)
                            {
                                break;
                            }
                            else
                            {
                                needToCheckALineAbove = false;
                            }
                        }

                        if (!needToCheckALineAbove
                            && objectIdOfPixelAbove(yyImage, objectMap, i, imageWidth) == currentObjectIndex)
                        {
                            needToCheckALineAbove = true;
                        }

                        if (objectMap[i] == currentObjectIndex)
                        {
                            objectMap[i] = bridgedObjectIndex;
                        }
                    }
                    while (i > 0);

                    currentObjectIndex = bridgedObjectIndex;
                }

                objectMap[currentIndex] = currentObjectIndex;
                previousPixelWasEmpty = false;
            }
            else // Empty pixel
            {
                previousPixelWasEmpty = true;
            }

            currentIndex++;
        }
    }

	// TODO: optimization
	if (pTransformFn == ChromaFilterTransformImageYUY2)
	{
		delete[] yyImage;
	}

    return objectMap;
}


//------------------------------------------------------------------
// organizeObjectMap
//
// Organizes the objects in the given object map so that the object
// IDs start from 1 and the last object ID is the same as the object
// count: 1, 2, 3, ..., n
//
// Returns the object count.
//------------------------------------------------------------------
UINT16 ImageProcessingUtils::organizeObjectMap(UINT16* objectMap, const UINT32& objectMapSize)
{
    std::vector<UINT16> objectIds;
    UINT16 currentId;

    for (UINT32 i = 0; i < objectMapSize; ++i)
    {
        currentId = objectMap[i];

        if (currentId != 0)
        {
            if (!idExists(objectIds, currentId))
            {
                objectIds.push_back(currentId);
            }
        }
    }

    if (objectIds.size() > 1)
    {
        std::sort(objectIds.begin(), objectIds.end(),
            [](UINT16 const& a, UINT16 const& b) { return a < b; });
    }
    
    for (UINT16 i = 0; i < objectIds.size(); ++i)
    {
        currentId = objectIds.at(i);

        if (currentId != i + 1)
        {
            for (UINT32 j = 0; j < objectMapSize; ++j)
            {
                if (objectMap[j] == currentId)
                {
                    objectMap[j] = i + 1;
                }
            }
        }
    }

    return objectIds.size();
}


//------------------------------------------------------------------
// extractSortedObjectPoints
//
// Note: The given object map has to be sorted (object IDs have to
// be 1, 2, 3, ..., n).
//------------------------------------------------------------------
std::vector<D2D_POINT_2U>* ImageProcessingUtils::extractSortedObjectPoints(
    UINT16* objectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, UINT32 objectId)
{
    std::vector<D2D_POINT_2U>* points = new std::vector<D2D_POINT_2U>();
    int currentIndex = 0;

    for (UINT32 y = 0; y < objectMapHeight; ++y)
    {
        for (UINT32 x = 0; x < objectMapWidth; ++x)
        {
            if (objectMap[currentIndex] == objectId)
            {
                D2D_POINT_2U newPoint;
                newPoint.x = x;
                newPoint.y = y;
                points->push_back(newPoint);
            }

            currentIndex++;
        }
    }

    sortPointVector(*points);

    return points;
}


//------------------------------------------------------------------
// createConvexHull
//
// Returns a list of points on the convex hull in counter-clockwise
// order. Note: The last point in the returned list is the same as
// the first one.
//------------------------------------------------------------------
ConvexHull* ImageProcessingUtils::createConvexHull(std::vector<D2D_POINT_2U>& points, bool alreadySorted)
{
    const int n = points.size();
    int k = 0;
    ConvexHull* convexHull = new std::vector<D2D_POINT_2U>(2 * n);

    if (!alreadySorted)
    {
        sortPointVector(points);
    }

    // Build lower hull
    for (int i = 0; i < n; ++i)
    {
        while (k >= 2 && cross(convexHull->at(k - 2), convexHull->at(k - 1), points.at(i)) <= 0)
        {
            k--;
        }

        convexHull->at(k).x = points.at(i).x;
        convexHull->at(k).y = points.at(i).y;
        k++;
    }

    // Build upper hull
    for (int i = n - 2, t = k + 1; i >= 0; i--)
    {
        while (k >= t && cross(convexHull->at(k - 2), convexHull->at(k - 1), points.at(i)) <= 0)
        {
            k--;
        }

        convexHull->at(k).x = points.at(i).x;
        convexHull->at(k).y = points.at(i).y;
        k++;
    }

    convexHull->resize(k);
    return convexHull;
}


//------------------------------------------------------------------
// cross
// 
// 2D cross product of OA and OB vectors, i.e. z-component of their
// 3D cross product. Returns a positive value, if OAB makes a
// counter-clockwise turn, negative for clockwise turn, and zero if
// the points are collinear.
//------------------------------------------------------------------
inline double ImageProcessingUtils::cross(const D2D_POINT_2U& O, const D2D_POINT_2U& A, const D2D_POINT_2U& B)
{
    const int crossProduct =
        ((int)(A.x) - (int)(O.x)) * ((int)(B.y) - (int)(O.y))
        - ((int)(A.y) - (int)(O.y)) * ((int)(B.x) - (int)(O.x));
    return crossProduct;
}



//------------------------------------------------------------------
// sortPointVector
//
// Sorts points lexicographically.
//------------------------------------------------------------------
void ImageProcessingUtils::sortPointVector(std::vector<D2D_POINT_2U>& pointVector)
{
    const int pointCount = pointVector.size();

    if (pointCount > 1)
    {
        std::sort(pointVector.begin(), pointVector.end(),
            [](D2D_POINT_2U const& a, D2D_POINT_2U const& b)
                {
                    if (a.x < b.x || (a.x == b.x && a.y < b.y))
                    {
                        return true;
                    }

                    return false;
                });
    }
}


//------------------------------------------------------------------
// objectIdOfPixelAbove
//
// Checks if pixels exist above and returns the first object ID
// found. If no pixels are found, will return 0.
//------------------------------------------------------------------
inline UINT16 ImageProcessingUtils::objectIdOfPixelAbove(
    BYTE* binaryImage, UINT16* objectMap,
    const UINT32& currentIndex, const UINT32& imageWidth,
    bool fromLeftToRight)
{
    const int cornerCase = (currentIndex % imageWidth == 0)
        ? -1 // This pixel is the first one on this row
        : ((currentIndex + 1) % imageWidth == 0)
        ? 1 // This pixel is the last one on this row
        : 0;

    const int pixelAboveIndex = currentIndex - imageWidth;
    const int order = fromLeftToRight ? -1 : 1;
    UINT16 bridgedObjectIndex = 0;

    if (pixelAboveIndex + order >= 0
        && cornerCase != order
        && binaryImage[pixelAboveIndex + order] == SelectedPixelValue) // Top-left, if order == -1
    {
        bridgedObjectIndex = objectMap[pixelAboveIndex + order];
    }
    else if (pixelAboveIndex >= 0 && binaryImage[pixelAboveIndex] == SelectedPixelValue) // Top
    {
        bridgedObjectIndex = objectMap[pixelAboveIndex];
    }
    else if (pixelAboveIndex + order >= 0
             && cornerCase != order
             && binaryImage[pixelAboveIndex + order] == SelectedPixelValue) // Top-right, if order == +1
    {
        bridgedObjectIndex = objectMap[pixelAboveIndex + order];
    }

    return bridgedObjectIndex;
}


//------------------------------------------------------------------
// calculateLineEquation
//
// Returns false, if no equations could be calculated (when delta X
// is zero).
//------------------------------------------------------------------
bool ImageProcessingUtils::calculateLineEquation(const D2D_POINT_2U& point1, const D2D_POINT_2U& point2, double* a, int* b)
{
    int deltaX = (int)(point2.x) - (int)(point1.x);

    if (deltaX != 0)
    {
        *a = (double)((int)(point2.y) - (int)(point1.y)) / (double)deltaX;
		*b = (int)round((int)(point1.y) - *a * point1.x);
        return true;
    }

    return false;
}


//------------------------------------------------------------------
// lineBoundingBox
//
//------------------------------------------------------------------
D2D_RECT_U ImageProcessingUtils::lineBoundingBox(
	const D2D_POINT_2U& point1, const D2D_POINT_2U& point2, bool leftAndRightHaveToHaveEvenValues)
{
	D2D_RECT_U rect;
	rect.left = point1.x < point2.x ? point1.x : point2.x;
	rect.top = point1.y < point2.y ? point1.y : point2.y;
	rect.right = point1.x < point2.x ? point2.x : point1.x;
	rect.bottom = point1.y < point2.y ? point2.y : point1.y;

	if (leftAndRightHaveToHaveEvenValues)
	{
		if (rect.left % 2 != 0)
		{
			rect.left -= 1;
		}
		
		if (rect.right % 2 != 0)
		{
			rect.right -= 1;
		}
	}

	return rect;
}


//------------------------------------------------------------------
// visualizeObjectNV12
//
//------------------------------------------------------------------
void ImageProcessingUtils::visualizeObjectNV12(
	BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight, UINT16* objectMap, const UINT16& objectId)
{
	UINT32 currentIndex = 0;

	UINT32 yIndex1 = 0;
	UINT32 yIndex2 = 0;
	UINT32 yIndexForUVPlane = 0;
	const UINT32 uvPlaneStart = imageWidth * imageHeight - 1;

	BYTE yy = 0xe0;
	BYTE u = 0x00;
	BYTE v = 0x00;

	for (UINT32 y = 0; y < imageHeight; ++y)
	{
		for (UINT32 x = 0; x + 1 < imageWidth; x += 2)
		{
			if (objectMap[currentIndex] == objectId || objectMap[currentIndex + 1] == objectId)
			{
				yIndex1 = y * imageWidth;
				yIndex2 = (y + 1) * imageWidth;
				yIndexForUVPlane = uvPlaneStart + y / 2 * imageWidth;

				image[yIndex1 + x] = yy;
				image[yIndex1 + x + 1] = yy;
				image[yIndex2 + x] = yy;
				image[yIndex2 + x + 1] = yy;
				image[yIndexForUVPlane + x] = u;
				image[yIndexForUVPlane + x + 1] = v;
			}

			currentIndex += 2;
		}
	}
}


//------------------------------------------------------------------
// idExists
//
//------------------------------------------------------------------
inline bool ImageProcessingUtils::idExists(const std::vector<UINT16>& ids, const UINT16& id)
{
    const int size = ids.size();

    for (int i = 0; i < size; ++i)
    {
        if (ids.at(i) == id)
        {
            return true;
        }
    }

    return false;
}
