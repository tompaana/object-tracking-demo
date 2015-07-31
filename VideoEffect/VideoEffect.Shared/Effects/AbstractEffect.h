#ifndef ABSTRACTEFFECT_H
#define ABSTRACTEFFECT_H

#include <d2d1.h>

class Settings;


class AbstractEffect
{
public: // Construction
    AbstractEffect(GUID videoFormatSubtype);

public: // Methods
    virtual void apply(
        const D2D_RECT_U& targetRect,
        _Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* destination,
        _In_ LONG destinationStride,
        _In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* source,
        _In_ LONG sourceStride,
        _In_ DWORD widthInPixels,
        _In_ DWORD heightInPixels) = 0;

    GUID videoFormatSubtype() const;

protected: // Members
    GUID m_videoFormatSubtype;
    Settings* m_settings;
};

#endif // ABSTRACTEFFECT_H
