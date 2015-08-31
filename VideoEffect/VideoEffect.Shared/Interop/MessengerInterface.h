#include "pch.h"
#pragma once

namespace VideoEffect
{
    public interface class MessengerInterface
    {
    public:
        virtual void SetState(int state) = 0;
        virtual int State() = 0;

        virtual void SetLockedRect(int centerX, int centerY, int width, int height);

        virtual void UpdateOperationDurationInMilliseconds(int milliseconds) = 0;

        virtual void SaveFrame(
            const Platform::Array<byte, 1>^ pixelArray,
            int width, int height,
            int counter, int seriesIdentifier) = 0;

        virtual bool IsSettingsChangedFlagRaised();
        virtual float Threshold();
        virtual Platform::Array<float, 1>^ TargetYuv();
        virtual bool RemoveNoise();
        virtual bool ApplyEffectOnly();

        virtual bool IsModeChangedFlagRaised();
        virtual int Mode();

        virtual int IsFrameRequested();

        virtual void NotifyFrameCaptured(const Platform::Array<byte, 1>^ pixelArray, int width, int height, int frameId) = 0;

        virtual void NotifyPostProcessComplete(
            const Platform::Array<byte, 1>^ pixelArray, int imageWidth, int imageHeight,
            int fromCenterX, int fromCenterY, int fromWidth, int fromHeight,
            int toCenterX, int toCenterY, int toWidth, int toHeight) = 0;
    };
}
