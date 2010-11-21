create table attribute_group (
	group_id serial primary key,
	group_name text not null unique
);

create table attributes (
	attr_id serial primary key,
	attr_name text not null check (char_length(attr_name) > 0),
	group_id integer references attribute_group not null,

	unique (attr_name, group_id)
);

create table attribute_pool (
	obj_id integer not null,
	attr_id integer references attributes not null,
	attr_value text not null,

	unique (obj_id, attr_id)
);

create index attribute_pool_obj_id_index on attribute_pool (obj_id);

create function attr_clean(integer, integer) returns void as $$
	delete
		from attribute_pool p using attributes a
	where
		obj_id = $2 and p.attr_id = a.attr_id
		and a.group_id = $1;
$$ language sql;

create function attr_install(name text, tab text, fid text) returns void as $$
declare
	id integer;
begin
	insert into attribute_group (group_name) values (name)
	returning group_id into id;

	execute 'create rule ' || quote_ident('attr_del_' || name)
		|| ' as on delete to ' || quote_ident(tab)
		|| ' do also select attr_clean(' || id
		|| ', OLD.' || quote_ident(fid) || ')';
end;
$$ language plpgsql;

select attr_install ('user', 'users', 'user_id');
select attr_install ('unit', 'units', 'unit_id');
select attr_install ('consumer', 'consumers', 'consumer_id');
select attr_install ('operator', 'operators', 'oper_id');
select attr_install ('gateway', 'gateways', 'gw_id');

create rule attr_del as on delete to attributes do also
	delete from attribute_pool where attr_id = OLD.attr_id;

create function attr_get(aidv integer[], oid integer) returns table(id integer, value text) as $$
begin
	return query select
		attr_id, attr_value
	from
		attribute_pool, unnest(aidv) as foo(aid)
	where
		attr_id = aid and obj_id = oid;
end;
$$ language plpgsql;

create function attr_set(aid integer, val text, oid integer) returns void as $$
begin
	update attribute_pool set attr_value = val where obj_id = oid and attr_id = aid;
	if not found then
		insert into attribute_pool (obj_id, attr_id, attr_value)
		values (oid, aid, val);
	end if;
end;
$$ language plpgsql;

create function attr_find(aid integer, exp text) returns table (id integer, value text) as $$
begin
	return query select
		obj_id, attr_value
	from attribute_pool where attr_id = aid and attr_value ~* exp;
end;
$$ language plpgsql;

