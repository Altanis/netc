#ifndef STRING_H
#define STRING_H

#include <stddef.h>

#define SSO_STRING_MAX_LENGTH 15

/** A struct which represents an SSO string. */
typedef struct sso_string {
    /** The length of the string. */
    size_t length;
    /** The capacity of the string. */
    size_t capacity;
    /** The string data. */
    union 
    {
        /** The string data if the string is shorter than SSO_STRING_MAX_LENGTH. */
        char short_string[SSO_STRING_MAX_LENGTH + 1];
        /** The string data if the string is longer than SSO_STRING_MAX_LENGTH. */
        char *long_string;
    };
} string_t;

/** Initializes a SSO string. */
void sso_string_init(string_t *string, const char *data);
/** Sets the value of a SSO string. */
void sso_string_set(string_t *string, const char *data);
/** Gets the value of a SSO string. */
const char *sso_string_get(string_t *string);

/** Concatenates two SSO strings. */
void sso_string_concat(string_t *dest, string_t *src);
/** Concatenates a SSO string and a char *buffer. */
void sso_string_concat_buffer(string_t *dest, const char *src);
/** Concatenates a SSO string and a char. */
void sso_string_concat_char(string_t *dest, const char src);
/** Goes back `n` chars and inserts a null terminator. */
void sso_string_backspace(string_t *string, size_t n);

/** Copies a SSO string. */
void sso_string_copy(string_t *dest, string_t *src);
/** Copies a SSO string into a buffer. */
void sso_string_copy_buffer(char *dest, string_t *src);

/** Compares two SSO strings. */
int sso_string_compare(string_t *string1, string_t *string2);

/** Ensures the string is null terminated. */
void sso_string_ensure_null_terminated(string_t *string);

/** Frees a SSO string. */
void sso_string_free(string_t *string);

#endif // STRING_H