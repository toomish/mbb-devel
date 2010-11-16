function attr_list(tag, group)
	local xml

	tag.attr._group = group
	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.attr, "_name")) do
		print(xt._name)
	end
end

function attr_man(tag, group, name)
	tag.attr._group = group
	tag.attr._name = name

	mbb.request(tag)
end

function attr_del(tag, group, name)
	if caution() then
		attr_man(tag, group, name)
	end
end

function attr_rename(tag, group, name, newname)
	tag.attr._group = group
	tag.attr._name = name
	tag.newname._value = newname

	mbb.request(tag)
end

function attr_set(tag, group, obj, name, value)
	tag.attr._group = group
	tag.attr._name = name
	tag.attr._value = value
	tag.obj._name = obj

	mbb.request(tag)
end

function attr_unset(tag, group, obj, name)
	tag.attr._group = group
	tag.attr._name = name
	tag.obj._name = obj

	mbb.request(tag)
end

function attr_get(tag, group, obj, ...)
	tag.attr._group = group
	tag.obj._name = obj

	for n, name in pairs({ ... }) do
		tag.attr.__next._name = name
	end

	local xml

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.attr, "_name")) do
		print(xt._name, xt._value)
	end
end

function attr_find(tag, group, name, value)
	local xml

	tag.attr._group = group
	tag.attr._name = name
	tag.attr._value = value

	xml = mbb.request(tag)

	for n, xt in ipairs(xml_tag_sort(xml.obj, "_name")) do
		print(xt._name, xt._value)
	end
end

cmd_register("attr list", "mbb-attr-list", "attr_list", 1)
cmd_register("attr add", "mbb-attr-add", "attr_man", 2)
cmd_register("attr del", "mbb-attr-del", "attr_del", 2)
cmd_register("attr rename", "mbb-attr-rename", "attr_rename", 3)
cmd_register("attr set", "mbb-attr-set", "attr_set", 4)
cmd_register("attr unset", "mbb-attr-unset", "attr_unset", 3)
cmd_register("attr get", "mbb-attr-get", "attr_get", 2, nil)
cmd_register("attr find", "mbb-attr-find", "attr_find", 3)
