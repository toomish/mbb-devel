xml_tag_meta = {
__index = function (tag, key)
	if key == "__body" then
		return xml_tag_get_body(tag)
	elseif string.sub(key, 1, 1) == "_" then
		key = string.sub(key, 2)
		return xml_tag_get_attr(tag, key)
	end

	return xml_tag_get_child(tag, key)
end
}

function xml_meta_get_table(key)
	if string.sub(key, 1, 2) == "__" then
		return key, nil
	end

	if string.sub(key, 1, 1) == "_" then
		key = string.sub(key, 2)
		return key, "__attrs"
	end

	return key, "__childs"
end

xml_meta = {
__index = function (table, key)
	local t, t_name

	key, t_name = xml_meta_get_table(key)
	if not t_name then
		if key ~= "__next" then
			return rawget(table, key)
		else
			local x, nextx

			x = xml_new(table.__self)
			nextx = rawget(table, "__next_")
			if nextx ~= nil then
				rawset(x, "__next_", nextx)
			end

			rawset(table, "__next_", x)
			return x
		end
	end

	t = rawget(table, t_name)

	if t_name == "__childs" then
		if not t then
			local v = xml_new(key)

			rawset(table, t_name, { [key] = v })
			return v
		elseif not t[key] then
			local v = xml_new(key)

			t[key] = v
			return v
		end
	end

	if not t then
		return nil
	end

	return t[key]
end,

__newindex = function (table, key, value)
	local t, t_name

	key, t_name = xml_meta_get_table(key)
	if not t_name then
		return rawset(table, key, value)
	end

	-- make body assign here
	if t_name == "__childs" then
		value.__self = key
		setmetatable(value, xml_meta)
	end

	t = rawget(table, t_name)
	if not t then
		rawset(table, t_name, { [key] = value })
	else
		t[key] = value
	end

	return table
end,

__tostring = function (xml)
	local push = function (l, val) l[#l + 1] = val end
	local l = {}

	push(l, string.format("<%s", xml.__self))

	if xml.__attrs then
		for key, value in pairs(xml.__attrs) do
			local s

			s = markup_escape(tostring(value))
			push(l, string.format(" %s='%s'", key, s))
		end
	end

	if not xml.__childs then
		push(l, "/>")
	else
		push(l, ">")
		for key, value in pairs(xml.__childs) do
			local x

			x = value
			while x do push(l, tostring(x))
				x = x.__next_
			end
		end
		push(l, string.format("</%s>", xml.__self))
	end

	return table.concat(l)
end
}

function xml_tag_iter(tag, ...)
	local args = { ... }

	if #args == 0 then
		local func = function (s, t)
			if t == nil then
				return s
			else
				return xml_tag_next(t)
			end
		end

		return func, tag, nil
	else
		local func = function (s, t)
			local list = {}

			if not t then
				t = s
			else
				t = xml_tag_next(t)
			end

			if t == nil then return nil end

			list_push(list, t)

			for i, key in ipairs(args) do
				list_push(list, t[key])
			end

			return unpack(list)
		end

		return func, tag, nil
	end
end

function xml_tag_fsort(tag, cmp)
	local list = {}

	while tag ~= nil do
		list_push(list, tag)
		tag = xml_tag_next(tag)
	end

	table.sort(list, cmp)

	return list
end

function xml_tag_sort(tag, field)
	local cmp = function (a, b)
		return a[field] < b[field]
	end

	return xml_tag_fsort(tag, cmp)
end

function xml_tag_nsort(tag, field)
	local cmp = function (a, b)
		return tonumber(a[field]) < tonumber(b[field])
	end

	return xml_tag_fsort(tag, cmp)
end

function xml_new(name)
	local t = {}

	setmetatable(t, xml_meta)
	t.__self = name

	return t
end

mbb = {
	request = function (tag, check)
		local xml

		if check == nil then
			check = true
		elseif type(check) ~= "boolean" then
			error("invalid type for 'check' arg")
		end

		xml = server_request(tag)
		if check and xml._result ~= "ok" then
			error("method '%s' failed: %s", tag._name, xml._desc)
		end

		return xml, xml._desc 
	end,

	tag = function (method)
		tag = xml_new("request")
		tag._name = method

		return tag
	end
}

