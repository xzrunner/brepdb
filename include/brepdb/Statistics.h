#pragma once

#include "brepdb/SpatialIndex.h"

namespace brepdb
{

class Statistics : public IStatistics
{
public:
	Statistics();
	Statistics(const Statistics&);
	~Statistics() override;
	Statistics& operator=(const Statistics&);

	//
	// IStatistics interface
	//
	virtual uint64_t GetReads() const override;
	virtual uint64_t GetWrites() const override;
	virtual uint32_t GetNumberOfNodes() const override;
	virtual uint64_t GetNumberOfData() const override;

	virtual uint64_t getSplits() const;
	virtual uint64_t getHits() const;
	virtual uint64_t getMisses() const;
	virtual uint64_t getAdjustments() const;
	virtual uint64_t getQueryResults() const;
	virtual uint32_t getTreeHeight() const;
	virtual uint32_t getNumberOfNodesInLevel(uint32_t l) const;

private:
	void reset();

	uint64_t m_u64Reads;

	uint64_t m_u64Writes;

	uint64_t m_u64Splits;

	uint64_t m_u64Hits;

	uint64_t m_u64Misses;

	uint32_t m_u32Nodes;

	uint64_t m_u64Adjustments;

	uint64_t m_u64QueryResults;

	uint64_t m_u64Data;

	uint32_t m_u32TreeHeight;

	std::vector<uint32_t> m_nodesInLevel;

}; // Statistics

}