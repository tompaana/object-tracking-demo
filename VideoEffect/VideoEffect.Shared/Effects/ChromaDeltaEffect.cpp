#include "pch.h"

#include "ChromaDeltaEffect.h"

#include "ImageProcessing\ImageProcessingCommon.h"
#include "ImageProcessing\ImageProcessingUtils.h"
#include "Settings.h"


ChromaDeltaEffect::ChromaDeltaEffect(GUID videoFormatSubtype)
    : AbstractEffect(videoFormatSubtype),
    m_previousFrame(NULL)
{

}


ChromaDeltaEffect::~ChromaDeltaEffect()
{
    free(m_previousFrame);
}



void ChromaDeltaEffect::apply(
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

    if (!m_previousFrame)
    {
        m_previousFrame = ImageProcessingUtils::newEmptyFrame(widthInPixels, heightInPixels, m_videoFormatSubtype);
        memcpy(destination, m_previousFrame, ImageProcessingUtils::frameSize(widthInPixels, heightInPixels, m_videoFormatSubtype));
    }
    else
    {
        BYTE* sourceBegin = const_cast<BYTE*>(source);

        if (m_videoFormatSubtype == MFVideoFormat_NV12)
        {
            applyChromaDeltaNV12((BYTE)m_settings->m_threshold, true, targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
        }
        else if (m_videoFormatSubtype == MFVideoFormat_YUY2)
        {
            applyChromaDeltaYUY2((BYTE)m_settings->m_threshold, true, targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
        }

        ImageProcessingUtils::copyFrame(sourceBegin, m_previousFrame,
            ImageProcessingUtils::frameSize(widthInPixels, heightInPixels, m_videoFormatSubtype));
    }
}


float ChromaDeltaEffect::calculateDelta(
    const BYTE& y0, const BYTE& u0, const BYTE& v0,
    const BYTE& y1, const BYTE& u1, const BYTE& v1,
    BYTE& dy, BYTE& du, BYTE& dv)
{
    //dy = y1 - y0;
    du = u1 - u0;
    dv = v1 - v0;
#pragma warning(push)
#pragma warning(disable: 4244) 
    return /*abs(y0 - y1) +*/ abs(u0 - u1) + abs(v0 - v1);
#pragma warning(pop)
}


void ChromaDeltaEffect::applyChromaDeltaYUY2(
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
    const BYTE* pPreviousFrame = m_previousFrame;

    // Lines above the destination rectangle.
    for (; y < rcDest.top; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels * 2);
        pSrc += lSrcStride;
        pDest += lDestStride;
        pPreviousFrame += lSrcStride;
    }

    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);
    WORD* pSrc_Pixel = NULL;
    WORD* pDest_Pixel = NULL;
    WORD* pPreviousFramePixel = NULL;
    BYTE y0 = 0x0;
    BYTE y1 = 0x0;
    BYTE y2 = 0x0;
    BYTE y3 = 0x0;
    BYTE u0 = 0x0;
    BYTE v0 = 0x0;
    BYTE u1 = 0x0;
    BYTE v1 = 0x0;
    BYTE dy = 0x0;
    BYTE du = 0x0;
    BYTE dv = 0x0;
    float dTotal = 0;
    BYTE newY = 0x0;

    // Lines within the destination rectangle.
    for (; y < yEnd; y++)
    {
        pSrc_Pixel = (WORD*)pSrc;
        pDest_Pixel = (WORD*)pDest;
        pPreviousFramePixel = (WORD*)pPreviousFrame;

        for (UINT32 x = 0; (x + 1) < dwWidthInPixels; x += 2)
        {
            // Byte order is Y0 U0 Y1 V0
            // Each WORD is a byte pair (Y, U/V)
            // Windows is little-endian so the order appears reversed.

            if (x >= rcDest.left && x < rcDest.right)
            {
                y0 = pSrc_Pixel[x] & 0x00FF;
                u0 = pSrc_Pixel[x] >> 8;
                y1 = pSrc_Pixel[x + 1] & 0x00FF;
                v0 = pSrc_Pixel[x + 1] >> 8;

                y2 = pPreviousFramePixel[x] & 0x00FF;
                u1 = pPreviousFramePixel[x] >> 8;
                y3 = pPreviousFramePixel[x + 1] & 0x00FF;
                v1 = pPreviousFramePixel[x + 1] >> 8;

                dTotal = calculateDelta(y0 + y1, u0, v0, y2 + y3, u1, v1, dy, du, dv);

                if (dTotal > threshold)
                {
                    newY = SelectedPixelValue;
                }
                else
                {
                    newY = 0x0;
                }

                pDest_Pixel[x] = newY | (u1 << 8);
                pDest_Pixel[x + 1] = newY | (v1 << 8);
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
        pPreviousFrame += lSrcStride;
    }

    // Lines below the destination rectangle.
    for (; y < dwHeightInPixels; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels * 2);
        pSrc += lSrcStride;
        pDest += lDestStride;
        pPreviousFrame += lSrcStride;
    }
}


void ChromaDeltaEffect::applyChromaDeltaNV12(
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
    BYTE* pPreviousFrame = m_previousFrame;
    BYTE* yPlanePreviousFrame = m_previousFrame;
    BYTE* yPlaneDest = pDest;

    CopyMemory(pDest, pSrc, dwWidthInPixels * dwHeightInPixels);

    pDest += lDestStride * dwHeightInPixels;
    pSrc += lSrcStride * dwHeightInPixels;
    pPreviousFrame += lSrcStride * dwHeightInPixels;

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
        pPreviousFrame += temp;
        yPlaneDest += temp * 2;
        yPlanePreviousFrame += temp * 2;
        y += numberOfLinesAboveDestRect - 1;
    }

    // Lines within the destination rectangle.
    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);
    const DWORD endOfDestinationRectangleY = yEnd / 2;

    const DWORD rectWidth = (rcDest.right > dwWidthInPixels) ? 0 : rcDest.right - rcDest.left;
    const DWORD endOfDestinationRectangleX = (rectWidth == 0) ? dwWidthInPixels : rectWidth;

    BYTE y0 = 0x0;
    BYTE y1 = 0x0;
    BYTE u0 = 0x0;
    BYTE v0 = 0x0;
    BYTE u1 = 0x0;
    BYTE v1 = 0x0;
    BYTE dy = 0x0;
    BYTE du = 0x0;
    BYTE dv = 0x0;
    BYTE dTotal = 0x0;

    for (; y < endOfDestinationRectangleY; ++y)
    {
        if (rectWidth > 0 && rcDest.left > 0)
        {
            memcpy(pDest, pSrc, rcDest.left);
            pDest += rcDest.left;
            pSrc += rcDest.left;
            pPreviousFrame += rcDest.left;
            yPlaneDest += rcDest.left;
            yPlanePreviousFrame += rcDest.left;
        }

        for (DWORD x = 0; (x + 1) < endOfDestinationRectangleX; x += 2)
        {
            y0 = yPlaneDest[x] + yPlaneDest[x + 1]
                + yPlaneDest[x + dwWidthInPixels] + yPlaneDest[x + 1 + dwWidthInPixels];
            u0 = pSrc[x];
            v0 = pSrc[x + 1];

            y1 = yPlanePreviousFrame[x] + yPlanePreviousFrame[x + 1]
                + yPlanePreviousFrame[x + dwWidthInPixels] + yPlanePreviousFrame[x + 1 + dwWidthInPixels];
            u1 = pPreviousFrame[x];
            v1 = pPreviousFrame[x + 1];

#pragma warning(push)
#pragma warning(disable: 4244)
            dTotal = calculateDelta(y0, u0, v0, y1, u1, v1, dy, dy, dv);
#pragma warning(pop)

            if (dTotal > threshold * 2)
            {
                dTotal = SelectedPixelValue;
            }
            else
            {
                dTotal = 0x0;
            }

            if (dTotal == SelectedPixelValue)
            {
                pDest[x] = 0x0;
                pDest[x + 1] = 0x0;
            }
            else
            {
                pDest[x] = 0x80;
                pDest[x + 1] = 0x80;
            }

            yPlaneDest[x] = dTotal;
            yPlaneDest[x + 1] = dTotal;
            yPlaneDest[x + dwWidthInPixels] = dTotal;
            yPlaneDest[x + 1 + dwWidthInPixels] = dTotal;
        }

        if (rectWidth > 0)
        {
            pDest += rectWidth;
            pSrc += rectWidth;
            pPreviousFrame += rectWidth;
            yPlaneDest += rectWidth;
            yPlanePreviousFrame += rectWidth;
            memcpy(pDest, pSrc, dwWidthInPixels - rcDest.right);
            pDest += dwWidthInPixels - rcDest.right;
            pSrc += dwWidthInPixels - rcDest.right;
            pPreviousFrame += dwWidthInPixels - rcDest.right;
            yPlaneDest += dwWidthInPixels - rcDest.right + lDestStride;
            yPlanePreviousFrame += dwWidthInPixels - rcDest.right + lSrcStride;
        }
        else
        {
            pDest += lDestStride;
            pSrc += lSrcStride;
            pPreviousFrame += lSrcStride;
            yPlaneDest += lDestStride * 2;
            yPlanePreviousFrame += lSrcStride * 2;
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
