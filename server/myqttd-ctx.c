/* 
 *  MyQtt: A high performance open source MQTT implementation
 *  Copyright (C) 2016 Advanced Software Production Line, S.L.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation version 2.0 of the
 *  License
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 *  
 *  You may find a copy of the license under this software is released
 *  at COPYING file. This is GPL software 
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

/* local include */
#include <myqttd-ctx-private.h>

/** 
 * \defgroup myqttd_ctx MyQttd Context: API provided to handle MyQttd contexts
 */

/** 
 * \addtogroup myqttd_ctx
 * @{
 */

/** 
 * @brief Allows to create a new myqttd context (an object used by
 * the myqttd runtime to hold its current run time state). 
 *
 * The idea behind the myqttd initialization is to create a
 * context object and the call to \ref myqttd_init with that
 * context object to create a new run-time. This function also calls
 * to init the myqtt context associated. You can get it with \ref
 * myqttd_ctx_get_myqtt_ctx.
 *
 * Once required to finish the context created a call to \ref
 * myqttd_exit is required, followed by a call to \ref
 * myqttd_ctx_free.
 * 
 * @return A newly allocated reference to the MyQttd context
 * created. This function is already called by the myqttd engine.
 */
MyQttdCtx * myqttd_ctx_new ()
{
	MyQttdCtx   * ctx;
	struct timeval    tv;

	/* create the context */
	ctx        = axl_new (MyQttdCtx, 1);
	if (ctx == NULL)
		return NULL;

	/* get current stamp */
	gettimeofday (&tv, NULL);
	ctx->running_stamp = tv.tv_sec;

	/* create hash */
	ctx->data  = axl_hash_new (axl_hash_string, axl_hash_equal_string);
	myqtt_mutex_create (&ctx->data_mutex);

	/* set log descriptors to something not usable */
	ctx->general_log = -1;
	ctx->error_log   = -1;
	ctx->access_log  = -1;
	ctx->myqtt_log  = -1;

	/* init wait queue */
	ctx->wait_queue    = myqtt_async_queue_new ();

	/* init hash for auth backends */
	ctx->auth_backends = myqtt_hash_new (axl_hash_string, axl_hash_equal_string);

	/* init listener activators */
	ctx->listener_activators = myqtt_hash_new (axl_hash_string, axl_hash_equal_string);

	/* init on publish handlers and mutex associated */
	ctx->on_publish_handlers  = axl_list_new (axl_list_always_return_1, axl_free);

	/* return context created */
	return ctx;
}

/** 
 * @internal Allows to reinit internal state of the context process
 * after a child process creation. It also closed or removes internal
 * elements not required by child process.
 */
void           myqttd_ctx_reinit (MyQttdCtx * ctx, MyQttdChild * child)
{
	/* init pid to the child */
	ctx->pid = getpid ();

	/* define context child and child context */
	ctx->child   = child;
	child->ctx   = ctx;

	/* re-init mutex */
	myqtt_mutex_create (&ctx->exit_mutex);
	myqtt_mutex_create (&ctx->data_mutex);
	myqtt_mutex_create (&ctx->registered_modules_mutex);

	/* mutex on child object */
	myqtt_mutex_create (&ctx->child->mutex);

	/* clean child process list: reinit = axl_true */
	myqttd_process_init (ctx, axl_true);

	return;
}

/** 
 * @brief Allows to configure the myqtt context (MyQttCtx)
 * associated to the provided \ref MyQttdCtx.
 * 
 * @param ctx \ref MyQttdCtx to be configured.
 * @param myqtt_ctx MyQtt context to be configured.
 */
void            myqttd_ctx_set_myqtt_ctx (MyQttdCtx * ctx, 
					       MyQttCtx     * myqtt_ctx)
{
	v_return_if_fail (ctx);

	/* acquire a reference to the context */
	myqtt_ctx_ref (myqtt_ctx);
	
	/* configure myqtt ctx */
	ctx->myqtt_ctx = myqtt_ctx;

	/* configure reference on myqtt ctx */
	myqtt_ctx_set_data (myqtt_ctx, "myqttd:ctx", ctx);

	return;
}

/** 
 * @brief Allows to configure (add) a new onPublish handler that will
 * be called every time a publish operation is received.
 *
 * Handlers configured here (see \ref MyQttdOnPublish) will be able to
 * report any of the codes supported by \ref MyQttPublishCodes,
 * allowing to discard, accept or close the connection (for
 * example). See \ref MyQttPublishCodes for more reference.
 *
 * @param ctx The context where the onpublish handler is configured.
 *
 * @param on_publish The on publish handler to be configured.
 *
 * @param user_data A user defined pointer that will be passed in to
 * the handler.
 */
