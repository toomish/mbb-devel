/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#include "mbbstat.h"
#include "mbbtask.h"
#include "mbbinit.h"
#include "mbblog.h"
#include "mbbvar.h"
#include "mbbdb.h"

#include "varconv.h"
#include "query.h"

#include "macros.h"

#include <string.h>

struct stat_rec {
	guint64 nbyte_in;
	guint64 nbyte_out;
	gchar key[];
};

#define STAT_REC_KEY(r) ((gpointer) r->key)

struct double_key {
	time_t point;
	guint aid;
	guint bid;
};

struct single_key {
	time_t point;
	guint id;
};

union key {
	time_t point;
	struct double_key dkey;
	struct single_key skey;
};

struct mbb_stat_pool {
	GHashTable *ustat;
	GHashTable *lstat;
	GHashTable *ulstat;
};

static gboolean double_key_equal(struct double_key *a, struct double_key *b)
{
	if (a->aid != b->aid)
		return FALSE;
	if (a->bid != b->bid)
		return FALSE;
	if (a->point != b->point)
		return FALSE;
	return TRUE;
}

static guint double_key_hash(struct double_key *key)
{
	return (key->aid | (key->bid << 20)) ^ key->point;
}

static gboolean single_key_equal(struct single_key *a, struct single_key *b)
{
	if (a->id != b->id)
		return FALSE;
	if (a->point != b->point)
		return FALSE;
	return TRUE;
}

static guint single_key_hash(struct single_key *key)
{
	return (key->id << 10) | key->point;
}

struct mbb_stat_pool *mbb_stat_pool_new(void)
{
	struct mbb_stat_pool *pool;

	pool = g_new(struct mbb_stat_pool, 1);
	pool->ustat = g_hash_table_new_full(
		(GHashFunc) single_key_hash, (GEqualFunc) single_key_equal,
		NULL, g_free
	);
	pool->lstat = g_hash_table_new_full(
		(GHashFunc) single_key_hash, (GEqualFunc) single_key_equal,
		NULL, g_free
	);
	pool->ulstat = g_hash_table_new_full(
		(GHashFunc) double_key_hash, (GEqualFunc) double_key_equal,
		NULL, g_free
	);

	return pool;
}

void mbb_stat_pool_free(struct mbb_stat_pool *pool)
{
	g_hash_table_destroy(pool->ustat);
	g_hash_table_destroy(pool->lstat);
	g_hash_table_destroy(pool->ulstat);
	g_free(pool);
}

static void rec_update(GHashTable *ht, gpointer key, gsize key_size,
		       struct stat_rec *data)
{
	struct stat_rec *rec;

	rec = g_hash_table_lookup(ht, key);
	if (rec == NULL) {
		rec = g_malloc0(sizeof(struct stat_rec) + key_size);
		memcpy(STAT_REC_KEY(rec), key, key_size);
		g_hash_table_insert(ht, STAT_REC_KEY(rec), rec);
	}

	rec->nbyte_in += data->nbyte_in;
	rec->nbyte_out += data->nbyte_out;
}

void mbb_stat_feed(struct mbb_stat_pool *pool, struct mbb_stat_entry *entry)
{
	struct stat_rec rec;
	union key key;

	rec.nbyte_in = entry->nbyte_in;
	rec.nbyte_out = entry->nbyte_out;

	key.point = mbb_stat_extract_hour(entry->point);

	key.skey.id = entry->unit_id;
	rec_update(pool->ustat, &key, sizeof(struct single_key), &rec);

	key.skey.id = entry->link_id;
	rec_update(pool->lstat, &key, sizeof(struct single_key), &rec);

	key.dkey.aid = entry->unit_id;
	key.dkey.bid = entry->link_id;
	rec_update(pool->ulstat, &key, sizeof(struct double_key), &rec);
}

