/**
 * An class that arbitrary things can be done to for testing.
 */
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
	/* Allow tests to run */
	Thread.sleep(3000);
    }
}
