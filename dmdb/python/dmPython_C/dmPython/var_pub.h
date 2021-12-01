/******************************************************
file:
    var_pub.h
purpose:
    python type define for all DM Variables public function in dmPython
interface:
    {}
history:
    Date        Who         RefDoc      Memo
    2015-6-4    shenning                Created
*******************************************************/

#ifndef _VAR_PUB_H
#define _VAR_PUB_H

#include "strct.h"

#define NAMELEN                     128
#define MAX_PATH_LEN                256
#define BFILE_ID_LEN                10      //SESS��ע���ų���
#define END                         0
#define	SPACE				        32	/*   */

#ifdef DM64
typedef sdint8          int3264;
#else
typedef sdint4          int3264;
#endif


//-----------------------------------------------------------------------------
// define structure common to all variables
//-----------------------------------------------------------------------------
struct _udt_VariableType;
#define Variable_HEAD \
    PyObject_HEAD \
    DmParamDesc*        paramdesc;\
    dhstmt		        boundCursorHandle; \
    udint2 		        boundPos; \
    udt_Environment*	environment; \
    udint4 				allocatedElements; \
    udint4				actualElements; \
    udint4 				internalFetchNum; \
    int 				isArray; \
    int 				isAllocatedInternally; \
    slength 			*indicator; \
    slength 			*actualLength; \
    udint4 				size; \
    udint4				bufferSize; \
    udt_Cursor*         relatedCursor;\
    struct _udt_VariableType*	type;

/*����������壬�����������Ͷ��Դ˽ṹ������չ*/
typedef struct{
    Variable_HEAD
        void *data;	    /*��������*/
}udt_Variable;

//-----------------------------------------------------------------------------
// define function types for the common actions that take place on a variable
//-----------------------------------------------------------------------------
typedef int (*InitializeProc)(udt_Variable*, udt_Cursor*);
typedef void (*FinalizeProc)(udt_Variable*);
typedef int (*PreDefineProc)(udt_Variable*, dhdesc, sdint2);
typedef int (*PreFetchProc)(udt_Variable*, dhdesc, sdint2);
typedef int (*IsNullProc)(udt_Variable*, unsigned);
typedef int (*SetValueProc)(udt_Variable*, unsigned, PyObject*);
typedef PyObject * (*GetValueProc)(udt_Variable*, unsigned);
typedef udint4  (*GetBufferSizeProc)(udt_Variable*);
typedef DPIRETURN (*BindObjectValueProc)(udt_Variable*, unsigned, dhobj, udint4);

typedef struct _udt_VariableType {
    InitializeProc 	    initializeProc;		/*��ʼ��������LOB���͵ľ�������*/
    FinalizeProc 	    finalizeProc;		/*���٣���ӦLOB���͵ľ���ͷŵ�*/
    PreDefineProc 	    preDefineProc;	    /*����֮ǰ����*/    
    PreFetchProc 	    preFetchProc;		/*ִ��fetch֮ǰ����*/
    IsNullProc 		    isNullProc;		    /*NULLֵ����*/
    SetValueProc 	    setValueProc;		/*дֵ������Python��vt��ת��*/
    GetValueProc 	    getValueProc;		/*��ֵ������vt��Python��ת��*/
    GetBufferSizeProc   getBufferSizeProc;	/*���㻺���С����*/
    BindObjectValueProc bindObjectValueProc;    /*OBJECT���Ը�ֵ����*/
    PyTypeObject *	    pythonType;		    /*��Ӧ�Զ���Python���Ͷ���*/
    sdint2 			    cType;			    /*��Ӧ�󶨰�C���ͣ�SQL_C_XXX*/    
    udint4 			    size;				/*���ʹ�С���൱��sizeof*/
    int 				isCharacterData;	/*�Ƿ�Ϊ�ַ������ݣ���CLOB��BLOB*/
    int 				isVariableLength;	/*�Ƿ�䳤����*/
    int 				canBeCopied;		/*�Ƿ�����copy*/
    int 				canBeInArray;		/*�Ƿ������Ϊ�����Ա*/
} udt_VariableType;

