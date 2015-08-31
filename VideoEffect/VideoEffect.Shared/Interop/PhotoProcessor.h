#ifndef CPHOTOPROCESSOR_H
#define CPHOTOPROCESSOR_H

#define MAX_FRAME_COUNT 2

class AbstractEffect;
class ImageAnalyzer;
class ImageProcessingUtils;
class NoiseRemovalEffect;


namespace VideoEffect
{
    public ref class CPhotoProcessor sealed
    {
    public:
        CPhotoProcessor();
        virtual ~CPhotoProcessor();

    public:
        void SetMode(int mode);
        void SetTargetYUV(const Platform::Array<float, 1>^ targetYuv);
        void SetThreshold(float threshold);
        void SetFrame(const Platform::Array<byte, 1>^ pixelArray, int width, int height, int frameIndex);
        Platform::Array<byte>^ ProcessFrames(int frameIndex, bool removeNoise, bool applyEffectOnly);

    private:
        const UINT32 FrameSizeAsUint32(int frameIndex) const;
        BYTE* PlatformArrayToByteArray(const Platform::Array<byte, 1>^ pixelArray) const;

    private: // Members
        ImageProcessingUtils *m_imageProcessingUtils;
        ImageAnalyzer *m_imageAnalyzer;
        NoiseRemovalEffect *m_noiseRemovalEffect;
        AbstractEffect *m_effect;
        BYTE *m_frame[MAX_FRAME_COUNT];
        size_t m_frameSize[MAX_FRAME_COUNT];
        int m_frameWidth[MAX_FRAME_COUNT];
        int m_frameHeight[MAX_FRAME_COUNT];
        GUID m_videoFormatSubtype;
    };
}

#endif // CPHOTOPROCESSOR_H
