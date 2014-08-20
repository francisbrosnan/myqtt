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
#include <myqtt-storage.h>
#include <myqtt-conn-private.h>
#include <myqtt-ctx-private.h>
#include <dirent.h>

int __myqtt_storage_check (MyQttCtx * ctx, MyQttConn * conn, axl_bool check_tp, const char * topic_filter)
{
	int topic_filter_len = 0;

	/* check 1 */
	if (ctx == NULL || conn == NULL || (check_tp && topic_filter == NULL)) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Wrong parameters received, some of them are null (ctx: %p, conn: %p, topic_filter: %p",
			   ctx, conn, topic_filter);
		return axl_false;
	}

	/* check 2 */
	if (conn->clean_session || conn->client_identifier == NULL || strlen (conn->client_identifier) == 0) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Clean session is enabled or client identifier is not defined or it is empty");
		return axl_false;
	}

	/* check 3 */
	if (check_tp) {
		topic_filter_len = strlen (topic_filter);
		if (topic_filter_len == 0) {
			myqtt_log (MYQTT_LEVEL_CRITICAL, "Empty topic filter received");
			return axl_false;
		}

		return topic_filter_len;
	} /* end if */

	return axl_true; /* everything ok */
}

/** 
 * @brief Offline storage initialization for the provided client identifier.
 *
 * See \ref myqtt_storage_init for more information.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifier to initialize storage. 
 *
 * @param storage Part of the storage to initialize.
 *
 * @return If the function is not able to create default storage, the
 * function will fail, otherwise axl_true is returned. The function
 * also returns axl_false in the case client_id is NULL or empty or
 * the context is NULL too.
 */
