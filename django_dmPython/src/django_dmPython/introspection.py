from collections import namedtuple
import re
import dmPython

from django.db.backends.base.introspection import (
    BaseDatabaseIntrospection, FieldInfo, TableInfo,
)

FieldInfo = namedtuple('FieldInfo', FieldInfo._fields + ('extra',))
InfoLine = namedtuple('InfoLine', 'col_name data_type max_len num_prec num_scale extra column_default')

foreign_key_re = re.compile(r"\sCONSTRAINT `[^`]*` FOREIGN KEY \(`([^`]*)`\) REFERENCES `([^`]*)` \(`([^`]*)`\)")

class DatabaseIntrospection(BaseDatabaseIntrospection):
    data_types_reverse = {
        dmPython.DATE: 'DateField',
        dmPython.TIME: 'TimeField',
        dmPython.TIMESTAMP: 'DateTimeField',
        dmPython.NUMBER: 'DecimalField',
        dmPython.BIGINT: 'BigIntegerField',
        dmPython.ROWID: 'BigIntegerField',
        dmPython.DOUBLE: 'FloatField',
        dmPython.REAL:  'FloatField',
        dmPython.DECIMAL: 'DecimalField',
        dmPython.STRING: 'CharField',
        dmPython.FIXED_STRING: 'CharField',
        dmPython.BOOLEAN: 'BooleanField',
        dmPython.BLOB: 'BinaryField',
        dmPython.STRING: 'TextField',
        dmPython.INTERVAL: 'DurationField',                            
    }
    
    cache_bust_counter = 1

    def get_table_list(self, cursor):
        "Returns a list of table names in the current database."
        cursor.execute("select name, SUBTYPE$ from SYS.SYSOBJECTS WHERE TYPE$ = 'SCHOBJ' AND SUBTYPE$ IN('UTAB', 'VIEW') AND SCHID=CURRENT_SCHID();")        
            
        return [TableInfo(row[0], {'UTAB': 't', 'VIEW': 'v'}.get(row[1]))
                for row in cursor.fetchall()]

    def get_table_description(self, cursor, table_name):
        """
        Returns a description of the table, with the DB-API cursor.description interface."
        """
        # user_tab_columns gives data default for columns
        cursor.execute("""
            with TMP_VIEW as(
            select 
                tab.name as tab_name, 
                col.name as colname, 
                col.info2 & 0x1 as iden_col 
            from syscolumns col, sysobjects tab 
            where col.id=tab.id and tab.name = ? 
            )
            SELECT
                column_name,
                data_default,
                CASE
                    WHEN char_used IS NULL THEN data_length
                    ELSE char_length
                END as internal_size,
                TMP_VIEW.iden_col as is_autofield
            FROM user_tab_cols,TMP_VIEW
            WHERE table_name = TMP_VIEW.tab_name and column_name=TMP_VIEW.colname;
            """, [table_name])
        field_map = {
            column: (internal_size, default if default != 'NULL' else None, is_autofield)
            for column, default, internal_size, is_autofield in cursor.fetchall()
        }
        self.cache_bust_counter += 1
        cursor.execute("SELECT * FROM {} WHERE ROWNUM < 2 AND {} > 0".format(
            self.connection.ops.quote_name(table_name),
            self.cache_bust_counter))
        description = []
        for desc in cursor.description:
            name = desc[0]
            internal_size, default, is_autofield = field_map[name]
            name = name % {}  # cx_Oracle, for some reason, doubles percent signs.
            description.append(FieldInfo(
                self.identifier_converter(name), *desc[1:3], internal_size, desc[4] or 0,
                desc[5] or 0, *desc[6:], default, is_autofield,
            ))
        return description

    def _name_to_index(self, cursor, table_name):
        """
        Returns a dictionary of {field_name: field_index} for the given table.
        Indexes are 0-based.
        """
        return dict([(d[0], i) for i, d in enumerate(self.get_table_description(cursor, table_name))])

    def get_relations(self, cursor, table_name):
        """
        Returns a dictionary of {field_name: (field_name_other_table, other_table)}
        representing all relationships to the given table.
        """
        constraints = self.get_key_columns(cursor, table_name)
        relations = {}
        for my_fieldname, other_table, other_field in constraints:
            relations[my_fieldname] = (other_field, other_table)
        return relations

    def get_key_columns(self, cursor, table_name):
        """
        Returns a list of (column_name, referenced_table_name, referenced_column_name) for all
        key columns in given table.
        """
        
        sql = """
        select REF_COLS.NAME, REFED_COLS.NAME, REFED_TABS.NAME 
        from (select INDEXID, FINDEXID from SYS.SYSCONS where TYPE$='F' and TABLEID = (SELECT ID FROM SYS."SYSOBJECTS" WHERE NAME = '%s' AND SCHID=CURRENT_SCHID()))CONS, 
        (select COLS.NAME as NAME, INDS.ID as ID, OBJS.PID as PID from SYS.SYSCOLUMNS COLS, SYS.SYSINDEXES INDS, SYS.SYSOBJECTS OBJS where COLS.ID = OBJS.PID and OBJS.ID = INDS.ID AND SF_COL_IS_IDX_KEY(INDS.KEYNUM, INDS.KEYINFO, COLS.COLID)=1)REF_COLS, 
        (select COLS.NAME as NAME, INDS.ID as ID, OBJS.PID as PID from SYS.SYSCOLUMNS COLS, SYS.SYSINDEXES INDS, SYS.SYSOBJECTS OBJS where COLS.ID = OBJS.PID and OBJS.ID = INDS.ID AND SF_COL_IS_IDX_KEY(INDS.KEYNUM, INDS.KEYINFO, COLS.COLID)=1)REFED_COLS, 
        SYS.SYSOBJECTS REFED_TABS 
        where REF_COLS.ID = CONS.INDEXID and REFED_COLS.ID = CONS.FINDEXID and REFED_TABS.ID = REFED_COLS.PID
        """ % (table_name)
         
        key_columns = []
        cursor.execute(sql)
        key_columns.extend(cursor.fetchall())
        return key_columns

    def get_indexes(self, cursor, table_name):
        """
        Returns a dictionary of indexed fieldname -> infodict for the given
        table, where each infodict is in the format:
            {'primary_key': boolean representing whether it's the primary key,
             'unique': boolean representing whether it's a unique index}

        Only single-column indexes are introspected.
        """
        sql = """
        select 
              COLS.NAME AS NAME, 
              CASE CONS.TYPE$ WHEN 'P' THEN 1 ELSE 0 END AS is_primary,
              CASE INDS.ISUNIQUE WHEN 'Y' THEN 1 ELSE 0 END AS is_unique
        from    
              (select ID from SYS.SYSOBJECTS WHERE NAME = UPPER('%s'))TAB,
              (select NAME,ID,COLID from SYS.SYSCOLUMNS)COLS,
              (select TABLEID,INDEXID,TYPE$ from SYS.SYSCONS)CONS,
              (select INDEXES1.ID AS INDEXID,ISUNIQUE,XTYPE,KEYNUM,KEYINFO,INDEXES2.ID,INDEXES2.PID AS TABID from SYS.SYSINDEXES INDEXES1,SYS.SYSOBJECTS INDEXES2 WHERE INDEXES1.ID = INDEXES2.ID)INDS
        where
               TAB.ID = COLS.ID AND
               TAB.ID = CONS.TABLEID AND CONS.INDEXID = INDS.INDEXID AND
               TAB.ID = INDS.TABID AND SF_COL_IS_IDX_KEY(INDS.KEYNUM,INDS.KEYINFO,COLS.COLID) = 1;
        """
     
        cursor.execute(sql % table_name)
        
        rows = list(cursor.fetchall())        
        indexes = {}
        for row in rows:            
            indexes[row[0]] = {'primary_key': bool(row[1]), 'unique': bool(row[2])}
        return indexes

    def get_constraints(self, cursor, table_name):
        """
        Retrieves any constraints or keys (unique, pk, fk, check, index)
        across one or more columns.

        Returns a dict mapping constraint names to their attributes,
        where attributes is a dict with keys:
         * columns: List of columns this covers
         * primary_key: True if primary key, False otherwise
         * unique: True if this is a unique constraint, False otherwise
         * foreign_key: (table, column) of target, or None
         * check: True if check constraint, False otherwise
         * index: True if index, False otherwise.

        Some backends may return special constraint names that don't exist
        if they don't name constraints of a certain type (e.g. SQLite)
        """
        constraints = {}
        # Loop over the constraints, getting PKs and uniques
        cursor.execute("""
            select 
                   CONS.NAME,
                   COLS.NAME AS NAME, 
                   CASE CONS.TYPE$ WHEN 'P' THEN 1 ELSE 0 END AS is_primary,              
                   CASE INDS.ISUNIQUE WHEN 'Y' THEN 1 ELSE 0 END AS is_unique
            from    
                    (select ID from SYS.SYSOBJECTS WHERE NAME = UPPER('%s'))TAB,
                    (select NAME,ID,COLID from SYS.SYSCOLUMNS)COLS,
                    (select TABLEID,COLID,INDEXID,CONS1.TYPE$ AS TYPE$,CONS2.NAME AS NAME from SYS.SYSCONS CONS1,SYS.SYSOBJECTS CONS2 WHERE CONS1.ID = CONS2.ID)CONS,
                    (select INDEXES1.ID AS INDEXID,ISUNIQUE,XTYPE,KEYNUM,KEYINFO,INDEXES2.ID,INDEXES2.PID AS TABID from SYS.SYSINDEXES INDEXES1,SYS.SYSOBJECTS INDEXES2 WHERE INDEXES1.ID = INDEXES2.ID)INDS
            where
                     TAB.ID = COLS.ID AND
                     TAB.ID = CONS.TABLEID AND CONS.INDEXID = INDS.INDEXID AND (CONS.TYPE$ = 'P' OR CONS.TYPE$ = 'U') AND
                     TAB.ID = INDS.TABID AND INDS.XTYPE != 0 AND SF_COL_IS_IDX_KEY(INDS.KEYNUM,INDS.KEYINFO,COLS.COLID) = 1
            ORDER BY COLS.COLID;
        """ % table_name)
        for constraint, column, pk, unique in cursor.fetchall():
            # If we're the first column, make the record
            if constraint not in constraints:
                constraints[constraint] = {
                    "columns": [],
                    "primary_key": pk,
                    "unique": unique,
                    "foreign_key": None,
                    "check": False,
                    "index": True,  # All P and U come with index
                }
            # Record the details
            constraints[constraint]['columns'].append(column)
        # Check constraints
        cursor.execute("""
            select 
                CONS.NAME,
                COLS.NAME AS NAME
            from    
                (select ID from SYS.SYSOBJECTS WHERE NAME = UPPER('%s'))TAB,
                (select NAME,ID,COLID from SYS.SYSCOLUMNS)COLS,
                (select TABLEID,COLID,INDEXID,CONS1.TYPE$ AS TYPE$,CONS2.NAME AS NAME from SYS.SYSCONS CONS1,SYS.SYSOBJECTS CONS2 WHERE CONS1.ID = CONS2.ID)CONS    
            where
                 TAB.ID = COLS.ID AND
                 TAB.ID = CONS.TABLEID AND COLS.COLID = CONS.COLID AND CONS.TYPE$ = 'C'
             ORDER BY COLS.COLID;
        """ % table_name)
        for constraint, column in cursor.fetchall():
            # If we're the first column, make the record
            if constraint not in constraints:
                constraints[constraint] = {
                    "columns": [],
                    "primary_key": False,
                    "unique": False,
                    "foreign_key": None,
                    "check": True,
                    "index": False,
                }
            # Record the details
            constraints[constraint]['columns'].append(column)
        # Foreign key constraints
        cursor.execute("""
            select 
               CONS.NAME,
               COLS.NAME AS NAME,
               REFED_TABS.NAME,
               REFED_COLS.NAME
            from    
                (select ID from SYS.SYSOBJECTS WHERE NAME = UPPER('%s'))TAB,
                (select NAME,ID,COLID from SYS.SYSCOLUMNS)COLS,
                (select TABLEID,COLID,INDEXID,FINDEXID,CONS1.TYPE$ AS TYPE$,CONS2.NAME AS NAME from SYS.SYSCONS CONS1,SYS.SYSOBJECTS CONS2 WHERE CONS1.ID = CONS2.ID)CONS,
                (select INDEXES1.ID AS INDEXID,XTYPE,KEYNUM,KEYINFO,INDEXES2.ID,INDEXES2.PID AS TABID from SYS.SYSINDEXES INDEXES1,SYS.SYSOBJECTS INDEXES2 WHERE INDEXES1.ID = INDEXES2.ID)INDS,
                SYS.SYSCOLUMNS REFED_COLS,
                (select OBJS.PID AS PID, INDS.ID AS ID, INDS.XTYPE AS XTYPE, INDS.KEYNUM AS KEYNUM, INDS.KEYINFO AS KEYINFO from SYS.SYSOBJECTS OBJS, SYS.SYSINDEXES INDS where INDS.ID = OBJS.ID) REFED_INDS,              
                SYS.SYSOBJECTS REFED_TABS
            where
                 TAB.ID = COLS.ID AND
                 TAB.ID = CONS.TABLEID AND CONS.TYPE$ = 'F' AND 
                 CONS.INDEXID = INDS.INDEXID AND TAB.ID = INDS.TABID AND INDS.XTYPE != 0 AND SF_COL_IS_IDX_KEY(INDS.KEYNUM,INDS.KEYINFO,COLS.COLID) = 1 AND
                 REFED_INDS.ID = CONS.FINDEXID AND REFED_TABS.ID = REFED_INDS.PID AND 
                 REFED_TABS.ID = REFED_COLS.ID AND      
                 REFED_INDS.XTYPE != 0 AND SF_COL_IS_IDX_KEY(REFED_INDS.KEYNUM,REFED_INDS.KEYINFO,REFED_COLS.COLID) = 1
                 ORDER BY COLS.COLID; 
        """ % table_name)
        for constraint, column, other_table, other_column in cursor.fetchall():
            # If we're the first column, make the record
            if constraint not in constraints:
                constraints[constraint] = {
                    "columns": [],
                    "primary_key": False,
                    "unique": False,
                    "foreign_key": (other_table, other_column),
                    "check": False,
                    "index": False,
                }
            # Record the details
            constraints[constraint]['columns'].append(column)
        # Now get indexes
        cursor.execute("""
            select 
               INDS.NAME,
               COLS.NAME AS NAME
            from    
                (select ID from SYS.SYSOBJECTS WHERE NAME = UPPER('%s'))TAB,
                (select NAME,ID,COLID from SYS.SYSCOLUMNS)COLS,    
                (select OBJS.PID AS PID, OBJS.NAME AS NAME, INDS.ID AS ID, XTYPE, KEYNUM, KEYINFO from SYS.SYSOBJECTS OBJS, SYS.SYSINDEXES INDS where INDS.ID = OBJS.ID) INDS
            where
                 TAB.ID = COLS.ID AND
                 INDS.PID = TAB.ID AND     
                 SF_COL_IS_IDX_KEY(INDS.KEYNUM,INDS.KEYINFO,COLS.COLID) = 1 AND      
                 NOT EXISTS (select 1 from sys.syscons cons where cons.indexid = INDS.ID)
                 ORDER BY COLS.COLID;
        """ % table_name)
        for constraint, column in cursor.fetchall():
            # If we're the first column, make the record
            if constraint not in constraints:
                constraints[constraint] = {
                    "columns": [],
                    "primary_key": False,
                    "unique": False,
                    "foreign_key": None,
                    "check": False,
                    "index": True,
                }
            # Record the details
            constraints[constraint]['columns'].append(column)
        return constraints
    
    # added on 2019-7-30
    def get_sequences(self, cursor, table_name, table_fields=()):
        cursor.execute("""
            SELECT
                user_constraints.constraint_name,
                cols.column_name
            FROM
                user_constraints,
                user_cons_columns cols
            WHERE
                user_constraints.constraint_name = cols.constraint_name
                AND user_constraints.table_name = cols.table_name
                AND user_constraints.constraint_type = 'P'
                AND cols.table_name = ?
        """, [table_name])
        # Oracle allows only one identity column per table.
        row = cursor.fetchone()
        if row:
            return [{
                'name': self.identifier_converter(row[0]),
                'table': self.identifier_converter(table_name),
                'column': self.identifier_converter(row[1]),
            }]
        # To keep backward compatibility for AutoFields that aren't Oracle
        # identity columns.
        for f in table_fields:
            if isinstance(f, models.AutoField):
                return [{'table': table_name, 'column': f.column}]
        return []   
        
