#ifndef FLOW_H
#define FLOW_H

#include <glib.h>
#include <time.h>

#define FLOW_HEADER_SIZE 28

typedef enum {
	FLOW_FIELD_PROTO = 1,
	FLOW_FIELD_PORT = 2
} FlowField;

typedef struct flow_stream FlowStream;

struct flow_data {
	guint32 srcaddr;
	guint32 dstaddr;
	guint32 exaddr;
	guint32 nbytes;
	guint16 input;
	guint16 output;
	guint16 srcport;
	guint16 dstport;
	guint8 proto;
	time_t begin;
};

typedef enum {
	FLOW_ERROR_OPEN_FAILED,
	FLOW_ERROR_INIT_FAILED,
	FLOW_ERROR_POOR_FLOW,
} FlowError;

#define FLOW_ERROR (flow_error_quark())

GQuark flow_error_quark(void);

FlowStream *flow_stream_new(gchar *path, guint mask, GError **error);
guint32 flow_stream_length(FlowStream *flow);
guint32 flow_stream_tell(FlowStream *flow);
void flow_stream_close(FlowStream *flow);

gboolean flow_stream_read(FlowStream *flow, struct flow_data *fd);

#endif
