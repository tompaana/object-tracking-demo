// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "RealtimeTransform.h"

#include "BufferLock.h"
#include "Effects\ChromaDeltaEffect.h"
#include "Effects\ChromaFilterEffect.h"
#include "Effects\EdgeDetectionEffect.h"
#include "Effects\NoiseRemovalEffect.h"
#include "ImageProcessing\ImageAnalyzer.h"
#include "ImageProcessing\ImageProcessingUtils.h"
#include "ImageProcessing\ObjectDetails.h"
#include "Interop\InteropUtils.h"
#include "Settings.h"

#include <wrl.h>
#include <robuffer.h>
#include <collection.h>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm> // for str.remove
#include <wrl\module.h>

#pragma comment(lib, "d2d1")

using namespace Microsoft::WRL;
using namespace Platform::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;
using namespace Platform;

ActivatableClass(CRealtimeTransform);


// Constants
const float RelativeJitterThresholdForItemLock = 1.0f; // Jitter relative to item size (0.5 means the jitter cannot exceed half of item size)
const float RelativeJitterThresholdForDetectingMovement = 0.8f;
const float RelativeTargetCropPadding = RelativeJitterThresholdForItemLock - 0.5f; // Padding size relative to item size
const int NumberOfIterationsNeededForItemLock = 5;
const int NumberOfOperationTimeMeasurementsNeededForAverage = 10;



CRealtimeTransform::CRealtimeTransform() :
    CAbstractTransform(),
    m_effect(NULL),
    m_noiseRemovalEffect(NULL),

    m_itemX(0),
    m_itemY(0),
    m_itemWidth(0),
    m_itemHeight(0),
    m_targetLockIterations(0),
    m_targetLocked(false),
    m_targetWasJustLocked(false),

    m_operationTimeMeasurementAverageInMilliseconds(0),
    m_operationTimeMeasurementCounter(0)
{
    InitializeCriticalSectionEx(&m_critSec, 3000, 0);

    m_systemTime0 = new SYSTEMTIME();
    m_systemTime1 = new SYSTEMTIME();

    m_operationTimeMeasurementsInMilliseconds = new WORD[NumberOfOperationTimeMeasurementsNeededForAverage];
}

CRealtimeTransform::~CRealtimeTransform()
{
    delete m_systemTime0;
    delete m_systemTime1;
    delete m_operationTimeMeasurementsInMilliseconds;
    delete m_effect;
    delete m_noiseRemovalEffect;
}

// Initialize the instance.
STDMETHODIMP CRealtimeTransform::RuntimeClassInitialize()
{
    // Create the attribute store.
    return MFCreateAttributes(&m_pAttributes, 3);
}


//-------------------------------------------------------------------
// ProcessOutput
// Process an output sample.
//-------------------------------------------------------------------

