#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <glib.h>

#include <ftlib.h>

#include "flow.h"

#include "strerr.h"

struct flow_stream {
	struct ftio ftio;
	struct fts3rec_offsets offsets;
	FlowField mask;
	guint32 length;
	guint32 cur;
};

#define IMPORT_REC(cur, field, fo, addr, type) \
	cur.field = (type *) (addr + fo.field)

#define IMPORT_REC8(cur, field, fo, addr) \
	IMPORT_REC(cur, field, fo, addr, guint8)

#define IMPORT_REC16(cur, field, fo, addr) \
	IMPORT_REC(cur, field, fo, addr, guint16)

#define IMPORT_REC32(cur, field, fo, addr) \
	IMPORT_REC(cur, field, fo, addr, guint32)

#define FLOW_MEM(type, field, off, ptr) (*(type *) (ptr + off.field))
#define FLOW_MEM8(field, off, ptr) FLOW_MEM(guint8, field, off, ptr)
#define FLOW_MEM16(field, off, ptr) FLOW_MEM(guint16, field, off, ptr)
#define FLOW_MEM32(field, off, ptr) FLOW_MEM(guint32, field, off, ptr)

static const guint FT_REQ_FIELDS =
	FT_XFIELD_SRCADDR | FT_XFIELD_DSTADDR | FT_XFIELD_DOCTETS |
	FT_XFIELD_EXADDR | FT_XFIELD_DPKTS | FT_XFIELD_NEXTHOP |
	FT_XFIELD_INPUT | FT_XFIELD_OUTPUT |
	FT_XFIELD_UNIX_SECS | FT_XFIELD_UNIX_NSECS |
	FT_XFIELD_SYSUPTIME | FT_XFIELD_FIRST;

GQuark flow_error_quark(void)
{
	return g_quark_from_string("flow-error-quark");
}

static inline void flows_offsets(struct ftio *ftio, struct fts3rec_offsets *fo)
{
        struct ftver ftv;

        ftio_get_ver(ftio, &ftv);
        fts3rec_compute_offsets(fo, &ftv);
}

FlowStream *flow_stream_new(gchar *path, guint mask, GError **error)
{
	guint xfield_mask = FT_REQ_FIELDS;
	FlowStream flow;
	gint fd;

	if (mask & FLOW_FIELD_PROTO)
		xfield_mask |= FT_XFIELD_PROT;

	if (mask & FLOW_FIELD_PORT) {
		xfield_mask |= FT_XFIELD_SRCPORT;
		xfield_mask |= FT_XFIELD_DSTADDR;
	}

	if ((fd = open(path, O_RDONLY)) < 0) {
		gchar *msg = strerr(errno);

		g_set_error(error, FLOW_ERROR, FLOW_ERROR_OPEN_FAILED,
			"open failed: %s", msg);
		g_free(msg);
		return NULL;
	}

	if (ftio_init(&flow.ftio, fd, FT_IO_FLAG_READ) < 0) {
		close(fd);
		g_set_error(error, FLOW_ERROR, FLOW_ERROR_INIT_FAILED,
			"invalid stream");
		return NULL;
	}

	if (ftio_check_xfield(&flow.ftio, xfield_mask) < 0) {
		close(fd);
		g_set_error(error, FLOW_ERROR, FLOW_ERROR_POOR_FLOW,
			"required fields are missing");
		return NULL;
	}

	flows_offsets(&flow.ftio, &flow.offsets);
	flow.length = ftio_get_flows_count(&flow.ftio);
	flow.mask = mask;
	flow.cur = 0;

	return g_memdup(&flow, sizeof(FlowStream));;
}

guint32 flow_stream_length(FlowStream *flow)
{
	return flow->length;
}

guint32 flow_stream_tell(FlowStream *flow)
{
	return flow->cur;
}

void flow_stream_close(FlowStream *flow)
{
	ftio_close(&flow->ftio);
	g_free(flow);
}

gboolean flow_stream_read(FlowStream *flow, struct flow_data *fd)
{
	guint32 noctet, npkt;
	struct fts3rec_all cur;
	struct fttime ftt;
	char *data;

	data = ftio_read(&flow->ftio);
	if (data == NULL)
		return FALSE;

	fd->srcaddr = FLOW_MEM32(srcaddr, flow->offsets, data);
	fd->dstaddr = FLOW_MEM32(dstaddr, flow->offsets, data);
	fd->exaddr = FLOW_MEM32(exaddr, flow->offsets, data);

	noctet = FLOW_MEM32(dOctets, flow->offsets, data);
	npkt = FLOW_MEM32(dPkts, flow->offsets, data);

	fd->nbytes = noctet + npkt * FLOW_HEADER_SIZE;

	fd->input = FLOW_MEM16(input, flow->offsets, data);
	fd->output = FLOW_MEM16(output, flow->offsets, data);

	if (flow->mask & FLOW_FIELD_PROTO)
		fd->proto = FLOW_MEM8(prot, flow->offsets, data);

	if (flow->mask & FLOW_FIELD_PORT) {
		fd->srcport = FLOW_MEM16(srcport, flow->offsets, data);
		fd->dstport = FLOW_MEM16(dstport, flow->offsets, data);
	}

	IMPORT_REC32(cur, unix_secs, flow->offsets, data);
	IMPORT_REC32(cur, unix_nsecs, flow->offsets, data);
	IMPORT_REC32(cur, sysUpTime, flow->offsets, data);
	IMPORT_REC32(cur, First, flow->offsets, data);

	ftt = ftltime(
		*cur.sysUpTime, *cur.unix_secs,
		*cur.unix_nsecs, *cur.First
	);

	fd->begin = ftt.secs;

	flow->cur++;

	return TRUE;
}

