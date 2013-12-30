--[[
	Configuration file for qcontrol (LUA syntax)
	Supports both QNAP TS-109 and TS-209.
--]]

register("ts209")

-- Requires CONFIG_KEYBOARD_GPIO enabled in the kernel and
-- the kernel module gpio_keys to be loaded.
register("evdev", "/dev/input/by-path/platform-gpio-keys-event",
	408, "restart_button",
	133, "media_button")

register("system-status")

-- Set to "false" to suppress the sounding of the buzzer
buzzer = true

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
