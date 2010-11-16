function log_show_levels(tag, sid)
	local xml

	if sid ~= nil then
		tag.session._sid = sid
	end

	xml = mbb.request(tag)

	local cmp = function (a, b)
		local sa = a._value
		local sb = b._value

		if #sa < #sb then
			return true 
		elseif #sa > #sb then
			return false
		elseif sa < sb then
			return true
		end

		return false
	end

	for n, xt in ipairs(xml_tag_fsort(xml.level, cmp)) do
		print(xt._value)
	end
end

function log_do_levels(tag, ...)
	for n, lvl in pairs({ ... }) do
		tag.level.__next._value = lvl
	end

	mbb.request(tag)
end

function log_trace_show(tag)
	local xml

	xml = mbb.request(tag)

	local buf = ""

	for n, xt in ipairs(xml_tag_nsort(xml.session, "_sid")) do
		local sid

		if buf ~= "" then buf = buf .. " " end

		sid = xt._sid
		if not xt._zombie then
			buf = buf .. sid
		else
			buf = buf .. string.format("[%s]", sid)
		end
	end

	if buf ~= "" then
		print(buf)
	end
end

function log_trace_do(tag, ...)
	for n, sid in pairs({ ... }) do
		tag.sid.__next._value = sid
	end

	mbb.request(tag)
end

function log_push(tag, ...)
	tag.message._value = table.concat({ ... }, " ");
	mbb.request(tag)
end

cmd_register("server log showall", "mbb-server-log-showall", "log_show_levels")
cmd_register("log on", "mbb-log-on")
cmd_register("log off", "mbb-log-off")
cmd_register("log show", "mbb-log-show-levels", "log_show_levels", 0, 1)
cmd_register("log set", "mbb-log-set-levels", "log_do_levels", nil)
cmd_register("log add", "mbb-log-add-levels", "log_do_levels", nil)
cmd_register("log del", "mbb-log-del-levels", "log_do_levels", nil)
cmd_register("log trace show", "mbb-log-trace-show", "log_trace_show")
cmd_register("log trace add", "mbb-log-trace-add", "log_trace_do", nil)
cmd_register("log trace del", "mbb-log-trace-del", "log_trace_do", nil)
cmd_register("log trace clean", "mbb-log-trace-clean")
cmd_register("log trace zclean", "mbb-log-trace-zclean")
cmd_register("log push", "mbb-log-push", "log_push", 1, nil)
