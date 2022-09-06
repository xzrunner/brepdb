#pragma once

#include "brepdb/Node.h"

namespace brepdb
{

class Index : public Node, public std::enable_shared_from_this<Index>
{
public:
	Index(RTree* tree, id_type id, uint32_t level);

	virtual std::shared_ptr<Node> ChooseSubtree(const Region& mbr, uint32_t level, std::stack<id_type>& path_buf) override;
	virtual std::shared_ptr<Node> FindLeaf(const Region& mbr, id_type id, std::stack<id_type>& path_buf) override;

	virtual void Split(uint32_t data_len, uint8_t* data, const Region& mbr, id_type id, std::shared_ptr<Node>& left, std::shared_ptr<Node>& right) override;

	void AdjustTree(const Node* n, std::stack<id_type>& path_buf, bool force = false);
	void AdjustTree(const Node* n1, const Node* n2, std::stack<id_type>& path_buf, uint8_t* overflow_tbl);

private:
	uint32_t FindLeastEnlargement(const Region& r) const;
	uint32_t FindLeastOverlap(const Region& r) const;

	class OverlapEntry
	{
	public:
		uint32_t m_index;
		double m_enlargement;
		Region m_original;
		Region m_combined;
		double m_oa;
		double m_ca;

		static int CompareEntries(const void* pv1, const void* pv2)
		{
			OverlapEntry* pe1 = *(OverlapEntry**)pv1;
			OverlapEntry* pe2 = *(OverlapEntry**)pv2;

			if (pe1->m_enlargement < pe2->m_enlargement) return -1;
			if (pe1->m_enlargement > pe2->m_enlargement) return 1;
			return 0;
		}
	}; // OverlapEntry

}; // Index

}