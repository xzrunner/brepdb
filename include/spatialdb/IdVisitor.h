#pragma once

#include "spatialdb/SpatialIndex.h"

namespace spatialdb
{

class IdVisitor : public IVisitor
{
public:
    IdVisitor() {}
    
    virtual spatialdb::VisitorStatus VisitNode(const INode& n) override { 
        return spatialdb::VisitorStatus::Continue;
    }
    virtual void VisitData(const IData& d) override {
        m_results += 1;
        m_vector.push_back(d.GetIdentifier());
    }
    virtual void VisitData(std::vector<const IData*>& v) override {}

    uint64_t GetResultCount() const { return m_results; }
    std::vector<uint64_t>& GetResults() { return m_vector; }

private:
    std::vector<uint64_t> m_vector;
    uint64_t m_results = 0;

}; // IdVisitor

}