#include "brepdb/Index.h"
#include "brepdb/RTree.h"
#include "brepdb/Exception.h"

#include <assert.h>

namespace brepdb
{

Index::Index(RTree* tree, id_type id, uint32_t level) 
	: Node(tree, id, level, tree->m_index_capacity)
{
}

std::shared_ptr<Node> Index::ChooseSubtree(const Region& mbr, uint32_t level, std::stack<id_type>& path_buf)
{
	if (m_level == level) {
		return shared_from_this();
	}

	path_buf.push(m_identifier);

	uint32_t child = 0;

	switch (m_tree->m_tree_var)
	{
		case RV_LINEAR:
		case RV_QUADRATIC:
			child = FindLeastEnlargement(mbr);
			break;
		case RV_RSTAR:
			if (m_level == 1)
			{
				// if this node points to leaves...
				child = FindLeastOverlap(mbr);
			}
			else
			{
				child = FindLeastEnlargement(mbr);
			}
		break;
		default:
			throw NotSupportedException("Index::chooseSubtree: Tree variant not supported.");
	}
	assert(child != std::numeric_limits<uint32_t>::max());

	std::shared_ptr<Node> n = m_tree->ReadNode(m_children_id[child]);
	std::shared_ptr<Node> ret = n->ChooseSubtree(mbr, level, path_buf);
	assert(n.unique());

	return ret;
}

std::shared_ptr<Node> Index::FindLeaf(const Region& mbr, id_type id, std::stack<id_type>& path_buf)
{
	path_buf.push(m_identifier);

	for (int i = 0; i < m_children; ++i)
	{
		if (m_children_mbr[i].ContainsRegion(mbr))
		{
			std::shared_ptr<Node> n = m_tree->ReadNode(m_children_id[i]);
			std::shared_ptr<Node> l = n->FindLeaf(mbr, id, path_buf);
			if (l.get() != nullptr) {
				return l;
			}
		}
	}

	path_buf.pop();

	return nullptr;
}

void Index::Split(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::shared_ptr<Node>& left, std::shared_ptr<Node>& right)
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
			throw NotSupportedException("Index::split: Tree variant not supported.");
	}

	auto l = std::make_shared<Index>(m_tree, m_identifier, m_level);
	auto r = std::make_shared<Index>(m_tree, -1, m_level);

	l->m_node_mbr.MakeInfinite();
	r->m_node_mbr.MakeInfinite();

	uint32_t c_idx;
	for (c_idx = 0; c_idx < g1.size(); ++c_idx) {
		l->InsertEntry(0, nullptr, m_children_mbr[g1[c_idx]], m_children_id[g1[c_idx]]);
	}
	for (c_idx = 0; c_idx < g2.size(); ++c_idx) {
		r->InsertEntry(0, nullptr, m_children_mbr[g2[c_idx]], m_children_id[g2[c_idx]]);
	}

	left = l;
	right = r;
}


void Index::AdjustTree(const Node* n, std::stack<id_type>& path_buf, bool force)
{
	++m_tree->m_stats.adjustments;

	// find entry pointing to old node;
	uint32_t child;
	for (child = 0; child < m_children; ++child) {
		if (m_children_id[child] == n->m_identifier) {
			break;
		}
	}

	// MBR needs recalculation if either:
	//   1. the NEW child MBR is not contained.
	//   2. the OLD child MBR is touching.
	bool bContained = m_node_mbr.ContainsRegion(n->m_node_mbr);
	bool bTouches = m_node_mbr.TouchesRegion(m_children_mbr[child]);
	bool bRecompute = !bContained || (bTouches && m_tree->m_tight_mbrs);

	m_children_mbr[child] = n->m_node_mbr;

	if (bRecompute || force)
	{
		m_node_mbr.MakeInfinite();
		for (int i = 0; i < m_children; ++i) {
			m_node_mbr.Combine(m_children_mbr[i]);
		}
	}

	m_tree->WriteNode(*this);

	if ((bRecompute || force) && (!path_buf.empty()))
	{
		id_type parent = path_buf.top(); path_buf.pop();
		std::shared_ptr<Node> n = m_tree->ReadNode(parent);
		std::static_pointer_cast<Index>(n)->AdjustTree(this, path_buf, force);
	}
}