HRESULT CRealtimeTransform::ProcessOutput(
    DWORD                   dwFlags,
    DWORD                   cOutputBufferCount,
    MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, // one per stream
    DWORD                   *pdwStatus
    )
{
    // Check input parameters...

    // This MFT does not accept any flags for the dwFlags parameter.

    // The only defined flag is MFT_PROCESS_OUTPUT_DISCARD_WHEN_NO_BUFFER. This flag 
    // applies only when the MFT marks an output stream as lazy or optional. But this
    // MFT has no lazy or optional streams, so the flag is not valid.

    if (dwFlags != 0)
    {
        return E_INVALIDARG;
    }

    if (pOutputSamples == NULL || pdwStatus == NULL)
    {
        return E_POINTER;
    }

    // There must be exactly one output buffer.
    if (cOutputBufferCount != 1)
    {
        return E_INVALIDARG;
    }

    // It must contain a sample.
    if (pOutputSamples[0].pSample == NULL)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = S_OK;

    IMFMediaBuffer *pInput = NULL;
    IMFMediaBuffer *pOutput = NULL;

    EnterCriticalSection(&m_critSec);

    // There must be an input sample available for processing.
    if (m_pSample == NULL)
    {
        hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
        goto done;
    }

    // Initialize streaming.

    hr = BeginStreaming();
    if (FAILED(hr))
    {
        goto done;
    }

    // Get the input buffer.
    hr = m_pSample->ConvertToContiguousBuffer(&pInput);
    if (FAILED(hr))
    {
        goto done;
    }

    // Get the output buffer.
    hr = pOutputSamples[0].pSample->ConvertToContiguousBuffer(&pOutput);
    if (FAILED(hr))
    {
        goto done;
    }

    if (m_messenger)
    {
        if (m_messenger->IsSettingsChangedFlagRaised())
        {
            // Get the changed settings
            m_settings->m_threshold = m_messenger->Threshold();
            m_settings->setTargetYuv(m_messenger->TargetYuv());
            m_settings->m_removeNoise = m_messenger->RemoveNoise();
            m_settings->m_applyEffectOnly = m_messenger->ApplyEffectOnly();
            ClearTargetLock();
        }

        if ((m_messenger->State() != VideoEffectState::Locked
             && m_messenger->State() != VideoEffectState::Triggered)
            && m_messenger->IsModeChangedFlagRaised())
        {
            SetMode((Mode)m_messenger->Mode());
        }
    }

    hr = OnProcessOutput(pInput, pOutput);

    if (FAILED(hr))
    {
        goto done;
    }

    // Set status flags.
    pOutputSamples[0].dwStatus = 0;
    *pdwStatus = 0;

    // Copy the duration and time stamp from the input sample, if present.

    LONGLONG hnsDuration = 0;
    LONGLONG hnsTime = 0;

    if (SUCCEEDED(m_pSample->GetSampleDuration(&hnsDuration)))
    {
        hr = pOutputSamples[0].pSample->SetSampleDuration(hnsDuration);
        if (FAILED(hr))
        {
            goto done;
        }
    }

    if (SUCCEEDED(m_pSample->GetSampleTime(&hnsTime)))
    {
        hr = pOutputSamples[0].pSample->SetSampleTime(hnsTime);
    }

done:
    SafeRelease(&m_pSample); // Release our input sample.
    SafeRelease(&pInput);
    SafeRelease(&pOutput);
    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// OnProcessOutput
//
// Generate output data.
//-------------------------------------------------------------------
HRESULT CRealtimeTransform::OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut)
{
    BYTE *pDest = NULL;         // Destination buffer.
    LONG lDestStride = 0;       // Destination stride.

    BYTE *pSrc = NULL;          // Source buffer.
    LONG lSrcStride = 0;        // Source stride.

    // Helper objects to lock the buffers.
    VideoBufferLock inputLock(pIn);
    VideoBufferLock outputLock(pOut);

    // Stride if the buffer does not support IMF2DBuffer
    LONG lDefaultStride = 0;

    HRESULT hr = m_imageProcessingUtils->getDefaultStride(m_pInputType, &lDefaultStride);

    if (FAILED(hr))
    {
        goto done;
    }

    // Lock the input buffer.
    hr = inputLock.LockBuffer(lDefaultStride, m_imageHeightInPixels, &pSrc, &lSrcStride);

    if (FAILED(hr))
    {
        goto done;
    }

    // Lock the output buffer.
    hr = outputLock.LockBuffer(lDefaultStride, m_imageHeightInPixels, &pDest, &lDestStride);

    if (FAILED(hr))
    {
        goto done;
    }

    // Invoke the image transform function.

    GetSystemTime(m_systemTime0);

    if (m_messenger->State() == VideoEffectState::Idle
        || m_messenger->State() == VideoEffectState::Triggered
        || m_messenger->State() == VideoEffectState::PostProcess
        || m_settings->m_mode == Mode::Passthrough)
    {
        // No transform - simply copy the frame
        UINT32 arrayLength = lDestStride * m_imageHeightInPixels;

        if (m_videoFormatSubtype == MFVideoFormat_NV12)
        {
#pragma warning(push)
#pragma warning(disable: 4244)
            arrayLength *= 1.5f;
#pragma warning(pop)
        }

        CopyMemory(pDest, pSrc, arrayLength);

        if (m_settings->m_removeNoise && m_noiseRemovalEffect)
        {
            m_noiseRemovalEffect->apply(
                m_rcDest,
                pDest, lDestStride, pSrc, lSrcStride,
                m_imageWidthInPixels, m_imageHeightInPixels);
        }
    }
    else if (m_messenger->State() == VideoEffectState::Locking
             || m_messenger->State() == VideoEffectState::Locked)
    {
        BYTE *sourceFrame = pSrc;

        if (m_settings->m_removeNoise && m_noiseRemovalEffect)
        {
            m_noiseRemovalEffect->apply(
                m_rcDest,
                pDest, lDestStride, sourceFrame, lSrcStride,
                m_imageWidthInPixels, m_imageHeightInPixels);

            sourceFrame = pDest;
        }

        if (m_settings->m_mode == Mode::ChromaFilter)
        {
            dynamic_cast<ChromaFilterEffect*>(m_effect)->setDimmUnselectedPixels(m_targetLocked);
        }

        m_effect->apply(
            m_rcDest,
            pDest, lDestStride, sourceFrame, lSrcStride,
            m_imageWidthInPixels, m_imageHeightInPixels);

        ObjectDetails objectDetails;

        if (!m_settings->m_applyEffectOnly)
        {
            if (m_targetLocked)
            {
                if (m_targetWasJustLocked)
                {
                    if (m_settings->m_mode == Mode::ChromaFilter)
                    {
                        objectDetails = dynamic_cast<ChromaFilterEffect*>(m_effect)->currentObject();
#pragma warning(push)
#pragma warning(disable: 4244) 
                        m_itemX = objectDetails._centerX;
                        m_itemY = objectDetails._centerY;
                        m_itemWidth = objectDetails._width;
                        m_itemHeight = objectDetails._height;
#pragma warning(pop)
                    }

                    m_targetWasJustLocked = false;
                }
                else
                {
                    if (m_settings->m_mode == Mode::ChromaFilter)
                    {
                        objectDetails = dynamic_cast<ChromaFilterEffect*>(m_effect)->currentObject();
                    }
                }

                if (objectDetails._width > 0)
                {
                    UpdateTargetLock(objectDetails);
                    DrawCrosshair(pDest, objectDetails);
                }
                else
                {
                    // No object detected, but state is locked -> trigger
                    if (m_settings->m_mode == Mode::ChromaFilter)
                    {
                        m_messenger->SetState(VideoEffectState::Triggered);
                    }

                    ClearTargetLock();
                }
            }
            else
            {
                // Locking
                ConvexHull *convexHull = ExtractBestCircularConvexHull(pDest, m_rcDest, 3, true);

                if (convexHull)
                {
                    double radius = 0;
                    D2D_POINT_2U circleCenter;
                    m_imageAnalyzer->getMinimalEnclosingCircle(*convexHull, radius, circleCenter);
                    
                    objectDetails._centerX = circleCenter.x;
                    objectDetails._centerY = circleCenter.y;
                    objectDetails._width = (UINT32)(radius * 2);
                    objectDetails._height = objectDetails._width;

                    UpdateTargetLock(objectDetails);

                    delete convexHull;

                    DrawCrosshair(pDest, objectDetails);
                }
                else
                {
                    ClearTargetLock();
                }
            }
        }
    }

    GetSystemTime(m_systemTime1);
    UpdateOperationTimeAverage(m_systemTime1->wMilliseconds - m_systemTime0->wMilliseconds);
    m_messenger->UpdateOperationDurationInMilliseconds(OperationTimeAverage());

    // Set the data size on the output buffer.
    hr = pOut->SetCurrentLength(m_cbImageSize);

    // The VideoBufferLock class automatically unlocks the buffers.
done:
    return hr;
}