udt_VariableType*
Variable_TypeBySQLType (
    udint2      sqlType,             // SQL type, SQL_XXX
    int         value_flag           // ��LOB������Ч�������ж���ȡLOB������ȡLOB�����е�ֵ
);

udt_VariableType*
Variable_TypeByValue(
    PyObject*       value,              // Python type
    udint4*         size               // size to use (OUT)    
);

udt_Variable*
Variable_New(
    udt_Cursor*         cursor,         /* cursor to associate variable with */
    udint4              numElements,    /* number of elements to allocate */
    udt_VariableType*   type,           /* variable type */
    sdint4              size            /* used only for variable length types */
);

int 
Variable_Bind(
    udt_Variable*   var,     // variable to bind
    udt_Cursor*     cursor,  // cursor to bind to    
    udint2          pos      // position to bind to
);

int 
Variable_Check(
    PyObject*       object  // Python object to check
);

udt_Variable*
Variable_NewByValue(
    udt_Cursor*     cursor,         // cursor to associate variable with
    PyObject*       value,          // Python value to associate
    unsigned        numElements,     // number of elements to allocate
    unsigned        ipos             /*��������� 1-based*/
);

udt_Variable*
Variable_NewByType(
    udt_Cursor* cursor,         // cursor to associate variable with
    PyObject*   value,            // Python data type to associate
    unsigned    numElements        // number of elements to allocate
);

udt_Variable*
Variable_NewByVarType(
    udt_Cursor*         cursor,         // cursor to associate variable with
    udt_VariableType*   varType,            // Python data type to associate
    unsigned            numElements,        // number of elements to allocate
    udint4              size            // buffer length to alloc
);

int
Variable_IsNull(
    udt_Variable*       var // variable to return the string for
);

int 
Variable_SetValue(
    udt_Variable*   var,        // variable to set
    udint4          arrayPos,   // array position
    PyObject*       value       // value to set
);

PyObject*
Variable_GetValue(
    udt_Variable*   var,      // variable to get the value for
    udint4          arrayPos  // array position
);

int
Variable_BindObjectValue(
    udt_Variable*   var,
    udint4          arrayPos,
    dhobj           obj_hobj,
    udint4          value_nth
);

udt_Variable*
Variable_Define(
    udt_Cursor*     cursor,         // cursor in use
    dhdesc          hdesc_col,
    udint2          position,       // position in define list
    udint4          numElements     // number of elements to create
);

int 
Variable_Resize(
    udt_Variable*   self,   // variable to resize
    unsigned        size    // new size to use
);

void
Variable_Import();

int
Variable_PutDataAftExec(
    udt_Variable*   var,        // for variable to put data
    udint4          arrayPos    // array position
);

void
Variable_Finalize(
    udt_Variable*       self
);

//-----------------------------------------------------------------------------
// interval type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_Interval;
extern  udt_VariableType    vt_YMInterval;

typedef struct {
    Variable_HEAD
    dpi_interval_t* data;   /* ��-ʱ�������ͣ��ɶ�Ӧ��python��timedelta����*/
} udt_IntervalVar;

typedef struct {
    Variable_HEAD
    sdbyte*         data;   /* ��-�¼�����ͣ����ַ�����ʽ*/
} udt_YMIntervalVar;

void
IntervalVar_import();

//-----------------------------------------------------------------------------
// date/time/timestamp type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_Date;
extern  udt_VariableType    vt_Time;
extern  udt_VariableType    vt_Timestamp;
extern  udt_VariableType    vt_TimeTZ;
extern  udt_VariableType    vt_TimestampTZ;

typedef struct {
    Variable_HEAD
    dpi_date_t*         data;
} udt_DateVar;

typedef struct {
    Variable_HEAD
    dpi_time_t*         data;
} udt_TimeVar;

