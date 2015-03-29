/* 
 *  MyQtt: A high performance open source MQTT implementation
 *  Copyright (C) 2014 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307 USA
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is LGPL software: you are welcome to develop
 *  proprietary applications using this library without any royalty or
 *  fee but returning back any change, improvement or addition in the
 *  form of source code, project image, documentation patches, etc.
 *
 *  For commercial support on build MQTT enabled solutions contact us:
 *          
 *      Postal address:
 *         Advanced Software Production Line, S.L.
 *         C/ Antonio Suarez Nº 10, 
 *         Edificio Alius A, Despacho 102
 *         Alcalá de Henares 28802 (Madrid)
 *         Spain
 *
 *      Email address:
 *         info@aspl.es - http://www.aspl.es/mqtt
 *                        http://www.aspl.es/myqtt
 */

#include <myqttd.h>

/* these includes are prohibited in a production ready code. They are
   just used to check various internal aspects of the MyQtt
   implementation. If you need your code to use them to work keep in
   mind you are referencing internal structures and definition that
   may change at any time */
#include <myqttd-ctx-private.h>
#include <myqtt-ctx-private.h>

#ifdef AXL_OS_UNIX
#include <signal.h>
#endif

axl_bool test_common_enable_debug = axl_false;

/* default listener location */
const char * listener_host = "localhost";
const char * listener_port = "1883";

MyQttCtx * init_ctx (void)
{
	MyQttCtx * ctx;

	/* call to init the base library and close it */
	ctx = myqtt_ctx_new ();

	/* enable log if requested by the user */
	if (test_common_enable_debug) {
		myqtt_log_enable (ctx, axl_true);
		myqtt_color_log_enable (ctx, axl_true);
		myqtt_log2_enable (ctx, axl_true);
	} /* end if */

	return ctx;
}

MyQttdCtx * init_ctxd (MyQttCtx * myqtt_ctx, const char * config)
{
	MyQttdCtx * ctx;

	/* call to init the base library and close it */
	ctx = myqttd_ctx_new ();

	/* enable log if requested by the user */
	if (test_common_enable_debug) {
		myqttd_log_enable (ctx, axl_true);
		myqttd_color_log_enable (ctx, axl_true);
		myqttd_log2_enable (ctx, axl_true);
	} /* end if */

	if (! myqttd_init (ctx, myqtt_ctx, config)) {
		printf ("Error: unable to initialize MyQtt library..\n");
		return NULL;
	} /* end if */

	/* enable log if requested by the user */
	if (test_common_enable_debug) {
		myqtt_log_enable (MYQTTD_MYQTT_CTX (ctx), axl_true);
		myqtt_color_log_enable (MYQTTD_MYQTT_CTX (ctx), axl_true);
		myqtt_log2_enable (MYQTTD_MYQTT_CTX (ctx), axl_true);
	} /* end if */

	/* now run config */
	if (! myqttd_run_config (ctx)) {
		printf ("Failed to run current config, finishing process: %d", getpid ());
		return NULL;
	} /* end if */

	return ctx;
}

axl_bool  test_00 (void) {

	MyQttdCtx * ctx;
	MyQttCtx  * myqtt_ctx;
	
	/* call to init the base library and close it */
	printf ("Test 00: init library and server engine..\n");
	myqtt_ctx = init_ctx ();
	ctx       = init_ctxd (myqtt_ctx, "myqtt.example.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 00: library and server engine started.. ok\n");

	/* now close the library */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}

void queue_message_received (MyQttCtx * ctx, MyQttConn * conn, MyQttMsg * msg, axlPointer user_data)
{
	MyQttAsyncQueue * queue = user_data;

	/* push message received */
	printf ("Test --: queue received %p, msg=%p, msg-id=%d\n", queue, msg, myqtt_msg_get_id (msg));
	myqtt_msg_ref (msg);
	myqtt_async_queue_push (queue, msg);
	return;
} 

void queue_message_received_only_one (MyQttCtx * ctx, MyQttConn * conn, MyQttMsg * msg, axlPointer user_data)
{
	MyQttAsyncQueue * queue = user_data;

	/* push message received */
	myqtt_msg_ref (msg);
	myqtt_async_queue_push (queue, msg);
	return;
} 

