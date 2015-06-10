#ifndef OBJECTDETAILS_H
#define OBJECTDETAILS_H

#include <mfapi.h>

class ObjectDetails
{
public:
	ObjectDetails() :
		_id(0),
        _area(0),
		_width(0),
		_height(0),
		_centerX(0),
		_centerY(0)
	{
	}

	ObjectDetails & operator= (const ObjectDetails & other)
	{
		if (this != &other)
		{
			_id = other._id;
			_area = other._area;
			_width = other._width;
			_height = other._height;
			_centerX = other._centerX;
			_centerY = other._centerY;
		}

		return *this;
	}

public: // Members
	UINT16 _id;
	UINT32 _area;
	UINT32 _width;
	UINT32 _height;
	UINT32 _centerX;
	UINT32 _centerY;
};

#endif // OBJECTDETAILS_H