typedef struct {
    Variable_HEAD
    dpi_timestamp_t*    data;
} udt_TimestampVar;

typedef struct {
    Variable_HEAD
    sdbyte*             data;  /* ʱ�����ͣ����ַ�����ʽ��� */
} udt_TZVar;

void
DateVar_import();


//-----------------------------------------------------------------------------
//  string type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_String;
extern  udt_VariableType    vt_FixedChar;
extern  udt_VariableType    vt_Binary;
extern  udt_VariableType    vt_FixedBinary;

#if PY_MAJOR_VERSION < 3
extern  udt_VariableType    vt_UnicodeString;
extern  udt_VariableType    vt_FixedUnicodeChar;
#endif

typedef struct {
    Variable_HEAD
    sdbyte*      data;
} udt_StringVar;


//-----------------------------------------------------------------------------
// number type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_Integer;
extern  udt_VariableType    vt_Bigint;
extern  udt_VariableType    vt_RowId;

extern  udt_VariableType    vt_Double;
extern  udt_VariableType    vt_Float;

extern  udt_VariableType    vt_Boolean;
extern  udt_VariableType    vt_NumberAsString;

typedef struct {
    Variable_HEAD
    sdint4*     data;   /* 1/2/4�ֽڳ��ȣ���Ӧbyte/tinyint/smallint/int���� */
} udt_NumberVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* bigint����Ӧpython��Py_Long_Long��ط��������ڵͰ汾��ʵ�ֲ�������Ϊ���㴦��ͳһ���ַ�����������ʾ */
} udt_BigintVar;

typedef struct {
    Variable_HEAD
    double*     data;   /* float/double/double precision ���� */
} udt_DoubleVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* real���ͣ�dpi�ӿ�ӳ��ΪC��float���ͣ���pythonֻ֧��˫����double���ͣ�Ϊ��������ת�����µľ��ȶ�ʧ�������ַ������� */
} udt_FloatVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* numeric/number/decimal/dec ���� */
} udt_NumberStrVar;

//-----------------------------------------------------------------------------
// long type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_LongBinary;
extern  udt_VariableType    vt_LongString;

typedef struct {
    Variable_HEAD
        char *data; /** ǰ4���ֽ�Ϊ��Ч���ݳ��� **/
} udt_LongVar;

int
vLong_PutData(
    udt_LongVar*    self,    // variable to get buffer size
    udint4          arrayPos    // array position
);

//-----------------------------------------------------------------------------
// BFILE type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_BFILE;

typedef struct {
    Variable_HEAD
    void*               data;
    unsigned            pos;
    udt_Connection*     connection;
} udt_BFileVar;

typedef struct {
    PyObject_HEAD
    udt_BFileVar*   BFileVar;
    unsigned        pos;
} udt_ExternalBFile;

//-----------------------------------------------------------------------------
// LOB type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_BLOB;
extern  udt_VariableType    vt_CLOB;

typedef struct {
    Variable_HEAD
    dhloblctr*			data;
    udt_Connection*     connection;
    PyObject*           exLobs;    
} udt_LobVar;

int 
vLobVar_Write(
    udt_LobVar*     var,        // variable to perform write against
    unsigned        position,   // position to perform write against
    PyObject*       dataObj,    // data object to write into LOB
    udint4          start_pos,     // offset into variable
    udint4*         amount      // amount to write
);

//-----------------------------------------------------------------------------
// external LOB type
//-----------------------------------------------------------------------------
typedef struct {
    PyObject_HEAD
    udt_LobVar* lobVar;
    unsigned    pos;
    unsigned    internalFetchNum;
} udt_ExternalLobVar;


PyObject*
ExternalLobVar_New(
    udt_LobVar*     var,    // variable to encapsulate
    unsigned        pos     // position in array to encapsulate
);

PyObject*
exLobVar_BytesToString(
    PyObject*       bsObject,
    slength         len
);

