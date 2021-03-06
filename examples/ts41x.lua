--[[
	Configuration file for qcontrol (LUA syntax)
	Supports the QNAP TS-410, TS-410U, TS-419P and TS-419U.
--]]

register("ts41x")

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
		gpioctl = "/sys/class/gpio/export"
		g, err = io.open(gpioctl, "w")
		if not g then
			logprint("ts41x: unable to open gpio control file "..gpioctl..": "..err)
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
	local now = os.time()

	local function should_log(  )
		local delta_temp = math.abs(temp - last_temp_value)
		local delta_time = os.difftime(now, last_temp_log)

		-- Haven't previously logged, log now for the first time
		if ( not last_temp_log ) then
			return true
		end
		-- Temperature has changed by more than 5
		if ( delta_temp >= 5 ) then
			return true
		end
		-- More than 5 minutes have elapsed...
		if ( delta_time >= 300 ) then
			if ( delta_temp > 1 ) then
				--- ...and the change is by more than +/-1
				return true
			else
				--- ...insignificant change, wait another 5 minutes
				last_temp_log = now
				return false
			end
		end
		-- Otherwise no need to log
		return false
	end

	if ( should_log() ) then
		logprint(string.format("ts41x: temperature %d", temp))
		last_temp_log = now
		last_temp_value = temp
	end
end

last_fan_setting = nil

function setfan( temp, speed )
	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts41x: temperature %d setting fan to \"%s\"", temp, speed))
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
