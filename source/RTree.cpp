#include "brepdb/RTree.h"
#include "brepdb/Node.h"
#include "brepdb/Index.h"
#include "brepdb/Leaf.h"
#include "brepdb/Exception.h"
#include "brepdb/IdVisitor.h"

#include <iostream>
#include <queue>
#include <map>

#include <assert.h>

namespace
{

using namespace brepdb;

class Data : public IData, public ISerializable
{
public:
	Data(uint32_t len, uint8_t* data, Region& r, id_type id)
		: m_id(id)
		, m_region(r)
		, m_data(nullptr)
		, m_data_len(len)
	{
		if (m_data_len > 0)
		{
			m_data = new uint8_t[m_data_len];
			memcpy(m_data, data, m_data_len);
		}
	}
	virtual ~Data() 
	{
		delete[] m_data;
	}

	//
	// IObject interface
	//
	virtual Data* Clone() override
	{
		return new Data(m_data_len, m_data, m_region, m_id);
	}

	//
	// IEntry interface
	//
	virtual id_type GetIdentifier() const override
	{
		return m_id;
	}
	virtual void GetShape(IShape** out) const override
	{
		*out = new Region(m_region);
	}

	//
	// IData interface
	//
	virtual void GetData(uint32_t& len, uint8_t** data) const override
	{
		len = m_data_len;
		*data = nullptr;

		if (m_data_len > 0)
		{
			*data = new uint8_t[m_data_len];
			memcpy(*data, m_data, m_data_len);
		}
	}

	//
	// ISerializable interface
	//
	virtual uint32_t GetByteArraySize() const override
	{
		return
			sizeof(id_type) +
			sizeof(uint32_t) +
			m_data_len +
			m_region.GetByteArraySize();
	}
	virtual void LoadFromByteArray(const uint8_t* data) override
	{
		auto ptr = data;

		memcpy(&m_id, ptr, sizeof(id_type));
		ptr += sizeof(id_type);

		delete[] m_data;
		m_data = nullptr;

		memcpy(&m_data_len, ptr, sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		if (m_data_len > 0)
		{
			m_data = new uint8_t[m_data_len];
			memcpy(m_data, ptr, m_data_len);
			ptr += m_data_len;
		}

		m_region.LoadFromByteArray(ptr);
	}
	virtual void StoreToByteArray(uint8_t** data, uint32_t& len) const override
	{
		// it is thread safe this way.
		uint32_t regionsize;
		uint8_t* regiondata = nullptr;
		m_region.StoreToByteArray(&regiondata, regionsize);

		len = sizeof(id_type) + sizeof(uint32_t) + m_data_len + regionsize;

		*data = new uint8_t[len];
		uint8_t* ptr = *data;

		memcpy(ptr, &m_id, sizeof(id_type));
		ptr += sizeof(id_type);
		memcpy(ptr, &m_data_len, sizeof(uint32_t));
		ptr += sizeof(uint32_t);

		if (m_data_len > 0)
		{
			memcpy(ptr, m_data, m_data_len);
			ptr += m_data_len;
		}

		memcpy(ptr, regiondata, regionsize);
		delete[] regiondata;
		// ptr += regionsize;
	}

private:
	id_type m_id;
	Region m_region;
	uint8_t* m_data = nullptr;
	uint32_t m_data_len = 0;

}; // Data

class NNEntry
{
public:
	id_type m_id;
	IEntry* m_entry;
	double m_min_dist;

	NNEntry(id_type id, IEntry* e, double f) 
		: m_id(id)
		, m_entry(e)
		, m_min_dist(f) 
	{
	}
	~NNEntry() = default;

}; // NNEntry

class NNComparator : public INearestNeighborComparator
{
public:
	double GetMinimumDistance(const IShape& query, const IShape& entry) 
	{
		return query.GetMinimumDistance(entry);
	}

