// Validation 05 : proper way to break the #pragma block
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        void __attribute__((cfcheck(1),noinline)) __i1_cfcprAgma_ftest()
        {
            if(some_integer() == 5)
                return;
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