axl_bool myqtt_storage_init_offline (MyQttCtx * ctx, const char * client_identifier, MyQttStorage storage)
{
	char       * full_path;
	mode_t       umask_mode;
	char       * env;

	/* check input parameters */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0)
		return axl_false;

	/* get previous umask and set a secure one by default during operations */
	umask_mode = umask (0077);

	/* lock during operation during this global configuration */
	if (! ctx->storage_path) {
		myqtt_mutex_lock (&ctx->ref_mutex);
		if (! ctx->storage_path) {
			env = myqtt_support_getenv ("HOME");
			if (env) {
				ctx->storage_path = myqtt_support_build_filename (env, ".myqtt-storage", NULL);
				axl_free (env);
			} else {
				ctx->storage_path = axl_strdup (".myqtt-storage");
			}

			/* set default hash size for storage path */
			ctx->storage_path_hash_size = 4096;
		} /* end if */

		/* if reached this point without having storage dir defined fail */
		if (! ctx->storage_path) {
			myqtt_mutex_unlock (&ctx->ref_mutex);
			myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to allocate/define storage directory for the provided context");
			return axl_false;
		}  /* end if */

		/* create base storage path directory */
		if (! myqtt_support_file_test (ctx->storage_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (mkdir (ctx->storage_path, 0700)) {
				/* restore umask */
				umask (umask_mode);

				/* release */
				myqtt_mutex_unlock (&ctx->ref_mutex);

				myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s, error was: %s", ctx->storage_path, myqtt_errno_get_error (errno));
				return axl_false;
			} /* end if */
		} /* end if */

		myqtt_mutex_unlock (&ctx->ref_mutex);
	} /* end if */

	if (! myqtt_support_file_test (ctx->storage_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (mkdir (ctx->storage_path, 0700)) {
			/* restore umask */
			umask (umask_mode);

			myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s, error was: %s", ctx->storage_path, myqtt_errno_get_error (errno));
			return axl_false;
		} /* end if */
	} /* end if */

	/* lock during check */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, NULL);
	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (mkdir (full_path, 0700)) {
			/* restore umask */
			umask (umask_mode);

			myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s, error was: %s", full_path, myqtt_errno_get_error (errno));
			axl_free (full_path);
			return axl_false;
		} /* end if */
	} /* end if */

	myqtt_log (MYQTT_LEVEL_DEBUG, "Storage dir created at: %s", full_path);
	axl_free (full_path);

	/* now create message directory, subs and will */
	if ((storage & MYQTT_STORAGE_MSGS) == MYQTT_STORAGE_MSGS) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (mkdir (full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);
				
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s for messages, error was: %s", full_path, myqtt_errno_get_error (errno));
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* subs */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "subs", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (mkdir (full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);
				
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s for messages, error was: %s", full_path, myqtt_errno_get_error (errno));
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* will */
	if ((storage & MYQTT_STORAGE_ALL) == MYQTT_STORAGE_ALL) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "will", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (mkdir (full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);
				
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s for messages, error was: %s", full_path, myqtt_errno_get_error (errno));
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* pkgids */
	if ((storage & MYQTT_STORAGE_PKGIDS) == MYQTT_STORAGE_PKGIDS) {
		full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", NULL);
		if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
			if (mkdir (full_path, 0700)) {
				/* restore umask */
				umask (umask_mode);
				
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create storage directory %s for messages, error was: %s", full_path, myqtt_errno_get_error (errno));
				axl_free (full_path);
				return axl_false;
			} /* end if */
		} /* end if */
		axl_free (full_path);
	} /* end if */

	/* restore umask */
	umask (umask_mode);

	return axl_true;
}


/** 
 * @brief Inits storage service for the provided client id.
 *
 * You can use \ref myqtt_storage_set_path to change default
 * location. If nothing is provided on the previous function, the
 * following locations are used to handle session storage:
 *
 * - ${HOME}/.myqtt-storage (if HOME variable is defined).
 * - .myqtt-storage
 * 
 *
 * @param ctx The context where the operation takes place. Cannot be NULL.
 *
 * @param client_id The client identifier to init storage
 * services. Cannot be NULL or empty.
 *
 * @return If the function is not able to create default storage, the
 * function will fail, otherwise axl_true is returned. The function
 * also returns axl_false in the case client_id is NULL or empty or
 * the context is NULL too.
 */
axl_bool myqtt_storage_init (MyQttCtx * ctx, MyQttConn * conn, MyQttStorage storage)
{
	axl_bool result;

	/* check input parameters */
	if (ctx == NULL || conn == NULL || conn->client_identifier == NULL || strlen (conn->client_identifier) == 0)
		return axl_false;

	/* record storage was initilized */
	if ((storage & conn->myqtt_storage_init) == storage)
		return axl_true;

	/* create base storage path directory */
	myqtt_mutex_lock (&conn->op_mutex);

	/* check again initialization */
	if ((storage & conn->myqtt_storage_init) == storage) {

		/* already initialized */
		/* create base storage path directory */
		myqtt_mutex_unlock (&conn->op_mutex);

		return axl_true;
	} /* end if */

	/* call to offline implementation */
	result = myqtt_storage_init_offline (ctx, conn->client_identifier, storage);

	/* remember result only on ok event */
	if (result) {
		/* record storage was initilized */
		conn->myqtt_storage_init |= storage;
	} /* end if */

	/* release */
	myqtt_mutex_unlock (&conn->op_mutex);
	
	return result;
}

int      __myqtt_storage_get_size_from_file_name (MyQttCtx * ctx, const char * file_name, int * position)
{
	int  iterator = 0;
	char buffer[10];

	/* clear buffer */
	memset (buffer, 0, 10);
	while (file_name[iterator] && iterator < 10) {
		if (file_name[iterator] < 48 || file_name[iterator] > 57) {
			if (position)
				(*position) = iterator;
			return myqtt_support_strtod (buffer, NULL);
		}

		buffer[iterator] = file_name[iterator];
		iterator++;
	}

	/* unable to identify value */
	return -1;
}

axl_bool __myqtt_storage_sub_exists (MyQttCtx * ctx, const char * full_path, const char * topic_filter, int topic_filter_len, MyQttQos requested_qos, axl_bool remove_if_found)
{
	struct dirent * entry;
	DIR           * sub_dir;
	int             sub_size;
	char            buffer[4096];
	int             desp;
	int             bytes_read;
	FILE          * handle;
	char          * aux_path;

	sub_dir = opendir (full_path);
	if (sub_dir == NULL) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Failed to open directory %s for inspection, error was: %s", full_path, myqtt_errno_get_error (errno));
		return axl_false;
	} /* end if */

	/* clear buffer */
	memset (buffer, 0, 4096);

	entry = readdir (sub_dir);
	while (entry) {
		/* skip known directories we are not interested in */
		if (axl_cmp (entry->d_name, ".") || axl_cmp (entry->d_name, "..")) {
			/* get next entry */
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* get subscription size */
		sub_size = __myqtt_storage_get_size_from_file_name (ctx, entry->d_name, NULL);

		if (sub_size == topic_filter_len) {
			/* ok, found a size match, try now to check content */
			aux_path  = myqtt_support_build_filename (full_path, entry->d_name, NULL);
			myqtt_log (MYQTT_LEVEL_DEBUG, "Found topic filter size match at %s", aux_path);

			/* open the file */
			handle    = fopen (aux_path, "r");
			if (handle == NULL) {
				myqtt_log (MYQTT_LEVEL_CRITICAL, "Uanble to open directory %s, error was: %s", aux_path, myqtt_errno_get_error (errno));
				axl_free (aux_path);
				break;
			} /* end if */

			desp = 0;
			while (desp < topic_filter_len) {

				/* read content from file into the buffer to do a partial check */
				bytes_read = fread (buffer, 4096, 1, handle);

				/* do a partial check */
				if (! axl_memcmp (buffer, topic_filter + desp, bytes_read)) {
					/* mismatch found, this is not the file */
					break;
				} /* end if */
				
				desp += bytes_read;
			}

			/* remove subscription */
			if (remove_if_found)
				unlink (aux_path);

			/* reached this point, we have found the file */
			axl_free (aux_path);
			fclose (handle);
			
			closedir (sub_dir);
			return axl_true;
		} /* end if */


		/* get next entry */
		entry = readdir (sub_dir);
	}

	closedir (sub_dir);

	return axl_false;
}

/** 
 * @brief Function to record subscription for the provided client. 
 *
 * @param ctx The context where the storage operation takes place.
 *
 * @param conn The connection for which the subscription will be stored.
 *
 * @param topic_filter The subscription's topic filter
 *
 * @param requested_qos The QoS requested by the application level.
 *
 * @return axl_true in the case the operation was completed otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_sub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter, MyQttQos requested_qos)
{
	char             * full_path;
	char             * hash_value;
	char             * path_item;
	struct timeval     stamp;
	int                topic_filter_len;
	int                written;
	FILE             * sub_file;

	/* check input parameters */
	topic_filter_len = __myqtt_storage_check (ctx, conn, axl_true, topic_filter);
	if (! topic_filter_len)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_filter) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;

	/* now create message directory */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "subs", hash_value, NULL);
	if (full_path == NULL) {
		axl_free (hash_value);
		return axl_false;
	} /* end if */

	/* create parent directory if it wasn't created */
	if (! myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_DIR)) {
		if (mkdir (full_path, 0700)) {
			/* get log */
			myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to create topic filter directory to store subscription: %s (error was: %s)", full_path, myqtt_errno_get_error (errno));

			axl_free (full_path);
			axl_free (hash_value);
			return axl_false;
		} /* end if */
	} else {
		/* directory exists, check if the subscription is
		 * already on the disk */
		if (__myqtt_storage_sub_exists (ctx, full_path, topic_filter, topic_filter_len, requested_qos, axl_false)) {
			axl_free (full_path);
			axl_free (hash_value);
			return axl_true;
		} /* end if */

	} /* end if */
	axl_free (full_path);

	/* now storage subscription */
	gettimeofday (&stamp, NULL);
	path_item = axl_strdup_printf ("%d-%d-%d-%d-%d", topic_filter_len, requested_qos, hash_value, stamp.tv_sec, stamp.tv_usec);

	if (path_item == NULL) {
		axl_free (hash_value);
		return axl_false;
	} /* end if */

	/* build full path */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "subs", hash_value, path_item, NULL);
	axl_free (path_item);
	axl_free (hash_value);

	if (full_path == NULL) 
		return axl_false;

	/* open file and release path */
	sub_file = fopen (full_path, "w");
	if (sub_file == NULL) {
		/* report log */
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Unable to save subscription, failed to write to file '%s', error was: %s", full_path, myqtt_errno_get_error (errno));
		axl_free (full_path);
		return axl_false;
	} /* end if */
	axl_free (full_path);

	written = fwrite (topic_filter, 1, topic_filter_len, sub_file);
	if (written != topic_filter_len) {
		/* report log */
		myqtt_log (MYQTT_LEVEL_CRITICAL, "fwrite() call failed to write expected bytes (%d), written (%d), error was: %s", topic_filter_len, written, myqtt_errno_get_error (errno));
		return axl_false;
	} /* end if */

	fclose (sub_file);

	return axl_true;
}

