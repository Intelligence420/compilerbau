#include "util/string.h"
#include "palm/memory.h"
#include "palm/str.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct string *string_new(void) {
    char *buf = MEMmalloc(4 * sizeof(char));
    *buf = 0;

    struct string *str = MEMmalloc(sizeof(struct string));
    *str = (struct string){
        .buf = buf,
        .len = 0,
        .cap = 4,
    };

    return str;
}

void string_drop(struct string *str) {
    MEMfree(str->buf);
    MEMfree(str);
}

void string_push(struct string *str, char *buf) {
    size_t buflen = STRlen(buf);
    if (buflen == 0) {
        return;
    }

    size_t newlen = str->len + buflen;
    if (newlen > str->cap) {
        str->cap = newlen + 1 > str->cap * 2 ? newlen + 1 : str->cap * 2;
        str->buf = MEMrealloc(str->buf, str->cap * sizeof(char));
    }

    strcpy(str->buf + str->len * sizeof(char), buf);
    str->len = newlen;
}

void swritef(struct string *str, const char *restrict format, ...) {
    va_list args1;
    va_start(args1, format);
    va_list args2;
    va_copy(args2, args1);

    int len = vsnprintf(NULL, 0, format, args1);
    va_end(args1);

    char buf[len + 1];
    vsnprintf(buf, sizeof buf, format, args2);
    va_end(args2);

    string_push(str, buf);
}
