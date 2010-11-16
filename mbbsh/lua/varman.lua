function var_list(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.var, "_name")) do
		local mod = xt._module

		if mod == nil then
			print(xt._name)
		else
			printf("%s [%s]", xt._name, mod)
		end
	end
end

function var_show(tag, var)
	local xml

	tag.var._name = var

	xml = mbb.request(tag)
	print(xml.var._value)
end

function var_set(tag, var, value)
	tag.var._name = var
	tag.var._value = value

	mbb.request(tag)
end

function show_vars(tag, re)
	if re then tag.regex._value = re end

	local xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.var, "_name")) do
		printf("%s = %s", xt._name, xt._value)
	end
end

function var_cache_add(tag, var, value)
	tag.var._name = var
	if value then
		tag.var._value = value
	end

	mbb.request(tag)
end

function var_cache_do(tag, var)
	tag.var._name = var
	mbb.request(tag)
end

cmd_register("var list", "mbb-var-list", "var_list")
cmd_register("var show", "mbb-var-show", "var_show", 1)
cmd_register("var set", "mbb-var-set", "var_set", 2)
cmd_register("show vars", "mbb-show-vars", "show_vars", 0, 1)

cmd_register("var cache add", "mbb-var-cache-add", "var_cache_add", 1, 1)
cmd_register("var cache load", "mbb-var-cache-load", "var_cache_do", 1)
cmd_register("var cache del", "mbb-var-cache-del", "var_cache_do", 1)
cmd_register("var cache show", "mbb-var-cache-show", "var_show", 1)
cmd_register("var cache list", "mbb-var-cache-list", "var_list")
cmd_register("show cached", "mbb-show-cached", "show_vars")
