/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbmodule.h"

#include "mbbxmlmsg.h"
#include "mbbthread.h"
#include "mbbstat.h"
#include "mbbunit.h"
#include "mbbinit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbblog.h"
#include "mbbxtv.h"
#include "mbbdb.h"

#include "macros.h"
#include "xmltag.h"
#include "query.h"

#include <stdarg.h>
#include <string.h>

#include "interface.h"

struct stat_opt {
	guint32 block_size;
	gboolean human;

	time_t start;
	time_t end;
	gchar *step;
};

struct size_sym {
	gchar *sym;
	guint pow;
} size_syms[] = {
	{ "b", 0 },
	{ "k", 10 },
	{ "m", 20 },
	{ "g", 30 }
};

enum {
	SQI_UNIT,
	SQI_CONSUMER,
	SQI_OPER,
	SQI_LINK,
	SQI_GW
};

#define SQI(name) (&sqi_table[SQI_##name])

struct sqi {
	gchar *tables;
	gchar *field;
	gchar *cond;
	gchar *opt_cond;
} sqi_table[] = { {
	"unit_stat s, units u", "u.unit_name", "s.unit_id = u.unit_id", NULL
}, {
	"unit_stat s, units u, consumers c", "c.consumer_name",
	"s.unit_id = u.unit_id and u.consumer_id = c.consumer_id", NULL
}, {
	"link_stat s, gwlinks l, operators o", "o.oper_name",
	"s.gwlink_id = l.gwlink_id and l.oper_id = o.oper_id", NULL
}, {
	"link_stat s", "gwlink_id", NULL, NULL
}, {
	"link_stat s, gwlinks l, gateways g", "g.gw_name",
	"s.gwlink_id = l.gwlink_id and l.gw_id = g.gw_id", NULL
} };

static void import_stat(MbbDbIter *iter, XmlTag *tag)
{
	while (mbb_db_iter_next(iter)) {
		xml_tag_new_child(tag, "stat",
			"name", variant_new_string(mbb_db_iter_value(iter, 0)),
			"in", variant_new_string(mbb_db_iter_value(iter, 1)),
			"out", variant_new_string(mbb_db_iter_value(iter, 2))
		);
	}
}

static void import_dstat(MbbDbIter *iter, XmlTag *tag)
{
	while (mbb_db_iter_next(iter)) {
		xml_tag_new_child(tag, "stat",
			"name", variant_new_string(mbb_db_iter_value(iter, 0)),
			"in", variant_new_string(mbb_db_iter_value(iter, 1)),
			"out", variant_new_string(mbb_db_iter_value(iter, 2)),
			"point", variant_new_string(mbb_db_iter_value(iter, 3))
		);
	}
}

static XmlTag *stat_exec_query(gchar *query, gboolean bystep, gboolean reorder)
{
	GError *error = NULL;
	MbbDbIter *iter;
	XmlTag *tag;

	iter = mbb_db_query_iter(query, &error);

	if (iter == NULL)
		return mbb_xml_msg_from_error(error);

	tag = mbb_xml_msg_ok();
	if (bystep)
		import_dstat(iter, tag);
	else
		import_stat(iter, tag);
	mbb_db_iter_free(iter);

	if (reorder)
		xml_tag_reorder_all(tag);

	return tag;
}

#define POINT_IN_COND "s.point >= %t and s.point < %t"
#define POINT_BY_COND "s.point >= ptou(d.date) and s.point < ptou(d.date + '1 %S')"

#define ROUND_FMT(field) "round(sum(" #field ") / %d, 3)"

static gchar *stat_query(struct sqi *sqi, GSList *names, struct stat_opt *so)
{
	query_format("select %S", sqi->field);
	if (so->human == FALSE || so->block_size == 1)
		query_append(", sum(nbyte_in), sum(nbyte_out)");
	else {
		query_append_format(", " ROUND_FMT(nbyte_in), so->block_size);
		query_append_format(", " ROUND_FMT(nbyte_out), so->block_size);
	}

	if (so->step == NULL)
		query_append_format(" from %S", sqi->tables);
	else {
		if (so->human == FALSE)
			query_append(", ptou(d.date)");
		else
			query_append(", to_char(d.date, 'YYYY-MM-DD HH24-MI')");

		query_append_format(" from %S, dates d", sqi->tables);
	}

	query_append(" where ");
	if (names != NULL) {
		query_append_format("%S in (%s", sqi->field, names->data);
		for (names = names->next; names != NULL; names = names->next)
			query_append_format(", %s", names->data);
		query_append(") and ");
	}

	if (sqi->cond != NULL) {
		query_append_format("%S and ", sqi->cond);

		if (sqi->opt_cond != NULL)
			query_append_format("%S and ", sqi->opt_cond);
	}

	if (so->step != NULL)
		query_append_format(POINT_BY_COND, so->step);
	else
		query_append_format(POINT_IN_COND, so->start, so->end);

	query_append_format(" group by %S", sqi->field);

	if (so->step != NULL)
		query_append(", d.date");

	if (so->human) {
		if (so->step == NULL)
			query_append_format(" order by %S", sqi->field);
		else
			query_append_format(" order by d.date, %S", sqi->field);
	}

	return query_append(";");
}

