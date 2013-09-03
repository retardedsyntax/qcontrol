--[[
	Configuration file for qcontrol (LUA syntax)
	Supports the QNAP TS-410, TS-410U, TS-419P and TS-419U.
--]]

register("ts41x")

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

function lcd_button( state, down, up )
-- exactly key 1 and 2 pressed switches backlight off
	if state == 3 then
		piccmd("lcd-backlight", "off")
	else
-- any key pressed activates the backlight
		if down ~= 0 then
			piccmd("lcd-backlight", "on")
		end
-- pressing key 1 switches off the usbled the media-key enables...
		if down == 1 then
			piccmd("usbled", "off")
		end
	end
end

-- If argument is a function then call it, else return it.
function evalfn(f)
	if type(f) == "function" then
		return f(  )
	else
		return f
	end
end

-- Select a value based on the state of a GPIO pin.
--
-- results == list of result to return for each state
-- value=0       => results[1]
-- value=1       => results[2]
-- value unknown => results[3]
--
-- If result[N] is a function it will be called,
-- otherwise it is simply returned as is.
function gpio_select(number, results)
	local path=string.format("/sys/class/gpio/gpio%d/value", number)
	local gpio=io.open(path)
	if not gpio then
		logprint("ts41x: "..path.." does not exist, trying to enable")
		g = io.open("/sys/class/gpio/enable")
		if not g then
			logprint("ts41x: unable to open gpio control file")
			return evalfn(results[3])
		end
		g:write(number)
		gpio=io.open(path)
	end
	if not gpio then
		logprint("ts41x: unable to open gpio file")
		return evalfn(results[3])
	end

	local v=gpio:read("*n")

	if v == 0     then return evalfn(results[1])
	elseif v == 1 then return evalfn(results[2])
	else               return evalfn(results[3])
	end
end

-- MPP45_GPIO, JP1: 0: LCD, 1: serial console
gpio_select(45, {
	function () register("a125", "/dev/ttyS0") end,
	nil,
	nil})

fanfail = 0

function fan_error(  )
	fanfail = fanfail + 1
	if fanfail == 3 then
		logprint("ts41x: fan error")
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
		logprint(string.format("ts41x: temperature %d", temp))
		last_temp_log = now
		last_temp_value = temp
	end
end

last_fan_setting = nil

function setfan( speed )
	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts41x: setting fan to \"%s\"", speed))
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

confdir("/etc/qcontrol.d")
