//-----------------------------------------------------------------------------
// Cursor.c
//   Definition of the Python type Cursor.
//-----------------------------------------------------------------------------
#include "py_Dameng.h"
#include "row.h"
#include "Error.h"
#include "Buffer.h"
#include "var_pub.h"
#include "stdio.h"
#include "trc.h"

#include <datetime.h>

static 
PyObject*
Cursor_GetDescription(
	udt_Cursor	*self,
    void*       args
);

PyObject*
Cursor_Execute_inner(
    udt_Cursor*     self, 
    PyObject*       statement,
    PyObject*       executeArgs,
    int             is_many,
    int             exec_direct,
    int             from_call
);

static
sdint2
Cursor_GetParamDescFromDm(
    udt_Cursor*     self
);

PyObject*
Cursor_MakeupProcParams(
	udt_Cursor*     self
);

static
void
Cursor_ExecRs_Clear(
    udt_Cursor*     self    // cursor to set the rowcount on
);

static 
sdint2
Cursor_SetRowCount(
    udt_Cursor*     self    // cursor to set the rowcount on
);

void
Cursor_Data_init()
{
	PyDateTime_IMPORT;
}

static 
void
Cursor_init_inner(
    udt_Cursor*     self
)
{
    Py_INCREF(Py_None);
    self->statement     = Py_None;

    Py_INCREF(Py_None);
    self->environment   = (udt_Environment*)Py_None;

    Py_INCREF(Py_None);
    self->connection    = (udt_Connection*)Py_None;

    Py_INCREF(Py_None);
    self->rowFactory    = Py_None;

    Py_INCREF(Py_None);
    self->inputTypeHandler  = Py_None;

    Py_INCREF(Py_None);
    self->outputTypeHandler = Py_None;

    Py_INCREF(Py_None);
    self->description       = Py_None;

    Py_INCREF(Py_None);
    self->map_name_to_index = Py_None;

    Py_INCREF(Py_None);
    self->column_names      = Py_None;

    Py_INCREF(Py_None);
    self->lastrowid_obj     = Py_None;

    self->totalRows         = -1;
    self->rowNum            = 0;
    self->with_rows         = 0;
    self->rowCount          = -1;

    self->col_variables     = NULL;
    self->param_variables   = NULL;
    self->execute_num       = 0;
}

static 
sdint2
Cursor_IsOpen_without_err(
    udt_Cursor*     self
)
{
    if (self->isOpen <= 0)
    {
        return -1;
    }

    return 0;
}

static 
sdint2
Cursor_IsOpen(
    udt_Cursor*     self
)
{
	if (Cursor_IsOpen_without_err(self) < 0){
		PyErr_SetString(g_InternalErrorException, "Not Open");
		return -1;
	}

	return 0;
}

sdint2
Cursor_AllocHandle(
    udt_Cursor*     self
)
{
    DPIRETURN		rt = DSQL_SUCCESS;
    dhstmt			hstmt;	

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_alloc_stmt(self->connection->hcon, &hstmt);	
        rt = dpi_set_stmt_attr(hstmt, DSQL_ATTR_CURSOR_TYPE, (dpointer)DSQL_CURSOR_STATIC, 0);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->connection->hcon, DSQL_HANDLE_DBC, rt, "Cursor_Init():dpi_alloc_stmt") < 0)
            return -1;	

    self->handle    = hstmt;
    return 0;
}

PyObject*
Cursor_New(    
    udt_Connection*     connection
)
{
    udt_Cursor*         self;
    
    self                = (udt_Cursor*)g_CursorType.tp_alloc(&g_CursorType, 0);
    if (self == NULL)
        return NULL;    

    Cursor_init_inner(self);

    Py_INCREF(connection);
    self->connection    = connection;

    Py_INCREF(connection->environment);
    self->environment   = connection->environment;

    // ????????????	
    if (Cursor_AllocHandle(self) < 0)
    {
        Cursor_free_inner(self);
        Py_TYPE(self)->tp_free((PyObject*) self);

        return NULL;
    }
    
    self->execute_num   = 0;
    self->arraySize     = 50;
    self->org_arraySize = self->arraySize;
    self->bindArraySize = 1;    
    self->org_bindArraySize = self->bindArraySize;
    self->statementType = -1;
    self->outputSize    = -1;
    self->outputSizeColumn = -1;
    self->isOpen        = 1;
    self->isClosed      = 0;

    self->bindColDesc   = NULL;
    self->bindParamDesc = NULL;
    self->paramCount    = 0;
    self->colCount      = 0;
    self->rowNum        = 0;

    self->is_iter       = 0;
    
    return (PyObject*)self;
}

static 
PyObject*
Cursor_Repr(
    udt_Cursor*     cursor
)
{
	PyObject *connectionRepr, *module, *name, *result, *format, *formatArgs;

    format = dmString_FromAscii("<%s.%s on %s>");
    if (!format)
        return NULL;

    connectionRepr = PyObject_Repr((PyObject*) cursor->connection);
    if (!connectionRepr) {
        Py_DECREF(format);
        return NULL;
    }

    if (GetModuleAndName(Py_TYPE(cursor), &module, &name) < 0) {
        Py_DECREF(format);
        Py_DECREF(connectionRepr);
        return NULL;
    }

    formatArgs = PyTuple_Pack(3, module, name, connectionRepr);
    Py_DECREF(module);
    Py_DECREF(name);
    Py_DECREF(connectionRepr);
    if (!formatArgs) {
        Py_DECREF(format);
        return NULL;
    }

    result = PyUnicode_Format(format, formatArgs);
    Py_DECREF(format);
    Py_DECREF(formatArgs);
    return result;
}


//-----------------------------------------------------------------------------
// Cursor_FreeHandle()
//   Free the handle 
//-----------------------------------------------------------------------------
sdint2 
Cursor_FreeHandle(
    udt_Cursor*     self,       // cursor object
	int             raiseException      // raise an exception, if necesary?
)
{
    DPIRETURN   rt = DSQL_SUCCESS;

	if (self->handle) 
    {
		Py_BEGIN_ALLOW_THREADS
			rt = dpi_free_handle(DSQL_HANDLE_STMT, self->handle);
		Py_END_ALLOW_THREADS
		if (raiseException && 
			Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, "Cursor_FreeHandle():cursor free") < 0)
			return -1;
	}

    self->handle = NULL;
	return  0;
}

void
Cursor_free_paramdesc(
	udt_Cursor*     self
)
{	    
    self->hdesc_param   = NULL;

	if (self->bindParamDesc != NULL)
    {        
		PyMem_Free(self->bindParamDesc);
	}
    self->bindParamDesc = NULL;  

    self->paramCount    = 0;
}

void
Cursor_free_coldesc(
	udt_Cursor*     self
)
{	
    self->hdesc_col = NULL;

	if (self->bindColDesc != NULL)
	{		
		PyMem_Free(self->bindColDesc);
	}
	self->bindColDesc = NULL;    
}

void
Cursor_free_inner(
    udt_Cursor*     self
)
{
    Cursor_free_paramdesc(self);
    Cursor_free_coldesc(self);

    Py_CLEAR(self->statement);
    Py_DECREF(self->environment);
    Py_DECREF(self->connection);      
    Py_CLEAR(self->rowFactory);    
    Py_CLEAR(self->inputTypeHandler);    
    Py_CLEAR(self->outputTypeHandler);    
    Py_CLEAR(self->description);    
    Py_CLEAR(self->map_name_to_index);
    Py_CLEAR(self->column_names);
    Py_CLEAR(self->param_variables);
    Py_CLEAR(self->col_variables);
    Py_CLEAR(self->lastrowid_obj);
}

sdint2
Cursor_InternalClose(
	udt_Cursor*     self
)
{
	Py_BEGIN_ALLOW_THREADS	
	dpi_close_cursor(self->handle);
	Py_END_ALLOW_THREADS

	return 0;
}

static 
PyObject*
Cursor_Close_inner(
    udt_Cursor*     self
)
{
    /** ????????????Cursor_Close???????? **/
	if (Cursor_IsOpen(self) < 0)
    {
		PyErr_Clear();

        Py_RETURN_NONE;
    }

    /** ???????????????????????????????? **/
    if (self->connection->isConnected == 1)
    {
        Cursor_InternalClose(self);

        Cursor_FreeHandle(self, 0);
    }	

    /** ????Cursor???????????? **/
    Cursor_free_inner(self);

    Cursor_init_inner(self);

	self->isOpen = 0;
    self->isClosed = 1;

	Py_INCREF(Py_None);
	return Py_None;
}

