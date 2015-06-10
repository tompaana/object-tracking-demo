#include "pch.h"

#include <wrl.h>
#include <robuffer.h>
#include <collection.h>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>  //for str.remove
#include <wrl\module.h>
#include <cstdlib> // for srand, rand
#include <ctime>   // for time

#include "BufferEffect.h"
#include "BufferLock.h"
#include "ImageAnalyzer.h"
#include "ImageProcessingUtils.h"
#include "ObjectDetails.h"
#include "Settings.h"

#pragma comment(lib, "d2d1")

using namespace Microsoft::WRL;
using namespace Platform::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;
using namespace Platform;

ActivatableClass(CBufferEffect);

typedef void(*UnmanagedEventFromAppHandler)(String^ msg);


CBufferEffect::CBufferEffect() :
	CAbstractEffect(),
    m_currentBufferIndex(0),
    m_numberOfFramesBufferedAfterTriggered(0),
    m_bufferIndexWhenTriggered(-1)
{
    InitializeCriticalSectionEx(&m_critSec, 3000, 0);

    for (UINT8 i = 0; i < BufferSize; ++i)
    {
        m_frameRingBuffer[i] = NULL;
    }
}

CBufferEffect::~CBufferEffect()
{
    SafeRelease(&m_pInputType);
    SafeRelease(&m_pOutputType);
    SafeRelease(&m_pSample);
    SafeRelease(&m_pAttributes);

    ClearFrameRingBuffer();

    DeleteCriticalSection(&m_critSec);
}


// Initialize the instance.
STDMETHODIMP CBufferEffect::RuntimeClassInitialize()
{
    // Create the attribute store.
    return MFCreateAttributes(&m_pAttributes, 3);
}


// IMediaExtension methods

//-------------------------------------------------------------------
// SetProperties
// Sets the configuration of the effect
//-------------------------------------------------------------------
HRESULT CBufferEffect::SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *pConfiguration)
{
	Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> spSetting;
	pConfiguration->QueryInterface(IID_PPV_ARGS(&spSetting));

	IPropertySet^ properties = reinterpret_cast<IPropertySet^>(pConfiguration);
	m_messenger = safe_cast<VideoEffect::MessengerInterface^>(properties->Lookup(L"Communication"));

	return S_OK;
}


// Generate output data.

HRESULT CBufferEffect::OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut)
{
	return -1;
}


//-------------------------------------------------------------------
// ProcessInput
// Process an input sample.
//-------------------------------------------------------------------

