#include "brepdb/Node.h"
#include "brepdb/Index.h"
#include "brepdb/RTree.h"
#include "brepdb/Exception.h"

#include <exception>
#include <string>
#include <cmath>

#include <assert.h>

namespace
{

using brepdb::Region;

class RStarSplitEntry
{
public:
	RStarSplitEntry(Region* pr, uint32_t index, uint32_t dimension) 
		: m_region(pr)
		, m_index(index)
		, m_sort_dim(dimension) 
	{
	}

	static int CompareLow(const void* pv1, const void* pv2)
	{
        RStarSplitEntry* pe1 = *(RStarSplitEntry**)pv1;
		RStarSplitEntry* pe2 = *(RStarSplitEntry**)pv2;

		assert(pe1->m_sort_dim == pe2->m_sort_dim);

		auto low1 = pe1->m_region->GetLow();
		auto low2 = pe2->m_region->GetLow();
		if (low1[pe1->m_sort_dim] < low2[pe2->m_sort_dim]) {
			return -1;
		}
		if (low1[pe1->m_sort_dim] > low2[pe2->m_sort_dim]) {
			return 1;
		}
		return 0;
	}

	static int CompareHigh(const void* pv1, const void* pv2)
	{
		RStarSplitEntry* pe1 = *(RStarSplitEntry**)pv1;
		RStarSplitEntry* pe2 = *(RStarSplitEntry**)pv2;

		assert(pe1->m_sort_dim == pe2->m_sort_dim);

		auto high1 = pe1->m_region->GetHigh();
		auto high2 = pe2->m_region->GetHigh();
		if (high1[pe1->m_sort_dim] < high2[pe2->m_sort_dim]) {
			return -1;
		}
		if (high1[pe1->m_sort_dim] > high2[pe2->m_sort_dim]) {
			return 1;
		}
		return 0;
	}

public:
	Region* m_region;
	uint32_t m_index;
	uint32_t m_sort_dim;

}; // RstarSplitEntry

class ReinsertEntry
{
public:
	ReinsertEntry(uint32_t index, double dist) 
		: m_index(index)
		, m_dist(dist) 
	{
	}

	static int CompareReinsertEntry(const void* pv1, const void* pv2)
	{
		ReinsertEntry* pe1 = *(ReinsertEntry**)pv1;
		ReinsertEntry* pe2 = *(ReinsertEntry**)pv2;

		if (pe1->m_dist < pe2->m_dist) {
			return -1;
		}
		if (pe1->m_dist > pe2->m_dist) {
			return 1;
		}
		return 0;
	}

public:
	uint32_t m_index;
	double m_dist;

}; // ReinsertEntry


}

