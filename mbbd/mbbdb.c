#include <stdarg.h>

#include "mbbthread.h"
#include "mbblog.h"
#include "mbbdb.h"

#include "strconv.h"
#include "macros.h"
#include "query.h"

static GHashTable *ht = NULL;
static struct mbb_db *db = NULL;
static gpointer db_conn = NULL;
static struct mbb_db_auth *db_auth = NULL;

static GStaticPrivate db_priv_key = G_STATIC_PRIVATE_INIT;
static GStaticRecMutex query_mutex; /* initialize it in init() */

void mbb_db_register(struct mbb_db *db)
{
	if (ht == NULL) {
		ht = g_hash_table_new_full(
			g_str_hash, g_str_equal, NULL, g_free
		);
	}

	g_hash_table_insert(ht, db->name, db);
}

struct mbb_db *mbb_db_search(gchar *name)
{
	if (ht == NULL)
		return NULL;

	return g_hash_table_lookup(ht, name);
}

GQuark mbb_db_error_quark(void)
{
	return g_quark_from_static_string("mbb-db-error-quark");
}

gboolean mbb_db_choose(gchar *dbtype)
{
	if (db != NULL)
		return FALSE;

	db = mbb_db_search(dbtype);

	return db != NULL;
}

static inline gboolean db_get_initialized(GError **error)
{
	if (db == NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_NOT_INITIALIZED,
			"db driver not choosed");
		return FALSE;
	}

	return TRUE;
}

gboolean mbb_db_open(struct mbb_db_auth *auth, GError **error)
{
	g_return_val_if_fail(db_get_initialized(error), FALSE);

	if (db_conn != NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_ALREADY_CONNECTED,
			"connection is already opened");
		return FALSE;
	}

	db_conn = db->open(auth, error);
	if (db_conn != NULL) {
		db_auth = auth;
		return TRUE;
	}

	return FALSE;
}

static inline void db_close(gpointer conn)
{
	db->close(conn);
}

gboolean mbb_db_dup_conn(GError **error)
{
	gpointer conn;

	g_return_val_if_fail(db_get_initialized(error), FALSE);

	if (db_auth == NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_NOT_CONNECTED,
			"not connected");
		return FALSE;
	}

	if (g_static_private_get(&db_priv_key) != NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_ALREADY_CONNECTED,
			"connection is already alive");
		return FALSE;
	}

	conn = db->open(db_auth, error);
	if (conn == NULL)
		return FALSE;

	g_static_private_set(&db_priv_key, conn, db_close);
	mbb_log("duplicate db connection");

	return TRUE;
}

static inline gpointer db_get_conn(GError **error)
{
	gpointer conn;

	if (! db_get_initialized(error))
		return NULL;

	conn = g_static_private_get(&db_priv_key);
	if (conn == NULL)
		conn = db_conn;

	if (conn == NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_NOT_CONNECTED,
			"not connected");
		return NULL;
	}

	return conn;
}

gboolean mbb_db_dup_conn_once(GError **error)
{
	gpointer conn;

	conn = db_get_conn(error);
	g_return_val_if_fail(conn != NULL, FALSE);

	if (conn != db_conn)
		return TRUE;

	return mbb_db_dup_conn(error);
}

static inline void db_query_lock(gpointer conn)
{
	if (conn == db_conn)
		g_static_rec_mutex_lock(&query_mutex);
}

static inline void db_query_unlock(gpointer conn)
{
	if (conn == db_conn)
		g_static_rec_mutex_unlock(&query_mutex);
}

gboolean mbb_db_query(gchar *command, mbb_db_func_t func, gpointer user_data, GError **error)
{
	gpointer conn;
	gboolean ret;

	conn = db_get_conn(error);
	g_return_val_if_fail(conn != NULL, FALSE);

	mbb_log_lvl(MBB_LOG_QUERY, "%s", command);

	db_query_lock(conn);
	ret = db->query(conn, command, func, user_data, error);
	db_query_unlock(conn);

	return ret;
}

gchar *mbb_db_escape(gchar *str)
{
	gpointer conn;
	gchar *esc;

	conn = db_get_conn(NULL);
	g_return_val_if_fail(conn != NULL, NULL);

	if (db->escape == NULL)
		return NULL;

	db_query_lock(conn);
	esc = db->escape(conn, str);
	db_query_unlock(conn);

	return esc;
}

struct mbb_db_iter *mbb_db_query_iter(gchar *command, GError **error)
{
	struct mbb_db_iter *iter;
	gpointer conn;

	conn = db_get_conn(error);
	g_return_val_if_fail(conn != NULL, NULL);

	mbb_log_lvl(MBB_LOG_QUERY, "%s", command);

	db_query_lock(conn);
	iter = db->query_iter(conn, command, error);
	db_query_unlock(conn);

	return iter;
}

gboolean mbb_db_iter_next(struct mbb_db_iter *iter)
{
	return db->iter_next(iter);
}

gint mbb_db_iter_nrow(struct mbb_db_iter *iter)
{
	return db->iter_get_nrow(iter);
}

