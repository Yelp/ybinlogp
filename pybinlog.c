#include <Python.h>
#include <structmember.h>
#include <stddef.h>
#include "ybinlogp.h"

/**
 * FORMAT DESCRIPTION EVENTS
 **/

typedef struct {
	PyObject_HEAD
	struct format_description_event fde;
} FormatDescriptionEventObject;

static PyMemberDef
FormatDescriptionEventObject_members[] = {
	{"format_version", T_SHORT, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, format_version), READONLY, "The MySQL binlog format version"},
	{"server_version", T_STRING, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, server_version), READONLY, "The MySQL server version"},
	{"timestamp", T_UINT, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, timestamp), READONLY, "The timestamp for the binlog"},
	{"header_len", T_UBYTE, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, header_len), READONLY, "The length of header fields"},
	{NULL}
};

static int
FormatDescriptionEventObject_init(FormatDescriptionEventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->fde, 0, sizeof(struct format_description_event));
	return 1;
}

static PyTypeObject
FormatDescriptionEventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                               /* ob_size */
	"FormatDescriptionEvent",        /* tp_name */
	sizeof(FormatDescriptionEventObject), /* tp_basicsize */
	0,                               /* tp_itemsize */
	0,                               /* tp_dealloc */
	0,                               /* tp_print */
	0,                               /* tp_getattr */
	0,                               /* tp_setattr */
	0,                               /* tp_compare */
	0,                               /* tp_repr */
	0,                               /* tp_as_number */
	0,                               /* tp_as_sequence */
	0,                               /* tp_as_mapping */
	0,                               /* tp_hash */
	0,                               /* tp_call */
	0,                               /* tp_str */
	0,                               /* tp_getattro */
	0,                               /* tp_setattro */
	0,                               /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
	"MySQL format description event", /* tp_doc */
	0,                               /* tp_traverse */
	0,                               /* tp_clear */
	0,                               /* tp_richcompare */
	0,                               /* tp_weaklistoffset */
	0,                               /* tp_iter */
	0,                               /* tp_iternext */
	0,                               /* tp_methods */
	FormatDescriptionEventObject_members, /* tp_members */
	0,                               /* tp_getset */
	0,                               /* tp_base */
	0,                               /* tp_dict */
	0,                               /* tp_descr_get */
	0,                               /* tp_descr_set */
	0,                               /* tp_dictoffset */
	(initproc)FormatDescriptionEventObject_init, /* tp_init */
	0,                               /* tp_alloc */
	0,                               /* tp_new */
};

typedef struct {
	PyObject_HEAD
	struct event event;
	PyObject *data;
} EventObject;

static PyMemberDef
EventObject_members[] = {
	{"timestamp", T_UINT, offsetof(EventObject, event) + offsetof(struct event, timestamp), 0, "timestamp"},
	{"type_code", T_BYTE, offsetof(EventObject, event) + offsetof(struct event, type_code), 0, "type_code"},
	{"server_id", T_UINT, offsetof(EventObject, event) + offsetof(struct event, server_id), 0, "server_id"},
	{"length", T_UINT, offsetof(EventObject, event) + offsetof(struct event, length), 0, "length"},
	{"next_position", T_UINT, offsetof(EventObject, event) + offsetof(struct event, next_position), 0, "next_position"},
	{"flags", T_UINT, offsetof(EventObject, event) + offsetof(struct event, flags), 0, "flags"},
	{"offset", T_UINT, offsetof(EventObject, event) + offsetof(struct event, offset), 0, "offset"},
	{"data", T_OBJECT_EX, offsetof(EventObject, data), READONLY, "binlog data"},
	{NULL} /* Sentinel */
};

static int
EventObject_init(EventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->event, 0, sizeof(struct event));
	self->data = Py_None;
	Py_INCREF(self->data);
	return 1;
}

static void
EventObject_dealloc(EventObject *self)
{
	/* dispose will call free */
	if (self->event.data != NULL) {
		free(self->event.data);
	}
	Py_DECREF(self->data);
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
	FILE *f;
	PyObject *file;

	if (!PyArg_ParseTuple(args, "O", &file))
		return 0;

	if (file != NULL) {
		self->file = file;
		Py_INCREF(file);
		f = PyFile_AsFile(file);
		fd = fileno(f);

		self->event = PyObject_New(EventObject, &EventObjectType);
		EventObject_init(self->event, NULL, NULL);
		Py_DECREF(self->event->data); /* should decref None */

		read_fde(fd, &(self->event->event));

		/*
		self->event->data = (PyObject *) PyObject_New(FormatDescriptionEventObject, &FormatDescriptionEventObjectType);
		FormatDescriptionEventObject_init((FormatDescriptionEventObject *)self->event->data, NULL, NULL);
		memcpy(&((FormatDescriptionEventObject *) self->event->data)->fde,
			   format_description_event_data(&self->event->event),
			   format_description_event_data_len(&self->event->event));
		*/

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

	if ((PyObject *) self->event != Py_None) {
		ev = self->event;
		//if (ev->data == NULL)
		//ev->data = PyString_FromStringAndSize(ev->event.data, ev->event.length - 19);

		self->event = PyObject_New(EventObject, &EventObjectType);
		EventObject_init(self->event, NULL, NULL);
		offset = next_after(&ev->event);
		read_event(fileno(PyFile_AsFile(self->file)), &(self->event->event), offset);
		Py_INCREF(self->event);

#if 0
		Py_DECREF(self->event); /* should decref None */
		switch(self->event->event.type_code) {
		case 2: /* EVENT_QUERY */
			if (self->event->event.type_code == 2)
				fprintf(stderr, "zomg query");
			break;
		}
#endif

		//Py_DECREF(ev);
		return (PyObject *)ev;
	} else
		Py_RETURN_NONE;
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

	mod = Py_InitModule3("binlog", NULL, "Python module for parsing MySQL binlogs");
	if (mod == NULL)
		return;

	/* FormatDescriptionEvent */
	FormatDescriptionEventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&FormatDescriptionEventObjectType) < 0)
		return;
	Py_INCREF(&FormatDescriptionEventObjectType);
	PyModule_AddObject(mod, "FormatDescriptionEvent", (PyObject *) &FormatDescriptionEventObjectType);

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

	PyModule_AddObject(mod, "EVENT_UNKNOWN", PyInt_FromLong(0L));
	PyModule_AddObject(mod, "EVENT_START", PyInt_FromLong(1L));
	PyModule_AddObject(mod, "EVENT_QUERY", PyInt_FromLong(2L));
	PyModule_AddObject(mod, "EVENT_STOP", PyInt_FromLong(3L));
	PyModule_AddObject(mod, "EVENT_ROTATE", PyInt_FromLong(4L));
	PyModule_AddObject(mod, "EVENT_INTVAR", PyInt_FromLong(5L));
	PyModule_AddObject(mod, "EVENT_LOAD", PyInt_FromLong(6L));
	PyModule_AddObject(mod, "EVENT_SLAVE", PyInt_FromLong(7L));
	PyModule_AddObject(mod, "EVENT_CREATE_FILE", PyInt_FromLong(8L));
	PyModule_AddObject(mod, "EVENT_APPEND_BLOCK", PyInt_FromLong(9L));
	PyModule_AddObject(mod, "EVENT_EXEC_LOAD", PyInt_FromLong(10L));
}
