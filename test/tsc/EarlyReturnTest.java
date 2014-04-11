/**
 * Java code to support early return tests
 */
public class EarlyReturnTest {
	private String value = "initial value";

	/**
	 * The method that will be returned from early, before the string
	 * "failed" is able to be returned
	 */
	public String setValue() {
		return "failed";
	}

	/**
	 * The test method to be called
	 */
	public void test() {
		value = setValue();
	}
}
