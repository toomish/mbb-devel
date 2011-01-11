create or replace function update_unit_link_stat(
	integer, integer, bigint, bigint, bigint
) returns void as $$
begin
	update unit_link_stat set
		nbyte_in = nbyte_in + $3,
		nbyte_out = nbyte_out + $4
	where
		unit_id = $1 and gwlink_id = $2 and point = $5;
	if not found then
		insert
			into unit_link_stat
			(unit_id, gwlink_id, nbyte_in, nbyte_out, point)
		values
			($1, $2, $3, $4, $5);
	end if;
end;
$$ language plpgsql;

create or replace function update_unit_stat(
	integer, bigint, bigint, bigint
) returns void as $$
begin
	update unit_stat set
		nbyte_in = nbyte_in + $2,
		nbyte_out = nbyte_out + $3
	where
		unit_id = $1 and point = $4;
	if not found then
		insert
			into unit_stat
			(unit_id, nbyte_in, nbyte_out, point)
		values
			($1, $2, $3, $4);
	end if;
end;
$$ language plpgsql;

create or replace function update_link_stat(
	integer, bigint, bigint, bigint
) returns void as $$
begin
	update link_stat set
		nbyte_in = nbyte_in + $2,
		nbyte_out = nbyte_out + $3
	where
		gwlink_id = $1 and point = $4;
	if not found then
		insert
			into link_stat
			(gwlink_id, nbyte_in, nbyte_out, point)
		values
			($1, $2, $3, $4);
	end if;
end;
$$ language plpgsql;

create or replace function ptou(timestamp with time zone) returns bigint as $$
	select extract('epoch' from $1)::bigint;
$$ language sql strict stable;

create or replace function utop(bigint) returns timestamp with time zone as $$
	select (timestamp with time zone 'epoch' + $1 * interval '1 second');
$$ language sql strict stable;

create or replace function
	create_view_dates(name text, step text, tstart bigint, tend bigint)
returns void as $$
begin
	execute 'create temp view ' || quote_ident(name) || ' as'
		|| ' select distinct date_trunc(' || quote_literal(step)
		|| ' , utop(point)) as date'
		|| ' from (select distinct point from unit_stat'
		|| ' where point >= ' || tstart
		|| ' and point < ' || tend || ') as points order by date';
end;
$$ language plpgsql;

