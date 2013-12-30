--[[
	Configuration file for qcontrol (LUA syntax)
	Supports the QNAP TS-409.
--]]

register("ts409")

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
		logprint("ts409: fan error")
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

last_temp_log = nil
last_temp_value = 0

function logtemp( temp )
	now = os.time()
	-- Log only every 5 minutes or if the temperature has changed by
	-- more than 5.
	if ( ( not last_temp_log ) or
	     ( os.difftime(now, last_temp_log) >= 300 ) or
	     ( math.abs( temp - last_temp_value ) >= 5 ) ) then
		logprint(string.format("ts409: temperature %d", temp))
		last_temp_log = now
		last_temp_value = temp
	end
end

last_fan_setting = nil

function setfan( temp, speed )
	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts409: temperature %d setting fan to \"%s\"", temp, speed))
	end
	piccmd("fanspeed", speed)
	last_fan_setting = speed
end

-- hysteresis implementation:
--    - - > 35 > - - - > 40 > - - - > 50 > - - - > 65 > - - -
--  silence --    low    --  medium   --   high    -- full  |
--    - - < 32 < - - - < 35 < - - - < 45 < - - - < 55 < - - -
function temp( temp )
	logtemp(temp)
	if last_fan_setting == "full" then
		if temp < 55 then
			setfan(temp, "high")
		end
	elseif last_fan_setting == "high" then
		if temp > 65 then
			setfan(temp, "full")
		elseif temp < 45 then
			setfan(temp, "medium")
		end
	elseif last_fan_setting == "medium" then
		if temp > 50 then
			setfan(temp, "high")
		elseif temp < 35 then
			setfan(temp, "low")
		end
	elseif last_fan_setting == "low" then
		if temp > 40 then
			setfan(temp, "medium")
		elseif temp < 32 then
			setfan(temp, "silence")
		end
	elseif last_fan_setting == "silence" then
		if temp > 35 then
			setfan(temp, "low")
		end
	else
		setfan(temp, "high")
	end
end

confdir("/etc/qcontrol.d")
--
-- Local variables:
-- mode: lua
-- indent-level: 8
-- End:
