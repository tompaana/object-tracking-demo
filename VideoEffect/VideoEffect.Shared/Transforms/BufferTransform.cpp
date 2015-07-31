#include "pch.h"

#include "BufferTransform.h"
#include "BufferLock.h"
#include "Effects\ChromaFilterEffect.h"
#include "ImageProcessing\ImageAnalyzer.h"
#include "ImageProcessing\ImageProcessingUtils.h"
#include "ImageProcessing\ObjectDetails.h"
#include "Settings.h"

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

#pragma comment(lib, "d2d1")

using namespace Microsoft::WRL;
using namespace Platform::Collections;
using namespace Windows::Storage::Streams;
using namespace Windows::Foundation::Collections;
using namespace Platform;

ActivatableClass(CBufferTransform);

typedef void(*UnmanagedEventFromAppHandler)(String^ msg);


CBufferTransform::CBufferTransform() :
    CAbstractTransform(),
    m_effect(NULL),
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

CBufferTransform::~CBufferTransform()
{
    SafeRelease(&m_pInputType);
    SafeRelease(&m_pOutputType);
    SafeRelease(&m_pSample);
    SafeRelease(&m_pAttributes);

    ClearFrameRingBuffer();

    DeleteCriticalSection(&m_critSec);

    delete m_effect;
}


// Initialize the instance.
STDMETHODIMP CBufferTransform::RuntimeClassInitialize()
{
    // Create the attribute store.
    return MFCreateAttributes(&m_pAttributes, 3);
}


#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)

//-------------------------------------------------------------------
// ProcessOutput
// Process an output sample.
//-------------------------------------------------------------------

HRESULT CBufferTransform::ProcessOutput(
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
    SafeRelease(&m_pSample);   // Release our input sample.
    SafeRelease(&pInput);
    SafeRelease(&pOutput);
    LeaveCriticalSection(&m_critSec);
    return hr;
}

#endif // WINAPI_FAMILY_PC_APP


// Generate output data.

HRESULT CBufferTransform::OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut)
{
#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
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

    // Just pass the image through as-is
    assert(m_effect != NULL);
    ImageProcessingUtils::copyFrame(pSrc, pDest,
        ImageProcessingUtils::frameSize(m_imageWidthInPixels, m_imageHeightInPixels, m_effect->videoFormatSubtype()));

    // Set the data size on the output buffer.
    hr = pOut->SetCurrentLength(m_cbImageSize);

    // The VideoBufferLock class automatically unlocks the buffers.
done:
    return hr;
#else // WINAPI_FAMILY_PC_APP
    return -1;
#endif // WINAPI_FAMILY_PC_APP
}


//-------------------------------------------------------------------
// OnMfMtSubtypeResolved
//
// Sets the effect based on the given subtype.
//-------------------------------------------------------------------
void CBufferTransform::OnMfMtSubtypeResolved(const GUID subtype)
{
    CAbstractTransform::OnMfMtSubtypeResolved(subtype);
    m_effect = new ChromaFilterEffect(subtype);
}


//-------------------------------------------------------------------
// ProcessInput
// Process an input sample.
//-------------------------------------------------------------------