HRESULT CBufferEffect::ProcessInput(
    DWORD               dwInputStreamID,
    IMFSample           *pSample,
    DWORD               dwFlags
)
{
    // Check input parameters.
    if (pSample == NULL)
    {
        return E_POINTER;
    }

    if (dwFlags != 0)
    {
        return E_INVALIDARG; // dwFlags is reserved and must be zero.
    }

    HRESULT hr = S_OK;

    EnterCriticalSection(&m_critSec);

    // Validate the input stream 
    if (!IsValidInputStream(dwInputStreamID))
    {
        hr = MF_E_INVALIDSTREAMNUMBER;
        goto done;
    }

    // Check for valid media types.
    // The client must set input and output types before calling ProcessInput.
    if (!m_pInputType || !m_pOutputType)
    {
        hr = MF_E_NOTACCEPTING;   
        goto done;
    }

    int frameRequestId = m_messenger->FrameRequested();

    if (frameRequestId > 0)
	{
        UINT8 previousFrameBufferIndex = PreviousBufferIndex(m_currentBufferIndex);

        if (m_frameRingBuffer[previousFrameBufferIndex])
        {
            // Provide the previous frame in the buffer
            NotifyFrameCaptured(previousFrameBufferIndex, frameRequestId);
        }
        else
        {
            // Provide the current sample
            NotifyFrameCaptured(pSample, frameRequestId);
        }
	}

	if (m_messenger->State() == VideoEffectState::Triggered)
	{
        m_numberOfFramesBufferedAfterTriggered++;

        if (m_numberOfFramesBufferedAfterTriggered > BufferSize / 2)
		{
			m_messenger->SetState(VideoEffectState::PostProcess);
            m_numberOfFramesBufferedAfterTriggered = 0;
		}

        if (m_bufferIndexWhenTriggered < 0)
		{
            // Store the index when triggered
            m_bufferIndexWhenTriggered = m_currentBufferIndex;
		}
	}

	if (m_messenger->State() == VideoEffectState::PostProcess)
	{
		//SaveBuffers();

		// Find the object from last frame and the frame which triggered the state change
        UINT8 closestBufferIndexToTriggeredWithObject = 0;
		UINT8 lastBufferIndexWithObject = 0;
        ObjectDetails objectDetailsCloseToTriggeredFrame;
		ObjectDetails objectDetailsFromLastFrame;

        if (GetFirstFrameWithObject(
                m_bufferIndexWhenTriggered, PreviousBufferIndex(m_bufferIndexWhenTriggered), false,
                closestBufferIndexToTriggeredWithObject, objectDetailsCloseToTriggeredFrame)
            && GetFirstFrameWithObject(
                m_currentBufferIndex, CalculateBufferIndex(m_currentBufferIndex, -((int)BufferSize / 2) + 1), false,
                lastBufferIndexWithObject, objectDetailsFromLastFrame)
            && lastBufferIndexWithObject != closestBufferIndexToTriggeredWithObject)
		{
            LONG stride = 0;
            BYTE *lastFrame = m_frameRingBuffer[lastBufferIndexWithObject];
            BYTE *triggeredFrame = m_frameRingBuffer[closestBufferIndexToTriggeredWithObject];

            bool lastFrameIsLeft = objectDetailsFromLastFrame._centerX < objectDetailsCloseToTriggeredFrame._centerX;
            UINT32 joinX = (objectDetailsFromLastFrame._centerX + objectDetailsCloseToTriggeredFrame._centerX) / 2;

            BYTE *mergedFrame = ImageProcessingUtils::mergeFramesNV12(
                (lastFrameIsLeft ? lastFrame : triggeredFrame),
                (lastFrameIsLeft ? triggeredFrame : lastFrame),
                m_imageWidthInPixels, m_imageHeightInPixels, joinX);

            NotifyPostProcessComplete(
                mergedFrame, FrameSizeAsUint32(),
                objectDetailsCloseToTriggeredFrame, objectDetailsFromLastFrame);

            delete[] mergedFrame;
		}

        ClearFrameRingBuffer();
        m_bufferIndexWhenTriggered = -1;

		m_messenger->SetState(VideoEffectState::Idle);
	} // if (m_messenger->State() == VideoEffectState::PostProcess)
	
    if (m_messenger->State() == VideoEffectState::Locked
        || m_messenger->State() == VideoEffectState::Triggered)
    {
        if (m_frameRingBuffer[m_currentBufferIndex])
        {
            // Release the previous sample in the index
            delete[] m_frameRingBuffer[m_currentBufferIndex];
            m_frameRingBuffer[m_currentBufferIndex] = NULL;
        }

        m_frameRingBuffer[m_currentBufferIndex] = CopyFrameFromSample(pSample);

        m_currentBufferIndex >= BufferSize - 1 ? m_currentBufferIndex = 0 : m_currentBufferIndex++;
    }

done:
    //SafeRelease(&m_pSample);
    LeaveCriticalSection(&m_critSec);
    return hr;
}


//-------------------------------------------------------------------
// FrameSize()
//
// Returns the frame (data) size.
//-------------------------------------------------------------------
inline const size_t CBufferEffect::FrameSize() const
{
    return sizeof(BYTE) * FrameSizeAsUint32();
}


//-------------------------------------------------------------------
// FrameSizeAsUint32()
//
// Returns the frame (data) size.
//-------------------------------------------------------------------
inline const UINT32 CBufferEffect::FrameSizeAsUint32() const
{
    float coefficient = 1.0f;

    if (m_pTransformFn == ChromaFilterTransformImageNV12)
    {
        // NV12:
        // Y plane: width * height
        // UV plane: width * height / 2
        coefficient = 1.5f;
    }

    return (UINT32)(m_imageWidthInPixels * m_imageHeightInPixels * coefficient);
}


//-------------------------------------------------------------------
// ClearFrameRingBuffer
//
// Resets the frame ring buffer and counters.
//-------------------------------------------------------------------
void CBufferEffect::ClearFrameRingBuffer()
{
    for (UINT8 i = 0; i < BufferSize; ++i)
    {
        delete[] m_frameRingBuffer[i];
        m_frameRingBuffer[i] = NULL;
    }

    m_currentBufferIndex = 0;
}


//-------------------------------------------------------------------
// CalculateBufferIndex
//
//
//-------------------------------------------------------------------
inline UINT8 CBufferEffect::CalculateBufferIndex(const UINT8 &originalIndex, const int &delta)
{
    assert(abs(delta) < BufferSize);
    int index = (int)originalIndex + delta;

    if (index < 0)
    {
        index = BufferSize + index;
    }
    else if (index > BufferSize - 1)
    {
        index = index - BufferSize;
    }

    return (UINT8)index;
}


//-------------------------------------------------------------------
// PreviousBufferIndex
//
// Returns the previous buffer index from the given index.
//-------------------------------------------------------------------
inline UINT8 CBufferEffect::PreviousBufferIndex(const UINT8 &bufferIndex)
{
    UINT8 previousIndex = bufferIndex - 1;

    if ((int)bufferIndex - 1 < 0)
    {
        previousIndex = BufferSize - 1;
    }

    return previousIndex;
}


