/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbboperator.h"
#include "mbbgateway.h"
#include "mbbgwlink.h"
#include "mbbconsumer.h"
#include "mbbunit.h"
#include "mbbdbload.h"
#include "mbbgroup.h"
#include "mbbuser.h"
#include "mbbcap.h"
#include "mbbdb.h"

#include <string.h>

#include "varconv.h"
#include "debug.h"
#include "trash.h"

#define QUERY_USERS \
	"select user_id, user_name, user_secret from users"

#define QUERY_GROUPS "select group_id, group_name from groups"

#define QUERY_GROUPS_POOL "select group_id, user_id from groups_pool"

#define QUERY_CONSUMERS \
	"select consumer_id, consumer_name, user_id, time_start, time_end from consumers"

#define QUERY_ACCUNITS \
	"select unit_id, unit_name, consumer_id, local, time_start, time_end from units"

#define QUERY_UNIT_IP_POOL \
	"select " \
		"self_id, unit_id, inet_addr, inet_flag, " \
		"time_start, time_end, nice " \
	"from unit_ip_pool"

#define QUERY_GATEWAYS "select gw_id, gw_name, gw_ip from gateways"
#define QUERY_OPERATORS "select oper_id, oper_name from operators"
#define QUERY_GWLINKS \
	"select " \
		"gwlink_id, oper_id, gw_id, link, " \
		"time_start, time_end " \
	"from gwlinks"

GQuark mbb_db_load_error_quark(void)
{
	return g_quark_from_static_string("mbb-db-load-error-quark");
}

static inline gboolean check_isnull(gchar *s, GError **error)
{
	if (s == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "unexpected null value");
		return TRUE;
	}

	return FALSE;
}

static gboolean conv_uint(guint *p, gchar *s, GError **error)
{
	if (check_isnull(s, error))
		return FALSE;

	if (! var_conv_uint(s, p)) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid integer: %s", s);
		return FALSE;
	}

	return TRUE;
}

static gboolean conv_int(gint *p, gchar *s, GError **error)
{
	if (s == NULL)
		return TRUE;

	if (! var_conv_int(s, p)) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid integer: %s", s);
		return FALSE;
	}

	return TRUE;
}

static gboolean conv_time(time_t *p, gchar *s, GError **error)
{
	if (s == NULL)
		return TRUE;

	if (! var_conv_utime(s, p)) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid integer: %s", s);
		return FALSE;
	}

	return TRUE;
}

static gboolean conv_bool(gboolean *p, gchar *s, GError **error)
{
	if (check_isnull(s, error))
		return FALSE;

	if (! strcmp(s, "t") || ! strcmp(s, "true"))
		*p = TRUE;
	else if (! strcmp(s, "f") || ! strcmp(s, "false"))
		*p = FALSE;
	else {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid bool: %s", s);
		return FALSE;
	}

	return TRUE;
}

typedef gboolean (*load_func_t)(MbbDbIter *iter, Trash *trash, GError **error);

static gboolean mbb_db_load(gchar *query, load_func_t func, GError **error)
{
	MbbDbIter *iter;
	Trash trash = TRASH_INITIALIZER;

	iter = mbb_db_query_iter(query, error);
	if (iter == NULL)
		return FALSE;

	while (mbb_db_iter_next(iter)) {
		if (func(iter, &trash, error) == FALSE) {
			mbb_db_iter_free(iter);
			trash_empty(&trash);
			return FALSE;
		}
	}

	mbb_db_iter_free(iter);
	trash_release_data(&trash);
	return TRUE;
}

