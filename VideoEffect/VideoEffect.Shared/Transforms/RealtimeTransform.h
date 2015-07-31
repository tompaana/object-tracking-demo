#ifndef REALTIMEFINDERTRANSFORM_H
#define REALTIMEFINDERTRANSFORM_H

#include "AbstractTransform.h"
#include "Common.h"
#include "Interop\MessengerInterface.h"
#include "RealtimeTransform_h.h"

class AbstractEffect;

// CLSID of the MFT.
DEFINE_GUID(CLSID_RealtimeTransformMFT,
0x2f3dbc05, 0xc011, 0x4a8f, 0xb2, 0x64, 0xe4, 0x2e, 0x35, 0xc6, 0x7b, 0xf4);


// CRealtimeTransform class:

class CRealtimeTransform WrlSealed : public CAbstractTransform
{
    InspectableClass(RuntimeClass_RealtimeTransform_RealtimeTransform, BaseTrust)

public: // Construction and destruction
    CRealtimeTransform();
    virtual ~CRealtimeTransform();

public: // From CAbstratEffect
    STDMETHOD(RuntimeClassInitialize)();

    STDMETHODIMP ProcessOutput(
        DWORD                   dwFlags,
        DWORD                   cOutputBufferCount,
        MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, // one per stream
        DWORD                   *pdwStatus);

public: // New methods
    WORD OperationTimeAverage();

protected: // From CAbstratEffect
    HRESULT OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut);
    void OnMfMtSubtypeResolved(const GUID subtype);

private: // New methods
    void SetMode(const Mode& mode);
    void ClearTargetLock();
    void UpdateTargetLock(const ObjectDetails &objectDetails);
    void UpdateOperationTimeAverage(const UINT16 &millisecondsPerOperation);

private: // Members
    AbstractEffect *m_effect;

    D2D_RECT_U m_croppedProcessingArea;

    float m_itemX;
    float m_itemY;
    float m_itemWidth;
    float m_itemHeight;
    int m_targetLockIterations;
    bool m_targetLocked;
    bool m_targetWasJustLocked;

    LPSYSTEMTIME m_systemTime0;
    LPSYSTEMTIME m_systemTime1;
    WORD *m_operationTimeMeasurementsInMilliseconds;
    WORD m_operationTimeMeasurementAverageInMilliseconds;
    int m_operationTimeMeasurementCounter;
};

#endif // REALTIMEFINDERTRANSFORM_H
