/* Copyright (C) 2012 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef MBB_NETFLOW_H
#define MBB_NETFLOW_H

#include "xmltag.h"

#include "pathtree.h"
#include "flow.h"

extern struct stat_interface *stat_lib;

GSList *netflow_get_glob_list(XmlTag *tag, XmlTag **ans);

XmlTag *mbb_xml_msg_task_id(gint id);

GString *netflow_get_data_dir(void);
gchar *netflow_get_store_dir(gchar *name);

#endif