static gboolean load_users(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbUser *user;
	guint user_id;
	gchar *user_name;
	gchar *user_secret;

	if (! conv_uint(&user_id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	user_name = mbb_db_iter_value(iter, 1);
	user_secret = mbb_db_iter_value(iter, 2);
	user = mbb_user_new(user_id, user_name, user_secret);
	mbb_user_join(user);
	trash_push(trash, user, (GDestroyNotify) mbb_user_remove);

	return TRUE;
}

gboolean mbb_db_load_users(GError **error)
{
	return mbb_db_load(QUERY_USERS, load_users, error);
}

static gboolean load_groups(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbGroup *group;
	guint group_id;
	gchar *group_name;

	if (! conv_uint(&group_id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	group_name = mbb_db_iter_value(iter, 1);
	group = mbb_group_new(group_id, group_name);
	mbb_group_join(group);
	trash_push(trash, group, (GDestroyNotify) mbb_group_remove);

	return TRUE;
}

gboolean mbb_db_load_groups(GError **error)
{
	return mbb_db_load(QUERY_GROUPS, load_groups, error);
}

static gboolean load_groups_pool(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbGroup *group;
	MbbUser *user;
	guint group_id, user_id;

	(void) trash;

	if (! conv_uint(&group_id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	if (! conv_uint(&user_id, mbb_db_iter_value(iter, 1), error))
		return FALSE;

	if ((group = mbb_group_get_by_id(group_id)) == NULL)
		msg_warn("unknown group with id %d", group_id);
	else if ((user = mbb_user_get_by_id(user_id)) == NULL)
		msg_warn("unknown user with id %d", user_id);
	else
		mbb_group_add_user(group, user);

	return TRUE;
}

gboolean mbb_db_load_groups_pool(GError **error)
{
	return mbb_db_load(QUERY_GROUPS_POOL, load_groups_pool, error);
}

static gboolean load_consumers(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbConsumer *con;
	guint con_id;
	gint user_id;
	gchar *con_name;
	time_t start, end;

	if (! conv_uint(&con_id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	con_name = mbb_db_iter_value(iter, 1);

	user_id = -1;
	if (! conv_int(&user_id, mbb_db_iter_value(iter, 2), error))
		return FALSE;

	start = 0;
	if (! conv_time(&start, mbb_db_iter_value(iter, 3), error))
		return FALSE;

	end = 0;
	if (! conv_time(&end, mbb_db_iter_value(iter, 4), error))
		return FALSE;

	con = mbb_consumer_new(con_id, con_name, start, end);
	mbb_consumer_join(con);

	if (user_id >= 0) {
		MbbUser *user;

		user = mbb_user_get_by_id(user_id);
		if (user == NULL)
			msg_warn("unknown user with id %d", user_id);
		else {
			mbb_user_add_consumer(user, con);
			con->user = user;
		}
	}

	trash_push(trash, con, (GDestroyNotify) mbb_consumer_remove);

	return TRUE;
}

gboolean mbb_db_load_consumers(GError **error)
{
	return mbb_db_load(QUERY_CONSUMERS, load_consumers, error);
}

static gboolean load_units(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbUnit *unit;
	guint unit_id;
	gint con_id;
	gchar *unit_name;
	gboolean local;
	time_t start, end;

	if (! conv_uint(&unit_id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	con_id = -1;
	if (! conv_int(&con_id, mbb_db_iter_value(iter, 2), error))
		return FALSE;

	if (! conv_bool(&local, mbb_db_iter_value(iter, 3), error))
		return FALSE;

	unit_name = mbb_db_iter_value(iter, 1);
	start = -1;
	if (! conv_time(&start, mbb_db_iter_value(iter, 4), error))
		return FALSE;

	end = -1;
	if (! conv_time(&end, mbb_db_iter_value(iter, 5), error))
		return FALSE;

	unit = mbb_unit_new(unit_id, unit_name, start, end);
	unit->local = local;
	mbb_unit_join(unit);

	if (con_id >= 0) {
		MbbConsumer *con;

		con = mbb_consumer_get_by_id(con_id);
		if (con == NULL)
			msg_warn("unknown consumer with id %d", con_id);
		else {
			mbb_consumer_add_unit(con, unit);
			unit->con = con;
		}
	}

	trash_push(trash, unit, (GDestroyNotify) mbb_unit_remove);

	return TRUE;
}

gboolean mbb_db_load_units(GError **error)
{
	return mbb_db_load(QUERY_ACCUNITS, load_units, error);
}

static gboolean load_ip_pool(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbInetPoolEntry *entry;
	MbbUnit *unit;

	guint id, unit_id;
	struct inet inet;
	gboolean flag;
	time_t start, end;
	guint nice;

	if (! conv_uint(&id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	if (! conv_uint(&unit_id, mbb_db_iter_value(iter, 1), error))
		return FALSE;

	unit = mbb_unit_get_by_id(unit_id);
	if (unit == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INSIDE,
			    "no such unit %d", unit_id);
		return FALSE;
	}

	if (atoinet(mbb_db_iter_value(iter, 2), &inet) == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid inet: %s", mbb_db_iter_value(iter, 2));
		return FALSE;
	}

	if (! conv_bool(&flag, mbb_db_iter_value(iter, 3), error))
		return FALSE;

	start = -1;
	if (! conv_time(&start, mbb_db_iter_value(iter, 4), error))
		return FALSE;

	end = -1;
	if (! conv_time(&end, mbb_db_iter_value(iter, 5), error))
		return FALSE;

	if (! conv_uint(&nice, mbb_db_iter_value(iter, 6), error))
		return FALSE;

	entry = mbb_inet_pool_entry_new(&unit->self);
	entry->id = id;
	entry->inet = inet;
	entry->flag = flag;
	entry->start = start;
	entry->end = end;
	entry->nice = nice;

	mbb_unit_add_inet(unit, entry);
	trash_push(trash, entry, (GDestroyNotify) mbb_inet_pool_entry_delete);

	return TRUE;
}

gboolean mbb_db_load_unit_inet_pool(GError **error)
{
	return mbb_db_load(QUERY_UNIT_IP_POOL, load_ip_pool, error);
}

static gboolean load_gateway(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbGateway *gw;
	struct inet inet;
	gchar *name;
	guint id;

	if (! conv_uint(&id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	name = mbb_db_iter_value(iter, 1);

	if (atoipv4(mbb_db_iter_value(iter, 2), &inet) == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INVALID,
			    "invalid inet: %s", mbb_db_iter_value(iter, 2));
		return FALSE;
	}

	gw = mbb_gateway_new(id, name, inet.addr);
	mbb_gateway_join(gw);
	trash_push(trash, gw, (GDestroyNotify) mbb_gateway_remove);

	return TRUE;
}

gboolean mbb_db_load_gateways(GError **error)
{
	return mbb_db_load(QUERY_GATEWAYS, load_gateway, error);
}

static gboolean load_operator(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbOperator *op;
	gchar *name;
	guint id;

	if (! conv_uint(&id, mbb_db_iter_value(iter, 0), error))
		return FALSE;

	name = mbb_db_iter_value(iter, 1);

	op = mbb_operator_new(id, name);
	mbb_operator_join(op);
	trash_push(trash, op, (GDestroyNotify) mbb_operator_remove);

	return TRUE;
}

gboolean mbb_db_load_operators(GError **error)
{
	return mbb_db_load(QUERY_OPERATORS, load_operator, error);
}

static gboolean load_gwlink(MbbDbIter *iter, Trash *trash, GError **error)
{
	MbbGateway *gw;
	MbbOperator *op;
	MbbGwLink *gl;
	guint gw_id, op_id;
	time_t start, end;
	guint link;
	guint id;

	if (! conv_uint(&id, mbb_db_iter_value(iter, 0), error))
		return FALSE;
	if (! conv_uint(&op_id, mbb_db_iter_value(iter, 1), error))
		return FALSE;
	if (! conv_uint(&gw_id, mbb_db_iter_value(iter, 2), error))
		return FALSE;
	if (! conv_uint(&link, mbb_db_iter_value(iter, 3), error))
		return FALSE;

	start = end = 0;

	if (! conv_time(&start, mbb_db_iter_value(iter, 4), error))
		return FALSE;

	if (! conv_time(&end, mbb_db_iter_value(iter, 5), error))
		return FALSE;

	gw = mbb_gateway_get_by_id(gw_id);
	if (gw == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INSIDE,
			"no such gateway %d", gw_id);
		return FALSE;
	}

	op = mbb_operator_get_by_id(op_id);
	if (op == NULL) {
		g_set_error(error, MBB_DB_LOAD_ERROR, MBB_DB_LOAD_ERROR_INSIDE,
			"no such operator %d", op_id);
		return FALSE;
	}

	gl = mbb_gwlink_new(id);
	gl->gw = gw;
	gl->op = op;
	gl->link = link;
	gl->start = start;
	gl->end = end;

	mbb_gwlink_join(gl);
	if (! mbb_gateway_add_link(gw, gl, NULL))
		msg_warn("failed add link %d to gateway %s", gl->id, gw->name);
	mbb_operator_add_link(op, gl);

	trash_push(trash, gl, (GDestroyNotify) mbb_gwlink_remove);

	return TRUE;
}

gboolean mbb_db_load_gwlinks(GError **error)
{
	return mbb_db_load(QUERY_GWLINKS, load_gwlink, error);
}

