#ifndef STR_VIEW
#define STR_VIEW
#include <uchar.h>
// NOT NULL terminated
struct str_view
{
    const char* str;
    size_t length;
};

typedef struct str_view str_view;

int str_view_is_same(str_view str1, str_view str2);
int str_view_is_same2(str_view str1, const char* str2);
str_view str_view_init(const char* str, size_t length);
int str_view_atoi(str_view str);
int str_view_atoi_hex(str_view str);

#endif // STR_VIEW
