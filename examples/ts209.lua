--[[
	Configuration file for qcontrol (LUA syntax)
	Supports both QNAP TS-109 and TS-209.
--]]

register("ts209")

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
		 408, "restart_button",
		 133, "media_button")
else
	logprint("No evdev device found")
end

register("system-status")

-- Set to "false" to suppress the sounding of the buzzer
buzzer = true
-- Set to "false" if your device doesn't have a fan (TS-109 and TS-109 II)
has_fan = true

-- Turn off fan if there is no fan to avoid fan_error() being called
if not has_fan then
	piccmd("fanspeed", "stop")
end

function system_status( status )
	logprint("System status: "..status)
	if status == "start" then
		piccmd("statusled", "greenon")
		piccmd("powerled", "on")
		if buzzer then piccmd("buzzer", "short") end
	elseif status == "stop" then
		piccmd("statusled", "redon")
		piccmd("powerled", "1hz")
		if buzzer then piccmd("buzzer", "short") end
	else
		logprint("Unknown system status")
	end
end

function power_button( time )
	os.execute("poweroff")
end

function restart_button( time )
	os.execute("reboot")
end

function media_button( time )
	piccmd("usbled", "8hz")
end

fanfail = 0

function fan_error(  )
	fanfail = fanfail + 1
	if fanfail == 3 then
		logprint("ts209: fan error")
		piccmd("statusled", "red2hz")
		piccmd("buzzer", "long")
	else
		if fanfail == 10 then
			fanfail = 0
		end
	end
end

function fan_normal(  )
	piccmd("statusled", "greenon")
	fanfail = 0
end

last_fan_setting = nil

function setfan( speed )
	-- Do nothing if there's no fan
	if not has_fan then
		return
	end

	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts209: setting fan to \"%s\"", speed))
	end
	piccmd("fanspeed", speed)
	last_fan_setting = speed
end

function temp_low(  )
	setfan("silence")
end

function temp_high(  )
	setfan("full")
end

confdir("/etc/qcontrol.d")
--
-- Local variables:
-- mode: lua
-- indent-level: 8
-- End:
