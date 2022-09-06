#pragma once

#include "brepdb/SpatialIndex.h"
#include "brepdb/typedef.h"

namespace brepdb
{

class Edge;

class Region : public IShape
{
public:
	Region();
	Region(const double* low, const double* high);
	Region(const Point& low, const Point& high);
	Region(const Region& r);

	virtual Region& operator = (const Region& r);
	virtual bool operator == (const Region& r) const;

	//
	// IObject interface
	//
	virtual Region* Clone() override;

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

	const double* GetLow() const { return m_low; }
	const double* GetHigh() const { return m_high; }

	bool IntersectsRegion(const Region& r) const;
	bool ContainsRegion(const Region& r) const;
	bool TouchesRegion(const Region& r) const;
	double GetMinimumDistance(const Region& r) const;

	bool IntersectsEdge(const Edge& e) const;

	bool ContainsPoint(const Point& p) const;
	bool TouchesPoint(const Point& p) const;
	double GetMinimumDistance(const Point& p) const;

	Region GetIntersectingRegion(const Region& r) const;
	double GetIntersectingArea(const Region& r) const;
	double GetMargin() const;

	void Combine(const Region& r);
	void Combine(const Point& p);

	void MakeInfinite();

private:
	void Initialize(const double* low, const double* high);

private:
	double m_low[DIMENSION];
	double m_high[DIMENSION];

}; // Region

}