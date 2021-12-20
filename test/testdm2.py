#!/usr/bin/python
# coding:utf-8
import dmPython

try:
    conn = dmPython.connect(user='SYSDBA', password='SYSDBA', server='192.168.247.252', port=5236)
    cursor = conn.cursor()
    try:
        # 清空表，初始化测试环境
        cursor.execute('delete from PRODUCTION.PRODUCT_CATEGORY')
    except (dmPython.Error, Exception) as err:
        print(err)

    try:
        # 插入数据
        values = ('物理')
        cursor.execute("insert into PRODUCTION.PRODUCT_CATEGORY(name) values(?)", values)
        print('python: insert success!')

        # 查询数据
        cursor.execute("select name from PRODUCTION.PRODUCT_CATEGORY")
        res = cursor.fetchall()
        for tmp in res:
            for c1 in tmp:
                print(c1)

        print('python: select success!')
    except (dmPython.Error, Exception) as err:
        print(err)

    conn.close()
except (dmPython.Error, Exception) as err:
    print(err)
