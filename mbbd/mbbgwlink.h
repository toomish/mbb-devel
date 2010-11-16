#ifndef MBB_GWLINK_H
#define MBB_GWLINK_H

#include <glib.h>
#include <time.h>

struct mbb_gateway;
struct mbb_operator;

typedef struct mbb_gwlink MbbGwLink;

struct mbb_gwlink {
	gint id;

	struct mbb_operator *op;
	struct mbb_gateway *gw;
	guint link;

	time_t start;
	time_t end;
};

struct mbb_gwlink *mbb_gwlink_new(gint id);
void mbb_gwlink_free(struct mbb_gwlink *gl);
void mbb_gwlink_join(struct mbb_gwlink *gl);
void mbb_gwlink_detach(struct mbb_gwlink *gl);
void mbb_gwlink_remove(struct mbb_gwlink *gl);

struct mbb_gwlink *mbb_gwlink_get_by_id(gint id);

void mbb_gwlink_foreach(GFunc func, gpointer user_data);

#endif
