#include "pch.h"

#include "ImageAnalyzer.h" // Own header

#include "ImageProcessingUtils.h"
#include "ObjectDetails.h"


// Constants
const double TwoPi = Pi * 2;
const double AngleIncrement = 0.1667 * Pi; // Roughly 1/6 * pi


ImageAnalyzer::ImageAnalyzer(ImageProcessingUtils* imageProcessingUtils)
    : m_imageProcessingUtils(imageProcessingUtils)
{
}


ImageAnalyzer::~ImageAnalyzer()
{
}


//------------------------------------------------------------------
// extractObjectDetails
//
// Extracts object details from the given, organized object map.
//------------------------------------------------------------------
std::vector<ObjectDetails*> ImageAnalyzer::extractObjectDetails(
    const UINT16* organizedObjectMap, const UINT32& objectMapWidth, const UINT32& objectMapHeight, const UINT16& objectCount) const
{
    std::vector<ObjectDetails*> objectDetailsVector;
    UINT32* objectPixelCountOnXAxis = NULL;

    for (UINT16 objectId = 1; objectId <= objectCount; ++objectId)
    {
        ObjectDetails *newObjectDetails = new ObjectDetails();
        newObjectDetails->_id = objectId;

        UINT32 currentIndex = 0;
        UINT32 objectPixelCountOnYAxis = 0;
        objectPixelCountOnXAxis = new UINT32[objectMapWidth];

        for (UINT32 i = 0; i < objectMapWidth; ++i)
        {
            objectPixelCountOnXAxis[i] = 0;
        }

        for (UINT32 y = 0; y < objectMapHeight; ++y)
        {
            objectPixelCountOnYAxis = 0;

            for (UINT32 x = 0; x < objectMapWidth; ++x)
            {
                if (organizedObjectMap[currentIndex] == objectId)
                {
                    objectPixelCountOnYAxis++;
                    objectPixelCountOnXAxis[x]++;
                    newObjectDetails->_area++;
                }

                currentIndex++;
            }

            if (newObjectDetails->_width < objectPixelCountOnYAxis)
            {
                newObjectDetails->_width = objectPixelCountOnYAxis;
                newObjectDetails->_centerY = y;
            }
        }

        for (UINT32 i = 0; i < objectMapWidth; ++i)
        {
            if (objectPixelCountOnXAxis[i] > newObjectDetails->_height)
            {
                newObjectDetails->_height = objectPixelCountOnXAxis[i];
                newObjectDetails->_centerX = i;
            }
        }

        delete objectPixelCountOnXAxis;
        objectDetailsVector.push_back(newObjectDetails);
    }

    return objectDetailsVector;
}


//------------------------------------------------------------------
// resolveLargeObjectIds
//
// Resolves the object IDs of all objects whose width or height is
// equal or greater than the given minimum size.
//------------------------------------------------------------------
std::vector<UINT16>* ImageAnalyzer::resolveLargeObjectIds(
    const UINT16* organizedObjectMap,
    const UINT32& objectMapWidth, const UINT32& objectMapHeight,
    UINT16& objectCount, const UINT32& minSize) const
{
    std::vector<ObjectDetails*> objectDetails =
        extractObjectDetails(organizedObjectMap, objectMapWidth, objectMapHeight, objectCount);
    sortObjectDetailsBySize(objectDetails);
    const UINT16 objectDetailsCount = objectDetails.size();
    std::vector<UINT16>* largeObjectIds = new std::vector<UINT16>();

    for (UINT16 i = 0; i < objectDetailsCount; ++i)
    {
        ObjectDetails* currentObjectDetails = objectDetails.at(i);

        if (currentObjectDetails
            && (currentObjectDetails->_width * currentObjectDetails->_height) >= minSize)
        {
            largeObjectIds->push_back(objectDetails.at(i)->_id);
        }
    }
    
    return largeObjectIds;
}


