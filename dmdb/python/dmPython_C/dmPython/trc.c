#include "trc.h"
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include <stdio.h>
#include <time.h> 
#include "py_Dameng.h"

#define MAX_TRACE_MASSEGE_LEN (2048)
#define MAX_TIMESTAMP_STR_LEN (32)
#define TIMESTAMPE_BUF_LEN    (MAX_TIMESTAMP_STR_LEN + 1)

#ifdef TRACE
udint4  trace_mod = DMPYTHON_TRACE_ON;
#else
udint4  trace_mod = DMPYTHON_TRACE_OFF;
#endif

#ifdef WIN32 
#define LOCALTIME(tm,ti)  localtime_s(tm,ti) 
#else 
#define LOCALTIME(tm,ti)  localtime_r(ti,tm) 
#endif 

static
void
dpy_get_timestamp(
    sdbyte*       buf
)
{
    struct tm	cur_time;
    time_t		ltime;
    
    if (buf == NULL)
    {
        return;
    }
    
    time(&ltime);
    LOCALTIME(&cur_time, &ltime);
   
    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
        cur_time.tm_year + 1900, 
        cur_time.tm_mon + 1, 
        cur_time.tm_mday, 
        cur_time.tm_hour, 
        cur_time.tm_min, 
        cur_time.tm_sec);
}

void
dpy_trace(
    PyObject*       statement,
    PyObject*       args,
    sdbyte*         info,
    ...
)
{    
    int             rt;
    sdbyte          timestamp[TIMESTAMPE_BUF_LEN];    
    PyObject*       fileObj;
    PyObject*       strBegin;
    PyObject*       strInfo;
    va_list         vl;

#if PY_MAJOR_VERSION >= 3
    int             fd;

#ifndef WIN32 
    fd          = open(DMPYTHON_TRACE_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);
#else
    fd          = open(DMPYTHON_TRACE_FILE, O_RDWR | O_CREAT | O_APPEND, S_IREAD |S_IWRITE);
#endif

    if (fd < 0)
        return;

    fileObj     = PyFile_FromFd(fd, DMPYTHON_TRACE_FILE, "a+", -1, NULL, NULL, NULL, 1);

    if (fileObj == NULL)
    {
        close(fd);
        return;
    }

#else    
    fileObj     = PyFile_FromString(DMPYTHON_TRACE_FILE, "a+");
    if (fileObj == NULL)
    {    
        return;
    }
#endif

    va_start(vl, info);
    strInfo     = py_String_FromFormatV(info, vl);
    va_end(vl);

    dpy_get_timestamp(timestamp);        

    strBegin    = py_String_FromFormat("%s: \t", timestamp);    
    rt          = PyFile_WriteObject(strBegin, fileObj, Py_PRINT_RAW);
    if (rt < 0)
    {
        goto fun_end;
    }

    rt          = PyFile_WriteObject(strInfo, fileObj, Py_PRINT_RAW);
    if (rt < 0)
    {
        goto fun_end;
    }

    if (statement != NULL && statement != Py_None)
    {
        rt      = PyFile_WriteObject(statement, fileObj, Py_PRINT_RAW);
        if (rt < 0)
        {
            goto fun_end;
        }
    }    

    if (args != NULL && args != Py_None)
    {
        rt      = PyFile_WriteObject(args, fileObj, Py_PRINT_RAW);
        if (rt < 0)
        {
            goto fun_end;
        }
    }    
    
    rt          = PyFile_WriteString("\n", fileObj);
    if (rt < 0)
    {
        goto fun_end;
    }

fun_end:

    Py_DECREF(fileObj);
    Py_DECREF(strBegin);
    Py_DECREF(strInfo);
}

