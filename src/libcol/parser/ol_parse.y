%{
/* XXX: should be a pure-parser */
#include "col-internal.h"
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

%token KEYS DEFINE PROGRAM DELETE OLG_HASH_INSERT OLG_HASH_DELETE
%token <str> IDENT FCONST SCONST
%token <ival> ICONST

%type <ptr>     clause define rule table_ref column_ref
%type <str>     program_header
%type <list>    program_body opt_int_list int_list ident_list define_schema
%type <list>    opt_keys column_ref_list opt_rule_body rule_body
%type <ptr>     rule_body_elem assignment expr const_expr op_expr
%type <boolean> opt_delete opt_loc_spec
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

program_body: clause ';' program_body { $$ = list_prepend($3, $1); }
| /* EMPTY */ { $$ = list_make(context->pool); }
;

clause: define { $$ = $1; };
| rule { $$ = $1; }
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
opt_keys: KEYS '(' opt_int_list ')' ',' { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

define_schema: '{' ident_list '}' { $$ = $2; };

opt_int_list: int_list { $$ = $1; }
| /* EMPTY */ { $$ = NULL; }
;

int_list:
  ICONST { $$ = list_make1_int($1, context->pool); }
| int_list ',' ICONST { $$ = list_append_int($1, $3); }
;

ident_list:
  IDENT { $$ = list_make1($1, context->pool); }
| ident_list ',' IDENT { $$ = list_append($1, $3); }
;

rule: opt_delete table_ref opt_rule_body {
    if ($3 == NULL)
    {
        /* A rule without a body is a fact */
        AstFact *n = parser_alloc(sizeof(*n));
        n->node.kind = AST_FACT;
        n->head = $2;

        if ($1)
            ERROR("Cannot specify \"delete\" in a fact");

        $$ = n;
    }
    else
    {
        AstRule *n = parser_alloc(sizeof(*n));
        n->node.kind = AST_RULE;
        n->is_delete = $1;
        n->head = $2;
        n->body = $3;
        $$ = n;
    }
};

opt_delete: DELETE { $$ = true; }
| /* EMPTY */ { $$ = false; }
;

opt_rule_body: ':' '-' rule_body { $$ = $3; }
| /* EMPTY */ { $$ = NULL; }
;

rule_body:
  rule_body_elem { $$ = list_make1($1, context->pool); }
| rule_body ',' rule_body_elem { $$ = list_append($1, $3); }
;

rule_body_elem:
  table_ref
| assignment
  ;

table_ref: IDENT opt_hash_variant '(' column_ref_list ')' {
    AstTableRef *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_TABLE_REF;
    n->name = $1;
    n->hash_variant = $2;
    n->cols = $4;
    $$ = n;
};

assignment: column_ref ':' '=' expr {
    AstAssign *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_ASSIGN;
    n->lhs = $1;
    n->rhs = $4;
    $$ = n;
};

expr:
  op_expr
| const_expr
;

op_expr: { $$ = NULL; };

const_expr:
  ICONST
{
    AstConstExprInt *n = (AstConstExprInt *) parser_alloc(sizeof(*n));
    n->node.kind = AST_CONST_EXPR_INT;
    n->val = $1;
    $$ = n;
}
;

opt_hash_variant:
  OLG_HASH_DELETE       { $$ = AST_HASH_DELETE; }
| OLG_HASH_INSERT       { $$ = AST_HASH_INSERT; }
| /* EMPTY */           { $$ = AST_HASH_NONE; }
;

column_ref_list:
  column_ref { $$ = list_make1($1, context->pool); }
| column_ref_list ',' column_ref { $$ = list_append($1, $3); }
;

column_ref: opt_loc_spec IDENT
{
    AstColumnRef *n = parser_alloc(sizeof(*n));
    n->node.kind = AST_COLUMN_REF;
    n->has_loc_spec = $1;
    n->name = $2;
    $$ = n;
};

opt_loc_spec: '@'       { $$ = true; }
| /* EMPTY */           { $$ = false; }
;

%%

int
yyerror(ColParser *context, void *scanner, const char *message)
{
    printf("Parse error: %s\n", message);
    return 0;   /* return value ignored */
}
