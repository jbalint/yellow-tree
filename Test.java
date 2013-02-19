public class Test {
	static void a(int z, int y, int x, Integer w) {
		int a;
		a = 0;
		int b;
		b = 1;
		int c;
		c = a + b;
	}
	void b(int a)
	{
		if(a == 99)
		{
			System.out.println("a is 99");
		}
		else
		{
			System.out.println("a is not 99");
		}
	}
    public String toString() {
	return "Hi! I'm a Test object.";
    }
	public static void main(String args[]) {
		System.out.println("Jess");
		a(32, 22, 12, 2);
		int x[] = {1,2,3};
		System.out.println("x= " + x);
		Test t = new Test();
		t.b(400);
	}
}
