#pragma once

#include "brepdb/SpatialIndex.h"
#include "brepdb/Region.h"

#include <stack>
#include <memory>

namespace brepdb
{

class RTree;

class Node : public INode
{
public:
	virtual ~Node() override;

	//
	// IObject interface
	//
	virtual IObject* Clone() override;

	//
	// ISerializable interface
	//
	virtual uint32_t GetByteArraySize() const override;
	virtual void LoadFromByteArray(const uint8_t* data) override;
	virtual void StoreToByteArray(uint8_t** data, uint32_t& len) const override;

	//
	// IEntry interface
	//
	virtual id_type GetIdentifier() const override;
	virtual void GetShape(IShape** s) const override;

	//
	// INode interface
	//
	virtual uint32_t GetChildrenCount() const override;
	virtual id_type GetChildIdentifier(uint32_t index)  const override;
	virtual void GetChildShape(uint32_t index, IShape** out)  const override;
	virtual void GetChildData(uint32_t index, uint32_t& length, uint8_t** data) const override;
	virtual uint32_t GetLevel() const override;
	virtual bool IsIndex() const override;
	virtual bool IsLeaf() const override;

	virtual std::shared_ptr<Node> ChooseSubtree(const Region& mbr, uint32_t level, std::stack<id_type>& path_buf) = 0;
	virtual std::shared_ptr<Node> FindLeaf(const Region& mbr, id_type id, std::stack<id_type>& path_buf) = 0;

	virtual void Split(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::shared_ptr<Node>& left, std::shared_ptr<Node>& right) = 0;

	auto& GetRegion() const { return m_node_mbr; }

protected:
	Node(RTree* tree, id_type id, uint32_t level, uint32_t capacity);

	void InsertEntry(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id);
	void DeleteEntry(uint32_t index);

	bool InsertData(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::stack<id_type>& path_buf, uint8_t* overflow_tbl);
	void ReinsertData(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& reinsert, std::vector<uint32_t>& keep);

	void RTreeSplit(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& group1, std::vector<uint32_t>& group2);
	void RStarSplit(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& group1, std::vector<uint32_t>& group2);

	void PickSeeds(uint32_t& index1, uint32_t& index2);

	void CondenseTree(std::stack<std::shared_ptr<Node>>& to_reinsert, std::stack<id_type>& path_buf, std::shared_ptr<Node>& ptr_this);

protected:
	RTree* m_tree = nullptr;

	uint32_t m_level = 0;
	id_type  m_identifier = -1;
	uint32_t m_children = 0;
	uint32_t m_capacity = 0;

	Region m_node_mbr;

	// children
	uint32_t* m_children_data_len = nullptr;
	uint8_t** m_children_data     = nullptr;
	Region*   m_children_mbr      = nullptr;
	id_type*  m_children_id       = nullptr;

	uint32_t m_total_data_len = 0;

	friend class RTree;
	friend class Index;
	friend class Leaf;

}; // Node

}