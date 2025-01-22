void test() {
  int y=4;
  int x = 10;
  y=11;
  int w = 5;
  int z = 21;
  while(w > 0)
  {
    w -= 2;
    if (x > y)
    {
	x = x + 1;
	y = y - 1;
    }
    else
    {
	x = x - 1;
	y = y + 1;
    }
    z = x+y;
  }
}
