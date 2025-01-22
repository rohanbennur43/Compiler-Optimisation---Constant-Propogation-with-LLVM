void test()
{
	int a=-5, b=4, c=5, d=4;
	while(a < 3)
	{
		c = c-1;
		a = a+2;
		if(b > d)
		{
			b = b-d;
		}
		else
		{
			b = b+d;
		}
		b=b/2;
	}
}
