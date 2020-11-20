#include "str_view.h"
#include <ctype.h>

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
    for (size_t i = 0; i< str1.length && str2[i]; ++i)
    {
        if (str1.str[i] != str2[i])
            return 0;
    }
    return 1;
}

str_view str_view_init(const char* str, size_t length)
{
    str_view sv;
    sv.str = str;
    sv.length = length;
    return sv;
}

int str_view_atoi(str_view str)
{
    int result = 0;
    for (size_t i = 0; i < str.length; ++i)
    {
        result = result * 10 + str.str[i] - '0';
    }
    return result;
}

int str_view_atoi_hex(str_view str)
{
    int result = 0;
    for (size_t i = 0; i < str.length; ++i)
    {
        result = result * 16;
        char ch = str.str[i];
        if (isdigit(ch))
        {
            result += (ch - '0');
        }
        else if (isupper(ch))
        {
            result += (ch - 'A');
        }
        else if (islower(ch))
        {
            result += (ch - 'a');
        }
        else
        {
            result = 0;
            break;
        }
        
    }
    return result;
}
