#ifndef LOGGER_H
#define LOGGER_H

#include "types/tuple.h"

typedef struct C4Logger C4Logger;

C4Logger *logger_make(C4Runtime *c4);
void c4_log(C4Runtime *c4, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
char *log_tuple(C4Runtime *c4, Tuple *tuple);
char *log_datum(C4Runtime *c4, Datum datum, DataType type);

#endif  /* LOGGER_H */
