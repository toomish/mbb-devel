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

struct urec_key {
	guint unit_id;
	guint link_id;
	time_t point;
};

struct urec {
	struct urec_key key;
	guint64 nbyte_in;
	guint64 nbyte_out;

};

struct lrec_key {
	guint link_id;
	time_t point;
};

struct lrec {
	struct lrec_key key;
	guint64 nbyte_in;
	guint64 nbyte_out;
};

struct mbb_stat_pool {
	GHashTable *ustat;
	GHashTable *lstat;
};

static gboolean urec_key_equal(struct urec_key *a, struct urec_key *b)
{
	if (a->unit_id != b->unit_id)
		return FALSE;
	if (a->link_id != b->link_id)
		return FALSE;
	if (a->point != b->point)
		return FALSE;
	return TRUE;
}

static guint urec_key_hash(struct urec_key *key)
{
	return (key->unit_id | (key->link_id << 20)) ^ key->point;
}

static gboolean lrec_key_equal(struct lrec_key *a, struct lrec_key *b)
{
	if (a->link_id != b->link_id)
		return FALSE;
	if (a->point != b->point)
		return FALSE;
	return TRUE;
}

static guint lrec_key_hash(struct lrec_key *key)
{
	return (key->link_id << 10) | key->point;
}

struct mbb_stat_pool *mbb_stat_pool_new(void)
{
	struct mbb_stat_pool *pool;

	pool = g_new(struct mbb_stat_pool, 1);
	pool->ustat = g_hash_table_new_full(
		(GHashFunc) urec_key_hash, (GEqualFunc) urec_key_equal,
		NULL, g_free
	);
	pool->lstat = g_hash_table_new_full(
		(GHashFunc) lrec_key_hash, (GEqualFunc) lrec_key_equal,
		NULL, g_free
	);

	return pool;
}

void mbb_stat_pool_free(struct mbb_stat_pool *pool)
{
	g_hash_table_destroy(pool->ustat);
	g_hash_table_destroy(pool->lstat);
	g_free(pool);
}

static void urec_update(struct mbb_stat_pool *pool,
			struct mbb_stat_entry *entry)
{
	struct urec_key key;
	struct urec *rec;

	key.unit_id = entry->unit_id;
	key.link_id = entry->link_id;
	key.point = entry->point;

	rec = g_hash_table_lookup(pool->ustat, &key);
	if (rec != NULL) {
		rec->nbyte_in += entry->nbyte_in;
		rec->nbyte_out += entry->nbyte_out;
	} else {
		rec = g_new(struct urec, 1);

		rec->key = key;
		rec->nbyte_in = entry->nbyte_in;
		rec->nbyte_out = entry->nbyte_out;

		g_hash_table_insert(pool->ustat, &rec->key, rec);
	}
}

static void lrec_update(struct mbb_stat_pool *pool,
			struct mbb_stat_entry *entry)
{
	struct lrec_key key;
	struct lrec *rec;

	key.link_id = entry->link_id;
	key.point = entry->point;

	rec = g_hash_table_lookup(pool->lstat, &key);
	if (rec != NULL) {
		rec->nbyte_in += entry->nbyte_in;
		rec->nbyte_out += entry->nbyte_out;
	} else {
		rec = g_new(struct lrec, 1);

		rec->key = key;
		rec->nbyte_in = entry->nbyte_in;
		rec->nbyte_out = entry->nbyte_out;

		g_hash_table_insert(pool->lstat, &rec->key, rec);
	}
}

void mbb_stat_feed(struct mbb_stat_pool *pool, struct mbb_stat_entry *entry)
{
	time_t tmp = entry->point;

	entry->point = mbb_stat_extract_hour(tmp);

	urec_update(pool, entry);
	lrec_update(pool, entry);

	entry->point = tmp;
}

static gboolean urec_save(struct urec *rec)
{
	struct urec_key *key = &rec->key;
	GError *error = NULL;
	gchar *query;

	query = query_function("update_unit_stat", "ddqqt",
		key->unit_id, key->link_id, rec->nbyte_in, rec->nbyte_out,
		key->point
	);

	if (! mbb_db_query(query, NULL, NULL, &error)) {
		mbb_log("urec_save: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
}

static gboolean lrec_save(struct lrec *rec)
{
	struct lrec_key *key = &rec->key;
	GError *error = NULL;
	gchar *query;

	query = query_function("update_link_stat", "dqqt",
		key->link_id, rec->nbyte_in, rec->nbyte_out, key->point
	);

	if (! mbb_db_query(query, NULL, NULL, &error)) {
		mbb_log("lrec_save: %s", error->message);
		g_error_free(error);
		return FALSE;
	}

	return TRUE;
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

void mbb_stat_pool_save(struct mbb_stat_pool *pool)
{
	static GStaticMutex mutex = G_STATIC_MUTEX_INIT;

	GHashTableIter iter;
	mbb_log_lvl_t mask;
	gpointer rec;

	if (! g_static_mutex_trylock(&mutex)) {
		mbb_log("wait for mutex");
		g_static_mutex_lock(&mutex);
	}

	mbb_log_mask(LOG_MASK_DEL, MBB_LOG_QUERY, &mask);

	if (! mbb_task_poll_state())
		goto out;

	mbb_log("save to db");

	if (! db_begin())
		goto out;

	g_hash_table_iter_init(&iter, pool->ustat);
	while (g_hash_table_iter_next(&iter, NULL, &rec)) {
		if (! mbb_task_poll_state() || ! urec_save(rec)) {
			db_rollback();
			goto out;
		}
	}

	g_hash_table_iter_init(&iter, pool->lstat);
	while (g_hash_table_iter_next(&iter, NULL, &rec)) {
		if (! mbb_task_poll_state() || ! lrec_save(rec)) {
			db_rollback();
			goto out;
		}
	}

	db_commit();

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
	if (! mbb_db_begin(error))
		return FALSE;

	if (! stat_table_clean("unit_stat", start, end, error)) {
		mbb_db_rollback(NULL);
		return FALSE;
	}

	if (! stat_table_clean("link_stat", start, end, error)) {
		mbb_db_rollback(NULL);
		return FALSE;
	}

	mbb_db_commit(NULL);

	return TRUE;
}

