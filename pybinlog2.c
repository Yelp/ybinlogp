#include <Python.h>
#include <structmember.h>
#include <stddef.h>
#include "ybinlogp.h"

#define LLU_TYPE         unsigned long long

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


PyMODINIT_FUNC initbinlog(void)
{
	PyObject *mod;
    long i;

	init_ybinlogp();

	mod = Py_InitModule3("binlog", NULL, "Python module for parsing MySQL binlogs");
	if (mod == NULL)
		return;

    for (i = MIN_TYPE_CODE; i < NUM_EVENT_TYPES; ++i) {
        PyModule_AddIntConstant(mod, event_type(i), i);
    }
}
