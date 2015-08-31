#ifndef COMMON_H
#define COMMON_H

#include <mfapi.h>


// Forward declarations
class ObjectDetails;
struct D2D_RECT_U;
struct D2D_POINT_2U;


// Constants
const double Pi = 3.14159265359;
const float RelativeObjectSizeThreshold = 0.001f; // Size (area) relative to view, e.g. 0.05f -> 5 %


// Video FOURCC codes.
const DWORD FOURCC_YUY2 = '2YUY';
const DWORD FOURCC_UYVY = 'YVYU';
const DWORD FOURCC_NV12 = '21VN';


// Static array of media types (preferred and accepted).
const GUID g_MediaSubtypes[] =
{
    MFVideoFormat_NV12,
    MFVideoFormat_YUY2,
    MFVideoFormat_UYVY
};


template <typename T>
inline T clamp(const T& val, const T& minVal, const T& maxVal)
{
    return (val < minVal ? minVal : (val > maxVal ? maxVal : val));
}


template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}


typedef std::vector<D2D_POINT_2U> ConvexHull;


//-------------------------------------------------------------------
// VideoEffectState
//
// Idle: The default stage 
// Locking: Locking target
// Locked: Target has been locked, now we buffer everything and wait trigger
// Triggered
// PostProcess: Triggering has happened, this will read buffer and pass it forward
//-------------------------------------------------------------------
enum VideoEffectState
{
    Idle = 0,
    Locking = 1,
    Locked = 2,
    Triggered = 3,
    PostProcess = 4
};


//-------------------------------------------------------------------
// Mode
//
// Note: These has to be in sync with the managed side!
//-------------------------------------------------------------------
enum Mode
{
    Passthrough = 0,
    ChromaFilter = 1,
    EdgeDetection = 2,
    ChromaDelta = 3
};


//------------------------------------------------------------------
// DeletePointerVector
//
// Deletes the given vector and its content.
//------------------------------------------------------------------
template<class T>
void DeletePointerVector(std::vector<T*> **pPointerVector)
{
    if (pPointerVector && *pPointerVector)
    {
        int size = (*pPointerVector)->size();

        while (size > 0)
        {
            delete (*pPointerVector)->at(size - 1);
            (*pPointerVector)->pop_back();
            size = (*pPointerVector)->size();
        }

        delete *pPointerVector;
        *pPointerVector = NULL;
    }
}


//------------------------------------------------------------------
// DeletePointerVector
//
// Deletes the content of the given vector.
//------------------------------------------------------------------
template<class T>
void DeletePointerVector(std::vector<T*> &pointerVector)
{
    int size = pointerVector.size();

    while (size > 0)
    {
        delete pointerVector.at(size - 1);
        pointerVector.pop_back();
        size = pointerVector.size();
    }
}


//------------------------------------------------------------------
// DeleteArrayVector
//
// Deletes the content of the given vector.
//------------------------------------------------------------------
template<class T>
void DeleteArrayVector(std::vector<T*> &pointerVector)
{
    int size = pointerVector.size();

    while (size > 0)
    {
        delete[] pointerVector.at(size - 1);
        pointerVector.pop_back();
        size = pointerVector.size();
    }
}


#endif // COMMON_H
