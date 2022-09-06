#include "brepdb/RTree.h"
#include "brepdb/Node.h"
#include "brepdb/Index.h"
#include "brepdb/Leaf.h"
#include "brepdb/Exception.h"
#include "brepdb/IdVisitor.h"

#include <iostream>
#include <queue>

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

RTree::RTree(const std::shared_ptr<IStorageManager>& sm)
	: m_storage_mgr(sm)
{
	InitNew();
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

	insertData_impl(len, buffer, mbr, shape_id);
		// the buffer is stored in the tree. Do not delete here.
}

bool RTree::DeleteData(const IShape& shape, id_type shape_id)
{
	Region mbr;
	shape.GetMBR(mbr);

	return deleteData_impl(mbr, shape_id);
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
				else //not a leaf
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

			if(n->m_level == 0)
			{
				v.VisitNode(*n);

				for (int i = 0; i < n->m_children; ++i)
				{
					if(query.ContainsShape(n->m_children_mbr[i]))
					{
						Data data = Data(n->m_children_data_len[i], n->m_children_data[i], n->m_children_mbr[i], n->m_children_id[i]);
						v.VisitData(data);
						++m_stats.query_results;
					}
				}
			}
			else //not a leaf
			{
				if(query.ContainsShape(n->m_node_mbr))
				{
					VisitSubTree(n, v);
				}
				else if(query.IntersectsShape(n->m_node_mbr))
				{
					v.VisitNode(*n);

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
			v.VisitNode(*n);

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
	return false;
}

//void GetStatistics(IStatistics** out) const;

void RTree::Flush()
{
	//StoreHeader();
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
		m_stats.m_nodesInLevel[n->m_level] = m_stats.m_nodesInLevel[n->m_level] + 1;
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
	m_stats.tree_height = 1;
	m_stats.nodes_in_level.push_back(0);

	Leaf root(this, -1);
	m_root_id = WriteNode(root);
}

void RTree::insertData_impl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id) 
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

void RTree::insertData_impl(uint32_t data_len, uint8_t* data, Region& mbr, id_type id, uint32_t level, uint8_t* overflow_tbl) 
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

bool RTree::deleteData_impl(const Region& mbr, id_type id) 
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
				}
				else {
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
			v.VisitNode(*n);

			for (int i = 0; i < n->m_children; ++i)
			{
				if (query.IntersectsShape(n->m_children_mbr[i])) {
					st.push(ReadNode(n->m_children_id[i]));
				}
			}
		}
	}
}

void RTree::SelfJoinQuery(id_type id1, id_type id2, const Region& r, IVisitor& vis)
{

}

void RTree::VisitSubTree(const std::shared_ptr<Node>& sub_tree, IVisitor& v)
{

}

}