void            myqttd_ctx_add_on_publish (MyQttdCtx       * ctx, 
					   MyQttdOnPublish   on_publish, 
					   axlPointer        user_data)
{
	MyQttdOnPublishData * data;

	if (ctx == NULL || on_publish == NULL)
		return;

	/* add the reference */
	data = axl_new (MyQttdOnPublishData, 1);
	if (data == NULL)
		return;
	data->on_publish = on_publish;
	data->user_data  = user_data;

	/* add the handler */
	myqtt_mutex_lock (&ctx->data_mutex);
	axl_list_append (ctx->on_publish_handlers, data);
	myqtt_mutex_unlock (&ctx->data_mutex);

	return;
}

/** 
 * @brief Allows to register a new listener activator, a handler that
 * is called to startup a MQTT listener running a particular protocol
 * (or maybe user defined behaviour).
 *
 * @param ctx The context where the handler is being registered
 *
 * @param proto The protocol label that will identify this
 * handler. This protocol label will be used by system administrator
 * while configuring ports this myqttd server instance will listen
 * to. See /etc/myqtt/myqtt.conf (inside /global-settings/ports/port).
 *
 * @param listener_activator The listener activator that will be
 * called if the proto label matches.
 *
 * @param user_data User defined pointer that will be passed in into
 * the listener activator handler.
 *
 */
void            myqttd_ctx_add_listener_activator (MyQttdCtx               * ctx,
						   const char              * proto,
						   MyQttdListenerActivator   listener_activator,
						   axlPointer                user_data)
{
	MyQttdListenerActivatorData * data;

	if (ctx == NULL || proto == NULL || listener_activator == NULL)
		return;

	/* add the reference */
	data = axl_new (MyQttdListenerActivatorData, 1);
	if (data == NULL)
		return;
	data->listener_activator = listener_activator;
	data->user_data          = user_data;

	/* add listener activator */
	myqtt_hash_replace_full (ctx->listener_activators, axl_strdup(proto), axl_free, data, axl_free);

	return;
}

/** 
 * @brief Allows to get the \ref MyQttdCtx associated to the
 * myqtt.
 * 
 * @param ctx The myqttd context where it is expected to find a
 * MyQtt context (MyQttCtx).
 * 
 * @return A reference to the MyQttCtx.
 */
MyQttCtx     * myqttd_ctx_get_myqtt_ctx (MyQttdCtx * ctx)
{
	v_return_val_if_fail (ctx, NULL);

	/* return the myqtt context associated */
	return ctx->myqtt_ctx;
}

/** 
 * @brief Allows to configure user defined data indexed by the
 * provided key, associated to the \ref MyQttdCtx.
 * 
 * @param ctx The \ref MyQttdCtx to configure with the provided data.
 *
 * @param key The index string key under which the data will be
 * retreived later using \ref myqttd_ctx_get_data. The function do
 * not support storing NULL keys.
 *
 * @param data The user defined pointer to data to be stored. If NULL
 * is provided the function will understand it as a removal request,
 * calling to delete previously stored data indexed by the same key.
 */
void            myqttd_ctx_set_data       (MyQttdCtx * ctx,
					       const char    * key,
					       axlPointer      data)
{
	v_return_if_fail (ctx);
	v_return_if_fail (key);

	/* acquire the mutex */
	myqtt_mutex_lock (&ctx->data_mutex);

	/* perform a simple insert */
	axl_hash_insert (ctx->data, (axlPointer) key, data);

	/* release the mutex */
	myqtt_mutex_unlock (&ctx->data_mutex);

	return;
}


/** 
 * @brief Allows to configure user defined data indexed by the
 * provided key, associated to the \ref MyQttdCtx, with optionals
 * destroy handlers.
 *
 * This function is quite similar to \ref myqttd_ctx_set_data but
 * it also provides support to configure a set of handlers to be
 * called to terminate data associated once finished \ref
 * MyQttdCtx.
 * 
 * @param ctx The \ref MyQttdCtx to configure with the provided data.
 *
 * @param key The index string key under which the data will be
 * retreived later using \ref myqttd_ctx_get_data. The function do
 * not support storing NULL keys.
 *
 * @param data The user defined pointer to data to be stored. If NULL
 * is provided the function will understand it as a removal request,
 * calling to delete previously stored data indexed by the same key.
 *
 * @param key_destroy Optional handler to destroy key stored.
 *
 * @param data_destroy Optional handler to destroy value stored.
 */
