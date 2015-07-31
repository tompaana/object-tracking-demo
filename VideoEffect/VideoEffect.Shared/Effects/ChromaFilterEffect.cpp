#include "pch.h"

#include "ChromaFilterEffect.h"
#include "ImageProcessing\ImageProcessingCommon.h"
#include "Settings.h"


ChromaFilterEffect::ChromaFilterEffect(GUID videoFormatSubtype)
    : AbstractEffect(videoFormatSubtype)
{
}


void ChromaFilterEffect::apply(
    const D2D_RECT_U& targetRect,
    _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
    _In_ LONG destinationStride,
    _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
    _In_ LONG sourceStride,
    _In_ DWORD widthInPixels,
    _In_ DWORD heightInPixels)
{
    m_objectDetails.reset();

    if (m_videoFormatSubtype == MFVideoFormat_YUY2)
    {
        applyYUY2(m_objectDetails,
            m_settings->m_targetYuv, (BYTE)m_settings->m_threshold,
            m_dimmUnselectedPixels, targetRect, destination, destinationStride, source, sourceStride,
            widthInPixels, heightInPixels);
    }
    else if (m_videoFormatSubtype == MFVideoFormat_NV12)
    {
        applyNV12(m_objectDetails,
            m_settings->m_targetYuv, (BYTE)m_settings->m_threshold,
            m_dimmUnselectedPixels, targetRect, destination, destinationStride, source, sourceStride,
            widthInPixels, heightInPixels);
    }
    else
    {
        throw "Video format not supported";
    }
}


ObjectDetails ChromaFilterEffect::currentObject() const
{
    return m_objectDetails;
}


void ChromaFilterEffect::setDimmUnselectedPixels(const bool& dimmUnselecctedPixels)
{
    m_dimmUnselectedPixels = dimmUnselecctedPixels;
}


inline void ChromaFilterEffect::getColorFilteredValues(
    BYTE* py, BYTE* pu, BYTE* pv,
    const BYTE& targetY, const BYTE& targetU, const BYTE& targetV,
    const BYTE& threshold, const bool& dimmUnselected)
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


//-------------------------------------------------------------------
// applyYUY2
//
// Converts YUY2 image. This mostly used by tablets and desktop 
// webcams.
//-------------------------------------------------------------------
inline void ChromaFilterEffect::applyYUY2(
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
                BYTE y0 = pSrc_Pixel[x] & 0x00FF;
                BYTE u = pSrc_Pixel[x] >> 8;
                BYTE y1 = pSrc_Pixel[x + 1] & 0x00FF;
                BYTE v = pSrc_Pixel[x + 1] >> 8;

                getColorFilteredValues(&y0, &u, &v, targetY, targetU, targetV, threshold, dimmFilteredPixels);
                getColorFilteredValues(&y1, &u, &v, targetY, targetU, targetV, threshold, dimmFilteredPixels);

                pDest_Pixel[x] = y0 | (u << 8);
                pDest_Pixel[x + 1] = y1 | (v << 8);

                if (y0 == SelectedPixelValue)
                {
                    numberOfSelectedPixelsY++;
                    numberOfSelectedPixelsX[x]++;
                }

                if (y1 == SelectedPixelValue)
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
// applyNV12
//
// Converts NV12 image. This mostly used by phones.
//-------------------------------------------------------------------
inline void ChromaFilterEffect::applyNV12(
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

            getColorFilteredValues(&yy, &u, &v, targetY, targetU, targetV, threshold, dimmFilteredPixels);

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

