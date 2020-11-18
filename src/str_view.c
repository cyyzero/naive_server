
#include "str_view.h"

int str_view_is_same(str_view str1, str_view str2)
{
    int length = str1.length < str2.length ? str1.length : str2.length;
    for (int i = 0; i < length; ++i)
    {
        if (str1.str[i] != str2.str[i])
            return 0;
    }
    return 1;
}

// str is null-terminated
int str_view_is_same2(str_view str1, const char* str2)
{
    for (int i = 0; i< str1.length && str2[i]; ++i)
    {
        if (str1.str[i] != str2[i])
            return 0;
    }
    return 1;
}

str_view str_view_init(const char* str, int length)
{
    str_view sv;
    sv.str = str;
    sv.length = length;
    return sv;
}

int str_view_atoi(str_view str)
{
    int result = 0;
    for (int i = 0; i < str.length; ++i)
    {
        result = result * 10 + str.str[i] - '0';
    }
    return result;
}
