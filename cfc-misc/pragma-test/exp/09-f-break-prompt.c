// Example 09 : should emit normal error prompt !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            if(some_integer() == 5)
            {
                void nested_function()
                {
                    break;
                }
                nested_function();
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
