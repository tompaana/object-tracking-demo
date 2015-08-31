#ifndef YUY2PIXEL_H
#define YUY2PIXEL_H

#include <mfapi.h>

// TODO: Use SimpleYuvPixel class instead of this and remove this

class Yuy2Pixel
{
public:
    Yuy2Pixel() :
        _y0(0x0),
        _y1(0x0),
        _u(0x0),
        _v(0x0)
    {
    }

    static Yuy2Pixel* average(const Yuy2Pixel pixels[], const UINT8& pixelCount)
    {
        Yuy2Pixel* averagePixel = new Yuy2Pixel();
        getAverage(pixels, pixelCount, *averagePixel);
        return averagePixel;
    }

    static void getAverage(const Yuy2Pixel pixels[], const UINT8& pixelCount, Yuy2Pixel& averagePixel)
    {
        float y0Sum = 0;
        float y1Sum = 0;
        float uSum = 0;
        float vSum = 0;

        for (UINT8 i = 0; i < pixelCount; ++i)
        {
            y0Sum += (float)pixels[i]._y0;
            y1Sum += (float)pixels[i]._y1;
            uSum += (float)pixels[i]._u;
            vSum += (float)pixels[i]._v;
        }

        averagePixel._y0 = (BYTE)(y0Sum / (float)pixelCount);
        averagePixel._y1 = (BYTE)(y1Sum / (float)pixelCount);
        averagePixel._u = (BYTE)(uSum / (float)pixelCount);
        averagePixel._v = (BYTE)(vSum / (float)pixelCount);
    }

    Yuy2Pixel & operator= (const Yuy2Pixel & other)
    {
        if (this != &other)
        {
            _y0 = other._y0;
            _y1 = other._y1;
            _u = other._u;
            _v = other._v;
        }

        return *this;
    }

    void calculateAverage(const Yuy2Pixel pixels[], const UINT8& pixelCount)
    {
        getAverage(pixels, pixelCount, *this);
    }

public: // Members
    BYTE _y0;
    BYTE _y1;
    BYTE _u;
    BYTE _v;
};

#endif // YUY2PIXEL_H
