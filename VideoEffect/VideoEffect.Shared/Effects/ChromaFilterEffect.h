#ifndef CHROMAFILTEREFFECT_H
#define CHROMAFILTEREFFECT_H

#include "AbstractEffect.h"
#include "ImageProcessing\ObjectDetails.h"


class ChromaFilterEffect : public AbstractEffect
{
public:
    ChromaFilterEffect(GUID videoFormatSubtype);

public: // From AbstractFilter
    virtual void apply(
        const D2D_RECT_U& targetRect,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
        _In_ LONG destinationStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
        _In_ LONG sourceStride,
        _In_ DWORD widthInPixels,
        _In_ DWORD heightInPixels);

public: // New methods
    ObjectDetails currentObject() const;
    void setDimmUnselectedPixels(const bool& dimmUnselecctedPixels);

protected: // New methods
    void getColorFilteredValues(
        BYTE* py, BYTE* pu, BYTE* pv,
        const BYTE& targetY, const BYTE& targetU, const BYTE& targetV,
        const BYTE& threshold, const bool& dimmUnselected);

    void applyYUY2(
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
        _In_ DWORD dwHeightInPixels);

    void applyNV12(
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
        _In_ DWORD dwHeightInPixels);

protected: // Members
    ObjectDetails m_objectDetails;
    bool m_dimmUnselectedPixels;
};

#endif // CHROMAFILTEREFFECT_H
