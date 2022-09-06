#pragma once

#include "brepdb/SpatialIndex.h"
#include "brepdb/typedef.h"

namespace brepdb
{

class Face : public IShape
{
public:
	Face() {}
	Face(const double* verts, size_t num);
	Face(const Face& f);
	virtual ~Face();

	virtual Face& operator = (const Face& f);
	virtual bool operator == (const Face& f) const;

	//
	// IObject interface
	//
	virtual Face* Clone() override;

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

private:
	void Initialize(const double* verts, size_t num);

private:
	double* m_vertices = nullptr;
	size_t m_num = 0;

}; // Face

}