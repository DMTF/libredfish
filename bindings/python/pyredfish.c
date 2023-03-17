//----------------------------------------------------------------------------
// Copyright Notice:
// Copyright 2017-2019 DMTF. All rights reserved.
// License: BSD 3-Clause License. For full text see link: https://github.com/DMTF/libredfish/blob/main/LICENSE.md
//----------------------------------------------------------------------------
#include <Python.h>
#include "redfish.h"

typedef struct {
  PyObject_HEAD
  redfishService* cService;
} pyRedfishService;

typedef struct {
  PyObject_HEAD
  redfishPayload* cPayload;
} pyRedfishPayload;

static void redfishService_dealloc(PyObject* self);
static PyObject* redfishService_attribs(PyObject *self, char *attrname);
static void redfishPayload_dealloc(PyObject* self);
static PyObject* redfishPayload_attribs(PyObject *self, char *attrname);
static PyObject* redfishPayload_repr(PyObject* self);

PyTypeObject redfishService_Type = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,
  "RedfishService",         /* char *tp_name; */
  sizeof(pyRedfishService), /* int tp_basicsize; */
  0,                        /* int tp_itemsize; */
  redfishService_dealloc,   /* destructor tp_dealloc; */
  NULL,                     /* printfunc  tp_print;   */
  redfishService_attribs,   /* getattrfunc  tp_getattr; */
  NULL,                     /* setattrfunc  tp_setattr;  */
  NULL,                     /* cmpfunc  tp_compare;  */
  NULL,                     /* reprfunc  tp_repr; */
  NULL,                     /* PyNumberMethods *tp_as_number; */
  0,                        /* PySequenceMethods *tp_as_sequence; */
  0,                        /* PyMappingMethods *tp_as_mapping; */
  NULL,                     /* hashfunc tp_hash; */
  0,                        /* ternaryfunc tp_call; */
  NULL                      /* reprfunc tp_str; */
};

PyTypeObject redfishPayload_Type = {
  PyObject_HEAD_INIT(&PyType_Type)
  0,
  "RedfishPayload",         /* char *tp_name; */
  sizeof(pyRedfishPayload), /* int tp_basicsize; */
  0,                        /* int tp_itemsize; */
  redfishPayload_dealloc,   /* destructor tp_dealloc; */
  NULL,                     /* printfunc  tp_print;   */
  redfishPayload_attribs,   /* getattrfunc  tp_getattr; */
  NULL,                     /* setattrfunc  tp_setattr;  */
  NULL,                     /* cmpfunc  tp_compare;  */
  redfishPayload_repr,      /* reprfunc  tp_repr; */
  NULL,                     /* PyNumberMethods *tp_as_number; */
  0,                        /* PySequenceMethods *tp_as_sequence; */
  0,                        /* PyMappingMethods *tp_as_mapping; */
  NULL,                     /* hashfunc tp_hash; */
  0,                        /* ternaryfunc tp_call; */
  NULL                      /* reprfunc tp_str; */
};

static void redfishService_dealloc(PyObject* self)
{
    cleanupServiceEnumerator(((pyRedfishService*)self)->cService);
    PyMem_DEL(self);
}

static void redfishPayload_dealloc(PyObject* self)
{
    cleanupPayload(((pyRedfishPayload*)self)->cPayload);
    PyMem_DEL(self);
}

static PyObject* pyCreateServiceEnumerator(PyObject *self, PyObject *args)
{
    const char* host;
    const char* root = NULL;
    PyObject* pyAuth = NULL;
    enumeratorAuthentication cAuth;
    unsigned int flags = 0;
    redfishService* cService;
    pyRedfishService* pyService;

    if(!PyArg_ParseTuple(args, "s|sOI", &host, &root, &pyAuth, &flags))
    {
        return NULL;
    }
    if(pyAuth != NULL)
    {
        if(!PyArg_ParseTuple(pyAuth, "Is|s", &cAuth.authType, &cAuth.authCodes.userPass.username, &cAuth.authCodes.userPass.password))
        {
            return NULL;
        }
        cService = createServiceEnumerator(host, root, &cAuth, flags);
    }
    else
    {
        cService = createServiceEnumerator(host, root, NULL, flags);
    }
    if(cService == NULL)
    {
        return NULL;
    }
    pyService = PyObject_NEW(pyRedfishService, &redfishService_Type);
    if(pyService)
    {
        pyService->cService = cService;
    }
    return (PyObject*)pyService;
}

