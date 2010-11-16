#ifndef MBB_DB_H
#define MBB_DB_H

#include <glib.h>

struct mbb_db_auth {
	gchar *hostaddr;
	gchar *login;
	gchar *secret;
	gchar *database;
};

typedef gboolean (*mbb_db_func_t)(gchar **argv, gpointer user_data, GError **error);

struct mbb_db_iter;

typedef struct mbb_db_iter MbbDbIter;

struct mbb_db {
	gchar *name;

	gpointer (*open)(struct mbb_db_auth *auth, GError **error);
	gboolean (*query)(gpointer conn, gchar *command, mbb_db_func_t func,
			  gpointer user_data, GError **error);

	gchar *(*escape)(gpointer conn, gchar *str);

	struct mbb_db_iter *(*query_iter)(gpointer conn, gchar *command, GError **error);
	gboolean (*iter_next)(struct mbb_db_iter *iter);
	gint (*iter_get_nrow)(struct mbb_db_iter *iter);
	gint (*iter_get_ncol)(struct mbb_db_iter *iter);
	gchar *(*iter_get_value)(struct mbb_db_iter *iter, gint field);
	void (*iter_free)(struct mbb_db_iter *iter);

	gboolean (*begin)(gpointer conn, GError **error);
	gboolean (*rollback)(gpointer conn, GError **error);
	gboolean (*commit)(gpointer conn, GError **error);

	void (*close)(gpointer conn);
};

#define MBB_DB_ERROR (mbb_db_error_quark())

typedef enum {
	MBB_DB_ERROR_NOT_INITIALIZED,
	MBB_DB_ERROR_ALREADY_CONNECTED,
	MBB_DB_ERROR_CONNECT,
	MBB_DB_ERROR_NOT_CONNECTED,
	MBB_DB_ERROR_QUERY,
	MBB_DB_ERROR_UNSUPPORTED,
	MBB_DB_ERROR_INSERT_FAILED
} MbbDbError;

GQuark mbb_db_error_quark(void);

gboolean mbb_db_choose(gchar *dbtype);

gboolean mbb_db_open(struct mbb_db_auth *auth, GError **error);
gboolean mbb_db_dup_conn(GError **error);
gboolean mbb_db_dup_conn_once(GError **error);
gboolean mbb_db_query(gchar *command, mbb_db_func_t func, gpointer user_data, GError **error);

gchar *mbb_db_escape(gchar *str);

struct mbb_db_iter *mbb_db_query_iter(gchar *command, GError **error);
gboolean mbb_db_iter_next(struct mbb_db_iter *iter);
gint mbb_db_iter_nrow(struct mbb_db_iter *iter);
gint mbb_db_iter_ncol(struct mbb_db_iter *iter);
gchar *mbb_db_iter_value(struct mbb_db_iter *iter, gint field);
void mbb_db_iter_free(struct mbb_db_iter *iter);

gboolean mbb_db_begin(GError **error);
gboolean mbb_db_rollback(GError **error);
gboolean mbb_db_commit(GError **error);

void mbb_db_register(struct mbb_db *db);
struct mbb_db *mbb_db_search(gchar *name);

struct mbb_db_iter *mbb_db_select(GError **error, gchar *table, ...);
gboolean mbb_db_insert(GError **error, gchar *table, gchar *fmt, ...);
gint mbb_db_insert_ret(GError **error, gchar *table, gchar *field, gchar *fmt, ...);
gboolean mbb_db_delete(GError **error, gchar *table, gchar *fmt, ...);
gboolean mbb_db_update(GError **error, gchar *table, gchar *fmt, gchar *name, ...);

#endif