/** 
 * @brief Allows to check if the provided topic_filter is already subscribed for the provided connection and optionally removing it.
 *
 * @param ctx The context where operation takes place.
 *
 * @param conn The connection to check subscription.
 *
 * @param topic_filter The topic filter to check.
 *
 * @param remove_if_found Remove the subscription if found.
 */
axl_bool myqtt_storage_sub_exists_common (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter, MyQttQos requested_qos, axl_bool remove_if_found)
{
	int    topic_filter_len;
	char * full_path;
	char * hash_value;

	/* check input parameters */
	topic_filter_len = __myqtt_storage_check (ctx, conn, axl_true, topic_filter);
	if (! topic_filter_len)
		return axl_false;

	/* hash topic filter */
	hash_value = axl_strdup_printf ("%u", axl_hash_string ((axlPointer) topic_filter) % ctx->storage_path_hash_size);
	if (hash_value == NULL)
		return axl_false;

	/* now create message directory */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "subs", hash_value, NULL);
	axl_free (hash_value);
	if (full_path == NULL) {
		return axl_false;
	} /* end if */

	/* call to check subscription in the provided directory */
	if (__myqtt_storage_sub_exists (ctx, full_path, topic_filter, topic_filter_len, requested_qos, remove_if_found)) {
		axl_free (full_path);
		return axl_true;
	} /* end if */

	axl_free (full_path);

	return axl_false;
}

