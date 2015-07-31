#ifndef ABSTRACTTRANSFORM_H
#define ABSTRACTTRANSFORM_H

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

#include <INITGUID.H>

#include "Common.h"
#include "Interop\MessengerInterface.h"


// Forward declaration
class ImageAnalyzer;
class ImageProcessingUtils;
class Settings;


class CAbstractTransform
    : public Microsoft::WRL::RuntimeClass<
        Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::RuntimeClassType::WinRtClassicComMix>,
        ABI::Windows::Media::IMediaExtension,
        IMFTransform>
{
public:
    CAbstractTransform();

    virtual ~CAbstractTransform();

    STDMETHOD(RuntimeClassInitialize)();

    // IMediaExtension
    virtual STDMETHODIMP SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *pConfiguration);

    // IMFTransform
    STDMETHODIMP GetStreamLimits(
        DWORD   *pdwInputMinimum,
        DWORD   *pdwInputMaximum,
        DWORD   *pdwOutputMinimum,
        DWORD   *pdwOutputMaximum
        );

    STDMETHODIMP GetStreamCount(
        DWORD   *pcInputStreams,
        DWORD   *pcOutputStreams
        );

    STDMETHODIMP GetStreamIDs(
        DWORD   dwInputIDArraySize,
        DWORD   *pdwInputIDs,
        DWORD   dwOutputIDArraySize,
        DWORD   *pdwOutputIDs
        );

    STDMETHODIMP GetInputStreamInfo(
        DWORD                     dwInputStreamID,
        MFT_INPUT_STREAM_INFO *   pStreamInfo
        );

    STDMETHODIMP GetOutputStreamInfo(
        DWORD                     dwOutputStreamID,
        MFT_OUTPUT_STREAM_INFO *  pStreamInfo
        );

    STDMETHODIMP GetAttributes(IMFAttributes** pAttributes);

    STDMETHODIMP GetInputStreamAttributes(
        DWORD           dwInputStreamID,
        IMFAttributes   **ppAttributes
        );

    STDMETHODIMP GetOutputStreamAttributes(
        DWORD           dwOutputStreamID,
        IMFAttributes   **ppAttributes
        );

    STDMETHODIMP DeleteInputStream(DWORD dwStreamID);

    STDMETHODIMP AddInputStreams(
        DWORD   cStreams,
        DWORD   *adwStreamIDs
        );

    STDMETHODIMP GetInputAvailableType(
        DWORD           dwInputStreamID,
        DWORD           dwTypeIndex, // 0-based
        IMFMediaType    **ppType
        );

    STDMETHODIMP GetOutputAvailableType(
        DWORD           dwOutputStreamID,
        DWORD           dwTypeIndex, // 0-based
        IMFMediaType    **ppType
        );

    STDMETHODIMP SetInputType(
        DWORD           dwInputStreamID,
        IMFMediaType    *pType,
        DWORD           dwFlags
        );

    STDMETHODIMP SetOutputType(
        DWORD           dwOutputStreamID,
        IMFMediaType    *pType,
        DWORD           dwFlags
        );

    STDMETHODIMP GetInputCurrentType(
        DWORD           dwInputStreamID,
        IMFMediaType    **ppType
        );

    STDMETHODIMP GetOutputCurrentType(
        DWORD           dwOutputStreamID,
        IMFMediaType    **ppType
        );

    STDMETHODIMP GetInputStatus(
        DWORD           dwInputStreamID,
        DWORD           *pdwFlags
        );

    STDMETHODIMP GetOutputStatus(DWORD *pdwFlags);

    STDMETHODIMP SetOutputBounds(
        LONGLONG        hnsLowerBound,
        LONGLONG        hnsUpperBound
        );

    STDMETHODIMP ProcessEvent(
        DWORD              dwInputStreamID,
        IMFMediaEvent      *pEvent
        );

    STDMETHODIMP ProcessMessage(
        MFT_MESSAGE_TYPE    eMessage,
        ULONG_PTR           ulParam
        );

    virtual STDMETHODIMP ProcessInput(
        DWORD               dwInputStreamID,
        IMFSample           *pSample,
        DWORD               dwFlags
        );

    virtual STDMETHODIMP ProcessOutput(
        DWORD                   dwFlags,
        DWORD                   cOutputBufferCount,
        MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, // one per stream
        DWORD                   *pdwStatus
        );


protected:
    // HasPendingOutput: Returns TRUE if the MFT is holding an input sample.
    BOOL HasPendingOutput() const { return m_pSample != NULL; }

    // IsValidInputStream: Returns TRUE if dwInputStreamID is a valid input stream identifier.
    BOOL IsValidInputStream(DWORD dwInputStreamID) const
    {
        return dwInputStreamID == 0;
    }

    // IsValidOutputStream: Returns TRUE if dwOutputStreamID is a valid output stream identifier.
    BOOL IsValidOutputStream(DWORD dwOutputStreamID) const
    {
        return dwOutputStreamID == 0;
    }

    HRESULT OnGetPartialType(DWORD dwTypeIndex, IMFMediaType **ppmt);
    HRESULT OnCheckInputType(IMFMediaType *pmt);
    HRESULT OnCheckOutputType(IMFMediaType *pmt);
    HRESULT OnCheckMediaType(IMFMediaType *pmt);
    void    OnSetInputType(IMFMediaType *pmt);
    void    OnSetOutputType(IMFMediaType *pmt);
    
    HRESULT OnFlush();

    virtual HRESULT UpdateFormatInfo();
    virtual HRESULT BeginStreaming();
    virtual HRESULT EndStreaming();
    virtual HRESULT OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut) = 0;

    virtual void OnMfMtSubtypeResolved(const GUID subtype);


protected: // Members
    CRITICAL_SECTION            m_critSec;

    D2D_RECT_U                  m_rcDest;                   // Destination rectangle for the effect.

    // Streaming
    bool                        m_bStreamingInitialized;
    IMFSample                   *m_pSample;                            // Input sample.
    IMFMediaType                *m_pInputType;                        // Input media type.
    IMFMediaType                *m_pOutputType;                        // Output media type.

    // Fomat information
    UINT32                      m_imageWidthInPixels;
    UINT32                      m_imageHeightInPixels;
    DWORD                       m_cbImageSize;              // Image size, in bytes.

    IMFAttributes               *m_pAttributes;

    VideoEffect::MessengerInterface ^m_messenger;
    ImageAnalyzer *m_imageAnalyzer;
    ImageProcessingUtils *m_imageProcessingUtils;
    Settings *m_settings;
    GUID m_videoFormatSubtype;
};

#endif // ABSTRACTTRANSFORM_H
