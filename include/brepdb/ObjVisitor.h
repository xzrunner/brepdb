#pragma once

#include "brepdb/SpatialIndex.h"

namespace brepdb
{

class ObjVisitor : public IVisitor
{
public:
    ObjVisitor() {}
    virtual ~ObjVisitor() {
        for (auto& i : m_vector) {
            delete i;
        }
    }
    
    virtual brepdb::VisitorStatus VisitNode(const INode& n) override { 
        return brepdb::VisitorStatus::Continue;
    }
    virtual void VisitData(const IData& d) override {
        IData* item = dynamic_cast<IData*>(const_cast<IData&>(d).Clone());
        ++m_results;
        m_vector.push_back(item);
    }
    virtual void VisitData(std::vector<const IData*>& v) override {}

    uint64_t GetResultCount() const { return m_results; }
    std::vector<IData*>& GetResults() { return m_vector; }

private:
    std::vector<IData*> m_vector;
    uint64_t m_results = 0;

}; // ObjVisitor

}