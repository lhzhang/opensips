/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * ---------
 *  2004-02-11  FIFO/CANCEL + alignments (hash=f(callid,cseq)) (uli+jiri)
 */


#ifndef _CANCEL_H
#define _CANCEL_H

#include <stdio.h> /* just for FILE* for fifo_uac_cancel */

/* a buffer is empty but cannot be used by anyone else;
   particularly, we use this value in the buffer pointer
   in local_buffer to tell "a process is already scheduled
   to generate a CANCEL, other processes are not supposed to"
   (which might happen if for example in a three-branch forking,
   two 200 would enter separate processes and compete for
   canceling the third branch); note that to really avoid
   race conditions, the value must be set in REPLY_LOCK
*/

#define BUSY_BUFFER ((char *)-1)

void which_cancel( struct cell *t, branch_bm_t *cancel_bm );
void cancel_uacs( struct cell *t, branch_bm_t cancel_bm );
void cancel_branch( struct cell *t, int branch );

int fifo_uac_cancel( FILE* stream, char *response_file );

int unixsock_uac_cancel(str* msg);

unsigned int t_uac_cancel(str *headers,str *body,
	unsigned int cancelledIdx,unsigned int cancelledLabel,
	transaction_cb cb, void* cbp);
typedef unsigned int (*tuaccancel_f)( str *headers,str *body,
	unsigned int cancelledIdx,unsigned int cancelledLabel,
	transaction_cb cb, void* cbp);

char *build_cancel(struct cell *Trans,unsigned int branch,
	unsigned int *len );

inline short static should_cancel_branch( struct cell *t, int b )
{
	int last_received;
	short should;

	last_received=t->uac[b].last_received;
	/* cancel only if provisional received and noone else
	   attempted to cancel yet */
	should=last_received>=100 && last_received<200
		&& t->uac[b].local_cancel.buffer.s==0;
	/* we'll cancel -- label it so that noone else
		(e.g. another 200 branch) will try to do the same */
	if (should) t->uac[b].local_cancel.buffer.s=BUSY_BUFFER;
	return should;
}


#endif
