drop rule if exists attr_del on attributes;
drop function if exists attr_install(text, text, text, text);

drop table if exists attribute_pool;
drop table if exists attributes;
drop table if exists attribute_group;

drop function if exists attr_clean(integer, integer) cascade;

drop function if exists attr_get(integer array, integer);
drop function if exists attr_set(integer, text, integer);
drop function if exists attr_find(integer, text);

