/*
 * $Id$
 *
 * XMPP Module
 * This file is part of openser, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Author: Andreea Spirea
 *
 */

/*
 * An inbound SIP message:
 *   from sip:user1@domain1 to sip:user2*domain2@gateway_domain
 * is translated to an XMPP message:
 *   from user1*domain1@xmpp_domain to user2@domain2
 *
 * An inbound XMPP message:
 *   from user1@domain1 to user2*domain2@xmpp_domain
 * is translated to a SIP message:
 *   from sip:user1*domain1@gateway_domain to sip:user2@domain2
 *
 * Where '*' is the domain_separator, and gateway_domain and
 * xmpp_domain are defined below.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "../../sr_module.h"

#include "xmpp.h"
#include "network.h"
#include "xode.h"

struct xmpp_private_data {
	int fd;		/* socket */
	int running;
};

static int xode_send(int fd, xode x)
{
	char *str = xode_to_str(x);
	int len = strlen(str);
	
	DBG("xmpp: xode_send [%s]\n", str);

	if (net_send(fd, str, len) != len) {
		LOG(L_ERR, "xmpp: send() error: %s\n", strerror(errno));
		return -1;
	}
	return len;
}

static void stream_node_callback(int type, xode node, void *arg) 
{
	struct xmpp_private_data *priv = (struct xmpp_private_data *) arg;
	char *id, *hash, *tag;
	char buf[4096];
	xode x;

	DBG("xmpp: stream callback: %d: %s\n", type, node ? xode_get_name(node) : "n/a");
	switch (type) {
	case XODE_STREAM_ROOT:
		id = xode_get_attrib(node, "id");
		snprintf(buf, sizeof(buf), "%s%s", id, xmpp_password);
		hash = shahash(buf);
		
		x = xode_new_tag("handshake");
		xode_insert_cdata(x, hash, -1);
		xode_send(priv->fd, x);
		xode_free(x);
		break;
	case XODE_STREAM_NODE:
		tag = xode_get_name(node);

		if (!strcmp(tag, "handshake")) {
			DBG("xmpp: handshake succeeded\n");
		} else if (!strcmp(tag, "message")) {
			char *from = xode_get_attrib(node, "from");
			char *to = xode_get_attrib(node, "to");
			char *type = xode_get_attrib(node, "type");
			xode body = xode_get_tag(node, "body");
			char *msg;
			
			if (!type)
				type = "chat";
			if (!strcmp(type, "error")) {	
				DBG("xmpp: received message error stanza\n");
				goto out;
			}
			
			if (!from || !to || !body) {
				DBG("xmpp: invalid <message/> attributes\n");
				goto out;
			}

			if (!(msg = xode_get_data(body)))
				msg = "";
			xmpp_send_sip_msg(
				encode_jid_to_sip_uri(from),
				decode_jid_to_sip_uri(to),
				msg);
		}
		break;
	case XODE_STREAM_ERROR:
		LOG(L_ERR, "xmpp: stream error\n");
		/* fall-through */
	case XODE_STREAM_CLOSE:
		priv->running = 0;
		break;
	}
out:
	xode_free(node);
}

/**
 *
 */
static int do_send_message_component(struct xmpp_private_data *priv,
		struct xmpp_pipe_cmd *cmd)
{
	xode x;

	DBG("xmpp: do_send_message_component from=[%s] to=[%s] body=[%s]\n",
			cmd->from, cmd->to, cmd->body);

	x = xode_new_tag("message");
	xode_put_attrib(x, "id", cmd->id); // XXX
	xode_put_attrib(x, "from", encode_sip_uri_to_jid(cmd->from));
	xode_put_attrib(x, "to", decode_sip_uri_to_jid(cmd->to));
	xode_put_attrib(x, "type", "chat");
	xode_insert_cdata(xode_insert_tag(x, "body"), cmd->body, -1);
			
	xode_send(priv->fd, x);
	xode_free(x);

	/* missing error handling here ?!?!*/
	return 0;
}

static int do_send_bulk_message_component(struct xmpp_private_data *priv,
		struct xmpp_pipe_cmd *cmd)
{
	int len;

	DBG("xmpp: do_send_bulk_message_component from=[%s] to=[%s] body=[%s]\n",
			cmd->from, cmd->to, cmd->body);
	len = strlen(cmd->body);
	if (net_send(priv->fd, cmd->body, len) != len) {
		LOG(L_ERR, "xmpp: do_send_bulk_message_component: %s\n",
				strerror(errno));
		return -1;
	}
	return 0;
}


int xmpp_component_child_process(int data_pipe)
{
	int fd, maxfd, rv;
	fd_set fdset;
	xode_pool pool;
	xode_stream stream;
	struct xmpp_private_data priv;
	struct xmpp_pipe_cmd *cmd;
	
	while (1) {
		fd = net_connect(xmpp_host, xmpp_port);
		if (fd < 0) {
			sleep(3);
			continue;
		}
		
		priv.fd = fd;
		priv.running = 1;
		
		pool = xode_pool_new();
		stream = xode_stream_new(pool, stream_node_callback, &priv);
		
		net_printf(fd,
			"<?xml version='1.0'?>"
			"<stream:stream xmlns='jabber:component:accept' to='%s' "
			"version='1.0' xmlns:stream='http://etherx.jabber.org/streams'>",
			xmpp_domain);
		
		while (priv.running) {
			FD_ZERO(&fdset);
			FD_SET(data_pipe, &fdset);
			FD_SET(fd, &fdset);
			maxfd = fd > data_pipe ? fd : data_pipe;
			rv = select(maxfd + 1, &fdset, NULL, NULL, NULL);
			
			if (rv < 0) {
				LOG(L_ERR, "xmpp: select() error: %s\n", strerror(errno));
			} else if (!rv) {
				/* timeout */
			} else if (FD_ISSET(fd, &fdset)) {
				char *buf = net_read_static(fd);

				if (!buf)
					/* connection closed */
					break;

				DBG("xmpp: server read\n[%s]\n", buf);
				xode_stream_eat(stream, buf, strlen(buf));
			} else if (FD_ISSET(data_pipe, &fdset)) {
				if (read(data_pipe, &cmd, sizeof(cmd)) != sizeof(cmd)) {
					LOG(L_ERR, "xmpp: unable to read from command pipe: %s\n",
							strerror(errno));
				} else {
					DBG("xmpp: got pipe cmd %d\n", cmd->type);
					switch (cmd->type) {
					case XMPP_PIPE_SEND_MESSAGE:
						do_send_message_component(&priv, cmd);
						break;
					case XMPP_PIPE_SEND_PACKET:
					case XMPP_PIPE_SEND_PSUBSCRIBE:
					case XMPP_PIPE_SEND_PNOTIFY:
						do_send_bulk_message_component(&priv, cmd);
						break;
					}
					xmpp_free_pipe_cmd(cmd);
				}
			}
		}
		
		xode_pool_free(pool);
		
		close(fd);
	}
	return 0;
}

