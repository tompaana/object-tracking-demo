#ifndef SIMPLEYUVPIXEL_H
#define SIMPLEYUVPIXEL_H

#include <mfapi.h>

// TODO: Move average calculation methods to ImageProcessingUtils

class SimpleYuvPixel
{
public:
    SimpleYuvPixel() :
        _y(0x0),
        _u(0x0),
        _v(0x0)
    {
    }

    static SimpleYuvPixel* average(const SimpleYuvPixel pixels[], const UINT8& pixelCount)
    {
        SimpleYuvPixel* averagePixel = new SimpleYuvPixel();
        getAverage(pixels, pixelCount, *averagePixel);
        return averagePixel;
    }

    static void getAverage(const SimpleYuvPixel pixels[], const UINT8& pixelCount, SimpleYuvPixel& averagePixel)
    {
        float ySum = 0;
        float uSum = 0;
        float vSum = 0;

        for (UINT8 i = 0; i < pixelCount; ++i)
        {
            ySum += (float)pixels[i]._y;
            uSum += (float)pixels[i]._u;
            vSum += (float)pixels[i]._v;
        }

        averagePixel._y = (BYTE)(ySum / (float)pixelCount);
        averagePixel._u = (BYTE)(uSum / (float)pixelCount);
        averagePixel._v = (BYTE)(vSum / (float)pixelCount);
    }

    SimpleYuvPixel & operator= (const SimpleYuvPixel & other)
    {
        if (this != &other)
        {
            _y = other._y;
            _u = other._u;
            _v = other._v;
        }

        return *this;
    }

    void calculateAverage(const SimpleYuvPixel pixels[], const UINT8& pixelCount)
    {
        getAverage(pixels, pixelCount, *this);
    }

public: // Members
    BYTE _y;
    BYTE _u;
    BYTE _v;
};

#endif // SIMPLEYUVPIXEL_H
