#include "mbbmodule.h"
#include "mbbxmlmsg.h"
#include "mbbplock.h"
#include "mbbunit.h"
#include "mbbfunc.h"
#include "mbblock.h"
#include "mbblog.h"
#include "mbbxtv.h"
#include "mbbvar.h"
#include "mbbdb.h"

#include "varconv.h"
#include "inet.h"

#include "xmltag.h"
#include "macros.h"

#include "pingerloop.h"

#include <string.h>

#define PINGER_DEFAULT_PERIOD 60
#define PINGER_WARN_TIMEOUT 600

typedef enum {
	PINGER_ERROR_ATTR_UNSET,
	PINGER_ERROR_CONV,
	PINGER_ERROR_INVALID_VALUE
} PingerError;

#define PINGER_ERROR (pinger_error_quark())

static GQuark pinger_error_quark(void);

struct attr {
	gchar *name;
	gchar *value;
};

struct attr_map_elem {
	gchar *attr;
	guint off;
	gboolean (*conv)(gchar *, gpointer);
	gboolean need;
};

struct attr_map {
	struct attr_map_elem *elem;
	gsize count;
	gchar *group;
};

struct pinger_unit {
	MbbUnit *unit;

	ipv4_t client;
	ipv4_t gw;
	ipv4_t neighbour;

	guint period;
	guint warn_timeout;
};

#define PUNIT_OFFSET(field) G_STRUCT_OFFSET(struct pinger_unit, field)

static struct attr_map_elem punit_map_elem[] = {
	{ "pinger.ip.client", PUNIT_OFFSET(client), var_conv_ipv4, TRUE },
	{ "pinger.ip.gateway", PUNIT_OFFSET(gw), var_conv_ipv4, FALSE },
	{ "pinger.ip.neighbour", PUNIT_OFFSET(neighbour), var_conv_ipv4, FALSE },
	{ "pinger.time.period", PUNIT_OFFSET(period), var_conv_uint, FALSE },
	{ "pinger.time.warn", PUNIT_OFFSET(warn_timeout), var_conv_uint, FALSE }
};

static struct attr_map punit_map = {
	.elem = punit_map_elem,
	.count = NELEM(punit_map_elem),
	.group = "unit"
};

static gint (*attr_read_ptr)(gchar *, gint, struct attr *, gsize, GError **);

#define mbb_attr_read(group, id, av, len, err) \
	(*attr_read_ptr)(group, id, av, len, err)

static guint pinger_default_period = PINGER_DEFAULT_PERIOD;
static guint pinger_warn_timeout = PINGER_WARN_TIMEOUT;

static GHashTable *ht = NULL;

static void pinger_unit_free(struct pinger_unit *pu)
{
	mbb_unit_unref(pu->unit);
	g_free(pu);
}

static inline void ht_init(void)
{
	ht = g_hash_table_new_full(
		g_direct_hash, g_direct_equal,
		NULL, (GDestroyNotify) pinger_unit_free
	);
}

static GQuark pinger_error_quark(void)
{
	return g_quark_from_string("pinger-error-quark");
}

static gboolean attr_do_map(gpointer ptr, gint id, struct attr_map *map,
			    GError **error)
{
	gboolean ret = FALSE;
	struct attr *av;
	guint n;

	av = g_new0(struct attr, map->count);
	for (n = 0; n < map->count; n++)
		av[n].name = map->elem[n].attr;

	if (mbb_attr_read(map->group, id, av, map->count, error) < 0)
		goto out;

	for (n = 0; n < map->count; n++) {
		struct attr_map_elem *elem = &map->elem[n];
		struct attr *attr = &av[n];
		gpointer p;

		if (attr->value == NULL) {
			if (elem->need) {
				g_set_error(error,
					PINGER_ERROR, PINGER_ERROR_ATTR_UNSET,
					"attr %s is not set", attr->name);
				goto out;
			}

			continue;
		}

		p = G_STRUCT_MEMBER_P(ptr, elem->off);
		if (! elem->conv(attr->value, p)) {
			g_set_error(error, PINGER_ERROR, PINGER_ERROR_CONV,
				"attr '%s': invalid value '%s'",
				attr->name, attr->value);
			goto out;
		}
	}

	ret = TRUE;
out:
	for (n = 0; n < map->count; n++)
		g_free(av[n].value);
	g_free(av);

	return ret;
}

static struct pinger_unit *pinger_unit_new(MbbUnit *unit, GError **error)
{
	struct pinger_unit punit;