void Index::AdjustTree(const Node* n1, const Node* n2, std::stack<id_type>& path_buf, uint8_t* overflow_tbl)
{
	++m_tree->m_stats.adjustments;

	// find entry pointing to old node;
	uint32_t child;
	for (child = 0; child < m_children; ++child) {
		if (m_children_id[child] == n1->m_identifier) {
			break;
		}
	}

	// MBR needs recalculation if either:
	//   1. either child MBR is not contained.
	//   2. the OLD child MBR is touching.
	bool contained1 = m_node_mbr.ContainsRegion(n1->m_node_mbr);
	bool contained2 = m_node_mbr.ContainsRegion(n2->m_node_mbr);
	bool contained = contained1 && contained2;
	bool touches = m_node_mbr.TouchesRegion(m_children_mbr[child]);
	bool recompute = !contained || (touches && m_tree->m_tight_mbrs);

	m_children_mbr[child] = n1->m_node_mbr;

	if (recompute)
	{
		m_node_mbr.MakeInfinite();
		for (int i = 0; i < m_children; ++i) {
			m_node_mbr.Combine(m_children_mbr[i]);
		}
	}

	// No write necessary here. insertData will write the node if needed.
	//m_tree->writeNode(this);

	bool adjusted = InsertData(0, nullptr, n2->m_node_mbr, n2->m_identifier, path_buf, overflow_tbl);

	// if n2 is contained in the node and there was no split or reinsert,
	// we need to adjust only if recalculation took place.
	// In all other cases insertData above took care of adjustment.
	if (!adjusted && recompute && !path_buf.empty())
	{
		id_type parent = path_buf.top(); path_buf.pop();
		std::shared_ptr<Node> n = m_tree->ReadNode(parent);
		std::static_pointer_cast<Index>(n)->AdjustTree(this, path_buf);
	}
}

uint32_t Index::FindLeastEnlargement(const Region& r) const
{
	double area = std::numeric_limits<double>::infinity();
	uint32_t best = std::numeric_limits<uint32_t>::max();
	for (uint32_t i = 0; i < m_children; ++i)
	{
		Region t = m_children_mbr[i];
		t.Combine(r);

		double a = m_children_mbr[i].GetArea();
		double enl = t.GetArea() - a;

		if (enl < area)
		{
			area = enl;
			best = i;
		}
		else if (enl == area)
		{
			// this will rarely happen, so compute best area on the fly only
			// when necessary.
			if (enl == std::numeric_limits<double>::infinity() || 
				a < m_children_mbr[best].GetArea()) {
				best = i;
			}
		}
	}

	return best;
}

uint32_t Index::FindLeastOverlap(const Region& r) const
{
	OverlapEntry** entries = new OverlapEntry*[m_children];

	double least_overlap = std::numeric_limits<double>::max();
	double me = std::numeric_limits<double>::max();
	OverlapEntry* best = nullptr;

	// find combined region and enlargement of every entry and store it.
	for (int i = 0; i < m_children; ++i)
	{
		try
		{
			entries[i] = new OverlapEntry();
		}
		catch (...)
		{
			for (int j = 0; j < i; ++j) {
				delete entries[j];
			}
			delete[] entries;
			throw;
		}

		entries[i]->m_index = i;
		entries[i]->m_original = m_children_mbr[i];
		entries[i]->m_combined = m_children_mbr[i];
		entries[i]->m_combined.Combine(r);
		entries[i]->m_oa = entries[i]->m_original.GetArea();
		entries[i]->m_ca = entries[i]->m_combined.GetArea();
		entries[i]->m_enlargement = entries[i]->m_ca - entries[i]->m_oa;

		if (entries[i]->m_enlargement < me)
		{
			me = entries[i]->m_enlargement;
			best = entries[i];
		}
		else if (entries[i]->m_enlargement == me && entries[i]->m_oa < best->m_oa)
		{
			best = entries[i];
		}
	}

	if (me < -std::numeric_limits<double>::epsilon() || 
		me > std::numeric_limits<double>::epsilon())
	{
		uint32_t c_itr;

		if (m_children > m_tree->m_near_minimum_overlap_factor)
		{
			// sort entries in increasing order of enlargement.
			::qsort(entries, m_children, sizeof(OverlapEntry*), OverlapEntry::CompareEntries);
			assert(entries[0]->m_enlargement <= entries[m_children - 1]->m_enlargement);

			c_itr = m_tree->m_near_minimum_overlap_factor;
		}
		else
		{
			c_itr = m_children;
		}

		// calculate overlap of most important original entries (near minimum overlap cost).
		for (int c_idx = 0; c_idx < c_itr; ++c_idx)
		{
			double dif = 0.0;
			OverlapEntry* e = entries[c_idx];

			for (int c_child = 0; c_child < m_children; ++c_child)
			{
				if (e->m_index != c_child)
				{
					double f = e->m_combined.GetIntersectingArea(m_children_mbr[c_child]);
					if (f != 0.0) {
						dif += f - e->m_original.GetIntersectingArea(m_children_mbr[c_child]);
					}
				}
			}

			if (dif < least_overlap)
			{
				least_overlap = dif;
				best = entries[c_idx];
			}
			else if (dif == least_overlap)
			{
				if (e->m_enlargement == best->m_enlargement)
				{
					// keep the one with least area.
					if (e->m_original.GetArea() < best->m_original.GetArea()) {
						best = entries[c_idx];
					}
				}
				else
				{
					// keep the one with least enlargement.
					if (e->m_enlargement < best->m_enlargement) best = entries[c_idx];
				}
			}
		}
	}

	uint32_t ret = best->m_index;

	for (uint32_t i = 0; i < m_children; ++i) {
		delete entries[i];
	}
	delete[] entries;

	return ret;
}

}