/** 
 * @brief Allows to check if the provided topic_filter is already
 * subscribed for the provided connection.
 *
 * @param ctx The context where operation takes place.
 *
 * @param conn The connection to check subscription.
 *
 * @param topic_filter The topic filter to check.
 */
axl_bool myqtt_storage_sub_exists (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter)
{
	/* call to check if it exists but without removing without caring about qos */
	return myqtt_storage_sub_exists_common (ctx, conn, topic_filter, 0, axl_false);
}

void __myqtt_storage_sub_conn_register (MyQttCtx * ctx, MyQttConn * conn, const char * file_name, const char * full_path)
{
	/* get qos from file name */
	int        pos           = 0;
	int        size          = __myqtt_storage_get_size_from_file_name (ctx, file_name, &pos);
	MyQttQos   qos           = MYQTT_QOS_0;
	char     * topic_filter;
	FILE     * _file;

	/* get qos */
	if (pos >= (strlen (file_name) + 1))
		return;

	qos = __myqtt_storage_get_size_from_file_name (ctx, file_name + pos + 1, NULL);
	topic_filter = axl_new (char, size + 1);
	if (topic_filter == NULL)
		return;

	_file = fopen (full_path, "r");
	if (_file == NULL) {
		axl_free (topic_filter);
		return;
	} /* end if */

	if (fread (topic_filter, 1, size, _file) != size) {
		fclose (_file);
		axl_free (topic_filter);
		return;
	} /* end if */
	fclose (_file);

	/* call to register */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Recovering subs conn-id=%d qos=%d sub=%s", conn->id, qos, topic_filter);
	__myqtt_reader_subscribe (ctx, conn, topic_filter, qos);

	/* it is not required to release topic_filter here, that
	 * reference is not owned by __myqtt_reader_subscribe */

	return;
}

