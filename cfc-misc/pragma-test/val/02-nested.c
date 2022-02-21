// Experiment 02 : nested pragma
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        void __attribute__((cfcheck(1),noinline)) __i1_cfcprAgma_ftest()
        {
            for(int j = some_integer(); j <= 5; ++j)
            {
                void __attribute__((cfcheck(0),noinline)) __i2_cfcprAgma_f__i1_cfcprAgma_ftest()
                {
                    if(some_integer() + j != 9)
                        some_integer();
                }
                __i2_cfcprAgma_f__i1_cfcprAgma_ftest();
            }
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