static 
PyObject*
Cursor_Close(
    udt_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_Close\n"));

    retObj      = Cursor_Close_inner(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_Close, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
void
Cursor_Free(
    udt_Cursor*     self
)
{	
    if (Cursor_IsOpen_without_err(self) >= 0)    
        Cursor_Close(self);    

    Cursor_free_inner(self);

	Py_TYPE(self)->tp_free((PyObject*) self);
}

static 
sdint2
Cursor_IsDDL(
    sdint2      stmtType
)
{
	switch(stmtType){
		case DSQL_DIAG_FUNC_CODE_CREATE_TAB:
		case DSQL_DIAG_FUNC_CODE_DROP_TAB:
		case DSQL_DIAG_FUNC_CODE_CREATE_VIEW:
		case DSQL_DIAG_FUNC_CODE_DROP_VIEW:
		case DSQL_DIAG_FUNC_CODE_CREATE_INDEX:
		case DSQL_DIAG_FUNC_CODE_DROP_INDEX:
		case DSQL_DIAG_FUNC_CODE_CREATE_USER:
		case DSQL_DIAG_FUNC_CODE_DROP_USER:
		case DSQL_DIAG_FUNC_CODE_CREATE_ROLE:
		case DSQL_DIAG_FUNC_CODE_DROP_ROLE:
		case DSQL_DIAG_FUNC_CODE_DROP:
		case DSQL_DIAG_FUNC_CODE_CREATE_SCHEMA:
		case DSQL_DIAG_FUNC_CODE_CREATE_CONTEXT_INDEX:
		case DSQL_DIAG_FUNC_CODE_DROP_CONTEXT_INDEX:
		case DSQL_DIAG_FUNC_CODE_CREATE_LINK:
			return 0;
	}

	return -1;
}

//-----------------------------------------------------------------------------
// Cursor_GetStatementType()
//   Determine if the cursor is executing a select statement.
//-----------------------------------------------------------------------------
static 
sdint2 
Cursor_GetStatementType(
    udt_Cursor *self        // cursor to perform binds on
)
{
	sdint4      statementType;
	slength     len;
	DPIRETURN   status = DSQL_SUCCESS;

	Py_BEGIN_ALLOW_THREADS
	status = dpi_get_diag_field(DSQL_HANDLE_STMT, self->handle, 0, 
		DSQL_DIAG_DYNAMIC_FUNCTION_CODE, (dpointer) &statementType, 0, &len);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_GetStatementType()") < 0)
	{
		return -1;
	}

	self->statementType = statementType;

    Py_CLEAR(self->col_variables);

	return 0;
}

/* ??????????????prepare???????????????? */
static
sdint2
Cursor_hasPrepared(
    udt_Cursor*     self,               // cursor to perform prepare on
    PyObject**      statement,
    udt_Buffer*     buffer,
    int             direct_flag
)
{
    /* ??????????????????????????prepare */
    if ((*statement == Py_None) && 
        (self->statement == NULL || self->statement == Py_None)) 
    {
        PyErr_SetString(g_ProgrammingErrorException,
            "no statement specified and no prior statement prepared");

        return -1;
    }    

    /* ??????DDL????????????????prepare, executedirect????????prepare?????????????????????????? */
    if (*statement == Py_None || *statement == self->statement)        
    {
        if(!direct_flag && Cursor_IsDDL (self->statementType) < 0)
            return 1;

        //Py_INCREF(self->statement);
        *statement = self->statement;
    }

    if (dmBuffer_FromObject(buffer, *statement, self->environment->encoding) < 0)
    {
        //Py_XDECREF(*statement);		
        return -1;
    }

    /* ??????????0 */    
    if (strlen((char*)buffer->ptr) == 0)
    {
        PyErr_SetString(g_ProgrammingErrorException,
            "no statement specified and no prior statement prepared");

        dmBuffer_Clear(buffer); 
        return -1;
    }

    Py_CLEAR(self->statement);        
    return 0;
}

static
void
Cursor_clearDescExecInfo(
    udt_Cursor*     self,
    int             clear_param
)
{
    /* ???????? */
    Cursor_InternalClose(self);

    /* ???????????????? */
    if (clear_param)
    {
        Cursor_free_paramdesc(self);

        if (self->param_variables != NULL)
        {
            Py_CLEAR(self->param_variables);
            self->param_variables   = NULL;
        }
    }

    /* ?????????????? */
    Cursor_free_coldesc(self);

    /** ???????????????? **/
    Cursor_ExecRs_Clear(self);
}

//-----------------------------------------------------------------------------
// Cursor_InternalPrepare()
//   Internal method for preparing a statement for execution.
//-----------------------------------------------------------------------------
static 
sdint2 
Cursor_InternalPrepare(
    udt_Cursor*     self,               // cursor to perform prepare on
    PyObject*       statement           // statement to prepare    
)
{
    udt_Buffer      statementBuffer;  
    DPIRETURN       status = DSQL_SUCCESS;
    sdint2          ret;

    /* ????????????????prepare???????????? */
    ret = Cursor_hasPrepared(self, &statement, &statementBuffer, 0);
    if (ret != 0)
        return ret;

    /* ???????????????????????? */
    Cursor_clearDescExecInfo(self, 1);

	// prepare statement
    Py_BEGIN_ALLOW_THREADS
        //prepare??????????????????????????
        status = dpi_unbind_params(self->handle);
        status = dpi_prepare(self->handle, (sdbyte*)statementBuffer.ptr);
    Py_END_ALLOW_THREADS

    dmBuffer_Clear(&statementBuffer);    
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
            "Cursor_InternalPrepare(): prepare") < 0) 
	{
        return -1;
	}

    // clear bind variables, if applicable
    if (!self->setInputSizes) 
    {
        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    // clear row factory, if spplicable
    Py_XDECREF(self->rowFactory);
    self->rowFactory = NULL;

    // determine if statement is a query
    if (Cursor_GetStatementType(self) < 0)
        return -1;	

    /* ??????????????????????cursor.prepare??execute????prepare?????????????????? */
    if (Cursor_GetParamDescFromDm(self) < 0)
        return -1;

    Py_INCREF(statement);
    self->statement     = statement;

    return 0;
}

static 
sdint2 
Cursor_InternalExecDirect(
    udt_Cursor*     self,               // cursor to perform prepare on
    PyObject*       statement           // statement to prepare      
)
{
    udt_Buffer      statementBuffer;
    DPIRETURN       status = DSQL_SUCCESS;

    /* dpi_exec_direct??????????prepare???????????????????????????? */
    if (Cursor_hasPrepared(self, &statement, &statementBuffer, 1) < 0)
        return -1;

    /* ???????????????????????? */
    Cursor_clearDescExecInfo(self, 1);

    // prepare statement
    Py_BEGIN_ALLOW_THREADS
        status = dpi_exec_direct(self->handle, (sdbyte*)statementBuffer.ptr);
    Py_END_ALLOW_THREADS

    dmBuffer_Clear(&statementBuffer);
    
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
            "Cursor_InternalPrepare(): prepare") < 0) 
    {
        return -1;
    }

    // clear bind variables, if applicable
    if (!self->setInputSizes) 
    {
        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    // clear row factory, if spplicable
    Py_XDECREF(self->rowFactory);
    self->rowFactory = NULL;

    // determine if statement is a query
    if (Cursor_GetStatementType(self) < 0)
        return -1;	

    /* ???????????????? */
    if (Cursor_SetRowCount(self) < 0)
        return -1;

    Py_INCREF(statement);
    self->statement     = statement;

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_ExecRs_Clear()
//   ????????????????????
//-----------------------------------------------------------------------------
static
void
Cursor_ExecRs_Clear(
    udt_Cursor*     self    // cursor to set the rowcount on
)
{
    // ????????????????????????????
    if (self->description != Py_None)
    {
        Py_CLEAR(self->description);

        Py_INCREF(Py_None);
        self->description = Py_None;		
    }

    if (self->map_name_to_index != Py_None)
    {
        Py_CLEAR(self->map_name_to_index);

        Py_INCREF(Py_None);
        self->map_name_to_index = Py_None;
    }

    if (self->column_names != Py_None)
    {
        Py_CLEAR(self->column_names);

        Py_INCREF(Py_None);
        self->column_names  = Py_None;
    }

    self->colCount  = 0;
    self->rowNum    = 0;
    self->rowCount  = -1;
    self->with_rows = 0;
}

//-----------------------------------------------------------------------------
// Cursor_SetRowCount()
//   Set the rowcount variable.
//-----------------------------------------------------------------------------
static 
sdint2
Cursor_SetRowCount(
    udt_Cursor*     self    // cursor to set the rowcount on
)
{
	sdint8      rowCount;
	DPIRETURN   status = DSQL_SUCCESS; 
    sdint8      lastrowid;

    if (self->statementType == DSQL_DIAG_FUNC_CODE_SELECT||
        self->statementType == DSQL_DIAG_FUNC_CODE_CALL) {		
		self->rowCount = 0;				
		// ????????fetch????????????????????????
		self->actualRows    = -1;

		Py_BEGIN_ALLOW_THREADS
			status = dpi_row_count(self->handle, &rowCount);
		Py_END_ALLOW_THREADS

		if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
			"Cursor_SetRowCount()") < 0)
        {
			return -1;
        }

		self->totalRows = (slength)rowCount;	

        /** ???????????????????? **/
        if (self->totalRows > 0)
        {
            self->with_rows = 1;
        }

	} else if (self->statementType == DSQL_DIAG_FUNC_CODE_INSERT ||
		self->statementType == DSQL_DIAG_FUNC_CODE_UPDATE ||
		self->statementType == DSQL_DIAG_FUNC_CODE_DELETE ) {
			Py_BEGIN_ALLOW_THREADS
				status = dpi_row_count(self->handle, &rowCount);
			Py_END_ALLOW_THREADS
			if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
				"Cursor_SetRowCount()") < 0)
			{
				return -1;
			}

			self->totalRows = (slength)rowCount;
	} else {
		self->totalRows     = -1;
	}

    /** ????????lastrowid **/
    Py_DECREF(self->lastrowid_obj);
    if (self->statementType == DSQL_DIAG_FUNC_CODE_INSERT ||
        self->statementType == DSQL_DIAG_FUNC_CODE_UPDATE ||
        self->statementType == DSQL_DIAG_FUNC_CODE_DELETE )
    {
        Py_BEGIN_ALLOW_THREADS
            status = dpi_get_diag_field(DSQL_HANDLE_STMT, self->handle, 0, DSQL_DIAG_ROWID, (dpointer)&lastrowid, 0, NULL);
        Py_END_ALLOW_THREADS
            if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                "Cursor_SetRowCount()") < 0)
            {
                return -1;
            }

            self->lastrowid_obj = Py_BuildValue("l", lastrowid);
    }
    else
    {
        Py_INCREF(Py_None);
        self->lastrowid_obj     = Py_None;
    }

	return 0;
}

static
sdint2
Cursor_PutDataVariable_onerow(
    udt_Cursor*     self,
    Py_ssize_t      irow
)
{
    udint4          i;
    udt_Variable*   var;

    for (i = 0; i < self->paramCount; i ++)
    {
        var     = (udt_Variable*)PyList_GET_ITEM(self->param_variables, i);

        if (Variable_PutDataAftExec(var, irow) < 0)
        {
            return -1;
        }
    }

    return 0;
}

