#pragma once

namespace brepdb
{

class Point;

class Math
{
public:
    static double DoubleAreaTriangle(const Point& a, const Point& b, const Point& c);
    static bool LeftOf(const Point& a, const Point& b, const Point& c);
    static bool Collinear(const Point& a, const Point& b, const Point& c);
    static bool Between(const Point& a, const Point& b, const Point& c);
    static bool Between(double a, double b, double c);
    static bool IntersectsProper(const Point& a, const Point& b, const Point& c, const Point& d);
    static bool Intersects(const Point& a, const Point& b, const Point& c, const Point& d);

}; // Math

}