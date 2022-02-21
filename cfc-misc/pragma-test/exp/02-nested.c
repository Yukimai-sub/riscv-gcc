// Example 02 : nested pragma
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            for(int j = some_integer(); j <= 5; ++j)
            {
                #pragma cfcheck off
                {
                    if(some_integer() + j != 9)
                        some_integer();
                }

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
