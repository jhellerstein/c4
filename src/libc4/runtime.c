#include "c4-internal.h"
#include "net/network.h"
#include "router.h"
#include "runtime.h"
#include "storage/sqlite.h"
#include "types/catalog.h"

static void * APR_THREAD_FUNC instance_thread_main(apr_thread_t *thread,
                                                   void *data);

static Datum
get_local_addr(int port, apr_pool_t *tmp_pool)
{
    char buf[APRMAXHOSTLEN + 1];
    char addr[APRMAXHOSTLEN + 1 + 20];
    apr_status_t s;

    s = apr_gethostname(buf, sizeof(buf), tmp_pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    snprintf(addr, sizeof(addr), "tcp:%s:%d", buf, port);
    printf("Local address = %s\n", addr);
    return string_from_str(addr);
}

static char *
get_user_home_dir(apr_pool_t *pool)
{
    apr_status_t s;
    apr_uid_t uid;
    apr_gid_t gid;
    char *user_name;
    char *home_dir;

    s = apr_uid_current(&uid, &gid, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_uid_name_get(&user_name, uid, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_uid_homepath_get(&home_dir, user_name, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return home_dir;
}

static char *
get_c4_base_dir(int port, apr_pool_t *pool, apr_pool_t *tmp_pool)
{
    char *home_dir;
    char *base_dir;
    apr_status_t s;

    home_dir = get_user_home_dir(tmp_pool);
    base_dir = apr_psprintf(pool, "%s/c4_home/tcp_%d",
                            home_dir, port);
    s = apr_dir_make_recursive(base_dir, APR_FPROT_OS_DEFAULT, tmp_pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    printf("c4: Using base_dir = %s\n", base_dir);
    return base_dir;
}

static C4Instance *
c4_instance_make(int port, apr_queue_t *queue)
{
    apr_status_t s;
    apr_pool_t *pool;
    C4Instance *c4;

    s = apr_pool_create(&pool, NULL);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    c4 = apr_pcalloc(pool, sizeof(*c4));
    c4->pool = pool;
    c4->tmp_pool = make_subpool(c4->pool);
    c4->log = logger_make(c4);
    c4->cat = cat_make(c4);
    c4->router = router_make(c4, queue);
    c4->net = network_make(c4, port);
    c4->port = network_get_port(c4->net);
    c4->local_addr = get_local_addr(c4->port, c4->tmp_pool);
    c4->base_dir = get_c4_base_dir(c4->port, c4->pool, c4->tmp_pool);
    c4->sql = sqlite_init(c4);

    return c4;
}

/*
 * Data that needs to be passed to a new C4 runtime thread during startup.
 * Annoyingly, we need to bundle this into a struct, because we're only allowed
 * to pass a single pointer.
 */
typedef struct RuntimeInitData
{
    apr_queue_t *queue;
    int port;
} RuntimeInitData;

/*
 * Invoked by the client API (in the client's thread) to create a new C4 runtime
 * as a separate thread. Note that we only allocate the thread itself in the
 * given pool; the C4 runtime itself uses a distinct top-level APR pool.
 */
apr_thread_t *
c4_instance_start(int port, apr_queue_t *queue, apr_pool_t *pool)
{
    apr_status_t s;
    apr_threadattr_t *thread_attr;
    apr_thread_t *thread;
    RuntimeInitData *init_data;

    init_data = ol_alloc(sizeof(*init_data));
    init_data->queue = queue;
    init_data->port = port;

    s = apr_threadattr_create(&thread_attr, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    s = apr_thread_create(&thread, thread_attr, instance_thread_main,
                          init_data, pool);
    if (s != APR_SUCCESS)
        FAIL_APR(s);

    return thread;
}

static void * APR_THREAD_FUNC
instance_thread_main(apr_thread_t *thread, void *data)
{
    RuntimeInitData *init_data = (RuntimeInitData *) data;
    C4Instance *c4;

    c4 = c4_instance_make(init_data->port, init_data->queue);
    ol_free(init_data);

    network_start(c4->net);
    router_main_loop(c4->router);

    /* Client initiated an orderly shutdown */
    network_destroy(c4->net);
    apr_pool_destroy(c4->pool);
    apr_thread_exit(thread, APR_SUCCESS);

    return NULL;        /* Return value ignored */
}