HRESULT CBufferTransform::ProcessInput(
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

    bool winApiIsFamilyPcApp = false;
#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
    winApiIsFamilyPcApp = true;
#endif

    if (!m_messenger || (winApiIsFamilyPcApp && m_messenger->State() == VideoEffectState::Idle))
    {
        // Initialize streaming
        hr = BeginStreaming();

        if (FAILED(hr))
        {
            goto done;
        }

        // Check if an input sample is already queued
        if (m_pSample != NULL)
        {
            hr = MF_E_NOTACCEPTING; // We already have an input sample
            goto done;
        }

        m_pSample = pSample;
        pSample->AddRef(); // Hold a reference count on the sample

        if (!m_messenger)
        {
            goto done;
        }
    }

    int frameRequestId = m_messenger->IsFrameRequested();

    if (frameRequestId > 0)
    {
        /*UINT8 previousFrameBufferIndex = PreviousBufferIndex(m_currentBufferIndex);

        if (m_frameRingBuffer[previousFrameBufferIndex])
        {
            // Provide the previous frame in the buffer
            NotifyFrameCaptured(previousFrameBufferIndex, frameRequestId);
        }
        else
        {*/
            // Provide the current sample
            NotifyFrameCaptured(pSample, frameRequestId);
        //}
    }

    if (m_messenger->State() == VideoEffectState::Triggered)
    {
        m_numberOfFramesBufferedAfterTriggered++;

        if (m_numberOfFramesBufferedAfterTriggered > BufferSize - 3)
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

        UINT8 previousBufferIndexFromTriggeredFrame = PreviousBufferIndex(m_bufferIndexWhenTriggered);
        UINT8 toIndexFromLastFrame = CalculateBufferIndex(m_currentBufferIndex, -((int)BufferSize) + 3);

        if (GetFirstFrameWithObject(
            m_bufferIndexWhenTriggered, previousBufferIndexFromTriggeredFrame, false,
                closestBufferIndexToTriggeredWithObject, objectDetailsCloseToTriggeredFrame)
            && GetFirstFrameWithObject(
                m_currentBufferIndex, toIndexFromLastFrame, false,
                lastBufferIndexWithObject, objectDetailsFromLastFrame)
            && lastBufferIndexWithObject != closestBufferIndexToTriggeredWithObject)
        {
            LONG stride = 0;
            BYTE *lastFrame = m_frameRingBuffer[lastBufferIndexWithObject];
            BYTE *triggeredFrame = m_frameRingBuffer[closestBufferIndexToTriggeredWithObject];

            bool lastFrameIsLeft = objectDetailsFromLastFrame._centerX < objectDetailsCloseToTriggeredFrame._centerX;
            UINT32 joinX = (objectDetailsFromLastFrame._centerX + objectDetailsCloseToTriggeredFrame._centerX) / 2;

            BYTE *mergedFrame = NULL;

            if (m_effect->videoFormatSubtype() == MFVideoFormat_NV12)
            {
                mergedFrame = ImageProcessingUtils::mergeFramesNV12(
                              (lastFrameIsLeft ? lastFrame : triggeredFrame),
                              (lastFrameIsLeft ? triggeredFrame : lastFrame),
                              m_imageWidthInPixels, m_imageHeightInPixels, joinX);
            }
            else if (m_effect->videoFormatSubtype() == MFVideoFormat_YUY2)
            {
                mergedFrame = ImageProcessingUtils::mergeFramesYUY2(
                              (lastFrameIsLeft ? lastFrame : triggeredFrame),
                              (lastFrameIsLeft ? triggeredFrame : lastFrame),
                              m_imageWidthInPixels, m_imageHeightInPixels, joinX);
            }

            if (mergedFrame)
            {
                NotifyPostProcessComplete(mergedFrame, FrameSizeAsUint32(),
                                          objectDetailsCloseToTriggeredFrame, objectDetailsFromLastFrame);

                delete[] mergedFrame;
            }
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
inline const size_t CBufferTransform::FrameSize() const
{
    return sizeof(BYTE) * FrameSizeAsUint32();
}


//-------------------------------------------------------------------
// FrameSizeAsUint32()
//
// Returns the frame (data) size.
//-------------------------------------------------------------------
inline const UINT32 CBufferTransform::FrameSizeAsUint32() const
{
    float coefficient = 1.0f;

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

    return (UINT32)(m_imageWidthInPixels * m_imageHeightInPixels * coefficient);
}


//-------------------------------------------------------------------
// ClearFrameRingBuffer
//
// Resets the frame ring buffer and counters.
//-------------------------------------------------------------------
void CBufferTransform::ClearFrameRingBuffer()
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
inline UINT8 CBufferTransform::CalculateBufferIndex(const UINT8 &originalIndex, const int &delta)
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
inline UINT8 CBufferTransform::PreviousBufferIndex(const UINT8 &bufferIndex)
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
BYTE* CBufferTransform::CopyFrameFromSample(IMFSample *pSample)
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
bool CBufferTransform::GetFrameFromSample(IMFSample *pSample, BYTE **ppFrame, LONG &lStride, VideoBufferLock **ppVideoBufferLock)
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
void CBufferTransform::NotifyFrameCaptured(BYTE *pFrame, const UINT32 &frameSize, const int &frameId)
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
void CBufferTransform::NotifyFrameCaptured(const UINT8 &frameIndex, const int &frameId)
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
void CBufferTransform::NotifyFrameCaptured(IMFSample *pSample, const int &frameId)
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
void CBufferTransform::NotifyPostProcessComplete(
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
void CBufferTransform::SaveBuffers()
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
void CBufferTransform::SaveBuffer(const UINT8 &frameIndex, const int &frameCounter, const int &seriesIdentifier)
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
ObjectDetails* CBufferTransform::GetObjectFromFrame(const UINT8 &frameIndex)
{
    ObjectDetails *objectDetails = NULL;
    BYTE *frame = m_frameRingBuffer[frameIndex];

    if (m_effect && frame)
    {
        LONG stride = m_imageWidthInPixels;
        m_rcDest = D2D1::RectU(0, 0, m_imageWidthInPixels, m_imageHeightInPixels);
        size_t frameSize = FrameSize();
        BYTE *processedFrame = (BYTE*)malloc(frameSize);
        memcpy(processedFrame, frame, frameSize);

        dynamic_cast<ChromaFilterEffect*>(m_effect)->setDimmUnselectedPixels(false);
        m_effect->apply(
            m_rcDest, processedFrame, stride, frame, stride,
            m_imageWidthInPixels, m_imageHeightInPixels);

        objectDetails = new ObjectDetails();
        *objectDetails = dynamic_cast<ChromaFilterEffect*>(m_effect)->currentObject();

        ConvexHull *convexHull =
            m_imageAnalyzer->bestConvexHullDetails(
                processedFrame, m_imageWidthInPixels, m_imageHeightInPixels, 3, m_effect->videoFormatSubtype());

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
bool CBufferTransform::GetFirstFrameWithObject(
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
