#include "brepdb/Region.h"
#include "brepdb/Point.h"
#include "brepdb/Edge.h"
#include "brepdb/ShapeType.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <algorithm>

namespace brepdb
{

Region::Region()
{
	memset(m_low, 0, DIMENSION * sizeof(double));
	memset(m_high, 0, DIMENSION * sizeof(double));
}

Region::Region(const double* low, const double* high)
{
	Initialize(low, high);
}

Region::Region(const Point& low, const Point& high)
{
	Initialize(low.GetCoords(), high.GetCoords());
}

Region::Region(const Region& r)
{
	Initialize(r.m_low, r.m_high);
}

Region& Region::operator = (const Region& r)
{
	if (this != &r) {
		Initialize(r.m_low, r.m_high);
	}
	return *this;
}

bool Region::operator == (const Region& r) const
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		if (m_low[i] < r.m_low[i] - std::numeric_limits<double>::epsilon() ||
			m_low[i] > r.m_low[i] + std::numeric_limits<double>::epsilon() ||
			m_high[i] < r.m_high[i] - std::numeric_limits<double>::epsilon() ||
			m_high[i] > r.m_high[i] + std::numeric_limits<double>::epsilon())
			return false;
	}
	return true;
}

Region* Region::Clone()
{
	return new Region(*this);
}

uint32_t Region::GetByteArraySize() const
{
	return 2 * DIMENSION * sizeof(double);
}

void Region::LoadFromByteArray(const uint8_t* data)
{
	auto ptr = data;
	memcpy(m_low, ptr, DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(m_high, ptr, DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);
}

void Region::StoreToByteArray(uint8_t** data, uint32_t& length) const
{
	length = GetByteArraySize();
	*data = new uint8_t[length];
	uint8_t* ptr = *data;

	memcpy(ptr, m_low, DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(ptr, m_high, DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);
}

uint32_t Region::ShapeType() const
{
	return ST_REGION;
}

bool Region::IntersectsShape(const IShape& s) const
{
	bool ret = false;

	switch (s.ShapeType())
	{
	case ST_POINT:
	{
		const Point& p = static_cast<const Point&>(s);
		ret = ContainsPoint(p);
	}
		break;
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

bool Region::ContainsShape(const IShape& s) const
{
	bool ret = false;

	switch (s.ShapeType())
	{
	case ST_POINT:
	{
		const Point& p = static_cast<const Point&>(s);
		ret = ContainsPoint(p);
	}
		break;
	case ST_REGION:
	{
		const Region& r = static_cast<const Region&>(s);
		ret = ContainsRegion(r);
	}
		break;
	}

	return ret;
}

bool Region::TouchesShape(const IShape& s) const
{
	bool ret = false;

	switch (s.ShapeType())
	{
	case ST_POINT:
	{
		const Point& p = static_cast<const Point&>(s);
		ret = TouchesPoint(p);
	}
		break;
	case ST_REGION:
	{
		const Region& r = static_cast<const Region&>(s);
		ret = TouchesRegion(r);
	}
		break;
	}

	return ret;
}

void Region::GetCenter(Point& p) const
{
	double coords[DIMENSION];
	for (int i = 0; i < DIMENSION; ++i) {
		coords[i] = (m_low[i] + m_high[i]) / 2.0;
	}
	p = Point(coords);
}

void Region::GetMBR(Region& r) const
{
	r = *this;
}

double Region::GetArea() const
{
	double area = 1.0;
	for (int i = 0; i < DIMENSION; ++i) {
		area *= m_high[i] - m_low[i];
	}
	return area;
}

double Region::GetMinimumDistance(const IShape& s) const
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
		ret = GetMinimumDistance(r);
	}
		break;
	}

	return ret;
}

bool Region::IntersectsRegion(const Region& r) const
{
	for (int i = 0; i < DIMENSION; ++i) {
		if (m_low[i] > r.m_high[i] || m_high[i] < r.m_low[i]) {
			return false;
		}
	}
	return true;
}

bool Region::ContainsRegion(const Region& r) const
{
	for (int i = 0; i < DIMENSION; ++i) {
		if (m_low[i] > r.m_low[i] || m_high[i] < r.m_high[i]) {
			return false;
		}
	}
	return true;
}

bool Region::TouchesRegion(const Region& r) const
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		if ((m_low[i] >= r.m_low[i] - std::numeric_limits<double>::epsilon() &&
			 m_low[i] <= r.m_low[i] + std::numeric_limits<double>::epsilon()) ||
			(m_high[i] >= r.m_high[i] - std::numeric_limits<double>::epsilon() &&
			 m_high[i] <= r.m_high[i] + std::numeric_limits<double>::epsilon()))
			return true;
	}
	return false;
}