static
sdint2
Cursor_PutDataVariable(
    udt_Cursor*     self,
    Py_ssize_t      rowsize
)
{    
    int             rt = 0;
    Py_ssize_t      i;

    /* dpi_param_data????????????????????put_data??????????????put???????????????????????????? */
    for (i = 0; i < rowsize; i ++)
    {
        rt      = Cursor_PutDataVariable_onerow(self, i);
        if (rt < 0)
        {
            return rt;
        }
    }

    rt          = dpi_param_data(self->handle, NULL);

    /* ????0?????????????????????????????? */
    if (rt == DSQL_SUCCESS)
        return 0;

    /* ????????????????DSQL_NEED_DATA???????? */
    if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
        "vLong_PutData():dpi_param_data") < 0)
    {
        return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_InternalExecute()
//   Perform the work of executing a cursor and set the rowcount appropriately
// regardless of whether an error takes place.
//-----------------------------------------------------------------------------
static 
sdint2
Cursor_InternalExecute(
	udt_Cursor*     self,
    Py_ssize_t      rowsize
)
{
	DPIRETURN 		status = DSQL_SUCCESS;
    sdint2          ret;

    Cursor_clearDescExecInfo(self, 0);

	Py_BEGIN_ALLOW_THREADS
		status = dpi_exec(self->handle);
	Py_END_ALLOW_THREADS

    /* ??NEED_DATA????long string/binary?????????????? */
    if (status == DSQL_NEED_DATA)
    {
        if (Cursor_PutDataVariable(self, rowsize) < 0)
        {
            return -1;        
        }
    }
    else
    {
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status, 
            "Cursor_InternalExecute()") < 0)
        {
            return -1;
        }
    }

    ret     = Cursor_SetRowCount(self);

    //????????????????unbind param
    if (self->paramCount > 0)
    {
        Py_BEGIN_ALLOW_THREADS
            status = dpi_unbind_params(self->handle);
        Py_END_ALLOW_THREADS
    }

    return ret;
}

static
sdint2
Cursor_GetColDescFromDm_low(
    udt_Cursor*     self,
    dhdesc          hdesc_col
)
{    
    DPIRETURN   rt = DSQL_SUCCESS;
    udint2      icol;
    sdint4      val_len;

    self->bindColDesc = PyMem_Malloc(self->colCount * sizeof(DmColDesc));
    if (self->bindColDesc == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }
    memset(self->bindColDesc, 0, self->colCount * sizeof(DmColDesc));    

    for (icol = 0; icol < self->colCount; icol ++)
    {
        rt  = dpi_desc_column(self->handle, icol + 1, 
                              self->bindColDesc[icol].name, sizeof(self->bindColDesc[icol].name), &self->bindColDesc[icol].nameLen,
                              &self->bindColDesc[icol].sql_type, &self->bindColDesc[icol].prec, 
                              &self->bindColDesc[icol].scale, &self->bindColDesc[icol].nullable);
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_desc_column") < 0)
        {
            return -1;		
        }

        rt  = dpi_get_desc_field(hdesc_col, icol + 1, DSQL_DESC_DISPLAY_SIZE, (dpointer)&self->bindColDesc[icol].display_size,
            0, &val_len);
        if (Environment_CheckForError(self->environment, hdesc_col, DSQL_HANDLE_DESC, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field[DSQL_DESC_DISPLAY_SIZE]") < 0)
        {
            return -1;		
        }        
    }

    return 0;
}

static
sdint2
Cursor_GetColDescFromDm(
    udt_Cursor*     self
)
{
    DPIRETURN   rt = DSQL_SUCCESS;
    sdint4      val_len;

    /* ?????????????? */    
    Py_BEGIN_ALLOW_THREADS
        rt  = dpi_get_stmt_attr(self->handle, DSQL_ATTR_IMP_ROW_DESC, (dpointer)&self->hdesc_col, 0, &val_len);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
            "Cursor_GetColDescFromDm():dpi_get_stmt_attr") < 0)
            return -1;	    

    return Cursor_GetColDescFromDm_low(self, self->hdesc_col);        
}

static 
sdint2
Cursor_SetColVariables(
    udt_Cursor*     self
)
{
    udint2          icol;
    udt_Variable*   udt_var;

    if ((int)self->arraySize < 0 || self->arraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ErrorException, "Invalid cursor arraysize\n");
        return -1;
    }

    Py_CLEAR(self->col_variables);

    self->col_variables = PyList_New(self->colCount);
    if (self->col_variables == NULL)
    {
        if (!PyErr_Occurred())
            PyErr_NoMemory();

        return -1;
    }

    for (icol = 0; icol < self->colCount; icol ++)
    {
        udt_var = Variable_Define(self, self->hdesc_col, icol + 1, self->arraySize);
        if (udt_var == NULL)
            return -1;

        if (PyList_SET_ITEM(self->col_variables, icol, (PyObject*)udt_var) < 0)
            return -1;
    }

    self->org_bindArraySize = self->bindArraySize;

    return 0;
}

static 
sdint2
Cursor_PerformDefine(
    udt_Cursor*     self,
    sdint2*         isQuery
)
{
	DPIRETURN status = DSQL_SUCCESS;	
	sdint2	i = 0;
    PyObject*       desc;

    if (isQuery)
    {
        *isQuery = 0;
    }

	// determine number of items in select-list
	Py_BEGIN_ALLOW_THREADS
	status = dpi_number_columns(self->handle, &self->colCount);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_PerformDefine()") < 0)
	{
		return -1;		
	}

    /* ????????0?? ?????????? */
    if (self->colCount == 0)
    {        
        return 0;
    }

    if (isQuery)
    {
        *isQuery = 1;
    }

    /** ???????????????? **/
    if (Cursor_GetColDescFromDm(self) < 0)
        return -1;    

    /** ?????????? **/
    if (Cursor_SetColVariables(self) < 0)
        return -1;

    /** ???????????? **/
    desc    = Cursor_GetDescription(self, NULL);
    if (desc == NULL)
        return -1;
    //Py_CLEAR(desc);
    Py_DECREF(desc);

	return 0;
}

static
sdint2
Cursor_GetParamDescFromDm_low(
    udt_Cursor*     self
)
{    
    DPIRETURN   rt = DSQL_SUCCESS;
    udint2      iparam;    

    self->bindParamDesc = PyMem_Malloc(self->paramCount * sizeof(DmParamDesc));
    if (self->bindParamDesc == NULL)
    {
        PyErr_NoMemory();
        return -1;
    }
    memset(self->bindParamDesc, 0, self->paramCount * sizeof(DmParamDesc));    

    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        rt  = dpi_desc_param(self->handle, iparam + 1, 
                             &self->bindParamDesc[iparam].sql_type, &self->bindParamDesc[iparam].prec, 
                             &self->bindParamDesc[iparam].scale, &self->bindParamDesc[iparam].nullable);
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_desc_param") < 0)
        {
            return -1;		
        }     

        rt  = dpi_get_desc_field(self->hdesc_param, iparam + 1, DSQL_DESC_PARAMETER_TYPE, 
                                 (dpointer)&self->bindParamDesc[iparam].param_type, 0, NULL);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field") < 0)
        {
            return -1;		
        }

        /* ????????????DSQL_DESC_NAME */
        rt  = dpi_get_desc_field(self->hdesc_param, iparam + 1, DSQL_DESC_NAME, 
            (dpointer)self->bindParamDesc[iparam].name, 128, &self->bindParamDesc[iparam].namelen);

        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_GetColDescFromDm():dpi_get_desc_field") < 0)
        {
            return -1;		
        }
    }

    return 0;
}

static
sdint2
Cursor_GetParamDescFromDm(
    udt_Cursor*     self
)
{
    DPIRETURN   rt = DSQL_SUCCESS;
    sdint4      val_len;    

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_number_params(self->handle, &self->paramCount);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Cursor_InternalPrepare(): dpi_number_params") < 0) 
            return -1;  

    if (self->paramCount <= 0)
    {
        return 0;
    }

    /** ???????????????? **/    
    Py_BEGIN_ALLOW_THREADS
        rt = dpi_get_stmt_attr(self->handle, DSQL_ATTR_IMP_PARAM_DESC, &self->hdesc_param, 0, &val_len);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt, 
            "Cursor_GetParamDescFromDm():dpi_get_stmt_attr") < 0)
            return -1;

    return Cursor_GetParamDescFromDm_low(self);
}

static
sdint2
Cursor_SetParamRowSize_Oper(
	udt_Cursor*     cursor,
	udint4			paramrowSize
)
{
	DPIRETURN		rt = DSQL_SUCCESS;

	Py_BEGIN_ALLOW_THREADS
		rt = dpi_set_stmt_attr(cursor->handle, DSQL_ATTR_PARAMSET_SIZE, (dpointer)paramrowSize, 0);
	Py_END_ALLOW_THREADS
	if (Environment_CheckForError(cursor->environment, cursor->handle, DSQL_HANDLE_STMT, rt,
		"Cursor_SetParamRowSize_Oper():dpi_set_stmt_attr") < 0)
		return -1;

	return 0;
}

static
sdint2
Cursor_setParamVariablesHelper(
    udt_Cursor*     self,
    PyObject*       iValue,
    unsigned        numElements, 
    unsigned        irow,   /* ????????????????0-based */
    unsigned        ipos,   /* ????????????????1-based */
    udt_Variable*   org_var,
    udt_Variable**  new_var
)
{
    udt_Variable*   udt_var = NULL;    
    int             is_udt = 0;    

    *new_var    = NULL;
    is_udt      = Variable_Check(iValue); 

    /** ??????????????None???????????????????????????????????????????????????? **/
    if (org_var != NULL)
    {
        /** ????????????????;??????????????????org_var?? **/
        if (is_udt == 1)
        {
            /** ???????????????????????????? **/
            if ((PyObject*)org_var != iValue)
            {
                Py_INCREF(iValue);
                *new_var    = (udt_Variable*)iValue;                
            }            
        }
        else if (numElements > ((udt_Variable*)org_var)->allocatedElements)
        {
            *new_var    = Variable_NewByVarType(self, org_var->type, numElements, org_var->size);
            if (!*new_var)
                return -1;

            if (Variable_SetValue(*new_var, irow, iValue) < 0)
                return -1;
        }
        else if (Variable_SetValue(org_var, irow, iValue) < 0)
        {
            // executemany() should simply fail after the first element
            if (irow > 0)
                return -1;

            // anything other than index error or type error should fail
            if (!PyErr_ExceptionMatches(PyExc_IndexError) &&
                !PyErr_ExceptionMatches(PyExc_TypeError))
                return -1;

            // clear the exception and try to create a new variable
            PyErr_Clear();
            org_var = NULL;
        }        
    }    

    /** ???????????????????????? **/
    if (org_var == NULL)
    {
        /** ??????????????????????????;??????????Python????????udt????**/
        if (is_udt)
        {
            Py_INCREF(iValue);

            udt_var             = (udt_Variable*)iValue;
            udt_var->boundPos   = 0;
        }
        else
        {
            udt_var             = Variable_NewByValue(self, iValue, numElements, ipos);
            if (udt_var == NULL)
                return -1;

            if (Variable_SetValue(udt_var, irow, iValue) < 0)
                return -1;
        }

        (*new_var)  = udt_var;
    }

    return 0;
}