PyObject *exLobVar_Str(udt_ExternalLobVar*);
PyObject*
exLobVar_Bytes(
    udt_ExternalLobVar* var  // variable to return the string for
);  

//-----------------------------------------------------------------------------
// Cursor variable type
//-----------------------------------------------------------------------------
extern  udt_VariableType vt_Cursor;

typedef struct {
    Variable_HEAD
    dhstmt*         data;
    udt_Connection* connection;
    PyObject*       cursors;
} udt_CursorVar;

//-----------------------------------------------------------------------------
// Object variable type
//-----------------------------------------------------------------------------
typedef struct {
    PyObject_HEAD
    udt_Connection*     connection;
    udt_Environment*	environment;
    PyObject*			schema;
    PyObject*			name;    
    sdint2              sql_type;
    sdint2              prec;
    sdint2              scale;         

    PyObject*           varValue;   /** ����ֵ���� **/
    PyObject*			attributes; /** �ṹ����class/record������Ա��������Ϣ������������Ԫ�����ͣ�Ϊһ��LIST����LIST���ȶ�Ӧfield_count **/    
} udt_ObjectType;

typedef struct {
    PyObject_HEAD
    udt_Connection*     connection;
    PyObject*			schema;
    PyObject*			name;    
    udt_ObjectType*	    ObjType;
} udt_ObjectAttribute;

udt_ObjectType* ObjectType_New(udt_Connection*, dhobjdesc);

int
ObjectType_IsObjectType(
    udt_ObjectType* self    // object type to return the string for
);

//-----------------------------------------------------------------------------
// Object type
//-----------------------------------------------------------------------------
extern udt_VariableType vt_Object;
extern udt_VariableType vt_Record;
extern udt_VariableType vt_Array;
extern udt_VariableType vt_SArray;

typedef struct {
    Variable_HEAD
    dhobj*          data;       /** �������͵�hdobj���ݾ�� **/    
    dhobjdesc		desc;       /** �������dhobjdesc������� **/
    udt_Connection* connection; 
    PyObject*       exObjects;  /** ͨ���󶨲���������OJBECT�������� **/
} udt_ObjectVar;

int
ObjectVar_GetParamDescAndObjHandles(
    udt_ObjectVar*  self,            // variable to set up    
    dhdesc          hdesc_param,
    sdint2          pos              // position in define list��1-based
);

int
ObjectVar_SetValue_Inner(
    udt_ObjectVar*  var, 
    unsigned        pos, 
    dhobj           hobj,
    dhobjdesc       hobjdesc
);

PyObject*
ObjectVar_GetBoundExObj(
    udt_ObjectVar*      var, 
    unsigned            pos
);

//-----------------------------------------------------------------------------
// external Object type
//-----------------------------------------------------------------------------
typedef struct {
    PyObject_HEAD   
    udt_ObjectVar*      refered_objVar; /** ��Ϊ��ֵ���������������������õ�OBJECTVar���� **/
    udint8              cursor_execNum; /** ����cursorִ���˴��� **/
    udt_Connection*     connection;
    udt_ObjectType*     objectType;
    PyObject*           objectValue;    /** ��Type������attributesһһ��Ӧ **/    
    dhobj               hobj;
    dhdesc              hobjdesc;  /** ��refered_objVar != NULL���������е�descֵ��� **/
    udint4              value_count;
    udt_Cursor*         ownCursor;    
    int                 MatchHandle_execd; /** �Ƿ�ִ�й�setvalue **/
} udt_ExternalObjectVar;

PyObject*
ExObjVar_New_FromObjVar(    
    udt_ObjectVar*  objVar,    
    dhobjdesc       hobjdesc,
    dhobj           hobj
);

int
ExObjVar_MatchCheck(
    udt_ExternalObjectVar*  self,
    dhobjdesc               hobjdesc,
    dhobj                   hobj,
    udint4*                 value_count
);

PyObject*
exBFileVar_NEW(
    udt_BFileVar*   var,        // variable to determine value for
    unsigned        pos         // array position
);

#endif
