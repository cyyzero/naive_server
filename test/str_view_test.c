#include "../src/str_view.h"
#include "../src/str_view.c"

#include <assert.h>
int main()
{
    str_view sv = str_view_init("test", 4);
    assert((str_view_is_same2(sv, "te") == 1));
    sv = str_view_init("123456", 6);
    assert(str_view_atoi(sv) == 123456);
    
}