#include <Python.h>
#include <structmember.h>
#include <stddef.h>
#include "ybinlogp.h"

#define LLU_TYPE         unsigned long long

/***********
 * FORMAT DESCRIPTION EVENTS
 **********/

typedef struct {
	PyObject_HEAD
	struct format_description_event fde;
} FormatDescriptionEventObject;

static PyMemberDef
FormatDescriptionEventObject_members[] = {
	{"format_version", T_SHORT, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, format_version), READONLY, "The MySQL binlog format version"},
	//{"server_version", T_STRING, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, server_version), READONLY, "The MySQL server version"},
	{"timestamp", T_UINT, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, timestamp), READONLY, "The timestamp for the binlog"},
	{"header_len", T_UBYTE, offsetof(FormatDescriptionEventObject, fde) + offsetof(struct format_description_event, header_len), READONLY, "The length of header fields"},
	{NULL}
};

static int
FormatDescriptionEventObject_init(FormatDescriptionEventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->fde, 0, sizeof(struct format_description_event));
	return 0;
}

static PyObject*
FormatDescriptionEventObject_repr(FormatDescriptionEventObject *self)
{
	return PyString_FromFormat("FormatDescriptionEvent(format_version=%d, timestamp=%d, header_len=%d)",
							   self->fde.format_version,
							   self->fde.timestamp,
							   self->fde.header_len);
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
	(reprfunc) FormatDescriptionEventObject_repr, /* tp_repr */
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

/***********
 * QUERY EVENTS
 **********/

typedef struct {
	PyObject_HEAD
	struct query_event query;
	PyObject *db_name;
	PyObject *query_text;
} QueryEventObject;

static PyMemberDef
QueryEventObject_members[] = {
	{"thread_id", T_UINT, offsetof(QueryEventObject, query) + offsetof(struct query_event, thread_id), READONLY, "The MySQL thread ID"},
	{"query_time", T_UINT, offsetof(QueryEventObject, query) + offsetof(struct query_event, query_time), READONLY, "The query time, measured in seconds"},
	// db_name_len omitted
	{"error_code", T_USHORT, offsetof(QueryEventObject, query) + offsetof(struct query_event, error_code), READONLY, "The query's error code"},
	{"database_name", T_OBJECT_EX, offsetof(QueryEventObject, db_name), READONLY, "The database name"},
	{"query_text", T_OBJECT_EX, offsetof(QueryEventObject, query_text), READONLY, "The query text"},
	{NULL}
};


static int
QueryEventObject_init(QueryEventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->query, 0, sizeof(struct query_event));

	self->db_name = Py_None;
	Py_INCREF(Py_None);

	self->query_text = Py_None;
	Py_INCREF(Py_None);

	return 0;
}

static void
QueryEventObject_dealloc(QueryEventObject *self)
{
	Py_DECREF(self->db_name);
	Py_DECREF(self->query_text);
	self->ob_type->tp_free((PyObject *) self);
}

static PyObject*
QueryEventObject_repr(QueryEventObject *self)
{
	return PyString_FromFormat("QueryEvent(db_name=%s, thread_id=%d, query_time=%d, query_text=%s)",
							   PyString_AsString(PyObject_Repr(self->db_name)),
							   self->query.thread_id,
							   self->query.query_time,
							   PyString_AsString(PyObject_Repr(self->query_text)));
}

static PyTypeObject
QueryEventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                               /* ob_size */
	"QueryEvent",                    /* tp_name */
	sizeof(QueryEventObject),        /* tp_basicsize */
	0,                               /* tp_itemsize */
	(destructor) QueryEventObject_dealloc, /* tp_dealloc */
	0,                               /* tp_print */
	0,                               /* tp_getattr */
	0,                               /* tp_setattr */
	0,                               /* tp_compare */
	(reprfunc) QueryEventObject_repr, /* tp_repr */
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
	"MySQL query event",             /* tp_doc */
	0,                               /* tp_traverse */
	0,                               /* tp_clear */
	0,                               /* tp_richcompare */
	0,                               /* tp_weaklistoffset */
	0,                               /* tp_iter */
	0,                               /* tp_iternext */
	0,                               /* tp_methods */
	QueryEventObject_members, /* tp_members */
	0,                               /* tp_getset */
	0,                               /* tp_base */
	0,                               /* tp_dict */
	0,                               /* tp_descr_get */
	0,                               /* tp_descr_set */
	0,                               /* tp_dictoffset */
	(initproc)QueryEventObject_init, /* tp_init */
	0,                               /* tp_alloc */
	0,                               /* tp_new */
};

