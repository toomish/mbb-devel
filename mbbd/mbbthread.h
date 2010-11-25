/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_THREAD_H
#define MBB_THREAD_H

#include "mbbmsgqueue.h"
#include "mbbsession.h"
#include "signaller.h"
#include "mbbuser.h"
#include "mbbcap.h"

#include "xmlparser.h"

struct thread_env {
	guint sid;
	struct mbb_session ss;

        int sock;

        XmlParser *parser;

	MbbMsgQueue *msg_queue;
	Signaller *signaller;
};

struct mbb_user *mbb_thread_get_user(void);
mbb_cap_t mbb_thread_get_cap(void);
gint mbb_thread_get_uid(void);
gchar *mbb_thread_get_peer(void);
gint mbb_thread_get_tid(void);
MbbMsgQueue *mbb_thread_get_msg_queue(void);
Signaller *mbb_thread_get_signaller(void);

void mbb_thread_update_cap(struct thread_env *te);

void mbb_thread_http_client(struct thread_env *te);
void mbb_thread_xml_client(struct thread_env *te);

#endif