int __myqtt_storage_sub_count_aux (MyQttCtx * ctx, MyQttConn * conn, const char * aux_path, axl_bool __register)
{
	DIR           * files;
	struct dirent * entry;
	char          * full_path;
	int             count = 0;

	files = opendir (aux_path);
	entry = readdir (files);
	while (files && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (files);
			continue;
		} /* end if */
		
		/* build path */
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_REG) == DT_REG)
			count ++;

		if (__register) {
			/* register subscription */
			full_path = myqtt_support_build_filename (aux_path, entry->d_name, NULL);
			__myqtt_storage_sub_conn_register (ctx, conn, entry->d_name, full_path);
			axl_free (full_path);
		} /* end if */
#else 
		full_path = myqtt_support_build_filename (aux_path, entry->d_name, NULL);
		if (myqtt_support_file_test (full_path, FILE_EXISTS | FILE_IS_REGULAR)) {
			count ++;
		} /* end if */

		/* call to register */
		if (__register)
			__myqtt_storage_sub_conn_register (ctx, conn, entry->d_name, full_path);

		axl_free (full_path);
#endif



		/* next file */
		entry = readdir (files);
	}

	closedir (files);

	return count;
}

/** 
 * @internal Function to iterate over all subscriptions to restore
 * connection state.
 *
 * @param ctx The context where the operation will take place
 *
 * @param conn The connection where the operation will be implemented.
 */
int __myqtt_storage_iteration (MyQttCtx * ctx, MyQttConn * conn, axl_bool __register) {

	char          * full_path;
	char          * aux_path;
	DIR           * sub_dir;
	struct dirent * entry;
	int             total = 0;

	/* check input parameters */
	if (! __myqtt_storage_check (ctx, conn, axl_false, NULL))
		return 0;

	/* get full path to subscriptions */
	full_path = myqtt_support_build_filename (ctx->storage_path, conn->client_identifier, "subs", NULL);
	if (full_path == NULL) 
		return axl_false; /* allocation failure */

	sub_dir = opendir (full_path);
	entry   = readdir (sub_dir);
	while (sub_dir && entry) {

		/* get next entry and skip those we are not interested in */
		if (axl_cmp (".", entry->d_name) || axl_cmp ("..", entry->d_name)) {
			entry = readdir (sub_dir);
			continue;
		} /* end if */

		/* check if it is a directory */
#if defined(_DIRENT_HAVE_D_TYPE)
		if ((entry->d_type & DT_DIR) == DT_DIR) {
			/* count subscriptions */
			aux_path  = myqtt_support_build_filename (full_path, entry->d_name, NULL);
			total    += __myqtt_storage_sub_count_aux (ctx, conn, aux_path, __register);
			axl_free (aux_path);
		}
#else 
		aux_path = myqtt_support_build_filename (full_path, entry->d_name, NULL);
		if (myqtt_support_file_test (aux_path, FILE_EXISTS | FILE_IS_DIR)) {
			/* count subscriptions */
			total += __myqtt_storage_sub_count_aux (ctx, conn, aux_path, __register);
		}
		axl_free (aux_path);
#endif

		/* get next entry */
		entry   = readdir (sub_dir);
	}

	closedir (sub_dir);
	axl_free (full_path);

	if (__register)
		return __register;

	return total;
}

/** 
 * @brief Allows to get current number of subscriptions registered on
 * the storage for the provided connection.
 */
int      myqtt_storage_sub_count (MyQttCtx * ctx, MyQttConn * conn)
{
	/* call to iterate and count */
	return __myqtt_storage_iteration (ctx, conn, axl_false);
}

/** 
 * @brief Function to unsubscribe the provided topic filter on the
 * provided connection.
 *
 * @param ctx The context where the operation will take place.
 *
 * @param conn The connection for which the subscription will be stored.
 *
 * @param topic_filter The unsubscription' topic filter
 *
 * @return axl_true in the case the operation was completed otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_unsub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter) 
{
	/* call to remove subscription */
	return myqtt_storage_sub_exists_common (ctx, conn, topic_filter, 0, axl_true);
}

/** 
 * @brief Allows to storage the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection to select local session storage.
 *
 * @param packet_id The packet id associated to the message.
 *
 * @param app_msg The application message to store.
 *
 * @param app_msg_size The size of the application message to store.
 *
 * @return The function return NULL on failure and a handler that
 * points to the message stored.
 */
