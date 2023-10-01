#include "utils/string.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void sso_string_init(string_t *string, const char *data)
{
    string->length = strlen(data);
    string->capacity = string->length > SSO_STRING_MAX_LENGTH ? string->length : SSO_STRING_MAX_LENGTH;

    if (string->length > SSO_STRING_MAX_LENGTH) string->long_string = strdup(data);
    else
    {
        memcpy(string->short_string, data, string->length);
        // string->long_string = NULL;
    };

    sso_string_ensure_null_terminated(string);
};

void sso_string_set(string_t *string, const char *data)
{
    sso_string_free(string);
    sso_string_init(string, data);
};

const char *sso_string_get(const string_t *string)
{
    return string->length > SSO_STRING_MAX_LENGTH ? string->long_string : string->short_string;
};

void sso_string_concat(string_t *dest, const string_t *src)
{
    size_t total_length = dest->length + src->length;

    if (total_length > SSO_STRING_MAX_LENGTH)
    {
        char *new_long_string = (char *)malloc(total_length + 1);
        memcpy(new_long_string, sso_string_get(dest), dest->length);
        strcat(new_long_string, sso_string_get(src));
        sso_string_free(dest);
        dest->long_string = new_long_string;
    }
    else
    {
        strcat(dest->short_string, sso_string_get(src));
        dest->length = total_length;
    };

    dest->capacity = total_length;

    sso_string_ensure_null_terminated(dest);
};

void sso_string_concat_buffer(string_t *dest, const char *src)
{
    size_t total_length = dest->length + strlen(src);

    if (total_length > SSO_STRING_MAX_LENGTH)
    {
        char *new_long_string = (char *)malloc(total_length + 1);
        
        memcpy(new_long_string, sso_string_get(dest), dest->length);
        strcat(new_long_string, src);
        sso_string_free(dest);

        dest->long_string = new_long_string;
        dest->length = total_length;
    }
    else
    {
        strcat(dest->short_string, src);
        dest->length = total_length;
    };

    dest->capacity = total_length;

    sso_string_ensure_null_terminated(dest);
};

void sso_string_concat_char(string_t *dest, char src)
{
    size_t total_length = dest->length + 1;

    if (total_length > SSO_STRING_MAX_LENGTH)
    {
        char *new_long_string = (char *)malloc(total_length + 1);

        memcpy(new_long_string, sso_string_get(dest), dest->length);
        new_long_string[dest->length] = src;
        new_long_string[total_length] = '\0';
        sso_string_free(dest);

        dest->long_string = new_long_string;
        dest->length = total_length;
    }
    else
    {
        dest->short_string[dest->length] = src;
        dest->short_string[total_length] = '\0';
        dest->length = total_length;
    }

    dest->capacity = total_length;

    sso_string_ensure_null_terminated(dest);
};

// void sso_string_backspace(string_t *string, size_t n)
// {
//     size_t new_length = string->length - n;

//     if (new_length > SSO_STRING_MAX_LENGTH)
//     {
//         if (string->length > SSO_STRING_MAX_LENGTH) string->long_string[new_length] = '\0';
//         else
//         {
//             strcpy(string->long_string, string->short_string);
//             string->long_string[new_length] = '\0';
//         }
//     }
//     else
//     {
//         if (string->length > SSO_STRING_MAX_LENGTH)
//         {
//             strcpy(string->short_string, string->long_string);
//             string->short_string[new_length] = '\0';
//             printf("short: %s\n", string->short_string);
//         }
//         else string->short_string[new_length] = '\0';
//     };

//     string->length = new_length;
// };

void sso_string_backspace(string_t *string, size_t n)
{
    size_t new_length = string->length > n ? string->length - n : 0;

    if (new_length > SSO_STRING_MAX_LENGTH)
    {
        if (string->length <= SSO_STRING_MAX_LENGTH)
        {
            string->long_string = (char *)malloc(new_length + 1);
            memcpy(string->long_string, string->short_string, new_length);
        }
        string->long_string[new_length] = '\0';
        string->capacity = new_length;
    }
    else
    {
        if (string->length > SSO_STRING_MAX_LENGTH)
            strncpy(string->short_string, string->long_string, new_length);

        string->short_string[new_length] = '\0';
        string->capacity = SSO_STRING_MAX_LENGTH;
    }

    string->length = new_length;
};

void sso_string_copy(string_t *dest, const string_t *src)
{
    sso_string_free(dest);

    if (src->length > SSO_STRING_MAX_LENGTH) dest->long_string = strdup(sso_string_get(src));
    else memcpy(dest->short_string, sso_string_get(src), src->length);
    
    dest->length = src->length;
    dest->capacity = src->capacity;

    sso_string_ensure_null_terminated(dest);
};

size_t sso_string_length(const string_t *string)
{
    return string->length;
};

int sso_string_compare(const string_t *string1, const string_t *string2)
{
    if (string1->length == string2->length) return memcmp(sso_string_get(string1), sso_string_get(string2), string1->length);
    else return strcmp(sso_string_get(string1), sso_string_get(string2));
};

void sso_string_ensure_null_terminated(string_t *string)
{
    if (string->length > SSO_STRING_MAX_LENGTH) string->long_string[string->length] = '\0';
    else string->short_string[string->length] = '\0';
};

void sso_string_free(string_t *string)
{
    if (string->length > SSO_STRING_MAX_LENGTH && string->long_string != NULL)
    {
        free(string->long_string);
    };

    string->length = 0;
    string->capacity = SSO_STRING_MAX_LENGTH;
};