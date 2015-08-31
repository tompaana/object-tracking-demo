#include "pch.h"

#include "PhotoProcessor.h"

#include "Common.h"
#include "Effects\ChromaDeltaEffect.h"
#include "Effects\ChromaFilterEffect.h"
#include "Effects\EdgeDetectionEffect.h"
#include "Effects\NoiseRemovalEffect.h"
#include "ImageProcessing\ImageAnalyzer.h"
#include "ImageProcessing\ImageProcessingUtils.h"
#include "Settings.h"

using namespace VideoEffect;


CPhotoProcessor::CPhotoProcessor()
    : m_imageProcessingUtils(NULL),
    m_imageAnalyzer(NULL),
    m_effect(NULL),
    m_videoFormatSubtype(MFVideoFormat_YUY2)
{
    for (int i = 0; i < MAX_FRAME_COUNT; ++i)
    {
        m_frame[i] = NULL;
        m_frameSize[i] = 0;
        m_frameWidth[i] = 0;
        m_frameHeight[i] = 0;
    }

    m_imageProcessingUtils = new ImageProcessingUtils();
    m_imageAnalyzer = new ImageAnalyzer(m_imageProcessingUtils);
    m_noiseRemovalEffect = new NoiseRemovalEffect(m_videoFormatSubtype);
}


CPhotoProcessor::~CPhotoProcessor()
{
    for (int i = 0; i < MAX_FRAME_COUNT; ++i)
    {
        free(m_frame[i]);
    }

    delete m_effect;
    delete m_imageAnalyzer;
    delete m_imageProcessingUtils;
}


void CPhotoProcessor::SetMode(int mode)
{
    Settings *settings = Settings::instance();
    settings->m_mode = mode;

    delete m_effect;
    m_effect = NULL;
    ChromaFilterEffect *chromaFilterEffect = NULL;

    switch (mode)
    {
    case Mode::ChromaDelta:
        m_effect = new ChromaDeltaEffect(m_videoFormatSubtype);
        break;
    case Mode::ChromaFilter:
        chromaFilterEffect = new ChromaFilterEffect(m_videoFormatSubtype);
        chromaFilterEffect->setDimmUnselectedPixels(true);
        m_effect = chromaFilterEffect;
        break;
    case Mode::EdgeDetection:
        m_effect = new EdgeDetectionEffect(m_videoFormatSubtype);
        break;
    }
}


void CPhotoProcessor::SetTargetYUV(const Platform::Array<float, 1>^ targetYuv)
{
    Settings::instance()->setTargetYuv(targetYuv);
}


void CPhotoProcessor::SetThreshold(float threshold)
{
    Settings::instance()->m_threshold = threshold;
}


void CPhotoProcessor::SetFrame(const Platform::Array<byte, 1>^ pixelArray, int width, int height, int frameIndex)
{
    if (m_frame[frameIndex] != NULL)
    {
        free(m_frame[frameIndex]);
    }

    m_frame[frameIndex] = PlatformArrayToByteArray(pixelArray);
    m_frameSize[frameIndex] = pixelArray->Length * sizeof(BYTE);
    m_frameWidth[frameIndex] = width;
    m_frameHeight[frameIndex] = height;
}


