#include <Python.h>
#include "ybinlogp.h"

typedef struct {
	PyObject_HEAD
	struct event *event;
	PyObject *data;
} EventObject;

static PyMemberDef
Event_members[] = {
	{"timestamp", T_UINT, offsetof(EventObject, event) + offsetof(event, timestamp), 0, "timestamp"},
	{"type_code", T_UINT, offsetof(EventObject, event) + offsetof(event, type_code), 0, "type_code"},
	{"server_id", T_UINT, offsetof(EventObject, event) + offsetof(event, server_id), 0, "server_id"},
	{"length", T_UINT, offsetof(EventObject, event) + offsetof(event, length), 0, "length"},
	{"next_position", T_UINT, offsetof(EventObject, event) + offsetof(event, next_position), 0, "next_position"},
	{"flags", T_UINT, offsetof(EventObject, event) + offsetof(event, flags), 0, "flags"},
	{"data", T_OBJECT_EXC, offsetof(EventObject, data),  0, "flags"},
	{"offset", T_UINT, offsetof(EventObject, event) + offsetof(event, offset), 0, "offset"},
	{NULL} /* Sentinel */
};

static int
EventObject_init(EventObject *self, PyObject *args, PyObject *kwds)
{
	init_event(self->event);
	self->data = NULL;
	return 1;
}

static PyObject*
EventObject_advance(EventObject *self, PyObject *args, PyObject *kwds)
{
	int fd;
	off64_t offset;
	if (!PyArg_ParseTuple(args, "i", &fd))
		return NULL;
	offset = next_after(self->event);
	reset_event(self->event);
	read_event(fd, self->event, offset);

	Py_XDECREF(self->data);
	self->data = PyByteArray_FromStringAndSize(self->event->data, self->event->length - 19);
	Py_RETURN_NONE;
}


static void
EventObject_dealloc(EventObject *self)
{
	dispose_event(self->event);
	Py_XDECREF(self->data);
	self->ob_type->tp_free((PyObject *) self);
}

static PyMethodDef
EventObject_methods[] = {
	{ "advance", (PyCFunction) EventObject_advance, METH_VARARGS, "advance through a file descriptor" },
	{ NULL }
};

static PyTypeObject
EventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /* ob_size */
	"Event",                   /* tp_name */
	sizeof(EventObject),       /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)EventObject_dealloc, /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_compare */
	0,                         /* tp_repr */
	0,                         /* tp_as_number */
	0,                         /* tp_as_sequence */
	0,                         /* tp_as_mapping */
	0,                         /* tp_hash */
	0,                         /* tp_call */
	0,                         /* tp_str */
	0,                         /* tp_getattro */
	0,                         /* tp_setattro */
	0,                         /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
	"MySQL binlog event",        /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	EventObject_methods,       /* tp_methods */
	0,       /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)EventObject_init,/* tp_init */
	0,                         /* tp_alloc */
	0,                         /* tp_new */
};


/*
static PyObject*
binlog_read_fde(PyObject *self, PyObject *args)
{
	int fd;
	FILE *file;
	PyObject *obj;
	if (!PyArg_ParseTuple(args, "O", &obj))
		return NULL;

	if (!PyFile_Check(obj))
		return NULL;

	file = PyFile_AsFile(obj);
	rewind(file);

	fd = fileno(file);
	read_fde(fd);

	Py_RETURN_NONE;
}

static PyObject*
binlog_nearest_offset(PyObject *self, PyObject *args)
{
	const int *fd;
	const long *start;
	PY_LONG_LONG offset;
	struct event *evbuf;
	if (!PyArg_ParseTuple(args, "il", &fd, &start))
		return NULL;

	evbuf = malloc(sizeof(struct event));
	init_event(evbuf);

	offset = (PY_LONG_LONG) nearest_offset(fd, (off64_t) start, evbuf, 1);
	return PyLong_FromLongLong(offset);
}
*/

PyMODINIT_FUNC initbinlog(void)
{
	PyObject *mod;

	mod = Py_InitModule3("binlog", NULL, "test");
	if (mod == NULL)
		return;

	EventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&EventObjectType) < 0)
		return;

	Py_INCREF(&EventObjectType);
	PyModule_AddObject(mod, "Event", (PyObject *) &EventObjectType);
}
