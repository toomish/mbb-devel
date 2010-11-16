function stat_tag_fill(tag, start, op, optarg)
	tag.time._start = start

	if op == "to" then
		tag.time._end = optarg
	elseif op == "as" then
		tag.time._period = optarg
	else
		error("invalid operand '%s'", op)
	end
end

function stat_common(tag, start, op, optarg, ...)
	local size_args = { b = 1, k = 1024, m = 1024 ^ 2, g = 1024 ^ 3 }
	local align = { 0, 0, 0, 0 }
	local opt_step = false
	local opt_size = 1
	local opt_align = false

	local args, arglen
	local xml

	args = { ... }
	arglen = #args

	stat_tag_fill(tag, start, op, optarg)

	local n = 1
	while n <= arglen do
		local op = args[n]

		local function get_next_arg(msg)
			if msg == nil then
				msg = "option missed"
			end

			if n == arglen then
				error(msg)
			end

			n = n + 1
			return args[n]
		end

		if op == "--by" then
			if tag.time._step then
				error("operand 'by' is already used")
			end

			tag.time._step = get_next_arg()
			opt_step = true
		elseif op == "-s" or op == "--size" then
			local tmp = get_next_arg()

			if not size_args[tmp] then
				error("invalid size argument '%s'", tmp)
			end

			opt_size = size_args[tmp]
		elseif op == "-a" or op == "--aligned" then
			opt_align = true
		elseif op == "--for" then
			tag.name._value = get_next_arg("names missed")

			for i = n + 1, arglen do
				tag.name.__next._value = args[i]
			end

			break
		else
			error("invalid operand '%s'", op)
		end

		n = n + 1
	end

	xml = mbb.request(tag)

	local vt = {}
	for xt in xml_tag_iter(xml.stat) do
		local entry = {}
		entry.name = xt._name

		if opt_size == 1 then
			entry.bin = xt._in
			entry.bout = xt._out
		else
			entry.bin = string.format("%.3f", tonumber(xt._in) / opt_size)
			entry.bout = string.format("%.3f", tonumber(xt._out) / opt_size)
		end

		if opt_step then
			entry.point = timefmt(xt._point)
		end

		list_push(vt, entry)
	end

	if #vt == 0 then
		return
	end

	local cmp_func

	if not opt_step then
		cmp_func = function (a, b)
			return a.name < b.name
		end
	else
		cmp_func = function (a, b)
			if a.point == b.point then
				return a.name < b.name
			end

			return a.point < b.point
		end
	end

	table.sort(vt, cmp_func)

	if opt_align then
		local function align_save_max(len, no)
			if len > align[no] then
				align[no] = len
			end
		end

		for k, v in ipairs(vt) do
			align_save_max(#v.name, 1)
			align_save_max(#v.bin, 2)
			align_save_max(#v.bout, 3)

			if opt_step then align_save_max(#v.point, 4) end
		end

		for k, v in ipairs(align) do
			align[k] = v + 1
		end
	end

	local fmt

	if not opt_align then
		fmt = "%s %s %s"

		if opt_step then
			fmt = fmt .. " %s"
		end
	else
		fmt = string.format("%%-%ds%%-%ds%%-%ds", align[1], align[2], align[3])

		if opt_step then
			fmt = fmt .. string.format("%%-%ds", align[4])
		end
	end

	if not opt_step then
		for k, entry in ipairs(vt) do
			printf(fmt, entry.name, entry.bin, entry.bout)
		end
	else
		for k, entry in ipairs(vt) do
			printf(fmt, entry.name, entry.bin, entry.bout, entry.point)
		end
	end
end

function stat_wipe(tag, start, op, optarg)
	stat_tag_fill(tag, start, op, optarg)

	if caution() then mbb.request(tag) end
end

cmd_register("stat unit", "mbb-stat-unit", "stat_common", 3, nil)
cmd_register("stat consumer", "mbb-stat-consumer", "stat_common", 3, nil)
cmd_register("stat operator", "mbb-stat-operator", "stat_common", 3, nil)
cmd_register("stat link", "mbb-stat-link", "stat_common", 3, nil)
cmd_register("stat gateway", "mbb-stat-gateway", "stat_common", 3, nil)

cmd_register("stat wipe", "mbb-stat-wipe", "stat_wipe", 3)

cmd_register("self stat unit", "mbb-self-stat-unit", "stat_common", 3, nil)
cmd_register("self stat consumer", "mbb-self-stat-consumer", "stat_common", 3, nil)
