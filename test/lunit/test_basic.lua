-- ============================================================
-- some basic tests
-- ============================================================
require("lunit")
module("test_basic", lunit.testcase, package.seeall)

function test_go()
   assert_true(true)
end