	memset(&punit, 0, sizeof(punit));
	punit.unit = unit;

	if (attr_do_map(&punit, unit->id, &punit_map, error) == FALSE)
		return NULL;

	if (punit.client == 0) {
		g_set_error(error, PINGER_ERROR, PINGER_ERROR_INVALID_VALUE,
			"attr %s is zero", punit_map_elem[0].attr);
		return NULL;
	}

	if (punit.period == 0)
		punit.period = pinger_default_period;

	if (punit.warn_timeout == 0)
		punit.warn_timeout = pinger_warn_timeout;

	if (punit.warn_timeout < 2 * punit.period) {
		g_set_error(error, PINGER_ERROR, PINGER_ERROR_INVALID_VALUE,
			"condition %s < 2 * %s failed",
			punit_map_elem[3].attr, punit_map_elem[4].attr);
		return NULL;
	}

	return g_memdup(&punit, sizeof(punit));
}

static inline void pinger_add_unit(struct pinger_unit *pu)
{
	g_hash_table_insert(ht, GINT_TO_POINTER(pu->unit->id), pu);
}

static inline void pinger_del_unit(gint id)
{
	g_hash_table_remove(ht, GINT_TO_POINTER(id));
}

static inline struct pinger_unit *pinger_get_unit(gint id)
{
	return g_hash_table_lookup(ht, GINT_TO_POINTER(id));
}

static MbbUnit *mbb_unit_ref_by_name(gchar *name)
{
	MbbUnit *unit;

	mbb_lock_reader_lock();
	unit = mbb_unit_get_by_name(name);
	if (unit != NULL)
		mbb_unit_ref(unit);
	mbb_lock_reader_unlock();

	return unit;
}

static MbbUnit *mbb_unit_ref_by_id(gint id)
{
	MbbUnit *unit;

	mbb_lock_reader_lock();
	unit = mbb_unit_get_by_id(id);
	if (unit != NULL)
		mbb_unit_ref(unit);
	mbb_lock_reader_unlock();

	return unit;
}

static void load_units(GSList *list)
{
	struct pinger_unit *pu;
	MbbUnit *unit;
	gint id;

	ht_init();

	for (; list != NULL; list = list->next) {
		GError *error = NULL;

		id = GPOINTER_TO_INT(list->data);
		unit = mbb_unit_ref_by_id(id);

		if (unit == NULL) {
			mbb_log_self("unknown unit with id %d", id);
			continue;
		}

		pu = pinger_unit_new(unit, &error);

		if (pu != NULL)
			pinger_add_unit(pu);
		else {
			mbb_log_self("unit %s: %s", unit->name, error->message);
			mbb_unit_unref(unit);
			g_error_free(error);
		}
	}
}

