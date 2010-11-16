function show_consumers(tag, re)
	local xml

	if re then tag.regex._value = re end

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.consumer, "_name")) do
		local fmt = "%-20s {%s} {%s}"
		local name

		if not xt._user then
			name = xt._name
		else
			name = string.format("%s (%s)", xt._name, xt._user)
		end

		printf(fmt, name, timefmt(xt._start), timefmt(xt._end))
	end
end

function user_show_consumers(tag, user)
	tag.user._name = user
	show_consumers(tag)
end

function add_consumer(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

function drop_consumer(tag, name)
	if caution() then
		tag.name._value = name
		mbb.request(tag)
	end
end

function consumer_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

function consumer_mod_time_start(tag, name, time)
	tag.name._value = name
	tag.time._start = time

	mbb.request(tag)
end

function consumer_mod_time_end(tag, name, time)
	tag.name._value = name
	tag.time._end = time

	mbb.request(tag)
end

function consumer_sync_units(tag, name)
	local function unit_list(con)
		local units = {}
		local xml
		local tag

		tag = mbb.tag "mbb-show-units"
		tag.consumer._name = con

		xml = mbb.request(tag)
		for xt in xml_tag_iter(xml.unit) do
			list_push(units, xt._name)
		end

		table.sort(units)
		return units
	end

	for _,name in ipairs(unit_list(name)) do
		unit_map_sync(nil, name)
	end
end

function consumer_timeoff(tag, name, time)
	if time == "inf" then
		error("why %s ?", time)
	end

	if time == nil then time = "now" end

	consumer_mod_time_end(tag, name, time)
	consumer_sync_units(nil, name)
end

function consumer_add_unit(tag, con, unit)
	tag.consumer._name = con
	tag.unit._name = unit

	mbb.request(tag)
end

function consumer_add_inherit_unit(tag, con, unit)
	tag.consumer._name = con
	tag.unit._name = unit
	tag.unit._inherit = true

	mbb.request(tag)
end

function consumer_set_user(tag, con, user)
	tag.consumer._name = con
	tag.user._name = user

	mbb.request(tag)
end

function consumer_unset_user(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

cmd_register("show consumers", "mbb-show-consumers", "show_consumers", 0, 1)
cmd_register("self show consumers", "mbb-self-show-consumers", "show_consumers")
cmd_register("user show consumers", "mbb-show-consumers", "user_show_consumers", 1)

cmd_register("add consumer", "mbb-add-consumer", "add_consumer", 1)
cmd_register("drop consumer", "mbb-drop-consumer", "drop_consumer", 1)
cmd_register("consumer mod name", "mbb-consumer-mod-name", "consumer_mod_name", 2)
cmd_register("consumer mod time start", "mbb-consumer-mod-time", "consumer_mod_time_start", 2)
cmd_register("consumer mod time end", "mbb-consumer-mod-time", "consumer_mod_time_end", 2)

cmd_register("consumer add unit", "mbb-consumer-add-unit", "consumer_add_unit", 2)
cmd_register("consumer add inherit unit", "mbb-consumer-add-unit", "consumer_add_inherit_unit", 2)
cmd_register("consumer timeoff", "mbb-consumer-mod-time", "consumer_timeoff", 1, 1)
cmd_register("consumer sync units", "mbb-unit-map-rebuild", "consumer_sync_units", 1)

cmd_register("consumer set user", "mbb-consumer-set-user", "consumer_set_user", 2)
cmd_register("consumer detach", "mbb-consumer-unset-user", "consumer_unset_user", 1)
