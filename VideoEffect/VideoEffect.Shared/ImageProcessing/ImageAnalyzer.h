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
        const UINT16* organizedObjectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, const UINT16& objectCount) const;

    std::vector<UINT16>* resolveLargeObjectIds(
        const UINT16* organizedObjectMap,
        const UINT32& objectMapWidth, const UINT32& objectMapHeight,
        UINT16& objectCount, const UINT32& minSize) const;

    std::vector<ConvexHull*> extractConvexHullsOfLargestObjects(
        BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U& targetRect,
        const UINT8& maxNumberOfConvexHulls, const GUID& videoFormatSubtype) const;

    ConvexHull* convexHullClosestToCircle(const std::vector<ConvexHull*> &convexHulls, LONG &error) const;

    void calculateConvexHullDimensions(
        const ConvexHull& convexHull,
        UINT32& width, UINT32& height, UINT32& centerX, UINT32& centerY) const;

    ObjectDetails* convexHullDimensionsAsObjectDetails(const ConvexHull& convexHull) const;

    ObjectDetails* convexHullMinimalEnclosingCircleAsObjectDetails(const ConvexHull& convexHull) const;

    LONG circleCircumferenceError(
        const ConvexHull& convexHull, const double& radius, const D2D_POINT_2U& center) const;

    double circleAreaError(UINT32 measuredDiameter, UINT32 measuredArea) const;

    ConvexHull* extractBestCircularConvexHull(std::vector<ConvexHull*>& convexHulls) const;

    ConvexHull* extractBestCircularConvexHull(
        BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U& targetRect,
        const UINT8& maxCandidates, const GUID& videoFormatSubtype, bool drawConvexHulls = false) const;

    ConvexHull* extractBestCircularConvexHull(
        BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight,
        const UINT8& maxCandidates, const GUID& videoFormatSubtype, bool drawConvexHulls = false) const;

    bool objectCenterIsWithinConvexHullBounds(const ObjectDetails& objectDetails, const ConvexHull& convexHull) const;

    void getMinimalEnclosingCircle(const ConvexHull& convexHull, double& radius, D2D_POINT_2U& circleCenter) const;

private:
    void getPointOnCircumference(const D2D_POINT_2U& center, const double& radius, const double& angle, D2D_POINT_2L& point) const;
    void sortObjectDetailsBySize(std::vector<ObjectDetails*> &objectDetails) const;

private: // Members
    ImageProcessingUtils* m_imageProcessingUtils; // Not owned
};

#endif // IMAGEANALYZER_H