//-------------------------------------------------------------------
// CopyFrameFromSample
//
// Copies the frame from the given sample.
// Returns a newly created frame or NULL in case of an error.
//-------------------------------------------------------------------
BYTE* CBufferEffect::CopyFrameFromSample(IMFSample *pSample)
{
    BYTE *frame = NULL;

    if (pSample)
    {
        IMFMediaBuffer *mediaBuffer = NULL;
        pSample->GetBufferByIndex(0, &mediaBuffer);

        BYTE *start = NULL;
        DWORD maxLength = 0;
        DWORD currentLength = 0;

        mediaBuffer->Lock(&start, &maxLength, &currentLength);

        size_t frameSize = FrameSize();
        frame = (BYTE*)malloc(frameSize);
        memcpy(frame, start, frameSize);

        mediaBuffer->Unlock();
        SafeRelease(&mediaBuffer);
    }

    return frame;
}


//-------------------------------------------------------------------
// GetFrameFromBuffer
//
// Inserts the frame to the given frame (ppFrame) and stride to
// lStride. Returns true, if successful, false otherwise.
//
// Note: You must remember to unlock the VideoBufferLock!
//-------------------------------------------------------------------
bool CBufferEffect::GetFrameFromSample(IMFSample *pSample, BYTE **ppFrame, LONG &lStride, VideoBufferLock **ppVideoBufferLock)
{
	bool success = false;

	if (pSample)
	{
		IMFMediaBuffer *mediaBuffer = NULL;
		pSample->GetBufferByIndex(0, &mediaBuffer);
		(*ppVideoBufferLock) = new VideoBufferLock(mediaBuffer);
		LONG lDefaultStride = 0;

		HRESULT hr = ImageProcessingUtils::getDefaultStride(m_pInputType, &lDefaultStride);

		if (SUCCEEDED(hr))
		{
			hr = (*ppVideoBufferLock)->LockBuffer(lDefaultStride, m_imageHeightInPixels, ppFrame, &lStride);

			if (SUCCEEDED(hr))
			{
				success = true;
			}
		}

        SafeRelease(&mediaBuffer);
	}

	return success;
}


//-------------------------------------------------------------------
// NotifyFrameCaptured
//
// 
//-------------------------------------------------------------------
void CBufferEffect::NotifyFrameCaptured(BYTE *pFrame, const UINT32 &frameSize, const int &frameId)
{
    if (pFrame)
    {
        Array<byte>^ array = ref new Array<byte>(pFrame, frameSize);
        m_messenger->NotifyFrameCaptured(array, (int)m_imageWidthInPixels, (int)m_imageHeightInPixels, frameId);
    }
}


//-------------------------------------------------------------------
// NotifyFrameCaptured
//
//
//-------------------------------------------------------------------
void CBufferEffect::NotifyFrameCaptured(const UINT8 &frameIndex, const int &frameId)
{
    if (m_frameRingBuffer[frameIndex])
	{
        NotifyFrameCaptured(m_frameRingBuffer[frameIndex], FrameSizeAsUint32(), frameId);
    }
}


//-------------------------------------------------------------------
// NotifyFrameCaptured
//
// Note: This method supports only NV12 format!
//-------------------------------------------------------------------
void CBufferEffect::NotifyFrameCaptured(IMFSample *pSample, const int &frameId)
{
    IMFMediaBuffer *mediaBuffer = NULL;
    pSample->GetBufferByIndex(0, &mediaBuffer);
    VideoBufferLock *videoBufferLock = new VideoBufferLock(mediaBuffer);
    LONG lDefaultStride = 0;

    HRESULT hr = ImageProcessingUtils::getDefaultStride(m_pInputType, &lDefaultStride);

    if (SUCCEEDED(hr))
    {
        BYTE *frame = NULL;
        LONG stride = 0;
        hr = videoBufferLock->LockBuffer(lDefaultStride, m_imageHeightInPixels, &frame, &stride);

        if (SUCCEEDED(hr))
        {
            NotifyFrameCaptured(frame, FrameSizeAsUint32(), frameId);
            videoBufferLock->UnlockBuffer();
        }
    }

    SafeRelease(&mediaBuffer);
    delete videoBufferLock;
}


