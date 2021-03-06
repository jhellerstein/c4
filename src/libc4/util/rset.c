/*
 * RSet is a refcounted set type: each element of the set has a count of the
 * number of times it has been inserted. The count is decremented on removal;
 * when the count reaches zero, the element is deleted from the set.
 *
 * RSet is based on C4Hash, as of rev fad4ec5d1b9e (r635).
 */

/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <string.h>

#include "c4-internal.h"
#include "util/rset.h"

/*
 * The internal form of an rset.
 *
 * The table is an array indexed by the hash of the key; collisions are resolved
 * by hanging a linked list of rset entries off each element of the array.
 * Although this is a really simple design it isn't too bad given that pools
 * have a low allocation overhead.
 */

typedef struct rset_entry_t rset_entry_t;

struct rset_entry_t {
    rset_entry_t *next;
    void         *key;
    unsigned int  hash;
    unsigned int  refcount;
};

/*
 * Data structure for iterating through an rset.
 *
 * We keep a pointer to the next rset entry here to allow the current entry to
 * be freed or otherwise mangled between calls to rset_iter_next().
 */
struct rset_index_t {
    rset_t       *rs;
    rset_entry_t *this;
    rset_entry_t *next;
    unsigned int  index;
    bool          at_end;
};

/*
 * The size of the array is always a power of two. We use the maximum index
 * rather than the size so that we can use bitwise-AND for modular
 * arithmetic. The count of rset entries may be greater depending on the chosen
 * collision rate.
 *
 * We allocate the bucket array in a sub-pool, "array_pool". This allows us
 * to reclaim the old bucket array after an expansion.
 */
struct rset_t {
    apr_pool_t           *pool;
    apr_pool_t           *array_pool;
    rset_entry_t        **array;
    unsigned int          count, max;
    void                 *user_data;
    rset_hashfunc_t       hash_func;
    rset_keycomp_func_t   cmp_func;
    rset_entry_t         *free; /* List of recycled entries */
};

#define INITIAL_MAX 15 /* tunable == 2^n - 1 */


static rset_entry_t **alloc_array(rset_t *rs, unsigned int max)
{
   return apr_pcalloc(rs->array_pool, sizeof(*rs->array) * (max + 1));
}

rset_t *rset_make(apr_pool_t *pool, void *cb_data,
                  rset_hashfunc_t hash_func, rset_keycomp_func_t cmp_func)
{
    apr_pool_t *array_pool;
    rset_t *rs;

    if (apr_pool_create(&array_pool, pool) != APR_SUCCESS)
        return NULL;

    rs = apr_palloc(pool, sizeof(rset_t));
    rs->pool = pool;
    rs->array_pool = array_pool;
    rs->free = NULL;
    rs->count = 0;
    rs->max = INITIAL_MAX;
    rs->array = alloc_array(rs, rs->max);
    rs->user_data = cb_data;
    rs->hash_func = hash_func;
    rs->cmp_func = cmp_func;
    return rs;
}

/*
 * rset iteration functions.
 */
rset_index_t *rset_iter_make(apr_pool_t *p, rset_t *rs)
{
    rset_index_t *ri;

    ri = apr_palloc(p, sizeof(*ri));
    ri->rs = rs;
    rset_iter_reset(ri);

    return ri;
}

void rset_iter_reset(rset_index_t *ri)
{
    ri->index = 0;
    ri->this = NULL;
    ri->next = NULL;
    ri->at_end = false;
}

bool rset_iter_next(rset_index_t *ri)
{
    if (ri->at_end)
        return false;

    ri->this = ri->next;
    while (!ri->this) {
        if (ri->index > ri->rs->max) {
            ri->at_end = true;
            return false;
        }

        ri->this = ri->rs->array[ri->index++];
    }
    ri->next = ri->this->next;
    /*
     * GCC perf hack: We are very likely to read the next entry in the rset in
     * the near future. Hence, prefetch to try to avoid a cache miss.  It seems
     * to even be a win to do the conditional branch to prefetch the next bucket
     * as well.
     */
    __builtin_prefetch(ri->next);
    if (ri->index <= ri->rs->max)
        __builtin_prefetch(ri->rs->array[ri->index]);
    return true;
}

void *rset_this(rset_index_t *ri)
{
    if (ri->at_end)
        FAIL();

    return ri->this->key;
}

static void expand_array(rset_t *rs)
{
    apr_pool_t *new_array_pool;
    apr_pool_t *old_array_pool;
    rset_index_t *ri;
    rset_entry_t **new_array;
    unsigned int new_max;

    if (apr_pool_create(&new_array_pool, rs->pool) != APR_SUCCESS)
        return; /* Give up and don't try to expand the array */
    old_array_pool = rs->array_pool;
    rs->array_pool = new_array_pool;

    new_max = rs->max * 2 + 1;
    new_array = alloc_array(rs, new_max);

    ri = rset_iter_make(old_array_pool, rs);
    while (rset_iter_next(ri)) {
        unsigned int i = ri->this->hash & new_max;
        ri->this->next = new_array[i];
        new_array[i] = ri->this;
    }

    rs->array = new_array;
    rs->max = new_max;

    apr_pool_destroy(old_array_pool);
}

/*
 * This is where we keep the details of the hash function and control the
 * maximum collision rate.
 *
 * If "make_new" is true, it creates and initializes a new rset entry if there
 * isn't already one there; it returns an updatable pointer so that entries can
 * be removed.
 */
static rset_entry_t **find_entry(rset_t *rs, void *key, bool make_new)
{
    rset_entry_t **rep, *re;
    unsigned int hash;

    hash = rs->hash_func(key, rs->user_data);

    /* scan linked list */
    for (rep = &rs->array[hash & rs->max], re = *rep;
         re; rep = &re->next, re = *rep)
    {
        if (re->hash == hash &&
            rs->cmp_func(re->key, key, rs->user_data))
            break;
    }
    if (re || !make_new)
        return rep;

    /* add a new entry */
    if ((re = rs->free) != NULL)
        rs->free = re->next;
    else
        re = apr_palloc(rs->pool, sizeof(*re));
    re->next = NULL;
    re->hash = hash;
    re->key  = key;
    re->refcount = 0;
    *rep = re;
    rs->count++;
    return rep;
}

unsigned int rset_get(rset_t *rs, void *elem)
{
    rset_entry_t *entry;

    entry = *find_entry(rs, elem, false);
    if (entry)
        return entry->refcount;
    else
        return 0;
}

bool rset_add(rset_t *rs, void *elem)
{
    rset_entry_t *entry;

    entry = *find_entry(rs, elem, true);
    entry->refcount++;
    /* check that the collision rate isn't too high */
    if (rs->count > rs->max)
        expand_array(rs);

    return (entry->refcount == 1);
}

void *rset_remove(rset_t *rs, void *elem, unsigned int *new_refcount)
{
    rset_entry_t **rep;
    rset_entry_t *entry;

    rep = find_entry(rs, elem, false);
    if (*rep == NULL)
        return NULL;

    entry = *rep;
    entry->refcount--;
    *new_refcount = entry->refcount;
    if (entry->refcount == 0)
    {
        /* Delete entry */
        *rep = entry->next;
        entry->next = rs->free;
        rs->free = entry;
        rs->count--;
    }

    return entry->key;
}

unsigned int rset_count(rset_t *rs)
{
    return rs->count;
}
