#include "col-internal.h"
#include "operator/scan.h"
#include "operator/scancursor.h"
#include "types/sqlite_table.h"
#include "types/table.h"
#include "types/tuple.h"

static apr_status_t table_cleanup(void *data);
static int table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen);
static unsigned int table_hash_tuple(const char *key, apr_ssize_t *klen);

ColTable *
table_make(TableDef *def, ColInstance *col, apr_pool_t *pool, bool sql)
{
    ColTable *tbl;

    tbl = apr_pcalloc(pool, sizeof(*tbl));
    tbl->pool = pool;
    tbl->col = col;
    tbl->def = def;

	if (sql)
		tbl->sql_table = sqlite_table_make(tbl);
	else
		tbl->tuples = col_hash_make_custom(pool,
										   table_hash_tuple,
										   table_cmp_tuple);

    apr_pool_cleanup_register(pool, tbl, table_cleanup,
                              apr_pool_cleanup_null);

    return tbl;
}

/*
 * Unpin the tuples contained in this table.
 */
static apr_status_t
table_cleanup(void *data)
{
    ColTable *tbl = (ColTable *) data;
    col_hash_index_t *hi;

    for (hi = col_hash_first(tbl->pool, tbl->tuples);
         hi != NULL; hi = col_hash_next(hi))
    {
        Tuple *t;

        col_hash_this(hi, (const void **) &t, NULL, NULL);
        tuple_unpin(t);
    }

    return APR_SUCCESS;
}

static int
table_cmp_tuple(const void *k1, const void *k2, apr_size_t klen)
{
    Tuple *t1 = (Tuple *) k1;
    Tuple *t2 = (Tuple *) k2;

    ASSERT(klen == sizeof(Tuple *));

    if (tuple_equal(t1, t2))
        return 0;
    else
        return 1;
}

static unsigned int
table_hash_tuple(const char *key, apr_ssize_t *klen)
{
    Tuple *t = (Tuple *) key;

    ASSERT(*klen == sizeof(Tuple *));
    return tuple_hash(t);
}

/*
 * Insert the tuple into this table. Returns "true" if the tuple was added;
 * returns false if the insert was a no-op because the tuple is already
 * contained by this table.
 */
bool
table_insert(ColTable *tbl, Tuple *t)
{
    Tuple *val;
	bool notdup;

	if (tbl->sql_table)
	    notdup = sqlite_table_insert(tbl->def->table, t);
    else
    {
		val = col_hash_set_if_new(tbl->tuples, t, sizeof(t), t);
		tuple_pin(val);
		notdup = (bool) (t == val);
	}

    return notdup;
}

Tuple *
table_scan_first(ColTable *tbl, ScanCursor *cur)
{
	Tuple *ret_tuple;

	cur->hi = NULL;
	cur->sqlite_stmt = NULL;
	if (tbl->sql_table)
		return sqlite_table_scan_first(tbl, cur);
	else
    {
		cur->hi = col_hash_first(NULL, tbl->tuples);
		col_hash_this(cur->hi, (const void **) &ret_tuple, NULL, NULL);
		return ret_tuple;
	}
}

Tuple *
table_scan_next(ColTable *tbl, ScanCursor *cur)
{
	Tuple *ret_tuple;

	if (tbl->sql_table)
		return sqlite_table_scan_next(tbl, cur);
	else {
		cur->hi = col_hash_next(cur->hi);
		if (cur->hi == NULL)
			return NULL;
		col_hash_this(cur->hi, (const void **) &ret_tuple, NULL, NULL);
		return ret_tuple;
	}
}