static gboolean db_pinger_load(void)
{
	GError *error = NULL;
	MbbDbIter *iter;
	GSList *list;

	iter = mbb_db_select(&error, "pinger_units", "unit_id", NULL, NULL);

	if (iter == NULL) {
		mbb_log_self("db_pinger_load failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	list = NULL;
	while (mbb_db_iter_next(iter)) {
		gchar *var;
		gint id;

		var = mbb_db_iter_value(iter, 0);
		if (! var_conv_int(var, &id)) {
			mbb_log_self("non integer unit_id '%s'", var);

			g_slist_free(list);
			mbb_db_iter_free(iter);
			return FALSE;
		}

		list = g_slist_prepend(list, GINT_TO_POINTER(id));
	}

	load_units(list);

	g_slist_free(list);
	mbb_db_iter_free(iter);

	return TRUE;
}

static gboolean db_pinger_add(gint id, GError **error)
{
	return mbb_db_insert(error, "pinger_units", "d", "unit_id", id);
}

static gboolean db_pinger_del(gint id, GError **error)
{
	return mbb_db_delete(error, "pinger_units", "unit_id = %d", id);
}

static void mbb_pinger_add(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	struct pinger_unit *pu;
	GError *error;
	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	if ((unit = mbb_unit_ref_by_name(name)) == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	on_final { mbb_unit_unref(unit); }

	error = NULL;
	pu = pinger_unit_new(unit, &error);
	if (pu == NULL) final
		*ans = mbb_xml_msg_from_error(error);

	mbb_plock_writer_lock();
	if (pinger_get_unit(unit->id) == NULL) {
		if (db_pinger_add(unit->id, &error) == FALSE) final {
			mbb_plock_writer_unlock();
			*ans = mbb_xml_msg_from_error(error);
		}
	}

	pinger_add_unit(pu);
	mbb_plock_writer_unlock();
}

static void mbb_pinger_del(XmlTag *tag, XmlTag **ans)
{
	DEFINE_XTV(XTV_UNIT_NAME);

	MbbUnit *unit;
	gchar *name;

	MBB_XTV_CALL(&name);

	if ((unit = mbb_unit_ref_by_name(name)) == NULL) final
		*ans = mbb_xml_msg(MBB_MSG_UNKNOWN_UNIT);

	mbb_plock_writer_lock();
	if (pinger_get_unit(unit->id) != NULL) {
		GError *error = NULL;

		if (db_pinger_del(unit->id, &error) == FALSE) final {
			mbb_plock_writer_unlock();
			mbb_unit_unref(unit);
			*ans = mbb_xml_msg_from_error(error);
		}

		pinger_del_unit(unit->id);
	}
	mbb_plock_writer_unlock();

	mbb_unit_unref(unit);
}

static void mbb_pinger_show_units(XmlTag *tag G_GNUC_UNUSED, XmlTag **ans)
{
	struct pinger_unit *pu;
	GHashTableIter iter;

	*ans = mbb_xml_msg_ok();

	mbb_lock_reader_lock();
	mbb_plock_reader_lock();

	g_hash_table_iter_init(&iter, ht);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &pu)) {
		xml_tag_new_child(*ans, "unit",
			"name", variant_new_string(pu->unit->name)
		);
	}

	mbb_plock_reader_unlock();
	mbb_lock_reader_unlock();
}

static gboolean pinger_default_period_set(gchar *arg, gpointer p G_GNUC_UNUSED)
{
	guint val;

	if (! var_conv_uint(arg, &val))
		return FALSE;

	if (val == 0)
		val = PINGER_DEFAULT_PERIOD;

	if (2 * val >= pinger_warn_timeout)
		return FALSE;

	pinger_default_period = val;
	return TRUE;
}

static gboolean pinger_warn_timeout_set(gchar *arg, gpointer p G_GNUC_UNUSED)
{
	guint val;

	if (! var_conv_uint(arg, &val))
		return FALSE;

	if (val == 0)
		val = PINGER_WARN_TIMEOUT;

	if (val <= 2 * pinger_default_period)
		return FALSE;

	pinger_warn_timeout = val;
	return TRUE;
}

MBB_DEFINE_VAR(
	pinger_default_period, &pinger_default_period,
	var_str_uint, pinger_default_period_set,
	MBB_CAP_ALL, MBB_CAP_ADMIN
);

MBB_DEFINE_VAR(
	pinger_warn_timeout, &pinger_warn_timeout,
	var_str_uint, pinger_warn_timeout_set,
	MBB_CAP_ALL, MBB_CAP_ADMIN
);

MBB_FUNC_REGISTER_STRUCT("mbb-pinger-add", mbb_pinger_add, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-pinger-del", mbb_pinger_del, MBB_CAP_ADMIN);
MBB_FUNC_REGISTER_STRUCT("mbb-pinger-show-units", mbb_pinger_show_units, MBB_CAP_ADMIN);

static void load_module(void)
{
	mbb_module_sym_t sym;

	if ((sym = mbb_module_ref_sym("attr.so", "mbb_attr_read")) == NULL) {
		mbb_log_self("cannot find symbol attr.so:mbb_attr_read");
		return;
	}

	mbb_module_sym_assign(attr_read_ptr, sym);

	if (db_pinger_load() == FALSE)
		return;

	if (pinger_loop_init() == FALSE) {
		mbb_log_self("unable to create socket");
		return;
	}

	mbb_module_add_var(&MBB_VAR(pinger_default_period));
	mbb_module_add_var(&MBB_VAR(pinger_warn_timeout));

	mbb_module_add_func(&MBB_FUNC(mbb_pinger_add));
	mbb_module_add_func(&MBB_FUNC(mbb_pinger_del));
	mbb_module_add_func(&MBB_FUNC(mbb_pinger_show_units));
}

static void unload_module(void)
{
	pinger_loop_end();

	mbb_module_unref_sym((mbb_module_sym_t) attr_read_ptr);

	if (ht != NULL)
		g_hash_table_destroy(ht);
}

MBB_DEFINE_MODULE("pinger as is :)")
