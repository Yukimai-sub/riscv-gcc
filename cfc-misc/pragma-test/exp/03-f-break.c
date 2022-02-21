// Example 03 : break out of #pragma block !should fail!
int some_integer();
void test()
{
    for(int i = 1; i <= 10; ++i)
    {
        #pragma cfcheck on
        {
            if(some_integer() == 5)
                break; // this break intends to break out of the for loop
        } // which may cause control flow transfer between checked and unchecked region
        // so we do not allow it, as well as cross-region continue and goto, see also 08
        if(some_integer() == 6)
            some_integer();
    }
}
int main()
{
    test();
    return 0;
}
