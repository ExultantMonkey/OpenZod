#include "Math.h"

#include <cmath>

double OpenZod::Util::Math::DistanceBetweenPoints(int x1, int y1, int x2, int y2)
{
    int dx = x1 - x2;
	int dy = y1 - y2;
	
	return sqrt((double)((dx * dx) + (dy * dy)));
}

double OpenZod::Util::Math::DistanceBetweenPoints(COMMON::xy_struct a, COMMON::xy_struct b)
{
    return DistanceBetweenPoints(a.x, a.y, b.x, b.y);
}

bool OpenZod::Util::Math::PointWithinArea(int px, int py, int ax, int ay, int aw, int ah)
{
    if((px < ax) || (py < ay) || (px > ax + aw) || (py > ay + ah))
    {
        return false;
    }

	return true;
}

bool OpenZod::Util::Math::IsZ(float num)
{
    return (num < 0.00001) && (num > -0.00001);
}

bool OpenZod::Util::Math::IsZ(double num)
{
    return (num < 0.00001) && (num > -0.00001);
}

bool OpenZod::Util::Math::Is1(float num)
{
    return (num < 1.00001) && (num > 0.99999);
}

bool OpenZod::Util::Math::Is1(double num)
{
    return (num < 1.00001) && (num > 0.99999);
}

double OpenZod::Util::Math::PointDistanceFromLine(int x1, int y1, int x2, int y2, int px, int py)
{
	int a = -1 * (y1 - y2);
	int b = (x1 - x2);
	int c = -1 * (a * x1 + b * y1);

	double under = sqrt((double)((a * a) + (b * b)));
	
	//distance from line equals...
	//|A*x + B*y + C| / (A^2 + B^2)^0.5
	
	return abs(a * px + b * py + c) / under;
}

bool OpenZod::Util::Math::PointsWithinDistance(int x1, int y1, int x2, int y2, int distance)
{
	//quick prelim tests
	if((x2 < x1 - distance) || (x2 > x1 + distance) || (y2 < y1 - distance) || (y2 > y1 + distance))
    {
        return false;
    }

	//semi quick tests
	int sh_dist = distance * 0.707106781; //sin(45)
	int dx = abs(x1 - x2);
	int dy = abs(y1 - y2);
	if(dx < sh_dist && dy < sh_dist)
    {
        return true;
    }

	//slow test
	if(sqrt((float)((dx * dx) + (dy * dy))) > distance)
    {
        return false;
    }

	return true;
}