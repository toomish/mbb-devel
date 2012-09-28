function list_push(list, value)
	list[#list + 1] = value
end

function printf(...)
	print(string.format(...))
end

function timewrap(t, flag)
	if flag then
		return "^" .. timefmt(t) .. "^"
	end

	return "{" .. timefmt(t) .. "}"
end

local old_error = error

function error(...) old_error(string.format(...), 0) end

function nearload(name)
	if not nearload_selfdir then
		local info = debug.getinfo(2, "S")
		nearload_selfdir = info.source:sub(2):match("(.*/)") or ""
	end

	if name:find("/", 1, true) then
		printf("nearload '%s' failed: contains /", name)
		return
	end

	local func = loadfile(nearload_selfdir .. name)

	if func then
		func()
	else
		printf("nearload '%s' failed: invalid file", name)
	end
end
