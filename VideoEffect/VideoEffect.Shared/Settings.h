#pragma once

class Settings
{
public:
    static Settings* instance();

public:
    void setTargetYuv(const Platform::Array<float, 1>^ targetYuv);

private:
    Settings();
    ~Settings();

public:
    float m_targetYuv[3];
    float m_threshold;
    int m_mode;
};