static XmlTag *stat_fetch(struct sqi *sqi, GSList *names, struct stat_opt *so)
{
	gchar *query;

	query = stat_query(sqi, names, so);

	return stat_exec_query(query, so->step != NULL, so->human);
}

static gboolean create_view_dates(time_t start, time_t end, gchar *step,
				  GError **error)
{
	gchar *query;

	query = query_function("create_view_dates",
		"sstt", "dates", step, start, end
	);

	return mbb_db_query(query, NULL, NULL, error);
}

static inline void drop_view_dates(void)
{
	mbb_db_query("drop view dates;", NULL, NULL, NULL);
}

gboolean parse_time_args(time_t *start, time_t *end, gchar *period, gchar *step,
			 XmlTag **ans)
{
	static gchar *step_name[] = { "hour", "day", "month", "year" };

	if (step != NULL) {
		gboolean valid = FALSE;
		guint n;

		for (n = 0; n < NELEM(step_name); n++)
			if (! strcmp(step_name[n], step)) {
				valid = TRUE;
				break;
			}

		if (! valid) {
			*ans = mbb_xml_msg_error("invalid step name");
			return FALSE;
		}
	}

	if (period == NULL) {
		if (*end == VAR_CONV_TIME_UNSET)
			*end = *start + 1;
		else if (*start >= *end) {
			*ans = mbb_xml_msg(MBB_MSG_INVALID_TIME_ORDER);
			return FALSE;
		}
	} else {
		void (*date_inc)(GDate *, guint);
		struct tm tm;
		gint d, m, y;
		GDate date;

		if (*end != VAR_CONV_TIME_UNSET) {
			*ans = mbb_xml_msg_error("exclusive arguments");
			return FALSE;
		}

		localtime_r(start, &tm);
		tm.tm_sec = 0;
		tm.tm_min = 0;
		tm.tm_hour = 0;

		if (! strcmp(period, "day"))
			date_inc = g_date_add_days;
		else if (! strcmp(period, "month")) {
			date_inc = g_date_add_months;
			tm.tm_mday = 1;
		} else if (! strcmp(period, "year")) {
			date_inc = g_date_add_years;
			tm.tm_mday = 1;
			tm.tm_mon = 0;
		} else {
			*ans = mbb_xml_msg_error("invalid period value");
			return FALSE;
		}

		d = tm.tm_mday;
		m = tm.tm_mon + 1;
		y = tm.tm_year + 1900;

		if (! g_date_valid_dmy(d, m, y)) {
			*ans = mbb_xml_msg_error(
				"g_date_valid_dmy(%d, %d, %d) failed", d, m, y
			);
			return FALSE;
		}

		*start = mktime(&tm);
		g_date_clear(&date, 1);
		g_date_set_dmy(&date, d, m, y);
		date_inc(&date, 1);
		g_date_to_struct_tm(&date, &tm);
		*end = mktime(&tm);
	}

	return TRUE;
}

static GSList *get_name_list(GSList *list)
{
	GSList *orig_list = list;

	for (; list != NULL; list = list->next)
		list->data = variant_get_string(list->data);

	return orig_list;
}

