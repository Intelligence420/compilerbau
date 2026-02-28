#pragma once

#include <stddef.h>

struct string {
    char *buf;
    size_t len;
    size_t cap;
};

struct string *string_new(void);
void string_drop(struct string *str);
void string_push(struct string *str, char *buf);
void swritef(struct string *str, const char *restrict format, ...);