axlPointer myqtt_storage_store_msg_offline (MyQttCtx      * ctx, 
					    const char    * client_identifier,
					    int             packet_id, 
					    MyQttQos        qos, 
					    unsigned char * app_msg, 
					    int             app_msg_size)
{
	char            * full_path;
	char            * ref;
	struct timeval    stamp;
	FILE            * handle;

	/* check input values:
	 *
	 * don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || packet_id < 0 || app_msg_size <= 0 || app_msg == NULL)
		return NULL;

	/* call to init message store */
	if (! myqtt_storage_init_offline (ctx, client_identifier, MYQTT_STORAGE_MSGS))
		return NULL;

	/* build path */
	gettimeofday (&stamp, NULL);
	ref       = axl_strdup_printf ("%d-%d-%d-%d-%d", packet_id, app_msg_size, qos, stamp.tv_sec, stamp.tv_usec);
	if (! ref)
		return NULL;
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "msgs", ref, NULL);
	axl_free (ref);
	if (! full_path) 
		return NULL;

	/* save file */
	handle = fopen (full_path, "w");
	if (! handle) {
		/* report failure */
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Failed to store message at %s, error was: %s", full_path, myqtt_errno_get_error (errno));
		axl_free (full_path);
		return NULL;
	} /* end if */
	
	if (fwrite (app_msg, 1, app_msg_size, handle) != app_msg_size) {
		myqtt_log (MYQTT_LEVEL_CRITICAL, "Failed to storage message at %s, error was: %s", full_path, myqtt_errno_get_error (errno));
		fclose (handle);
		axl_free (full_path);
		return NULL;
	} /* end if */

	/* message saved */

	fclose (handle);
	return full_path;	
}

/** 
 * @brief Allows to storage the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection to select local session storage.
 *
 * @param packet_id The packet id associated to the message.
 *
 * @param app_msg The application message to store.
 *
 * @param app_msg_size The size of the application message to store.
 *
 * @return The function return NULL on failure and a handler that
 * points to the message stored.
 */
axlPointer myqtt_storage_store_msg   (MyQttCtx * ctx, MyQttConn * conn, int packet_id, MyQttQos qos, unsigned char * app_msg, int app_msg_size)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return axl_false; 

	return myqtt_storage_store_msg_offline (ctx, conn->client_identifier, packet_id, qos, app_msg, app_msg_size);
}

/** 
 * @brief Allows to release the provided message on the local session
 * storage associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection to select local session storage.
 *
 * @param handle Reference to the message as returned by \ref myqtt_storage_store_msg
 *
 * @return axl_true if the message was successfully stored, otherwise
 * axl_false is returned.
 */
axl_bool myqtt_storage_release_msg   (MyQttCtx * ctx, MyQttConn * conn, axlPointer handle)
{
	/* check input values */
	if (ctx == NULL || conn == NULL)
		return axl_false;

	/* call to init message store */
	if (! myqtt_storage_init (ctx, conn, MYQTT_STORAGE_MSGS))
		return axl_false;

	/* release the message */
	if (handle) {
		unlink ((const char *) handle);
		axl_free ((char *) handle);
	} /* end if */

	return axl_true;
}

/** 
 * @brief Allows to lock the provided id or fail if it is already in
 * use.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param client_identifier The client identifer where to lock the provided pkgid.
 *
 * @param pkg_id The packet id to lock. 
 *
 * @return axl_true if the pkgid was locked, otherwise axl_false is returned. 
 */
axl_bool myqtt_storage_lock_pkgid_offline (MyQttCtx      * ctx, 
					   const char    * client_identifier,
					   int             pkg_id)
{
	int    handle;
	char * full_path;
	char * ref;

	/* don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || pkg_id < 1)
		return axl_false;

	/* create full path to lock pkgid */
	ref       = axl_strdup_printf ("%d", pkg_id);
	if (ref == NULL)
		return axl_false;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", ref, NULL);
	axl_free (ref);
	if (full_path == NULL) 
		return axl_false;

	/* open directory handle */
	myqtt_log (MYQTT_LEVEL_DEBUG, "Attempting to lock pkgid file at: %s", full_path);
	handle = open (full_path, O_CREAT | O_EXCL, 0600);
	axl_free (full_path);
	if (handle < 0)
		return axl_false;
	close (handle);

	return axl_true;	
}

