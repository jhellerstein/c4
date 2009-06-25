%{
/* XXX: should be a pure-parser */
#include "col-internal.h"
#include "parser/makefuncs.h"
/* XXX: see note about #include order in parser.c */
#include "parser/parser-internal.h"
#include "ol_scan.h"
#include "util/list.h"

int yyerror(ColParser *context, void *scanner, const char *message);

#define parser_alloc(sz)        apr_pcalloc(context->pool, (sz))
%}

%union
{
    apr_int64_t     ival;
    char           *str;
    List           *list;
    void           *ptr;
    bool            boolean;
    AstHashVariant  hash_v;
}

%start program
%error-verbose
%parse-param { ColParser *context }
%parse-param { void *scanner }
%lex-param { yyscan_t scanner }

%token KEYS DEFINE PROGRAM DELETE NOTIN OL_HASH_INSERT OL_HASH_DELETE
       OL_ASSIGN OL_FALSE OL_TRUE
%token <str> IDENT FCONST SCONST
%token <ival> ICONST

%nonassoc OL_ASSIGN
%left OL_EQ OL_NEQ
%nonassoc '>' '<' OL_GTE OL_LTE
%left '+' '-'
%left '*' '/' '%'
%left UMINUS

%type <ptr>     clause define rule table_ref column_ref join_clause
%type <str>     program_header
%type <list>    program_body opt_int_list int_list ident_list define_schema
%type <list>    opt_keys column_ref_list opt_rule_body rule_body
%type <ptr>     rule_body_elem predicate pred_expr expr const_expr op_expr
%type <ptr>     var_expr column_ref_expr rule_prefix
%type <boolean> opt_not bool_const opt_delete
%type <hash_v>  opt_hash_variant

%%
program: program_header program_body {
    AstProgram *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_PROGRAM;
    n->name = $1;
    n->clauses = $2;
    context->result = n;
};

program_header: PROGRAM IDENT ';' { $$ = $2; };

program_body:
  clause ';' program_body       { $$ = list_prepend($3, $1); }
| /* EMPTY */                   { $$ = list_make(context->pool); }
;

clause:
  define        { $$ = $1; }
| rule          { $$ = $1; }
;

define: DEFINE '(' IDENT ',' opt_keys define_schema ')' {
    AstDefine *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_DEFINE;
    n->name = $3;
    n->keys = $5;
    n->schema = $6;
    $$ = n;
};

/*
 * Note that we currently equate an empty key list with an absent key list;
 * this is inconsistent with JOL
 */
opt_keys:
  KEYS '(' opt_int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

opt_int_list:
  int_list      { $$ = $1; }
| /* EMPTY */   { $$ = NULL; }
;

int_list:
  ICONST                { $$ = list_make1_int($1, context->pool); }
| int_list ',' ICONST   { $$ = list_append_int($1, $3); }
;

ident_list:
  IDENT                 { $$ = list_make1($1, context->pool); }
| ident_list ',' IDENT  { $$ = list_append($1, $3); }
;

rule: rule_prefix opt_rule_body {
    AstRule *rule = (AstRule *) $1;

    if ($2 == NULL)
    {
        /* A rule without a body is actually a fact */
        AstFact *n = parser_alloc(sizeof(*n));
        n->node.kind = AST_FACT;
        n->head = rule->head;

        if (rule->name != NULL)
            ERROR("Cannot assign a name to facts");
        if (rule->is_delete)
            ERROR("Cannot specify \"delete\" in a fact");

        $$ = n;
    }
    else
    {
        rule->body = $2;
        $$ = rule;
    }
};

rule_prefix:
  IDENT opt_delete table_ref    { $$ = make_rule($1, $2, $3, context->pool); }
| DELETE table_ref              { $$ = make_rule(NULL, true, $2, context->pool); }
| table_ref                     { $$ = make_rule(NULL, false, $1, context->pool); }
;

opt_delete:
DELETE { $$ = true; }
| { $$ = false; }
;

opt_rule_body:
  ':' '-' rule_body     { $$ = $3; }
| /* EMPTY */           { $$ = NULL; }
;

rule_body:
  rule_body_elem                { $$ = list_make1($1, context->pool); }
