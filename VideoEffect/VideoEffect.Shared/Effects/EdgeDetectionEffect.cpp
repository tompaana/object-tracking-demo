#include "pch.h"

#include "EdgeDetectionEffect.h"
#include "ImageProcessing\ImageProcessingCommon.h"
#include "Settings.h"


EdgeDetectionEffect::EdgeDetectionEffect(GUID videoFormatSubtype)
    : AbstractEffect(videoFormatSubtype)
{
}


void EdgeDetectionEffect::apply(
    const D2D_RECT_U& targetRect,
    _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
    _In_ LONG destinationStride,
    _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
    _In_ LONG sourceStride,
    _In_ DWORD widthInPixels,
    _In_ DWORD heightInPixels)
{
    if (m_videoFormatSubtype == MFVideoFormat_YUY2)
    {
        applyYUY2((BYTE)m_settings->m_threshold, true, targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
    }
    else if (m_videoFormatSubtype == MFVideoFormat_NV12)
    {
        applyNV12((BYTE)m_settings->m_threshold, true, targetRect, destination, destinationStride, source, sourceStride, widthInPixels, heightInPixels);
    }
    else
    {
        throw "Video format not supported";
    }
}


inline BYTE EdgeDetectionEffect::calculateGradient(
    const BYTE& y0, const BYTE& u0, const BYTE& v0,
    const BYTE& y1, const BYTE& u1, const BYTE& v1)
{
    return abs(y0 - y1) + abs(u0 - u1) + abs(v0 - v1);
}



//-------------------------------------------------------------------
// gradientTransformImageYUY2
//
//
//-------------------------------------------------------------------
void EdgeDetectionEffect::applyYUY2(
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
    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);

    // Lines above the destination rectangle.
    for (; y < rcDest.top; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels * 2);
        pSrc += lSrcStride;
        pDest += lDestStride;
    }

    WORD *pSrc_Pixel = NULL;
    WORD *pDest_Pixel = NULL;
    BYTE y0 = 0x0;
    BYTE y1 = 0x0;
    BYTE y2 = 0x0;
    BYTE y3 = 0x0;
    BYTE u0 = 0x0;
    BYTE v0 = 0x0;
    BYTE u1 = 0x0;
    BYTE v1 = 0x0;
    //BYTE ySum = 0x0;
    BYTE newY0 = 0x0;
    BYTE newY1 = 0x0;

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
                y0 = pSrc_Pixel[x] & 0x00FF;
                u0 = pSrc_Pixel[x] >> 8;
                y1 = pSrc_Pixel[x + 1] & 0x00FF;
                v0 = pSrc_Pixel[x + 1] >> 8;

                //ySum = y0 + y1;

                if (x + 2 < dwWidthInPixels)
                {
                    // There is at least one pixel on the right
                    y2 = pSrc_Pixel[x + 2] & 0x00ff;
                    y3 = pSrc_Pixel[x + 3] & 0x00ff;
                    u1 = pSrc_Pixel[x + 2] >> 8;
                    v1 = pSrc_Pixel[x + 3] >> 8;

                    newY0 = calculateGradient(y0 + y1, u0, v0, y2 + y3, u1, v1);
                }
                else
                {
                    newY0 = 0x0;
                }

                if (y < yEnd - 1)
                {
                    // There is at least one line of pixels below
                    y2 = pSrc_Pixel[x + dwWidthInPixels] & 0x00ff;
                    y3 = pSrc_Pixel[x + 1 + dwWidthInPixels] & 0x00ff;
                    u1 = pSrc_Pixel[x + dwWidthInPixels] >> 8;
                    v1 = pSrc_Pixel[x + 1 + dwWidthInPixels] >> 8;

                    newY1 = calculateGradient(y0 + y1, u0, v0, y2 + y3, u1, v1);

                    if ((float)newY0 + (float)newY1 >= SelectedPixelValue)
                    {
                        newY0 = SelectedPixelValue;
                    }
                    else
                    {
                        newY0 += newY1;
                    }
                }

                if (newY0 > threshold * 2)
                {
                    newY0 = SelectedPixelValue;
                }
                else
                {
                    newY0 = 0x0;
                }

                if (newY0 == SelectedPixelValue)
                {
                    //u0 = 0x0;
                    //v0 = 0x0;
                    u0 = 0x80;
                    v0 = 0x80;
                }
                else
                {
                    u0 = 0x80;
                    v0 = 0x80;
                }

                pDest_Pixel[x] = newY0 | (u0 << 8);
                pDest_Pixel[x + 1] = newY0 | (v0 << 8);
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


