function modname(name)
	if string.sub(name, -3) == ".so" then
		return name
	end

	return name .. ".so"
end

function module_list(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.module, "_name")) do
		print(xt._name)
	end
end

function module_do_load(tag, name)
	tag.module._name = modname(name)
	mbb.request(tag)
end

function module_reload(tag, name)
	tag.module._name = modname(name)
	mbb.request(tag)

	tag._name = "mbb-module-load"
	mbb.request(tag)
end

function show_modules(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.module, "_name")) do
		ts = timewrap(xt._start, false)
		printf("%-20s %4s %2s/%-2s %s [%d]",
			xt._name, xt._state, xt._nreg, xt._total, ts, xt._nuse
		)
	end
end

function print_mod_info(caption, xml)
	local reg_status

	print(caption .. ":")

	for n, xt in ipairs(xml_tag_sort(xml, "_name")) do
		reg_status = "*"
		if xt._reg ~= "true" then
			reg_status = " "
		end

		printf("\t%s %s", reg_status, xt._name)
	end
end
	
function module_info(tag, name)
	local xml

	tag.module._name = modname(name)

	xml = mbb.request(tag)

	local funcs, vars, reg_status
	local xt

	xt = xml.desc
	if xt then printf("info: %s", xt.__body) end

	funcs = xml.func
	if funcs ~= nil then print_mod_info("functions", funcs) end

	vars = xml.var
	if vars ~= nil then print_mod_info("variables", vars) end
end

function module_do(tag, name)
	tag.module._name = modname(name)
	mbb.request(tag)
end

cmd_register("module list", "mbb-module-list", "module_list")
cmd_register("module load", "mbb-module-load", "module_do_load", 1)
cmd_register("module unload", "mbb-module-unload", "module_do_load", 1)
cmd_register("module reload", "mbb-module-unload", "module_reload", 1)
cmd_register("show modules", "mbb-show-modules", "show_modules")
cmd_register("module info", "mbb-module-info", "module_info", 1)
cmd_register("module run", "mbb-module-run", "module_do", 1)
cmd_register("module stop", "mbb-module-stop", "module_do", 1)
cmd_register("module vars save", "mbb-module-vars-save", "module_do", 1)
cmd_register("module vars load", "mbb-module-vars-load", "module_do", 1)
