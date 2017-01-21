//
// This file contains the algorithm to keep track of free rectangles
// and allocate/free them.
//
// Programmer's note:
// This may not be the most efficient implementation, but it's a hard
// problem anyway.
//
#include "freearea.h"

using namespace std;

void FreeArea::addRect( const RECT& rect )
{
	// Check if the rectangle is completely contained within an existing rectangle
	for (vector<RECT>::iterator p = Rects.begin(); p != Rects.end(); p++)
	{
		if ((rect.x >= p->x) && (rect.x + rect.w <= p->x + p->w) &&
			(rect.y >= p->y) && (rect.y + rect.h <= p->y + p->h))
		{
			return;
		}
	}

	// If not, add it
	if (Recycled.empty())
	{
		Rects.push_back( rect );
	}
	else
	{
		Rects[ Recycled.top() ] = rect;
		Recycled.pop();
	}
}

bool FreeArea::removeRect( const RECT& rect )
{
	bool complete = false;
	for (size_t i = 0; i < Rects.size(); i++)
	{
		if (Rects[i].w != 0 &&
			(rect.x + rect.w > Rects[i].x) && (rect.x < Rects[i].x + Rects[i].w) &&
			(rect.y + rect.h > Rects[i].y) && (rect.y < Rects[i].y + Rects[i].h))
		{
			// The rectangle intersect an existing rectangle
			if ((rect.x >= Rects[i].x) && (rect.x + rect.w <= Rects[i].x + Rects[i].w) &&
				(rect.y >= Rects[i].y) && (rect.y + rect.h <= Rects[i].y + Rects[i].h))
			{
				// The rectangle is completely contained within this rectangle
				complete = true;
			}

			// Remove it
			RECT r = Rects[i];
			Rects[i].w = 0;
			Recycled.push(i);

			// Now create (at most four) rectangles that describe the remainder
			if (rect.x > r.x)
			{
				// Create the left rectangle
				RECT nr = {r.x, r.y, rect.x - r.x, r.h};
				addRect(nr);
			}

			if (rect.y > r.y)
			{
				// Create the top rectangle
				RECT nr = {r.x, r.y, r.w, rect.y - r.y};
				addRect(nr);
			}

			if (rect.x + rect.w < r.x + r.w)
			{
				// Create the right rectangle
				RECT nr = {rect.x + rect.w, r.y, (r.x + r.w) - (rect.x + rect.w), r.h};
				addRect(nr);
			}

			if (rect.y + rect.h < r.y + r.h)
			{
				// Create the bottom rectangle
				RECT nr = { r.x, rect.y + rect.h, r.w, (r.y + r.h) - (rect.y + rect.h)};
				addRect(nr);
			}
		}
	}
	return complete;
}

bool FreeArea::getFreeArea( RECT& area )
{
	// Find a free rectangle that can hold the area
	for (vector<RECT>::iterator p = Rects.begin(); p != Rects.end(); p++)
	{
		if ((p->w >= area.w) && (p->h >= area.h))
		{
			// Found one, remove it
			RECT rect;
			rect.x = area.x = p->x;
			rect.y = area.y = p->y;
			rect.w = area.w;
			rect.h = area.h;
			removeRect(rect);
			return true;
		}
	}

	// No rectangle could hold the area
	return false;
}

bool FreeArea::addUsedArea( int x, int y, int width, int height )
{
	RECT rect = {x, y, width, height};
	return removeRect( rect );
}

void FreeArea::addFreeArea( int x, int y, int width, int height )
{	
	RECT rect = {x, y, width, height};
	addRect( rect );
}
