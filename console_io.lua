 --   _____                      _        _____ ____  
 --  / ____|                    | |      |_   _/ __ \ 
 -- | |     ___  _ __  ___  ___ | | ___    | || |  | |
 -- | |    / _ \| '_ \/ __|/ _ \| |/ _ \   | || |  | |
 -- | |___| (_) | | | \__ \ (_) | |  __/  _| || |__| |
 --  \_____\___/|_| |_|___/\___/|_|\___| |_____\____/ 

-- IO module that reads/writes to console

local console_io = {}
console_io.__index = console_io

function console_io:new(o)
   o = o or {}
   setmetatable(o, self)
   return o
end

function console_io:write(...)
   io.stdout:write(...)
   io.stdout:flush()
end

function console_io:print(...)
   self:write(...)
   self:write("\n")
end

function console_io:flush()
   io.stdout:flush()
end

function console_io:read(format)
   return io.stdin:read(format)
end

function console_io:command_loop()
   while true do
      self:write("yt> ")
      local cmd = self:read("*l")
      local chunk = load(cmd)
      local success, m2 = pcall(chunk)
      if not success then
	 self:print("Error: " .. m2)
      end
   end
end

print("console_io.lua - loaded with " .. _VERSION)

return console_io