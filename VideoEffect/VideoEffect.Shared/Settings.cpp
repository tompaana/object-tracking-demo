#include "pch.h"
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
	: m_threshold(0)
{
	m_targetYUV[0] = 0;
	m_targetYUV[1] = 0;
	m_targetYUV[2] = 0;
}


Settings::~Settings()
{
}
