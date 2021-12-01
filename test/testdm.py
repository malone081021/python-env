
#!/usr/bin/python
#coding:utf-8
import dmPython
try:
    conn = dmPython.connect(user='SYSDBA', password='SYSDBA', server='192.168.247.252',  port=5236)
    cursor  = conn.cursor()
    print('python: conn success!')
    conn.close()
except (dmPython.Error, Exception) as err:
    print(err)
