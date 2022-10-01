#include "common.h"

#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif /* !defined(MIN) */

#ifndef MAX
#define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#endif /* !defined(MAX) */

int cmp_pointer(void *left, void *right)
{
	return(left == right);
}

int geom_intersects(struct geom *first, struct geom *second)
{
	int top;
	int bottom;
	int left;
	int right;
	int width;
	int height;

	/*
	 *           left
	 *           |   right
	 *           |   |
	 *           v   v
	 *    +----------+
	 *    |          |
	 *    |      +---+------+ <-- top
	 *    |      |   |      |
	 *    +------+---+      | <-- bottom
	 *           |          |
	 *           +----------+
	 *
	 * This function calculated the area of the intersection of
	 * the two rectangles. If the area is zero, the rectangles
	 * do not intersect.
	 */

	top    = MAX(first->y,
		     second->y);
	bottom = MIN(first->y  + first->h,
		     second->y + second->h);
	left   = MAX(first->x,
		     second->x);
	right  = MIN(first->x  + first->w,
		     second->x + second->w);

	width  = MAX(0, right - left);
	height = MAX(0, bottom - top);

	return(width * height);
}