/***********
 * INTVAR EVENTS
 **********/

typedef struct {
	PyObject_HEAD
	struct intvar_event intvar;
} IntvarEventObject;

static PyMemberDef
IntvarEventObject_members[] = {
	{"type", T_UBYTE, offsetof(IntvarEventObject, intvar) + offsetof(struct intvar_event, type), READONLY, "The intvar type"},
	{"value", T_ULONGLONG, offsetof(IntvarEventObject, intvar) + offsetof(struct intvar_event, value), READONLY, "The intvar value"},
	{NULL}
};


static int
IntvarEventObject_init(IntvarEventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->intvar, 0, sizeof(struct intvar_event));
	return 0;
}

static PyObject*
IntvarEventObject_repr(IntvarEventObject *self)
{
	return PyString_FromFormat("IntvarEvent(type=%u, value=%llu)",
							   (unsigned int) self->intvar.type,
							   (LLU_TYPE) self->intvar.value);
}

static PyTypeObject
IntvarEventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                               /* ob_size */
	"IntvarEvent",                   /* tp_name */
	sizeof(IntvarEventObject),       /* tp_basicsize */
	0,                               /* tp_itemsize */
	0,                               /* tp_dealloc */
	0,                               /* tp_print */
	0,                               /* tp_getattr */
	0,                               /* tp_setattr */
	0,                               /* tp_compare */
	(reprfunc) IntvarEventObject_repr, /* tp_repr */
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
	"MySQL intvar event",            /* tp_doc */
	0,                               /* tp_traverse */
	0,                               /* tp_clear */
	0,                               /* tp_richcompare */
	0,                               /* tp_weaklistoffset */
	0,                               /* tp_iter */
	0,                               /* tp_iternext */
	0,                               /* tp_methods */
	IntvarEventObject_members,       /* tp_members */
	0,                               /* tp_getset */
	0,                               /* tp_base */
	0,                               /* tp_dict */
	0,                               /* tp_descr_get */
	0,                               /* tp_descr_set */
	0,                               /* tp_dictoffset */
	(initproc)IntvarEventObject_init, /* tp_init */
	0,                               /* tp_alloc */
	0,                               /* tp_new */
};

/***********
 * ROTATE EVENTS
 **********/

typedef struct {
	PyObject_HEAD
	struct rotate_event rotate;
	PyObject *next_file;
} RotateEventObject;

static PyMemberDef
RotateEventObject_members[] = {
	{"next_position", T_ULONGLONG, offsetof(RotateEventObject, rotate) + offsetof(struct rotate_event, next_position), READONLY, "The binlog position within the next file"},
	{"next_file", T_OBJECT_EX, offsetof(RotateEventObject, next_file), READONLY, "The name of the next binlog"},
	{NULL}
};


static int
RotateEventObject_init(RotateEventObject *self, PyObject *args, PyObject *kwds)
{
	memset(&self->rotate, 0, sizeof(struct rotate_event));
	self->next_file = Py_None;
	Py_INCREF(Py_None);
	return 0;
}

