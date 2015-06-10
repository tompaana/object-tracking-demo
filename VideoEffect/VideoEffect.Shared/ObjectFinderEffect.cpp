// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "pch.h"

#include "BufferLock.h"
#include "ObjectFinderEffect.h"
#include "ImageAnalyzer.h"
#include "ImageProcessingUtils.h"
#include "ObjectDetails.h"
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

ActivatableClass(CObjectFinderEffect);


// Constants
const float RelativeJitterThresholdForItemLock = 1.0f; // Jitter relative to item size (0.5 means the jitter cannot exceed half of item size)
const float RelativeJitterThresholdForDetectingMovement = 0.8f;
const float RelativeTargetCropPadding = RelativeJitterThresholdForItemLock - 0.5f; // Padding size relative to item size
const int NumberOfIterationsNeededForItemLock = 5;
const int NumberOfOperationTimeMeasurementsNeededForAverage = 10;



CObjectFinderEffect::CObjectFinderEffect() :
	CAbstractEffect(),
	
    //m_transform(D2D1::Matrix3x2F::Identity()),

    m_croppedProcessingArea({ 0, 0, 0, 0 }),
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

CObjectFinderEffect::~CObjectFinderEffect()
{
	delete m_systemTime0;
	delete m_systemTime1;
	delete m_operationTimeMeasurementsInMilliseconds;
}

// Initialize the instance.
STDMETHODIMP CObjectFinderEffect::RuntimeClassInitialize()
{
    // Create the attribute store.
    return MFCreateAttributes(&m_pAttributes, 3);
}

// IMediaExtension methods

//-------------------------------------------------------------------
// SetProperties
// Sets the configuration of the effect
//-------------------------------------------------------------------
HRESULT CObjectFinderEffect::SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *pConfiguration)
{
	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> spSetting;
	pConfiguration->QueryInterface(IID_PPV_ARGS(&spSetting));

	HSTRING key;
	WindowsCreateString(L"Threshold", 9, &key);
	boolean found = false;
    m_settings->m_threshold = GetFloatProperty(spSetting, key, found);

	WindowsCreateString(L"PropertyY", 9, &key);
	m_settings->m_targetYUV[0] = GetFloatProperty(spSetting, key, found);

	WindowsCreateString(L"PropertyU", 9, &key);
	m_settings->m_targetYUV[1] = GetFloatProperty(spSetting, key, found);

	WindowsCreateString(L"PropertyV", 9, &key);
	m_settings->m_targetYUV[2] = GetFloatProperty(spSetting, key, found);

	IPropertySet^ properties = reinterpret_cast<IPropertySet^>(pConfiguration);
	m_messenger = safe_cast<VideoEffect::MessengerInterface^>(properties->Lookup(L"Communication"));
	
	ClearTargetLock();

	return S_OK;
}


//-------------------------------------------------------------------
// GetFloatProperty
// 
//-------------------------------------------------------------------
float CObjectFinderEffect::GetFloatProperty(
	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> &spSetting,
	const HSTRING &key, boolean &found)
{
	float value = 0.0f;

	spSetting->HasKey(key, &found);
	IInspectable* valueAsInsp = NULL;

	if (found)
	{
		spSetting->Lookup(key, &valueAsInsp);
	}

	if (valueAsInsp)
	{
		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IPropertyValue> pPropertyValue;
		HRESULT hr = valueAsInsp->QueryInterface(IID_PPV_ARGS(&pPropertyValue));

		if (!FAILED(hr))
		{
			hr = pPropertyValue->GetSingle(&value);
		}
	}

	return value;
}


//-------------------------------------------------------------------
// ProcessOutput
// Process an output sample.
//-------------------------------------------------------------------

