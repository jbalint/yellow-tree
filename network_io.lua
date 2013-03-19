 --  _   _      _                      _      _____ ____  
 -- | \ | |    | |                    | |    |_   _/ __ \ 
 -- |  \| | ___| |___      _____  _ __| | __   | || |  | |
 -- | . ` |/ _ \ __\ \ /\ / / _ \| '__| |/ /   | || |  | |
 -- | |\  |  __/ |_ \ V  V / (_) | |  |   <   _| || |__| |
 -- |_| \_|\___|\__| \_/\_/ \___/|_|  |_|\_\ |_____\____/ 

-- IO module that works on a TCP connection

local NETWORK_IO_PORT = 8023

local network_io = {}
network_io.__index = network_io

function network_io:finish()
   self.client:close()
   self.server:close()
end

function network_io:new(o)
   o = o or {}
   setmetatable(o, self)
   return o
end

function network_io:write(...)
   return self.client:send(string.format("%s", unpack({...})))
end

function network_io:print(...)
   if self:write(...) then
      return self:write("\n")
   else
      return nil
   end
end

function network_io:flush()
end

function network_io:read(format)
   return self.client:receive(format)
end

function network_io:command_loop()
   local socket = require("socket")
   self.server = assert(socket.bind("*", NETWORK_IO_PORT))
   self.server:setoption('tcp-nodelay', true)
   self.server:setoption('reuseaddr', true)
   print("network_io.lua - server started on port " .. NETWORK_IO_PORT)
   while true do
      self.client = self.server:accept()
      self.client:send("Welcome to Yellow Tree\n")
      print("network_io.lua - accepted connection ")-- from " .. self.client:getpeername())
      while true do
	 if not self:write("yt> ") then
	    break
	 end
	 local cmd = self.client:receive("*l")
	 print(string.format("cmd is '%s'", cmd))
	 if not cmd then
	    break
	 end
	 local chunk = load(cmd)
	 local success, m2 = pcall(chunk)
	 if not success then
	    self:print("Error: " .. m2)
	 end
      end
      print("network_io.lua - client disconnected")
      self.client:close()
   end
   print("network_io.lua - closing server")
   self.server:close()
end

print("network_io.lua - loaded with " .. _VERSION)

return network_io