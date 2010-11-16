#ifndef MBB_OPERATOR_H
#define MBB_OPERATOR_H

#include <glib.h>

struct mbb_gwlink;

typedef struct mbb_operator MbbOperator;

struct mbb_operator {
	gint id;
	gchar *name;
	GSList *links;
};

struct mbb_operator *mbb_operator_new(gint id, gchar *name);
void mbb_operator_free(struct mbb_operator *op);
void mbb_operator_join(struct mbb_operator *op);
void mbb_operator_detach(struct mbb_operator *op);
void mbb_operator_remove(struct mbb_operator *op);

gboolean mbb_operator_mod_name(struct mbb_operator *op, gchar *newname);

struct mbb_operator *mbb_operator_get_by_id(gint id);
struct mbb_operator *mbb_operator_get_by_name(gchar *name);

void mbb_operator_foreach(GFunc func, gpointer user_data);

void mbb_operator_add_link(struct mbb_operator *op, struct mbb_gwlink *gl);
void mbb_operator_del_link(struct mbb_operator *op, struct mbb_gwlink *gl);

#endif