static void
RotateEventObject_dealloc(RotateEventObject *self)
{
	Py_DECREF(self->next_file);
	self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
RotateEventObject_repr(RotateEventObject *self)
{
	return PyString_FromFormat("RotateEvent(next_file=%s, next_position=%llu)",
							   PyString_AsString(PyObject_Repr(self->next_file)),
							   (LLU_TYPE) self->rotate.next_position);
}

static PyTypeObject
RotateEventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                               /* ob_size */
	"RotateEvent",                   /* tp_name */
	sizeof(RotateEventObject),       /* tp_basicsize */
	0,                               /* tp_itemsize */
	(destructor) RotateEventObject_dealloc, /* tp_dealloc */
	0,                               /* tp_print */
	0,                               /* tp_getattr */
	0,                               /* tp_setattr */
	0,                               /* tp_compare */
	(reprfunc) RotateEventObject_repr, /* tp_repr */
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
	"MySQL log rotate event",        /* tp_doc */
	0,                               /* tp_traverse */
	0,                               /* tp_clear */
	0,                               /* tp_richcompare */
	0,                               /* tp_weaklistoffset */
	0,                               /* tp_iter */
	0,                               /* tp_iternext */
	0,                               /* tp_methods */
	RotateEventObject_members,       /* tp_members */
	0,                               /* tp_getset */
	0,                               /* tp_base */
	0,                               /* tp_dict */
	0,                               /* tp_descr_get */
	0,                               /* tp_descr_set */
	0,                               /* tp_dictoffset */
	(initproc) RotateEventObject_init, /* tp_init */
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
	return 0;
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

static PyObject*
EventObject_repr(EventObject *self)
{
	return PyString_FromFormat("EventObject(type_code=%d, offset=%llu, server_id=%u, timestamp=%d, data=%s)",
							   self->event.type_code,
							   (LLU_TYPE) self->event.offset,
							   self->event.server_id,
							   self->event.timestamp,
							   PyString_AsString(PyObject_Repr(self->data)));
}

static PyTypeObject
EventObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /* ob_size */
	"Event",                   /* tp_name */
	sizeof(EventObject),       /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor) EventObject_dealloc, /* tp_dealloc */
	0,                         /* tp_print */
	0,                         /* tp_getattr */
	0,                         /* tp_setattr */
	0,                         /* tp_compare */
	(reprfunc) EventObject_repr, /* tp_repr */
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

		read_fde(fd, &(self->event->event));

		Py_DECREF(self->event->data); /* should decref None */
		self->event->data = (PyObject *) PyObject_New(FormatDescriptionEventObject, &FormatDescriptionEventObjectType);
		FormatDescriptionEventObject_init((FormatDescriptionEventObject *)self->event->data, NULL, NULL);

		/* XXX: can't access .server_version after this */
		memcpy(&((FormatDescriptionEventObject *) self->event->data)->fde, self->event->event.data, sizeof(struct format_description_event));

		return 0;
	} else {
		return 1;
	}
}

static void
ParserObject_dealloc(ParserObject *self)
{
	Py_DECREF(self->file);
	self->ob_type->tp_free((PyObject *) self);
}

static PyObject*
ParserObject_iter(ParserObject *self)
{
	return (PyObject *) self;
}

