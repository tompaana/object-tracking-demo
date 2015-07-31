#ifndef CHROMADELTAEFFECT_H
#define CHROMADELTAEFFECT_H

#include "AbstractEffect.h"


class ChromaDeltaEffect : public AbstractEffect
{
public:
    ChromaDeltaEffect(GUID videoFormatSubtype);
    virtual ~ChromaDeltaEffect();

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
    float calculateDelta(
        const BYTE& y0, const BYTE& u0, const BYTE& v0,
        const BYTE& y1, const BYTE& u1, const BYTE& v1,
        BYTE& dy, BYTE& du, BYTE& dv);

    void applyChromaDeltaYUY2(
        const BYTE& threshold,
        const bool& dimmFilteredPixels,
        const D2D_RECT_U& rcDest,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
        _In_ LONG lDestStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
        _In_ LONG lSrcStride,
        _In_ DWORD dwWidthInPixels,
        _In_ DWORD dwHeightInPixels);

    void applyChromaDeltaNV12(
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
    BYTE* m_previousFrame;
};

#endif // CHROMADELTAEFFECT_H
