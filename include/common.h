#ifndef COMMON_H
#define COMMON_H

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SQL_ERROR_MESSAGE_SIZE 256
#define SQL_PATH_BUFFER_SIZE 4096

typedef struct {
    int line;
    int column;
    char message[SQL_ERROR_MESSAGE_SIZE];
} SqlError;

void sql_set_error(SqlError *error, int line, int column, const char *fmt, ...);
char *sql_strdup(const char *src);
char *sql_strndup(const char *src, size_t len);
int sql_stricmp(const char *left, const char *right);
char *sql_read_text_file(const char *path, SqlError *error);

#endif
