#include <apr_hash.h>

#include "col-internal.h"
#include "types/catalog.h"

struct ColCatalog
{
    ColInstance *col;
    apr_pool_t *pool;

    apr_hash_t *schema_tbl;
};

ColCatalog *
cat_make(ColInstance *col)
{
    apr_pool_t *pool;
    ColCatalog *cat;

    pool = make_subpool(col->pool);
    cat = apr_pcalloc(pool, sizeof(*cat));
    cat->col = col;
    cat->pool = pool;
    cat->schema_tbl = apr_hash_make(cat->pool);

    return cat;
}

Schema *
cat_get_schema(ColCatalog *cat, const char *name)
{
    return apr_hash_get(cat->schema_tbl, name, APR_HASH_KEY_STRING);
}

void
cat_set_schema(ColCatalog *cat, const char *name, Schema *schema)
{
    if (cat_get_schema(cat, name) != NULL)
        ERROR("duplicate schema definition: %s", name);

    apr_hash_set(cat->schema_tbl, name,
                 APR_HASH_KEY_STRING, schema);
}

bool
is_valid_type_name(const char *type_name)
{
    return (bool) (lookup_type_name(type_name) != TYPE_INVALID);
}

DataType
lookup_type_name(const char *type_name)
{
    if (strcmp(type_name, "bool") == 0)
        return TYPE_BOOL;
    if (strcmp(type_name, "char") == 0)
        return TYPE_CHAR;
    if (strcmp(type_name, "double") == 0)
        return TYPE_DOUBLE;
    if (strcmp(type_name, "int") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int2") == 0)
        return TYPE_INT2;
    if (strcmp(type_name, "int4") == 0)
        return TYPE_INT4;
    if (strcmp(type_name, "int8") == 0)
        return TYPE_INT8;
    if (strcmp(type_name, "string") == 0)
        return TYPE_STRING;

    return TYPE_INVALID;
}
