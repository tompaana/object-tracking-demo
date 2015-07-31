#ifndef INTEROPUTILS_H
#define INTEROPUTILS_H

#include <wrl\client.h>
#include <windows.foundation.h>

class CInteropUtils
{
public:
    static float GetFloatProperty(
        Microsoft::WRL::ComPtr<ABI::Windows::Foundation::Collections::IMap<HSTRING, IInspectable *>> &spSetting,
        const HSTRING &key, boolean &found);
};

#endif // INTEROPUTILS_H
