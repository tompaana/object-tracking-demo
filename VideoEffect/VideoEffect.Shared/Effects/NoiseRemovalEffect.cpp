#include "pch.h"

#include "NoiseRemovalEffect.h"

#include "ImageProcessing\ImageProcessingCommon.h"
#include "ImageProcessing\ImageProcessingUtils.h"
#include "Settings.h"

#define TWO_PIXELS_AT_A_TIME 1


NoiseRemovalEffect::NoiseRemovalEffect(GUID videoFormatSubtype)
    : AbstractEffect(videoFormatSubtype)
{

}


NoiseRemovalEffect::~NoiseRemovalEffect()
{
}


void NoiseRemovalEffect::apply(
    const D2D_RECT_U& targetRect,
    _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
    _In_ LONG destinationStride,
    _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
    _In_ LONG sourceStride,
    _In_ DWORD widthInPixels,
    _In_ DWORD heightInPixels)
{
    if (m_videoFormatSubtype != MFVideoFormat_NV12 && m_videoFormatSubtype != MFVideoFormat_YUY2)
    {
        throw "Video format not supported";
    }

    if (m_videoFormatSubtype == MFVideoFormat_NV12)
    {
        applyNearestNeighborSmootherNv12(targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
        //destination = ImageProcessingUtils::copyFrame(const_cast<BYTE*>(source), ImageProcessingUtils::frameSize(widthInPixels, heightInPixels, m_videoFormatSubtype));
    }
    else if (m_videoFormatSubtype == MFVideoFormat_YUY2)
    {
        applyNearestNeighborSmootherYuy2(targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
    }
}


inline void NoiseRemovalEffect::getYuy2Pixel(const WORD* frame, Yuy2Pixel& pixel, const UINT32& index) const
{
    pixel._y0 = frame[index] & 0x00FF;
    pixel._u = frame[index] >> 8;
    pixel._y1 = frame[index + 1] & 0x00FF;
    pixel._v = frame[index + 1] >> 8;
}


inline void NoiseRemovalEffect::getYuy2Pixel(
    const BYTE* frame, Yuy2Pixel& pixel, const UINT32& x, const UINT32& y, const DWORD& frameWidthInPixels) const
{
    getYuy2Pixel((WORD*)frame, pixel, (y * frameWidthInPixels + x));
}


inline const UINT8 NoiseRemovalEffect::getYuy2NeighborPixels(
    const BYTE* frame, const UINT32& x, const UINT32& y, const DWORD& frameWidthInPixels, const DWORD& frameHeightInPixels)
{
    UINT8 foundNeighborPixelCount = 0;
    int currentX = 0;
    int currentY = 0;
    bool currentPixelExists = true;

    for (UINT8 i = 0; i < 8; ++i)
    {
        currentX =
            (i == 0 || i == 3 || i == 5) ? x - 2
            : (i == 1 || i == 6) ? x
            : x + 2;

#pragma warning(push)
#pragma warning(disable: 4018) 
        if (currentX < 0 || currentX >= frameWidthInPixels)
        {
            currentPixelExists = false;
        }

        if (currentPixelExists)
        {
            currentY =
                (i < 3) ? y - 1
                : (i > 4) ? y + 1
                : y;

            if (currentY < 0 || currentY >= frameHeightInPixels)
            {
                currentPixelExists = false;
            }
        }
#pragma warning(pop)

        if (currentPixelExists)
        {
            getYuy2Pixel(frame, _neighborYuy2Pixels[foundNeighborPixelCount], (UINT32)currentX, (UINT32)currentY, frameWidthInPixels);
            foundNeighborPixelCount++;
        }
        else
        {
            currentPixelExists = true;
            continue;
        }
    }

    return foundNeighborPixelCount;
}


//-------------------------------------------------------------------
// applyNearestNeighborSmootherYuy2
//
//
//-------------------------------------------------------------------
void NoiseRemovalEffect::applyNearestNeighborSmootherYuy2(
    const D2D_RECT_U& rcDest,
    _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
    _In_ LONG lDestStride,
    _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
    _In_ LONG lSrcStride,
    _In_ DWORD dwWidthInPixels,
    _In_ DWORD dwHeightInPixels)
{
    BYTE* sourceBeginning = const_cast<BYTE*>(pSrc);
    DWORD y = 0;

    // Lines above the destination rectangle.
    for (; y < rcDest.top; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels * 2);
        pSrc += lSrcStride;
        pDest += lDestStride;
    }

    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);
    WORD* pSrc_Pixel = NULL;
    WORD* pDest_Pixel = NULL;

    UINT8 neighborPixelCount = 0;
    Yuy2Pixel averagePixel;

    // Lines within the destination rectangle.
    for (; y < yEnd; y++)
    {
        pSrc_Pixel = (WORD*)pSrc;
        pDest_Pixel = (WORD*)pDest;

        for (UINT32 x = 0; (x + 1) < dwWidthInPixels; x += 2)
        {
            // Byte order is Y0 U0 Y1 V0
            // Each WORD is a byte pair (Y, U/V)
            // Windows is little-endian so the order appears reversed.

            if (x >= rcDest.left && x < rcDest.right)
            {
                neighborPixelCount = getYuy2NeighborPixels(sourceBeginning, x, y, dwWidthInPixels, dwHeightInPixels);
                averagePixel.calculateAverage(_neighborYuy2Pixels, neighborPixelCount);

                pDest_Pixel[x] = averagePixel._y0 | (averagePixel._u << 8);
                pDest_Pixel[x + 1] = averagePixel._y1 | (averagePixel._v << 8);
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
}



inline void NoiseRemovalEffect::getNv12Pixel(
    const BYTE* frame, SimpleYuvPixel& pixel, const DWORD& x, const DWORD& y,
    const DWORD& yPlaneLength, const DWORD& frameWidthInPixels) const
{
    bool uComesFirst = (x % 2 == 0);
    const DWORD yIndex = y * frameWidthInPixels + x;
    const DWORD uvLineIndex = frameWidthInPixels * (y / 2);

    pixel._y = frame[yIndex];

    if (uComesFirst)
    {
        pixel._u = frame[yPlaneLength + uvLineIndex + x];
        pixel._v = frame[yPlaneLength + uvLineIndex + x + 1];
    }
    else
    {
        pixel._v = frame[yPlaneLength + uvLineIndex + x];
        pixel._u = frame[yPlaneLength + uvLineIndex + x + 1];
    }
}


inline const UINT8 NoiseRemovalEffect::getNv12NeighborPixels(
    const BYTE* frame, const DWORD& x, const DWORD& y, const DWORD& frameWidthInPixels, const DWORD& frameHeightInPixels)
{
    UINT8 foundNeighborPixelCount = 0;
    int currentX = 0;
    int currentY = 0;
    bool currentPixelExists = true;

    for (DWORD i = 0; i < 8; ++i)
    {
        currentX =
            (i == 0 || i == 3 || i == 5) ? x - 1
            : (i == 1 || i == 6) ? x
            : x + 1;

#pragma warning(push)
#pragma warning(disable: 4018) 
        if (currentX < 0 || currentX >= frameWidthInPixels)
        {
            currentPixelExists = false;
        }

        if (currentPixelExists)
        {
            currentY =
                (i < 3) ? y - 1
                : (i > 4) ? y + 1
                : y;

            if (currentY < 0 || currentY >= frameHeightInPixels)
            {
                currentPixelExists = false;
            }
#pragma warning(pop)
        }

        if (currentPixelExists)
        {
            const DWORD yPlaneLength = frameWidthInPixels * frameHeightInPixels;
            getNv12Pixel(frame, _neighborNv12Pixels[foundNeighborPixelCount], (DWORD)currentX, (DWORD)currentY, yPlaneLength, frameWidthInPixels);
            foundNeighborPixelCount++;
        }
        else
        {
            currentPixelExists = true;
            continue;
        }
    }

    return foundNeighborPixelCount;
}


//-------------------------------------------------------------------
// applyNearestNeighborSmootherNv12
//
//
//-------------------------------------------------------------------
void NoiseRemovalEffect::applyNearestNeighborSmootherNv12(
    const D2D_RECT_U& rcDest,
    _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
    _In_ LONG lDestStride,
    _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
    _In_ LONG lSrcStride,
    _In_ DWORD dwWidthInPixels,
    _In_ DWORD dwHeightInPixels)
{
    BYTE* sourceBeginning = const_cast<BYTE*>(pSrc);
    BYTE* yPlaneDest = pDest;

    CopyMemory(pDest, pSrc, dwWidthInPixels * dwHeightInPixels);

    pDest += lDestStride * dwHeightInPixels;
    pSrc += lSrcStride * dwHeightInPixels;

    // U-V plane

    // NOTE: The U-V plane has 1/2 the number of lines as the Y plane.

    // Lines above the destination rectangle.
    const UINT32 numberOfLinesAboveDestRect = rcDest.top / 2;
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

    // Lines within the destination rectangle.
    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);
    const DWORD endOfDestinationRectangleY = yEnd / 2;

    const DWORD rectWidth = (rcDest.right > dwWidthInPixels) ? 0 : rcDest.right - rcDest.left;
    const DWORD endOfDestinationRectangleX = (rectWidth == 0) ? dwWidthInPixels : rectWidth;

    UINT8 neighborPixelCount = 0;
    SimpleYuvPixel averagePixel;

    for (; y < endOfDestinationRectangleY; ++y)
    {
        if (rectWidth > 0 && rcDest.left > 0)
        {
            memcpy(pDest, pSrc, rcDest.left);
            pDest += rcDest.left;
            pSrc += rcDest.left;
            yPlaneDest += rcDest.left;
        }

#ifdef TWO_PIXELS_AT_A_TIME
        for (DWORD x = 0; (x + 1) < endOfDestinationRectangleX; x += 2)
#else
        for (DWORD x = 0; x < endOfDestinationRectangleX; ++x)
#endif
        {
            neighborPixelCount = getNv12NeighborPixels(sourceBeginning, x, y, dwWidthInPixels, dwHeightInPixels);
            averagePixel.calculateAverage(_neighborNv12Pixels, neighborPixelCount);

#ifdef TWO_PIXELS_AT_A_TIME
            yPlaneDest[x] = averagePixel._y;
            yPlaneDest[x + 1] = averagePixel._y;

            if ((x & 2) == 0)
            {
                pDest[x] = averagePixel._u;
                pDest[x + 1] = averagePixel._v;
            }
#else
            yPlaneDest[x] = averagePixel._y;

            if (x & 2 == 0)
            {
                pDest[x] = averagePixel._u;
            }
            else
            {
                pDest[x] = averagePixel._v;
            }
#endif
        }

        if (rectWidth > 0)
        {
            /*pDest += rectWidth;
            pSrc += rectWidth;
            yPlaneDest += rectWidth;
            memcpy(pDest, pSrc, dwWidthInPixels - rcDest.right);*/
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
    }

    // Lines below the destination rectangle.
    for (; y < dwHeightInPixels / 2; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels);
        pSrc += lSrcStride;
        pDest += lDestStride;
    }
}