//------------------------------------------------------------------
// extractConvexHullsOfLargestObjects
//
// Returns a vector of newly allocated convex hulls.
//------------------------------------------------------------------
std::vector<ConvexHull*> ImageAnalyzer::extractConvexHullsOfLargestObjects(
    BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U& targetRect,
    const UINT8& maxNumberOfConvexHulls, const GUID& videoFormatSubtype) const
{
    std::vector<ConvexHull*> convexHulls;

    UINT16* objectMap = m_imageProcessingUtils->createObjectMap(binaryImage, imageWidth, imageHeight, targetRect, videoFormatSubtype);

    if (objectMap)
    {
        UINT16 objectCount = m_imageProcessingUtils->organizeObjectMap(objectMap, imageWidth * imageHeight);
        UINT32 minSize = (UINT32)((float)imageWidth * (float)imageHeight * RelativeObjectSizeThreshold);
        std::vector<UINT16>* largeObjectIds = resolveLargeObjectIds(objectMap, imageWidth, imageHeight, objectCount, minSize);
        objectCount = largeObjectIds->size();
        std::vector<D2D_POINT_2U>* sortedPoints = NULL;
        ConvexHull* convexHull = NULL;

        for (UINT16 i = 0; i < objectCount && convexHulls.size() <= maxNumberOfConvexHulls; ++i)
        {
            sortedPoints = m_imageProcessingUtils->extractSortedObjectPoints(objectMap, imageWidth, imageHeight, largeObjectIds->at(i));

            if (sortedPoints)
            {
                convexHull = m_imageProcessingUtils->createConvexHull(*sortedPoints, true);
                convexHulls.push_back(convexHull);
                delete sortedPoints;
            }
        }

        delete largeObjectIds;
    }

    delete objectMap;

    return convexHulls;
}


//------------------------------------------------------------------
// convexHullClosestToCircle
//
//
//------------------------------------------------------------------
ConvexHull* ImageAnalyzer::convexHullClosestToCircle(const std::vector<ConvexHull*> &convexHulls, LONG &error) const
{
    const UINT16 convexHullCount = convexHulls.size();
    ConvexHull* convexHull = NULL;
    UINT32 width = 0;
    UINT32 height = 0;
    D2D_POINT_2U center;
    LONG currentError = 0;
    error = -1;
    double radius = 0;
    int bestIndex = -1;

    for (UINT16 i = 0; i < convexHullCount; ++i)
    {
        convexHull = convexHulls.at(i);

        if (convexHull)
        {
            getMinimalEnclosingCircle(*convexHull, radius, center);
            currentError = circleCircumferenceError(*convexHull, radius, center);

            if (currentError < error || error < 0)
            {
                error = currentError;
                bestIndex = i;
            }
        }
    }

    if (bestIndex >= 0)
    {
        convexHull = convexHulls.at(bestIndex);
    }

    return convexHull;
}


//------------------------------------------------------------------
// calculateConvexHullDimensions
//
// Calculates the width, height and the center point of the given
// convex hull.
//------------------------------------------------------------------
inline void ImageAnalyzer::calculateConvexHullDimensions(
    const ConvexHull& convexHull, UINT32& width, UINT32& height, UINT32& centerX, UINT32& centerY) const
{
    const UINT32 convexHullSize = convexHull.size();

    if (convexHullSize < 2)
    {
        return;
    }

    D2D_POINT_2U currentPoint = convexHull.at(0);
    UINT32 minX = currentPoint.x;
    UINT32 maxX = currentPoint.x;
    UINT32 minY = currentPoint.y;
    UINT32 maxY = currentPoint.y;

    for (UINT32 i = 1; i < convexHullSize; ++i)
    {
        currentPoint = convexHull.at(i);

        if (currentPoint.x < minX)
        {
            minX = currentPoint.x;
        }
        else if (currentPoint.x > maxX)
        {
            maxX = currentPoint.x;
        }

        if (currentPoint.y < minY)
        {
            minY = currentPoint.y;
        }
        else if (currentPoint.y > maxY)
        {
            maxY = currentPoint.y;
        }
    }

    width = maxX - minX;
    height = maxY - minY;
    centerX = (maxX + minX) / 2;
    centerY = (maxY + minY) / 2;
}


//------------------------------------------------------------------
// convexHullDimensionsAsObjectDetails
//
//
//------------------------------------------------------------------
ObjectDetails* ImageAnalyzer::convexHullDimensionsAsObjectDetails(const ConvexHull& convexHull) const
{
    ObjectDetails* objectDetails = new ObjectDetails();

    calculateConvexHullDimensions(convexHull,
        objectDetails->_width, objectDetails->_height,
        objectDetails->_centerX, objectDetails->_centerY);

    return objectDetails;
}


//------------------------------------------------------------------
// convexHullMinimalEnclosingCircleAsObjectDetails
//
//
//------------------------------------------------------------------
ObjectDetails* ImageAnalyzer::convexHullMinimalEnclosingCircleAsObjectDetails(const ConvexHull& convexHull) const
{
    double radius = 0;
    D2D_POINT_2U circleCenter;
    getMinimalEnclosingCircle(convexHull, radius, circleCenter);

    ObjectDetails* objectDetails = new ObjectDetails();
    objectDetails->_centerX = circleCenter.x;
    objectDetails->_centerY = circleCenter.y;
    objectDetails->_width = (UINT32)(radius * 2);
    objectDetails->_height = objectDetails->_width;

    return objectDetails;
}


