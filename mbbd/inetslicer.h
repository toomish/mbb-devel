/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef INET_SLICER_H
#define INET_SLICER_H

#include "slicer.h"

#define IPV4_TO_POINTER(i) ((gpointer) (gulong) (i))
#define GPOINTER_TO_IPV4(p) ((ipv4_t) (gulong) (p))

extern struct slice_ops inet_ops;

Slicer *inet_slicer_new(struct slice_data_ops *sdops);

#endif
