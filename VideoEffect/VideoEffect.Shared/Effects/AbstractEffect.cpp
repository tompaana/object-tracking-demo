#include "pch.h"

#include "AbstractEffect.h"
#include "Settings.h"

AbstractEffect::AbstractEffect(GUID videoFormatSubtype)
    : m_videoFormatSubtype(videoFormatSubtype),
    m_settings(Settings::instance())
{
}

GUID AbstractEffect::videoFormatSubtype() const
{
    return m_videoFormatSubtype;
}
