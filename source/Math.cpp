#include "brepdb/Math.h"
#include "brepdb/Point.h"

namespace brepdb
{

double Math::DoubleAreaTriangle(const Point& a, const Point& b, const Point& c) 
{
    auto pa = a.GetCoords();
    auto pb = b.GetCoords();
    auto pc = c.GetCoords();
    return (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pc[0] - pa[0]) * (pb[1] - pa[1]);
}

bool Math::LeftOf(const Point& a, const Point& b, const Point& c) 
{
    return DoubleAreaTriangle(a, b, c) > 0;
}

bool Math::Collinear(const Point& a, const Point& b, const Point& c) 
{
    return DoubleAreaTriangle(a, b, c) == 0;
}

bool Math::IntersectsProper(const Point& a, const Point& b, const Point& c, const Point& d) 
{
    if (Collinear(a, b, c) || Collinear(a, b, d) ||
        Collinear(c, d, a) || Collinear(c, d, b)) {
        return false;
    }
    return (LeftOf(a, b, c) ^ LeftOf(a, b, d)) &&
           (LeftOf(c, d, a) ^ LeftOf(c, d, b));
}

bool Math::Between(const Point& a, const Point& b, const Point& c) 
{
    if (!Collinear(a, b, c)) {
        return false;
    }
    auto pa = a.GetCoords();
    auto pb = b.GetCoords();
    auto pc = c.GetCoords();
    if ( pa[0] != pb[0] ) {
        return Between(pa[0], pb[0], pc[0]);
    } else {
        return Between(pa[1], pb[1], pc[1]);
    }
}

bool Math::Between(double a, double b, double c) 
{
    return (a <= c && c <= b) || (a >= c && c >= b);
}

bool Math::Intersects(const Point& a, const Point& b, const Point& c, const Point& d) 
{
    if (IntersectsProper(a, b, c, d)) {
        return true;
    }  else if (Between(a, b, c) || Between(a, b, d) ||
                Between(c, d, a) || Between(c, d, b) ) { 
        return true;
    } else { 
        return false;
    }
}

}