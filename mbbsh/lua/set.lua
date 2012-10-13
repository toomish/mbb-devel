local check_yesno = function (str)
	return str == "no" or str == "yes"
end

local settings = {
	["session.sort.mtime"] = {
		value = "no",
		check = check_yesno
	},

	["su.autoreload"] = {
		value = "no",
		check = check_yesno
	}
}

function get_set(name)
	local v = settings[name]

	if not v then
		return nil
	end

	return v.value
end

function do_set(name, value)
	if not name then
		for k, v in pairs(settings) do
			printf("%s = %s", k, v.value)
		end
	elseif not value then
		local v = settings[name]

		if v then
			printf("%s = %s", name, v.value)
		end
	else
		local v = settings[name]

		if not v then
			print("no such setting")
		else
			if not v.check then
				v.value = value
			else
				if v.check(value) then
					v.value = value
				else
					print("invalid value")
				end
			end
		end
	end
end

cmd_register("set", nil, "do_set", 0, 2)
