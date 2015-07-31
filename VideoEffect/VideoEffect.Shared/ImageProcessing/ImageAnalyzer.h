#ifndef IMAGEANALYZER_H
#define IMAGEANALYZER_H

#include <D2d1helper.h>
#include <mfapi.h>
#include "Common.h"

// Forward declarations
class ImageProcessingUtils;
class ObjectDetails;


class ImageAnalyzer
{
public:
    ImageAnalyzer(ImageProcessingUtils* imageProcessingUtils);
    ~ImageAnalyzer();

public:
    std::vector<ObjectDetails*> extractObjectDetails(
        const UINT16* organizedObjectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, const UINT16& objectCount);

    std::vector<UINT16>* resolveLargeObjectIds(
        const UINT16* organizedObjectMap,
        const UINT32& objectMapWidth, const UINT32& objectMapHeight,
        UINT16& objectCount, const UINT32& minSize);

    std::vector<ConvexHull*> extractConvexHullsOfLargestObjects(
        BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight,
        const UINT8& maxNumberOfConvexHulls, const GUID& videoFormatSubtype);

    ConvexHull* convexHullClosestToCircle(const std::vector<ConvexHull*> &convexHulls, LONG &error);

    void calculateConvexHullDimensions(
        const ConvexHull& convexHull,
        UINT32& width, UINT32& height, UINT32& centerX, UINT32& centerY);

    ObjectDetails* convexHullDimensionsAsObjectDetails(const ConvexHull& convexHull);

    LONG circleCircumferenceError(
        const ConvexHull& convexHull, const UINT32& convexHullWidth, const UINT32& convexHullHeight,
        const D2D_POINT_2U& convexHullCenter);

    double circleAreaError(UINT32 measuredDiameter, UINT32 measuredArea);

    ConvexHull* bestConvexHullDetails(
        BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight,
        const UINT8& maxCandidates, const GUID& videoFormatSubtype);

    bool objectIsWithinConvexHullBounds(const ObjectDetails& objectDetails, const ConvexHull& convexHull);

private:
    void getPointOnCircumference(const D2D_POINT_2U& center, const UINT32& radius, const double& angle, D2D_POINT_2L& point);
    void sortObjectDetailsBySize(std::vector<ObjectDetails*> &objectDetails);

private: // Members
    ImageProcessingUtils* m_imageProcessingUtils; // Not owned
};

#endif // IMAGEANALYZER_H