double Region::GetMinimumDistance(const Region& r) const
{
	double ret = 0.0;

	for (int i = 0; i < DIMENSION; ++i)
	{
		double x = 0.0;

		if (r.m_high[i] < m_low[i]) {
			x = std::abs(r.m_high[i] - m_low[i]);
		} else if (m_high[i] < r.m_low[i]) {
			x = std::abs(r.m_low[i] - m_high[i]);
		}

		ret += x * x;
	}

	return std::sqrt(ret);
}

bool Region::IntersectsEdge(const Edge& e) const
{
	Point ll(m_low), ur(m_high);

	double c_ul[2] = { m_low[0], m_high[1] };
	double c_lr[2] = { m_high[0], m_low[1] };
	Point ul(&c_ul[0]), lr(&c_lr[0]);

	Point p1(e.GetStart()), p2(e.GetEnd());

	return ContainsPoint(p1) || ContainsPoint(p2) ||
		   e.IntersectsShape(Edge(ll, ul)) || e.IntersectsShape(Edge(ul, ur)) ||
		   e.IntersectsShape(Edge(ur, lr)) || e.IntersectsShape(Edge(lr, ll));

}

bool Region::ContainsPoint(const Point& p) const
{
	auto p_pos = p.GetCoords();
	for (int i = 0; i < DIMENSION; ++i) {
		if (m_low[i] > p_pos[i] || m_high[i] < p_pos[i]) {
			return false;
		}
	}
	return true;
}

bool Region::TouchesPoint(const Point& p) const
{
	auto p_pos = p.GetCoords();
	for (int i = 0; i < DIMENSION; ++i)
	{
		if ((m_low[i] >= p_pos[i] - std::numeric_limits<double>::epsilon() &&
		     m_low[i] <= p_pos[i] + std::numeric_limits<double>::epsilon()) ||
			(m_high[i] >= p_pos[i] - std::numeric_limits<double>::epsilon() &&
			 m_high[i] <= p_pos[i] + std::numeric_limits<double>::epsilon()))
			return true;
	}
	return false;
}

double Region::GetMinimumDistance(const Point& p) const
{
	double ret = 0.0;

	auto p_pos = p.GetCoords();
	for (int i = 0; i < DIMENSION; ++i)
	{
		if (p_pos[i] < m_low[i])
		{
			ret += std::pow(m_low[i] - p_pos[i], 2.0);
		}
		else if (p_pos[i] > m_high[i])
		{
			ret += std::pow(p_pos[i] - m_high[i], 2.0);
		}
	}

	return std::sqrt(ret);
}

Region Region::GetIntersectingRegion(const Region& r) const
{
	Region ret;
	ret.MakeInfinite();

	// check for intersection.
	// marioh: avoid function call since this is called billions of times.
	for (int i = 0; i < DIMENSION; ++i) {
		if (m_low[i] > r.m_high[i] || m_high[i] < r.m_low[i]) {
			return ret;
		}
	}

	for (int i = 0; i < DIMENSION; ++i)
	{
		ret.m_low[i] = std::max(m_low[i], r.m_low[i]);
		ret.m_high[i] = std::min(m_high[i], r.m_high[i]);
	}

	return ret;
}

double Region::GetIntersectingArea(const Region& r) const
{
	double ret = 1.0;
	double f1, f2;

	for (int i = 0; i < DIMENSION; ++i)
	{
		if (m_low[i] > r.m_high[i] || m_high[i] < r.m_low[i]) {
			return 0.0;
		}

		f1 = std::max(m_low[i], r.m_low[i]);
		f2 = std::min(m_high[i], r.m_high[i]);
		ret *= f2 - f1;
	}

	return ret;
}

double Region::GetMargin() const
{
	double mul = std::pow(2.0, static_cast<double>(DIMENSION) - 1.0);
	double margin = 0.0;

	for (int i = 0; i < DIMENSION; ++i) {
		margin += (m_high[i] - m_low[i]) * mul;
	}

	return margin;
}

void Region::Combine(const Region& r)
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		m_low[i] = std::min(m_low[i], r.m_low[i]);
		m_high[i] = std::max(m_high[i], r.m_high[i]);
	}
}

void Region::Combine(const Point& p)
{
	auto p_pos = p.GetCoords();
	for (int i = 0; i < DIMENSION; ++i)
	{
		m_low[i] = std::min(m_low[i], p_pos[i]);
		m_high[i] = std::max(m_high[i], p_pos[i]);
	}
}

void Region::MakeInfinite()
{
	for (int i = 0; i < DIMENSION; ++i)
	{
		m_low[i] = std::numeric_limits<double>::max();
		m_high[i] = -std::numeric_limits<double>::max();
	}
}

void Region::Initialize(const double* low, const double* high)
{
	memcpy(m_low, low, DIMENSION * sizeof(double));
	memcpy(m_high, high, DIMENSION * sizeof(double));
}

}