static PyObject*
ParserObject_next(ParserObject *self)
{
	off64_t offset;
	EventObject *ev;

	fprintf(stderr, "fuck off\n");

	if (self->event != NULL) {
		/* read the last event */
		ev = self->event;

		/* get the next event, and store it privately as self->event */
		fprintf(stderr, "1");
		self->event = PyObject_New(EventObject, &EventObjectType);
		EventObject_init(self->event, NULL, NULL);
		fprintf(stderr, "2");
		offset = next_after(&ev->event);
		if (read_event_unsafe(fileno(PyFile_AsFile(self->file)), &(self->event->event), offset), 0) {
			fprintf(stderr, "Got an error in read_event\n");
			return -1;
		}
		fprintf(stderr, "3");

		/* incref our event */
		Py_INCREF(self->event);
		fprintf(stderr, "4");

		/* try to convert event->data into the right kind of object */
		switch(self->event->event.type_code) {

		case 0: /* log parsing has ended */
			fprintf(stderr, "5");
			Py_DECREF(self->event);
			self->event = NULL;
			break;
		case 2: /* EVENT_QUERY */
			fprintf(stderr, "6");
			Py_DECREF(self->event->data);
			self->event->data = (PyObject *) PyObject_New(QueryEventObject, &QueryEventObjectType);
			QueryEventObject_init((QueryEventObject *) self->event->data, NULL, NULL);

			assert(self->event->event.data != NULL);
			fprintf(stderr, "self->event->event.data: %p\n", self->event->event.data);
			memcpy(&((QueryEventObject *) self->event->data)->query, self->event->event.data, sizeof(struct query_event));

			Py_DECREF(((QueryEventObject *) self->event->data)->db_name);
			((QueryEventObject *) self->event->data)->db_name = PyString_FromStringAndSize(
				query_event_db_name(&self->event->event),
				((struct query_event *) self->event->event.data)->db_name_len);

			Py_DECREF(((QueryEventObject *) self->event->data)->query_text);
			((QueryEventObject *) self->event->data)->query_text = PyString_FromStringAndSize(
				query_event_statement(&self->event->event),
				query_event_statement_len(&self->event->event));

			break;
		case 4: /* ROTATE EVENT */
			fprintf(stderr, "7");
			Py_DECREF(self->event->data);
			self->event->data = (PyObject *) PyObject_New(RotateEventObject, &RotateEventObjectType);
			RotateEventObject_init((RotateEventObject *) self->event->data, NULL, NULL);

			memcpy(&((RotateEventObject *) self->event->data)->rotate, self->event->event.data, sizeof(struct rotate_event));

			Py_DECREF(((RotateEventObject *) self->event->data)->next_file);
			((RotateEventObject *) self->event->data)->next_file = PyString_FromStringAndSize(
				rotate_event_file_name(&self->event->event),
				rotate_event_file_name_len(&self->event->event));

			break;

		case 5: /* INTVAR EVENT *
			fprintf(stderr, "8");
			Py_DECREF(self->event->data);
			fprintf(stderr, "9\n");
			self->event->data = (PyObject *) PyObject_New(IntvarEventObject, &IntvarEventObjectType);
			fprintf(stderr, "10\n");
			IntvarEventObject_init((IntvarEventObject *) self->event->data, NULL, NULL);
			fprintf(stderr, "11, self->event->data = %p\n", self->event->data);
			fprintf(stderr, "11, self->event->data->intvar = %p\n", &((IntvarEventObject *) self->event->data)->intvar);
			fprintf(stderr, "11, event.data = %p\n", self->event->event.data);
			fprintf(stderr, "event.data.type = %d\n", ((struct intvar_event *) self->event->event.data)->type);
			fprintf(stderr, "11, sizeof -> %zd\n", sizeof(struct intvar_event));
			memcpy(&((IntvarEventObject *) self->event->data)->intvar, self->event->event.data, sizeof(struct intvar_event));
			fprintf(stderr, "12\n");*/
			break;

		default:
			fprintf(stderr, "WTF? type_code is %d\n", self->event->event.type_code);
			break;
		}

		fprintf(stderr, "fuck on, biatch\n");
		//Py_DECREF(ev);
		return (PyObject *) ev;
	} else {
		fprintf(stderr, "fuck on, biatch\n");
		return NULL;
	}
}

/*
static PyMethodDef
ParserObject_methods[] = {
    {NULL, NULL, 0, NULL} 
};
*/



static PyTypeObject
ParserObjectType = {
	PyObject_HEAD_INIT(NULL)
	0,                         /* ob_size */
	"BinlogParser",                  /* tp_name */
	sizeof(ParserObject),      /* tp_basicsize */
	0,                         /* tp_itemsize */
	(destructor) ParserObject_dealloc, /* tp_dealloc */
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
	(getiterfunc) ParserObject_iter, /* tp_iter */
	(iternextfunc) ParserObject_next, /* tp_iternext */
	0,      /* tp_methods */
	ParserObject_members,      /* tp_members */
	0,                         /* tp_getset */
	0,                         /* tp_base */
	0,                         /* tp_dict */
	0,                         /* tp_descr_get */
	0,                         /* tp_descr_set */
	0,                         /* tp_dictoffset */
	(initproc) ParserObject_init,/* tp_init */
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

	/* QueryEvent */
	QueryEventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&QueryEventObjectType) < 0)
		return;
	Py_INCREF(&QueryEventObjectType);
	PyModule_AddObject(mod, "QueryEvent", (PyObject *) &QueryEventObjectType);

	/* IntvarEvent */
	IntvarEventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&IntvarEventObjectType) < 0)
		return;
	Py_INCREF(&IntvarEventObjectType);
	PyModule_AddObject(mod, "IntvarEvent", (PyObject *) &IntvarEventObjectType);

	/* RotateEvent */
	RotateEventObjectType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&RotateEventObjectType) < 0)
		return;
	Py_INCREF(&RotateEventObjectType);
	PyModule_AddObject(mod, "RotateEvent", (PyObject *) &RotateEventObjectType);

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
