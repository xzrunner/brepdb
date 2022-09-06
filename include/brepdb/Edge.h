#pragma once

#include "brepdb/SpatialIndex.h"
#include "brepdb/typedef.h"

namespace brepdb
{

class Point;

class Edge : public IShape
{
public:
	Edge();
	Edge(const double* start, const double* end);
	Edge(const Point& start, const Point& end);
	Edge(const Edge& e);

	virtual Edge& operator = (const Edge& e);
	virtual bool operator == (const Edge& e) const;

	//
	// IObject interface
	//
	virtual Edge* Clone() override;

	//
	// ISerializable interface
	//
	virtual uint32_t GetByteArraySize() const override;
	virtual void LoadFromByteArray(const uint8_t* data) override;
	virtual void StoreToByteArray(uint8_t** data, uint32_t& length) const override;

	//
	// IShape interface
	//
	virtual uint32_t ShapeType() const override;
	virtual bool IntersectsShape(const IShape& s) const override;
	virtual bool ContainsShape(const IShape& s) const override;
	virtual bool TouchesShape(const IShape& s) const override;
	virtual void GetCenter(Point& p) const override;
	virtual void GetMBR(Region& r) const override;
	virtual double GetArea() const override;
	virtual double GetMinimumDistance(const IShape& s) const override;

	const double* GetStart() const { return m_start; }
	const double* GetEnd() const { return m_end; }

	bool IntersectsEdge(const Edge& e) const;
	bool IntersectsRegion(const Region& r) const;
	double GetMinimumDistance(const Point& p) const;

private:
	void Initialize(const double* start, const double* end);

private:
	double m_start[DIMENSION];
	double m_end[DIMENSION];

}; // Edge

}