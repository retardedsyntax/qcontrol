--[[
	Configuration file for qcontrol (LUA syntax)
	Supports QNAP TS-110, TS-119, TS-210, TS-219 and TS-219P.
--]]

register("ts219")

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

-- Set to "false" if your device doesn't have a fan (TS-119 and HS-210)
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
		logprint(string.format("ts219: temperature %d", temp))
		last_temp_log = now
		last_temp_value = temp
	end
end

last_fan_setting = nil

function setfan( temp, speed )
	-- Do nothing if there's no fan
	if not has_fan then
		return
	end

	if ( ( not last_fan_setting ) or
	     ( last_fan_setting ~= speed ) ) then
		logprint(string.format("ts219: temperature %d setting fan to \"%s\"", temp, speed))
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
