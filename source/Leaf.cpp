#include "brepdb/Leaf.h"
#include "brepdb/RTree.h"
#include "brepdb/Exception.h"

namespace brepdb
{

Leaf::Leaf(RTree* tree, id_type id)
	: Node(tree, id, 0, tree->m_leaf_capacity)
{
}

std::shared_ptr<Node> Leaf::ChooseSubtree(const Region& mbr, uint32_t level, std::stack<id_type>& path_buf)
{
	return shared_from_this();
}

std::shared_ptr<Node> Leaf::FindLeaf(const Region& mbr, id_type id, std::stack<id_type>& path_buf)
{
	for (int i = 0; i < m_children; ++i) {
		if (m_children_id[i] == id && mbr == m_children_mbr[i]) {
			return shared_from_this();
		}
	}

	return nullptr;
}

void Leaf::Split(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::shared_ptr<Node>& left, std::shared_ptr<Node>& right)
{
	++m_tree->m_stats.splits;

	std::vector<uint32_t> g1, g2;

	switch (m_tree->m_tree_var)
	{
		case RV_LINEAR:
		case RV_QUADRATIC:
			RTreeSplit(data_len, data, mbr, id, g1, g2);
			break;
		case RV_RSTAR:
			RStarSplit(data_len, data, mbr, id, g1, g2);
			break;
		default:
			throw NotSupportedException("Leaf::split: Tree variant not supported.");
	}

	auto l = std::make_shared<Leaf>(m_tree, -1);
	auto r = std::make_shared<Leaf>(m_tree, -1);

	l->m_node_mbr.MakeInfinite();
	r->m_node_mbr.MakeInfinite();

	uint32_t c_idx;

	for (c_idx = 0; c_idx < g1.size(); ++c_idx)
	{
		l->InsertEntry(m_children_data_len[g1[c_idx]], m_children_data[g1[c_idx]], m_children_mbr[g1[c_idx]], m_children_id[g1[c_idx]]);
		// we don't want to delete the data array from this node's destructor!
		m_children_data[g1[c_idx]] = nullptr;
	}

	for (c_idx = 0; c_idx < g2.size(); ++c_idx)
	{
		r->InsertEntry(m_children_data_len[g2[c_idx]], m_children_data[g2[c_idx]], m_children_mbr[g2[c_idx]], m_children_id[g2[c_idx]]);
		// we don't want to delete the data array from this node's destructor!
		m_children_data[g2[c_idx]] = nullptr;
	}

	left = l;
	right = r;
}

void Leaf::DeleteData(const Region& mbr, id_type id, std::stack<id_type>& path_buf)
{
	uint32_t child;
	for (child = 0; child < m_children; ++child)
	{
		if (m_children_id[child] == id && mbr == m_children_mbr[child]) {
			break;
		}
	}

	DeleteEntry(child);
	m_tree->WriteNode(*this);

	std::stack<std::shared_ptr<Node>> to_reinsert;
	std::shared_ptr<Node> p_this = shared_from_this();
	CondenseTree(to_reinsert, path_buf, p_this);

	// re-insert eliminated nodes.
	while (!to_reinsert.empty())
	{
		std::shared_ptr<Node> n = to_reinsert.top(); to_reinsert.pop();
		m_tree->DeleteNode(*n);

		for (uint32_t c_child = 0; c_child < n->m_children; ++c_child)
		{
			// keep this in the for loop. The tree height might change after insertions.
			uint8_t* overflowTable = new uint8_t[m_tree->m_stats.tree_height];
			memset(overflowTable, 0, m_tree->m_stats.tree_height);
			m_tree->insertData_impl(n->m_children_data_len[c_child], n->m_children_data[c_child], n->m_children_mbr[c_child], n->m_children_id[c_child], n->m_level, overflowTable);
			n->m_children_data[c_child] = nullptr;
			delete[] overflowTable;
		}
	}
}

}