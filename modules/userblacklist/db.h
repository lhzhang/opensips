/*
 * $Id$
 *
 * Copyright (C) 2007 1&1 Internet AG
 *
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _DB_H_
#define _DB_H_

#include "../../sr_module.h"
#include "dt.h"

int db_bind(const str *url);
int db_init(const str *url, const str *table);
void db_destroy(void);

int db_build_userbl_tree(const str *user, const str *domain, const str *table, struct dt_node_t *root, int use_domain);
int db_reload_source(const str *table, struct dt_node_t *root);

#endif
