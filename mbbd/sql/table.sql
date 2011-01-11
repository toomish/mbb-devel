-- create type auth_policy as enum ('drop', 'pass', 'trust');

create table users (
	user_id serial primary key,
	user_name text not null unique,
	user_secret text
	-- policy auth_policy not null
);

/*
create table user_auth (
	user_id integer references users not null,
	inet_addr inet not null,
	policy auth_policy not null
);
*/

insert into users (user_name, user_secret) values ('root', 'toor');

create table groups (
	group_id integer primary key check (group_id < 32),
	group_name text not null unique
);

insert into groups values (0, 'root');
insert into groups values (1, 'wheel');
insert into groups values (4, 'admin');
insert into groups values (10, 'log');
insert into groups values (20, 'user');
insert into groups values (26, 'consumer');

create table groups_pool (
	group_id integer references groups not null,
	user_id integer references users not null,

	unique (group_id, user_id)
);

insert into groups_pool values (0, (select user_id from users where user_name = 'root'));

create rule user_group_del as on delete to users do also
	delete from groups_pool where user_id = OLD.user_id;

create table consumers (
	consumer_id serial primary key,
	consumer_name text not null unique,

	user_id integer references users default null,

	time_start bigint not null check (time_start >= 0),
	time_end bigint not null check (time_end >= 0),

	check (time_start = 0 or time_end = 0 or time_start <= time_end)
);

create table units (
	unit_id serial primary key,
	unit_name text not null unique,

	consumer_id integer references consumers default null,

	time_start bigint check (time_start >= 0),
	time_end bigint check (time_end >= 0),

	local boolean not null default false,

	check (time_start = 0 or time_end = 0 or time_start <= time_end)
);

create table unit_ip_pool (
	self_id serial primary key,
	unit_id integer references units not null,

	inet_addr inet not null,
	inet_flag boolean not null default true,

	time_start bigint check (time_start >= 0),
	time_end bigint check (time_end >= 0),

	nice integer not null default 0 check (nice >= 0),

	check (time_start = 0 or time_end = 0 or time_start <= time_end)
);

create sequence gateways_id_seq;
create table gateways (
	gw_id serial primary key,
	gw_name text not null unique,
	gw_ip inet not null unique check (masklen(gw_ip) = 32)
);

create table operators (
	oper_id serial primary key,
	oper_name text not null unique
);

create table gwlinks (
	gwlink_id serial primary key,

	oper_id integer references operators not null,
	gw_id integer references gateways not null,
	link integer not null,

	time_start bigint not null check (time_start >= 0),
	time_end bigint not null check (time_end >= 0),

	check (time_start = 0 or time_end = 0 or time_start <= time_end)
);

create table unit_stat (
	unit_id integer references units not null,

	nbyte_in bigint not null,
	nbyte_out bigint not null,

	point bigint not null,

	unique (unit_id, point)
);

create index unit_stat_unit_id_index on unit_stat (unit_id);
create index unit_stat_hour_no_index on unit_stat (point);

create table link_stat (
	gwlink_id integer references gwlinks not null,

	nbyte_in bigint not null,
	nbyte_out bigint not null,

	point bigint not null,

	unique (gwlink_id, point)
);

create index link_stat_gwlink_id_index on link_stat (gwlink_id);
create index link_stat_hour_no_index on link_stat (point);

create table unit_link_stat (
	unit_id integer references units not null,
	gwlink_id integer references gwlinks not null,

	nbyte_in bigint not null,
	nbyte_out bigint not null,

	point bigint not null,

	unique (unit_id, gwlink_id, point)
);