/** ??????????????????dict???????????????? **/
static
PyObject*
Cursor_getParamValue_FromDict(
    udt_Cursor*     self,
    PyObject*       dict,
    PyObject*       dickKeys,
    int             iparam
)
{
    PyObject*       iValue = Py_None;
    PyObject*       keyObj;
    Py_ssize_t      key_num;
    Py_ssize_t      key_i;
    char*           strvalue;

    iValue      = PyDict_GetItemString(dict, self->bindParamDesc[iparam].name);
    if (iValue != NULL)
        return iValue;

    iValue      = Py_None;
    key_num     = PyList_GET_SIZE(dickKeys);
    for (key_i = 0; key_i < key_num; key_i ++)
    {
        keyObj      = PyList_GetItem(dickKeys, key_i); 
        if (keyObj == NULL)
            return NULL;

        strvalue    = py_String_asString(keyObj);
#ifdef WIN32
        if (stricmp(strvalue, self->bindParamDesc[iparam].name) == 0)
#else        
        if (strcasecmp(strvalue, self->bindParamDesc[iparam].name) == 0)
#endif        
        {
            iValue  = PyDict_GetItemString(dict, strvalue);
            if (iValue == NULL)
            {
                PyErr_SetString(PyExc_ValueError, 
                    "Error occurs in dict to be bound");
                return NULL;
            }

            break;
        }
    }

    return iValue;
}   

/** ?????????????????????????????? **/
static
sdint2
Cursor_setParamVariables_oneRow(
    udt_Cursor*     self,
    PyObject*       parameters,
    Py_ssize_t      irow,
    Py_ssize_t      n_row
)
{
    sdint2          ret = -1;
    int             boundByPos = 0;
    int             iparam;    
    Py_ssize_t      param_num = 0;
    PyObject*       iValue;    
    PyObject*       dictKeys = NULL;
    udt_Variable*       new_var = NULL;
    udt_VariableType*   new_varType = NULL;     //????????
    udt_VariableType*   tmp_varType = NULL;     //????????
    udint4              size;
    int                 is_udt;
    
    /** ?????????????????????? **/
    if (parameters != NULL && parameters != Py_None)
    {
        if (PySequence_Check(parameters))
        {
            param_num   = PySequence_Size(parameters);        
            boundByPos  = 1;
        }
        else if (PyDict_Check(parameters))
        {
            param_num   = PyDict_Size(parameters);
            boundByPos  = 0;
            dictKeys    = PyDict_Keys(parameters);
        }
        else
        {
            PyErr_SetString(g_ProgrammingErrorException, 
                "only bound by Position or Name supported.");

            return -1;
        }            
    }    
    
    /* ????param_variables???????????????????????????????????????????? */    
    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {       
        iValue  = Py_None;

        if (param_num > 0)
        {
            if (!boundByPos)
            {           
                iValue  = Cursor_getParamValue_FromDict(self, parameters, dictKeys, iparam);
                if (iValue == NULL)
                    goto fun_end;
            }
            else if (iparam < param_num)
            {
                iValue  = PySequence_GetItem(parameters, iparam);
                if (!iValue)
                    goto fun_end;
                Py_DECREF(iValue);
            }
        }

        /* ????Py_None?????????? */
        if (iValue == Py_None)
        {
            continue;
        }

        /* ???????????????????? */
        is_udt  = Variable_Check(iValue); 

        /* prepare????????????????param_variables?????????????????????????? */
        new_var = PyList_GET_ITEM(self->param_variables, iparam);

        /* ??????????????????????None??????????????????????????????????List?? */
        if (new_var == NULL)
        {
            new_varType = Variable_TypeByValue(iValue, &size);
            if (new_varType == NULL)
                goto fun_end;

            /* ??????????????????????????????????????????????????LongString????LongBinary???????????? */
            if (self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT_OUTPUT ||
                self->bindParamDesc[iparam].param_type == DSQL_PARAM_OUTPUT ||
                self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT)
            {
                tmp_varType = Variable_TypeBySQLType(self->bindParamDesc[iparam].sql_type, 1);
                if (tmp_varType == NULL)
                {
                    goto fun_end;
                }

                if (new_varType == &vt_String || new_varType == &vt_Binary)
                {
                    if (new_varType == tmp_varType ||
                        tmp_varType == &vt_LongString ||tmp_varType == &vt_LongBinary)
                    {
                        new_varType = tmp_varType;
                        size    = -1;
                    }                
                }            
            }

            new_var     = Variable_NewByVarType(self, new_varType, n_row, size);
            if (new_var == NULL)
            {
                goto fun_end;
            }

            /** ???????????????????????????????? **/
            if (new_var->type->pythonType == &g_ObjectVarType &&
                ObjectVar_GetParamDescAndObjHandles((udt_ObjectVar*)new_var, self->hdesc_param, iparam + 1) < 0)
            {
                goto fun_end;
            }

            /* ???????????????????????? */
            if (Variable_SetValue(new_var, irow, iValue) < 0)
            {
                goto fun_end;
            }

            /* ???????????????????????????????????????????? */
            PyList_SetItem(self->param_variables, iparam, new_var);

            continue;
        }
        
        /* ?????????????????????????????????????????????????????????? */
        if (Variable_SetValue(new_var, irow, iValue) < 0)
        {
            goto fun_end;
        }                                   
    }    
    
    ret     = 0;

fun_end:
    Py_XDECREF(dictKeys);

    return ret;
}

static
sdint2
Cursor_setParamVariables(
    udt_Cursor*     self,
    PyObject*       parameters,
    int             is_many,
    Py_ssize_t*     prow_size
)
{
    int                 boundByPos;
    PyObject*           tmp_param = NULL;    
    Py_ssize_t          irow;
    int                 iparam;    
    udt_Variable*       new_var = NULL;
    udt_VariableType*   new_varType;    

    if (is_many)
    {
        if (parameters == NULL || parameters == Py_None || PyDict_Check(parameters))
        {
            PyErr_SetString(g_InterfaceErrorException, "expecting a sequence of parameters");

            return -1;
        }

        /** ?????????????????????? **/
        boundByPos      = PySequence_Check(parameters);
        if (boundByPos == 0)
        {
            PyErr_SetString(g_ProgrammingErrorException, 
                "only bound by Position supported.");
            
            return -1;
        }

        // ensure that input sizes are reset
        // this is done before binding is attempted so that if binding fails and
        // a new statement is prepared, the bind variables will be reset and
        // spurious errors will not occur
        self->setInputSizes         = 0;
    }

    /** ???????????????????????????????????????????? **/
    if (is_many == 0)
        *prow_size          = 1;
    else
        *prow_size          = PySequence_Size(parameters);

    Py_CLEAR(self->param_variables);

    self->param_variables   = PyList_New(self->paramCount);
    if (self->param_variables == NULL)
    {
        return -1; 
    }

    /* ????????????????????????????NULL */
    for (irow = 0; irow < *prow_size; irow ++)
    {
        /* ???????????? */
        if (irow == 0 && is_many == 0)
        {
            tmp_param   = parameters;
        }
        else /* ???????????????? */
        {
            tmp_param   = PySequence_GetItem(parameters, irow);
            Py_DECREF(tmp_param);
        }        

        if (Cursor_setParamVariables_oneRow(self, tmp_param, irow, *prow_size) < 0)
        {
            return -1;
        }
    }

    /* ????????????????????????????????????????????????????????None????????????SQL_TYPE???????????? */
    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        new_var         = PyList_GET_ITEM(self->param_variables, iparam);
        if (new_var != NULL)
            continue;

        new_varType     = Variable_TypeBySQLType(self->bindParamDesc[iparam].sql_type, 1);
        if (new_varType == NULL)
        {
            return -1;
        }

        new_var         = Variable_NewByVarType(self, new_varType, *prow_size, new_varType->size);
        if (new_var == NULL)
        {
            return -1;
        }

        /* ???????????????????????????????????????????????? **/
        if ((self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT_OUTPUT ||
            self->bindParamDesc[iparam].param_type == DSQL_PARAM_OUTPUT) &&
            new_var->type->pythonType == &g_ObjectVarType &&
            ObjectVar_GetParamDescAndObjHandles((udt_ObjectVar*)new_var, self->hdesc_param, iparam + 1) < 0)
        {
            return -1;        
        }

        PyList_SetItem(self->param_variables, iparam, new_var);
    }

    return 0;
}