//-------------------------------------------------------------------
// OnMfMtSubtypeResolved
//
// Sets the effect based on the given subtype.
//-------------------------------------------------------------------
void CRealtimeTransform::OnMfMtSubtypeResolved(const GUID subtype)
{
    CAbstractTransform::OnMfMtSubtypeResolved(subtype);
    m_noiseRemovalEffect = new NoiseRemovalEffect(subtype);
}


//-------------------------------------------------------------------
// OperationTimeAverage
//
// Returns the operation time average.
//-------------------------------------------------------------------
WORD CRealtimeTransform::OperationTimeAverage()
{
    return m_operationTimeMeasurementAverageInMilliseconds;
}


//-------------------------------------------------------------------
// ExtractBestCircularConvexHull
//
// Extracts convex hulls from the given frame and tries to determine
// the best candidate to match the desired object.
//
// Note that the returned convex hull is owned by the caller.
//-------------------------------------------------------------------
ConvexHull *CRealtimeTransform::ExtractBestCircularConvexHull(
    BYTE *pFrame, const D2D_RECT_U &targetRect, int maxConvexHullCount, bool drawConvexHulls) const
{
    return m_imageAnalyzer->extractBestCircularConvexHull(
        pFrame, m_imageWidthInPixels, m_imageHeightInPixels, targetRect,
        maxConvexHullCount, m_effect->videoFormatSubtype(), drawConvexHulls);
}


