function show_users(tag, re)
	if re then tag.regex._value = re end

	local xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.user, "_name")) do
		print(xt._name)
	end
end

function self_show_groups(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.group, "_name")) do
		print(xt._name)
	end
end

function user_show_groups(tag, user)
	tag.user._name = user
	self_show_groups(tag)
end

function add_user(tag, name)
	tag.name._value = name
	tag.secret._value = readpass()

	mbb.request(tag)
end

function user_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

function user_mod_pass(tag, name)
	local pass

	tag.name._value = name

	pass = readpass()
	if not pass then
		tag.newsecret._value = ""
	else
		tag.newsecret._value = pass
	end

	mbb.request(tag)
end

function self_mod_pass(tag)
	local pass

	tag.secret._value = getpass("current password: ")

	pass = readpass()
	if not pass then
		tag.newsecret._value = ""
	else
		tag.newsecret._value = pass
	end

	mbb.request(tag)
end

function drop_user(tag, name)
	if caution() then
		tag.name._value = name
		mbb.request(tag)
	end
end

cmd_register("show users", "mbb-show-users", "show_users", 0, 1)
cmd_register("user show groups", "mbb-user-show-groups", "user_show_groups", 1)
cmd_register("self show groups", "mbb-self-show-groups", "self_show_groups")
cmd_register("add user", "mbb-add-user", "add_user", 1)
cmd_register("user mod name", "mbb-user-mod-name", "user_mod_name", 2)
cmd_register("user mod pass", "mbb-user-mod-pass", "user_mod_pass", 1)
cmd_register("self mod pass", "mbb-self-mod-pass", "self_mod_pass")
cmd_register("drop user", "mbb-drop-user", "drop_user", 1)
