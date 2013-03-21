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
    public int polyMorphic1() {
	return 1;
    }
    public int polyMorphic2(int x) {
	return x + 10;
    }
    public int polyMorphic3(String x) {
	return Integer.parseInt(x);
    }
    public int polyMorphic4(int x, Test y) {
	return 4;
    }
    public int polyMorphic5(int x, Object y) {
	return 5;
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
