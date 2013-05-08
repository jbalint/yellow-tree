readme for lunit tests

LD_LIBRARY_PATH=/path/to/yellow-tree
export LD_LIBRARY_PATH

java -agentlib:yt=runfile=lunit_wrapper.lua -cp . BasicTestClass

or

echo "r -agentlib:yt=runfile=lunit_wrapper.lua -cp . BasicTestClass" | gdb `which java`