//------------------------------------------------------------------
// circleCircumferenceError
//
// This method does not calculate the error of the whole
// circumference. Instead, it takes N number of points on the
// circumference of a circle and compares them to the closest points
// found on the convex hull.
//
// Note: The method does not handle situations where the convex hull
// does not contain the full object (e.g. only half of the ball is
// visible).
//------------------------------------------------------------------
LONG ImageAnalyzer::circleCircumferenceError(
    const ConvexHull& convexHull, const double& radius, const D2D_POINT_2U& center) const
{
    D2D_POINT_2L pointOnPerfectCircleCircumference;
    ConvexHull convexHullCopy = convexHull;
    int indexOfSelectedPoint = -1;
    double pointError = -1;
    double tempError = 0;
    LONG totalError = 0;
    int currentConvexHullCopyPointCount = convexHullCopy.size();
    double angleIncrement = TwoPi / (double)currentConvexHullCopyPointCount;
    D2D_POINT_2U currentPointInConvexHull;
    const double normalizationCoefficient = radius * 2;
    
    for (double angle = 0; (angle < TwoPi && currentConvexHullCopyPointCount > 0); angle += angleIncrement)
    {
        getPointOnCircumference(center, radius, angle, pointOnPerfectCircleCircumference);

        pointError = -1;

        // Find the point closest
        for (int i = 0; i < currentConvexHullCopyPointCount; ++i)
        {
            currentPointInConvexHull = convexHullCopy.at(i);

            // Manhattan distance is faster than euclidean, and does not affect the
            // outcome since our scale of error is arbitrary.
            tempError =
                abs((LONG)currentPointInConvexHull.x - pointOnPerfectCircleCircumference.x)
                + abs((LONG)currentPointInConvexHull.y - pointOnPerfectCircleCircumference.y);

            tempError /= normalizationCoefficient; // Normalize

            if (pointError < tempError || pointError < 0)
            {
                pointError = tempError;
                indexOfSelectedPoint = i;
            }
        }

        if (indexOfSelectedPoint >= 0)
        {
            // Remove the used, selected point
            convexHullCopy.erase(convexHullCopy.begin() + indexOfSelectedPoint);
            indexOfSelectedPoint = -1;
            currentConvexHullCopyPointCount = convexHullCopy.size();
        }

        totalError += (LONG)pointError;
    }

    return totalError / convexHull.size();
}


//------------------------------------------------------------------
// circleAreaError
//
// Calculates the error between the area of a perfect circle (if it
// had the given diameter) and the given measured area.
// The calculated error is the difference between the perfect and
// the measured area as an absolute value.
//------------------------------------------------------------------
double ImageAnalyzer::circleAreaError(UINT32 measuredDiameter, UINT32 measuredArea) const
{
    float radius = static_cast<float>(measuredDiameter) / 2;
    return abs(measuredArea - Pi *  radius * radius);
}



//-------------------------------------------------------------------
// extractBestCircularConvexHull
//
// Finds the convex hull closest to a circle shape from the given
// set of convex hull. The others are deleted so note that the list
// is unusable after calling this method.
//-------------------------------------------------------------------
ConvexHull* ImageAnalyzer::extractBestCircularConvexHull(std::vector<ConvexHull*>& convexHulls) const
{
    ConvexHull* convexHull = NULL;

    if (convexHulls.size() > 0)
    {
        LONG error = 0;

        convexHull = convexHullClosestToCircle(convexHulls, error);
       
        if (convexHull)
        {
            // Remove the selected convexhull
            for (UINT16 i = 0; i < convexHulls.size(); ++i)
            {
                if (convexHulls.at(i) == convexHull)
                {
                    convexHulls[i] = NULL;
                    break;
                }
            }  
        }

        DeletePointerVector(convexHulls);
    }

    return convexHull;
}


//-------------------------------------------------------------------
// extractBestCircularConvexHull
//
// Extracts convex hulls from the given frame and tries to determine
// the best candidate to match the desired (circular) object.
//
// Note that the returned convex hull is owned by the caller.
//-------------------------------------------------------------------
ConvexHull* ImageAnalyzer::extractBestCircularConvexHull(
    BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight, const D2D_RECT_U& targetRect,
    const UINT8& maxCandidates, const GUID& videoFormatSubtype, bool drawConvexHulls) const
{
    std::vector<ConvexHull*> convexHulls =
        extractConvexHullsOfLargestObjects(binaryImage, imageWidth, imageHeight, targetRect, maxCandidates, videoFormatSubtype);

    if (drawConvexHulls)
    {
#pragma warning(push)
#pragma warning(disable: 4018) 
        for (int i = 0; i < convexHulls.size(); ++i)
        {
            m_imageProcessingUtils->visualizeConvexHull(
                binaryImage, imageWidth, imageHeight,
                *convexHulls.at(i), videoFormatSubtype, 4, 0x4c, 0x54, 0xff);
        }
#pragma warning(pop)
    }

    ConvexHull* convexHull = extractBestCircularConvexHull(convexHulls);

    if (convexHull && drawConvexHulls)
    {
        m_imageProcessingUtils->visualizeConvexHull(
            binaryImage, imageWidth, imageHeight,
            *convexHull, videoFormatSubtype, 4, 0xff, 0x0, 0x0);
    }

    return convexHull;
}


