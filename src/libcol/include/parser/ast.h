#ifndef AST_H
#define AST_H

#include "types/schema.h"
#include "util/list.h"

typedef enum AstNodeKind
{
    AST_PROGRAM,
    AST_DEFINE,
    AST_RULE,
    AST_FACT,
    AST_TABLE_REF,
    AST_COLUMN_REF,
    AST_JOIN_CLAUSE,
    AST_PREDICATE,
    AST_ASSIGN,
    AST_OP_EXPR,
    AST_CONST_EXPR_BOOL,
    AST_CONST_EXPR_CHAR,
    AST_CONST_EXPR_DOUBLE,
    AST_CONST_EXPR_INT,
    AST_CONST_EXPR_STRING,
    AST_VAR_EXPR
} AstNodeKind;

typedef struct AstNode
{
    AstNodeKind kind;
} AstNode;

typedef struct AstProgram
{
    AstNode node;
    char *name;
    List *clauses;
} AstProgram;

typedef struct AstDefine
{
    AstNode node;
    char *name;
    List *keys;
    List *schema;
} AstDefine;

typedef struct AstTableRef
{
    AstNode node;
    char *name;
    List *cols;
} AstTableRef;

typedef struct AstFact
{
    AstNode node;
    AstTableRef *head;
} AstFact;

typedef struct AstRule
{
    AstNode node;
    char *name;
    AstTableRef *head;
    List *body;
    bool is_delete;
    /* Filled-in during parse-analysis */
    bool is_network;
} AstRule;

typedef enum AstHashVariant
{
    AST_HASH_NONE = 0,
    AST_HASH_DELETE,
    AST_HASH_INSERT
} AstHashVariant;

typedef struct AstJoinClause
{
    AstNode node;
    AstTableRef *ref;
    bool not;
    AstHashVariant hash_variant;
} AstJoinClause;

typedef struct AstPredicate
{
    AstNode node;
    AstNode *expr;
} AstPredicate;

typedef struct AstColumnRef
{
    AstNode node;
    bool has_loc_spec;
    AstNode *expr;
} AstColumnRef;

typedef struct AstAssign
{
    AstNode node;
    AstColumnRef *lhs;
    AstNode *rhs;
} AstAssign;

typedef enum AstOperKind
{
    AST_OP_PLUS,
    AST_OP_MINUS,
    AST_OP_TIMES,
    AST_OP_DIVIDE,
    AST_OP_MODULUS,
    AST_OP_UMINUS,
    AST_OP_LT,
    AST_OP_LTE,
    AST_OP_GT,
    AST_OP_GTE,
    AST_OP_EQ,
    AST_OP_NEQ,
    AST_OP_ASSIGN
} AstOperKind;

typedef struct AstOpExpr
{
    AstNode node;
    AstNode *lhs;
    AstNode *rhs;
    AstOperKind op_kind;
} AstOpExpr;

typedef struct AstConstExprBool
{
    AstNode node;
    bool val;
} AstConstExprBool;

typedef struct AstConstExprChar
{
    AstNode node;
    unsigned char val;
} AstConstExprChar;

typedef struct AstConstExprDouble
{
    AstNode node;
    char *val;
} AstConstExprDouble;

typedef struct AstConstExprInt
{
    AstNode node;
    apr_int64_t val;
} AstConstExprInt;

typedef struct AstConstExprString
{
    AstNode node;
    char *val;
} AstConstExprString;

typedef struct AstVarExpr
{
    AstNode node;
    char *name;
    /* Filled-in by the analysis phase */
    DataType type;
} AstVarExpr;

#endif  /* AST_H */
