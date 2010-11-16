function show_tasks(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_nsort(xml.task, "_id")) do
		local ts = timewrap(xt._start, false)
		local fmt = "%-7s %s %s %s:%s %s"

		printf(fmt, xt._id, xt._name, xt._state, xt._user, xt._sid, ts)
	end
end

function task_do_run(tag, no)
	tag.task._id = no
	mbb.request(tag)
end

cmd_register("show tasks", "mbb-show-tasks", "show_tasks")
cmd_register("task run", "mbb-task-run", "task_do_run", 1)
cmd_register("task stop", "mbb-task-stop", "task_do_run", 1)
cmd_register("task cancel", "mbb-task-cancel", "task_do_run", 1)
