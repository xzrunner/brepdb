#include "brepdb/Face.h"
#include "brepdb/Point.h"
#include "brepdb/Region.h"
#include "brepdb/ShapeType.h"

#include <cstring>
#include <limits>
#include <algorithm>

namespace brepdb
{

Face::Face(const double* verts, size_t num)
{
	Initialize(verts, num);
}

Face::Face(const Face& f)
{
	Initialize(f.m_vertices, f.m_num);
}

Face::~Face()
{
	delete[] m_vertices;
}

Face& Face::operator = (const Face& f)
{
	if (this != &f) {
		Initialize(f.m_vertices, f.m_num);
	}
	return *this;
}

bool Face::operator == (const Face& f) const
{
	if (m_num != f.m_num) {
		return false;
	}

	for (int i = 0, n = m_num * DIMENSION; i < n; ++i)
	{
		if (m_vertices[i] < f.m_vertices[i] - std::numeric_limits<double>::epsilon() ||
			m_vertices[i] > f.m_vertices[i] + std::numeric_limits<double>::epsilon()) {
			return false;
		}
	}

	return true;
}

Face* Face::Clone()
{
	return new Face(*this);
}

uint32_t Face::GetByteArraySize() const
{
	return sizeof(uint32_t) + m_num * DIMENSION * sizeof(double);
}

void Face::LoadFromByteArray(const uint8_t* data)
{
	auto ptr = data;

	uint32_t num;
	memcpy(&num, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	Initialize(reinterpret_cast<const double*>(ptr), num);
}

void Face::StoreToByteArray(uint8_t** data, uint32_t& length) const
{
	length = GetByteArraySize();
	*data = new uint8_t[length];
	uint8_t* ptr = *data;

	memcpy(ptr, &m_num, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(ptr, m_vertices, m_num * DIMENSION * sizeof(double));
	//ptr += m_num * DIMENSION * sizeof(double);
}

void Face::Initialize(const double* verts, size_t num)
{
	if (m_vertices) {
		delete[] m_vertices;
	}

	m_vertices = new double[num * DIMENSION];
	if (m_vertices) {
		memcpy(m_vertices, verts, num * DIMENSION * sizeof(double));
	}
}

uint32_t Face::ShapeType() const
{
	return ST_FACE;
}

bool Face::IntersectsShape(const IShape& s) const
{
	return false;
}

bool Face::ContainsShape(const IShape& s) const
{
	return false;
}

bool Face::TouchesShape(const IShape& s) const
{
	return false;
}

void Face::GetCenter(Point& p) const
{
	if (m_num == 0) {
		return;
	}

	double coords[DIMENSION];
	memset(coords, 0, sizeof(coords));

	auto ptr = m_vertices;
	for (int i = 0; i < m_num; ++i) {
		for (int j = 0; j < DIMENSION; ++j) {
			coords[j] += *ptr++;
		}
	}

	for (int i = 0; i < DIMENSION; ++i) {
		coords[i] /= m_num;
	}
	p = Point(coords);
}

void Face::GetMBR(Region& r) const
{
	double low[DIMENSION], high[DIMENSION];
	for (int i = 0; i < DIMENSION; ++i) {
		low[i] = std::numeric_limits<double>::max();
		high[i] = -std::numeric_limits<double>::max();
	}

	auto ptr = m_vertices;
	for (int i = 0; i < m_num; ++i) {
		for (int j = 0; j < DIMENSION; ++j) {
			low[j] = std::min(low[j], *ptr);
			high[j] = std::min(low[j], *ptr);
			++ptr;
		}
	}

	r = Region(low, high);
}

double Face::GetArea() const
{
	return 0;
}

double Face::GetMinimumDistance(const IShape& s) const
{
	return std::numeric_limits<double>::max();
}

}