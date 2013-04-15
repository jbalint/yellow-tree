public class TestBug1 {
    private void doSomething() {
	throw new RuntimeException("oh noes!");
    }

    public void testBug1() {
	int x = 1;
	try {
	    doSomething();
	} catch(RuntimeException ex) {
	    System.out.println("Caught runtime exception");
	}
    }

    public static void main(String args[]) {
	new TestBug1().testBug1();
    }
}
