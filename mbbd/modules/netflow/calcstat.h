/* Copyright (C) 2010 Mikhail Osipov <mike.osipov@gmail.com> */
/* Published under the GNU General Public License V.2, see file COPYING */

#ifndef CALC_STAT_H
#define CALC_STAT_H

#include "xmltag.h"

void update_stat(XmlTag *tag, XmlTag **ans);
void plain_stat(XmlTag *tag, XmlTag **ans);
void feed_stat(XmlTag *tag, XmlTag **ans);

#endif
