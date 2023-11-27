#pragma once

#include "brepdb/Tools.h"
#include "brepdb/typedef.h"

#include <vector>

namespace brepdb
{

enum RTreeVariant
{
	RV_LINEAR = 0x0,
	RV_QUADRATIC,
	RV_RSTAR
};

enum PersistenObjectIdentifier
{
	PersistentIndex = 0x1,
	PersistentLeaf = 0x2
};

enum RangeQueryType
{
	ContainmentQuery = 0x1,
	IntersectionQuery = 0x2
};

enum class VisitorStatus
{
	Stop,
	Skip,
	Continue
};

class Point;
class Region;

class IShape : public IObject, public ISerializable
{
public:
	virtual uint32_t ShapeType() const = 0;
	virtual bool IntersectsShape(const IShape& s) const = 0;
	virtual bool ContainsShape(const IShape& s) const = 0;
	virtual bool TouchesShape(const IShape& s) const = 0;
	virtual void GetCenter(Point& p) const = 0;
	virtual void GetMBR(Region& r) const = 0;
	virtual double GetArea() const = 0;
	virtual double GetMinimumDistance(const IShape& s) const = 0;

}; // IShape

class IEntry : public IObject
{
public:
	virtual id_type GetIdentifier() const = 0;
	virtual void GetShape(IShape** out) const = 0;

}; // IEntry

class INode : public IEntry, public ISerializable
{
public:
	virtual uint32_t GetChildrenCount() const = 0;
	virtual id_type GetChildIdentifier(uint32_t index) const = 0;
	virtual void GetChildData(uint32_t index, uint32_t& len, uint8_t** data) const = 0;
	virtual void GetChildShape(uint32_t index, IShape** out) const = 0;
	virtual uint32_t GetLevel() const = 0;
	virtual bool IsIndex() const = 0;
	virtual bool IsLeaf() const = 0;

}; // INode

class IData : public IEntry
{
public:
	virtual void GetData(uint32_t& len, uint8_t** data) const = 0;

}; // IData

class IDataStream : public IObjectStream
{
public:
	virtual IData* GetNext() override = 0;

}; // IDataStream

class ICommand
{
public:
	virtual void Execute(const INode& n) = 0;
	virtual ~ICommand() = default;
}; // ICommand

class INearestNeighborComparator
{
public:
	virtual double GetMinimumDistance(const IShape& query, const IShape& entry) = 0;
	virtual double GetMinimumDistance(const IShape& query, const IData& data) = 0;
	virtual ~INearestNeighborComparator() = default;
}; // INearestNeighborComparator

class IStorageManager
{
public:
	virtual void LoadByteArray(const id_type id, uint32_t& len, uint8_t** data) = 0;
	virtual void StoreByteArray(id_type& id, const uint32_t len, const uint8_t* const data) = 0;
	virtual void DeleteByteArray(const id_type id) = 0;
	virtual void Flush() = 0;
	virtual ~IStorageManager() = default;
}; // IStorageManager

class IVisitor
{
public:
	virtual VisitorStatus VisitNode(const INode& in) = 0;
	virtual void VisitData(const IData& in) = 0;
	virtual void VisitData(std::vector<const IData*>& v) = 0;
	virtual ~IVisitor() = default;
}; // IVisitor

class IQueryStrategy
{
public:
	virtual void GetNextEntry(const IEntry& previouslyFetched, id_type& nextEntryToFetch, bool& bFetchNextEntry) = 0;
	virtual ~IQueryStrategy() = default;
}; // IQueryStrategy

class ISpatialIndex
{
public:
	virtual void InsertData(uint32_t len, const uint8_t* data, const IShape& shape, id_type shape_id) = 0;
	virtual bool DeleteData(const IShape& shape, id_type shape_id) = 0;
	virtual void LevelTraversal(IVisitor& v) = 0;
	virtual void InternalNodesQuery(const IShape& query, IVisitor& v) = 0;
	virtual void ContainsWhatQuery(const IShape& query, IVisitor& v)  = 0;
	virtual void IntersectsWithQuery(const IShape& query, IVisitor& v) = 0;
	virtual void PointLocationQuery(const Point& query, IVisitor& v) = 0;
	virtual void NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v, INearestNeighborComparator& nnc) = 0;
	virtual void NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v) = 0;
	virtual void SelfJoinQuery(const IShape& s, IVisitor& v) = 0;
	virtual void QueryStrategy(IQueryStrategy& qs) = 0;
	//virtual void GetIndexProperties(PropertySet& out) const = 0;
	//virtual void AddCommand(ICommand* in, CommandType ct) = 0;
	virtual bool IsIndexValid() = 0;
	//virtual void GetStatistics(IStatistics** out) const = 0;
	virtual void Flush() = 0;
	virtual ~ISpatialIndex() = default;

}; // ISpatialIndex

enum StorageManagerConstants
{
	EmptyPage = -0x1,
	NewPage = -0x1
};

}