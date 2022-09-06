#pragma once

#include "brepdb/SpatialIndex.h"
#include "brepdb/typedef.h"

namespace brepdb
{

class Point : public IShape
{
public:
	Point();
	Point(const double* coords);
	Point(const Point& p);

	virtual Point& operator = (const Point& p);
	virtual bool operator == (const Point& p) const;

	//
	// IObject interface
	//
	virtual Point* Clone() override final;

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

	const double* GetCoords() const { return m_coords; }

private:
	double GetMinimumDistance(const Point& p) const;

private:
	double m_coords[DIMENSION];

}; // Point

}