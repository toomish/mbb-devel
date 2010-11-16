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

