function print_links(xml)
	for n, xt in ipairs(xml_tag_nsort(xml.gwlink, "_id")) do
		local ts, te
		local tmp
		local s

		ts = timefmt(xt._start)
		te = timefmt(xt._end)

		tmp = string.format("[%s]:%s", xt._link, xt._gateway)
		s = string.format("%-5s %-20s {%s} {%s}", xt._id, tmp, ts, te)

		if not xt._operator then
			print(s)
		else
			printf("%s %s", s, xt._operator)
		end
	end
end

function show_links(tag)
	local xml

	xml = mbb.request(tag)
	print_links(xml)
end

function operator_show_links(tag, name)
	local xml

	tag.operator._name = name
	xml = mbb.request(tag)

	print_links(xml)
end

function gateway_show_links(tag, name)
	local xml

	tag.gateway._name = name
	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.link, "_no")) do
		printf("[%s]", xt._no)

		for xt in xml_tag_iter(xt.gwlink) do
			local ts, te

			ts = timefmt(xt._start)
			te = timefmt(xt._end)

			printf("\t%-10s {%s} {%s} %s", xt._id, ts, te, xt._operator)
		end
	end
end

function add_link(tag, gw, link, op)
	tag.gateway._name = gw
	tag.link._value = link
	tag.operator._name = op

	mbb.request(tag)
end

function drop_link(tag, id)
	if caution() then
		tag.gwlink._id = id
		mbb.request(tag)
	end
end

function link_mod_time_start(tag, id, time)
	tag.gwlink._id = id
	tag.time._start = time

	mbb.request(tag)
end

function link_mod_time_end(tag, id, time)
	tag.gwlink._id = id
	tag.time._end = time

	mbb.request(tag)
end

function link_mod_time(tag, id, time_start, time_end)
	tag.gwlink._id = id
	tag.time._start = time_start
	tag.time._end = time_end

	mbb.request(tag)
end

cmd_register("show links", "mbb-show-gwlinks", "show_links")
cmd_register("operator show links", "mbb-operator-show-gwlinks", "operator_show_links", 1)
cmd_register("gateway show links", "mbb-gateway-show-gwlinks", "gateway_show_links", 1)
cmd_register("add link", "mbb-add-gwlink", "add_link", 3)
cmd_register("drop link", "mbb-drop-gwlink", "drop_link", 1)
cmd_register("link mod time start", "mbb-gwlink-mod-time", "link_mod_time_start", 2)
cmd_register("link mod time end", "mbb-gwlink-mod-time", "link_mod_time_end", 2)
cmd_register("link mod time range", "mbb-gwlink-mod-time", "link_mod_time", 3)
