#ifndef MONITOR_H
#define MONITOR_H 1

struct monitor;

int monitor_new(int id, int x, int y, int w, int h,
		struct monitor **monitor);
int monitor_free(struct monitor **monitor);

int monitor_get_id(struct monitor *monitor);
int monitor_get_geometry(struct monitor *monitor,
			 int *x, int *y, int *w, int *h);
int monitor_set_geometry(struct monitor *monitor,
			 int x, int y, int w, int h);

#endif /* MONITOR_H */
