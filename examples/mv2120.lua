--[[
	Configuration file for qcontrol (LUA syntax)
	Supports the HP Media Vault mv2120.
--]]

register("system-status")

function system_status( status )
	logprint("System status: "..status)
	if status == "start" then
		-- Nothing...
	elseif status == "stop" then
		-- Nothing
	else
		logprint("Unknown system status")
	end
end

-- Requires CONFIG_KEYBOARD_GPIO enabled in the kernel and
-- the kernel module gpio_keys to be loaded.

-- Different kernel versions use platform-gpio_keys-event or
-- platform-gpio-keys-event, find the right one.

function find_device( options )
	for index,option in ipairs(options) do
		local f=io.open(option)
		if f then
			f:close()
			return option
		end
	end
	return nil
end

evdev = find_device ( { "/dev/input/by-path/platform-gpio_keys-event",
			"/dev/input/by-path/platform-gpio-keys-event" } )
if evdev then
	logprint("Register evdev on "..evdev)
	register("evdev", evdev,
		 116, "power_button",
		 408, "restart_button")
else
	logprint("No evdev device found")
end

function power_button( time )
	os.execute("poweroff")
end

function restart_button( time )
	os.execute("reboot")
end

confdir("/etc/qcontrol.d")
--
-- Local variables:
-- mode: lua
-- indent-level: 8
-- End:
