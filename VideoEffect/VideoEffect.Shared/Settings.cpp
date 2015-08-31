#include "pch.h"

#include "Common.h"
#include "Settings.h"


static Settings* m_instance = NULL;


Settings* Settings::instance()
{
    if (!m_instance)
    {
        m_instance = new Settings();
    }

    return m_instance;
}


Settings::Settings()
    : m_threshold(0),
    m_mode(Mode::ChromaFilter),
    m_removeNoise(false),
    m_applyEffectOnly(false)
{
    m_targetYuv[0] = 0;
    m_targetYuv[1] = 0;
    m_targetYuv[2] = 0;
}


Settings::~Settings()
{
}


void Settings::setTargetYuv(const Platform::Array<float, 1>^ targetYuv)
{
    if (targetYuv)
    {
        try
        {
            m_targetYuv[0] = targetYuv[0];
            m_targetYuv[1] = targetYuv[1];
            m_targetYuv[2] = targetYuv[2];
        }
        catch (...)
        {
        }
    }
}
