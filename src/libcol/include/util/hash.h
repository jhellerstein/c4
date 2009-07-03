/*
 * ColHash is a hash table implementation based on the apr_hash code from
 * APR. This version taken from APR trunk, as of July 3 11:00 AM PST
 * (r791000). ~nrc
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

#ifndef COL_HASH_H
#define COL_HASH_H

/**
 * When passing a key to col_hash_set or col_hash_get, this value can be
 * passed to indicate a string-valued key, and have col_hash compute the
 * length automatically.
 *
 * @remark col_hash will use strlen(key) for the length. The NUL terminator
 *         is not included in the hash value (why throw a constant in?).
 *         Since the hash table merely references the provided key (rather
 *         than copying it), col_hash_this() will return the NUL-term'd key.
 */
#define COL_HASH_KEY_STRING     (-1)

/**
 * Abstract type for hash tables.
 */
typedef struct col_hash_t col_hash_t;

/**
 * Abstract type for scanning hash tables.
 */
typedef struct col_hash_index_t col_hash_index_t;

/**
 * Callback functions for calculating hash values.
 * @param key The key.
 * @param klen The length of the key, or COL_HASH_KEY_STRING to use the string 
 *             length. If COL_HASH_KEY_STRING then returns the actual key length.
 */
typedef unsigned int (*col_hashfunc_t)(const char *key, apr_ssize_t *klen);

/**
 * Callback functions for determining whether two hash table keys are equal.
 * @param k1 First key
 * @param k2 Second key
 * @param klen The length of both keys; the keys must be of equal length to
 *             be candidates for equality. The length will never be
 *             COL_HASH_KEY_STRING.
 * @return Zero if the two keys should be considered equal, non-zero otherwise.
 * @remark The default key comparison function is memcmp().
 */
typedef int (*col_keycomp_func_t)(const void *k1, const void *k2,
                                  apr_size_t klen);

/**
 * The default hash function.
 */
unsigned int col_hashfunc_default(const char *key, apr_ssize_t *klen);

/**
 * Create a hash table.
 * @param pool The pool to allocate the hash table out of
 * @return The hash table just created
  */
col_hash_t *col_hash_make(apr_pool_t *pool);

/**
 * Create a hash table with a custom hash function
 * @param pool The pool to allocate the hash table out of
 * @param hash_func A custom hash function
 * @param cmp_func A custom key comparison function
 * @return The hash table just created
  */
col_hash_t *col_hash_make_custom(apr_pool_t *pool, 
                                 col_hashfunc_t hash_func,
                                 col_keycomp_func_t cmp_func);

/**
 * Make a copy of a hash table
 * @param pool The pool from which to allocate the new hash table
 * @param h The hash table to clone
 * @return The hash table just created
 * @remark Makes a shallow copy
 */
col_hash_t *col_hash_copy(apr_pool_t *pool, const col_hash_t *h);

/**
 * Associate a value with a key in a hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @param klen Length of the key. Can be APR_HASH_KEY_STRING to use the string length.
 * @param val Value to associate with the key
 * @remark If the value is NULL the hash entry is deleted.
 */
void col_hash_set(col_hash_t *ht, const void *key,
                  apr_ssize_t klen, const void *val);

/**
 * Associated a value with a key in the hash table, iff the key is not
 * already in the hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @param klen Length of the key. Can be COL_HASH_KEY_STRING to use the
 *             string length.
 * @param val Value to associate with the key, if the key is not already in
 *            the hash table. Cannot be NULL.
 * @return The value associated with the key. If key is new, this is "val";
 *         otherwise, the hash table is unmodified, and the result is
 *         equivalent to col_hash_get(ht, key, klen).
 */
void *col_hash_set_if_new(col_hash_t *ht, const void *key,
                          apr_ssize_t klen, const void *val);


/**
 * Look up the value associated with a key in a hash table.
 * @param ht The hash table
 * @param key Pointer to the key
 * @param klen Length of the key. Can be APR_HASH_KEY_STRING to use the string length.
 * @return Returns NULL if the key is not present.
 */
void *col_hash_get(col_hash_t *ht, const void *key, apr_ssize_t klen);

