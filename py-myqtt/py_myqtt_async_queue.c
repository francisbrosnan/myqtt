/* 
 *  MyQtt: A high performance open source MQTT implementation
 *  Copyright (C) 2015 Advanced Software Production Line, S.L.
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

#include <py_myqtt_async_queue.h>

struct _PyMyQttAsyncQueue {
	/* header required to initialize python required bits for
	   every python object */
	PyObject_HEAD

	/* pointer to the myqtt context */
	MyQttAsyncQueue * async_queue;
};

/** 
 * @brief Allows to get the MyQttAsyncQueue type definition found inside the
 * PyMyQttAsyncQueue encapsulation.
 *
 * @param py_myqtt_async_queue The PyMyQttAsyncQueue that holds a reference to the
 * inner MyQttAsyncQueue.
 *
 * @return A reference to the inner MyQttAsyncQueue.
 */
MyQttAsyncQueue * py_myqtt_async_queue_get (PyMyQttAsyncQueue * py_myqtt_async_queue)
{
	if (py_myqtt_async_queue == NULL)
		return NULL;
	
	/* return current context created */
	return py_myqtt_async_queue->async_queue;
}

static int py_myqtt_async_queue_init_type (PyMyQttAsyncQueue *self, PyObject *args, PyObject *kwds)
{
    return 0;
}

/** 
 * @brief Function used to allocate memory required by the object myqtt.async_queue
 */
static PyObject * py_myqtt_async_queue_new (PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	PyMyQttAsyncQueue *self;

	/* create the object */
	self = (PyMyQttAsyncQueue *)type->tp_alloc(type, 0);

	/* create the context */
	self->async_queue = myqtt_async_queue_new ();

	py_myqtt_log (PY_MYQTT_DEBUG, "created new AsyncQueue: %p, self->queue: %p", self, self->async_queue);

	return (PyObject *)self;
}

/** 
 * @brief Function used to finish and dealloc memory used by the object myqtt.async_queue
 */
static void py_myqtt_async_queue_dealloc (PyMyQttAsyncQueue* self)
{
	/* do a log */
	py_myqtt_log (PY_MYQTT_DEBUG, "finishing PyVortxAsyncQueue reference: %p, self->queue: %p (items: %d)", 
		       self, self->async_queue, myqtt_async_queue_items (self->async_queue));

	/* free async_queue */
	myqtt_async_queue_safe_unref (&(self->async_queue)); 

	/* free the node it self */
	self->ob_type->tp_free ((PyObject*)self);

	return;
}

/** 
 * @brief Direct wrapper for the myqtt_async_queue_push function. 
 */
static PyObject * py_myqtt_async_queue_push (PyMyQttAsyncQueue* self, PyObject * args)
{
	PyObject * obj;

	/* now implement other attributes */
	if (! PyArg_ParseTuple (args, "O", &obj))
		return NULL;

	/* incremenet reference count */
	Py_INCREF (obj); 

	py_myqtt_log (PY_MYQTT_DEBUG, "pushing object %p into queue: %p, self->queue: %p",
		       obj, self, self->async_queue);

	/* let other threads to work after this */
	Py_BEGIN_ALLOW_THREADS

	/* push the item */
	myqtt_async_queue_push (self->async_queue, obj);

	/* restore thread state */
	Py_END_ALLOW_THREADS

	Py_INCREF (Py_None);
	return Py_None;
}

/** 
 * @brief Direct wrapper for the myqtt_async_queue_pop function.
 */
static PyObject * py_myqtt_async_queue_pop (PyMyQttAsyncQueue* self)
{
	PyObject           * _result;

	/* allow other threads to enter into the python space */
	Py_BEGIN_ALLOW_THREADS

	py_myqtt_log (PY_MYQTT_DEBUG, "allowed other threads to enter python space, poping next item (queue.pop ())");

	/* get the value */
	_result = myqtt_async_queue_pop (self->async_queue);

        py_myqtt_log (PY_MYQTT_DEBUG, "Finished wait on queue.pop (), reference returned: %p (queue: %p, self->queue: %p)", 
		       _result, self, self->async_queue);

	/* restore thread state */
	Py_END_ALLOW_THREADS

	/* do not decrement reference counting. It was increased to
	 * provide a reference owned by the caller */
	return _result;
}

/** 
 * @brief Direct wrapper for the myqtt_init_async_queue function. This method
 * receives a myqtt.async_queue object and initializes it calling to
 * myqtt_init_async_queue.
 */
static PyObject * py_myqtt_async_queue_timedpop (PyMyQttAsyncQueue* self, PyObject * args)
{
	PyObject * _result;
	int        microseconds = 0;

	/* now implement other attributes */
	if (! PyArg_ParseTuple (args, "i", &microseconds))
		return NULL;

	/* allow other threads to enter into the python space */
	Py_BEGIN_ALLOW_THREADS

	/* get the value */
	_result = myqtt_async_queue_timedpop (self->async_queue, microseconds);

	/* restore thread state */
	Py_END_ALLOW_THREADS

	if (_result == NULL) {
		Py_INCREF (Py_None);
		return Py_None;
	} /* end if */

	/* do not decrement reference counting. It was increased to
	 * provide a reference owned by the caller */
	return _result;
}