namespace brepdb
{

Node::Node(RTree* tree, id_type id, uint32_t level, uint32_t capacity)
	: m_tree(tree)
	, m_level(level)
	, m_identifier(id)
	, m_capacity(capacity)
{
	m_node_mbr.MakeInfinite();

	try
	{
		m_children_id       = new id_type[m_capacity + 1];
		m_children_data     = new uint8_t * [m_capacity + 1];
		m_children_data_len = new uint32_t[m_capacity + 1];
		m_children_mbr      = new Region[m_capacity + 1];
	}
	catch (...)
	{
		delete[] m_children_id;
		delete[] m_children_data;
		delete[] m_children_data_len;
		delete[] m_children_mbr;
		throw;
	}
}

Node::~Node()
{
	if (m_children_data != nullptr)
	{
		for (int i = 0; i < m_children; ++i) {
			if (m_children_data[i] != nullptr) {
				delete[] m_children_data[i];
			}
		}

		delete[] m_children_data;
	}

	delete[] m_children_data_len;
	delete[] m_children_mbr;
	delete[] m_children_id;
}

IObject* Node::Clone()
{
	throw std::exception("IObject::clone should never be called.");
}

uint32_t Node::GetByteArraySize() const
{
	return
		sizeof(uint32_t) +
		sizeof(uint32_t) +
		sizeof(uint32_t) +
		m_children * (DIMENSION * sizeof(double) * 2 + sizeof(id_type) + sizeof(uint32_t)) +
		m_total_data_len +
		2 * DIMENSION * sizeof(double);
}

void Node::LoadFromByteArray(const uint8_t* data)
{
	m_node_mbr.MakeInfinite();

	auto ptr = data;

	ptr += sizeof(uint32_t); // node type

	memcpy(&m_level, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	memcpy(&m_children, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	for (int i = 0; i < m_children; ++i)
	{
		m_children_mbr[i].MakeInfinite();

		memcpy(const_cast<double*>(m_children_mbr[i].GetLow()), ptr, DIMENSION * sizeof(double));
		ptr += DIMENSION * sizeof(double);
		memcpy(const_cast<double*>(m_children_mbr[i].GetHigh()), ptr, DIMENSION * sizeof(double));
		ptr += DIMENSION * sizeof(double);
		memcpy(&(m_children_id[i]), ptr, sizeof(id_type));
		ptr += sizeof(id_type);

		memcpy(&(m_children_data_len[i]), ptr, sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		if (m_children_data_len[i] > 0)
		{
			m_total_data_len += m_children_data_len[i];
			m_children_data[i] = new uint8_t[m_children_data_len[i]];
			memcpy(m_children_data[i], ptr, m_children_data_len[i]);
			ptr += m_children_data_len[i];
		}
		else
		{
			m_children_data[i] = nullptr;
		}
	}

	memcpy(const_cast<double*>(m_node_mbr.GetLow()), ptr, DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(const_cast<double*>(m_node_mbr.GetHigh()), ptr, DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);
}

void Node::StoreToByteArray(uint8_t** data, uint32_t& len) const
{
	len = GetByteArraySize();

	*data = new uint8_t[len];
	uint8_t* ptr = *data;

	const uint32_t node_type = m_level == 0 ? PersistentLeaf : PersistentIndex;

	memcpy(ptr, &node_type, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	memcpy(ptr, &m_level, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	memcpy(ptr, &m_children, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	for (int i = 0; i < m_children; ++i)
	{
		memcpy(ptr, m_children_mbr[i].GetLow(), DIMENSION * sizeof(double));
		ptr += DIMENSION * sizeof(double);
		memcpy(ptr, m_children_mbr[i].GetHigh(), DIMENSION * sizeof(double));
		ptr += DIMENSION * sizeof(double);
		memcpy(ptr, &(m_children_id[i]), sizeof(id_type));
		ptr += sizeof(id_type);

		memcpy(ptr, &(m_children_data_len[i]), sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		if (m_children_data_len[i] > 0)
		{
			memcpy(ptr, m_children_data[i], m_children_data_len[i]);
			ptr += m_children_data_len[i];
		}
	}

	// store the node MBR for efficiency. This increases the node size a little bit.
	memcpy(ptr, m_node_mbr.GetLow(), DIMENSION * sizeof(double));
	ptr += DIMENSION * sizeof(double);
	memcpy(ptr, m_node_mbr.GetHigh(), DIMENSION * sizeof(double));
	//ptr += DIMENSION * sizeof(double);

	assert(len == (ptr - *data) + DIMENSION * sizeof(double));
}

id_type Node::GetIdentifier() const
{
	return m_identifier;
}

void Node::GetShape(IShape** s) const
{
	*s = new Region(m_node_mbr);
}

uint32_t Node::GetChildrenCount() const
{
	return m_children;
}

id_type Node::GetChildIdentifier(uint32_t index)  const
{
	assert(index < m_children);
	return m_children_id[index];
}

void Node::GetChildShape(uint32_t index, IShape** out)  const
{
	assert(index < m_children);
	*out = new Region(m_children_mbr[index]);
}

void Node::GetChildData(uint32_t index, uint32_t& length, uint8_t** data) const
{
	assert(index < m_children);
	if (m_children_data[index] == nullptr)
	{
		length = 0;
		data = nullptr;
	}
	else
	{
		length = m_children_data_len[index];
		*data = m_children_data[index];
	}
}

uint32_t Node::GetLevel() const
{
	return m_level;
}

bool Node::IsIndex() const
{
	return m_level == 0;
}

bool Node::IsLeaf() const
{
	return m_level != 0;
}

void Node::InsertEntry(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id)
{
	assert(m_children < m_capacity);

	m_children_data_len[m_children] = data_len;
	m_children_data[m_children] = data;
	m_children_mbr[m_children] = mbr;
	m_children_id[m_children] = id;

	m_total_data_len += data_len;
	++m_children;

	m_node_mbr.Combine(mbr);
}

void Node::DeleteEntry(uint32_t index)
{
	assert(index >= 0 && index < m_children);

	Region r = m_children_mbr[index];

	m_total_data_len -= m_children_data_len[index];
	if (m_children_data[index] != nullptr) {
		delete[] m_children_data[index];
	}

	if (m_children > 1 && index != m_children - 1)
	{
		m_children_data_len[index] = m_children_data_len[m_children - 1];
		m_children_data[index] = m_children_data[m_children - 1];
		m_children_mbr[index] = m_children_mbr[m_children - 1];
		m_children_id[index] = m_children_id[m_children - 1];
	}

	--m_children;

	if (m_children == 0)
	{
		m_node_mbr.MakeInfinite();
	}
	else if (m_tree->m_tight_mbrs && m_node_mbr.TouchesRegion(r))
	{
		m_node_mbr.MakeInfinite();
		for (int i = 0; i < m_children; ++i) {
			m_node_mbr.Combine(m_children_mbr[i]);
		}
	}
}

bool Node::InsertData(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::stack<id_type>& path_buf, uint8_t* overflow_tbl)
{
	if (m_children < m_capacity)
	{
		bool adjusted = false;

		bool b = m_node_mbr.ContainsRegion(mbr);

		InsertEntry(data_len, data, mbr, id);
		m_tree->WriteNode(*this);

		if (!b && !path_buf.empty())
		{
			id_type parent = path_buf.top(); path_buf.pop();
			std::shared_ptr<Node> n = m_tree->ReadNode(parent);
			std::static_pointer_cast<Index>(n)->AdjustTree(this, path_buf);
			adjusted = true;
		}

		return adjusted;
	}
	else if (m_tree->m_tree_var == RV_RSTAR && !path_buf.empty() && overflow_tbl[m_level] == 0)
	{
		overflow_tbl[m_level] = 1;

		std::vector<uint32_t> v_reinsert, v_keep;
		ReinsertData(data_len, data, mbr, id, v_reinsert, v_keep);

		uint32_t l_reinsert = static_cast<uint32_t>(v_reinsert.size());
		uint32_t l_keep = static_cast<uint32_t>(v_keep.size());

		uint8_t** reinsertdata = nullptr;
		Region*   reinsertmbr = nullptr;
		id_type*  reinsertid = nullptr;
		uint32_t* reinsertlen = nullptr;
		uint8_t** keepdata = nullptr;
		Region*   keepmbr = nullptr;
		id_type*  keepid = nullptr;
		uint32_t* keeplen = nullptr;

		try
		{
			reinsertdata = new uint8_t*[l_reinsert];
			reinsertmbr  = new Region[l_reinsert];
			reinsertid   = new id_type[l_reinsert];
			reinsertlen  = new uint32_t[l_reinsert];

			keepdata = new uint8_t*[m_capacity + 1];
			keepmbr  = new Region[m_capacity + 1];
			keepid   = new id_type[m_capacity + 1];
			keeplen  = new uint32_t[m_capacity + 1];
		}
		catch (...)
		{
			delete[] reinsertdata;
			delete[] reinsertmbr;
			delete[] reinsertid;
			delete[] reinsertlen;
			delete[] keepdata;
			delete[] keepmbr;
			delete[] keepid;
			delete[] keeplen;
			throw;
		}

		uint32_t c_idx;

		for (c_idx = 0; c_idx < l_reinsert; ++c_idx)
		{
			reinsertlen[c_idx]  = m_children_data_len[v_reinsert[c_idx]];
			reinsertdata[c_idx] = m_children_data[v_reinsert[c_idx]];
			reinsertmbr[c_idx]  = m_children_mbr[v_reinsert[c_idx]];
			reinsertid[c_idx]   = m_children_id[v_reinsert[c_idx]];
		}

		for (c_idx = 0; c_idx < l_keep; ++c_idx)
		{
			keeplen[c_idx]  = m_children_data_len[v_keep[c_idx]];
			keepdata[c_idx] = m_children_data[v_keep[c_idx]];
			keepmbr[c_idx]  = m_children_mbr[v_keep[c_idx]];
			keepid[c_idx]   = m_children_id[v_keep[c_idx]];
		}

		delete[] m_children_data_len;
		delete[] m_children_data;
		delete[] m_children_mbr;
		delete[] m_children_id;

		m_children_data_len = keeplen;
		m_children_data = keepdata;
		m_children_mbr = keepmbr;
		m_children_id = keepid;
		m_children = l_keep;
		m_total_data_len = 0;

		for (int i = 0; i < m_children; ++i) {
			m_total_data_len += m_children_data_len[i];
		}

		m_node_mbr.MakeInfinite();
		for (int i = 0; i < m_children; ++i) {
			m_node_mbr.Combine(m_children_mbr[i]);
		}

		m_tree->WriteNode(*this);

		// Divertion from R*-Tree algorithm here. First adjust
		// the path to the root, then start reinserts, to avoid complicated handling
		// of changes to the same node from multiple insertions.
		id_type parent = path_buf.top(); path_buf.pop();
		std::shared_ptr<Node> n = m_tree->ReadNode(parent);
		std::static_pointer_cast<Index>(n)->AdjustTree(this, path_buf, true);

		for (c_idx = 0; c_idx < l_reinsert; ++c_idx)
		{
			m_tree->insertData_impl(
				reinsertlen[c_idx], reinsertdata[c_idx],
				reinsertmbr[c_idx], reinsertid[c_idx],
				m_level, overflow_tbl);
		}

		delete[] reinsertdata;
		delete[] reinsertmbr;
		delete[] reinsertid;
		delete[] reinsertlen;

		return true;
	}
	else
	{
		std::shared_ptr<Node> n, nn;
		Split(data_len, data, mbr, id, n, nn);

		if (path_buf.empty())
		{
			n->m_level = m_level;
			nn->m_level = m_level;
			n->m_identifier = -1;
			nn->m_identifier = -1;
			m_tree->WriteNode(*n);
			m_tree->WriteNode(*nn);

			std::shared_ptr<Node> ptr_r = std::make_shared<Index>(m_tree, m_tree->m_root_id, m_level + 1);
			ptr_r->InsertEntry(0, nullptr, n->m_node_mbr, n->m_identifier);
			ptr_r->InsertEntry(0, nullptr, nn->m_node_mbr, nn->m_identifier);

			m_tree->WriteNode(*ptr_r);

			m_tree->m_stats.nodes_in_level[m_level] = 2;
			m_tree->m_stats.nodes_in_level.push_back(1);
			m_tree->m_stats.tree_height = m_level + 2;
		}
		else
		{
			n->m_level = m_level;
			nn->m_level = m_level;
			n->m_identifier = m_identifier;
			nn->m_identifier = -1;

			m_tree->WriteNode(*n);
			m_tree->WriteNode(*nn);

			id_type c_parent = path_buf.top(); path_buf.pop();
			std::shared_ptr<Node> n = m_tree->ReadNode(c_parent);
			std::static_pointer_cast<Index>(n)->AdjustTree(n.get(), nn.get(), path_buf, overflow_tbl);
		}

		return true;
	}
}

void Node::ReinsertData(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& reinsert, std::vector<uint32_t>& keep)
{

}

void Node::RTreeSplit(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& group1, std::vector<uint32_t>& group2)
{
	uint32_t minimum_load = static_cast<uint32_t>(std::floor(m_capacity * m_tree->m_fill_factor));

	// use this mask array for marking visited entries.
	uint8_t* mask = new uint8_t[m_capacity + 1];
	memset(mask, 0, m_capacity + 1);

	// insert new data in the node for easier manipulation. Data arrays are always
	// by one larger than node capacity.
	m_children_data_len[m_capacity] = data_len;
	m_children_data[m_capacity]     = data;
	m_children_mbr[m_capacity]      = mbr;
	m_children_id[m_capacity]       = id;
	// m_total_data_len does not need to be increased here.

	// initialize each group with the seed entries.
	uint32_t seed1, seed2;
	PickSeeds(seed1, seed2);

	group1.push_back(seed1);
	group2.push_back(seed2);

	mask[seed1] = 1;
	mask[seed2] = 1;

	// find MBR of each group.
	Region mbr1 = m_children_mbr[seed1];
	Region mbr2 = m_children_mbr[seed2];

	// count how many entries are left unchecked (exclude the seeds here.)
	uint32_t remaining = m_capacity + 1 - 2;

	while (remaining > 0)
	{
		if (minimum_load - group1.size() == remaining)
		{
			// all remaining entries must be assigned to group1 to comply with minimun load requirement.
			for (int i = 0; i < m_capacity + 1; ++i)
			{
				if (mask[i] == 0)
				{
					group1.push_back(i);
					mask[i] = 1;
					--remaining;
				}
			}
		}
		else if (minimum_load - group2.size() == remaining)
		{
			// all remaining entries must be assigned to group2 to comply with minimun load requirement.
			for (int i = 0; i < m_capacity + 1; ++i)
			{
				if (mask[i] == 0)
				{
					group2.push_back(i);
					mask[i] = 1;
					--remaining;
				}
			}
		}
		else
		{
			// For all remaining entries compute the difference of the cost of grouping an
			// entry in either group. When done, choose the entry that yielded the maximum
			// difference. In case of linear split, select any entry (e.g. the first one.)
			uint32_t sel;
			double md1 = 0.0, md2 = 0.0;
			double m = -std::numeric_limits<double>::max();
			const double a1 = mbr1.GetArea();
			const double a2 = mbr2.GetArea();

			for (int i = 0; i < m_capacity + 1; ++i)
			{
				if (mask[i] == 0)
				{
					Region a = mbr1, b = mbr2;
					a.Combine(m_children_mbr[i]);
					b.Combine(m_children_mbr[i]);

					double d1 = a.GetArea() - a1;
					double d2 = b.GetArea() - a2;
					double d = std::abs(d1 - d2);

					if (d > m)
					{
						m = d;
						md1 = d1; md2 = d2;
						sel = i;
						if (m_tree->m_tree_var == RV_LINEAR || m_tree->m_tree_var == RV_RSTAR) {
							break;
						}
					}
				}
			}

			// determine the group where we should add the new entry.
			int32_t group = -1;

			if (md1 < md2)
			{
				group1.push_back(sel);
				group = 1;
			}
			else if (md2 < md1)
			{
				group2.push_back(sel);
				group = 2;
			}
			else if (a1 < a2)
			{
				group1.push_back(sel);
				group = 1;
			}
			else if (a2 < a1)
			{
				group2.push_back(sel);
				group = 2;
			}
			else if (group1.size() < group2.size())
			{
				group1.push_back(sel);
				group = 1;
			}
			else if (group2.size() < group1.size())
			{
				group2.push_back(sel);
				group = 2;
			}
			else
			{
				group1.push_back(sel);
				group = 1;
			}
			mask[sel] = 1;
			--remaining;
			if (group == 1)
			{
				mbr1.Combine(m_children_mbr[sel]);
			}
			else
			{
				mbr2.Combine(m_children_mbr[sel]);
			}
		}
	}

	delete[] mask;
}

void Node::RStarSplit(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::vector<uint32_t>& group1, std::vector<uint32_t>& group2)
{
	RStarSplitEntry** data_low = nullptr;
	RStarSplitEntry** data_high = nullptr;

	try
	{
		data_low = new RStarSplitEntry*[m_capacity + 1];
		data_high = new RStarSplitEntry*[m_capacity + 1];
	}
	catch (...)
	{
		delete[] data_low;
		throw;
	}

	m_children_data_len[m_capacity] = data_len;
	m_children_data[m_capacity]     = data;
	m_children_mbr[m_capacity]      = mbr;
	m_children_id[m_capacity]       = id;
	// m_totalDataLength does not need to be increased here.

	uint32_t node_spf = static_cast<uint32_t>(std::floor((m_capacity + 1) * m_tree->m_split_distribution_factor));
	uint32_t split_distribution = (m_capacity + 1) - (2 * node_spf) + 2;

	for (int i = 0; i <= m_capacity; ++i)
	{
		try
		{
			data_low[i] = new RStarSplitEntry(&m_children_mbr[i], i, 0);
		}
		catch (...)
		{
			for (int j = 0; j < i; ++j) {
				delete data_low[j];
			}
			delete[] data_low;
			delete[] data_high;
			throw;
		}

		data_high[i] = data_low[i];
	}

	double minimum_margin = std::numeric_limits<double>::max();
	uint32_t split_axis = std::numeric_limits<uint32_t>::max();
	uint32_t sort_order = std::numeric_limits<uint32_t>::max();

	// chooseSplitAxis.
	for (int i = 0; i < DIMENSION; ++i)
	{
		::qsort(data_low, m_capacity + 1, sizeof(RStarSplitEntry*), RStarSplitEntry::CompareLow);
		::qsort(data_high, m_capacity + 1, sizeof(RStarSplitEntry*), RStarSplitEntry::CompareHigh);

		// calculate sum of margins and overlap for all distributions.
		double marginl = 0.0;
		double marginh = 0.0;

		Region bbl1, bbl2, bbh1, bbh2;

		for (int j = 1; j <= split_distribution; ++j)
		{
			uint32_t l = node_spf - 1 + j;

			bbl1 = *(data_low[0]->m_region);
			bbh1 = *(data_high[0]->m_region);

			for (int c_idx = 1; c_idx < l; ++c_idx)
			{
				bbl1.Combine(*(data_low[c_idx]->m_region));
				bbh1.Combine(*(data_high[c_idx]->m_region));
			}

			bbl2 = *(data_low[l]->m_region);
			bbh2 = *(data_high[l]->m_region);

			for (int c_idx = l + 1; c_idx <= m_capacity; ++c_idx)
			{
				bbl2.Combine(*(data_low[c_idx]->m_region));
				bbh2.Combine(*(data_high[c_idx]->m_region));
			}

			marginl += bbl1.GetMargin() + bbl2.GetMargin();
			marginh += bbh1.GetMargin() + bbh2.GetMargin();
		} // for (u32Child)

		double margin = std::min(marginl, marginh);

		// keep minimum margin as split axis.
		if (margin < minimum_margin)
		{
			minimum_margin = margin;
			split_axis = i;
			sort_order = (marginl < marginh) ? 0 : 1;
		}

		// increase the dimension according to which the data entries should be sorted.
		for (int c_idx = 0; c_idx <= m_capacity; ++c_idx)
		{
			data_low[c_idx]->m_sort_dim = i + 1;
		}
	} // for (cDim)

	for (int c_idx = 0; c_idx <= m_capacity; ++c_idx)
	{
		data_low[c_idx]->m_sort_dim = split_axis;
	}

	::qsort(data_low, m_capacity + 1, sizeof(RStarSplitEntry*), (sort_order == 0) ? RStarSplitEntry::CompareLow : RStarSplitEntry::CompareHigh);

	double ma = std::numeric_limits<double>::max();
	double mo = std::numeric_limits<double>::max();
	uint32_t split_point = std::numeric_limits<uint32_t>::max();

	Region bb1, bb2;

	for (int i = 1; i <= split_distribution; ++i)
	{
		uint32_t l = node_spf - 1 + i;

		bb1 = *(data_low[0]->m_region);

		for (int c_idx = 1; c_idx < l; ++c_idx)
		{
			bb1.Combine(*(data_low[c_idx]->m_region));
		}

		bb2 = *(data_low[l]->m_region);

		for (int c_idx = l + 1; c_idx <= m_capacity; ++c_idx)
		{
			bb2.Combine(*(data_low[c_idx]->m_region));
		}

		double o = bb1.GetIntersectingArea(bb2);

		if (o < mo)
		{
			split_point = i;
			mo = o;
			ma = bb1.GetArea() + bb2.GetArea();
		}
		else if (o == mo)
		{
			double a = bb1.GetArea() + bb2.GetArea();

			if (a < ma)
			{
				split_point = i;
				ma = a;
			}
		}
	} // for (u32Child)

	uint32_t l1 = node_spf - 1 + split_point;

	for (int c_idx = 0; c_idx < l1; ++c_idx)
	{
		group1.push_back(data_low[c_idx]->m_index);
		delete data_low[c_idx];
	}

	for (int c_idx = l1; c_idx <= m_capacity; ++c_idx)
	{
		group2.push_back(data_low[c_idx]->m_index);
		delete data_low[c_idx];
	}

	delete[] data_low;
	delete[] data_high;
}

void Node::PickSeeds(uint32_t& index1, uint32_t& index2)
{
	double separation = -std::numeric_limits<double>::max();
	double inefficiency = -std::numeric_limits<double>::max();

	switch (m_tree->m_tree_var)
	{
	case RV_LINEAR:
	case RV_RSTAR:
		for (int dim = 0; dim < DIMENSION; ++dim)
		{
			double least_lower = m_children_mbr[0].GetLow()[dim];
			double greatest_upper = m_children_mbr[0].GetHigh()[dim];
			uint32_t greatest_lower = 0;
			uint32_t least_upper = 0;
			double width;

			for (int i = 1; i <= m_capacity; ++i)
			{
				if (m_children_mbr[i].GetLow()[dim] > m_children_mbr[greatest_lower].GetLow()[dim]) {
					greatest_lower = i;
				}
				if (m_children_mbr[i].GetHigh()[dim] < m_children_mbr[least_upper].GetHigh()[dim]) {
					least_upper = i;
				}

				least_lower = std::min(m_children_mbr[i].GetLow()[dim], least_lower);
				greatest_upper = std::max(m_children_mbr[i].GetHigh()[dim], greatest_upper);
			}

			width = greatest_upper - least_lower;
			if (width <= 0) {
				width = 1;
			}

			double f = (m_children_mbr[greatest_lower].GetLow()[dim] - m_children_mbr[least_upper].GetHigh()[dim]) / width;

			if (f > separation)
			{
				index1 = least_upper;
				index2 = greatest_lower;
				separation = f;
			}
		}

		if (index1 == index2)
		{
			if (index2 == 0) ++index2;
			else --index2;
		}

		break;
	case RV_QUADRATIC:
		// for each pair of Regions (account for overflow Region too!)
		for (int i = 0; i < m_capacity; ++i)
		{
			double a = m_children_mbr[i].GetArea();

			for (int j = i + 1; j <= m_capacity; ++j)
			{
				// get the combined MBR of those two entries.
				Region r = m_children_mbr[i];
				r.Combine(m_children_mbr[j]);

				// find the inefficiency of grouping these entries together.
				double d = r.GetArea() - a - m_children_mbr[j].GetArea();

				if (d > inefficiency)
				{
					inefficiency = d;
					index1 = i;
					index2 = j;
				}
			}
		} 

		break;
	default:
		throw NotSupportedException("Node::pickSeeds: Tree variant not supported.");
	}
}

void Node::CondenseTree(std::stack<std::shared_ptr<Node>>& to_reinsert, std::stack<id_type>& path_buf, std::shared_ptr<Node>& ptr_this)
{
	uint32_t minimum_load = static_cast<uint32_t>(std::floor(m_capacity * m_tree->m_fill_factor));

	if (path_buf.empty())
	{
		// eliminate root if it has only one child.
		if (m_level != 0 && m_children == 1)
		{
			std::shared_ptr<Node> node = m_tree->ReadNode(m_children_id[0]);
			m_tree->DeleteNode(*node);
			node->m_identifier = m_tree->m_root_id;
			m_tree->WriteNode(*node);

			m_tree->m_stats.nodes_in_level.pop_back();
			m_tree->m_stats.tree_height -= 1;
			// HACK: pending deleteNode for deleted child will decrease nodesInLevel, later on.
			m_tree->m_stats.nodes_in_level[m_tree->m_stats.tree_height - 1] = 2;
		}
		else
		{
			// due to data removal.
			if (m_tree->m_tight_mbrs)
			{
				m_node_mbr.MakeInfinite();
				for (int i = 0; i < m_children; ++i) {
					m_node_mbr.Combine(m_children_mbr[i]);
				}
			}

            // write parent node back to storage.
			m_tree->WriteNode(*this);
		}
	}
	else
	{
		id_type c_parent = path_buf.top(); path_buf.pop();
		std::shared_ptr<Node> parent = m_tree->ReadNode(c_parent);

		// find the entry in the parent, that points to this node.
		uint32_t child;

		for (child = 0; child != parent->m_children; ++child)
		{
			if (parent->m_children_id[child] == m_identifier) {
				break;
			}
		}

		if (m_children < minimum_load)
		{
			// used space less than the minimum
			// 1. eliminate node entry from the parent. deleteEntry will fix the parent's MBR.
			parent->DeleteEntry(child);
			// 2. add this node to the stack in order to reinsert its entries.
			to_reinsert.push(ptr_this);
		}
		else
		{
			// adjust the entry in 'p' to contain the new bounding region of this node.
			parent->m_children_mbr[child] = m_node_mbr;

			// global recalculation necessary since the MBR can only shrink in size,
			// due to data removal.
			if (m_tree->m_tight_mbrs)
			{
				parent->m_node_mbr.MakeInfinite();
				for (int i = 0; i < parent->m_children; ++i) {
					parent->m_node_mbr.Combine(parent->m_children_mbr[i]);
				}
			}
		}

		// write parent node back to storage.
		m_tree->WriteNode(*parent);

		parent->CondenseTree(to_reinsert, path_buf, parent);
	}
}

}