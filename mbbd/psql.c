/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include <string.h>
#include <glib.h>

#include "mbbinit.h"
#include "mbbdb.h"

#include "macros.h"
#include "debug.h"

#include <libpq-fe.h>

struct mbb_db_iter {
	PGresult *res;
	gint nrow;
	gint ncol;
	gint current_row;
};

static struct mbb_db_iter *iter_new(PGresult *res)
{
	struct mbb_db_iter *iter;

	iter = g_new(struct mbb_db_iter, 1);
	iter->res = res;
	iter->nrow = PQntuples(res);
	iter->ncol = PQnfields(res);
	iter->current_row = -1;

	return iter;
}

static void append_escaped(GString *string, gchar *p)
{
	for (; *p != '\0'; p++) {
		if (*p == '\\' || *p == '\'')
			g_string_append_c(string, '\\');
		g_string_append_c(string, *p);
	}
}

static void append_key_value(GString *string, gchar *key, gchar *value)
{
	if (value == NULL)
		return;

	if (string->len != 0)
		g_string_append_c(string, ' ');
	g_string_append_printf(string, "%s = '", key);
	append_escaped(string, value);
	g_string_append_c(string, '\'');
}

static gpointer pq_open(struct mbb_db_auth *auth, GError **error)
{
	PGconn *conn;
	GString *string;

	/*
	if (pg_conn != NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_ALREADY_CONNECTED,
			"connection is already opened");
		return FALSE;
	}
	*/

	string = g_string_new(NULL);
	append_key_value(string, "hostaddr", auth->hostaddr);
	append_key_value(string, "user", auth->login);
	append_key_value(string, "password", auth->secret);
	append_key_value(string, "dbname", auth->database);

	conn = PQconnectdb(string->str);
	msg_warn("%s", string->str);
	g_string_free(string, TRUE);

	if (PQstatus(conn) != CONNECTION_OK) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_CONNECT,
			"connect failed: %s", PQerrorMessage(conn));
		PQfinish(conn);
		return NULL;
	}

	return conn;
}

static PGresult *pq_exec(gpointer conn, const gchar *command, GError **error)
{
	PGconn *pg_conn = conn;
	ExecStatusType status;
	PGresult *res;

	if (pg_conn == NULL) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_NOT_CONNECTED,
			"not connected");
		return NULL;
	}

	res = PQexec(pg_conn, command);
	status = PQresultStatus(res);
	if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
		g_set_error(error, MBB_DB_ERROR,
			MBB_DB_ERROR_QUERY,
			"%s", PQerrorMessage(pg_conn));
		PQclear(res);
		res = NULL;
	}

	return res;
}

static gboolean pq_query(gpointer conn, gchar *command, mbb_db_func_t func,
			 gpointer user_data, GError **error)
{
	PGconn *pg_conn = conn;
	PGresult *res;
	gint nrow;
	gint ncol;
	gchar **pp;
	gint r, c;
	gboolean ret;

	pp = NULL;
	res = pq_exec(pg_conn, command, error);
	if (res == NULL)
		return FALSE;

	ret = TRUE;
	if (PQresultStatus(res) == PGRES_COMMAND_OK)
		goto out;

	if (func == NULL)
		goto out;

	nrow = PQntuples(res);
	if (nrow == 0)
		goto out;

	ncol = PQnfields(res);

	pp = g_new(gchar *, ncol + 1);
	pp[ncol] = NULL;
	for (r = 0; r < nrow; r++) {
		for (c = 0; c < ncol; c++) {
			if (PQgetisnull(res, r, c))
				pp[c] = NULL;
			else
				pp[c] = PQgetvalue(res, r, c);
		}

		if (func(pp, user_data, error) == FALSE) {
			ret = FALSE;
			goto out;
		}
	}

out:
	g_free(pp);
	PQclear(res);
	return ret;
}

static gchar *pq_escape(gpointer conn, gchar *str)
{
	PGconn *pg_conn = conn;
	gint error;
	gchar *tmp;
	gsize len;

	if (pg_conn == NULL)
		return NULL;

	len = strlen(str);
	tmp = g_malloc(2 * len + 1);

	PQescapeStringConn(pg_conn, tmp, str, len, &error);

	if (error) {
		msg_warn("%s: %s", str, PQerrorMessage(pg_conn));
		g_free(tmp);
		tmp = NULL;
	}

	return tmp;
}

static struct mbb_db_iter *pq_query_iter(gpointer conn, gchar *command, GError **error)
{
	struct mbb_db_iter *iter;
	PGresult *res;

	res = pq_exec(conn, command, error);
	if (res == NULL)
		return NULL;

	iter = iter_new(res);

	return iter;
}

static gboolean pq_iter_next(struct mbb_db_iter *iter)
{
	iter->current_row++;

	if (iter->current_row == iter->nrow)
		return FALSE;
	return TRUE;
}

static gint pq_iter_get_nrow(struct mbb_db_iter *iter)
{
	return iter->nrow;
}

static gint pq_iter_get_ncol(struct mbb_db_iter *iter)
{
	return iter->ncol;
}

static gchar *pq_iter_get_value(struct mbb_db_iter *iter, gint field)
{
	gint cr;

	cr = iter->current_row;
	if (cr < 0 || cr == iter->nrow)
		return NULL;

	if (PQgetisnull(iter->res, cr, field))
		return NULL;
	return PQgetvalue(iter->res, cr, field);
}

static void pq_iter_free(struct mbb_db_iter *iter)
{
	PQclear(iter->res);
	g_free(iter);
}

static void pq_close(gpointer conn)
{
	PGconn *pg_conn = conn;

	if (pg_conn != NULL) {
		msg_warn("close db connection");
		PQfinish(pg_conn);
	}
}

static inline gboolean silent_exec(gpointer conn, gchar *query, GError **error)
{
	PGresult *res;

	res = pq_exec(conn, query, error);
	if (res != NULL) {
		PQclear(res);
		return TRUE;
	}

	return FALSE;
}

static gboolean begin(gpointer conn, GError **error)
{
	return silent_exec(conn, "begin", error);
}

static gboolean rollback(gpointer conn, GError **error)
{
	return silent_exec(conn, "rollback", error);
}

static gboolean commit(gpointer conn, GError **error)
{
	return silent_exec(conn, "commit", error);
}

static struct mbb_db pg_db = {
	.name = "postgres",

	.open = pq_open,
	.query = pq_query,
	.query_iter = pq_query_iter,

	.escape = pq_escape,

	.iter_next = pq_iter_next,
	.iter_get_nrow = pq_iter_get_nrow,
	.iter_get_ncol = pq_iter_get_ncol,
	.iter_get_value = pq_iter_get_value,
	.iter_free = pq_iter_free,

	.begin = begin,
	.rollback = rollback,
	.commit = commit,

	.close = pq_close
};

static void __init init(void)
{
	static struct mbb_init_struct pg_db_init =
		MBB_INIT_STRUCT(mbb_db_register, &pg_db);

	mbb_init_push(&pg_db_init);
}

