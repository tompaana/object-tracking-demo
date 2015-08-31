#ifndef NOISEREMOVALEFFECT_H
#define NOISEREMOVALEFFECT_H

#include "AbstractEffect.h"
#include "ImageProcessing\Yuy2Pixel.h"
#include "ImageProcessing\SimpleYuvPixel.h"

#define NEIGHBOR_PIXEL_COUNT 8


class NoiseRemovalEffect : public AbstractEffect
{
public:
    NoiseRemovalEffect(GUID videoFormatSubtype);
    ~NoiseRemovalEffect();

public: // From AbstractFilter
    virtual void apply(
        const D2D_RECT_U& targetRect,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
        _In_ LONG destinationStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
        _In_ LONG sourceStride,
        _In_ DWORD widthInPixels,
        _In_ DWORD heightInPixels);

protected: // New methods
    void getYuy2Pixel(const WORD* frame, Yuy2Pixel& pixel, const UINT32& index) const;
    void getYuy2Pixel(const BYTE* frame, Yuy2Pixel& pixel, const UINT32& x, const UINT32& y, const DWORD& frameWidthInPixels) const;

    const UINT8 getYuy2NeighborPixels(
        const BYTE* frame, const UINT32& x, const UINT32& y,
        const DWORD& frameWidthInPixels, const DWORD& frameHeightInPixels);

    void applyNearestNeighborSmootherYuy2(
        const D2D_RECT_U& rcDest,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
        _In_ LONG lDestStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
        _In_ LONG lSrcStride,
        _In_ DWORD dwWidthInPixels,
        _In_ DWORD dwHeightInPixels);

    void getNv12Pixel(
        const BYTE* frame, SimpleYuvPixel& pixel, const DWORD& x, const DWORD& y,
        const DWORD& yPlaneLength, const DWORD& frameWidthInPixels) const;

    const UINT8 getNv12NeighborPixels(
        const BYTE* frame, const DWORD& x, const DWORD& y,
        const DWORD& frameWidthInPixels, const DWORD& frameHeightInPixels);

    void applyNearestNeighborSmootherNv12(
        const D2D_RECT_U& rcDest,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
        _In_ LONG lDestStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
        _In_ LONG lSrcStride,
        _In_ DWORD dwWidthInPixels,
        _In_ DWORD dwHeightInPixels);

protected: // Members
    Yuy2Pixel _neighborYuy2Pixels[NEIGHBOR_PIXEL_COUNT];
    SimpleYuvPixel _neighborNv12Pixels[NEIGHBOR_PIXEL_COUNT];
};

#endif // NOISEREMOVALEFFECT_H
