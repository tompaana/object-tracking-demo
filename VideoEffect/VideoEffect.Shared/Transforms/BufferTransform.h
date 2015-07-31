#ifndef BUFFERTRANSFORM_H
#define BUFFERTRANSFORM_H

#include <new>
#include <mfapi.h>
#include <mftransform.h>
#include <mfidl.h>
#include <mferror.h>
#include <strsafe.h>
#include <assert.h>

// Note: The Direct2D helper library is included for its 2D matrix operations.
#include <D2d1helper.h>

#include <wrl\implements.h>
#include <wrl\module.h>
#include <windows.media.h>

#include "BufferTransform_h.h"

#include <INITGUID.H>

#include "AbstractTransform.h"
#include "Common.h"
#include "Interop\MessengerInterface.h"

class AbstractEffect;
class VideoBufferLock;

/*
 * CLSID of the MFT.
 * IMPORTANT: If you implement your own MFT, create a new GUID for the CLSID.
 */
DEFINE_GUID(CLSID_BufferTransformMFT,
0x2f3dbc05, 0xc011, 0x4a8f, 0xb2, 0x64, 0xe4, 0x2e, 0x35, 0xc6, 0x7b, 0xf4);


// Constants
const UINT8 BufferSize = 35;


class CBufferTransform WrlSealed : public CAbstractTransform

{
    InspectableClass(RuntimeClass_BufferTransform_BufferTransform, BaseTrust)

public:
    CBufferTransform();
    ~CBufferTransform();

public: // From CAbstractTransform
    STDMETHOD(RuntimeClassInitialize)();
    STDMETHODIMP ProcessInput(DWORD dwInputStreamID, IMFSample *pSample, DWORD dwFlags);

#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
    STDMETHODIMP ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, DWORD *pdwStatus);
#endif // WINAPI_FAMILY_PC_APP

protected: // From CAbstractTransform
    HRESULT OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut);
    void OnMfMtSubtypeResolved(const GUID subtype);

private: // New methods
    const size_t FrameSize() const;
    const UINT32 FrameSizeAsUint32() const;

    void ClearFrameRingBuffer();
    UINT8 CalculateBufferIndex(const UINT8 &originalIndex, const int &delta);
    UINT8 PreviousBufferIndex(const UINT8 &bufferIndex);
    BYTE* CopyFrameFromSample(IMFSample *sample);
    bool GetFrameFromSample(IMFSample *pSample, BYTE **ppFrame, LONG &lStride, VideoBufferLock **ppVideoBufferLock);

    void NotifyFrameCaptured(BYTE *pFrame, const UINT32 &frameSize, const int &frameId);
    void NotifyFrameCaptured(IMFSample *pSample, const int &frameId);
    void NotifyFrameCaptured(const UINT8 &frameIndex, const int &frameId);
    
    void NotifyPostProcessComplete(
        BYTE *pFrame, const UINT32 &frameSize,
        const ObjectDetails &fromObjectDetails, const ObjectDetails &toObjectDetails);

    void SaveBuffers();
    void SaveBuffer(const UINT8 &frameIndex, const int &frameCounter, const int &seriesIdentifier);
    ObjectDetails *GetObjectFromFrame(const UINT8 &frameIndex);

    bool GetFirstFrameWithObject(
        const UINT8 &fromFrameIndex, const UINT8 &toFrameIndex, const bool &seekForward,
        UINT8 &frameIndexWithObject, ObjectDetails &objectDetails);

private: // Members
    AbstractEffect *m_effect;
    std::vector<BYTE*> m_frameRingBuffer = std::vector<BYTE*>(BufferSize);
    UINT8 m_currentBufferIndex;
    UINT8 m_numberOfFramesBufferedAfterTriggered;
    int m_bufferIndexWhenTriggered;
};
#endif