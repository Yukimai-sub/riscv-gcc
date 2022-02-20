// Example 00 : simple pragma
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
#pragma cfcheck on
        {
            if(some_integer() == 5)
                some_integer();
        }
        if(some_integer() == 6)
            some_integer();
    }
}
int main()
{
    test();
    return 0;
}
