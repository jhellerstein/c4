#ifndef AGG_H
#define AGG_H

#include "operator/operator.h"

typedef struct AggExprInfo
{
    int colno;
    AstAggKind agg_kind;
} AggExprInfo;

typedef struct AggOperator
{
    Operator op;
    int num_aggs;
    AggExprInfo **agg_info;
} AggOperator;

AggOperator *agg_op_make(AggPlan *plan, OpChain *chain);

#endif  /* AGG_H */