static PyObject* pyCreateRedfishPayload(PyObject *self, PyObject *args)
{
    const char* content;
    pyRedfishService* pyService;
    redfishService* cService;
    pyRedfishPayload* pyPayload;
    redfishPayload* cPayload;

    if(!PyArg_ParseTuple(args, "sO", &content, &pyService))
    {
        return NULL;
    }
    cService = pyService->cService;
    if(cService == NULL)
    {
        return NULL;
    }
    cPayload = createRedfishPayloadFromString(content, cService);
    if(cPayload == NULL)
    {
        return NULL;
    }
    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyGetUriFromService(PyObject *self, PyObject *args)
{
    const char* uri;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    json_t* cJson;
    const char* retStr;

    if(!PyArg_ParseTuple(args, "s", &uri))
    {
        return NULL;
    }

    cJson = getUriFromService(cService, uri);
    retStr = json_dumps(cJson, 0);
    return Py_BuildValue("s", retStr);
}

static PyObject* pyPatchUriFromService(PyObject *self, PyObject *args)
{
    const char* uri;
    const char* content;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    json_t* cJson;
    const char* retStr;

    if(!PyArg_ParseTuple(args, "ss", &uri, &content))
    {
        return NULL;
    }

    cJson = patchUriFromService(cService, uri, content);
    retStr = json_dumps(cJson, 0);
    return Py_BuildValue("s", retStr);
}

static PyObject* pyPostUriFromService(PyObject *self, PyObject *args)
{
    const char* uri;
    const char* content;
    int contentLength;
    char* contentType = NULL;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    json_t* cJson;
    const char* retStr;

    if(!PyArg_ParseTuple(args, "ss#|S", &uri, &content, &contentLength, &contentType))
    {
        return NULL;
    }

    cJson = postUriFromService(cService, uri, content, contentLength, contentType);
    retStr = json_dumps(cJson, 0);
    return Py_BuildValue("s", retStr);
}

static PyObject* pyDeleteUriFromService(PyObject *self, PyObject *args)
{
    const char* uri;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    bool ret;

    if(!PyArg_ParseTuple(args, "s", &uri))
    {
        return NULL;
    }

    ret = deleteUriFromService(cService, uri);
    return Py_BuildValue("b", ret);
}

static PyObject* pyGetRedfishServiceRoot(PyObject *self, PyObject *args)
{
    const char* version = NULL;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    redfishPayload* cPayload;
    pyRedfishPayload* pyPayload;

    if(!PyArg_ParseTuple(args, "|s", &version))
    {
        return NULL;
    }

    cPayload = getRedfishServiceRoot(cService, version);
    if(cPayload == NULL)
    {
        return NULL;
    }
    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyGetPayloadByPath(PyObject *self, PyObject *args)
{
    const char* version = NULL;
    redfishService* cService = ((pyRedfishService*)self)->cService;
    redfishPayload* cPayload;
    pyRedfishPayload* pyPayload;

    if(!PyArg_ParseTuple(args, "|s", &version))
    {
        return NULL;
    }

    cPayload = getPayloadByPath(cService, version);
    if(cPayload == NULL)
    {
        return NULL;
    }
    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyIsPayloadCollection(PyObject *self, PyObject *args)
{
    bool ret;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;

    ret = isPayloadCollection(cPayload);
    return Py_BuildValue("b", ret);
}

static PyObject* pyGetPayloadChild(PyObject *self, PyObject *args)
{
    unsigned int index;
    const char* name;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
    redfishPayload* cChildPayload = NULL;
    pyRedfishPayload* pyPayload;

    if(PyArg_ParseTuple(args, "I", &index))
    {
	cChildPayload = getPayloadByIndex(cPayload, index);
    }
    else if(PyArg_ParseTuple(args, "s", &name))
    {
	cChildPayload = getPayloadByNodeName(cPayload, name);
    }
    if(cChildPayload == NULL)
    {
	    return NULL;
    }

    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cChildPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyGetPayloadForPath(PyObject *self, PyObject *args)
{
    const char* path;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
    redfishPayload* cChildPayload = NULL;
    pyRedfishPayload* pyPayload;

    if(!PyArg_ParseTuple(args, "s", &path))
    {
	return NULL;
    }
    cChildPayload = getPayloadForPathString(cPayload, path);
    if(cChildPayload == NULL)
    {
	    return NULL;
    }

    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cChildPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyGetCollectionSize(PyObject *self, PyObject *args)
{
    size_t ret;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;

    ret = getCollectionSize(cPayload);
    return Py_BuildValue("I", ret);
}

static PyObject* pyPatchPayloadStringProperty(PyObject *self, PyObject *args)
{
    redfishPayload* cRetPayload;
    pyRedfishPayload* pyPayload;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
    const char* propName;
    const char* propValue;

    if(!PyArg_ParseTuple(args, "ss", &propName, &propValue))
    {
	return NULL;
    }

    cRetPayload = patchPayloadStringProperty(cPayload, propName, propValue);
    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cRetPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyPostPayload(PyObject *self, PyObject *args)
{
    redfishPayload* cRetPayload;
    pyRedfishPayload* pyPayload;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
    pyRedfishPayload* value;

    if(!PyArg_ParseTuple(args, "o", &value))
    {
	return NULL;
    }

    cRetPayload = postPayload(cPayload, value->cPayload);
    pyPayload = PyObject_NEW(pyRedfishPayload, &redfishPayload_Type);
    if(pyPayload)
    {
        pyPayload->cPayload = cRetPayload;
    }
    return (PyObject*)pyPayload;
}

static PyObject* pyDeletePayload(PyObject *self, PyObject *args)
{
    bool ret;
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;

    ret = deletePayload(cPayload);
    return Py_BuildValue("b", ret);
}

static PyMethodDef Methods[] = {
    {"createServiceEnumerator",  pyCreateServiceEnumerator, METH_VARARGS, "Create a Redfish Enumerator."},
    {"createRedfishPayload",  pyCreateRedfishPayload, METH_VARARGS, "Create a Redfish Payload."},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef ServiceMethods[] = {
    {"getUri", pyGetUriFromService, METH_VARARGS, "Get Uri from the Redfish Service."},
    {"patchUri", pyPatchUriFromService, METH_VARARGS, "Patch Uri from the Redfish Service."},
    {"postUri", pyPostUriFromService, METH_VARARGS, "Post Uri from the Redfish Service."},
    {"deleteUri", pyDeleteUriFromService, METH_VARARGS, "Delete Uri from the Redfish Service."},
    {"getRedfishServiceRoot", pyGetRedfishServiceRoot, METH_VARARGS, "Get the Service Root of the Redfish Service."},
    {"getPayloadByPath", pyGetPayloadByPath, METH_VARARGS, "Get a payload from the Service by the Red Path value."},
    {NULL, NULL, 0, NULL}
};

static PyMethodDef PayloadMethods[] = {
    {"isCollection", pyIsPayloadCollection, METH_VARARGS, "Is the payload a collection?"},
    {"getChild", pyGetPayloadChild, METH_VARARGS, "Get the child payload by index or name"},
    {"getPayloadForPath", pyGetPayloadForPath, METH_VARARGS, "Get a child (or grandchild) by RedPath"},
    {"getCollectionSize", pyGetCollectionSize, METH_VARARGS, "Get the size of the collection."},
    {"patch", pyPatchPayloadStringProperty, METH_VARARGS, "Patch a value to the payload."},
    {"post", pyPostPayload, METH_VARARGS, "POST to the collection."},
    {"delete", pyDeletePayload, METH_VARARGS, "Delete the payload."},
    {NULL, NULL, 0, NULL}
};

static PyObject* redfishService_attribs(PyObject *self, char *attrname)
{
    return Py_FindMethod(ServiceMethods, self, attrname);
}

static PyObject* redfishPayload_attribs(PyObject *self, char *attrname)
{
    if(strcmp(attrname, "value") == 0)
    {
        redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
	switch(json_typeof(cPayload->json))
	{
            case JSON_OBJECT:
		    /*TODO*/
		    return NULL;
            case JSON_ARRAY:
		    /*TODO*/
		    return NULL;
            case JSON_STRING:
		    return Py_BuildValue("s", json_string_value(cPayload->json));
	    case JSON_INTEGER:
		    return Py_BuildValue("i", json_integer_value(cPayload->json));
	    case JSON_REAL:
		    return Py_BuildValue("d", json_real_value(cPayload->json));
	    case JSON_TRUE:
		    return Py_BuildValue("b", 1);
	    case JSON_FALSE:
		    return Py_BuildValue("b", 0);
	    case JSON_NULL:
		    return Py_BuildValue("z", NULL);
	}
    }
    return Py_FindMethod(PayloadMethods, self, attrname);
}

static PyObject* redfishPayload_repr(PyObject* self)
{
    redfishPayload* cPayload = ((pyRedfishPayload*)self)->cPayload;
    char* str;
    PyObject* ret;

    str = payloadToString(cPayload, true);
    ret = Py_BuildValue("s", str);
    free(str);
    return ret;
}

PyMODINIT_FUNC initlibredfish(void)
{
    redfishService_Type.tp_methods = ServiceMethods;
    (void)Py_InitModule("libredfish", Methods);
}
