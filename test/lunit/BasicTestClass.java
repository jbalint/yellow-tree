/**
 * An class that arbitrary things can be done to for testing.
 */
public class BasicTestClass {
	static boolean doneWithTesting = true;

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

    private static Object runLock = new Object();
    public static void notifyRunLock() {
		synchronized (runLock) {
			runLock.notify();
		}
    }
    public static void waitRunLock() throws Exception {
		synchronized (runLock) {
			runLock.wait();
		}
    }

    public static void main(String args[]) throws Exception {
		while (true) {
			waitRunLock();
			if (doneWithTesting)
				break;
		}
		Thread.sleep(1000);
    }
}
