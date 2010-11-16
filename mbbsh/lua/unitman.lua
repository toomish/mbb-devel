function print_units(xml)
	for n, xt in ipairs(xml_tag_sort(xml.unit, "_name")) do
		local name, ts, te

		if not xt._consumer then
			name = xt._name
		else
			name = string.format("%s (%s)", xt._name, xt._consumer)
		end

		ts = timewrap(xt._start, xt._parent_start)
		te = timewrap(xt._end, xt._parent_end)

		printf("%-20s %s %s", name, ts, te)
	end
end

function show_units(tag, re)
	local xml

	if re then tag.regex._value = re end

	xml = mbb.request(tag)
	print_units(xml)
end

function unit_show_self(tag, name)
	tag.unit._name = name

	xml = mbb.request(tag)
	print_units(xml)
end

function consumer_show_units(tag, con)
	tag.consumer._name = con
	show_units(tag)
end

function user_show_units(tag, user)
	tag.user._name = user
	show_units(tag)
end

function add_unit(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

function drop_unit(tag, name)
	if caution() then
		tag.name._value = name
		mbb.request(tag)
	end
end

function unit_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

function unit_set_consumer(tag, unit, con)
	tag.unit._name = unit
	tag.consumer._name = con

	mbb.request(tag)
end

function unit_unset_consumer(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

function unit_mod_time_start(tag, name, time)
	tag.name._value = name
	tag.time._start = time

	mbb.request(tag)
end

function unit_mod_time_end(tag, name, time)
	tag.name._value = name
	tag.time._end = time

	mbb.request(tag)
end

function unit_timeoff(tag, unit, time)
	if time == "parent" or time == "inf" then
		error("why %s ?", time)
	end

	if time == nil then time = "now" end

	unit_mod_time_end(tag, unit, time)
	unit_map_sync(nil, unit)
end

function unit_local_do(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

cmd_register("show units", "mbb-show-units", "show_units", 0, 1)
cmd_register("consumer show units", "mbb-show-units", "consumer_show_units", 1)
cmd_register("user show units", "mbb-show-units", "user_show_units", 1)
cmd_register("self show units", "mbb-self-show-units", "show_units")
cmd_register("self consumer show units", "mbb-self-show-units", "consumer_show_units", 1)
cmd_register("unit show self", "mbb-unit-show-self", "unit_show_self", 1)

cmd_register("add unit", "mbb-add-unit", "add_unit", 1)
cmd_register("drop unit", "mbb-drop-unit", "drop_unit", 1)
cmd_register("unit mod name", "mbb-unit-mod-name", "unit_mod_name", 2)
cmd_register("unit set consumer", "mbb-unit-set-consumer", "unit_set_consumer", 2)
cmd_register("unit detach", "mbb-unit-unset-consumer", "unit_unset_consumer", 1)
cmd_register("unit mod time start", "mbb-unit-mod-time", "unit_mod_time_start", 2)
cmd_register("unit mod time end", "mbb-unit-mod-time", "unit_mod_time_end", 2)
cmd_register("unit timeoff", "mbb-unit-mod-time", "unit_timeoff", 1, 1)
cmd_register("show mapped", "mbb-show-mapped-units", "show_units")
cmd_register("show nomapped", "mbb-show-nomapped-units", "show_units")
cmd_register("show local units", "mbb-show-local-units", "show_units", 0, 1)
cmd_register("unit local add", "mbb-unit-local-add", "unit_local_do", 1)
cmd_register("unit local del", "mbb-unit-local-del", "unit_local_do", 1)
