#include "brepdb/Edge.h"
#include "brepdb/Point.h"
#include "brepdb/Region.h"
#include "brepdb/ShapeType.h"
#include "brepdb/Math.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>

namespace brepdb
{

Edge::Edge()
{
	memset(m_start, 0, DIMENSION * sizeof(double));
	memset(m_end, 0, DIMENSION * sizeof(double));
}

Edge::Edge(const double* start, const double* end)
{
	Initialize(start, end);
}

Edge::Edge(const Point& start, const Point& end)
{
	Initialize(start.GetCoords(), end.GetCoords());
}

Edge::Edge(const Edge& e)
{
	Initialize(e.m_start, e.m_end);
}

Edge& Edge::operator = (const Edge& e)
{
	if (this != &e) {
		Initialize(e.m_start, e.m_end);
	}
	return *this;
}

bool Edge::operator == (const Edge& e) const
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		if (m_start[i] < e.m_start[i] - std::numeric_limits<double>::epsilon() ||
			m_start[i] > e.m_start[i] + std::numeric_limits<double>::epsilon()) {
			return false;
		}

		if (m_end[i] < e.m_end[i] - std::numeric_limits<double>::epsilon() ||
			m_end[i] > e.m_end[i] + std::numeric_limits<double>::epsilon()) {
			return false;
		}
	}

	return true;
}

Edge* Edge::Clone()
{
	return new Edge(*this);
}

uint32_t Edge::GetByteArraySize() const
{
	return DIMENSION * sizeof(double) * 2;
}

void Edge::LoadFromByteArray(const uint8_t* data)
{
	auto ptr = data;
	memcpy(m_start, ptr, DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(m_end, ptr, DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);
}

void Edge::StoreToByteArray(uint8_t** data, uint32_t& length) const
{
	length = GetByteArraySize();
	*data = new uint8_t[length];
	uint8_t* ptr = *data;

	memcpy(ptr, m_start, DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(ptr, m_end, DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);
}

uint32_t Edge::ShapeType() const
{
	return ST_EDGE;
}

bool Edge::IntersectsShape(const IShape& s) const
{
	bool ret = false;

	switch (s.ShapeType())
	{
	case ST_EDGE:
	{
		const Edge& e = static_cast<const Edge&>(s);
		ret = IntersectsEdge(e);
	}
		break;
	case ST_REGION:
	{
		const Region& r = static_cast<const Region&>(s);
		ret = IntersectsRegion(r);
	}
		break;
	}

	return ret;
}

bool Edge::ContainsShape(const IShape& s) const
{
	return false;
}

bool Edge::TouchesShape(const IShape& s) const
{
	return false;
}

void Edge::GetCenter(Point& p) const
{
	double coords[DIMENSION];
	for (int i = 0; i < DIMENSION; ++i) {
		coords[i] = std::abs(m_start[i] - m_end[i]) / 2.0 + std::min(m_start[i], m_end[i]);
	}
	p = Point(coords);
}

void Edge::GetMBR(Region& r) const
{
	double low[DIMENSION], high[DIMENSION];
	for (int i = 0; i < DIMENSION; ++i)
	{
		low[i] = std::min(m_start[i], m_end[i]);
		high[i] = std::max(m_start[i], m_end[i]);
	}

	r = Region(low, high);
}

double Edge::GetArea() const
{
	return 0.0;
}

double Edge::GetMinimumDistance(const IShape& s) const
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
	}

	return ret;
}

bool Edge::IntersectsEdge(const Edge& e) const
{
	return Math::Intersects(Point(m_start), Point(m_end), Point(e.m_start), Point(e.m_end));
}

bool Edge::IntersectsRegion(const Region& r) const
{
	return r.IntersectsEdge(*this);
}

double Edge::GetMinimumDistance(const Point& p) const
{
	auto p_coords = p.GetCoords();

	if (m_end[0] >= m_start[0] - std::numeric_limits<double>::epsilon() &&
		m_end[0] <= m_start[0] + std::numeric_limits<double>::epsilon()) {
		return std::abs(p_coords[0] - m_start[0]);
	}

	if (m_end[1] >= m_start[1] - std::numeric_limits<double>::epsilon() &&
		m_end[1] <= m_start[1] + std::numeric_limits<double>::epsilon()) {
		return std::abs(p_coords[1] - m_start[1]);
	}

	double x1 = m_start[0];
	double x2 = m_end[0];
	double x0 = p_coords[0];
	double y1 = m_start[1];
	double y2 = m_end[1];
	double y0 = p_coords[1];

	return std::abs((x2 - x1) * (y1 - y0) - (x1 - x0) * (y2 - y1)) / (std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)));
}

void Edge::Initialize(const double* start, const double* end)
{
	memcpy(m_start, start, DIMENSION * sizeof(double));
	memcpy(m_end, end, DIMENSION * sizeof(double));
}

}