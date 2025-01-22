void test()
{
	int a=1,b=2,z,v;
	if(a != b)
		z = b+a;
	else
		z = b-a;
	do
	{
		if(z < 0)
			v = z+1;
		else
			v = z-1;
		b=b-1;
	} while(b>=0);
}
