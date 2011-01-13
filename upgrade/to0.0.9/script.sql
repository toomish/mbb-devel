create or replace function migrate(src text, dst text) returns void as $$
	from time import strftime, localtime

	FETCH_COUNT = 1000
	TIME_FMT = "%Y.%m.%d %H:%M"

	def timefmt(nsec, fmt = TIME_FMT):
		return strftime(fmt, localtime(nsec))


	class StatEntry:
		def __init__(self, x, y):
			self.nbyte_in = x
			self.nbyte_out = y

		def add(self, entry):
			self.nbyte_in += entry.nbyte_in
			self.nbyte_out += entry.nbyte_out

	def save():
		if point is None:
			return
	
		for uid in sorted(table):
			st = table[uid]
			plpy.execute("insert into %s values (%d, %d, %d, %d)" %
				(dst, uid, st.nbyte_in, st.nbyte_out, point))

		plpy.notice("save %s" % timefmt(point))
		table.clear()

	point = None
	table = {}

	plpy.execute("declare iter cursor for select * from %s order by point" % src)
	while True:
		data = plpy.execute("fetch %d from iter" % FETCH_COUNT)
		if data.nrows() is 0:
			save()
			break

		for row in data:
			new_point = int(row["point"])
			if point != new_point:
				save()
				point = new_point

			uid = int(row["unit_id"])
			entry = StatEntry(int(row["nbyte_in"]), int(row["nbyte_out"]))

			if uid not in table:
				table[uid] = entry
			else:
				table[uid].add(entry)
$$ language plpythonu;

create table temp_stat (
        unit_id integer references units not null,

        nbyte_in bigint not null,
        nbyte_out bigint not null,

        point bigint not null,

        unique (unit_id, point)
);

set enable_seqscan to false;
select migrate('unit_stat', 'temp_stat');
drop function migrate (text, text);

alter index unit_stat_unit_id_index rename to unit_link_stat_unit_id_index;
alter index unit_stat_hour_no_index rename to unit_link_stat_hour_no_index;

alter table unit_stat rename to unit_link_stat;
alter table temp_stat rename to unit_stat;

create index unit_stat_unit_id_index on unit_stat (unit_id);
create index unit_stat_hour_no_index on unit_stat (point);