//-------------------------------------------------------------------
// SetMode
//
//-------------------------------------------------------------------
void CRealtimeTransform::SetMode(const Mode& mode)
{
    m_settings->m_mode = mode;
    ClearTargetLock();

    delete m_effect;
    m_effect = NULL;

    switch (mode)
    {
    case Mode::ChromaDelta:
        m_effect = new ChromaDeltaEffect(m_videoFormatSubtype);
        break;
    case Mode::ChromaFilter:
        m_effect = new ChromaFilterEffect(m_videoFormatSubtype);
        break;
    case Mode::EdgeDetection:
        m_effect = new EdgeDetectionEffect(m_videoFormatSubtype);
        break;
    }
}


//-------------------------------------------------------------------
// ClearTargetLock
//-------------------------------------------------------------------
void CRealtimeTransform::ClearTargetLock()
{
    m_targetWasJustLocked = false;
    m_targetLocked = false;
    m_targetLockIterations = 0;
    m_rcDest = D2D1::RectU(0, 0, m_imageWidthInPixels, m_imageHeightInPixels);
}


//-------------------------------------------------------------------
// UpdateTargetLock
//
//
//-------------------------------------------------------------------
void CRealtimeTransform::UpdateTargetLock(const ObjectDetails &objectDetails)
{
    if (m_targetLockIterations == 0)
    {
#pragma warning(push)
#pragma warning(disable: 4244) 
        m_itemX = objectDetails._centerX;
        m_itemY = objectDetails._centerY;
        m_itemWidth = objectDetails._width;
        m_itemHeight = objectDetails._height;
#pragma warning(pop)
        m_targetLockIterations++;
    }
    else if (m_targetLockIterations < NumberOfIterationsNeededForItemLock)
    {
        const float maxJitterX = m_itemWidth * RelativeJitterThresholdForItemLock;
        const float maxJitterY = m_itemHeight * RelativeJitterThresholdForItemLock;

        if (abs(m_itemX - objectDetails._centerX) > maxJitterX
            || abs(m_itemY - objectDetails._centerY) > maxJitterY
            || abs(m_itemWidth - objectDetails._width) > maxJitterX
            || abs(m_itemHeight - objectDetails._height) > maxJitterY)
        {
            // Jitter threshold exceeded, discard the current locking process and start over
            ClearTargetLock();
        }
        else
        {
            m_itemX = (m_itemX + objectDetails._centerX) / 2;
            m_itemY = (m_itemY + objectDetails._centerY) / 2;
            m_itemWidth = (m_itemWidth + objectDetails._width) / 2;
            m_itemHeight = (m_itemHeight + objectDetails._height) / 2;
            m_targetLockIterations++;
        }
    }

    if (m_targetLockIterations == NumberOfIterationsNeededForItemLock)
    {
        if (!m_targetLocked)
        {
            // Lock successfull
#pragma warning(push)
#pragma warning(disable: 4244) 
            int left = (float)m_itemX - (float)(m_itemWidth * (RelativeTargetCropPadding + 0.5f));
            int right = (float)m_itemX + (float)(m_itemWidth * (RelativeTargetCropPadding + 0.5f));
            int top = (float)m_itemY - (float)(m_itemHeight * (RelativeTargetCropPadding + 0.5f));
            int bottom = (float)m_itemY + (float)(m_itemHeight * (RelativeTargetCropPadding + 0.5f));

            if (left < 0)
            {
                left = 0;
            }

            if (top < 0)
            {
                top = 0;
            }
            
            m_rcDest.left = left;
            m_rcDest.right = right;
            m_rcDest.top = top;
            m_rcDest.bottom = bottom;
#pragma warning(pop)

            m_targetLocked = true;
            
            m_messenger->SetLockedRect((int)m_itemX, (int)m_itemY, (int)(right - left), (int)(bottom - top));
            m_messenger->SetState(VideoEffectState::Locked); // Notify
            m_targetWasJustLocked = true;
        }
        else
        {
            // Locked
            const float maxJitterX = m_itemWidth * RelativeJitterThresholdForDetectingMovement;
            const float maxJitterY = m_itemHeight * RelativeJitterThresholdForDetectingMovement;

            if (abs(m_itemX - objectDetails._centerX) > maxJitterX
                || abs(m_itemY - objectDetails._centerY) > maxJitterY
                || abs(m_itemWidth - objectDetails._width) > maxJitterX
                || abs(m_itemHeight - objectDetails._height) > maxJitterY)
            {
                // Target motion (or camera motion) exceeded the limit
                m_messenger->SetState(VideoEffectState::Triggered);
                ClearTargetLock();
            }
        }
    }
}