Platform::Array<byte>^ CPhotoProcessor::ProcessFrames(int frameIndex, bool removeNoise, bool applyEffectOnly)
{
    BYTE *processedFrame = NULL;
    Platform::Array<byte>^ byteArray = nullptr;

    if (m_effect && m_frame[frameIndex])
    {
        processedFrame = ImageProcessingUtils::copyFrame(m_frame[frameIndex], m_frameSize[frameIndex]);

        D2D_RECT_U targetRect;
        targetRect.top = 0;
        targetRect.right = m_frameWidth[frameIndex];
        targetRect.bottom = m_frameHeight[frameIndex];
        targetRect.left = 0;
        
        BYTE* originalFrame = m_frame[frameIndex];
        LONG stride = m_frameWidth[frameIndex] * 2;

        if (removeNoise)
        {
            m_noiseRemovalEffect->apply(
                targetRect,
                processedFrame, stride,
                originalFrame, stride,
                m_frameWidth[frameIndex], m_frameHeight[frameIndex]);

            originalFrame = ImageProcessingUtils::copyFrame(processedFrame, m_frameSize[frameIndex]);
        }

        int secondFrameIndex = (frameIndex == 0) ? 1 : 0;
        BYTE* secondFrame = NULL;

        if (Settings::instance()->m_mode == Mode::ChromaDelta)
        {
            secondFrame = m_frame[secondFrameIndex];

            if (removeNoise && secondFrame)
            {
                BYTE* secondFrameWithNoiseRemoved = ImageProcessingUtils::copyFrame(secondFrame, m_frameSize[secondFrameIndex]);
                targetRect.right = m_frameWidth[secondFrameIndex];
                targetRect.bottom = m_frameHeight[secondFrameIndex];
                stride = m_frameWidth[secondFrameIndex] * 2;

                m_noiseRemovalEffect->apply(
                    targetRect,
                    secondFrameWithNoiseRemoved, stride,
                    secondFrame, stride,
                    m_frameWidth[secondFrameIndex], m_frameHeight[secondFrameIndex]);

                secondFrame = secondFrameWithNoiseRemoved;
            }
        }

        targetRect.right = m_frameWidth[frameIndex];
        targetRect.bottom = m_frameHeight[frameIndex];
        stride = m_frameWidth[frameIndex] * 2;

        m_effect->apply(
            targetRect,
            processedFrame, stride,
            originalFrame, stride,
            m_frameWidth[frameIndex], m_frameHeight[frameIndex]);

        if (Settings::instance()->m_mode == Mode::ChromaDelta && secondFrame != NULL)
        {
            targetRect.right = m_frameWidth[secondFrameIndex];
            targetRect.bottom = m_frameHeight[secondFrameIndex];
            stride = m_frameWidth[secondFrameIndex] * 2;

            m_effect->apply(
                targetRect,
                processedFrame, stride,
                secondFrame, stride,
                m_frameWidth[secondFrameIndex], m_frameHeight[secondFrameIndex]);
        }

        if (!applyEffectOnly && Settings::instance()->m_mode != Mode::ChromaDelta)
        {
            delete m_imageAnalyzer->extractBestCircularConvexHull(
                processedFrame, m_frameWidth[frameIndex], m_frameHeight[frameIndex],
                5, m_effect->videoFormatSubtype(), true);
        }

        if (removeNoise)
        {
            free(originalFrame);
            free(secondFrame);
        }
    }
    else if (m_frame[frameIndex])
    {
        // Passthrough
        processedFrame = ImageProcessingUtils::copyFrame(m_frame[frameIndex], m_frameSize[frameIndex]);

        if (removeNoise)
        {
            D2D_RECT_U targetRect;
            targetRect.top = 0;
            targetRect.right = m_frameWidth[frameIndex];
            targetRect.bottom = m_frameHeight[frameIndex];
            targetRect.left = 0;

            BYTE* originalFrame = m_frame[frameIndex];
            LONG stride = m_frameWidth[frameIndex] * 2;

            m_noiseRemovalEffect->apply(
                targetRect,
                processedFrame, stride,
                originalFrame, stride,
                m_frameWidth[frameIndex], m_frameHeight[frameIndex]);
        }
    }

    if (processedFrame)
    {
        byteArray = ref new Platform::Array<byte>(processedFrame, FrameSizeAsUint32(0));
        free(processedFrame);
    }

    return byteArray;
}


//-------------------------------------------------------------------
// FrameSizeAsUint32()
//
// Returns the frame (data) size.
//-------------------------------------------------------------------
const UINT32 CPhotoProcessor::FrameSizeAsUint32(int frameIndex) const
{
    float coefficient = 2.0f;

    if (m_effect)
    {
        if (m_effect->videoFormatSubtype() == MFVideoFormat_NV12)
        {
            // NV12:
            // Y plane: width * height
            // UV plane: width * height / 2
            coefficient = 1.5f;
        }
        else if (m_effect->videoFormatSubtype() == MFVideoFormat_YUY2)
        {
            coefficient = 2.0f;
        }
    }

    return (UINT32)(m_frameWidth[frameIndex] * m_frameHeight[frameIndex] * coefficient);
}


BYTE* CPhotoProcessor::PlatformArrayToByteArray(const Platform::Array<byte, 1>^ pixelArray) const
{
    size_t arraySize = pixelArray->Length * sizeof(BYTE);
    BYTE *byteArray = (BYTE*)malloc(arraySize);
    memcpy(byteArray, pixelArray->Data, arraySize);
    return byteArray;
}

