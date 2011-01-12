drop function update_unit_stat (integer, integer, bigint, bigint, bigint);

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

