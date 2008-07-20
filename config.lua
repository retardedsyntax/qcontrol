register("ts209")

register("evdev", "/dev/input/event0",
         116, "restart_button",
         408, "media_button")

function power_button( time )
	os.execute("poweroff")
end

fanfail = 0

function fan_error(  )
	fanfail = fanfail + 1
	if fanfail == 3 then
		print("ts209: fan error")
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

function temp_low(  )
	piccmd("fanspeed", "silence")
end

function temp_high(  )
	piccmd("fanspeed", "full")
end

function temp( temp )
	print("ts409 temperature:", temp)
	if temp > 80 then
		piccmd("fanspeed", "full")
	else
		if temp > 70 then
			piccmd("fanspeed", "high")
		end
	else
		if temp > 55 then
			piccmd("fanspeed", "medium")
		end
	end

function restart_button( time )
	os.execute("reboot")
end

function media_button( time )
	piccmd("usbled", "8hz")
end