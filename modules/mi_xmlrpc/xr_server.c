/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Open SIP Express Router (openser).
 *
 * openser is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-11-30  first version (lavinia)
 *  2007-02-02  support for asyncronous reply added (bogdan)
 */


#include "../../str.h"
#include "../../dprint.h"
#include "../../sr_module.h"
#include "../../mi/mi.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../../locking.h"
#include "../../ut.h"
#include "xr_writer.h"
#include "xr_parser.h"
#include "mi_xmlrpc.h"
#include "xr_server.h"
#include <xmlrpc_abyss.h>

gen_lock_t *xr_lock;

#define XMLRPC_ASYNC_FAILED   ((void*)-2)
#define XMLRPC_ASYNC_EXPIRED  ((void*)-3)




static inline void free_async_handler( struct mi_handler *hdl )
{
	if (hdl)
		shm_free(hdl);
}



static void xmlrpc_close_async( struct mi_root *mi_rpl, struct mi_handler *hdl,
																	int done)
{
	struct mi_root *shm_rpl;
	int x;

	if (!done) {
		/* we do not pass provisional stuff (yet) */
		if (mi_rpl)
			free_mi_tree( mi_rpl );
		return;
	}

	/* pass the tree via handler back to originating process */
	if ( mi_rpl==NULL || (shm_rpl=clone_mi_tree( mi_rpl, 1))==NULL )
		shm_rpl = XMLRPC_ASYNC_FAILED;
	if (mi_rpl)
		free_mi_tree(mi_rpl);

	lock_get(xr_lock);
	if (hdl->param==NULL) {
		hdl->param = shm_rpl;
		x = 0;
	} else {
		x = 1;
	}
	lock_release(xr_lock);

	if (x) {
		if (shm_rpl!=XMLRPC_ASYNC_FAILED)
			free_shm_mi_tree(shm_rpl);
		free_async_handler(hdl);
	}
}


#define MAX_XMLRPC_WAIT 2*60
static inline struct mi_root* wait_async_reply(struct mi_handler *hdl)
{
	struct mi_root *mi_rpl;
	int i;
	int x;

	for( i=0 ; i<MAX_XMLRPC_WAIT ; i++ ) {
		if (hdl->param)
			break;
		sleep_us(1000*500);
	}

	if (i==MAX_XMLRPC_WAIT) {
		/* no more waiting ....*/
		lock_get(xr_lock);
		if (hdl->param==NULL) {
			hdl->param = XMLRPC_ASYNC_EXPIRED;
			x = 0;
		} else {
			x = 1;
		}
		lock_release(xr_lock);
		if (x==0) {
			LOG(L_INFO,"INFO:mi_xmlrpc:wait_async_reply: exiting before "
				" receiving reply\n");
			return NULL;
		}
	}

	mi_rpl = (struct mi_root *)hdl->param;
	if (mi_rpl==XMLRPC_ASYNC_FAILED)
		mi_rpl = NULL;

	free_async_handler(hdl);
	return mi_rpl;
}



static inline struct mi_handler* build_async_handler()
{
	struct mi_handler *hdl;

	hdl = (struct mi_handler*)shm_malloc( sizeof(struct mi_handler) );
	if (hdl==0) {
		LOG(L_ERR,"ERROR:mi_xmlrpc:build_async_handler: no more shm mem\n");
		return 0;
	}

	hdl->handler_f = xmlrpc_close_async;
	hdl->param = 0;

	return hdl;
}