//-------------------------------------------------------------------
// NotifyPostProcessComplete
//
//
//-------------------------------------------------------------------
void CBufferEffect::NotifyPostProcessComplete(
	BYTE *pFrame, const UINT32 &frameSize,
	const ObjectDetails &fromObjectDetails, const ObjectDetails &toObjectDetails)
{
	if (pFrame)
	{
		Array<byte>^ array = ref new Array<byte>(pFrame, frameSize);

		m_messenger->NotifyPostProcessComplete(
			array, (int)m_imageWidthInPixels, (int)m_imageHeightInPixels,
			(int)fromObjectDetails._centerX, (int)fromObjectDetails._centerY, (int)fromObjectDetails._width, (int)fromObjectDetails._height,
			(int)toObjectDetails._centerX, (int)toObjectDetails._centerY, (int)toObjectDetails._width, (int)toObjectDetails._height);
	}
}


//-------------------------------------------------------------------
// SaveBuffers
//
//-------------------------------------------------------------------
void CBufferEffect::SaveBuffers()
{
	srand((UINT32)time(0));
	int r = rand();
	int frameCounter = 0;

	for (UINT32 i = m_currentBufferIndex + 1; i < BufferSize; ++i)
	{
		SaveBuffer(i, frameCounter, r);
		frameCounter++;
	}

	for (UINT32 i = 0; i < m_currentBufferIndex; ++i)
	{
		SaveBuffer(i, frameCounter, r);
		frameCounter++;
	}
}


//-------------------------------------------------------------------
// SaveBuffer
//
//
//-------------------------------------------------------------------
void CBufferEffect::SaveBuffer(const UINT8 &frameIndex, const int &frameCounter, const int &seriesIdentifier)
{
    if (m_frameRingBuffer[frameIndex])
	{
        Array<byte>^ array = ref new Array<byte>(m_frameRingBuffer[frameIndex], FrameSizeAsUint32());
        m_messenger->SaveFrame(array, m_imageWidthInPixels, m_imageHeightInPixels, frameCounter, seriesIdentifier);
	}
}


//-------------------------------------------------------------------
// GetObjectFromFrame
//
//
//-------------------------------------------------------------------
ObjectDetails* CBufferEffect::GetObjectFromFrame(const UINT8 &frameIndex)
{
	ObjectDetails *objectDetails = NULL;
	BYTE *frame = m_frameRingBuffer[frameIndex];

	if (m_pTransformFn && frame)
	{
        LONG stride = m_imageWidthInPixels;
        m_rcDest = D2D1::RectU(0, 0, m_imageWidthInPixels, m_imageHeightInPixels);

		objectDetails = new ObjectDetails();
        size_t frameSize = FrameSize();
		BYTE *processedFrame = (BYTE*)malloc(frameSize);
        memcpy(processedFrame, frame, frameSize);

		(*m_pTransformFn)(*objectDetails,
			m_settings->m_targetYUV, static_cast<BYTE>(m_settings->m_threshold), false,
			m_rcDest, processedFrame, stride, frame, stride,
			m_imageWidthInPixels, m_imageHeightInPixels);

		ConvexHull *convexHull =
			m_imageAnalyzer->bestConvexHullDetails(processedFrame, m_imageWidthInPixels, m_imageHeightInPixels, 3, m_pTransformFn);

		if (convexHull)
		{
			delete objectDetails;
			objectDetails = m_imageAnalyzer->convexHullDimensionsAsObjectDetails(*convexHull);
		}

		delete[] processedFrame;
	}

	return objectDetails;
}


//-------------------------------------------------------------------
// GetFirstFrameWithObject
//
// The index of the frame where the object was found is inserted to
// frameIndexWithObject and the corresponding object details to
// objectDetails.
//
// Returns true, if found, false otherwise.
//-------------------------------------------------------------------
bool CBufferEffect::GetFirstFrameWithObject(
	const UINT8 &fromFrameIndex, const UINT8 &toFrameIndex, const bool &seekForward,
    UINT8 &frameIndexWithObject, ObjectDetails &objectDetails)
{
    assert(fromFrameIndex < BufferSize && toFrameIndex < BufferSize);

	bool found = false;
	ObjectDetails *currentObjectDetails = NULL;
    int index = (int)fromFrameIndex;
    const UINT32 objectMinWidth = (UINT32)((float)m_imageWidthInPixels * RelativeObjectSizeThreshold);

    do
	{
		currentObjectDetails = GetObjectFromFrame(index);

		if (currentObjectDetails && currentObjectDetails->_width >= objectMinWidth)
		{
			objectDetails = *currentObjectDetails;
			frameIndexWithObject = (UINT8)index;
			found = true;
			delete currentObjectDetails;
			break;
		}

		delete currentObjectDetails;

        if (index == toFrameIndex)
        {
            break;
        }

        index += seekForward ? 1 : -1;

        if (index > BufferSize)
        {
            index = 0;
        }
        else if (index < 0)
        {
            index = BufferSize - 1;
        }
    } while (true);

	return found;
}
