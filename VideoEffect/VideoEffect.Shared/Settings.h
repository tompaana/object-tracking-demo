#pragma once

class Settings
{
public:
	static Settings* instance();

private:
	Settings();
	~Settings();

public:
	float m_targetYUV[3];
	float m_threshold;
};

