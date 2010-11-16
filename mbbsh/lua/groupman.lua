function show_groups(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.group, "_name")) do
		print(xt._name)
	end
end

function group_show_users(tag, group)
	local xml

	tag.group._name = group
	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.user, "_name")) do
		print(xt._name)
	end
end

function group_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

function group_do_user(tag, group, user)
	tag.group._name = group
	tag.user._name = user

	mbb.request(tag)
end

cmd_register("show groups", "mbb-show-groups", "show_groups")
cmd_register("group show users", "mbb-group-show-users", "group_show_users", 1)
cmd_register("group mod name", "mbb-group-mod-name", "group_mod_name", 2)
cmd_register("group add user", "mbb-group-add-user", "group_do_user", 2)
cmd_register("group del user", "mbb-group-del-user", "group_do_user", 2)