axl_bool  test_01 (void) {
	MyQttdCtx       * ctx;
	MyQttConn       * conn;
	MyQttCtx        * myqtt_ctx;
	int               sub_result;
	MyQttAsyncQueue * queue;
	MyQttMsg        * msg;
	
	/* call to init the base library and close it */
	printf ("Test 01: init library and server engine..\n");
	ctx       = init_ctxd (NULL, "test_01.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 01: library and server engine started.. ok (ctxd = %p, ctx = %p\n", ctx, MYQTTD_MYQTT_CTX (ctx));

	/* create connection to local server and test domain support */
	myqtt_ctx = init_ctx ();
	if (! myqtt_init_ctx (myqtt_ctx)) {
		printf ("Error: unable to initialize MyQtt library..\n");
		return axl_false;
	} /* end if */

	printf ("Test 01: connecting to myqtt server (client ctx = %p)..\n", myqtt_ctx);
	conn = myqtt_conn_new (myqtt_ctx, "test_01", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
	if (! myqtt_conn_is_ok (conn, axl_false)) {
		printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
		return axl_false;
	} /* end if */

	/* subscribe */
	printf ("Test 01: subscribe to the topic myqtt/test..\n");
	if (! myqtt_conn_sub (conn, 10, "myqtt/test", 0, &sub_result)) {
		printf ("ERROR: unable to subscribe, myqtt_conn_sub () failed, sub_result=%d", sub_result);
		return axl_false;
	} /* end if */	

	printf ("Test 01: subscription completed with qos=%d\n", sub_result);
	
	/* register on message handler */
	queue = myqtt_async_queue_new ();
	myqtt_conn_set_on_msg (conn, queue_message_received, queue);

	/* push a message */
	printf ("Test 01: publishing to the topic myqtt/test..\n");
	if (! myqtt_conn_pub (conn, "myqtt/test", "This is test message....", 24, MYQTT_QOS_0, axl_false, 0)) {
		printf ("ERROR: unable to publish message, myqtt_conn_pub() failed\n");
		return axl_false;
	} /* end if */

	/* receive it */
	printf ("Test 01: waiting for message myqtt/test (5 seconds)..\n");
	msg   = myqtt_async_queue_timedpop (queue, 5000000);
	myqtt_async_queue_unref (queue);
	if (msg == NULL) {
		printf ("ERROR: expected to find message from queue, but NULL was found..\n");
		return axl_false;
	} /* end if */

	/* check content */
	printf ("Test 01: received reply...checking data..\n");
	if (myqtt_msg_get_app_msg_size (msg) != 24) {
		printf ("ERROR: expected payload size of 24 but found %d\n", myqtt_msg_get_app_msg_size (msg));
		return axl_false;
	} /* end if */

	if (myqtt_msg_get_type (msg) != MYQTT_PUBLISH) {
		printf ("ERROR: expected to receive PUBLISH message but found: %s\n", myqtt_msg_get_type_str (msg));
		return axl_false;
	} /* end if */

	/* check content */
	if (! axl_cmp ((const char *) myqtt_msg_get_app_msg (msg), "This is test message....")) {
		printf ("ERROR: expected to find different content..\n");
		return axl_false;
	} /* end if */

	printf ("Test 01: everything is ok, finishing context and connection..\n");
	myqtt_msg_unref (msg);

	/* close connection */
	myqtt_conn_close (conn);
	myqtt_exit_ctx (myqtt_ctx, axl_true);

	printf ("Test 01: finishing MyQttdCtx..\n");

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}


axl_bool  test_02 (void) {
	MyQttdCtx       * ctx;
	MyQttCtx        * myqtt_ctx;
	int               sub_result;
	MyQttMsg        * msg;
	MyQttAsyncQueue * queue;
	MyQttConn       * conns[50];
	int               iterator;
	
	/* call to init the base library and close it */
	printf ("Test 02: init library and server engine..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 02: library and server engine started.. ok (ctxd = %p, ctx = %p\n", ctx, MYQTTD_MYQTT_CTX (ctx));

	/* create connection to local server and test domain support */
	myqtt_ctx = init_ctx ();
	if (! myqtt_init_ctx (myqtt_ctx)) {
		printf ("Error: unable to initialize MyQtt library..\n");
		return axl_false;
	} /* end if */


	printf ("Test 02: connecting to myqtt server (client ctx = %p, 50 connections)..\n", myqtt_ctx);
	iterator = 0;
	while (iterator < 50) {
		conns[iterator] = myqtt_conn_new (myqtt_ctx, "test_02", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
		if (! myqtt_conn_is_ok (conns[iterator], axl_false)) {
			printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
			return axl_false;
		} /* end if */

		/* next iterator */
		iterator++;
	} /* end while */

	/* register on message handler */
	queue = myqtt_async_queue_new ();

	/* subscribe */
	printf ("Test 02: subscribe to the topic myqtt/test (in 50 connections)..\n");
	iterator = 0;
	while (iterator < 50) {
		if (! myqtt_conn_sub (conns[iterator], 10, "myqtt/test", 0, &sub_result)) {
			printf ("ERROR: unable to subscribe, myqtt_conn_sub () failed, sub_result=%d", sub_result);
			return axl_false;
		} /* end if */	

		myqtt_conn_set_on_msg (conns[iterator], queue_message_received_only_one, queue);

		/* next iterator */
		iterator++;

	} /* end while */


	printf ("Test 02: publishing to the topic myqtt/test (on 50 connections)..\n");
	iterator = 0;
	while (iterator < 50) {

		/* push a message */
		if (! myqtt_conn_pub (conns[iterator], "myqtt/test", "This is test message....", 24, MYQTT_QOS_0, axl_false, 0)) {
			printf ("ERROR: unable to publish message, myqtt_conn_pub() failed\n");
			return axl_false;
		} /* end if */

		/* next iterator */
		iterator++;
	}

	printf ("Test 02: wait for 2500 messages (50 messages for each connection)\n");
	iterator = 0;
	while (iterator < 2500) {

		/* receive it */
		msg = myqtt_async_queue_timedpop (queue, 5000000);
		if (msg == NULL) {
			printf ("ERROR: expected to find message from queue, but NULL was found..\n");
			return axl_false;
		} /* end if */
		
		/* check content */
		if (myqtt_msg_get_app_msg_size (msg) != 24) {
			printf ("ERROR: expected payload size of 24 but found %d\n", myqtt_msg_get_app_msg_size (msg));
			return axl_false;
		} /* end if */
		
		if (myqtt_msg_get_type (msg) != MYQTT_PUBLISH) {
			printf ("ERROR: expected to receive PUBLISH message but found: %s\n", myqtt_msg_get_type_str (msg));
			return axl_false;
		} /* end if */
		
		/* check content */
		if (! axl_cmp ((const char *) myqtt_msg_get_app_msg (msg), "This is test message....")) {
			printf ("ERROR: expected to find different content..\n");
			return axl_false;
		} /* end if */
		
		myqtt_msg_unref (msg);

		/* next iterator */
		iterator++;
	}

	printf ("Test 02: everything is ok, finishing context and connection (iterator=%d)..\n", iterator);

	iterator = 0;
	while (iterator < 50) {

		/* close connection */
		myqtt_conn_close (conns[iterator]);

		/* next iterator */
		iterator++;
	}

	/* receive it */
	printf ("Test 02: releasing queue %p\n", queue);
	myqtt_async_queue_unref (queue);

	myqtt_exit_ctx (myqtt_ctx, axl_true);

	printf ("Test 02: finishing MyQttdCtx..\n");

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}

axl_bool connect_send_and_check (MyQttCtx   * myqtt_ctx, 
				 const char * client_id, const char * user, const char * password,
				 const char * topic,     const char * message, 
				 const char * check_reply,
				 MyQttQos qos, 
				 axl_bool skip_error_reporting)
{

	MyQttAsyncQueue * queue;
	MyQttConn       * conn;
	MyQttMsg        * msg;
	int               sub_result;
	MyQttConnOpts   * opts = NULL;

	/* create connection to local server and test domain support */
	if (myqtt_ctx == NULL) {
		myqtt_ctx = init_ctx ();
		if (! myqtt_init_ctx (myqtt_ctx)) {
			if (! skip_error_reporting)
				printf ("Error: unable to initialize MyQtt library..\n");
			return axl_false;
		} /* end if */
	} /* end if */

	printf ("Test --: connecting to myqtt server (client ctx = %p)..\n", myqtt_ctx);

	/* configure user and password */
	if (user && password) {
		opts = myqtt_conn_opts_new ();
		myqtt_conn_opts_set_auth (opts, user, password);
	} /* end if */
	
	conn = myqtt_conn_new (myqtt_ctx, client_id, axl_true, 30, listener_host, listener_port, opts, NULL, NULL);
	if (! myqtt_conn_is_ok (conn, axl_false)) {
		if (! skip_error_reporting)
			printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);

		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	/* subscribe */
	printf ("Test --: subscribe to the topic %s..\n", topic);
	if (! myqtt_conn_sub (conn, 10, topic, 0, &sub_result)) {
		if (! skip_error_reporting)
			printf ("ERROR: unable to subscribe, myqtt_conn_sub () failed, sub_result=%d", sub_result);
		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */	

	printf ("Test --: subscription completed with qos=%d (requested=%d)\n", sub_result, qos);
	
	/* register on message handler */
	queue = myqtt_async_queue_new ();
	myqtt_conn_set_on_msg (conn, queue_message_received, queue);

	/* push a message */
	printf ("Test --: publishing to the topic %s..\n", topic);
	if (! myqtt_conn_pub (conn, topic, (const axlPointer) message, strlen (message), qos, axl_false, 0)) {
		if (! skip_error_reporting)
			printf ("ERROR: unable to publish message, myqtt_conn_pub() failed\n");

		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	/* receive it */
	printf ("Test --: waiting for message %s (5 seconds)..\n", topic);
	msg   = myqtt_async_queue_timedpop (queue, 5000000);
	myqtt_async_queue_unref (queue);
	if (msg == NULL) {
		if (! skip_error_reporting)
			printf ("ERROR: expected to find message from queue, but NULL was found..\n");

		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	/* check content */
	printf ("Test --: received reply...checking data..\n");
	if (myqtt_msg_get_app_msg_size (msg) != strlen (check_reply ? check_reply : message)) {
		if (! skip_error_reporting)
			printf ("ERROR: expected payload size of %d but found %d\nContent found: %s\n", 
				(int) strlen (check_reply ? check_reply : message),
				myqtt_msg_get_app_msg_size (msg),
				(const char *) myqtt_msg_get_app_msg (msg));

		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	if (myqtt_msg_get_type (msg) != MYQTT_PUBLISH) {
		if (! skip_error_reporting)
			printf ("ERROR: expected to receive PUBLISH message but found: %s\n", myqtt_msg_get_type_str (msg));

		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	/* check content */
	if (! axl_cmp ((const char *) myqtt_msg_get_app_msg (msg), check_reply ? check_reply : message)) {
		if (! skip_error_reporting)
			printf ("ERROR: expected to find different content..\n");

		/* close connection */
		myqtt_conn_close (conn);
		myqtt_exit_ctx (myqtt_ctx, axl_true);

		return axl_false;
	} /* end if */

	printf ("Test --: everything is ok, finishing context and connection..\n");
	myqtt_msg_unref (msg);

	/* close connection */
	myqtt_conn_close (conn);
	myqtt_exit_ctx (myqtt_ctx, axl_true);

	return axl_true;
	
}

MyQttConn * connect_and_subscribe (MyQttCtx * myqtt_ctx, const char * client_id, 
				   const char * topic, 
				   MyQttQos qos, axl_bool skip_error_reporting)
{
	MyQttConn       * conn;
	int               sub_result;

	/* create connection to local server and test domain support */
	if (myqtt_ctx == NULL) {
		myqtt_ctx = init_ctx ();
		if (! myqtt_init_ctx (myqtt_ctx)) {
			if (! skip_error_reporting)
				printf ("Error: unable to initialize MyQtt library..\n");
			return axl_false;
		} /* end if */
	} /* end if */

	printf ("Test --: connecting to myqtt server (client ctx = %p)..\n", myqtt_ctx);
	
	conn = myqtt_conn_new (myqtt_ctx, client_id, axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
	if (! myqtt_conn_is_ok (conn, axl_false)) {
		if (! skip_error_reporting)
			printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
		return axl_false;
	} /* end if */

	/* subscribe */
	printf ("Test --: subscribe to the topic %s..\n", topic);
	if (! myqtt_conn_sub (conn, 10, topic, 0, &sub_result)) {
		if (! skip_error_reporting)
			printf ("ERROR: unable to subscribe, myqtt_conn_sub () failed, sub_result=%d", sub_result);
		return axl_false;
	} /* end if */	

	printf ("Test --: subscription completed with qos=%d (requested=%d)\n", sub_result, qos);
	return conn;
}

void close_conn_and_ctx (MyQttConn * conn)
{
	MyQttCtx * ctx = myqtt_conn_get_ctx (conn);
	
	printf ("Test --: releasing connection (%p)\n", conn);
	myqtt_conn_close (conn);

	/* now close the library */
	printf ("Test --: releasing context (%p)\n", ctx);
	myqtt_exit_ctx (ctx, axl_true);

	return;
}

axl_bool send_msg (MyQttConn * conn, const char * topic, const char * message, MyQttQos qos) {
	/* publish message */
	return myqtt_conn_pub (conn, topic, (const axlPointer) message, strlen (message), qos, axl_false, 0);
}

void configure_reception_queue_received (MyQttCtx * ctx, MyQttConn * conn, MyQttMsg * msg, axlPointer user_data)
{
	MyQttAsyncQueue * queue = user_data;

	/* push message received */
	myqtt_msg_ref (msg);
	myqtt_async_queue_push (queue, msg);
	return;
}

MyQttAsyncQueue * configure_reception (MyQttConn * conn) {
	MyQttAsyncQueue * queue = myqtt_async_queue_new ();

	/* configure reception on queue  */
	myqtt_conn_set_on_msg (conn, configure_reception_queue_received, queue);

	return queue;
}

axl_bool receive_and_check (MyQttAsyncQueue * queue, const char * topic, const char * message, MyQttQos qos, axl_bool skip_fail_if_null)
{
	MyQttMsg * msg;

	/* get message */
	msg   = myqtt_async_queue_timedpop (queue, 3000000);
	if (msg == NULL) {
		if (skip_fail_if_null)
			return axl_true;
		return axl_false;
	} /* end if */

	if (! axl_cmp (message, (const char *) myqtt_msg_get_app_msg (msg))) {
		printf ("Test --: message content mismatch received..\n");
		myqtt_msg_unref (msg);
		return axl_false;
	}

	if (! axl_cmp (topic, (const char *) myqtt_msg_get_topic (msg))) {
		printf ("Test --: message topic mismatch received..\n");
		myqtt_msg_unref (msg);
		return axl_false;
	}

	if (qos != myqtt_msg_get_qos (msg)) {
		printf ("Test --: message qos mismatch received..\n");
		myqtt_msg_unref (msg);
		return axl_false;
	}

	
	myqtt_msg_unref (msg);
	return axl_true;
	
}

axl_bool  test_03 (void) {
	MyQttdCtx       * ctx;

	MyQttConn       * conn;
	MyQttConn       * conn2;

	MyQttAsyncQueue * queue;
	MyQttAsyncQueue * queue2;
	
	/* call to init the base library and close it */
	printf ("Test 03: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 03: library and server engine started.. ok (ctxd = %p, ctx = %p\n", ctx, MYQTTD_MYQTT_CTX (ctx));

	if (myqttd_domain_count_enabled (ctx) != 0) {
		printf ("Test 03: expected to find 0 domains enabled but found %d\n", myqttd_domain_count_enabled (ctx));
		return axl_false;
	} /* end if */

	/* connect and send message */
	if (! connect_send_and_check (NULL, "test_02", NULL, NULL, "myqtt/test", "This is test message....", NULL, MYQTT_QOS_0, axl_false)) {
		printf ("Test 03: unable to connect and send message...\n");
		return axl_false;
	} /* end if */

	if (myqttd_domain_count_enabled (ctx) != 1) {
		printf ("Test 03: expected to find 1 domains enabled but found %d\n", myqttd_domain_count_enabled (ctx));
		return axl_false;
	} /* end if */

	/* connect and send message */
	if (! connect_send_and_check (NULL, "test_04", NULL, NULL, "myqtt/test", "This is test message....", NULL, MYQTT_QOS_0, axl_false)) {
		printf ("Test 03: unable to connect and send message...\n");
		return axl_false;
	} /* end if */

	if (myqttd_domain_count_enabled (ctx) != 2) {
		printf ("Test 03: expected to find 2 domains enabled but found %d\n", myqttd_domain_count_enabled (ctx));
		return axl_false;
	} /* end if */


	printf ("Test 03: checked domain activation for both services...Ok\n");

	/* connect and send message */
	if (connect_send_and_check (NULL, "test_05", NULL, NULL, "myqtt/test", "This is test message....", NULL, MYQTT_QOS_0, axl_true)) {
		printf ("Test 03: it should fail to connect but it connected..");
		return axl_false;
	} /* end if */

	/* ensure connections in domain */
	if (myqttd_domain_conn_count (myqttd_domain_find_by_name (ctx, "test_01.context")) != 0) {
		printf ("Test 03: expected to find 0 connection but found something different..\n");
		return axl_false;
	}

	/* connect and report connection */
	conn = connect_and_subscribe (NULL, "test_02", "myqtt/test", MYQTT_QOS_0, axl_false);
	if (conn == NULL) {
		printf ("Test 03: unable to connect to the domain..\n");
		return axl_false;
	}

	/* ensure connections in domain */
	if (myqttd_domain_conn_count (myqttd_domain_find_by_name (ctx, "test_01.context")) != 1) {
		printf ("Test 03: expected to find 1 connection but found something different..\n");
		return axl_false;
	}

	/* ensure connections in domain */
	if (myqttd_domain_conn_count (myqttd_domain_find_by_name (ctx, "test_02.context")) != 0) {
		printf ("Test 03: expected to find 0 connection but found something different..\n");
		return axl_false;
	}

	/* connect and report connection */
	conn2 = connect_and_subscribe (NULL, "test_04", "myqtt/test", MYQTT_QOS_0, axl_false);
	if (conn == NULL) {
		printf ("Test 03: unable to connect to the domain..\n");
		return axl_false;
	}

	/* ensure connections in domain */
	if (myqttd_domain_conn_count (myqttd_domain_find_by_name (ctx, "test_02.context")) != 1) {
		printf ("Test 03: expected to find 1 connection but found something different..\n");
		return axl_false;
	}
	

	/* configure message reception */
	queue  = configure_reception (conn);
	queue2 = configure_reception (conn2);

	/* send message for conn and check for reply */
	if (! send_msg (conn, "myqtt/test", "This is an application message", MYQTT_QOS_0)) {
		printf ("Test 03: unable to send message\n");
		return axl_false;
	}
	if (! send_msg (conn2, "myqtt/test", "This is an application message (2)", MYQTT_QOS_0)) {
		printf ("Test 03: unable to send message\n");
		return axl_false;
	}

	/* now receive message */
	printf ("Test 03: checking for messages received..\n");
	if (! receive_and_check (queue, "myqtt/test", "This is an application message", MYQTT_QOS_0, axl_false)) {
		printf ("Test 03: expected to receive different message..\n");
		return axl_false;
	}

	if (! receive_and_check (queue2, "myqtt/test", "This is an application message (2)", MYQTT_QOS_0, axl_false)) {
		printf ("Test 03: expected to receive different message..\n");
		return axl_false;
	}

	printf ("Test 03: now check if we don't receive messages from different domains (4 seconds waiting)..\n");
	if (myqtt_async_queue_timedpop (queue, 2000000)) {
		printf ("Test 03: expected to not receive any message over this connection..\n");
		return axl_false;
	}
	if (myqtt_async_queue_timedpop (queue2, 2000000)) {
		printf ("Test 03: expected to not receive any message over this connection..\n");
		return axl_false;
	}

	/* release queue */
	myqtt_async_queue_unref (queue);
	myqtt_async_queue_unref (queue2);


	/* close connection and context */
	close_conn_and_ctx (conn);
	close_conn_and_ctx (conn2);


	printf ("Test --: finishing MyQttdCtx..\n");
	

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}

MyQttPublishCodes test_04_handle_publish (MyQttdCtx * ctx,       MyQttdDomain * domain,  
					  MyQttCtx  * myqtt_ctx, MyQttConn    * conn, 
					  MyQttMsg  * msg,       axlPointer user_data)
{
	printf ("Test --: received message on server (topic: %s)\n", myqtt_msg_get_topic (msg));
	if (axl_cmp ("get-context", myqtt_msg_get_topic (msg))) {
		printf ("Test --: publish received on domain: %s\n", domain->name);

		if (! myqtt_conn_pub (conn, "get-context", domain->name, (int) strlen (domain->name), MYQTT_QOS_0, axl_false, 0))
			printf ("ERROR: failed to publish message in reply..\n");
		return MYQTT_PUBLISH_DISCARD;
	}

	return MYQTT_PUBLISH_OK;
}

axl_bool  test_04 (void) {

	MyQttdCtx       * ctx;

	/* call to init the base library and close it */
	printf ("Test 04: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 04: library and server engine started.. ok (ctxd = %p, ctx = %p\n", ctx, MYQTTD_MYQTT_CTX (ctx));

	/* configure the on publish */
	myqttd_ctx_add_on_publish (ctx, test_04_handle_publish, NULL);

	/* connect and send message */
	printf ("Test 04: checking context activation based or user/password/clientid (test_01.context)\n");
	if (! connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_02", "user-test-02", "test1234", 
				      "get-context", "get-context", "test_01.context", MYQTT_QOS_0, axl_false)) {
		printf ("Test --: unable to connect and send message...\n");
		return axl_false;
	} /* end if */

	/* connect and send message */
	printf ("Test 04: checking context activation based or user/password/clientid (test_02.context)\n");
	if (! connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_02", "user-test-02", "differentpass", 
				      "get-context", "get-context", "test_02.context", MYQTT_QOS_0, axl_false)) {
		printf ("Test --: unable to connect and send message...\n");
		return axl_false;
	} /* end if */
	
	printf ("Test --: finishing MyQttdCtx..\n");
	

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}


axl_bool  test_05 (void) {

	MyQttdCtx       * ctx;

	/* call to init the base library and close it */
	printf ("Test 05: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 05: library and server engine started.. ok (ctxd = %p, ctx = %p\n", ctx, MYQTTD_MYQTT_CTX (ctx));

	/* connect and send message */
	printf ("Test 05: checking context is not activated based or user/password/clientid (test_01.context)\n");
	if (connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_02", "user-test-02", "test1234-5", 
				      "get-context", "get-context", "test_01.context", MYQTT_QOS_0, axl_true)) {
		printf ("ERROR: it shouldn't work it does...\n");
		return axl_false;
	} /* end if */

	/* connect and send message */
	printf ("Test 05: checking context is not activated based or user/password/clientid (test_02.context)\n");
	if (connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_02", "user-test-02", "differentpass-5", 
				      "get-context", "get-context", "test_02.context", MYQTT_QOS_0, axl_true)) {
		printf ("ERROR: it shouldn't work it does...\n");
		return axl_false;
	} /* end if */
	
	printf ("Test --: finishing MyQttdCtx..\n");
	
	if (myqttd_domain_count_enabled (ctx) != 0) {
		printf ("Test --: expected to find 0 domains enabled but found %d\n", myqttd_domain_count_enabled (ctx));
		return axl_false;
	} /* end if */

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
		
	return axl_true;
}

axl_bool  test_06 (void) {

	MyQttdCtx           * ctx;
	MyQttdDomainSetting * setting;

	/* call to init the base library and close it */
	printf ("Test 06: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test --: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	printf ("Test 06: checking loaded settings..\n");
	if (ctx->default_setting == NULL) {
		printf ("Test --: default settings wheren't loaded..\n");
		return axl_false;
	} /* end if */

	if (! ctx->default_setting->require_auth) {
		printf ("Test --: default settings wheren't loaded for require auth..\n");
		return axl_false;
	} /* end if */

	if (! ctx->default_setting->restrict_ids) {
		printf ("Test --: default settings wheren't loaded for restrict ids..\n");
		return axl_false;
	} /* end if */

	/* now get settings by name */
	setting = myqtt_hash_lookup (ctx->domain_settings, "basic");
	if (setting == NULL) {
		printf ("ERROR: we should've found setting but NULL was found..\n"); 
		return axl_false;
	}

	printf ("Test --: check basic settings\n");
	if (setting->conn_limit != 5 ||
	    setting->message_size_limit != 256 ||
	    setting->storage_messages_limit != 10000 ||
	    setting->storage_quota_limit != 102400) {
		printf ("ERROR: expected different values (basic)...\n");
		return axl_false;
	}

	/* now get settings by name */
	setting = myqtt_hash_lookup (ctx->domain_settings, "standard");
	if (setting == NULL) {
		printf ("ERROR: we should've found setting but NULL was found..\n"); 
		return axl_false;
	}

	printf ("Test --: check standard settings\n");
	if (setting->conn_limit != 10 ||
	    setting->message_size_limit != 65536 ||
	    setting->storage_messages_limit != 20000 ||
	    setting->storage_quota_limit != 204800) {
		printf ("ERROR: expected different values (standard)...\n");
		return axl_false;
	} /* end if */

	printf ("Test --: settings ok\n");

	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);

	return axl_true;
}

MyQttPublishCodes test_07_handle_publish (MyQttdCtx * ctx,       MyQttdDomain * domain,  
					  MyQttCtx  * myqtt_ctx, MyQttConn    * conn, 
					  MyQttMsg  * msg,       axlPointer user_data)
{
	char * result;

	printf ("Test --: received message on server (topic: %s)\n", myqtt_msg_get_topic (msg));
	if (axl_cmp ("get-connections", myqtt_msg_get_topic (msg))) {
		printf ("Test --: publish received on domain: %s\n", domain->name);

		/* get current connections */
		result = axl_strdup_printf ("%d", axl_list_length (myqtt_ctx->conn_list) + axl_list_length (myqtt_ctx->srv_list));

		printf ("Test --: current connections are: %s\n", result);

		if (! myqtt_conn_pub (conn, "get-connections", result, (int) strlen (result), MYQTT_QOS_0, axl_false, 0))
			printf ("ERROR: failed to publish message in reply..\n");
		axl_free (result);
		return MYQTT_PUBLISH_DISCARD;
	}

	return MYQTT_PUBLISH_OK;
}


axl_bool  test_07 (void) {
	
	MyQttdCtx       * ctx;
	MyQttCtx        * myqtt_ctx;
	MyQttConn       * conns[50];
	int               iterator;

	/* call to init the base library and close it */
	printf ("Test 07: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	myqtt_ctx = init_ctx ();
	if (! myqtt_init_ctx (myqtt_ctx)) {
		printf ("Error: unable to initialize MyQtt library..\n");
		return axl_false;
	} /* end if */
	
	/* connect to test_01.context and create more than 5 connections */
	printf ("Test 07: creating five connections..\n");
	iterator = 0;
	while (iterator < 5) {
		conns[iterator] = myqtt_conn_new (myqtt_ctx, "test_02", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
		if (! myqtt_conn_is_ok (conns[iterator], axl_false)) {
			printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
			return axl_false;
		} /* end if */

		/* next iterator */
		iterator++;
	} /* end while */

	/* check connections */
	myqttd_ctx_add_on_publish (ctx, test_07_handle_publish, NULL);

	/* connect and send message */
	printf ("Test 07: requesting number of connections remotely..\n");
	if (! connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_02", "user-test-02", "test1234", 
				      /* we've got 6 connections because we have 5 plus the
					 connection that is being requesting the get-connections */
				      "get-connections", "", "6", MYQTT_QOS_0, axl_false)) {
		printf ("Test --: unable to connect and send message...\n");
		return axl_false;
	} /* end if */
	
	/* check limits */
	conns[iterator] = myqtt_conn_new (myqtt_ctx, "test_02", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
	if (myqtt_conn_is_ok (conns[iterator], axl_false)) {
		printf ("ERROR: it worked and it shouldn't...connect to %s:%s..\n", listener_host, listener_port);
		return axl_false;
	} /* end if */
	myqtt_conn_close (conns[iterator]);

	iterator = 0;
	while (iterator < 5) {
		/* close connection */
		myqtt_conn_close (conns[iterator]);
		/* next iterator */
		iterator++;
	} /* end while */

	/* connect to test_01.context and create more than 5 connections */
	printf ("Test 07: creating 10 connections (standard sensting)..\n");
	iterator = 0;
	while (iterator < 10) {
		conns[iterator] = myqtt_conn_new (myqtt_ctx, "test_04", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
		if (! myqtt_conn_is_ok (conns[iterator], axl_false)) {
			printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
			return axl_false;
		} /* end if */

		/* next iterator */
		iterator++;
	} /* end while */

	/* connect and send message */
	printf ("Test 07: requesting number of connections remotely..\n");
	if (! connect_send_and_check (NULL, 
				      /* client id, user and password */
				      "test_04", NULL, NULL,
				      /* we've got 6 connections because we have 5 plus the
					 connection that is being requesting the get-connections */
				      "get-connections", "", "11", MYQTT_QOS_0, axl_false)) {
		printf ("Test --: unable to connect and send message...\n");
		return axl_false;
	} /* end if */

	/* check limits */
	conns[iterator] = myqtt_conn_new (myqtt_ctx, "test_04", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
	if (myqtt_conn_is_ok (conns[iterator], axl_false)) {
		printf ("ERROR: it worked and it shouldn't...connect to %s:%s..\n", listener_host, listener_port);
		return axl_false;
	} /* end if */
	myqtt_conn_close (conns[iterator]);

	iterator = 0;
	while (iterator < 10) {
		/* close connection */
		myqtt_conn_close (conns[iterator]);
		/* next iterator */
		iterator++;
	} /* end while */

	myqtt_exit_ctx (myqtt_ctx, axl_true);
	printf ("Test 07: finishing MyQttdCtx..\n");
	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
	
	
	return axl_true;
}

axl_bool  test_08 (void) {
	
	MyQttdCtx       * ctx;
	MyQttCtx        * myqtt_ctx;
	MyQttConn       * conn;
	MyQttAsyncQueue * queue;
	const char      * message;
	int               sub_result;

	/* call to init the base library and close it */
	printf ("Test 08: init library and server engine (using test_02.conf)..\n");
	ctx       = init_ctxd (NULL, "test_02.conf");
	if (ctx == NULL) {
		printf ("Test 00: failed to start library and server engine..\n");
		return axl_false;
	} /* end if */

	myqtt_ctx = init_ctx ();
	if (! myqtt_init_ctx (myqtt_ctx)) {
		printf ("Error: unable to initialize MyQtt library..\n");
		return axl_false;
	} /* end if */
	
	/* connect to test_01.context and create more than 5 connections */
	printf ("Test 08: attempting to send a message bigger than configured allowed sizes\n");
	conn = myqtt_conn_new (myqtt_ctx, "test_02", axl_true, 30, listener_host, listener_port, NULL, NULL, NULL);
	if (! myqtt_conn_is_ok (conn, axl_false)) {
		printf ("ERROR: unable to connect to %s:%s..\n", listener_host, listener_port);
		return axl_false;
	} /* end if */

	/* subscribe to the topic to get notifications */
	if (! myqtt_conn_sub (conn, 10, "myqtt/test", 0, &sub_result)) {
		printf ("ERROR: unable to subscribe, myqtt_conn_sub () failed, sub_result=%d", sub_result);
		return axl_false;
	} /* end if */	

	/* configure asyncqueue reception */
	queue  = configure_reception (conn);

	message = "ksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jyksljg0823io2j135lknmwegoij2346oi24jtoi4mg4ylk34jy";

	if (! myqtt_conn_pub (conn, "myqtt/test", (axlPointer) message, (int) strlen (message), MYQTT_QOS_0, axl_false, 0))
		printf ("ERROR: failed to publish message in reply..\n");

	printf ("Test --: we shouldn't receive a totification for this message (waiting 3 seconds)\n");
	if (myqtt_async_queue_timedpop (queue, 3000000)) {
		printf ("Test --: expected to not receive any message over this connection..\n");
		return axl_false;
	}

	/* close connection */
	myqtt_conn_close (conn);

	myqtt_async_queue_unref (queue);

	myqtt_exit_ctx (myqtt_ctx, axl_true);
	printf ("Test 08: finishing MyQttdCtx..\n");
	/* finish server */
	myqttd_exit (ctx, axl_true, axl_true);
	
	return axl_true;
}



#define CHECK_TEST(name) if (run_test_name == NULL || axl_cmp (run_test_name, name))

typedef axl_bool (* MyQttTestHandler) (void);

/** 
 * @brief Helper handler that allows to execute the function provided
 * with the message associated.
 * @param function The handler to be called (test)
 * @param message The message value.
 */
int run_test (MyQttTestHandler function, const char * message) {

	printf ("--- --: starting %s\n", message);

	if (function ()) {
		printf ("%s [   OK   ]\n", message);
	} else {
		printf ("%s [ FAILED ]\n", message);
		exit (-1);
	}
	return 0;
}


#ifdef AXL_OS_UNIX
void __block_test (int value) 
{
	MyQttAsyncQueue * queue;

	printf ("******\n");
	printf ("****** Received a signal (the regression test is failing): pid %d..locking..!!!\n", myqtt_getpid ());
	printf ("******\n");

	/* block the caller */
	queue = myqtt_async_queue_new ();
	myqtt_async_queue_pop (queue);

	return;
}
#endif


/** 
 * @brief General regression test to check all features inside myqtt
 */
int main (int argc, char ** argv)
{
	char * run_test_name = NULL;

	/* install default handling to get notification about
	 * segmentation faults */
#ifdef AXL_OS_UNIX
	signal (SIGSEGV, __block_test);
	signal (SIGABRT, __block_test);
#endif

	printf ("** MyQtt: A high performance open source MQTT implementation\n");
	printf ("** Copyright (C) 2015 Advanced Software Production Line, S.L.\n**\n");
	printf ("** Regression tests: %s \n",
		VERSION);
	printf ("** To gather information about time performance you can use:\n**\n");
	printf ("**     time ./test_01 [--help] [--debug] [--no-unmap] [--run-test=NAME]\n**\n");
	printf ("** To gather information about memory consumed (and leaks) use:\n**\n");
	printf ("**     >> libtool --mode=execute valgrind --leak-check=yes --show-reachable=yes --error-limit=no ./test_01 [--debug]\n**\n");
	printf ("** Providing --run-test=NAME will run only the provided regression test.\n");
	printf ("** Available tests: test_00, test_01, test_02, test_03, test_04\n");
	printf ("**\n");
	printf ("** Report bugs to:\n**\n");
	printf ("**     <myqtt@lists.aspl.es> MyQtt Mailing list\n**\n");

	/* uncomment the following four lines to get debug */
	while (argc > 0) {
		if (axl_cmp (argv[argc], "--help")) 
			exit (0);
		if (axl_cmp (argv[argc], "--debug")) 
			test_common_enable_debug = axl_true;
	
		if (argv[argc] && axl_memcmp (argv[argc], "--run-test", 10)) {
			run_test_name = argv[argc] + 11;
			printf ("INFO: running test: %s\n", run_test_name);
		}
		argc--;
	} /* end if */

	/* run tests */
	CHECK_TEST("test_00")
	run_test (test_00, "Test 00: basic context initialization");

	CHECK_TEST("test_01")
	run_test (test_01, "Test 01: basic domain initialization (selecting one domain based on connection settings)");

	CHECK_TEST("test_02")
	run_test (test_02, "Test 02: sending 50 messages with 50 connections subscribed (2500 messages received)");

	CHECK_TEST("test_03")
	run_test (test_03, "Test 03: domain selection (working with two domains based on client_id selection)");

	CHECK_TEST("test_04")
	run_test (test_04, "Test 04: domain selection (working with two domains based on user/password selection)");

	CHECK_TEST("test_05")
	run_test (test_05, "Test 05: domain selection (testing right users with wrong passwords)");

	CHECK_TEST("test_06")
	run_test (test_06, "Test 06: check loading domain settings");

	CHECK_TEST("test_07")
	run_test (test_07, "Test 07: testing connection limits to a domain");

	CHECK_TEST("test_08")
	run_test (test_08, "Test 08: test message size limit to a domain");

	/* test message global quota limit to a domain */

	/* test message limits to a domain (it must reset day by day,
	   month by month) */

	/* test message filtering (by topic and by message) (it contains, it is equal) */

	/* domain activation when connected with WebSocket/WebSocketTLS when
	   providing same hostname as domain name */

	/* domain activation when connected with TLS MQTT when
	   providing same hostname as domain name */

	/* test support for port sharing (running MQTT, MQTT-tls,
	   WebSocket-MQTT..etc over the same port) */

	printf ("All tests passed OK!\n");

	/* terminate */
	return 0;
	
} /* end main */
