/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef TIME_SLICER_H
#define TIME_SLICER_H

#include "slicer.h"

#define TIME_TO_POINTER(t) ((gpointer) (t))
#define GPOINTER_TO_TIME(p) ((time_t) (p))

extern struct slice_ops time_ops;

Slicer *time_slicer_new(struct slice_data_ops *sdops);

#endif