static
sdint2
Cursor_BindParamVariable(
   udt_Cursor*      self,
   Py_ssize_t       rowsize
)
{
    DPIRETURN		rt = DSQL_SUCCESS;
    udint2          iparam;
    udt_Variable*   udt_var;
    ulength         rsize;

    rsize           = (ulength)rowsize;

    Py_BEGIN_ALLOW_THREADS
        rt = dpi_set_stmt_attr(self->handle, DSQL_ATTR_PARAMSET_SIZE, (dpointer)rsize, 0);
    Py_END_ALLOW_THREADS
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,
            "Desc_SetParamRowSize_Oper():dpi_set_stmt_attr") < 0)
            return -1;

    for (iparam = 0; iparam < self->paramCount; iparam ++)
    {
        udt_var = (udt_Variable*)PyList_GET_ITEM(self->param_variables, iparam);
        if (udt_var == NULL)
        {
            PyErr_SetString(g_ProgrammingErrorException,
                            "Not all parameters bound.");
            return -1;
        }

        if (Variable_Bind(udt_var, self, iparam + 1) < 0)
            return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Cursor_PerformBind()
//   Perform the binds on the cursor.
//-----------------------------------------------------------------------------
static
sdint2
Cursor_PerformBind(
   udt_Cursor*      self,                   // cursor to perform binds on
   PyObject*        parameters,	               // parameters to bind
   sdint2           isMany,						// ????????????????
   Py_ssize_t*      rowsize
)
{
    *rowsize        = 0;

    /** ????????setinputsize????????????????????setinputsize?????????????? **/
    if (self->setInputSizes)
    {
        if (PyList_GET_SIZE(self->param_variables) != self->paramCount)
        {
            self->setInputSizes = 0;

            Py_XDECREF(self->param_variables);
            self->param_variables = NULL;

            return 0;
        }
    }

    /** ????????????0???????????? **/
    if (self->paramCount == 0)
        return 0;        

    /** ???????????????????????????????? **/
    if (Cursor_setParamVariables(self, parameters, isMany, rowsize) < 0)
        return -1;

    /** ???????? **/
    return Cursor_BindParamVariable(self, *rowsize);
}

// ??????????????????????????????????????????????????
sdint4
Cursor_ParseArgs(
	PyObject		*args,
	PyObject		**specArg,		// SQL????????????????????
	PyObject		**seqArg		// ????????????
)
{
	Py_ssize_t  argCount = PyTuple_GET_SIZE(args);  
	sdint4      iparam;
	PyObject*   itemParam = NULL;
    PyObject*   itemParam_fst = NULL;

    if (specArg != NULL)
        *specArg = NULL;

    if (seqArg != NULL)
        *seqArg = NULL;

    if (argCount == 0)
        return 0;
	
	*specArg = PyTuple_GetItem(args, 0);
	if (!*specArg)
		return -1;	

    if (argCount == 1)
        return 0;

	// ??????????????tuple????list????dict??????????????????
	itemParam_fst   = PyTuple_GetItem(args, 1);
	if (itemParam_fst == NULL)
		return -1;
    
    itemParam   = itemParam_fst;

	// ????????
	if (!PyTuple_Check(itemParam) && !PyList_Check(itemParam) && !PyDict_Check(itemParam))
	{
		*seqArg = PyList_New(argCount - 1);
		if (!*seqArg)
			return -1;
		        
        Py_INCREF(itemParam);
		PyList_SetItem(*seqArg, 0, itemParam);		

		for (iparam = 2; iparam < argCount; iparam ++)
		{
			itemParam = PyTuple_GetItem(args, iparam);
			if (itemParam == NULL)
				return -1;

            Py_INCREF(itemParam);
			PyList_SetItem(*seqArg, iparam - 1, itemParam);			
		}
	}	
	else if(argCount == 2)
	{
		Py_INCREF(itemParam);
		*seqArg = itemParam;
	}
    else
    {
        PyErr_SetString(PyExc_ValueError, 
            "expecting a sequence or dict parameters");
        return -1;
    }
	
	return 0;
}

/* ????executedirect??????????dpi_exec_direct */
static 
PyObject*
Cursor_ExecuteDirect(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    PyObject*       statement = NULL;
    PyObject*       ret_obj = NULL;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ExecuteDirect\n"));
    
    if (!PyArg_ParseTuple(args, "O", &statement))
        goto fun_end;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "ENTER Cursor_ExecuteDirect,before Cursor_Execute_inner\n"));

    ret_obj         = Cursor_Execute_inner(self, statement, NULL, 0, 1, 0);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "EXIT Cursor_ExecuteDirect, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

int
Cursor_outparam_exist(
    udt_Cursor*     self
)
{
    udint2      i;

    if (self->paramCount == 0 ||
        self->bindParamDesc == NULL)
        return 0;

    for (i = 0; i < self->paramCount; i ++)
    {
        if (self->bindParamDesc[i].param_type == DSQL_PARAM_INPUT_OUTPUT ||
            self->bindParamDesc[i].param_type == DSQL_PARAM_OUTPUT)
            return 1;
    }

    return 0;
}

void
Cursor_BoundParamAndCols_Clear(
    udt_Cursor*     self
)
{
    Py_ssize_t      size;
    Py_ssize_t      i;
    PyObject*       item;

    if (self->param_variables != NULL)
    {
        size    = PyList_GET_SIZE(self->param_variables);
        
        for (i = 0; i < size; i ++)
        {
            item    = PyList_GET_ITEM(self->param_variables, i);
            if (item != NULL)
            {
                Variable_Finalize((udt_Variable*)item);
            }
        }
    }

    if (self->col_variables != NULL)
    {
        size    = PyList_GET_SIZE(self->col_variables);

        for (i = 0; i < size; i ++)
        {
            item    = PyList_GET_ITEM(self->col_variables, i);
            if (item != NULL)
            {
                Variable_Finalize((udt_Variable*)item);
            }
        }
    }
}

PyObject*
Cursor_Execute_inner(
    udt_Cursor*     self, 
    PyObject*       statement,
    PyObject*       executeArgs,
    int             is_many,
    int             exec_direct,
    int             from_call
)
{
    sdint2          isQuery = 0;
    PyObject*       paramsRet = NULL;
    Py_ssize_t      rowsize;

    /** statement??NULL?????? **/
    if (statement == NULL)
    {
        PyErr_SetString(PyExc_TypeError, "expecting a None or string statement arguement");
        
        return NULL;
    }

    if (executeArgs && 
        !PySequence_Check(executeArgs) && !PyDict_Check(executeArgs))
    {
        PyErr_SetString(PyExc_TypeError, "expecting a sequence or dict args");
        
        return NULL;
    }

    // make sure the cursor is open
    if (Cursor_IsOpen(self) < 0)
        return NULL;

    self->execute_num   += 1;

    // prepare the statement, if applicable
    if (exec_direct == 1)
    {
        if (Cursor_InternalExecDirect(self, statement) < 0)
            return NULL;
    }
    else
    {
        if (Cursor_InternalPrepare(self, statement) < 0)
        {
            goto fun_end;
        }

        // perform binds
        if (Cursor_PerformBind(self, executeArgs, is_many, &rowsize) < 0)
        {
            goto fun_end;
        }

        // execute the statement
        if (Cursor_InternalExecute(self, rowsize) < 0)
        {
            goto fun_end;
        }
    }

    // perform defines, if necessary    
    if ((self->statementType == DSQL_DIAG_FUNC_CODE_SELECT ||
        self->statementType == DSQL_DIAG_FUNC_CODE_CALL) && 
        Cursor_PerformDefine(self, &isQuery) < 0)
    {
        goto fun_end;
    }

    // reset the values of setoutputsize()
    self->outputSize = -1;
    self->outputSizeColumn = -1; 

    //reset the values of setinputsize()
    if (self->setInputSizes)
    {
        self->setInputSizes = 0;

        Py_XDECREF(self->param_variables);
        self->param_variables = NULL;
    }

    /** ????CALL???????????????????????????????????????? **/
    if (from_call == 1 || Cursor_outparam_exist(self))
    {
        paramsRet = Cursor_MakeupProcParams(self);
        if (paramsRet == NULL)
        {
            goto fun_end;
        }

        /* paramsRet????PyList_NEW????????????????????1????????????????1?????????? */
        //Py_INCREF(paramsRet);
        return paramsRet;        
    }        

    // for queries, return the cursor for convenience
    if (isQuery) 
    {
        Py_INCREF(self);
        return (PyObject*) self;        
    }

    // for all other statements, simply return None
    Py_INCREF(Py_None);
    return Py_None;    

fun_end:
    /** ?????????????????? **/
    Cursor_BoundParamAndCols_Clear(self);

    return NULL;
}

static 
PyObject*
Cursor_Execute(
    udt_Cursor*     self, 
    PyObject*       args, 
    PyObject*       keywordArgs
)
{
	PyObject*       statement = NULL;
    PyObject*       executeArgs = NULL; /** ?????????????????????? **/
    PyObject*       retObject = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Execute\n"));

	if (Cursor_ParseArgs(args, &statement, &executeArgs) < 0)
		goto fun_end;

    if (executeArgs == NULL && keywordArgs != NULL)
    {       
        executeArgs = keywordArgs;
        Py_INCREF(executeArgs);
    }
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, executeArgs, "ENTER Cursor_Execute,before Cursor_Execute_inner\n"));

    retObject       = Cursor_Execute_inner(self, statement, executeArgs, 0, 0, 0);
    Py_CLEAR(executeArgs);

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_Execute, %s\n", retObject == NULL ? "FAILED" : "SUCCESS"));

    return retObject;
}

static
PyObject*
Cursor_nextset_inner(
    udt_Cursor*     self
)
{
    DPIRETURN       rt = DSQL_SUCCESS;

    rt      = dpi_more_results(self->handle);
    if (!DSQL_SUCCEEDED(rt) && rt != DSQL_NO_DATA)
    {
        Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, rt,"Cursor_nextset_inner()");        

        return NULL;
    }

    if (rt == DSQL_NO_DATA)
    {
        Py_RETURN_NONE;
    }

    Py_RETURN_TRUE;
}

static
PyObject*
Cursor_nextset_Inner_ex(
    udt_Cursor*     self
)
{    
    PyObject*       ret;    

    /** ???????????????? **/
    Cursor_ExecRs_Clear(self);

    /** ?????????? **/
    Cursor_free_coldesc(self);

    /** ???????????????????????????????????????????????? **/
    ret     = Cursor_nextset_inner(self);
    if (!ret || ret == Py_None)
        return ret;
    
    /** ?????????? **/
    if (Cursor_PerformDefine(self, NULL) < 0)
        return NULL;

    if (Cursor_SetRowCount(self) < 0)
        return NULL;

    Py_RETURN_TRUE;
}