/** 
 * @brief Allows to lock the provided id or fail if it is already in
 * use.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection where the operation takes place, using its session.
 *
 * @param pkg_id The packet id to lock. 
 *
 * @return axl_true if the pkgid was locked, otherwise axl_false is returned. 
 */
axl_bool myqtt_storage_lock_pkgid (MyQttCtx * ctx, MyQttConn * conn, int pkg_id)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return axl_false;

	return myqtt_storage_lock_pkgid_offline (ctx, conn->client_identifier, pkg_id);
}

/** 
 * @brief Allows to release the provided id from the current session
 * associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection where the operation takes place, using its session.
 *
 * @param pkg_id The packet id to release. 
 *
 * @return axl_true if the pkgid was releaseed, otherwise axl_false is returned. 
 */
void     myqtt_storage_release_pkgid_offline    (MyQttCtx      * ctx, 
						 const char    * client_identifier,
						 int             pkg_id)
{
	char * full_path;
	char * ref;

	/* don't check here for (pkg_id > 65536) because we use values
	 * over that to store QoS0 messages that do not need a valid
	 * pkg_id but we need a different value to store them */
	if (ctx == NULL || client_identifier == NULL || strlen (client_identifier) == 0 || pkg_id < 1)
		return;

	/* create full path to lock pkgid */
	ref       = axl_strdup_printf ("%d", pkg_id);
	if (ref == NULL)
		return;

	/* build path */
	full_path = myqtt_support_build_filename (ctx->storage_path, client_identifier, "pkgids", ref, NULL);
	axl_free (ref);
	if (full_path == NULL) 
		return;

	/* open directory handle */
	unlink (full_path);
	axl_free (full_path);

	return;

}

/** 
 * @brief Allows to release the provided id from the current session
 * associated to the provided connection.
 *
 * @param ctx The context where the operation takes place.
 *
 * @param conn The connection where the operation takes place, using its session.
 *
 * @param pkg_id The packet id to release. 
 *
 * @return axl_true if the pkgid was releaseed, otherwise axl_false is returned. 
 */
void     myqtt_storage_release_pkgid (MyQttCtx * ctx, MyQttConn * conn, int pkg_id)
{
	/* avoid segfault when conn reference is NULL */
	if (conn == NULL)
		return;
	return myqtt_storage_release_pkgid_offline (ctx, conn->client_identifier, pkg_id);
}

/** 
 * @brief Allows to recover session stored by the server with current
 * storage for the provided connection.
 *
 * @param ctx The context where the operation will take place.
 *
 * @param conn The connection that is requested to recover session. 
 *
 * @return axl_true if session was recovered, otherwise axl_false is
 * returned.
 */
axl_bool myqtt_storage_session_recover (MyQttCtx * ctx, MyQttConn * conn)
{
	/* call to iterate and count */
	return __myqtt_storage_iteration (ctx, conn, axl_true);
}


/** 
 * @brief Allows to configure the storage location for the provided
 * MyQtt context.
 *
 * @param ctx The context where the operation takes place. It cannot be NULL.
 *
 * @param storage_path The storage dir to configure. It cannot be NULL
 * or empty.
 *
 */
void     myqtt_storage_set_path (MyQttCtx * ctx, const char * storage_path, int hash_size)
{
	if (ctx == NULL || storage_path == NULL || strlen (storage_path) == 0)
		return;

	/* lock during operation */
	myqtt_mutex_lock (&ctx->ref_mutex);

	/* update storage path */
	if (ctx->storage_path)
		axl_free (ctx->storage_path);
	ctx->storage_path = axl_strdup (storage_path);

	/* update hash size */
	ctx->storage_path_hash_size = hash_size;

	/* now unlock */
	myqtt_mutex_unlock (&ctx->ref_mutex);

	return;
}
       
