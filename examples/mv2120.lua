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
register("evdev", "/dev/input/by-path/platform-gpio-keys-event",
	116, "power_button",
	408, "restart_button")

function power_button( time )
	os.execute("poweroff")
end

function restart_button( time )
	os.execute("reboot")
end

confdir("/etc/qcontrol.d")
