describe("array assignment", function ()
 context("array of arrays assignment", function ()
 it("should assign an array to an element", function ()
	   ArrayTest.objArray[3] = "obj3"
	   ArrayTest.arrArray[1] = ArrayTest.objArray
	   assert_equal(ArrayTest.objArray.hashCode(), ArrayTest.arrArray[1].hashCode())
	   assert_true(ArrayTest.objArray.equals(ArrayTest.arrArray[1]))
	   assert_equal("obj3", ArrayTest.arrArray[1][3].toString())
 end)
 it("should assign object values using two indexes", function ()
	   ArrayTest.objArray[3] = "obj-3"
	   ArrayTest.arrArray[1] = ArrayTest.objArray
	   ArrayTest.arrArray[1][4] = "obj-4"
	   assert_equal("obj-3", ArrayTest.arrArray[1][3].toString())
	   assert_equal("obj-4", ArrayTest.arrArray[1][4].toString())
	   assert_equal("obj-4", ArrayTest.objArray[4].toString())
 end)
 end)

 context("object assignment", function ()
 it("should assign a basic string value", function ()
	   ArrayTest.objArray[1] = "Jess"
	   assert_equal("Jess", ArrayTest.objArray[1].toString())
 end)
 it("should assign a null", function ()
	   -- make sure it's non-nil before assigning a nil
	   ArrayTest.objArray[2] = "a value"
	   assert_equal("a value", ArrayTest.objArray[2].toString())
	   ArrayTest.objArray[2] = nil
	   assert_equal(nil, ArrayTest.objArray[2])
 end)
 end)

 context("boolean assignment", function ()
 it("should assign boolean values", function ()
	   ArrayTest.boolArray[1] = false
	   assert_equal(false, ArrayTest.boolArray[1])
	   ArrayTest.boolArray[1] = true
	   assert_equal(true, ArrayTest.boolArray[1])
 end)
 end)

 context("byte assignment", function ()
 it("should assign byte values", function ()
	   ArrayTest.byteArray[1] = 40
	   assert_equal(40, ArrayTest.byteArray[1])
	   ArrayTest.byteArray[1] = 255
	   assert_equal(-1, ArrayTest.byteArray[1])
 end)
 end)

 context("char assignment", function ()
 it("should assign char values", function ()
	   ArrayTest.charArray[1] = 62
	   assert_equal(62, ArrayTest.charArray[1])
	   ArrayTest.charArray[1] = 61
	   assert_equal(61, ArrayTest.charArray[1])
 end)
 end)

 context("short assignment", function ()
 it("should assign short values", function ()
	   ArrayTest.shortArray[1] = 162
	   assert_equal(162, ArrayTest.shortArray[1])
	   ArrayTest.shortArray[1] = 161
	   assert_equal(161, ArrayTest.shortArray[1])
 end)
 end)

 context("int assignment", function ()
 it("should assign int values", function ()
	   ArrayTest.intArray[1] = 362
	   assert_equal(362, ArrayTest.intArray[1])
	   ArrayTest.intArray[1] = 361
	   assert_equal(361, ArrayTest.intArray[1])
 end)
 end)

 context("long assignment", function ()
 it("should assign long values", function ()
	   ArrayTest.longArray[1] = 562
	   assert_equal(562, ArrayTest.longArray[1])
	   ArrayTest.longArray[1] = 561
	   assert_equal(561, ArrayTest.longArray[1])
 end)
 end)

 context("float assignment", function ()
 it("should assign float values", function ()
	   ArrayTest.floatArray[1] = .7
	   assert_greater_than(.701, ArrayTest.floatArray[1])
	   assert_less_than(.699, ArrayTest.floatArray[1])
	   ArrayTest.floatArray[1] = -123456789
	   -- is this Lua or Java changing the value
	   assert_equal(-123456792, ArrayTest.floatArray[1])
 end)
 end)

 context("double assignment", function ()
 it("should assign a double value", function ()
	   ArrayTest.dblArray[1] = 1.4
	   assert_equal(1.4, ArrayTest.dblArray[1])
 end)
 it("should assign an int value", function ()
	   ArrayTest.dblArray[1] = 400
	   assert_equal(400, ArrayTest.dblArray[1])
	   ArrayTest.dblArray[1] = "500" -- coerce
	   assert_equal(500, ArrayTest.dblArray[1])
 end)
 end)
 end)
