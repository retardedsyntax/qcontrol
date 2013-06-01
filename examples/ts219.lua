--[[
	Configuration file for qcontrol (LUA syntax)
	Supports QNAP TS-110, TS-119, TS-210, TS-219 and TS-219P.
--]]

register("ts219")

-- Requires CONFIG_KEYBOARD_GPIO enabled in the kernel and
-- the kernel module gpio_keys to be loaded.
register("evdev", "/dev/input/by-path/platform-gpio-keys-event",
	408, "restart_button",
	133, "media_button")

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
		logprint("ts219: fan error")
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
		logprint(string.format("ts219: temperature %d", temp))
		last_temp_log = now
		last_temp_value = temp
	end
end

last_fan_setting = nil

function setfan( speed )
	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts219: setting fan to \"%s\"", speed))
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
			setfan("high")
		end
	elseif last_fan_setting == "high" then
		if temp > 65 then
			setfan("full")
		elseif temp < 45 then
			setfan("medium")
		end
	elseif last_fan_setting == "medium" then
		if temp > 50 then
			setfan("high")
		elseif temp < 35 then
			setfan("low")
		end
	elseif last_fan_setting == "low" then
		if temp > 40 then
			setfan("medium")
		elseif temp < 32 then
			setfan("silence")
		end
	elseif last_fan_setting == "silence" then
		if temp > 35 then
			setfan("low")
		end
	else
		setfan("high")
	end
end
