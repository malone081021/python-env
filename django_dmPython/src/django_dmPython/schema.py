import binascii
import copy
import datetime
import re

from django.db.backends.base.schema import BaseDatabaseSchemaEditor
from django.db.utils import DatabaseError
from django.utils import six
#from django.utils.text import force_text


class DatabaseSchemaEditor(BaseDatabaseSchemaEditor):
    """
    This class (and its subclasses) are responsible for emitting schema-changing
    statements to the databases - model creation/removal/alteration, field
    renaming, index fiddling, and so on.

    It is intended to eventually completely replace DatabaseCreation.

    This class should be used by creating an instance for each set of schema
    changes (e.g. a migration file), and by first calling start(),
    then the relevant actions, and then commit(). This is necessary to allow
    things like circular foreign key references - FKs will only be created once
    commit() is called.
    """

    # Overrideable SQL templates
    #sql_create_table = "CREATE TABLE %(table)s (%(definition)s)"
    #sql_rename_table = "ALTER TABLE %(old_table)s RENAME TO %(new_table)s"
    #sql_retablespace_table = "ALTER TABLE %(table)s SET TABLESPACE %(new_tablespace)s"
    #sql_delete_table = "DROP TABLE %(table)s CASCADE"

    #sql_create_column = "ALTER TABLE %(table)s ADD COLUMN %(column)s %(definition)s"
    #sql_alter_column = "ALTER TABLE %(table)s %(changes)s"
    sql_alter_column_type = "MODIFY %(column)s %(type)s"
    sql_alter_column_null = "MODIFY %(column)s NULL"
    sql_alter_column_not_null = "MODIFY %(column)s NOT NULL"
    sql_alter_column_default = "ALTER COLUMN %(column)s SET DEFAULT %(default)s"
    #sql_alter_column_no_default = "ALTER COLUMN %(column)s DROP DEFAULT"
    #sql_delete_column = "ALTER TABLE %(table)s DROP COLUMN %(column)s CASCADE"
    #sql_rename_column = "ALTER TABLE %(table)s RENAME COLUMN %(old_column)s TO %(new_column)s"
    #sql_update_with_default = "UPDATE %(table)s SET %(column)s = %(default)s WHERE %(column)s IS NULL"

    #sql_create_check = "ALTER TABLE %(table)s ADD CONSTRAINT %(name)s CHECK (%(check)s)"
    #sql_delete_check = "ALTER TABLE %(table)s DROP CONSTRAINT %(name)s"

    #sql_create_unique = "ALTER TABLE %(table)s ADD CONSTRAINT %(name)s UNIQUE (%(columns)s)"
    #sql_delete_unique = "ALTER TABLE %(table)s DROP CONSTRAINT %(name)s"

    #sql_create_fk = (
    #    "ALTER TABLE %(table)s ADD CONSTRAINT %(name)s FOREIGN KEY (%(column)s) "
    #    "REFERENCES %(to_table)s (%(to_column)s) DEFERRABLE INITIALLY DEFERRED"
    #)
    #sql_create_inline_fk = None
    #sql_delete_fk = "ALTER TABLE %(table)s DROP CONSTRAINT %(name)s"

    #sql_create_index = "CREATE INDEX %(name)s ON %(table)s (%(columns)s)%(extra)s"
    #sql_delete_index = "DROP INDEX %(name)s"

    #sql_create_pk = "ALTER TABLE %(table)s ADD CONSTRAINT %(name)s PRIMARY KEY (%(columns)s)"
    #sql_delete_pk = "ALTER TABLE %(table)s DROP CONSTRAINT %(name)s"    

    def quote_value(self, value):
        """
        Returns a quoted version of the value so it's safe to use in an SQL
        string. This is not safe against injection from user code; it is
        intended only for use in making SQL scripts or preparing default values
        for particularly tricky backends (defaults are not user-defined, though,
        so this is safe).
        """
        if isinstance(value, (datetime.date, datetime.time, datetime.datetime)):
            return "'%s'" % value
        
        #elif isinstance(value, six.string_types):
        #    return "'%s'" % six.text_type(value).replace("\'", "\'\'")
        #elif isinstance(value, six.buffer_types):
        #    return "'%s'" % force_text(binascii.hexlify(value))
        
        elif isinstance(value, str):
            return "'%s'" % value.replace("\'", "\'\'").replace('%', '%%')
        elif isinstance(value, (bytes, bytearray, memoryview)):
            return "'%s'" % value.hex()
        
        elif isinstance(value, bool):
            return "1" if value else "0"
        else:
            return str(value) 
            
    def prepare_default(self, value):
        """
        Only used for backends which have requires_literal_defaults feature
        """
        return self.quote_value(value)
        
                    
        
