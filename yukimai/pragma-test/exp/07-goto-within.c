// Example 07 : goto within same region
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
                            some_integer();
                    }

                }else goto before;
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