xmlrpc_value*  default_method	(xmlrpc_env* 	env, 
								char* 			host,
								char* 			methodName,
								xmlrpc_value* 	paramArray,
								void* 			serverInfo){

	xmlrpc_value* ret = NULL;
	struct mi_root* mi_cmd = NULL;
	struct mi_root* mi_rpl = NULL;
	struct mi_handler *hdl = NULL;
	struct mi_cmd* f;
	char* response = 0;
	int is_shm = 0;

	DBG("DEBUG: mi_xmlrpc: default_method: starting up.....\n");

	f = lookup_mi_cmd(methodName, strlen(methodName));
	
	if ( f == 0 ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Command %s is not "
			"available!\n", methodName);
		xmlrpc_env_set_fault_formatted(env, XMLRPC_NO_SUCH_METHOD_ERROR, 
			"Requested command (%s) is not available!", methodName);
		goto error;
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done looking the mi command.\n");

	/* if asyncron cmd, build the async handler */
	if (f->flags&MI_ASYNC_RPL_FLAG) {
		hdl = build_async_handler( );
		if (hdl==0) {
			LOG(L_ERR, "ERROR:mi_xmlrpc:default_method: failed to build "
				"async handler\n");
			if ( !env->fault_occurred )
				xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR,
					"Internal server error while processing request");
			goto error;
		}
	} else {
		hdl = NULL;
	}

	if (f->flags&MI_NO_INPUT_FLAG) {
		mi_cmd = 0;
	} else {
		mi_cmd = xr_parse_tree(env, paramArray);
		if ( mi_cmd == NULL ){
			LOG(L_ERR,"ERROR: mi_xmlrpc: default_method: error parsing"
				" MI tree\n");
			if ( !env->fault_occurred )
				xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR,
					"The xmlrpc request could not be parsed into a MI tree!");
			goto error;
		}
		mi_cmd->async_hdl = hdl;
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done parsing the mi tree.\n");

	if ( ( mi_rpl = run_mi_cmd(f, mi_cmd) ) == 0 ){
		LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Command (%s) processing "
			"failed.\n", methodName);
		xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR, 
			"Command (%s) processing failed.\n", methodName);
		goto error;
	} else if (mi_rpl==MI_ROOT_ASYNC_RPL) {
		mi_rpl = wait_async_reply(hdl);
		hdl = 0;
		if (mi_rpl==0) {
			xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR, 
				"Command (%s) processing failed (async).\n", methodName);
			goto error;
		}
		is_shm = 1;
	}

	DBG("DEBUG: mi_xmlrpc: default_method: Done running the mi command.\n");

	if ( rpl_opt == 1 ) {
		if ( xr_build_response_array( env, mi_rpl ) != 0 ){
			if ( !env->fault_occurred ) {
				LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Failed to parse "
					"the xmlrpc response from the mi tree.\n");
				xmlrpc_env_set_fault(env, XMLRPC_INTERNAL_ERROR, 
					"Failed to parse the xmlrpc response from the mi tree.");
				}
			goto error;
		}
		DBG("DEBUG:mi_xmlrpc:default_method: Done building response array.\n");

		ret = xr_response;
	} else {
		if ( (response = xr_build_response( env, mi_rpl )) == 0 ){
			if ( !env->fault_occurred ) {
				LOG(L_ERR, "ERROR: mi_xmlrpc: default_method: Failed to parse "
					"the xmlrpc response from the mi tree.\n");
				xmlrpc_env_set_fault_formatted(env, XMLRPC_INTERNAL_ERROR,
					"Failed to parse the xmlrpc response from the mi tree.");
			}
			goto error;
		}
		DBG("DEBUG: mi_xmlrpc: default_method: Done building response.\n");

		ret = xmlrpc_build_value(env, "s", response);
	}

error:
	free_async_handler(hdl);
	if ( mi_cmd ) free_mi_tree( mi_cmd );
	if ( mi_rpl ) { is_shm?free_shm_mi_tree(mi_rpl):free_mi_tree(mi_rpl);}
	return ret;
}


int set_default_method ( xmlrpc_env * env )
{
	xmlrpc_registry * registry;

	registry = xmlrpc_server_abyss_registry();
	xmlrpc_registry_set_default_method(env, registry, &default_method, NULL);

	if ( env->fault_occurred ) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: set_default_method: Failed to add "
			"default method: %s\n", env->fault_string);
		return -1;
	}

	return 0;
}

int init_async_lock()
{
	xr_lock = lock_alloc();
	if (xr_lock==NULL) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: set_default_method: Failed to create "
			"lock\n");
		return -1;
	}
	if (lock_init(xr_lock)==NULL) {
		LOG(L_ERR, "ERROR: mi_xmlrpc: set_default_method: Failed to init "
			"lock\n");
		return -1;
	}

	return 0;
}

void destroy_async_lock()
{
	if (xr_lock) {
		lock_destroy(xr_lock);
		lock_dealloc(xr_lock);
	}
}
