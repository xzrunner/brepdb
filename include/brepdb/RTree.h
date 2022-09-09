#pragma once

#include "brepdb/SpatialIndex.h"

#include <memory>

namespace brepdb
{

class Node;

class RTree : public ISpatialIndex
{
public:
	RTree(const std::shared_ptr<IStorageManager>& sm, bool overwrite);
	virtual ~RTree();

	//
	// ISpatialIndex interface
	//
	virtual void InsertData(uint32_t len, const uint8_t* data, const IShape& shape, id_type shape_id) override;
	virtual bool DeleteData(const IShape& shape, id_type shape_id) override;
	virtual void LevelTraversal(IVisitor& v) override;
	virtual void InternalNodesQuery(const IShape& query, IVisitor& v) override;
	virtual void ContainsWhatQuery(const IShape& query, IVisitor& v) override;
	virtual void IntersectsWithQuery(const IShape& query, IVisitor& v) override;
	virtual void PointLocationQuery(const Point& query, IVisitor& v) override;
	virtual void NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v, INearestNeighborComparator& nnc) override;
	virtual void NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v) override;
	virtual void SelfJoinQuery(const IShape& s, IVisitor& v) override;
	virtual void QueryStrategy(IQueryStrategy& qs) override;
	//virtual void GetIndexProperties(PropertySet& out) const override;
	//virtual void AddCommand(ICommand* in, CommandType ct) override;
	virtual bool IsIndexValid() override;
	//virtual void GetStatistics(IStatistics** out) const override;
	virtual void Flush() override;

	id_type WriteNode(const Node& n);
	std::shared_ptr<Node> ReadNode(id_type page);
	void DeleteNode(const Node& n);

private:
	void InitNew();
	void InitOld();
	void StoreHeader();
	void LoadHeader();

	void insertData_impl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id);
	void insertData_impl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id, uint32_t level, uint8_t* overflow_tbl);
	bool deleteData_impl(const Region& mbr, id_type id);

	void RangeQuery(RangeQueryType type, const IShape& query, IVisitor& v);
	void SelfJoinQuery(id_type id1, id_type id2, const Region& r, IVisitor& vis);
	void VisitSubTree(const std::shared_ptr<Node>& sub_tree, IVisitor& v);

private:
	struct Statistics
	{
		uint64_t reads = 0;
		uint64_t writes = 0;
		uint64_t splits = 0;
		uint64_t hits = 0;
		uint64_t misses = 0;
		uint32_t nodes = 0;
		uint64_t adjustments = 0;
		uint64_t query_results = 0;
		uint64_t data = 0;
		uint32_t tree_height = 0;
		std::vector<uint32_t> nodes_in_level;
	};

private:
	std::shared_ptr<IStorageManager> m_storage_mgr = nullptr;

	id_type m_root_id = NewPage, m_header_id = NewPage;

	RTreeVariant m_tree_var = RV_RSTAR;

	double m_fill_factor = 0.7;

	uint32_t m_index_capacity = 10;
	uint32_t m_leaf_capacity = 10;

	uint32_t m_near_minimum_overlap_factor = 32;
	double m_split_distribution_factor = 0.4;
	double m_reinsert_factor = 0.3;

	Statistics m_stats;

	bool m_tight_mbrs = true;

	std::vector<std::shared_ptr<ICommand>> m_write_node_cmds;
	std::vector<std::shared_ptr<ICommand>> m_read_node_cmds;
	std::vector<std::shared_ptr<ICommand>> m_delete_node_cmds;

	friend class Node;
	friend class Leaf;
	friend class Index;

}; // RTree

}