static
PyObject*
Cursor_nextset(
    udt_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_nextset\n"));

    retObj          = Cursor_nextset_Inner_ex(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_nextset, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_ExecuteMany(
    udt_Cursor*     self, 
    PyObject*       args
)
{
	PyObject*       statement;
    PyObject*       argsList;
    PyObject*       rowParams;
    PyObject*       retObj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ExecuteMany\n"));

	if (!PyArg_ParseTuple(args, "OO", &statement, &argsList))
		return NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, argsList, "ENTER Cursor_ExecuteMany, after parse args\n"));

	if (PyIter_Check(argsList))
	{	
        Py_INCREF(Py_None);
        retObj      = Py_None;

		while(1)
		{
			rowParams = PyIter_Next(argsList);
			if (rowParams == NULL)
				break;

            Py_XDECREF(retObj);
            retObj  = Cursor_Execute_inner(self, statement, rowParams, 0, 0, 0);

            DMPYTHON_TRACE_INFO(dpy_trace(statement, rowParams, "ENTER Cursor_ExecuteMany, Cursor_Execute_inner Per Row, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

            if (retObj == NULL)
            {
                Py_DECREF(rowParams);
                return NULL;
            }

			Py_DECREF(rowParams);
		}
        
        return retObj;
	}
	
    retObj  = Cursor_Execute_inner(self, statement, argsList, 1, 0, 0);

    DMPYTHON_TRACE_INFO(dpy_trace(statement, argsList, "ENTER Cursor_ExecuteMany, Cursor_Execute_inner Per Row, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_Prepare(
    udt_Cursor*     self, 
    PyObject*       args
)
{
	PyObject*   	statement = NULL;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Prepare\n"));

	// statement text and optional tag is expected	
	if (!PyArg_ParseTuple(args, "O", &statement))
		goto fun_end;

	// make sure the cursor is open
	if (Cursor_IsOpen(self) < 0)
		goto fun_end;

    self->execute_num   += 1;
    
    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "ENTER Cursor_Prepare,before Cursor_InternalPrepare\n"));

	// prepare the statement
	if (Cursor_InternalPrepare(self, statement) < 0)
		goto fun_end;

	Py_INCREF(Py_None);	
    ret_obj     = Py_None;

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(statement, NULL, "EXIT Cursor_Prepare, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
sdint2
Cursor_FixupBoundCursor(
    udt_Cursor*         self
)
{
	if (self->handle && self->statementType < 0)
	{
        Cursor_ExecRs_Clear(self);

		if (Cursor_GetStatementType(self) < 0)
			return -1;

		if (Cursor_PerformDefine(self, NULL) < 0)
			return -1;

		if (Cursor_SetRowCount(self) < 0)
			return -1;
	}

	return 0;
}

static 
sdint2
Cursor_VerifyFetch(
    udt_Cursor*     self
)
{
	if (Cursor_IsOpen(self) < 0)
		return -1;

	if (Cursor_FixupBoundCursor(self) < 0)
		return -1;

	if (self->colCount <= 0)
	{
		PyErr_SetString(g_InterfaceErrorException, "not a query");
		return -1;
	}
	
	return 0;
}

static 
sdint2
Cursor_InternalFetch(
    udt_Cursor*     self
)  
{
    DPIRETURN       status = DSQL_SUCCESS;
    ulength         rowCount;
    ulength         realToGet;
    ulength         rowleft;
    int             i;
    udt_Variable*   var;
    ulength         array_size;

    if (!self->colCount || self->col_variables == NULL) 
    {
        PyErr_SetString(g_InterfaceErrorException, "query not executed");
        return -1;
    }

    /** fetch????????????????????arraysize **/
    if ((int)self->arraySize < 0 || self->arraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ErrorException, "Invalid cursor arraysize\n");
        return -1;
    }

    /** ????fetch????????????????arraysize **/
    array_size      = self->arraySize;
    if (self->arraySize > self->org_arraySize)
    {
        array_size  = self->org_arraySize;
    }

    rowleft         = (ulength)(self->totalRows - self->rowCount);   
    /** ???????????????? **/
	realToGet       = array_size < rowleft ? array_size : rowleft;    

    for (i = 0; i < PyList_GET_SIZE(self->col_variables); i ++)
    {
        var = (udt_Variable*) PyList_GET_ITEM(self->col_variables, i);

        var->internalFetchNum++;
        if (var->type->preFetchProc) 
        {
            if ((*var->type->preFetchProc)(var, self->hdesc_col, i + 1) < 0)
                return -1;
        }
    }

    Py_BEGIN_ALLOW_THREADS	
		status = dpi_set_stmt_attr(self->handle, DSQL_ATTR_ROW_ARRAY_SIZE, (dpointer)realToGet, sizeof(realToGet));
	Py_END_ALLOW_THREADS

	if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
		"Cursor_InternalFetch(): dpi_set_stmt_attr") < 0)
		return -1;

	Py_BEGIN_ALLOW_THREADS
		status = dpi_fetch(self->handle, &rowCount);
    Py_END_ALLOW_THREADS
    if (status != DSQL_NO_DATA) {
        if (Environment_CheckForError(self->environment, self->handle, DSQL_HANDLE_STMT, status,
                "Cursor_InternalFetch(): fetch") < 0)
            return -1;
    }

	self->rowNum = 0;
	self->actualRows = rowCount - self->rowNum;

    return 0;
}

static 
sdint2
Cursor_MoreRows(
    udt_Cursor*     self
)
{
    /*????????-1*/
	if (self->actualRows == (ulength)(-1) ||  
        self->rowNum >= self->actualRows)
	{
		if (self->rowCount >= self->totalRows)
			return 0;

		if (self->actualRows == (ulength)(-1) || self->rowNum == self->actualRows)
			if (Cursor_InternalFetch(self) < 0)
				return -1;		
	}
	
	return 1;
}

//-----------------------------------------------------------------------------
// Cursor_CreateRow()
//   Create an object for the row. The object created is a tuple unless a row
// factory function has been defined in which case it is the result of the
// row factory function called with the argument tuple that would otherwise be
// returned.
//-----------------------------------------------------------------------------
/*static 
PyObject*
Cursor_CreateRow(
	udt_Cursor*     self                   // cursor object
)
{
	PyObject*       item;
	int             numItems, pos;
	PyObject**      apValues;
    udt_Variable*   udt_var;

	// create a new tuple
	numItems = self->colCount;
	apValues = PyMem_Malloc(sizeof(PyObject*) * numItems);
	if (!apValues)
		return PyErr_NoMemory();

	// acquire the value for each item
	for (pos = 0; pos < numItems; pos++) 
    {
        udt_var     = (udt_Variable*)PyList_GET_ITEM(self->col_variables, pos);
        if (udt_var != NULL)
        {
            item    = Variable_GetValue(udt_var, self->rowNum);		
        }
        
		if (udt_var == NULL || item == NULL)
		{
			FreeRowValues(pos, apValues);
			return NULL;
		}

		apValues[pos] = item;
	}

	// increment row counters
	self->rowCount++;
	self->rowNum ++;	

	return (PyObject*)Row_New(self->description, self->map_name_to_index, numItems, apValues);
}*/

static 
PyObject*
Cursor_CreateRow_AsTuple(
	udt_Cursor*     self                   // cursor object
)
{
    PyObject*       item;
    PyObject*       tuple;
    int             numItems, pos;
    udt_Variable*   udt_var;

    // create a new tuple
    numItems    = self->colCount;
    tuple       = PyTuple_New(numItems);
    if (tuple == NULL)
        return NULL;

    // acquire the value for each item
    for (pos = 0; pos < numItems; pos++) 
    {
        udt_var     = (udt_Variable*)PyList_GET_ITEM(self->col_variables, pos);
        if (udt_var != NULL)
        {
            item    = Variable_GetValue(udt_var, self->rowNum);		
        }

        if (udt_var == NULL || item == NULL)
        {
            Py_XDECREF(tuple);
            return NULL;
        }

        PyTuple_SetItem(tuple, pos, item);
    }

    // increment row counters
    self->rowCount++;
    self->rowNum ++;	

    return tuple;
}

static 
PyObject*
Cursor_CreateRow_AsDict(
	udt_Cursor*     self                   // cursor object
)
{
    PyObject*       item = NULL;
    PyObject*       dict = NULL;
    PyObject*       key = NULL;
    int             numItems, pos;
    DmColDesc       *colinfo;
    udt_Variable*   udt_var;

    // create a new tuple
    numItems    = self->colCount;

    dict        = PyDict_New();
    if (dict == NULL)
        return NULL;

    // acquire the value for each item
    for (pos = 0; pos < numItems; pos++) 
    {
        udt_var     = (udt_Variable*)PyList_GET_ITEM(self->col_variables, pos);
        if (udt_var != NULL)
        {
            item    = Variable_GetValue(udt_var, self->rowNum);		
        }

        if (udt_var == NULL || item == NULL)
        {
            Py_XDECREF(dict);
            return NULL;
        }

        colinfo     = &self->bindColDesc[pos];

        key         = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);

        PyDict_SetItem(dict, key, item);

        /* PyDict_SetItem????index,key????????????1????????index,key??????????????????????????????1 */
        Py_DECREF(item);
        Py_XDECREF(key);
    }

    // increment row counters
    self->rowCount++;
    self->rowNum ++;	

    return dict;
}

PyObject *
Cursor_One_Fetch(
	udt_Cursor*     self
)
{
	sdint4	rt;
	
	rt = Cursor_MoreRows(self);
	if (rt < 0)
		return NULL;
	else if (rt > 0)
    {
        if (self->connection->cursor_class == DICT_CURSOR)
        {
            return Cursor_CreateRow_AsDict(self);
        }
        else
        {
            return Cursor_CreateRow_AsTuple(self); /*BUG553553??????????tuple*/
        }
    }

	Py_INCREF(Py_None);
	return Py_None;
}


PyObject*
Cursor_Many_Fetch(
	udt_Cursor*     self,
	ulength			rowSize
)
{
	ulength		index;
	PyObject	*list, *tuple;

	list = PyList_New(rowSize);
	for (index = 0; index < rowSize; index ++){
		tuple = Cursor_One_Fetch(self);
		if (tuple == NULL){
			Py_DECREF(list);
			return NULL;
		}

		PyList_SET_ITEM(list, index, tuple);
	}

	//Py_INCREF(list);
	return list;
}


