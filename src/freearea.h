//
// This file contains the class that maintains the Free/Used administration
// for one or more rectangles.
//
#ifndef FREEAREA_H
#define FREEAREA_H

#include <vector>
#include <stack>

class FreeArea
{
public:
	struct RECT
	{
		unsigned long x, y, w, h;
	};

private:
	std::vector<RECT>   Rects;
	std::stack<size_t> Recycled;

	void addRect( const RECT& rect );
	bool removeRect( const RECT& rect );

public:
	// Get a free area of size area.width by area.height
	// The resulting (x,y) coordinates are stored in the parameter along with the
	// requested width and height.
	bool getFreeArea( RECT& area );

	// Mark this area as used
	// Returns whether the indicated area was completely unused before
	bool addUsedArea( int x, int y, int width, int height );

	// Mark this area as free
	void addFreeArea( int x, int y, int width, int height );
};

#endif