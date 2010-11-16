function show_raw_inet(xml, unit)
	for xt in xml_tag_iter(xml.inet_entry) do
		local sign, ts, te

		if not xt._exclusive then sign = "+" else sign = "-" end

		ts = timewrap(xt._start, xt._parent_start)
		te = timewrap(xt._end, xt._parent_end)

		printf("%-10s %2s %s %20s %s %s",
			xt._id, xt._nice, sign, xt._inet, ts, te)
	end
end

function unit_show_raw_inet(tag, unit)
	local xml

	if unit then
		tag.unit._name = unit
	end

	xml = mbb.request(tag)
	show_raw_inet(xml)
end

function unit_add_inet(tag, unit, inet, nice)
	tag.unit._name = unit
	tag.inet_entry._inet = inet

	if nice then
		tag.inet_entry._nice = nice
	end

	mbb.request(tag)
end

function unit_add_neg_inet(tag, ...)
	tag.inet_entry._exclusive = true
	unit_add_inet(tag, ...)
end

function unit_add_inherit_inet(tag, ...)
	tag.inet_entry._inherit = true
	unit_add_inet(tag, ...)
end

function unit_add_inherit_neg_inet(tag, ...)
	tag.inet_entry._exclusive = true
	tag.inet_entry._inherit = true
	unit_add_inet(tag, ...)
end

function unit_drop_inet(tag, id)
	if caution() then
		tag.inet_entry._id = id
		mbb.request(tag)
	end
end

function unit_mod_inet_time_start(tag, id, time)
	tag.inet_entry._id = id
	tag.time._start = time

	mbb.request(tag)
end

function unit_mod_inet_time_end(tag, id, time)
	tag.inet_entry._id = id
	tag.time._end = time

	mbb.request(tag)
end

function unit_mod_inet_nice(tag, id, newnice)
	tag.inet_entry._id = id
	tag.newnice._value = newnice

	mbb.request(tag)
end

function show_unit_map(tag)
	local xml

	xml = mbb.request(tag)

	for xt in xml_tag_iter(xml.time_range) do
		printf("{%s} {%s}", timefmt(xt._min), timefmt(xt._max))

		for xt in xml_tag_iter(xt.inet_range) do
			printf("\t%20s  %20s    [%s]", xt._min, xt._max, xt._id)
		end
	end
end

function unit_map_show(tag, name)
	tag.name._value = name
	show_unit_map(tag)
end

function unit_map_inet_showall(tag, id)
	tag.inet_entry._id = id
	show_unit_map(tag)
end

function unit_map_rebuild(tag, name)
	local xml, msg

	tag.name._value = name
	xml, msg = mbb.request(tag, false)

	if msg then
		printf("rebuild map for unit '%s' failed: %s", name, msg)
		show_raw_inet(xml)
	end
end

function unit_map_sync(tag, name)
	local xml, msg

	if tag == nil then
		tag = mbb.tag "mbb-unit-map-rebuild"
	end

	tag.name._value = name

	xml, msg = mbb.request(tag, false)
	if msg then
		printf("rebuild map for unit '%s' failed: %s", name, msg)
	else
		tag = mbb.tag "mbb-map-add-unit"
		tag.unit._name = name

		xml, msg = mbb.request(tag, false)
		if msg then
			printf("map add unit '%s' failed: %s", name, msg)
		end
	end
end

function unit_map_clear(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

function map_do_unit(tag, unit)
	tag.unit._name = unit
	mbb.request(tag)
end

function unit_clear_inet(tag, unit)
	if caution() then map_do_unit(tag, unit) end
end

function map_show(tag)
	local xml

	xml = mbb.request(tag)

	for xt in xml_tag_iter(xml.inet_range) do
		printf("%-16s  %16s", xt._min, xt._max)

		for xt in xml_tag_iter(xt.time_range) do
			local ts, te

			ts = timefmt(xt._min)
			te = timefmt(xt._max)

			printf("\t%-20s {%s} {%s}", xt._unit, ts, te)
		end
	end
end

function map_show_inet(tag, id)
	tag.inet_entry._id = id
	map_show(tag)
end

function map_show_unit(tag, unit)
	tag.unit._name = unit
	map_show(tag)
end

function map_find_common(tag)
	local xml

	xml = mbb.request(tag)

	for xt in xml_tag_iter(xml.inet_entry) do
		print(xt._owner, xt._id)
	end
end

function map_find_net(tag, net, time_start, time_end)
	tag.net._value = net

	if time_start then
		tag.time._start = time_start

		if time_end then
			tag.time._end = time_end
		end
	end

	map_find_common(tag)
end

function map_find_unit(tag, name)
	tag.name._value = name
	map_find_common(tag)
end

function map_reload(tag)
	local units = {}
	local xml, msg

	mbb.request(tag)

	xml = mbb.request(mbb.tag "mbb-show-units")
	for xt in xml_tag_iter(xml.unit) do
		list_push(units, xt._name)
	end

	table.sort(units)
	for _, name in ipairs(units) do
		unit_map_sync(nil, name)
	end
end

cmd_register("unit show inet", "mbb-unit-show-raw-inet", "unit_show_raw_inet", 0, 1)

cmd_register("unit add inet", "mbb-unit-add-inet", "unit_add_inet", 2, 1)
cmd_register("unit add neg inet", "mbb-unit-add-inet", "unit_add_neg_inet", 2, 1)
cmd_register("unit add inherit inet", "mbb-unit-add-inet", "unit_add_inherit_inet", 2, 1)
cmd_register("unit add inherit neg inet", "mbb-unit-add-inet", "unit_add_inherit_neg_inet", 2, 1)
cmd_register("unit drop inet", "mbb-unit-drop-inet", "unit_drop_inet", 1)
cmd_register("unit clear inet", "mbb-unit-clear-inet", "unit_clear_inet", 1)

cmd_register("unit mod inet time start", "mbb-unit-mod-inet-time", "unit_mod_inet_time_start", 2)
cmd_register("unit mod inet time end", "mbb-unit-mod-inet-time", "unit_mod_inet_time_end", 2)
cmd_register("unit mod inet nice", "mbb-unit-mod-inet-nice", "unit_mod_inet_nice", 2)

cmd_register("unit map showall", "mbb-unit-map-showall", "unit_map_show", 1)
cmd_register("unit map show", "mbb-unit-map-show", "unit_map_show", 1)
cmd_register("unit map inet showall", "mbb-unit-map-inet-showall", "unit_map_inet_showall", 1)
cmd_register("unit map reload", "mbb-unit-map-rebuild", "unit_map_rebuild", 1)
cmd_register("unit map sync", "mbb-unit-map-rebuild", "unit_map_sync", 1)
cmd_register("unit map clear", "mbb-unit-map-clear", "unit_map_clear", 1)

cmd_register("map add", "mbb-map-add-unit", "map_do_unit", 1)
cmd_register("map del", "mbb-map-del-unit", "map_do_unit", 1)
cmd_register("map show all", "mbb-map-show-all", "map_show")
cmd_register("map show inet", "mbb-map-show-inet", "map_show_inet", 1)
cmd_register("map show unit", "mbb-map-show-unit", "map_show_unit", 1)
cmd_register("map clear", "mbb-map-clear")
cmd_register("map glue null", "mbb-map-glue-null")
cmd_register("map find net", "mbb-map-find-net", "map_find_net", 1, 3)
cmd_register("map find unit", "mbb-map-find-unit", "map_find_unit", 1)
cmd_register("map reload", "mbb-map-clear", "map_reload")