//-------------------------------------------------------------------
// DrawCrosshair
//
//
//-------------------------------------------------------------------
void CRealtimeTransform::DrawCrosshair(BYTE *pFrame, const ObjectDetails &objectDetails)
{
    D2D1_POINT_2U point1 = { 0, objectDetails._centerY };
    D2D1_POINT_2U point2 = { m_imageWidthInPixels, objectDetails._centerY };
    m_imageProcessingUtils->drawLine(pFrame, m_imageWidthInPixels, m_imageHeightInPixels, point1, point2, m_effect->videoFormatSubtype());

    point1.x = point2.x = objectDetails._centerX;
    point1.y = 0;
    point2.y = m_imageHeightInPixels;
    m_imageProcessingUtils->drawLine(pFrame, m_imageWidthInPixels, m_imageHeightInPixels, point1, point2, m_effect->videoFormatSubtype());
}


//-------------------------------------------------------------------
// UpdateOperationTimeAverage
//
//
//-------------------------------------------------------------------
void CRealtimeTransform::UpdateOperationTimeAverage(const WORD &measuredOperationTimeInMilliseconds)
{
    if (m_operationTimeMeasurementCounter < NumberOfOperationTimeMeasurementsNeededForAverage)
    {
        if (measuredOperationTimeInMilliseconds < 1000) // Sanity check
        {
            m_operationTimeMeasurementsInMilliseconds[m_operationTimeMeasurementCounter] = measuredOperationTimeInMilliseconds;
            m_operationTimeMeasurementCounter++;
        }
    }
    else
    {
        WORD sum = 0;

        for (int i = 0; i < NumberOfOperationTimeMeasurementsNeededForAverage; ++i)
        {
            sum += m_operationTimeMeasurementsInMilliseconds[i];
        }

        m_operationTimeMeasurementAverageInMilliseconds = sum / NumberOfOperationTimeMeasurementsNeededForAverage;
        m_operationTimeMeasurementCounter = 0;
    }
}
