#include <Python.h>
#include <structmember.h>
#include <stddef.h>
#include "ybinlogp.h"

typedef struct {
	PyObject_HEAD
	struct event *event;
	PyObject *data;
} EventObject;

static PyMemberDef
EventObject_members[] = {
	{"timestamp", T_UINT, offsetof(EventObject, event) + offsetof(struct event, timestamp), 0, "timestamp"},
	{"type_code", T_UINT, offsetof(EventObject, event) + offsetof(struct event, type_code), 0, "type_code"},
	{"server_id", T_UINT, offsetof(EventObject, event) + offsetof(struct event, server_id), 0, "server_id"},
	{"length", T_UINT, offsetof(EventObject, event) + offsetof(struct event, length), 0, "length"},
	{"next_position", T_UINT, offsetof(EventObject, event) + offsetof(struct event, next_position), 0, "next_position"},
	{"flags", T_UINT, offsetof(EventObject, event) + offsetof(struct event, flags), 0, "flags"},
	{"data", T_OBJECT_EX, offsetof(EventObject, data),  0, "flags"},
	{"offset", T_UINT, offsetof(EventObject, event) + offsetof(struct event, offset), 0, "offset"},
	{NULL} /* Sentinel */
};

static int
EventObject_init(EventObject *self, PyObject *args, PyObject *kwds)
{
	/* must be malloc, not PyMem_Malloc */
	self->event = malloc(sizeof(struct event));
	init_event(self->event);
	self->data = NULL;
	return 1;
}

static void
EventObject_dealloc(EventObject *self)
{
	/* dispose will call free */
	dispose_event(self->event);
	Py_XDECREF(self->data);
	self->ob_type->tp_free((PyObject *) self);
}

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
	0,                         /* tp_methods */
	EventObject_members,       /* tp_members */
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

typedef struct {
	PyObject_HEAD
	PyObject *file;
	EventObject *event;
} ParserObject;

static PyMemberDef
ParserObject_members[] = {
	{ "file", T_OBJECT_EX, offsetof(ParserObject, file), READONLY, "underlying file object" },
//  { "event", T_OBJECT_EX, offsetof(ParserObject, prev_event), READONLY, "the last event read" },
	{ NULL }
};
static int
ParserObject_init(ParserObject *self, PyObject *args, PyObject *kwds)
{
	int fd;
	off64_t offset;
	FILE *f;
	PyObject *file;

	if (!PyArg_ParseTuple(args, "O", &file))
		return 0;

	if (file != NULL) {
		self->file = file;
		Py_INCREF(file);
		f = PyFile_AsFile(file);
		fd = fileno(f);
		read_fde(fd);

		self->event = PyObject_New(EventObject, &EventObjectType);
		EventObject_init(self->event, NULL, NULL);
		//offset = nearest_offset(fd, 5, self->event->event, 1);
		return 1;
	} else {
		return 0;
	}
}

static void
ParserObject_dealloc(ParserObject *self)
{
	Py_DECREF(self->file);
	self->ob_type->tp_free((PyObject *) self);
}

static PyObject*
ParserObject_next(ParserObject *self, PyObject *args, PyObject *kwds)
{
	off64_t offset;
	EventObject *ev;

	if (self->event->event->next_position && self->event->event->next_position != self->event->event->offset) {
		Py_RETURN_NONE;
	}

	ev = PyObject_New(EventObject, &EventObjectType);
	EventObject_init(ev, NULL, NULL);
	offset = next_after(self->event->event);
	Py_DECREF(self->event);
	read_event(fileno(PyFile_AsFile(self->file)), ev->event, offset);
	self->event = ev;
	Py_INCREF(self->event);
	return (PyObject *)ev;
}

static PyMethodDef
ParserObject_methods[] = {
    {"next", (PyCFunction) ParserObject_next, METH_VARARGS, "iterate through the binlog" },
    {NULL, NULL, 0, NULL} /* Sentinel */
};



static PyTypeObject
ParserObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /* ob_size */
	"BinlogParser",                  /* tp_name */
	sizeof(ParserObject),      /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor)ParserObject_dealloc, /* tp_dealloc */
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
	"MySQL binlog parser",     /* tp_doc */
	0,                         /* tp_traverse */
	0,                         /* tp_clear */
	0,                         /* tp_richcompare */
	0,                         /* tp_weaklistoffset */
	0,                         /* tp_iter */
	0,                         /* tp_iternext */
	ParserObject_methods,      /* tp_methods */
	ParserObject_members,      /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc)ParserObject_init,/* tp_init */
	0,                         /* tp_alloc */
	0,                         /* tp_new */
};


PyMODINIT_FUNC initbinlog(void)
{
	PyObject *mod;
	init_ybinlogp();

	mod = Py_InitModule3("binlog", NULL, "test");
	if (mod == NULL)
		return;

	EventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&EventObjectType) < 0)
		return;
	Py_INCREF(&EventObjectType);
	PyModule_AddObject(mod, "Event", (PyObject *) &EventObjectType);

	ParserObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&ParserObjectType) < 0)
		return;
	Py_INCREF(&ParserObjectType);
	PyModule_AddObject(mod, "BinlogParser", (PyObject *) &ParserObjectType);
}
