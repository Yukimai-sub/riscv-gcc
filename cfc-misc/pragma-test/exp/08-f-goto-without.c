// Example 07 : goto between regions !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {before:
            for(int j = some_integer(); j < 5; ++j)
            {
                if(some_integer() == 4)
                {
                    #pragma cfcheck off
                    {
                        if(some_integer() == 7)
                            goto before;
                    } // this automatically fails due to implementation of #pragma cfcheck

                }
            }
            if(some_integer() == 8)
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
