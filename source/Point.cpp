#include "brepdb/Point.h"
#include "brepdb/Region.h"
#include "brepdb/ShapeType.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace brepdb
{

Point::Point()
{
	memset(m_coords, 0, DIMENSION * sizeof(double));
}

Point::Point(const double* coords)
{
	memcpy(m_coords, coords, DIMENSION * sizeof(double));
}

Point::Point(const Point& p)
{
	memcpy(m_coords, p.m_coords, DIMENSION * sizeof(double));
}

Point& Point::operator = (const Point& p)
{
	memcpy(m_coords, p.m_coords, DIMENSION * sizeof(double));
	return *this;
}

bool Point::operator == (const Point& p) const
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		if (m_coords[i] < p.m_coords[i] - std::numeric_limits<double>::epsilon() ||
			m_coords[i] > p.m_coords[i] + std::numeric_limits<double>::epsilon())  
			return false;
	}
	return true;
}

Point* Point::Clone()
{
	return new Point(*this);
}

uint32_t Point::GetByteArraySize() const
{
	return (sizeof(uint32_t) + DIMENSION * sizeof(double));
}

void Point::LoadFromByteArray(const uint8_t* data)
{
	memcpy(m_coords, data, DIMENSION * sizeof(double));
}

void Point::StoreToByteArray(uint8_t** data, uint32_t& length) const
{
	length = GetByteArraySize();
	*data = new uint8_t[length];
	uint8_t* ptr = *data;

	memcpy(ptr, m_coords, DIMENSION * sizeof(double));
}

uint32_t Point::ShapeType() const
{
	return ST_POINT;
}

bool Point::IntersectsShape(const IShape& s) const
{
	if (s.ShapeType() == ST_REGION)
	{
		const Region& r = static_cast<const Region&>(s);
		return r.ContainsPoint(*this);
	}

	return false;
}

bool Point::ContainsShape(const IShape& s) const
{
	return false;
}

bool Point::TouchesShape(const IShape& s) const
{
	bool ret = false;

	switch (s.ShapeType())
	{
	case ST_POINT:
	{
		const Point& p = static_cast<const Point&>(s);
		ret = *this == p;
	}
		break;
	case ST_REGION:
	{
		const Region& r = static_cast<const Region&>(s);
		ret = r.TouchesPoint(*this);
	}
		break;
	}

	return ret;
}

void Point::GetCenter(Point& p) const
{
	p = *this;
}

void Point::GetMBR(Region& r) const
{
	r = Region(m_coords, m_coords);
}

double Point::GetArea() const
{
	return 0.0;
}

double Point::GetMinimumDistance(const IShape& s) const
{
	double ret = std::numeric_limits<double>::max();

	switch (s.ShapeType())
	{
	case ST_POINT:
	{
		const Point& p = static_cast<const Point&>(s);
		ret = GetMinimumDistance(p);
	}
		break;
	case ST_REGION:
	{
		const Region& r = static_cast<const Region&>(s);
		ret = r.GetMinimumDistance(*this);
	}
		break;
	}

	return ret;
}

double Point::GetMinimumDistance(const Point& p) const
{
	double ret = 0.0;

	for (int i = 0; i < DIMENSION; ++i) {
		ret += std::pow(m_coords[i] - p.m_coords[i], 2.0);
	}

	return std::sqrt(ret);
}

}