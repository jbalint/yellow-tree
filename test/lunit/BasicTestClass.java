public class BasicTestClass {
    public static String someStaticMethod(String smth) {
	return "You passed: " + smth;
    }

    private String myVal;
    public BasicTestClass(String val) {
	myVal = val;
    }

    public String getMyVal() {
	return myVal;
    }

    public static void main(String args[]) throws Exception {
	char [] s =  new char[0];
	System.out.println("String length: " + s);
	System.out.println("String length: " + s.length);
	/* Allow tests to run */
	Thread.sleep(5000);
    }
}
