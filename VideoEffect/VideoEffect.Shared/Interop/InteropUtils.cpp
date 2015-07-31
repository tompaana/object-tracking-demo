#include "pch.h"

#include "InteropUtils.h"

using namespace Microsoft::WRL;

//-------------------------------------------------------------------
// GetFloatProperty
// 
//-------------------------------------------------------------------
float CInteropUtils::GetFloatProperty(
    ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> &spSetting,
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