	double GetMinimumDistance(const IShape& query, const IData& data) 
	{
		IShape* pS;
		data.GetShape(&pS);
		double ret = query.GetMinimumDistance(*pS);
		delete pS;
		return ret;
	}
}; // NNComparator

}

namespace brepdb
{

RTree::RTree(const std::shared_ptr<IStorageManager>& sm, bool overwrite)
	: m_storage_mgr(sm)
{
	if (overwrite) {
		InitNew();
	} else {
		InitOld();
	}
}

RTree::~RTree()
{
	StoreHeader();
}

void RTree::InsertData(uint32_t len, const uint8_t* data, const IShape& shape, id_type shape_id)
{
	// convert the shape into a Region (R-Trees index regions only; i.e., approximations of the shapes).
	Region mbr;
	shape.GetMBR(mbr);

	uint8_t* buffer = nullptr;
	if (len > 0)
	{
		buffer = new uint8_t[len];
		memcpy(buffer, data, len);
	}

	InsertDataImpl(len, buffer, mbr, shape_id);
		// the buffer is stored in the tree. Do not delete here.
}

bool RTree::DeleteData(const IShape& shape, id_type shape_id)
{
	Region mbr;
	shape.GetMBR(mbr);

	return DeleteDataImpl(mbr, shape_id);
}

void RTree::LevelTraversal(IVisitor& v)
{
	try
	{
		std::stack<std::shared_ptr<Node>> st;
		std::shared_ptr<Node> root = ReadNode(m_root_id);
		st.push(root);

		while (!st.empty())
		{
			std::shared_ptr<Node> n = st.top(); st.pop();

			VisitorStatus status = v.VisitNode(*n);

			if (n->IsIndex())
			{
				if (status == VisitorStatus::Continue)
				{
					for (int i = 0; i < n->m_children; ++i) {
						st.push(ReadNode(n->m_children_id[i]));
					}
				}
			}
		}

	}
	catch (...)
	{ 
		throw;
	}
}

void RTree::InternalNodesQuery(const IShape& query, IVisitor& v)
{
#ifdef HAVE_PTHREAD_H
	Tools::LockGuard lock(&m_lock);
#endif

	try
	{
		std::stack<std::shared_ptr<Node>> st;
		std::shared_ptr<Node> root = ReadNode(m_root_id);
		st.push(root);

		while (!st.empty())
		{
			std::shared_ptr<Node> n = st.top(); st.pop();

			if (query.ContainsShape(n->m_node_mbr))
			{
				IdVisitor v_id = IdVisitor();
				VisitSubTree(n, v_id);
				const uint64_t n_obj = v_id.GetResultCount();
				uint64_t* obj = new uint64_t[n_obj];
				std::copy(v_id.GetResults().begin(), v_id.GetResults().end(), obj);

				Data data = Data((uint32_t)(sizeof(uint64_t) * n_obj), (uint8_t*)obj, n->m_node_mbr, n->GetIdentifier());
				v.VisitData(data);
				++m_stats.query_results;
			}
			else
			{
				if (n->m_level == 0)
				{
					for (int i = 0; i < n->m_children; ++i)
					{
						if (query.ContainsShape(n->m_children_mbr[i]))
						{
							Data data = Data(sizeof(id_type), (uint8_t*)&n->m_children_id[i], n->m_children_mbr[i], n->GetIdentifier());
							v.VisitData(data);
							++m_stats.query_results;
						}
					}
				}
				else
				{
					if (query.IntersectsShape(n->m_node_mbr))
					{
						for (int i = 0; i < n->m_children; ++i) {
							st.push(ReadNode(n->m_children_id[i]));
						}
					}
				}
			}
		}

	}
	catch (...)
	{
		throw;
	}
}

void RTree::ContainsWhatQuery(const IShape& query, IVisitor& v)
{
	try
	{
		std::stack<std::shared_ptr<Node>> st;
		std::shared_ptr<Node> root = ReadNode(m_root_id);
		st.push(root);

		while (!st.empty())
		{
			std::shared_ptr<Node> n = st.top(); st.pop();

			if (n->m_level == 0)
			{
				v.VisitNode(*n);

				for (int i = 0; i < n->m_children; ++i)
				{
					if (query.ContainsShape(n->m_children_mbr[i]))
					{
						Data data = Data(n->m_children_data_len[i], n->m_children_data[i], n->m_children_mbr[i], n->m_children_id[i]);
						v.VisitData(data);
						++m_stats.query_results;
					}
				}
			}
			else
			{
				if (query.ContainsShape(n->m_node_mbr))
				{
					VisitSubTree(n, v);
				}
				else if (query.IntersectsShape(n->m_node_mbr))
				{
					VisitorStatus status = v.VisitNode(*n);
					if (status == VisitorStatus::Continue)
					{
						for (int i = 0; i < n->m_children; ++i) {
							if (query.IntersectsShape(n->m_children_mbr[i])) {
								st.push(ReadNode(n->m_children_id[i]));
							}
						}
					}
				}
			}
		}

	}
	catch (...)
	{
		throw;
	}
}

void RTree::IntersectsWithQuery(const IShape& query, IVisitor& v)
{
	RangeQuery(IntersectionQuery, query, v);
}

void RTree::PointLocationQuery(const Point& query, IVisitor& v)
{
	Region r(query, query);
	RangeQuery(IntersectionQuery, r, v);
}

void RTree::NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v, INearestNeighborComparator& nnc)
{
	auto ascending = [](const NNEntry* lhs, const NNEntry* rhs) 
	{ 
		return lhs->m_min_dist > rhs->m_min_dist;  
	};
	std::priority_queue<NNEntry*, std::vector<NNEntry*>, decltype(ascending)> queue(ascending);

	queue.push(new NNEntry(m_root_id, nullptr, 0.0));

	uint32_t count = 0;
	double knearest = 0.0;

	while (!queue.empty())
	{
		NNEntry* pFirst = queue.top();

		// report all nearest neighbors with equal greatest distances.
		// (neighbors can be more than k, if many happen to have the same greatest distance).
		if (count >= k && pFirst->m_min_dist > knearest) {
			break;
		}

		queue.pop();

		if (pFirst->m_entry == nullptr)
		{
			// n is a leaf or an index.
			std::shared_ptr<Node> n = ReadNode(pFirst->m_id);

			VisitorStatus status = v.VisitNode(*n);

			if (status == VisitorStatus::Continue)
			{
				for (int i = 0; i < n->m_children; ++i)
				{
					if (n->m_level == 0)
					{
						Data* e = new Data(n->m_children_data_len[i], n->m_children_data[i], n->m_children_mbr[i], n->m_children_id[i]);
						// we need to compare the query with the actual data entry here, so we call the
						// appropriate getMinimumDistance method of NearestNeighborComparator.
						queue.push(new NNEntry(n->m_children_id[i], e, nnc.GetMinimumDistance(query, *e)));
					}
					else
					{
						queue.push(new NNEntry(n->m_children_id[i], nullptr, nnc.GetMinimumDistance(query, n->m_children_mbr[i])));
					}
				}
			}
		}
		else
		{
			v.VisitData(*(static_cast<IData*>(pFirst->m_entry)));
			++m_stats.query_results;
			++count;
			knearest = pFirst->m_min_dist;
			delete pFirst->m_entry;
		}

		delete pFirst;
	}

	while (!queue.empty())
	{
		NNEntry* e = queue.top(); queue.pop();
		if (e->m_entry != nullptr) delete e->m_entry;
		delete e;
	}
}

void RTree::NearestNeighborQuery(uint32_t k, const IShape& query, IVisitor& v)
{
	NNComparator nnc;
	NearestNeighborQuery(k, query, v, nnc);
}

void RTree::SelfJoinQuery(const IShape& query, IVisitor& v)
{
	Region mbr;
	query.GetMBR(mbr);
	
	SelfJoinQuery(m_root_id, m_root_id, mbr, v);
}

void RTree::QueryStrategy(IQueryStrategy& qs)
{
	id_type next = m_root_id;

	bool has_next = true;
	while (has_next)
	{
		std::shared_ptr<Node> n = ReadNode(next);
		qs.GetNextEntry(*n, next, has_next);
	}
}

//void GetIndexProperties(PropertySet& out) const;
//void AddCommand(ICommand* in, CommandType ct);

bool RTree::IsIndexValid()
{
	class ValidateEntry
	{
	public:
		ValidateEntry(Region& r, const std::shared_ptr<Node>& node) 
			: m_parent_mbr(r)
			, m_node(node)
		{
		}

		Region m_parent_mbr;
		std::shared_ptr<Node> m_node = nullptr;

	}; // ValidateEntry

	bool ret = true;
	std::stack<ValidateEntry> st;
	std::shared_ptr<Node> root = ReadNode(m_root_id);

	if (root->m_level != m_stats.tree_height - 1)
	{
		std::cerr << "Invalid tree height." << std::endl;
		return false;
	}

	std::map<uint32_t, uint32_t> nodes_in_level;
	nodes_in_level.insert(std::pair<uint32_t, uint32_t>(root->m_level, 1));

	ValidateEntry e(root->m_node_mbr, root);
	st.push(e);

	while (!st.empty())
	{
		e = st.top(); st.pop();

		Region tmp_region;
		for (int dim = 0; dim < DIMENSION; ++dim) {
			for (int i = 0; i < e.m_node->m_children; ++i) {
				tmp_region.Combine(e.m_node->m_children_mbr[i]);
			}
		}

		if (!(tmp_region == e.m_node->m_node_mbr))
		{
			std::cerr << "Invalid parent information." << std::endl;
			ret = false;
		}
		else if (!(tmp_region == e.m_parent_mbr))
		{
			std::cerr << "Error in parent." << std::endl;
			ret = false;
		}

		if (e.m_node->m_level != 0)
		{
			for (uint32_t cChild = 0; cChild < e.m_node->m_children; ++cChild)
			{
				std::shared_ptr<Node> ptr_n = ReadNode(e.m_node->m_children_id[cChild]);
				ValidateEntry tmpEntry(e.m_node->m_children_mbr[cChild], ptr_n);

				auto itr = nodes_in_level.find(tmpEntry.m_node->m_level);
				if (itr == nodes_in_level.end())
				{
					nodes_in_level.insert(std::pair<uint32_t, uint32_t>(tmpEntry.m_node->m_level, 1l));
				}
				else
				{
					nodes_in_level[tmpEntry.m_node->m_level] = nodes_in_level[tmpEntry.m_node->m_level] + 1;
				}

				st.push(tmpEntry);
			}
		}
	}

	uint32_t nodes = 0;
	for (int i = 0; i < m_stats.tree_height; ++i)
	{
		if (nodes_in_level[i] != m_stats.nodes_in_level[i])
		{
			std::cerr << "Invalid nodesInLevel information." << std::endl;
			ret = false;
		}

		nodes += m_stats.nodes_in_level[i];
	}

	if (nodes != m_stats.nodes)
	{
		std::cerr << "Invalid number of nodes information." << std::endl;
		ret = false;
	}

	return ret;
}

//void GetStatistics(IStatistics** out) const;

void RTree::Flush()
{
	StoreHeader();
}

id_type RTree::WriteNode(const Node& n)
{
	uint8_t* buffer;
	uint32_t data_len;
	n.StoreToByteArray(&buffer, data_len);

	id_type page = n.m_identifier < 0 ? NewPage : n.m_identifier;
	try
	{
		m_storage_mgr->StoreByteArray(page, data_len, buffer);
		delete[] buffer;
	}
	catch (InvalidPageException& e)
	{
		delete[] buffer;
		std::cerr << e.what() << std::endl;
		throw;
	}

	if (n.m_identifier < 0)
	{
		const_cast<Node&>(n).m_identifier = page;
		++m_stats.nodes;

#ifndef NDEBUG
		try
		{
			m_stats.nodes_in_level[n.m_level] = m_stats.nodes_in_level.at(n.m_level) + 1;
		}
		catch(...)
		{
			throw IllegalStateException("writeNode: writing past the end of m_nodesInLevel.");
		}
#else
		m_stats.nodes_in_level[n.m_level] = m_stats.nodes_in_level[n.m_level] + 1;
#endif
	}

	++m_stats.writes;

	for (auto& cmd : m_write_node_cmds) {
		cmd->Execute(n);
	}

	return page;
}

std::shared_ptr<Node> RTree::ReadNode(id_type page)
{
	uint32_t data_len;
	uint8_t* buffer;

	try
	{
		m_storage_mgr->LoadByteArray(page, data_len, &buffer);
	}
	catch (InvalidPageException& e)
	{
		std::cerr << e.what() << std::endl;
		throw;
	}

	try
	{
		uint32_t node_type;
		memcpy(&node_type, buffer, sizeof(uint32_t));

		std::shared_ptr<Node> n = nullptr;
		if (node_type == PersistentIndex) {
			n = std::make_shared<Index>(this, -1, 0);
		} else if (node_type == PersistentLeaf) {
			n = std::make_shared<Leaf>(this, -1);
		} else {
			throw IllegalStateException("readNode: failed reading the correct node type information");
		}

		//n->m_pTree = this;
		n->m_identifier = page;
		n->LoadFromByteArray(buffer);

		++m_stats.reads;

		for (auto& cmd : m_read_node_cmds) {
			cmd->Execute(*n);
		}

		delete[] buffer;
		return n;
	}
	catch (...)
	{
		delete[] buffer;
		throw;
	}
}

void RTree::DeleteNode(const Node& n)
{
	try
	{
		m_storage_mgr->DeleteByteArray(n.m_identifier);
	}
	catch (InvalidPageException& e)
	{
		std::cerr << e.what() << std::endl;
		throw;
	}

	--m_stats.nodes;
	m_stats.nodes_in_level[n.m_level] = m_stats.nodes_in_level[n.m_level] - 1;

	for (auto& cmd : m_delete_node_cmds) {
		cmd->Execute(n);
	}
}

void RTree::InitNew()
{
	StoreHeader();

	m_stats.tree_height = 1;
	m_stats.nodes_in_level.push_back(0);

	Leaf root(this, -1);
	m_root_id = WriteNode(root);

	StoreHeader();
}

void RTree::InitOld()
{
	m_header_id = 0;
	LoadHeader();
}

void RTree::StoreHeader()
{
	const uint32_t header_sz =
		sizeof(id_type) +						// m_rootID
		sizeof(RTreeVariant) +					// m_treeVariant
		sizeof(double) +						// m_fillFactor
		sizeof(uint32_t) +						// m_indexCapacity
		sizeof(uint32_t) +						// m_leafCapacity
		sizeof(uint32_t) +						// m_nearMinimumOverlapFactor
		sizeof(double) +						// m_splitDistributionFactor
		sizeof(double) +						// m_reinsertFactor
		sizeof(uint32_t) +						// m_dimension
		sizeof(char) +							// m_bTightMBRs
		sizeof(uint32_t) +						// m_stats.m_nodes
		sizeof(uint64_t) +						// m_stats.m_data
		sizeof(uint32_t) +						// m_stats.m_treeHeight
		m_stats.tree_height * sizeof(uint32_t);	// m_stats.m_nodesInLevel

	uint8_t* header = new uint8_t[header_sz];
	uint8_t* ptr = header;

	memcpy(ptr, &m_root_id, sizeof(id_type));
	ptr += sizeof(id_type);
	memcpy(ptr, &m_tree_var, sizeof(RTreeVariant));
	ptr += sizeof(RTreeVariant);
	memcpy(ptr, &m_fill_factor, sizeof(double));
	ptr += sizeof(double);
	memcpy(ptr, &m_index_capacity, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(ptr, &m_leaf_capacity, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(ptr, &m_near_minimum_overlap_factor, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(ptr, &m_split_distribution_factor, sizeof(double));
	ptr += sizeof(double);
	memcpy(ptr, &m_reinsert_factor, sizeof(double));
	ptr += sizeof(double);
	//memcpy(ptr, &m_dimension, sizeof(uint32_t));
	//ptr += sizeof(uint32_t);
	char c = (char)m_tight_mbrs;
	memcpy(ptr, &c, sizeof(char));
	ptr += sizeof(char);
	memcpy(ptr, &(m_stats.nodes), sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(ptr, &(m_stats.data), sizeof(uint64_t));
	ptr += sizeof(uint64_t);
	memcpy(ptr, &(m_stats.tree_height), sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	for (uint32_t i = 0; i < m_stats.tree_height; ++i)
	{
		memcpy(ptr, &(m_stats.nodes_in_level[i]), sizeof(uint32_t));
		ptr += sizeof(uint32_t);
	}

	m_storage_mgr->StoreByteArray(m_header_id, header_sz, header);

	delete[] header;
}

void RTree::LoadHeader()
{
	uint32_t headerSize;
	uint8_t* header = nullptr;
	m_storage_mgr->LoadByteArray(m_header_id, headerSize, &header);

	uint8_t* ptr = header;

	memcpy(&m_root_id, ptr, sizeof(id_type));
	ptr += sizeof(id_type);
	memcpy(&m_tree_var, ptr, sizeof(RTreeVariant));
	ptr += sizeof(RTreeVariant);
	memcpy(&m_fill_factor, ptr, sizeof(double));
	ptr += sizeof(double);
	memcpy(&m_index_capacity, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(&m_leaf_capacity, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(&m_near_minimum_overlap_factor, ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(&m_split_distribution_factor, ptr, sizeof(double));
	ptr += sizeof(double);
	memcpy(&m_reinsert_factor, ptr, sizeof(double));
	ptr += sizeof(double);
	//memcpy(&m_dimension, ptr, sizeof(uint32_t));
	//ptr += sizeof(uint32_t);
	char c;
	memcpy(&c, ptr, sizeof(char));
	m_tight_mbrs = (c != 0);
	ptr += sizeof(char);
	memcpy(&(m_stats.nodes), ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);
	memcpy(&(m_stats.data), ptr, sizeof(uint64_t));
	ptr += sizeof(uint64_t);
	memcpy(&(m_stats.tree_height), ptr, sizeof(uint32_t));
	ptr += sizeof(uint32_t);

	for (uint32_t i = 0; i < m_stats.tree_height; ++i)
	{
		uint32_t c;
		memcpy(&c, ptr, sizeof(uint32_t));
		ptr += sizeof(uint32_t);
		m_stats.nodes_in_level.push_back(c);
	}

	delete[] header;
}

void RTree::InsertDataImpl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id) 
{
	std::stack<id_type> path_buf;
	uint8_t* overflow_tbl = nullptr;

	try
	{
		std::shared_ptr<Node> root = ReadNode(m_root_id);

		overflow_tbl = new uint8_t[root->m_level];
		memset(overflow_tbl, 0, root->m_level);

		std::shared_ptr<Node> l = root->ChooseSubtree(mbr, 0, path_buf);
		l->InsertData(data_len, data, mbr, id, path_buf, overflow_tbl);

		delete[] overflow_tbl;
		++m_stats.data;
	}
	catch (...)
	{
		delete[] overflow_tbl;
		throw;
	}
}

void RTree::InsertDataImpl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id, uint32_t level, uint8_t* overflow_tbl) 
{
	std::stack<id_type> path_buf;
	std::shared_ptr<Node> root = ReadNode(m_root_id);
	std::shared_ptr<Node> n = root->ChooseSubtree(mbr, level, path_buf);

	assert(n->m_level == level);

	if (n.get() == root.get())
	{
		assert(root.unique());
	}
	n->InsertData(data_len, data, mbr, id, path_buf, overflow_tbl);
}

bool RTree::DeleteDataImpl(const Region& mbr, id_type id) 
{
	std::stack<id_type> path_buf;
	std::shared_ptr<Node> root = ReadNode(m_root_id);
	std::shared_ptr<Node> l = root->FindLeaf(mbr, id, path_buf);
	if (l.get() == root.get()) {
		assert(root.unique());
	}

	if (l != nullptr)
	{
		std::static_pointer_cast<Leaf>(l)->DeleteData(mbr, id, path_buf);
		--m_stats.data;
		return true;
	}

	return false;
}

void RTree::RangeQuery(RangeQueryType type, const IShape& query, IVisitor& v)
{
	std::stack<std::shared_ptr<Node>> st;
	std::shared_ptr<Node> root = ReadNode(m_root_id);

	if (root->m_children > 0 && query.IntersectsShape(root->m_node_mbr)) {
		st.push(root);
	}

	while (!st.empty())
	{
		std::shared_ptr<Node> n = st.top(); st.pop();

		if (n->m_level == 0)
		{
			v.VisitNode(*n);

			for (int i = 0; i < n->m_children; ++i)
			{
				bool b;
				if (type == ContainmentQuery) {
					b = query.ContainsShape(n->m_children_mbr[i]);
				} else {
					b = query.IntersectsShape(n->m_children_mbr[i]);
				}

				if (b)
				{
					Data data = Data(n->m_children_data_len[i], n->m_children_data[i], n->m_children_mbr[i], n->m_children_id[i]);
					v.VisitData(data);
					++m_stats.query_results;
				}
			}
		}
		else
		{
			VisitorStatus status = v.VisitNode(*n);
			if (status == VisitorStatus::Continue)
			{
				for (int i = 0; i < n->m_children; ++i)
				{
					if (query.IntersectsShape(n->m_children_mbr[i])) {
						st.push(ReadNode(n->m_children_id[i]));
					}
				}
			}
		}
	}
}

void RTree::SelfJoinQuery(id_type id1, id_type id2, const Region& r, IVisitor& vis)
{
	std::shared_ptr<Node> n1 = ReadNode(id1);
	std::shared_ptr<Node> n2 = ReadNode(id2);
	vis.VisitNode(*n1);
	vis.VisitNode(*n2);

	for (int i = 0; i < n1->m_children; ++i)
	{
		if (r.IntersectsRegion(n1->m_children_mbr[i]))
		{
			for (int j = 0; j < n2->m_children; ++j)
			{
				if (r.IntersectsRegion(n2->m_children_mbr[j]) &&
					n1->m_children_mbr[i].IntersectsRegion(n2->m_children_mbr[j]))
				{
					if (n1->m_level == 0)
					{
						if (n1->m_children_id[i] != n2->m_children_id[j])
						{
							assert(n2->m_level == 0);

							std::vector<const IData*> v;
							Data e1(n1->m_children_data_len[i], n1->m_children_data[i], n1->m_children_mbr[i], n1->m_children_id[i]);
							Data e2(n2->m_children_data_len[j], n2->m_children_data[j], n2->m_children_mbr[j], n2->m_children_id[j]);
							v.push_back(&e1);
							v.push_back(&e2);
							vis.VisitData(v);
						}
					}
					else
					{
						Region rr = r.GetIntersectingRegion(n1->m_children_mbr[i].GetIntersectingRegion(n2->m_children_mbr[j]));
						SelfJoinQuery(n1->m_children_id[i], n2->m_children_id[j], rr, vis);
					}
				}
			}
		}
	}
}

void RTree::VisitSubTree(const std::shared_ptr<Node>& sub_tree, IVisitor& v)
{
	std::stack<std::shared_ptr<Node>> st;
	st.push(sub_tree);

	while (!st.empty())
	{
		std::shared_ptr<Node> n = st.top(); st.pop();

		VisitorStatus status = v.VisitNode(*n);

		if (n->m_level == 0)
		{
			for (int i = 0; i < n->m_children; ++i)
			{
				Data data = Data(n->m_children_data_len[i], n->m_children_data[i], n->m_children_mbr[i], n->m_children_id[i]);
				v.VisitData(data);
				++m_stats.query_results;
			}
		}
		else
		{
			if (status == VisitorStatus::Continue)
			{
				for (int i = 0; i < n->m_children; ++i)
				{
					st.push(ReadNode(n->m_children_id[i]));
				}
			}
		}
	}
}

}