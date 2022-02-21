// Validation 03 : break out of #pragma block !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        void __attribute__((cfcheck(1),noinline)) __i1_cfcprAgma_ftest()
        {
            if(some_integer() == 5)
                break; // this version already fails because the break is out of context
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
