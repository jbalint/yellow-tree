
/**
 * Base telescope test. This serves as a simple blocking runner to
 * allow running tests without Java code requirements or as a base
 * class for more involved tests.
 */
public class TelescopeTest {
	public static TelescopeTest instance = new TelescopeTest();

	public static void release() throws Exception {
		synchronized (instance) {
			instance.notify();
		}
	}

	public static void main(String args[]) throws Exception {
		synchronized (instance) {
			instance.wait();
		}
	}
}
