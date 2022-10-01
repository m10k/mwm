#ifndef MWM_LAYOUT_H
#define MWM_LAYOUT_H 1

struct monitor;
struct workspace;
struct geom;

typedef enum {
	LAYOUT_HORIZONTAL = (1 << 0),
	LAYOUT_VERTICAL   = (1 << 1)
} layout_orientation_t;

struct layout;

layout_orientation_t layout_get_orientation(struct layout*);

int layout_arrange(struct layout *layout,
		   struct workspace *workspace,
		   struct geom *usable_area);

#endif /* MWM_LAYOUT_H */