//-------------------------------------------------------------------
// gradientTransformImageNV12
//
//
//-------------------------------------------------------------------
void EdgeDetectionEffect::applyNV12(
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

    // Lines within the destination rectangle.
    const DWORD yEnd = min(rcDest.bottom, dwHeightInPixels);
    const DWORD endOfDestinationRectangleY = yEnd / 2;

    const DWORD rectWidth = (rcDest.right > dwWidthInPixels) ? 0 : rcDest.right - rcDest.left;
    const DWORD endOfDestinationRectangleX = (rectWidth == 0) ? dwWidthInPixels : rectWidth;

    BYTE y0 = 0x0;
    BYTE y1 = 0x0;
    BYTE y2 = 0x0;
    BYTE y3 = 0x0;
    BYTE u0 = 0x0;
    BYTE v0 = 0x0;
    BYTE u1 = 0x0;
    BYTE v1 = 0x0;
    BYTE newY0 = 0x0;
    BYTE newY1 = 0x0;

    for (; y < endOfDestinationRectangleY; ++y)
    {
        if (rectWidth > 0 && rcDest.left > 0)
        {
            memcpy(pDest, pSrc, rcDest.left);
            pDest += rcDest.left;
            pSrc += rcDest.left;
            yPlaneDest += rcDest.left;
        }

        for (DWORD x = 0; (x + 1) < endOfDestinationRectangleX; x += 2)
        {
            y0 = yPlaneDest[x] + yPlaneDest[x + 1] + yPlaneDest[x + dwWidthInPixels] + yPlaneDest[x + 1 + dwWidthInPixels];
            u0 = pSrc[x];
            v0 = pSrc[x + 1];

            if (x + 2 < endOfDestinationRectangleX)
            {
                // There is at least one pixel on the right
                y1 = yPlaneDest[x + 2] + yPlaneDest[x + 3]
                    + yPlaneDest[x + 2 + dwWidthInPixels] + yPlaneDest[x + 3 + dwWidthInPixels];
                u1 = pSrc[x + 2];
                v1 = pSrc[x + 3];

                newY0 = calculateGradient(y0, u0, v0, y1, u1, v1);
            }
            else
            {
                newY0 = 0x0;
            }

            if (y < endOfDestinationRectangleY - 1)
            {
                // There is at least one line of pixels below
                y1 = yPlaneDest[x + dwWidthInPixels] + yPlaneDest[x + 1 + dwWidthInPixels]
                    + yPlaneDest[x + dwWidthInPixels * 2] + yPlaneDest[x + 1 + dwWidthInPixels * 2];
                u1 = pSrc[x + dwWidthInPixels];
                v1 = pSrc[x + 1 + dwWidthInPixels];

                newY1 = calculateGradient(y0, u0, v0, y1, u1, v1);

                if ((float)newY0 + (float)newY1 >= SelectedPixelValue)
                {
                    newY0 = SelectedPixelValue;
                }
                else
                {
                    newY0 += newY1;
                }
            }

            if (newY0 > threshold * 2)
            {
                newY0 = SelectedPixelValue;
            }
            else
            {
                newY0 = 0x0;
            }

            if (newY0 == SelectedPixelValue)
            {
                pDest[x] = 0x80;
                pDest[x + 1] = 0x80;
                //pDest[x] = 0x0;
                //pDest[x + 1] = 0x0;
            }
            else
            {
                pDest[x] = 0x80;
                pDest[x + 1] = 0x80;
            }

            yPlaneDest[x] = newY0;
            yPlaneDest[x + 1] = newY0;
            yPlaneDest[x + dwWidthInPixels] = newY0;
            yPlaneDest[x + 1 + dwWidthInPixels] = newY0;
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
    }

    // Lines below the destination rectangle.
    for (; y < dwHeightInPixels / 2; y++)
    {
        memcpy(pDest, pSrc, dwWidthInPixels);
        pSrc += lSrcStride;
        pDest += lDestStride;
    }
}

