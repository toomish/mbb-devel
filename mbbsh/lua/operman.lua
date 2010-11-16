function show_operators(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.operator, "_name")) do
		print(xt._name)
	end
end

function add_operator(tag, name)
	tag.name._value = name
	mbb.request(tag)
end

function drop_operator(tag, name)
	if caution() then
		tag.name._value = name
		mbb.request(tag)
	end
end

function operator_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

cmd_register("show operators", "mbb-show-operators", "show_operators")
cmd_register("add operator", "mbb-add-operator", "add_operator", 1)
cmd_register("drop operator", "mbb-drop-operator", "drop_operator", 1)
cmd_register("operator mod name", "mbb-operator-mod-name", "operator_mod_name", 2)
