/******************************************************
file:
    vString.c
purpose:
    python type define for DM char/varchar/binary/varbinary variables in dmPython
interface:
    {}
history:
    Date        Who         RefDoc      Memo
    2015-6-9    wmm                     Created
*******************************************************/

#include "var_pub.h"
#include "Error.h"
#include "py_Dameng.h"
#include "Buffer.h"

//-----------------------------------------------------------------------------
// Declaration of string variable functions.
//-----------------------------------------------------------------------------
static int StringVar_Initialize(udt_StringVar*);
static int StringVar_SetValue(udt_StringVar*, unsigned, PyObject*);
static PyObject *StringVar_GetValue(udt_StringVar*, unsigned);

#if PY_MAJOR_VERSION < 3
static int StringVar_PostDefine(udt_StringVar*);
#endif

static udint4 StringVar_GetBufferSize(udt_StringVar*);
static int StringVar_BindObjectValue(udt_StringVar*, unsigned, dhobj, udint4);

//-----------------------------------------------------------------------------
// Python type declarations
//-----------------------------------------------------------------------------
PyTypeObject g_StringType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.STRING",                  // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};

#if PY_MAJOR_VERSION < 3
PyTypeObject g_UnicodeStrType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.UNICODE_STRING",          // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};
#endif

PyTypeObject g_FixedCharType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.FIXED_STRING",            // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};

#if PY_MAJOR_VERSION < 3
PyTypeObject g_FixedUnicodeCharType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.FIXED_UNICODE_STRING",    // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};
#endif

PyTypeObject g_BinaryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.BINARY",               // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};

PyTypeObject g_FixedBinaryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "dmPython.FIXED_BINARY",            // tp_name
    sizeof(udt_StringVar),              // tp_basicsize
    0,                                  // tp_itemsize
    0,                                  // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    0,                                  // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT,                 // tp_flags
    0                                   // tp_doc
};

/*
static PyTypeObject g_RowidVarType = {
PyVarObject_HEAD_INIT(NULL, 0)
"cx_Oracle.ROWID",                  // tp_name
sizeof(udt_StringVar),              // tp_basicsize
0,                                  // tp_itemsize
0,                                  // tp_dealloc
0,                                  // tp_print
0,                                  // tp_getattr
0,                                  // tp_setattr
0,                                  // tp_compare
0,                                  // tp_repr
0,                                  // tp_as_number
0,                                  // tp_as_sequence
0,                                  // tp_as_mapping
0,                                  // tp_hash
0,                                  // tp_call
0,                                  // tp_str
0,                                  // tp_getattro
0,                                  // tp_setattro
0,                                  // tp_as_buffer
Py_TPFLAGS_DEFAULT,                 // tp_flags
0                                   // tp_doc
};
*/

//-----------------------------------------------------------------------------
// variable type declarations
//-----------------------------------------------------------------------------
udt_VariableType vt_String = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) StringVar_GetBufferSize,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_StringType,                      // Python type
    DSQL_C_NCHAR,                       // c type
    MAX_STRING_CHARS,                   // element length (default)
    1,                                  // is character data
    1,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};

#if PY_MAJOR_VERSION < 3
udt_VariableType vt_UnicodeString = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,    
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) StringVar_GetBufferSize,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_UnicodeStrType,                  // Python type
    DSQL_C_NCHAR,
    MAX_STRING_CHARS,                   // element length (default)
    1,                                  // is character data
    1,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};
#endif

udt_VariableType vt_FixedChar = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,    
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) StringVar_GetBufferSize,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_FixedCharType,                   // Python type
    DSQL_C_NCHAR,                       // c type
    2000,                               // element length (default)
    1,                                  // is character data
    0,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};

#if PY_MAJOR_VERSION < 3
udt_VariableType vt_FixedUnicodeChar = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,    
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) StringVar_GetBufferSize,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_FixedUnicodeCharType,            // Python type
    DSQL_C_NCHAR,
    2000,                               // element length (default)
    1,                                  // is character data
    0,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};
#endif

/*
static udt_VariableType vt_Rowid = {
    (InitializeProc) StringVar_Initialize,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,
    (PostDefineProc) NULL,
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) StringVar_GetBufferSize,
    &g_RowidVarType,                    // Python type
    SQLT_CHR,                           // Oracle type
    SQLCS_IMPLICIT,                     // charset form
    18,                                 // element length (default)
    1,                                  // is character data
    0,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};*/

udt_VariableType vt_Binary = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) NULL,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_BinaryType,                      // Python type
    DSQL_C_BINARY,                      // c type
    MAX_BINARY_BYTES,                   // element length (default)
    0,                                  // is character data
    1,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};

udt_VariableType vt_FixedBinary = {
    (InitializeProc) NULL,
    (FinalizeProc) NULL,
    (PreDefineProc) NULL,
    (PreFetchProc) NULL,
    (IsNullProc) NULL,
    (SetValueProc) StringVar_SetValue,
    (GetValueProc) StringVar_GetValue,
    (GetBufferSizeProc) NULL,
    (BindObjectValueProc)StringVar_BindObjectValue,
    &g_FixedBinaryType,                 // Python type
    DSQL_C_BINARY,                      // c type
    2000,                               // element length (default)
    0,                                  // is character data
    0,                                  // is variable length
    1,                                  // can be copied
    1                                   // can be in array
};