void            myqttd_ctx_set_data_full  (MyQttdCtx * ctx,
					       const char    * key,
					       axlPointer      data,
					       axlDestroyFunc  key_destroy,
					       axlDestroyFunc  data_destroy)
{
	v_return_if_fail (ctx);
	v_return_if_fail (key);

	/* acquire the mutex */
	myqtt_mutex_lock (&ctx->data_mutex);

	/* perform a simple insert */
	axl_hash_insert_full (ctx->data, (axlPointer) key, key_destroy, data, data_destroy);

	/* release the mutex */
	myqtt_mutex_unlock (&ctx->data_mutex);

	return;
}

/** 
 * @brief Allows to retrieve data stored by \ref
 * myqttd_ctx_set_data and \ref myqttd_ctx_set_data_full.
 * 
 * @param ctx The \ref MyQttdCtx where the retrieve operation will
 * be performed.
 *
 * @param key The key that index the data to be returned.
 * 
 * @return A reference to the data or NULL if nothing was found. The
 * function also returns NULL if a NULL ctx is received.
 */
axlPointer      myqttd_ctx_get_data       (MyQttdCtx * ctx,
					       const char    * key)
{
	axlPointer data;
	v_return_val_if_fail (ctx, NULL);

	/* acquire the mutex */
	myqtt_mutex_lock (&ctx->data_mutex);

	/* perform a simple insert */
	data = axl_hash_get (ctx->data, (axlPointer) key);

	/* release the mutex */
	myqtt_mutex_unlock (&ctx->data_mutex);

	return data;
}

/** 
 * @brief Allows to implement a microseconds blocking wait.
 *
 * @param ctx The context where the wait will be implemented.
 *
 * @param microseconds Blocks the caller during the value
 * provided. 1.000.000 = 1 second.
 * 
 * If ctx is NULL or microseconds <= 0, the function returns
 * inmediately. 
 */
void            myqttd_ctx_wait           (MyQttdCtx * ctx,
					       long microseconds)
{
	if (ctx == NULL || microseconds <= 0) 
		return;
	/* acquire a reference */
	msg2 ("Process waiting during %d microseconds..", (int) microseconds);
	myqtt_async_queue_timedpop (ctx->wait_queue, microseconds);
	return;
}

/** 
 * @brief Allows to check if the provided myqttd ctx is associated to
 * a child process.
 *
 * @param ctx The context where the operation is taking place.
 *
 * This function can be used to check if the current execution context
 * is bound to a child process which means we are running in a child
 * process
 *
 * @return axl_false when contexts is representing master process
 * otherwise axl_true is returned (child process). Keep in mind the
 * function returns axl_false (master process) in the case or NULL
 * reference received.
 */
axl_bool        myqttd_ctx_is_child       (MyQttdCtx * ctx)
{
	if (ctx == NULL)
		return axl_false;
	return ctx->child != NULL;
}

/** 
 * @brief Deallocates the myqttd context provided.
 * 
 * @param ctx The context reference to terminate.
 */
void            myqttd_ctx_free (MyQttdCtx * ctx)
{
	/* do not perform any operation */
	if (ctx == NULL)
		return;

	/* terminate hash */
	axl_hash_free (ctx->data);
	ctx->data = NULL;
	myqtt_mutex_destroy (&ctx->data_mutex);

	/* release wait queue */
	myqtt_async_queue_unref (ctx->wait_queue);

	/* release hash holding auth backends */
	msg ("Releasing auth backends...");
	myqtt_hash_unref (ctx->auth_backends);

	/* include a error warning */
	if (ctx->myqtt_ctx) {
		if (myqtt_ctx_ref_count (ctx->myqtt_ctx) <= 0) 
			error ("ERROR: current process is attempting to release myqtt context more times than references supported");
		else {
			/* release myqtt reference acquired */
			msg ("Finishing MyQttCtx (%p, ref count: %d)", ctx->myqtt_ctx, myqtt_ctx_ref_count (ctx->myqtt_ctx));
			myqtt_ctx_unref (&(ctx->myqtt_ctx));
		}
	}

	/* now modules and myqtt library is stopped, terminate
	 * modules unloading them. This will allow having usable code
	 * mapped into modules address which is usable until the last
	 * time.  */
	myqttd_module_cleanup (ctx); 

	/* release publish handlers */
	axl_list_free (ctx->on_publish_handlers);

	/* release settings */
	axl_free (ctx->default_setting);
	myqtt_hash_destroy (ctx->domain_settings);
	myqtt_hash_destroy (ctx->listener_activators);

	/* release the node itself */
	msg ("Finishing MyQttdCtx (%p)", ctx);
	axl_free (ctx);

	return;
}

/* @} */