static 
PyObject*
Cursor_FetchOne(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchOne\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchOne,before Cursor_One_Fetch\n"));

	ret_obj         = Cursor_One_Fetch(self);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchOne, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
PyObject*
Cursor_FetchMany(
    udt_Cursor*     self, 
    PyObject*       args, 
    PyObject*       keywords
)
{
    static char*    keywordList[] = { "rows", NULL };
    ulength		    rowToGet;
    ulength         rowleft;
    Py_ssize_t      inputRow = self->arraySize;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchMany\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;

    if (!PyArg_ParseTupleAndKeywords(args, keywords, "|i", keywordList, &inputRow))
        goto fun_end;

	if (inputRow < 0 || inputRow >= INT_MAX)
    {
		PyErr_SetString(g_InterfaceErrorException, "Invalid rows value");
		goto fun_end;
	}	

    /* ????rows??????????????rowleft????????rows?????????????????????????? */
    rowleft     = (ulength)(self->totalRows - self->rowCount);
	rowToGet    = (ulength)inputRow < rowleft ? (ulength)inputRow : rowleft;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchMany,before Cursor_Many_Fetch rowleft ["slengthprefix"], rowToGet ["slengthprefix"]\n", rowleft, rowToGet));
	
    ret_obj     = Cursor_Many_Fetch(self, rowToGet);

fun_end:
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchMany, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static
PyObject*
Cursor_FetchAll(
    udt_Cursor*     self, 
    PyObject*       args
)
{
	ulength		    rowToGet;
    PyObject*       ret_obj = NULL;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchAll\n"));

	if (Cursor_VerifyFetch(self) < 0)
		goto fun_end;

	rowToGet    = (ulength)(self->totalRows - self->rowCount);
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_FetchAll,before Cursor_Many_Fetch rowToGet ["slengthprefix"]\n", rowToGet));

	ret_obj     = Cursor_Many_Fetch(self, rowToGet);

fun_end:

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_FetchAll, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static
PyObject*
Cursor_GetIter(
    udt_Cursor*     self
)
{
	if (Cursor_VerifyFetch(self) < 0)
		return NULL;

    self->is_iter   = 1;

	Py_INCREF(self);
	return (PyObject*)self;
}

static 
PyObject*
Cursor_GetNext_Inner(
    udt_Cursor*     self
)
{
	PyObject		*retObj;

	if (Cursor_VerifyFetch(self) < 0)
		return NULL;

	retObj = Cursor_One_Fetch(self);

    if (retObj != Py_None)
    {
        return retObj;
    }

    //PyErr_SetString(PyExc_StopIteration, "No data");

    if (self->is_iter == 1)
    {
        self->is_iter   = 0;

        return NULL;
    }
    else
    {
        Py_RETURN_NONE;
    }
}

static 
PyObject*
Cursor_GetNext(
    udt_Cursor*     self
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "ENTER Cursor_GetNext\n"));

    retObj      = Cursor_GetNext_Inner(self);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, NULL, "EXIT Cursor_GetNext\n"));

    return retObj;
}

static
udint4
Cursor_CalcStmtSize(
    udt_Cursor*     self,
    char*           procName,
    udint4          paramCount,
    udbyte          ret_value   /** 0????????????1?????????? **/
)
{
    /************************************************************************/
    /* ????????????
    /* begin
    /* ? = func(); ==>????????
    /*  proc();     ==>????????
    /* end;
    /************************************************************************/
    udint4          size = 14; /** = 5(begin) + 1(' ') + 3('('')'';') + 1(' ') + 4(end;) **/
    
    if (ret_value != 0)
    {
        size    += 4; /** '?'' ''='' '**/
    }

    size        += (udint4)strlen(procName);

    if (paramCount > 0)
    {
        size    += paramCount;      /*?*/
        size    += (paramCount - 1);/*,*/ 
        size    += (paramCount - 1);/*' '*/
    }

    return size;
}

static
PyObject*
Cursor_MakeStmtSQL(
    udt_Cursor*     self,
	char*           procName,
	udint4          paramCount,
    udbyte          ret_value   /** 0????????????1?????????? **/
)
{
    udint4          sql_len;
    sdbyte*         sql = NULL;	
	udint4	        iparam;
    PyObject*       sql_obj;

    sql_len = Cursor_CalcStmtSize(self, procName, paramCount, ret_value);
    sql     = PyMem_Malloc(sql_len + 1); /* ?????????? */
    if (sql == NULL)
    {
        return PyErr_NoMemory();
    }

    sprintf(sql, "begin ");
    
    if (ret_value != 0)
    {
        strcat(sql, "? = ");
    }

    strcat(sql, procName);
    strcat(sql, "(");	
	for (iparam = 0; iparam < paramCount; iparam ++)
	{
		if (iparam != paramCount -1)
			strcat(sql, "?, ");
		else
			strcat(sql, "?");
	}
	strcat(sql, "); end;");

	sql_obj = dmString_FromAscii(sql);

    PyMem_Free(sql);

    return sql_obj;
}


PyObject*
Cursor_MakeupProcParams(
	udt_Cursor*     self
)
{
	sdint4		iparam;
    sdint4      paramCount = self->paramCount;	
	PyObject*   paramVal;
    PyObject*   newParamVal;
    PyObject*   paramsRet;    

	paramsRet = PyList_New(paramCount);
	for (iparam = 0; iparam < paramCount; iparam ++)
	{
        paramVal    = PyList_GET_ITEM(self->param_variables, iparam);
        if (paramVal == NULL)
        {
            Py_DECREF(paramsRet);
            return NULL;
        }       

        /** ????OBJECT?????????????????????????????????????????????????????? **/
        if (((udt_Variable*)paramVal)->type->pythonType == &g_ObjectVarType &&
            self->bindParamDesc[iparam].param_type == DSQL_PARAM_INPUT)
        {
            newParamVal = ObjectVar_GetBoundExObj((udt_ObjectVar*)paramVal, 0);
        }
        else
        {
            newParamVal = Variable_GetValue((udt_Variable*)paramVal, 0);
        }
        if (newParamVal == NULL)
        {
            Py_DECREF(paramsRet);
            return NULL;
        }

		PyList_SetItem(paramsRet, iparam, newParamVal);
	}
		
	return paramsRet;
}

static
PyObject*
Cursor_CallExec_inner(
    udt_Cursor*     self, 
    PyObject*       args, 
    udint4          ret_value   /* ?????????????? */
)
{
    PyObject*       nameObj = NULL;
    PyObject*       parameters = NULL;
    PyObject*       sql = NULL;    
    char*           procName = NULL;
    Py_ssize_t      paramCount = 0;
    udt_Buffer	    buffer;  
    PyObject*       retObj = NULL;

    if (Cursor_ParseArgs(args, &nameObj, &parameters) < 0)
        return NULL;

    if (nameObj == NULL || nameObj == Py_None)
    {
        PyErr_SetString(g_InterfaceErrorException, "procedure name is illegal");
        return NULL;
    }

    // ????????	
    if (dmBuffer_FromObject(&buffer, nameObj, self->environment->encoding) < 0)
        return NULL;

    procName = PyMem_Malloc(buffer.size + 1);
    if (procName == NULL)
    {
        PyErr_NoMemory();
        return NULL;
    }

    strcpy(procName, buffer.ptr);
    dmBuffer_Clear(&buffer);    

    // ????????????????
    if (parameters != NULL)
        paramCount = PySequence_Size(parameters);
    else
        paramCount = 0;	

    // ????SQL????
    sql = Cursor_MakeStmtSQL(self, procName, (udint4)paramCount, ret_value);
    PyMem_Free(procName);

    if (ret_value != 0)
    {
        /** ??????????????????None??parameters???????????? **/
        //Py_XINCREF(parameters);

        if (parameters == NULL || parameters == Py_None)
        {            
            parameters  = PyList_New(1);

            /* PyList_SetItem:??steals?? a reference to item??
            ????Py_None??????????????????????????????????????
            ??????????????1??????parameters????????????????????PyNone??????1??  */
            Py_INCREF(Py_None);
            PyList_SetItem(parameters, 0, Py_None);
        }
        else
        {
            PyList_Insert(parameters, 0, Py_None);            
        }        
    }

    /** ???? **/
    retObj  = Cursor_Execute_inner(self, sql, parameters, 0, 0, 1);
    Py_CLEAR(sql);
    Py_CLEAR(parameters);
    
    return retObj;
}

static 
PyObject*
Cursor_CallProc(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    PyObject*       retObj;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_CallProc\n"));

	retObj  = Cursor_CallExec_inner(self, args, 0);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_CallProc, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_CallFunc(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_CallFunc\n"));

	retObj      = Cursor_CallExec_inner(self, args, 1);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_CallFunc, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static
PyObject*
Cursor_GetDescription(
	udt_Cursor	*self,
    void*       args
)
{
	PyObject*           desc = NULL;
    PyObject*           coldesc = NULL;
    udt_VariableType*   varType = NULL;
    PyObject*           typecode = NULL;
    PyObject*           colmap = NULL;
    PyObject*           index = NULL;
    PyObject*           colname_arr = NULL;
    PyObject*           colname = NULL;
	DmColDesc           *colinfo;
	sdint2		        icol;
    PyObject*           retObj = NULL;
    PyObject*           key = NULL;

    if (Cursor_IsOpen(self) < 0)
    {
        return NULL;
    }

    if (Cursor_FixupBoundCursor(self) < 0)
    {
        return NULL;
    }

    if (self->colCount <= 0)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    if (self->description != Py_None)
    {
        Py_INCREF(self->description);
        return self->description;
    }

    colname_arr = PyList_New(self->colCount);
	desc        = PyList_New(self->colCount);
	colmap      = PyDict_New();
	for (icol = 0; icol < self->colCount; icol ++)
	{
		colinfo = &self->bindColDesc[icol];

		// ??????????7????????????name,type_code,display_size ,internal_size, precision, scale, null_ok
        varType = Variable_TypeBySQLType(colinfo->sql_type, 0);
        if (varType == NULL)
        {
            goto done;
        }
        typecode    = (PyObject*)varType->pythonType;

        colname     = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);
        if (colname == NULL)
        {
            PyErr_SetString(g_OperationalErrorException, "NULL String");
            goto done;
        }          
        
        coldesc     = Py_BuildValue("(OOiiiii)",
                                colname,            
                                typecode,
                                colinfo->display_size,
                                colinfo->prec,
                                colinfo->prec,            
                                colinfo->scale,            
                                colinfo->nullable);

        /* Py_BuildValue????colname????????????1????colname??????????????????????1 */
        Py_XDECREF(colname);

        if (colinfo == NULL)
        {
            goto done;
        }

		// map_name_to_index
#if PY_MAJOR_VERSION < 3
		index = PyInt_FromLong(icol);
#else
        index = PyLong_FromLong(icol);
#endif
		if (!index)
        {
			goto done;
        }

        key             = dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding);

		PyDict_SetItem(colmap, 
                       key,
                       index);
        /* PyDict_SetItem????index,key????????????1????????index,key??????????????????????????????1 */
		Py_DECREF(index);       // SetItemString increments
        Py_XDECREF(key);
        index           = NULL;

		PyList_SetItem(desc, icol, coldesc);
		coldesc         = NULL;            // reference stolen by SET_ITEM
        //Py_XDECREF(coldesc);

        PyList_SetItem(colname_arr, 
                       icol, 
                       dmString_FromEncodedString(colinfo->name, strlen(colinfo->name), self->environment->encoding));
	}

	Py_XDECREF(self->description);
	self->description = desc;
	desc    = NULL;

    Py_XDECREF(self->map_name_to_index);
	self->map_name_to_index = colmap;
	colmap  = NULL;

    Py_XDECREF(self->column_names);
    self->column_names  = colname_arr;
    colname_arr = NULL;

done:

    Py_INCREF(self->description);
	retObj  = self->description;

	return retObj;
}