static inline gboolean rec_save_query(gchar *query, const gchar *func)
{
	GError *error = NULL;

	if (! mbb_db_query(query, NULL, NULL, &error)) {
		mbb_log("%s: %s", func, error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static gboolean unit_link_rec_save(struct stat_rec *rec)
{
	struct double_key *key = STAT_REC_KEY(rec);
	gchar *query;

	query = query_function("update_unit_link_stat", "ddqqt",
		key->aid, key->bid, rec->nbyte_in, rec->nbyte_out, key->point
	);

	return rec_save_query(query, __FUNCTION__);
}

static gboolean unit_rec_save(struct stat_rec *rec)
{
	struct single_key *key = STAT_REC_KEY(rec);
	gchar *query;

	query = query_function("update_unit_stat", "dqqt",
		key->id, rec->nbyte_in, rec->nbyte_out, key->point
	);

	return rec_save_query(query, __FUNCTION__);
}

static gboolean link_rec_save(struct stat_rec *rec)
{
	struct single_key *key = STAT_REC_KEY(rec);
	gchar *query;

	query = query_function("update_link_stat", "dqqt",
		key->id, rec->nbyte_in, rec->nbyte_out, key->point
	);

	return rec_save_query(query, __FUNCTION__);
}

gboolean mbb_stat_pool_init(void)
{
	GError *error = NULL;

	if (! mbb_db_dup_conn(&error)) {
		mbb_log("mbb_db_dup_conn failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static inline gboolean db_begin(void)
{
	GError *error = NULL;

	if (! mbb_db_begin(&error)) {
		mbb_log("mbb_db_begin failed: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static inline void db_rollback(void)
{
	GError *error = NULL;

	if (! mbb_db_rollback(&error)) {
		mbb_log("mbb_db_rollback failed: %s", error->message);
		g_error_free(error);
	}
}

static inline void db_commit(void)
{
	GError *error = NULL;

	if (! mbb_db_commit(&error)) {
		mbb_log("mbb_db_commit failed: %s", error->message);
		g_error_free(error);
	}
}

static gboolean save_records(GHashTable *ht, gboolean (*save)(struct stat_rec *))
{
	struct stat_rec *rec;
	GHashTableIter iter;

	g_hash_table_iter_init(&iter, ht);
	while (g_hash_table_iter_next(&iter, NULL, (gpointer *) &rec)) {
		if (! mbb_task_poll_state() || ! save(rec))
			return FALSE;
	}

	return TRUE;
}

void mbb_stat_pool_save(struct mbb_stat_pool *pool)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	mbb_log_lvl_t mask;

	mbb_log_mask(LOG_MASK_DEL, MBB_LOG_QUERY, &mask);

	if (! g_static_mutex_trylock(&mutex)) {
		mbb_log("wait for mutex");
		g_static_mutex_lock(&mutex);
	}

	if (! mbb_task_poll_state())
		goto out;

	mbb_log("save to db");

	if (! db_begin())
		goto out;

	if (! save_records(pool->ustat, unit_rec_save))
		goto rollback;

	if (! save_records(pool->lstat, link_rec_save))
		goto rollback;

	if (! save_records(pool->ulstat, unit_link_rec_save))
		goto rollback;

	db_commit();
	goto out;

rollback:
	db_rollback();
out:
	mbb_log_mask(LOG_MASK_SET, mask, NULL);
	g_static_mutex_unlock(&mutex);
	mbb_stat_pool_free(pool);
}

static gboolean stat_table_clean(gchar *table, time_t start, time_t end,
				 GError **error)
{
	return mbb_db_update(error,
		table, "qq",
		"nbyte_in", (guint64) 0, "nbyte_out", (guint64) 0,
		"point >= %lld and point < %lld",
		(guint64) start, (guint64) end
	);
}

gboolean mbb_stat_db_wipe(time_t start, time_t end, GError **error)
{
	gchar *tables[] = { "unit_stat", "link_stat", "unit_link_stat" };

	if (! mbb_db_begin(error))
		return FALSE;

	for (guint n = 0; n < NELEM(tables); n++) {
		if (! stat_table_clean(tables[n], start, end, error)) {
			mbb_db_rollback(NULL);
			return FALSE;
		}
	}

	mbb_db_commit(NULL);

	return TRUE;
}