/** 
 * @brief Direct mapping for unref operation myqtt_async_queue_unref
 */
/* static PyObject * py_myqtt_async_queue_unref (PyMyQttAsyncQueue* self)
{
	py_myqtt_log (PY_MYQTT_DEBUG, "calling to queue.unref (): %p, self->queue: %p",
		       self, self->async_queue); 
		       */
	/* decrease reference counting */
	/*	myqtt_async_queue_safe_unref (&(self->async_queue)); */

	/* and decrement references to the pyobject */
	/*	Py_CLEAR (self); */

	/* return None */
	/*	Py_INCREF (Py_None);
	return Py_None;
	}*/

/** 
 * @brief Direct mapping for the ref operation myqtt_async_queue_ref.
 */
PyObject * py_myqtt_async_queue_ref (PyMyQttAsyncQueue* self)
{
	/* increase reference counting */
	myqtt_async_queue_ref (self->async_queue);

	/* and decrement references to the pyobject */
	Py_INCREF (self);

	/* return None */
	Py_INCREF (Py_None);
	return Py_None;
}


static PyMethodDef py_myqtt_async_queue_methods[] = { 
	{"push", (PyCFunction) py_myqtt_async_queue_push, METH_VARARGS,
	 "Allows to push a new item into the queue. Use pop() and timedpop() to retreive items stored."},
	{"pop", (PyCFunction) py_myqtt_async_queue_pop, METH_NOARGS,
	 "Allows to get the next item available on the queue (myqtt.AsyncQueue). This method will block the caller. Check items attribute to know if there are pending elements."},
	{"timedpop", (PyCFunction) py_myqtt_async_queue_timedpop, METH_VARARGS,
	 "Allows to get the next item available on the queue (myqtt.AsyncQueue) limiting the wait period to the value (microseconds) provided. This method will block the caller. Check items attribute to know if there are pending elements."},
/*	{"ref", (PyCFunction) py_myqtt_async_queue_ref, METH_NOARGS,
	"Allows to increase reference counting on the provided queue reference. "}, */
/*	{"unref", (PyCFunction) py_myqtt_async_queue_unref, METH_NOARGS,
	"Allows to decrease reference counting on the provided queue reference. If reached 0 the queue is finished."}, */
 	{NULL}  
}; 

/** 
 * @brief This function implements the generic attribute getting that
 * allows to perform complex member resolution (not merely direct
 * member access).
 */
PyObject * py_myqtt_async_queue_get_attr (PyObject *o, PyObject *attr_name) {
	const char         * attr = NULL;
	PyObject           * result;
	PyMyQttAsyncQueue * self = (PyMyQttAsyncQueue *) o;

	/* now implement other attributes */
	if (! PyArg_Parse (attr_name, "s", &attr))
		return NULL;

	/* printf ("received request to return attribute value of '%s'..\n", attr); */

	if (axl_cmp (attr, "length")) {
		/* found length attribute */
		return Py_BuildValue ("i", myqtt_async_queue_length (self->async_queue));
	} else if (axl_cmp (attr, "waiters")) {
		/* found waiters attribute */
		return Py_BuildValue ("i", myqtt_async_queue_waiters (self->async_queue));
	} else if (axl_cmp (attr, "items")) {
		/* found items attribute */
		return Py_BuildValue ("i", myqtt_async_queue_items (self->async_queue));
	} /* end if */

	/* printf ("Attribute not found: '%s'..\n", attr); */

	/* first implement generic attr already defined */
	result = PyObject_GenericGetAttr (o, attr_name);
	if (result)
		return result;
	
	return NULL;
}

static PyTypeObject PyMyQttAsyncQueueType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size*/
    "myqtt.AsyncQueue",              /* tp_name*/
    sizeof(PyMyQttAsyncQueue),       /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)py_myqtt_async_queue_dealloc, /* tp_dealloc*/
    0,                         /* tp_print*/
    0,                         /* tp_getattr*/
    0,                         /* tp_setattr*/
    0,                         /* tp_compare*/
    0,                         /* tp_repr*/
    0,                         /* tp_as_number*/
    0,                         /* tp_as_sequence*/
    0,                         /* tp_as_mapping*/
    0,                         /* tp_hash */
    0,                         /* tp_call*/
    0,                         /* tp_str*/
    py_myqtt_async_queue_get_attr,  /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /* tp_flags*/
    "MyQtt context object required to function with MyQtt API",           /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    py_myqtt_async_queue_methods,     /* tp_methods */
    0, /* py_myqtt_async_queue_members, */     /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)py_myqtt_async_queue_init_type,      /* tp_init */
    0,                         /* tp_alloc */
    py_myqtt_async_queue_new,         /* tp_new */

};

/** 
 * @brief Inits the myqtt async_queue module. It is implemented as a type.
 */
void init_myqtt_async_queue (PyObject * module) 
{
    
	/* register type */
	if (PyType_Ready(&PyMyQttAsyncQueueType) < 0)
		return;
	
	Py_INCREF (&PyMyQttAsyncQueueType);
	PyModule_AddObject(module, "AsyncQueue", (PyObject *)&PyMyQttAsyncQueueType);

	return;
}





