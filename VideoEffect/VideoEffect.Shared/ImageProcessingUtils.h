#ifndef IMAGEPROCESSINGUTILS_H
#define IMAGEPROCESSINGUTILS_H

#include <d2d1.h>
#include "Common.h"

// Forward declarations
class ImageProcessingUtils;
class ObjectDetails;


void ChromaFilterTransformImageYUY2(
	ObjectDetails& objectDetails,
	const float* pTargetYUV,
	const BYTE& threshold,
    const bool& dimmFilteredPixels,
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels);

/*void TransformImage_UYVY(
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels);*/

void ChromaFilterTransformImageNV12(
	ObjectDetails& objectDetails,
	const float* pTargetYUV,
	const BYTE& threshold,
    const bool& dimmFilteredPixels,
	const D2D_RECT_U& rcDest,
	_Inout_updates_(_Inexpressible_(2 * lDestStride * dwHeightInPixels)) BYTE* pDest,
	_In_ LONG lDestStride,
	_In_reads_(_Inexpressible_(2 * lSrcStride * dwHeightInPixels)) const BYTE* pSrc,
	_In_ LONG lSrcStride,
	_In_ DWORD dwWidthInPixels,
	_In_ DWORD dwHeightInPixels);


class ImageProcessingUtils
{
public:
	ImageProcessingUtils();
    ~ImageProcessingUtils();

public: // Static methods
    static HRESULT getDefaultStride(IMFMediaType* mediaType, LONG* stride);
	static HRESULT getImageSize(DWORD fcc, UINT32 width, UINT32 height, DWORD* cbImage);

	static BYTE* mergeFramesNV12(
		BYTE* leftFrame, BYTE* rightFrame,
		const UINT32& frameWidth, const UINT32& frameHeight, UINT32& joinX);

	static BYTE* mergeFramesYUY2(
		BYTE* leftFrame, BYTE* rightFrame,
		const UINT32& frameWidth, const UINT32& frameHeight, UINT32& joinX);
	
    static BYTE* copyFrame(BYTE* source, const size_t size);

public:
	bool validateRect(const RECT& rc);
	bool validateRect(const D2D_RECT_U& rect, const UINT32& totalWidth, const UINT32& totalHeight);

    void drawLine(
		BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight,
		const D2D1_POINT_2U &point1, D2D1_POINT_2U &point2,
		const IMAGE_TRANSFORM_FUNCTION &pTransformFn,
		const UINT32& thickness = 1,
		const BYTE& yy = 0xff, const BYTE& u = 0x80, const BYTE& v = 0x80);

	void visualizeObjectNV12(
		BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight,
		UINT16* objectMap, const UINT16& objectId);

    BYTE* cropImage(BYTE* image, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U &cropRect);

    UINT16* createObjectMap(BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const IMAGE_TRANSFORM_FUNCTION &pTransformFn);
    UINT16 organizeObjectMap(UINT16* objectMap, const UINT32& objectMapSize);

    std::vector<D2D_POINT_2U>* extractSortedObjectPoints(
        UINT16* objectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, UINT32 objectId);

    ConvexHull* createConvexHull(std::vector<D2D_POINT_2U>& points, bool alreadySorted);

private:
    double cross(const D2D_POINT_2U& O, const D2D_POINT_2U& A, const D2D_POINT_2U& B);
    void sortPointVector(std::vector<D2D_POINT_2U>& pointVector);

    UINT16 objectIdOfPixelAbove(
        BYTE* binaryImage, UINT16* objectMap, const UINT32& currentIndex, const UINT32& imageWidth,
        bool fromLeftToRight = true);

    bool calculateLineEquation(const D2D_POINT_2U& point1, const D2D_POINT_2U& point2, double* a, int* b);
	D2D_RECT_U lineBoundingBox(const D2D_POINT_2U& point1, const D2D_POINT_2U& point2, bool leftAndRightHaveToHaveEvenValues = false);
    bool idExists(const std::vector<UINT16>& ids, const UINT16& id);
};

#endif