static 
PyObject*
Cursor_Parse(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Parse, NOT support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

static 
PyObject*
Cursor_SetInputSizes_inner(
    udt_Cursor*     self,
    PyObject*       args
)
{
    int             numArgs;
    PyObject*       value;
    udt_Variable*   var;
    Py_ssize_t      i;    

    // make sure the cursor is open
    if (Cursor_IsOpen(self) < 0)
        return NULL;

    // eliminate existing bind variables
    Py_CLEAR(self->param_variables);

    // if number of argument is 0, then return None;else create a new one
    numArgs = PyTuple_Size(args);
    if (numArgs == 0)
        Py_RETURN_NONE;

    self->param_variables   = PyList_New(numArgs);
    if (self->param_variables == NULL)
        return NULL;

    if ((sdint4)self->bindArraySize < 0 ||
        self->bindArraySize > ULENGTH_MAX)
    {
        PyErr_SetString(g_ProgrammingErrorException, 
            "invalid value of bindarraysize");

        return NULL;
    }
    
    // set the flag of inputSize 1
    self->setInputSizes     = 1;

    // process each input
    for (i = 0; i < numArgs; i++) 
    {
        value = PyTuple_GET_ITEM(args, i);
        if (value == Py_None) 
        {
            Py_INCREF(Py_None);
            PyList_SET_ITEM(self->param_variables, i, Py_None);
        } 
        else 
        {
            var = Variable_NewByType(self, value, self->bindArraySize);
            if (!var)
                return NULL;
            PyList_SET_ITEM(self->param_variables, i, (PyObject*) var);
        }
    }

    self->org_bindArraySize = self->bindArraySize;

    Py_INCREF(self->param_variables);
    return self->param_variables;
}

static 
PyObject*
Cursor_SetInputSizes(
    udt_Cursor*     self,
    PyObject*       args
)
{
    PyObject*       ret_obj;
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_SetInputSizes\n"));

    ret_obj         = Cursor_SetInputSizes_inner(self, args);
    
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_SetInputSizes, %s\n", ret_obj == NULL ? "FAILED" : "SUCCESS"));

    return ret_obj;
}

static 
PyObject*
Cursor_SetOutputSize_inner(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    self->outputSizeColumn = -1;
    if (!PyArg_ParseTuple(args, "i|i", &self->outputSize,
        &self->outputSizeColumn))
        return NULL;

    Py_INCREF(Py_None);
    return Py_None;
}

static 
PyObject*
Cursor_SetOutputSize(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    PyObject*       retObj;

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_SetOutputSize\n"));

    retObj      = Cursor_SetOutputSize_inner(self, args);

    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "EXIT Cursor_SetOutputSize, %s\n", retObj == NULL ? "FAILED" : "SUCCESS"));

    return retObj;
}

static 
PyObject*
Cursor_Var(
    udt_Cursor*     self, 
    PyObject*       args, 
    PyObject*       keywords
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_Var Not Support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

static 
PyObject*
Cursor_ArrayVar(
    udt_Cursor*     self, 
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_ArrayVar Not Support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

static 
PyObject*
Cursor_BindNames(
    udt_Cursor*     self,
    PyObject*       args
)
{
    DMPYTHON_TRACE_INFO(dpy_trace(NULL, args, "ENTER Cursor_BindNames Not Support\n"));

	PyErr_SetString(g_NotSupportedErrorException, "not support");
	return NULL;
}

//-----------------------------------------------------------------------------
// declaration of methods for Python type "Cursor"
//-----------------------------------------------------------------------------
static PyMethodDef g_CursorMethods[] = {
    { "execute",        (PyCFunction) Cursor_Execute,           METH_VARARGS | METH_KEYWORDS},
    { "executedirect",  (PyCFunction) Cursor_ExecuteDirect,     METH_VARARGS},
    { "fetchall",       (PyCFunction) Cursor_FetchAll,          METH_NOARGS },
    { "fetchone",       (PyCFunction) Cursor_FetchOne,          METH_NOARGS },
    { "fetchmany",      (PyCFunction) Cursor_FetchMany,         METH_VARARGS | METH_KEYWORDS },
    { "prepare",        (PyCFunction) Cursor_Prepare,           METH_VARARGS },
    { "parse",          (PyCFunction) Cursor_Parse,             METH_VARARGS },
    { "setinputsizes",  (PyCFunction) Cursor_SetInputSizes,     METH_VARARGS},
    { "executemany",    (PyCFunction) Cursor_ExecuteMany,       METH_VARARGS },
    { "callproc",       (PyCFunction) Cursor_CallProc,          METH_VARARGS},
    { "callfunc",       (PyCFunction) Cursor_CallFunc,          METH_VARARGS},
    { "setoutputsize",  (PyCFunction) Cursor_SetOutputSize,     METH_VARARGS },
    { "var",            (PyCFunction) Cursor_Var,               METH_VARARGS | METH_KEYWORDS },
    { "arrayvar",       (PyCFunction) Cursor_ArrayVar,          METH_VARARGS },
    { "bindnames",      (PyCFunction) Cursor_BindNames,         METH_NOARGS },
    { "close",          (PyCFunction) Cursor_Close,             METH_NOARGS },
    { "next",           (PyCFunction) Cursor_GetNext,           METH_NOARGS },
    { "nextset",        (PyCFunction) Cursor_nextset,           METH_NOARGS },
    { NULL,             NULL }
};


//-----------------------------------------------------------------------------
// declaration of members for Python type "Cursor"
//-----------------------------------------------------------------------------
static PyMemberDef g_CursorMembers[] = {
    { "arraysize",      T_INT,          offsetof(udt_Cursor, arraySize),        0 },
    { "bindarraysize",  T_INT,          offsetof(udt_Cursor, bindArraySize),    0 },
    { "rowcount",       T_INT,          offsetof(udt_Cursor, totalRows),        READONLY },  /** ???????????? **/
    { "rownumber",      T_INT,          offsetof(udt_Cursor, rowCount),         READONLY },   /** ????????????????0-based **/
    { "with_rows",      T_BOOL,         offsetof(udt_Cursor, with_rows),        READONLY },   /** ????????????????0-based **/
    { "statement",      T_OBJECT,       offsetof(udt_Cursor, statement),        READONLY },
    { "connection",     T_OBJECT_EX,    offsetof(udt_Cursor, connection),       READONLY },       
    { "column_names",   T_OBJECT_EX,    offsetof(udt_Cursor, column_names),     READONLY },
    { "lastrowid",      T_OBJECT,       offsetof(udt_Cursor, lastrowid_obj),    READONLY },
    { "_isClosed",      T_INT,          offsetof(udt_Cursor, isClosed),         READ_RESTRICTED },
    { "_statement",     T_OBJECT,       offsetof(udt_Cursor, statement),        READ_RESTRICTED },
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of calculated members for Python type "Connection"
//-----------------------------------------------------------------------------
static PyGetSetDef g_CursorCalcMembers[] = {
    { "description",            (getter) Cursor_GetDescription, 0,  0,  0},
    { NULL }
};


//-----------------------------------------------------------------------------
// declaration of Python type "Cursor"
//-----------------------------------------------------------------------------
PyTypeObject g_CursorType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "DmdbCursor",                     // tp_name
    sizeof(udt_Cursor),                 // tp_basicsize
    0,                                  // tp_itemsize
    (destructor) Cursor_Free,           // tp_dealloc
    0,                                  // tp_print
    0,                                  // tp_getattr
    0,                                  // tp_setattr
    0,                                  // tp_compare
    (reprfunc) Cursor_Repr,             // tp_repr
    0,                                  // tp_as_number
    0,                                  // tp_as_sequence
    0,                                  // tp_as_mapping
    0,                                  // tp_hash
    0,                                  // tp_call
    0,                                  // tp_str
    0,                                  // tp_getattro
    0,                                  // tp_setattro
    0,                                  // tp_as_buffer
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    // tp_flags
    0,                                  // tp_doc
    0,                                  // tp_traverse
    0,                                  // tp_clear
    0,                                  // tp_richcompare
    0,                                  // tp_weaklistoffset
    (getiterfunc) Cursor_GetIter,       // tp_iter
    (iternextfunc) Cursor_GetNext,      // tp_iternext
    g_CursorMethods,                    // tp_methods
    g_CursorMembers,                    // tp_members
    g_CursorCalcMembers,                // tp_getset
    0,                                  // tp_base
    0,                                  // tp_dict
    0,                                  // tp_descr_get
    0,                                  // tp_descr_set
    0,                                  // tp_dictoffset
    0,                                  // tp_init
    0,                                  // tp_alloc
    0,                                  // tp_new
    0,                                  // tp_free
    0,                                  // tp_is_gc
    0                                   // tp_bases
};