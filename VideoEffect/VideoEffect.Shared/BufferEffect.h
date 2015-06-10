#ifndef BUFFEREFFECT_H
#define BUFFEREFFECT_H

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

#include "BufferEffect_h.h"

#include <INITGUID.H>

#include "AbstractEffect.h"
#include "Common.h"
#include "MessengerInterface.h"

class VideoBufferLock;

// CLSID of the MFT.
DEFINE_GUID(CLSID_GrayscaleMFT,
0x2f3dbc05, 0xc011, 0x4a8f, 0xb2, 0x64, 0xe4, 0x2e, 0x35, 0xc6, 0x7b, 0xf4);

//
// * IMPORTANT: If you implement your own MFT, create a new GUID for the CLSID. *
//


// Configuration attributes

// {7BBBB051-133B-41F5-B6AA-5AFF9B33A2CB}
DEFINE_GUID(MFT_GRAYSCALE_DESTINATION_RECT, 
0x7bbbb051, 0x133b, 0x41f5, 0xb6, 0xaa, 0x5a, 0xff, 0x9b, 0x33, 0xa2, 0xcb);


// {14782342-93E8-4565-872C-D9A2973D5CBF}
DEFINE_GUID(MFT_GRAYSCALE_SATURATION, 
0x14782342, 0x93e8, 0x4565, 0x87, 0x2c, 0xd9, 0xa2, 0x97, 0x3d, 0x5c, 0xbf);

// {E0BADE5D-E4B9-4689-9DBA-E2F00D9CED0E}
DEFINE_GUID(MFT_GRAYSCALE_CHROMA_ROTATION, 
0xe0bade5d, 0xe4b9, 0x4689, 0x9d, 0xba, 0xe2, 0xf0, 0xd, 0x9c, 0xed, 0xe);


// Constants
const UINT8 BufferSize = 20;


class CBufferEffect WrlSealed : public CAbstractEffect

{
	InspectableClass(RuntimeClass_BufferEffect_BufferEffect, BaseTrust)

public:
    CBufferEffect();
    ~CBufferEffect();

public: // From CAbstractEffect
    STDMETHOD(RuntimeClassInitialize)();
	STDMETHODIMP SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *pConfiguration);
    STDMETHODIMP ProcessInput(DWORD dwInputStreamID, IMFSample *pSample, DWORD dwFlags);

#if (WINAPI_FAMILY == WINAPI_FAMILY_PC_APP)
	STDMETHODIMP ProcessOutput(DWORD dwFlags, DWORD cOutputBufferCount, MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, DWORD *pdwStatus);
#endif // WINAPI_FAMILY_PC_APP

protected: // From CAbstractEffect
    HRESULT OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut);

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
    VideoEffect::MessengerInterface ^m_messenger;
    std::vector<BYTE*> m_frameRingBuffer = std::vector<BYTE*>(BufferSize);
    UINT8 m_currentBufferIndex;
    UINT8 m_numberOfFramesBufferedAfterTriggered;
	int m_bufferIndexWhenTriggered;
};
#endif