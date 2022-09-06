#pragma once

#include "brepdb/Node.h"

namespace brepdb
{

class Leaf : public Node, public std::enable_shared_from_this<Leaf>
{
public:
	Leaf(RTree* tree, id_type id);

	virtual std::shared_ptr<Node> ChooseSubtree(const Region& mbr, uint32_t level, std::stack<id_type>& path_buf) override;
	virtual std::shared_ptr<Node> FindLeaf(const Region& mbr, id_type id, std::stack<id_type>& path_buf) override;

	virtual void Split(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::shared_ptr<Node>& left, std::shared_ptr<Node>& right) override;

	void DeleteData(const Region& mbr, id_type id, std::stack<id_type>& path_buf);

}; // Leaf

}
