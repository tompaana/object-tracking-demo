#ifndef CHROMAKEYITEMFINDEREFFECT_H
#define CHROMAKEYITEMFINDEREFFECT_H

#include "AbstractEffect.h"
#include "Common.h"
#include "MessengerInterface.h"
#include "ObjectFinderEffectTransform_h.h"


// CLSID of the MFT.
DEFINE_GUID(CLSID_ObjectFinderEffectMFT,
0x2f3dbc05, 0xc011, 0x4a8f, 0xb2, 0x64, 0xe4, 0x2e, 0x35, 0xc6, 0x7b, 0xf4);


// Configuration attributes

// {7BBBB051-133B-41F5-B6AA-5AFF9B33A2CB}
DEFINE_GUID(MFT_CHROMAKEYITEMFINDEREFFECT_DESTINATION_RECT, 
0x7bbbb051, 0x133b, 0x41f5, 0xb6, 0xaa, 0x5a, 0xff, 0x9b, 0x33, 0xa2, 0xcb);


// {14782342-93E8-4565-872C-D9A2973D5CBF}
DEFINE_GUID(MFT_CHROMAKEYITEMFINDEREFFECT_SATURATION, 
0x14782342, 0x93e8, 0x4565, 0x87, 0x2c, 0xd9, 0xa2, 0x97, 0x3d, 0x5c, 0xbf);

// {E0BADE5D-E4B9-4689-9DBA-E2F00D9CED0E}
DEFINE_GUID(MFT_CHROMAKEYITEMFINDEREFFECT_CHROMA_ROTATION, 
0xe0bade5d, 0xe4b9, 0x4689, 0x9d, 0xba, 0xe2, 0xf0, 0xd, 0x9c, 0xed, 0xe);


// CObjectFinderEffect class:

class CObjectFinderEffect WrlSealed : public CAbstractEffect
{
	InspectableClass(RuntimeClass_ObjectFinderEffectTransform_ObjectFinderEffectEffect, BaseTrust)

public: // Construction and destruction
    CObjectFinderEffect();
    virtual ~CObjectFinderEffect();


public: // From CAbstratEffect
    STDMETHOD(RuntimeClassInitialize)();
    STDMETHODIMP SetProperties(ABI::Windows::Foundation::Collections::IPropertySet *pConfiguration);

	STDMETHODIMP ProcessOutput(
		DWORD                   dwFlags,
		DWORD                   cOutputBufferCount,
		MFT_OUTPUT_DATA_BUFFER  *pOutputSamples, // one per stream
		DWORD                   *pdwStatus);


public: // New methods
	WORD OperationTimeAverage();

protected: // From CAbstratEffect
    HRESULT OnProcessOutput(IMFMediaBuffer *pIn, IMFMediaBuffer *pOut);

private: // New methods
	float GetFloatProperty(
		Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> &spSetting,
		const HSTRING &key, boolean &found);

    void ClearTargetLock();
    void UpdateTargetLock(const ObjectDetails &objectDetails);
    void UpdateOperationTimeAverage(const UINT16 &millisecondsPerOperation);


private: // Members
	// Transformation parameters
	//D2D1::Matrix3x2F            m_transform;                // Chroma transform matrix.

	VideoEffect::MessengerInterface ^m_messenger;

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

#endif // CHROMAKEYITEMFINDEREFFECT_H
