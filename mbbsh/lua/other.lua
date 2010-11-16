function show_methods(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.method, "_name")) do
		print(xt._name)
	end
end

function show_user_methods(tag, user)
	tag.user._name = user
	show_methods(tag)
end

function show_auth_methods(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.method, "_name")) do
		local mod = xt._module

		if mod == nil then
			print(xt._name)
		else
			printf("%s [%s]", xt._name, mod)
		end
	end
end

function get_auth_key(tag)
	local xml

	xml = mbb.request(tag)

	local key

	key = xml.key._value
	if not key then
		error("key missed")
	else
		print(key)
	end
end

function show_auth_keys(tag)
	local xml

	xml = mbb.request(tag)

	for xt in xml_tag_iter(xml.key) do
		local fmt = "%s %s %s %s"

		printf(fmt, xt._value, xt._user, xt._peer, xt._lifetime)
	end
end

function drop_auth_key(tag, key)
	tag.key._value = key
	mbb.request(tag)
end

function server_get_time(tag)
	local xml

	xml = mbb.request(tag)

	local t

	t = xml.time._value
	if not t then
		error("time missed")
	else
		print(t)
	end
end

function show_sessions(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_nsort(xml.session, "_sid")) do
		local fmt = "%-7s%10s@%-15s {%s}"

		printf(fmt, xt._sid, xt._user, xt._peer, timefmt(xt._start))
	end
end

cmd_register("self show methods", "mbb-self-show-methods", "show_methods")
cmd_register("user show methods", "mbb-user-show-methods", "show_user_methods", 1)
cmd_register("get authkey", "mbb-get-auth-key", "get_auth_key")
cmd_register("drop authkey", "mbb-drop-auth-key", "drop_auth_key", 1)
cmd_register("show authkeys", "mbb-show-auth-keys", "show_auth_keys")
cmd_register("server get time", "mbb-server-get-time", "server_get_time")
cmd_register("show sessions", "mbb-show-sessions", "show_sessions")
cmd_register("server auth list", "mbb-server-auth-list", "show_auth_methods")
