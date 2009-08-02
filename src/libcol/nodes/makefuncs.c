#include "col-internal.h"
#include "nodes/copyfuncs.h"
#include "nodes/makefuncs.h"
#include "types/schema.h"

AstProgram *
make_program(const char *name, List *defines, List *facts,
             List *rules, apr_pool_t *p)
{
    AstProgram *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_PROGRAM;
    result->name = apr_pstrdup(p, name);
    result->defines = list_copy_deep(defines, p);
    result->facts = list_copy_deep(facts, p);
    result->rules = list_copy_deep(rules, p);
    return result;
}

AstDefine *
make_define(const char *name, List *keys, List *schema, apr_pool_t *p)
{
    AstDefine *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_DEFINE;
    result->name = apr_pstrdup(p, name);
    result->keys = list_copy(keys, p);
    result->schema = list_copy_str(schema, p);
    return result;
}

AstRule *
make_rule(const char *name, bool is_delete, bool is_network,
          AstTableRef *head, List *joins, List *quals, apr_pool_t *p)
{
    AstRule *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_RULE;
    result->is_delete = is_delete;
    result->is_network = is_network;
    result->head = copy_node(head, p);

    if (name)
        result->name = apr_pstrdup(p, name);
    if (joins)
        result->joins = list_copy_deep(joins, p);
    if (quals)
        result->quals = list_copy_deep(quals, p);

    return result;
}

AstFact *
make_fact(AstTableRef *head, apr_pool_t *p)
{
    AstFact *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_FACT;
    result->head = copy_node(head, p);
    return result;
}

AstTableRef *
make_table_ref(const char *name, List *cols, apr_pool_t *p)
{
    AstTableRef *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_TABLE_REF;
    result->name = apr_pstrdup(p, name);
    result->cols = list_copy_deep(cols, p);
    return result;
}

AstColumnRef *
make_column_ref(bool has_loc_spec, ColNode *expr, apr_pool_t *p)
{
    AstColumnRef *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_COLUMN_REF;
    result->has_loc_spec = has_loc_spec;
    result->expr = copy_node(expr, p);
    return result;
}

AstJoinClause *
make_join_clause(AstTableRef *ref, bool not, AstHashVariant hash_v,
                 apr_pool_t *p)
{
    AstJoinClause *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_JOIN_CLAUSE;
    result->ref = copy_node(ref, p);
    result->not = not;
    result->hash_variant = hash_v;
    return result;
}

AstQualifier *
make_qualifier(ColNode *expr, apr_pool_t *p)
{
    AstQualifier *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_QUALIFIER;
    result->expr = copy_node(expr, p);
    return result;
}

AstOpExpr *
make_op_expr(ColNode *lhs, ColNode *rhs, AstOperKind op_kind, apr_pool_t *p)
{
    AstOpExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_OP_EXPR;
    result->lhs = copy_node(lhs, p);
    result->rhs = copy_node(rhs, p);
    result->op_kind = op_kind;
    return result;
}

AstVarExpr *
make_var_expr(const char *name, DataType type, apr_pool_t *p)
{
    AstVarExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_VAR_EXPR;
    result->name = apr_pstrdup(p, name);
    result->type = type;
    return result;
}

AstConstExpr *
make_const_expr(AstConstKind c_kind, const char *value, apr_pool_t *p)
{
    AstConstExpr *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = AST_CONST_EXPR;
    result->const_kind = c_kind;
    result->value = apr_pstrdup(p, value);
    return result;
}

FilterPlan *
make_filter_plan(List *quals, const char *tbl_name, apr_pool_t *p)
{
    FilterPlan *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = PLAN_FILTER;
    result->quals = list_copy_deep(quals, p);
    result->tbl_name = apr_pstrdup(p, tbl_name);
    return result;
}

InsertPlan *
make_insert_plan(AstTableRef *head, bool is_network, apr_pool_t *p)
{
    InsertPlan *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = PLAN_INSERT;
    result->head = copy_node(head, p);
    result->is_network = is_network;
    return result;
}

ScanPlan *
make_scan_plan(List *quals, const char *tbl_name, apr_pool_t *p)
{
    ScanPlan *result = apr_pcalloc(p, sizeof(*result));
    result->node.kind = PLAN_SCAN;
    result->quals = list_copy_deep(quals, p);
    result->tbl_name = apr_pstrdup(p, tbl_name);
    return result;
}
