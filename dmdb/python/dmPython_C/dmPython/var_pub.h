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
#define BFILE_ID_LEN                10      //SESS的注册编号长度
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

/*常规变量定义，其他变量类型都以此结构进行扩展*/
typedef struct{
    Variable_HEAD
        void *data;	    /*变量数据*/
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
    InitializeProc 	    initializeProc;		/*初始化，比如LOB类型的句柄申请等*/
    FinalizeProc 	    finalizeProc;		/*销毁，对应LOB类型的句柄释放等*/
    PreDefineProc 	    preDefineProc;	    /*绑定列之前处理*/    
    PreFetchProc 	    preFetchProc;		/*执行fetch之前处理*/
    IsNullProc 		    isNullProc;		    /*NULL值处理*/
    SetValueProc 	    setValueProc;		/*写值操作，Python到vt的转换*/
    GetValueProc 	    getValueProc;		/*读值操作，vt待Python的转换*/
    GetBufferSizeProc   getBufferSizeProc;	/*计算缓存大小操作*/
    BindObjectValueProc bindObjectValueProc;    /*OBJECT属性赋值操作*/
    PyTypeObject *	    pythonType;		    /*对应自定义Python类型对象*/
    sdint2 			    cType;			    /*对应绑定绑定C类型（SQL_C_XXX*/    
    udint4 			    size;				/*类型大小，相当于sizeof*/
    int 				isCharacterData;	/*是否为字符型数据，如CLOB和BLOB*/
    int 				isVariableLength;	/*是否变长数据*/
    int 				canBeCopied;		/*是否允许copy*/
    int 				canBeInArray;		/*是否可以作为数组成员*/
} udt_VariableType;

udt_VariableType*
Variable_TypeBySQLType (
    udint2      sqlType,             // SQL type, SQL_XXX
    int         value_flag           // 仅LOB类型有效，用于判断是取LOB对象还是取LOB对象中的值
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
    unsigned        ipos             /*参数绑定序号 1-based*/
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
    dpi_interval_t* data;   /* 日-时间间隔类型，可对应到python的timedelta方法*/
} udt_IntervalVar;

typedef struct {
    Variable_HEAD
    sdbyte*         data;   /* 年-月间隔类型，用字符串形式*/
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
    sdbyte*             data;  /* 时区类型，以字符串形式存放 */
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
    sdint4*     data;   /* 1/2/4字节长度，对应byte/tinyint/smallint/int类型 */
} udt_NumberVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* bigint，对应python的Py_Long_Long相关方法，由于低版本中实现不完整，为方便处理，统一用字符串类型来表示 */
} udt_BigintVar;

typedef struct {
    Variable_HEAD
    double*     data;   /* float/double/double precision 类型 */
} udt_DoubleVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* real类型，dpi接口映射为C的float类型，但python只支持双精度double类型，为避免类型转换导致的精度丢失，改用字符串处理 */
} udt_FloatVar;

typedef struct {
    Variable_HEAD
    sdbyte*     data;   /* numeric/number/decimal/dec 类型 */
} udt_NumberStrVar;

//-----------------------------------------------------------------------------
// long type
//-----------------------------------------------------------------------------
extern  udt_VariableType    vt_LongBinary;
extern  udt_VariableType    vt_LongString;

typedef struct {
    Variable_HEAD
        char *data; /** 前4个字节为有效数据长度 **/
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

    PyObject*           varValue;   /** 属性值对象 **/
    PyObject*			attributes; /** 结构对象（class/record）各成员的属性信息或者数组对象的元素类型，为一个LIST对象，LIST长度对应field_count **/    
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
    dhobj*          data;       /** 对象类型的hdobj数据句柄 **/    
    dhobjdesc		desc;       /** 对象本身的dhobjdesc描述句柄 **/
    udt_Connection* connection; 
    PyObject*       exObjects;  /** 通过绑定参数进来的OJBECT对象链表 **/
} udt_ObjectVar;

int
ObjectVar_GetParamDescAndObjHandles(
    udt_ObjectVar*  self,            // variable to set up    
    dhdesc          hdesc_param,
    sdint2          pos              // position in define list，1-based
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
    udt_ObjectVar*      refered_objVar; /** 作为列值或者输出或者输入参数引用的OBJECTVar对象 **/
    udint8              cursor_execNum; /** 关联cursor执行了次数 **/
    udt_Connection*     connection;
    udt_ObjectType*     objectType;
    PyObject*           objectValue;    /** 与Type中属性attributes一一对应 **/    
    dhobj               hobj;
    dhdesc              hobjdesc;  /** 若refered_objVar != NULL，则与其中的desc值相等 **/
    udint4              value_count;
    udt_Cursor*         ownCursor;    
    int                 MatchHandle_execd; /** 是否执行过setvalue **/
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