static gboolean so_size_conv(gchar *arg, gpointer p)
{
	guint n;

	for (n = 0; n < NELEM(size_syms); n++) {
		if (! strcmp(arg, size_syms[n].sym)) {
			*(guint32 *) p = 1 << size_syms[n].pow;
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean stat_opt_parse(struct stat_opt *so, XmlTag *tag, XmlTag **ans)
{
	static struct xml_tag_var so_size_xtv = {
		.path = "opt",
		.name = "size",
		.conv = so_size_conv
	};

	static struct xml_tag_var so_order_xtv = {
		.path = "opt",
		.name = "human",
		.conv = var_conv_bool
	};

	DEFINE_XTVN(xtvars, &so_size_xtv, XTV_TRUE, &so_order_xtv, XTV_TRUE);

	so->block_size = 1;
	so->human = FALSE;

	*ans = mbb_xtv_call(tag, xtvars, &so->block_size, &so->human);

	return *ans == NULL;
}

static void stat_common(XmlTag *tag, XmlTag **ans, struct sqi *sqi)
{
	DEFINE_XTV(XTV_TIME_START, XTV_TIME_END_, XTV_TIME_PERIOD_, XTV_TIME_STEP_);

	GError *error = NULL;
	struct stat_opt so;
	time_t start, end;
	gchar *period;
	GSList *names;

	so.step = period = NULL;
	end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&start, &end, &period, &so.step);

	if (! parse_time_args(&start, &end, period, so.step, ans))
		return;

	if (stat_opt_parse(&so, tag, ans) == FALSE)
		return;

	so.start = start;
	so.end = end;

	names = get_name_list(xml_tag_path_attr_list(tag, "name", "value"));

	on_final { g_slist_free(names); }

	if (! mbb_db_dup_conn_once(&error)) final
		*ans = mbb_xml_msg_from_error(error);

	if (so.step == NULL)
		*ans = stat_fetch(sqi, names, &so);
	else {
		if (! create_view_dates(start, end, so.step, &error)) final
			*ans = mbb_xml_msg_from_error(error);

		*ans = stat_fetch(sqi, names, &so);

		drop_view_dates();
	}

	g_slist_free(names);
}

static void stat_unit(XmlTag *tag, XmlTag **ans)
{
	stat_common(tag, ans, SQI(UNIT));
}

static void stat_consumer(XmlTag *tag, XmlTag **ans)
{
	stat_common(tag, ans, SQI(CONSUMER));
}

static void stat_link(XmlTag *tag, XmlTag **ans)
{
	stat_common(tag, ans, SQI(LINK));
}

static void stat_operator(XmlTag *tag, XmlTag **ans)
{
	stat_common(tag, ans, SQI(OPER));
}

static void stat_gateway(XmlTag *tag, XmlTag **ans)
{
	stat_common(tag, ans, SQI(GW));
}

static void self_sqi_init(struct sqi *sqi, gchar *name)
{
	sqi->field = name;
	sqi->tables = SQI(CONSUMER)->tables;
	sqi->cond = SQI(CONSUMER)->cond;

	sqi->opt_cond = g_strdup_printf(
		"c.user_id = %d", mbb_thread_get_uid()
	);
}

static void self_stat_unit(XmlTag *tag, XmlTag **ans)
{
	struct sqi sqi;

	self_sqi_init(&sqi, "u.unit_name");
	stat_common(tag, ans, &sqi);
	g_free(sqi.opt_cond);
}

static void self_stat_consumer(XmlTag *tag, XmlTag **ans)
{
	struct sqi sqi;

	self_sqi_init(&sqi, "c.consumer_name");
	stat_common(tag, ans, &sqi);
	g_free(sqi.opt_cond);
}

static void stat_wipe(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_TIME_START, XTV_TIME_END_, XTV_TIME_PERIOD_);

	GError *error = NULL;
	time_t start, end;
	gchar *period;

	period = NULL;
	end = VAR_CONV_TIME_UNSET;
	MBB_XTV_CALL(&start, &end, &period);

	if (! parse_time_args(&start, &end, period, NULL, ans))
		return;

	if (! mbb_stat_db_wipe(start, end, &error))
		*ans = mbb_xml_msg_from_error(error);
}

MBB_FUNC_REGISTER_STRUCT("mbb-stat-unit", stat_unit, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-stat-consumer", stat_consumer, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-stat-operator", stat_operator, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-stat-link", stat_link, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-stat-gateway", stat_gateway, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-stat-wipe", stat_wipe, MBB_CAP_WHEEL);

MBB_FUNC_REGISTER_STRUCT("mbb-self-stat-unit", self_stat_unit, MBB_CAP_CONS);
MBB_FUNC_REGISTER_STRUCT("mbb-self-stat-consumer", self_stat_consumer, MBB_CAP_CONS);

static void load_module(void)
{
	static struct stat_interface si = {
		.pool_init = mbb_stat_pool_init,

		.pool_new = mbb_stat_pool_new,
		.pool_feed = mbb_stat_feed,
		.pool_save = mbb_stat_pool_save,
		.pool_free = mbb_stat_pool_free,

		.db_wipe = mbb_stat_db_wipe,
		.parse_tag = parse_time_args
	};

	mbb_module_export(&si);

	mbb_module_add_func(&MBB_FUNC(stat_unit));
	mbb_module_add_func(&MBB_FUNC(stat_consumer));
	mbb_module_add_func(&MBB_FUNC(stat_operator));
	mbb_module_add_func(&MBB_FUNC(stat_link));
	mbb_module_add_func(&MBB_FUNC(stat_gateway));
	mbb_module_add_func(&MBB_FUNC(stat_wipe));
	mbb_module_add_func(&MBB_FUNC(self_stat_unit));
	mbb_module_add_func(&MBB_FUNC(self_stat_consumer));
}

static void unload_module(void)
{
}

MBB_DEFINE_MODULE("stat methods")
