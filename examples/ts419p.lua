--[[
	Configuration file for qcontrol (LUA syntax)
	Supports the QNAP TS-419P (or other TS-41x with LCD).
--]]

register("ts41x")
register("a125", "/dev/ttyS0")

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

fanfail = 0

function fan_error(  )
	fanfail = fanfail + 1
	if fanfail == 3 then
		print("ts41x: fan error")
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

function temp( temp )
	print("ts41x temperature:", temp)
	if temp > 80 then
		piccmd("fanspeed", "full")
	else
		if temp > 70 then
			piccmd("fanspeed", "high")
		else
			if temp > 55 then
				piccmd("fanspeed", "medium")
			end
		end
	end
end