HRESULT CObjectFinderEffect::ProcessOutput(
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
HRESULT CObjectFinderEffect::OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut)
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
    assert (m_pTransformFn != NULL);

    if (m_messenger->State() == VideoEffectState::Idle
        || m_messenger->State() == VideoEffectState::Triggered
        || m_messenger->State() == VideoEffectState::PostProcess)
    {
        // No transform - simply copy the frame
        UINT32 arrayLength = lDestStride * m_imageHeightInPixels;

        if (m_pTransformFn == ChromaFilterTransformImageNV12)
        {
#pragma warning(push)
#pragma warning(disable: 4244)
            arrayLength *= 1.5f;
#pragma warning(pop)
        }

        CopyMemory(pDest, pSrc, arrayLength);
    }
    else if (m_messenger->State() == VideoEffectState::Locking
        || m_messenger->State() == VideoEffectState::Locked)
    {
        GetSystemTime(m_systemTime0);
        ObjectDetails objectDetails;

        // TODO: Using cropping seems to cause some weird problems, fix 'em!
        (*m_pTransformFn)(objectDetails, m_settings->m_targetYUV, static_cast<BYTE>(m_settings->m_threshold), m_targetLocked,
            /*m_targetLocked ? m_croppedProcessingArea :*/ m_rcDest,
            pDest, lDestStride, pSrc, lSrcStride,
            m_imageWidthInPixels, m_imageHeightInPixels);

        GetSystemTime(m_systemTime1);
        UpdateOperationTimeAverage(m_systemTime1->wMilliseconds - m_systemTime0->wMilliseconds);

        m_messenger->UpdateOperationDurationInMilliseconds(OperationTimeAverage());

        if (objectDetails._width > 0)
        {
            if (m_targetLocked)
            {
                if (m_targetWasJustLocked)
                {
                    m_targetWasJustLocked = false;
                }

                UpdateTargetLock(objectDetails);
            }
            else
            {
                // Locking
                ConvexHull* convexHull = m_imageAnalyzer->bestConvexHullDetails(pDest, m_imageWidthInPixels, m_imageHeightInPixels, 3, m_pTransformFn);

                if (convexHull)
                {
                    if (m_imageAnalyzer->objectIsWithinConvexHullBounds(objectDetails, *convexHull))
                    {
                        // The following method, will change the state to Triggered, if enough
                        // movement is detected
                        UpdateTargetLock(objectDetails);
                    }
                    else
                    {
                        ClearTargetLock();
                    }

                    delete convexHull;
                }
                else
                {
                    ClearTargetLock();
                }
            }

            // Draw crosshair
            D2D1_POINT_2U point1 = { 0, objectDetails._centerY };
            D2D1_POINT_2U point2 = { m_imageWidthInPixels, objectDetails._centerY };
            m_imageProcessingUtils->drawLine(pDest, m_imageWidthInPixels, m_imageHeightInPixels, point1, point2, m_pTransformFn);

            point1.x = point2.x = objectDetails._centerX;
            point1.y = 0;
            point2.y = m_imageHeightInPixels;
            m_imageProcessingUtils->drawLine(pDest, m_imageWidthInPixels, m_imageHeightInPixels, point1, point2, m_pTransformFn);
        }
        else if (m_targetLocked)
        {
            // No object detected, but target was locked
            if (m_targetWasJustLocked)
            {
                // Mystery bug workaround: Sometimes the item is lost just after locking and we
                // don't want to do triggering, since nothing really happened
                ClearTargetLock();
                m_messenger->SetState(VideoEffectState::Locking);
                m_targetWasJustLocked = false;
            }
            else
            {
                // No object detected, but state is locked -> trigger
                m_messenger->SetState(VideoEffectState::Triggered);
                ClearTargetLock();
            }
        }

        // TODO: Temporary until YUV2 is fully supported
        if (m_messenger->State() == VideoEffectState::Triggered
            && m_pTransformFn == ChromaFilterTransformImageYUY2)
        {
            // Post processing is not currently supported for YUV2.
            // Thus let's switch back to idle.
            m_messenger->SetState(VideoEffectState::Idle);
        }
    }

    // Set the data size on the output buffer.
    hr = pOut->SetCurrentLength(m_cbImageSize);

    // The VideoBufferLock class automatically unlocks the buffers.
done:
    return hr;
}


//-------------------------------------------------------------------
// OperationTimeAverage
//
// Returns the operation time average.
//-------------------------------------------------------------------
WORD CObjectFinderEffect::OperationTimeAverage()
{
	return m_operationTimeMeasurementAverageInMilliseconds;
}


//-------------------------------------------------------------------
// ClearTargetLock
//-------------------------------------------------------------------
void CObjectFinderEffect::ClearTargetLock()
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
void CObjectFinderEffect::UpdateTargetLock(const ObjectDetails &objectDetails)
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
            m_targetLockIterations = 0;
            m_targetLocked = false;
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

            
            m_croppedProcessingArea.left = left;
            m_croppedProcessingArea.right = right;
            m_croppedProcessingArea.top = top;
            m_croppedProcessingArea.bottom = bottom;
            
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
// UpdateOperationTimeAverage
//
//
//-------------------------------------------------------------------
void CObjectFinderEffect::UpdateOperationTimeAverage(const WORD &measuredOperationTimeInMilliseconds)
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