//-------------------------------------------------------------------
// extractBestCircularConvexHull
//
// For convenience.
//-------------------------------------------------------------------
ConvexHull* ImageAnalyzer::extractBestCircularConvexHull(
    BYTE* binaryImage, const UINT32& imageWidth, const UINT32& imageHeight,
    const UINT8& maxCandidates, const GUID& videoFormatSubtype, bool drawConvexHulls) const
{
    D2D_RECT_U targetRect;
    targetRect.left = 0;
    targetRect.right = imageWidth;
    targetRect.top = 0;
    targetRect.bottom = imageHeight;

    return extractBestCircularConvexHull(
        binaryImage, imageWidth, imageHeight, targetRect,
        maxCandidates, videoFormatSubtype, drawConvexHulls);
}


//-------------------------------------------------------------------
// objectCenterIsWithinConvexHullBounds
//
//
//-------------------------------------------------------------------
bool ImageAnalyzer::objectCenterIsWithinConvexHullBounds(const ObjectDetails& objectDetails, const ConvexHull& convexHull) const
{
    ObjectDetails* convexHullObjectDetails = convexHullDimensionsAsObjectDetails(convexHull);

    bool isWithin = true;

    if (abs(static_cast<double>(convexHullObjectDetails->_centerX) - objectDetails._centerX) > convexHullObjectDetails->_width / 2
        || abs(static_cast<double>(convexHullObjectDetails->_centerY) - objectDetails._centerY) > convexHullObjectDetails->_height / 2)
    {
        // The object details center is outside of the convex hull
        isWithin = false;
    }

    delete convexHullObjectDetails;
    return isWithin;
}


//------------------------------------------------------------------
// getMinimalEnclosingCircle
//
// Calculates the minimal enclosing circle for the given convex
// hull.
//------------------------------------------------------------------
void ImageAnalyzer::getMinimalEnclosingCircle(
    const ConvexHull& convexHull, double& radius, D2D_POINT_2U& circleCenter) const
{
    // Find the verteces most distant
    const int convexHullSize = convexHull.size();
    D2D_POINT_2U currentPoint1;
    D2D_POINT_2U currentPoint2;
    double currentDistance = 0;
    double greatestDistance = 0;
    int vertexIndex1 = -1;
    int vertexIndex2 = -1;

    for (int i = 0; i < convexHullSize - 1; ++i)
    {
        for (int j = i + 1; j < convexHullSize; ++j)
        {
            if (i == j)
            {
                continue;
            }

            currentPoint1 = convexHull.at(i);
            currentPoint2 = convexHull.at(j);
            currentDistance = sqrt(
                pow(abs((double)currentPoint1.x - (double)currentPoint2.x), 2)
                + pow(abs((double)currentPoint1.y - (double)currentPoint2.y), 2));

            if (currentDistance > greatestDistance)
            {
                greatestDistance = currentDistance;
                vertexIndex1 = i;
                vertexIndex2 = j;
            }
        }
    }

    radius = greatestDistance / 2;
    circleCenter.x = (convexHull.at(vertexIndex1).x + convexHull.at(vertexIndex2).x) / 2;
    circleCenter.y = (convexHull.at(vertexIndex1).y+ convexHull.at(vertexIndex2).y) / 2;
}


//------------------------------------------------------------------
// getPointOnCircumference
//
// Calculates a point on circle's circumference based on the given
// center, radius and angle.
//------------------------------------------------------------------
void ImageAnalyzer::getPointOnCircumference(const D2D_POINT_2U& center, const double& radius, const double& angle, D2D_POINT_2L& point) const
{
#pragma warning(push)
#pragma warning(disable: 4244) 
    point.x = center.x + radius * cos(angle);
    point.y = center.y + radius * sin(angle);
#pragma warning(pop)
}


//------------------------------------------------------------------
// sortObjectDetailsBySize
//
// 
//------------------------------------------------------------------
void ImageAnalyzer::sortObjectDetailsBySize(std::vector<ObjectDetails*> &objectDetails) const
{
    std::sort(objectDetails.begin(), objectDetails.end(),
        [](ObjectDetails *a, ObjectDetails *b)
    {
        if (a->_width * a->_height > b->_width * b->_height)
        {
            return true;
        }

        return false;
    });
}

