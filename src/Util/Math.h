#pragma once

#include "../common.h"

namespace OpenZod { namespace Util {
class Math {
public:
    static double DistanceBetweenPoints(int x1, int y1, int x2, int y2);
    static double DistanceBetweenPoints(COMMON::xy_struct a, COMMON::xy_struct b);
    static bool PointWithinArea(int px, int py, int ax, int ay, int aw, int ah);
    static bool IsZ(float num);
    static bool IsZ(double num);
    static bool Is1(float num);
    static bool Is1(double num);
    static double PointDistanceFromLine(int x1, int y1, int x2, int y2, int px, int py);
    static bool PointsWithinDistance(int x1, int y1, int x2, int y2, int distance);
};
}}