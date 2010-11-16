function netflow_file_list(tag, ...)
	local xml

	for n, glob in pairs({ ... }) do
		tag.glob.__next._value = glob
	end

	xml = mbb.request(tag)

	for xt in xml_tag_iter(xml.file) do
		print(xt._path)
	end
end

function print_task(xml)
	printf("starting task %d", xml.task._id)
end

function netflow_grep_unit(tag, name, ...)
	local xml

	tag.unit._name = name
	for n, var in pairs({ ... }) do
		if string.sub(var, 1, 1) ~= "-" then
			tag.glob.__next._value = var
		else
			local tmp = string.match(var, "^%-%-with%-(%a+)$")

			if tmp ~= nil then
				tag.opt.__next._value = tmp
			else
				error("invalid option '%s'", var)
			end
		end
	end

	xml = mbb.request(tag)
	print_task(xml)
end

function netflow_stat_feed(tag, file)
	local xml

	tag.file._name = file
	xml = mbb.request(tag)

	print_task(xml)
end

function netflow_stat_plain(tag, ...)
	for n, glob in pairs({ ... }) do
		tag.glob.__next._value = glob
	end

	xml = mbb.request(tag)
	print_task(xml)
end

function netflow_stat_update(tag, start, op, optarg, ...)
	local xml

	stat_tag_fill(tag, start, op, optarg)
	netflow_stat_plain(tag, ...)
end

cmd_register("netflow ls", "mbb-netflow-file-list", "netflow_file_list", nil)
cmd_register("netflow grep unit", "mbb-netflow-grep-unit", "netflow_grep_unit", 2, nil)
cmd_register("netflow stat feed", "mbb-netflow-stat-feed", "netflow_stat_feed", 1)
cmd_register("netflow stat plain", "mbb-netflow-stat-plain", "netflow_stat_plain", 1, nil)
cmd_register("netflow stat update", "mbb-netflow-stat-update", "netflow_stat_update", 4, nil)
