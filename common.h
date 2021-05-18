#ifndef COMMON_H
#define COMMON_H 1

struct geom {
	int x;
	int y;
	unsigned int w;
	unsigned int h;
};

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE  (!FALSE)
#endif

int cmp_pointer(void *left, void *right);
int geom_intersects(struct geom *left, struct geom *right);

#endif /* COMMON_H */
