create table pinger_units (
	unit_id integer references units not null,
	creation bigint not null default ptou(current_timestamp)
);

create rule pinger_unit_del as on delete to units do also
	delete from pinger_units where unit_id = OLD.unit_id;

