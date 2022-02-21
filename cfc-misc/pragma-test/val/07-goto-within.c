// Validation 07 : goto within same region
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        void __attribute__((cfcheck(1),noinline)) __i1_cfcprAgma_ftest()
        {before:
            for(int j = some_integer(); j < 5; ++j)
            {
                if(some_integer() == 4)
                {
                    void __attribute__((cfcheck(0),noinline)) __i2_cfcprAgma_f__i1_cfcprAgma_ftest()
                    {
                        if(some_integer() == 7)
                            some_integer();
                    }
                    __i2_cfcprAgma_f__i1_cfcprAgma_ftest();
                }else goto before;
            }
            if(some_integer() == 8)
                some_integer();
        }
        __i1_cfcprAgma_ftest();
        if(some_integer() == 6)
            some_integer();
    }
}
int main()
{
    test();
    return 0;
}
