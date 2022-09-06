#pragma once

#include "brepdb/SpatialIndex.h"

namespace brepdb
{

class IdVisitor : public IVisitor
{
public:
    IdVisitor() {}
    
    virtual void VisitNode(const INode& n) override {}
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