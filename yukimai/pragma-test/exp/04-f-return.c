// Example 04 : return out of #pragma block !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            if(some_integer() == 5)
                return; // this return aims to return from test()
        } // which may cause control flow transfer between checked and unchecked region
        // so we do not allow this
        if(some_integer() == 6)
            some_integer();
    }
}
int main()
{
    test();
    return 0;
}