/**
 * Start iterating over the entries in a hash table.
 * @param p The pool to allocate the col_hash_index_t iterator. If this
 *          pool is NULL, then an internal, non-thread-safe iterator is used.
 * @param ht The hash table
 * @remark  There is no restriction on adding or deleting hash entries during
 * an iteration (although the results may be unpredictable unless all you do
 * is delete the current entry) and multiple iterations can be in
 * progress at the same time.
 */
col_hash_index_t *col_hash_first(apr_pool_t *p, col_hash_t *ht);

/**
 * Continue iterating over the entries in a hash table.
 * @param hi The iteration state
 * @return a pointer to the updated iteration state.  NULL if there are no more  
 *         entries.
 */
col_hash_index_t *col_hash_next(col_hash_index_t *hi);

/**
 * Get the current entry's details from the iteration state.
 * @param hi The iteration state
 * @param key Return pointer for the pointer to the key.
 * @param klen Return pointer for the key length.
 * @param val Return pointer for the associated value.
 * @remark The return pointers should point to a variable that will be set to the
 *         corresponding data, or they may be NULL if the data isn't interesting.
 */
void col_hash_this(col_hash_index_t *hi, const void **key, 
                   apr_ssize_t *klen, void **val);

/**
 * Get the number of key/value pairs in the hash table.
 * @param ht The hash table
 * @return The number of key/value pairs in the hash table.
 */
unsigned int col_hash_count(col_hash_t *ht);

/**
 * Clear any key/value pairs in the hash table.
 * @param ht The hash table
 */
void col_hash_clear(col_hash_t *ht);

/**
 * Merge two hash tables into one new hash table. The values of the overlay
 * hash override the values of the base if both have the same key.  Both
 * hash tables must use the same hash function.
 * @param p The pool to use for the new hash table
 * @param overlay The table to add to the initial table
 * @param base The table that represents the initial values of the new table
 * @return A new hash table containing all of the data from the two passed in
 */
col_hash_t *col_hash_overlay(apr_pool_t *p,
                             const col_hash_t *overlay, 
                             const col_hash_t *base);

/**
 * Merge two hash tables into one new hash table. If the same key
 * is present in both tables, call the supplied merge function to
 * produce a merged value for the key in the new table.  Both
 * hash tables must use the same hash function.
 * @param p The pool to use for the new hash table
 * @param h1 The first of the tables to merge
 * @param h2 The second of the tables to merge
 * @param merger A callback function to merge values, or NULL to
 *  make values from h1 override values from h2 (same semantics as
 *  col_hash_overlay())
 * @param data Client data to pass to the merger function
 * @return A new hash table containing all of the data from the two passed in
 */
col_hash_t *col_hash_merge(apr_pool_t *p,
                           const col_hash_t *h1,
                           const col_hash_t *h2,
                           void * (*merger)(apr_pool_t *p,
                                            const void *key,
                                            apr_ssize_t klen,
                                            const void *h1_val,
                                            const void *h2_val,
                                            const void *data),
                           const void *data);

/**
 * Declaration prototype for the iterator callback function of col_hash_do().
 *
 * @param rec The data passed as the first argument to col_hash_[v]do()
 * @param key The key from this iteration of the hash table
 * @param klen The key length from this iteration of the hash table
 * @param value The value from this iteration of the hash table
 * @remark Iteration continues while this callback function returns non-zero.
 * To export the callback function for col_hash_do() it must be declared 
 * in the _NONSTD convention.
 */
typedef int (col_hash_do_callback_fn_t)(void *rec, const void *key,
                                        apr_ssize_t klen,
                                        const void *value);

/** 
 * Iterate over a hash table running the provided function once for every
 * element in the hash table. The @param comp function will be invoked for
 * every element in the hash table.
 *
 * @param comp The function to run
 * @param rec The data to pass as the first argument to the function
 * @param ht The hash table to iterate over
 * @return FALSE if one of the comp() iterations returned zero; TRUE if all
 *            iterations returned non-zero
 * @see col_hash_do_callback_fn_t
 */
int col_hash_do(col_hash_do_callback_fn_t *comp,
                void *rec, const col_hash_t *ht);

/** @} */

#endif	/* !APR_HASH_H */
