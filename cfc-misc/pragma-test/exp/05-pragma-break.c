// Example 05 : proper way to break the #pragma block
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            if(some_integer() == 5)
                #pragma cfcheck break
            some_integer();
        } 
        // the '#pragma cfcheck break' directs control flow to here
        if(some_integer() == 6)
            some_integer();
    }
}
int main()
{
    test();
    return 0;
}
