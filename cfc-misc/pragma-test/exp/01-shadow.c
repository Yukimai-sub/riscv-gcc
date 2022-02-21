// Example 01 : shadowed variables
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
#pragma cfcheck on
        {
            if(i+some_integer() == 5)
                some_integer();
            int i = some_integer();
            if(i*i == 25)
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