gint mbb_db_iter_ncol(struct mbb_db_iter *iter)
{
	return db->iter_get_ncol(iter);
}

gchar *mbb_db_iter_value(struct mbb_db_iter *iter, gint field)
{
	return db->iter_get_value(iter, field);
}

void mbb_db_iter_free(struct mbb_db_iter *iter)
{
	db->iter_free(iter);
}

/*
void mbb_db_close(void)
{
	g_return_if_fail(db_get_initialized(NULL));

	if (db_conn != NULL) {
		db->close(db_conn);
		db_conn = db_auth = NULL;
	}
}
*/

static gboolean make_call(glong off, gboolean open, GError **error)
{
	gboolean (*callback)(gpointer, GError **);
	gpointer conn;
	gboolean ret;

	conn = db_get_conn(error);
	g_return_val_if_fail(conn != NULL, FALSE);

	*(gpointer *) &callback = G_STRUCT_MEMBER(gpointer, db, off);
	if (callback == NULL) {
		g_set_error(error, MBB_DB_ERROR, MBB_DB_ERROR_UNSUPPORTED,
			    "unsupported");
		return FALSE;
	}

	if (conn != db_conn)
		ret = callback(conn, error);
	else {
		if (open)
			g_static_rec_mutex_lock(&query_mutex);

		ret = callback(conn, error);

		if (! open)
			g_static_rec_mutex_unlock(&query_mutex);
	}

	return ret;
}

#define DB_STRUCT_OFFSET(member) G_STRUCT_OFFSET(struct mbb_db, member)

gboolean mbb_db_begin(GError **error)
{
	return make_call(DB_STRUCT_OFFSET(begin), TRUE, error);
}

gboolean mbb_db_rollback(GError **error)
{
	return make_call(DB_STRUCT_OFFSET(rollback), FALSE, error);
}

gboolean mbb_db_commit(GError **error)
{
	return make_call(DB_STRUCT_OFFSET(commit), FALSE, error);
}

gboolean mbb_db_insert(GError **error, gchar *table, gchar *fmt, ...)
{
	gchar *query;
	va_list ap;

	va_start(ap, fmt);
	query = query_insert_ap(table, NULL, fmt, ap);
	va_end(ap);

	return mbb_db_query(query, NULL, NULL, error);
}

static gboolean check_single_result(MbbDbIter *iter, GError **error)
{
	gint nrow, ncol;

	nrow = mbb_db_iter_nrow(iter);
	ncol = mbb_db_iter_ncol(iter);

	if (nrow != 1 || ncol != 1) {
		g_set_error(error, MBB_DB_ERROR, MBB_DB_ERROR_INSERT_FAILED,
			"invalid returning result: %d rows, %d cols", nrow, ncol);
		return FALSE;
	}

	return TRUE;
}

static gint fetch_number(gchar *str, GError **error)
{
	GError *str_conv_error = NULL;
	gint id;

	if (str == NULL) {
		g_set_error(error, MBB_DB_ERROR, MBB_DB_ERROR_INSERT_FAILED,
			"insert returns null value");
		return -1;
	}

	id = str_conv_to_uint64(str, &str_conv_error);
	if (str_conv_error != NULL) {
		g_error_free(str_conv_error);
		g_set_error(error, MBB_DB_ERROR, MBB_DB_ERROR_INSERT_FAILED,
			"insert returns invalid number %s", str);
		return -1;
	}

	return id;
}

gint mbb_db_insert_ret(GError **error, gchar *table, gchar *field, gchar *fmt, ...)
{
	MbbDbIter *iter;
	gchar *query;
	va_list ap;
	gint id;

	va_start(ap, fmt);
	query = query_insert_ap(table, field, fmt, ap);
	va_end(ap);

	iter = mbb_db_query_iter(query, error);
	if (iter == NULL)
		return -1;

	if (! check_single_result(iter, error)) {
		mbb_db_iter_free(iter);
		return -1;
	}

	mbb_db_iter_next(iter);
	id = fetch_number(mbb_db_iter_value(iter, 0), error);
	mbb_db_iter_free(iter);

	return id;
}

gboolean mbb_db_delete(GError **error, gchar *table, gchar *fmt, ...)
{
	gchar *query;
	va_list ap;

	va_start(ap, fmt);
	query = query_delete_ap(table, fmt, ap);
	va_end(ap);

	return mbb_db_query(query, NULL, NULL, error);
}

gboolean mbb_db_update(GError **error, gchar *table, gchar *fmt, gchar *name, ...)
{
	gchar *query;
	va_list ap;

	va_start(ap, name);
	query = query_update_ap(table, fmt, name, ap);
	va_end(ap);

	return mbb_db_query(query, NULL, NULL, error);
}

struct mbb_db_iter *mbb_db_select(GError **error, gchar *table, ...)
{
	gchar *query;
	va_list ap;

	va_start(ap, table);
	query = query_select_ap(table, ap);
	va_end(ap);

	return mbb_db_query_iter(query, error);
}

static void __init init(void)
{
	g_static_rec_mutex_init(&query_mutex);
}