| rule_body ',' rule_body_elem  { $$ = list_append($1, $3); }
;

rule_body_elem:
  join_clause
| predicate
;

join_clause: opt_not IDENT opt_hash_variant '(' column_ref_list ')' {
    AstJoinClause *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_JOIN_CLAUSE;
    n->not = $1;
    n->hash_variant = $3;
    n->ref = make_table_ref($2, $5, context->pool);
};

opt_not:
  NOTIN                 { $$ = true; }
| /* EMPTY */           { $$ = false; }
;

opt_hash_variant:
  OL_HASH_DELETE       { $$ = AST_HASH_DELETE; }
| OL_HASH_INSERT       { $$ = AST_HASH_INSERT; }
| /* EMPTY */          { $$ = AST_HASH_NONE; }
;

table_ref: IDENT '(' column_ref_list ')' { $$ = make_table_ref($1, $3, context->pool); };

/* XXX: Temp hack to workaround parser issues */
predicate: '^' pred_expr { $$ = make_predicate($2, context->pool); };

expr:
  op_expr
| const_expr
| var_expr
;

op_expr:
  expr '+' expr         { $$ = make_op_expr($1, $3, AST_OP_PLUS, context->pool); }
| expr '-' expr         { $$ = make_op_expr($1, $3, AST_OP_MINUS, context->pool); }
| expr '*' expr         { $$ = make_op_expr($1, $3, AST_OP_TIMES, context->pool); }
| expr '/' expr         { $$ = make_op_expr($1, $3, AST_OP_DIVIDE, context->pool); }
| expr '%' expr         { $$ = make_op_expr($1, $3, AST_OP_MODULUS, context->pool); }
| '-' expr              { $$ = make_op_expr($2, NULL, AST_OP_UMINUS, context->pool); }
| pred_expr             { $$ = $1; }
;

/*
 * Note that we treat assignment as a predicate in the parser, because it
 * would be painful to teach Bison how to tell the difference. Instead, we
 * distinguish between assigments and predicates in the semantic analysis
 * phase, and construct an AstAssign node there.
 */
pred_expr:
  expr '<' expr         { $$ = make_op_expr($1, $3, AST_OP_LT, context->pool); }
| expr '>' expr         { $$ = make_op_expr($1, $3, AST_OP_GT, context->pool); }
| expr OL_LTE expr      { $$ = make_op_expr($1, $3, AST_OP_LTE, context->pool); }
| expr OL_GTE expr      { $$ = make_op_expr($1, $3, AST_OP_GTE, context->pool); }
| expr OL_EQ expr       { $$ = make_op_expr($1, $3, AST_OP_EQ, context->pool); }
| expr OL_NEQ expr      { $$ = make_op_expr($1, $3, AST_OP_NEQ, context->pool); }
| column_ref OL_ASSIGN expr { $$ = make_op_expr($1, $3, AST_OP_ASSIGN, context->pool); }
;

const_expr:
  ICONST
{
    AstConstExprInt *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_INT;
    n->val = $1;
    $$ = n;
}
| bool_const
{
    AstConstExprBool *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_INT;
    n->val = $1;
    $$ = n;
}
| FCONST
{
    AstConstExprDouble *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_DOUBLE;
    n->val = $1;
    $$ = n;
}
| SCONST
{
    AstConstExprString *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_STRING;
    n->val = $1;
    $$ = n;
}
;

bool_const:
  OL_TRUE       { $$ = true; }
| OL_FALSE      { $$ = false; }
;

var_expr: IDENT {
    AstVarExpr *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_VAR_EXPR;
    n->name = $1;
    $$ = n;
};

column_ref_list:
  column_ref { $$ = list_make1($1, context->pool); }
| column_ref_list ',' column_ref { $$ = list_append($1, $3); }
;

column_ref:
  '@' column_ref_expr   { $$ = make_column_ref(true, $2, context->pool); }
| column_ref_expr       { $$ = make_column_ref(false, $1, context->pool); }
;

column_ref_expr:
  const_expr
| var_expr
;

%%

int
yyerror(ColParser *context, void *scanner, const char *message)
{
    printf("Parse error: %s\n", message);
    return 0;   /* return value ignored */
}
