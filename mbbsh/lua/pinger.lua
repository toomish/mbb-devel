function pinger_do(tag, name)
	tag.unit._name = name
	mbb.request(tag)
end

function pinger_show_units(tag)
	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.unit, "_name")) do
		print(xt._name)
	end
end

cmd_register("pinger add", "mbb-pinger-add", "pinger_do", 1)
cmd_register("pinger del", "mbb-pinger-del", "pinger_do", 1)
cmd_register("pinger show units", "mbb-pinger-show-units", "pinger_show_units")
