Yellow Tree Configuration
=========================

# Lua setup
* The `LUA_PATH` environment variable needs to include `/path/to/yellow-tree/?.lua`

# Java setup
* The `LD_LIBRARY_PATH` should include `/path/to/yellow-tree` (where `libyt.so` resides)
* The Java command should include `-agentlib:yt` or `-agentlib:yt=options`

# Options
* `runfile` - A script can be passed to be run upon startup. It can be used for initialization, setting breakpoints, or as a complete debugger script.
 * If `runfile` returns true, the debugger will immediately break to the command prompt after running the file.
 * Alternatively, returning false will begin execution normally.
