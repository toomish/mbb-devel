function show_gateways(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.gateway, "_name")) do
		printf("%-20s %s", xt._name, xt._addr)
	end
end

function add_gateway(tag, name, addr)
	tag.name._value = name
	tag.addr._value = addr

	mbb.request(tag)
end

function drop_gateway(tag, name)
	if caution() then
		tag.name._value = name
		mbb.request(tag)
	end
end

function gateway_mod_name(tag, name, newname)
	tag.name._value = name
	tag.newname._value = newname

	mbb.request(tag)
end

cmd_register("show gateways", "mbb-show-gateways", "show_gateways")
cmd_register("add gateway", "mbb-add-gateway", "add_gateway", 2)
cmd_register("drop gateway", "mbb-drop-gateway", "drop_gateway", 1)
cmd_register("gateway mod name", "mbb-gateway-mod-name", "gateway_mod_name", 2)
