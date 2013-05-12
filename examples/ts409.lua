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

function setfan( speed )
	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts409: setting fan to \"%s\"", speed))
	end
	piccmd("fanspeed", speed)
	last_fan_setting = speed
end

function temp( temp )
	logtemp(temp)
	if temp > 80 then
		setfan("full")
	elseif temp > 70 then
		setfan("high")
	elseif temp > 55 then
		setfan("medium")
	elseif temp > 30 then
		setfan("low")
	else
		setfan("silence")
	end
end
