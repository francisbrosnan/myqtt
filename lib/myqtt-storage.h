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
#ifndef __MYQTT_STORAGE_H__
#define __MYQTT_STORAGE_H__

#include <myqtt.h>

axl_bool myqtt_storage_init (MyQttCtx * ctx, MyQttConn * conn);

axl_bool myqtt_storage_sub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter, MyQttQos requested_qos);

axl_bool myqtt_storage_sub_exists (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter);

int      myqtt_storage_sub_count (MyQttCtx * ctx, MyQttConn * conn);

axl_bool myqtt_storage_unsub (MyQttCtx * ctx, MyQttConn * conn, const char * topic_filter);

void     myqtt_storage_set_path (MyQttCtx * ctx, const char * storage_path, int hash_size);

/*** internal API: don't use it, it may change at any time ***/
int      __myqtt_storage_get_size_from_file_name (MyQttCtx * ctx, const char * file_name);

#endif