//-----------------------------------------------------------------------------
// StringVar_Initialize()
//   Initialize the variable. variable_new里面会有alloc
//-----------------------------------------------------------------------------
/*
static 
int 
StringVar_Initialize(
    udt_StringVar*  var                 // variable to initialize
)                 
{  
    var->actualLength = (slength*) PyMem_Malloc(var->allocatedElements * sizeof(slength));
    if (!var->actualLength) 
    {
        PyErr_NoMemory();
        return -1;
    }

    return 0;
}
*/

//-----------------------------------------------------------------------------
// StringVar_SetValue()
//   Set the value of the variable.
//-----------------------------------------------------------------------------
static 
int 
StringVar_SetValue(
    udt_StringVar*      var,                // variable to set value for
    unsigned            pos,                // array position to set
    PyObject*           value               // value to set
)
{
    udt_Buffer          buffer;

    // populate the buffer and confirm the maximum size is not exceeded
    if (dmBuffer_FromObject(&buffer, value, var->environment->encoding) < 0)
        return -1;

    if (var->type->isCharacterData && buffer.numCharacters > MAX_STRING_CHARS) 
    {
        dmBuffer_Clear(&buffer);
        PyErr_SetString(PyExc_ValueError, "string data too large");
        
        return -1;
    } 
    else if (!var->type->isCharacterData && buffer.size > MAX_BINARY_BYTES) 
    {
        dmBuffer_Clear(&buffer);
        PyErr_SetString(PyExc_ValueError, "binary data too large");

        return -1;
    }

    // ensure that the buffer is large enough
    if (buffer.size > (Py_ssize_t)var->bufferSize) 
    {
        if (Variable_Resize( (udt_Variable*) var, buffer.numCharacters) < 0) 
        {
            dmBuffer_Clear(&buffer);
            return -1;
        }
    }

    // keep a copy of the string
    var->indicator[pos]     = (slength)buffer.size;
    var->actualLength[pos]  = (slength)buffer.size;

    if (buffer.size)
    {
        memcpy(var->data + var->bufferSize * pos, buffer.ptr, buffer.size);
    }

    dmBuffer_Clear(&buffer);

    return 0;
}


//-----------------------------------------------------------------------------
// StringVar_GetValue()
//   Returns the value stored at the given array position.
//-----------------------------------------------------------------------------
static 
PyObject*
StringVar_GetValue(
    udt_StringVar*      var,                // variable to determine value for
    unsigned            pos                 // array position
)                       
{
    char*               data;

    data = var->data + pos * var->bufferSize;

    if (var->type == &vt_Binary || var->type == &vt_FixedBinary)
        return PyBytes_FromStringAndSize(data, var->actualLength[pos]);

    /* 取到的值按照当前encoding直接返回，不管是否为unicode串 */
    return dmString_FromEncodedString(data, var->actualLength[pos], var->environment->encoding);
}

#if PY_MAJOR_VERSION < 3
//-----------------------------------------------------------------------------
// StringVar_PostDefine()
//   Set the character set information when values are fetched from this
// variable.
//-----------------------------------------------------------------------------
static 
int 
StringVar_PostDefine(
    udt_StringVar*  var
)
{
    /*
    sword status;

    //DSQL_ATTR_SQL_CHARSET

    status = OCIAttrSet(var->defineHandle, OCI_HTYPE_DEFINE,
        &var->type->charsetForm, 0, OCI_ATTR_CHARSET_FORM,
        var->environment->errorHandle);
    if (Environment_CheckForError(var->environment, status,
        "StringVar_PostDefine(): setting charset form") < 0)
        return -1;
    */

    return 0;
}
#endif


//-----------------------------------------------------------------------------
// StringVar_GetBufferSize()
//   Returns the buffer size to use for the variable.
//-----------------------------------------------------------------------------
static udint4 StringVar_GetBufferSize(
    udt_StringVar* self)                // variable to get buffer size for
{
    if (self->type->isCharacterData)
        return self->size * self->environment->maxBytesPerCharacter;

    return self->size;
}

static 
int 
StringVar_BindObjectValue(
    udt_StringVar*      var, 
    unsigned            pos, 
    dhobj               hobj,
    udint4              val_nth
)
{
    DPIRETURN       rt = DSQL_SUCCESS;

    rt      = dpi_set_obj_val(hobj, val_nth, var->type->cType, (dpointer)((sdbyte*)var->data + var->bufferSize * pos), var->indicator[pos]);
    if (Environment_CheckForError(var->environment, hobj, DSQL_HANDLE_OBJECT, rt, 
        "vCursor_BindObjectValue():dpi_set_obj_val") < 0)
    {
        return -1;
    }

    return 0;
}

