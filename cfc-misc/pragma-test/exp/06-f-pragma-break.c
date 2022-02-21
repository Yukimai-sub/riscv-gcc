// Example 06 : invalid use of pragma break !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            for(int j = some_integer(); j < 5; ++j)
            {
                void nested_function()
                {
                    if(some_integer() == 6)
                        #pragma cfcheck break
                } // this fails and needs no explanation
                nested_function();
                if(some_integer() == 5)
                    some_integer();
            }
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
