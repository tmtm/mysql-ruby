//	ruby mysql module
//	$Id: mysql.cc,v 1.8 1998/11/29 13:02:16 tommy Exp $
//

#include "ruby.h"
#include <mysql/mysql.h>

extern "C" {
    void Init_mysql(void);
}

#define NILorSTRING(obj)	(NIL_P(obj)? NULL: (Check_SafeStr(obj), RSTRING(obj)->ptr))
#define NILorINT(obj)		(NIL_P(obj)? NULL: NUM2INT(obj))

VALUE cMysql;
VALUE cMysqlRes;
VALUE cMysqlField;
VALUE eMysql;

struct mysql {
    MYSQL handler;
    bool connection;
    bool query_with_result;
};

//	free Mysql class object
//
static void free_mysql(struct mysql* my)
{
    if (my->connection)
	mysql_close(&my->handler);
    free(my);
}

//	free MysqlRes class object
//
static void free_mysqlres(MYSQL_RES* res)
{
    mysql_free_result(res);
}

//	free MysqlField class object
//
static void free_mysqlfield(MYSQL_FIELD* f)
{
    free(f->name);
    free(f->table);
    free(f->def);
    free(f);
}


//-------------------------------
// Mysql class method
//

//	real_connect([host, [user, [passwd, [db, [port, [sock, [flag]]]]]]])
//
static VALUE real_connect(int argc, VALUE* argv, VALUE klass)
{
    VALUE host, user, passwd, db, port, sock, flag;
    char *h, *u, *p, *d, *s;
    uint pp, f;

    rb_scan_args(argc, argv, "07", &host, &user, &passwd, &db, &port, &sock, &flag);
    h = NILorSTRING(host);
    u = NILorSTRING(user);
    p = NILorSTRING(passwd);
    d = NILorSTRING(db);
    pp = NILorINT(port);
    s = NILorSTRING(sock);
    f = NILorINT(flag);

    MYSQL my;
    mysql_init(&my);
    if (mysql_real_connect(&my, h, u, p, d, pp, s, f) == NULL)
	Raise(eMysql, "%s", mysql_error(&my));

    struct mysql* myp;
    VALUE obj = Data_Make_Struct(klass, struct mysql, 0, free_mysql, myp);
    myp->handler = my;
    myp->connection = true;
    myp->query_with_result = true;
    obj_call_init(obj);
    return obj;
}

//	escape_string(string)
//
static VALUE escape_string(VALUE klass, VALUE str)
{
    Check_SafeStr(str);
    VALUE ret = str_new(0, (RSTRING(str)->len)*2+1);
    RSTRING(ret)->len = mysql_escape_string(RSTRING(ret)->ptr, RSTRING(str)->ptr, RSTRING(str)->len);
    return ret;
}

//	client_info()
//
static VALUE client_info(VALUE klass)
{
    return str_new2(mysql_get_client_info());
}

//-------------------------------
// Mysql object method
//

//	affected_rows()
//
static VALUE affected_rows(VALUE obj)
{
    struct mysql *myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_affected_rows(&myp->handler));
}

//	close()
//
static VALUE my_close(VALUE obj)
{
    struct mysql *myp;
    Data_Get_Struct(obj, struct mysql, myp);
    mysql_close(&myp->handler);
    if (mysql_error(&myp->handler))
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    myp->connection = false;
    return TRUE;
}

//	create_db(db)
//
static VALUE create_db(VALUE obj, VALUE db)
{
    Check_SafeStr(db);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_create_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return TRUE;
}

//	drop_db(db)
//
static VALUE drop_db(VALUE obj, VALUE db)
{
    Check_SafeStr(db);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_drop_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return TRUE;
}

//	errno()
//
static VALUE my_errno(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_errno(&myp->handler));
}

//	error()
//
static VALUE my_error(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return str_new2(mysql_error(&myp->handler));
}

//	host_info()
//
static VALUE host_info(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return str_new2(mysql_get_host_info(&myp->handler));
}

//	proto_info()
//
static VALUE proto_info(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_get_proto_info(&myp->handler));
}

//	server_info()
//
static VALUE server_info(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return str_new2(mysql_get_server_info(&myp->handler));
}

//	info()
//
static VALUE info(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return mysql_info(&myp->handler)? str_new2(mysql_info(&myp->handler)): Qnil;
}

//	insert_id()
//
static VALUE insert_id(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_insert_id(&myp->handler));
}

//	kill(pid)
//
static VALUE my_kill(VALUE obj, VALUE pid)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    int p = NUM2INT(pid);
    if (mysql_kill(&myp->handler, p) != 0)
	Raise(eMysql, "%d: %s", p, mysql_error(&myp->handler));
    return TRUE;
}

//	list_dbs([db])
//
static VALUE list_dbs(int argc, VALUE* argv, VALUE obj)
{
    VALUE db;
    rb_scan_args(argc, argv, "01", &db);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);

    MYSQL_RES* res = mysql_list_dbs(&myp->handler, NILorSTRING(db));
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));

    unsigned int n = mysql_num_rows(res);
    VALUE ret = ary_new2(n);
    for (unsigned int i=0; i<n; i++)
	ary_store(ret, i, str_new2(mysql_fetch_row(res)[0]));
    return ret;
}

//	list_fields(table, [field])
//
static VALUE list_fields(int argc, VALUE* argv, VALUE obj)
{
    VALUE table, field;
    rb_scan_args(argc, argv, "11", &table, &field);
    Check_SafeStr(table);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);

    MYSQL_RES* res = mysql_list_fields(&myp->handler, RSTRING(table)->ptr, NILorSTRING(field));
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return Data_Wrap_Struct(cMysqlRes, 0, free_mysqlres, res);
}

//	list_processes()
//
static VALUE list_processes(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    MYSQL_RES* res = mysql_list_processes(&myp->handler);
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return Data_Wrap_Struct(cMysqlRes, 0, free_mysqlres, res);
}

//	list_tables([table])
//
static VALUE list_tables(int argc, VALUE* argv, VALUE obj)
{
    VALUE table;
    rb_scan_args(argc, argv, "01", &table);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);

    MYSQL_RES* res = mysql_list_tables(&myp->handler, NILorSTRING(table));
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));

    unsigned int n = mysql_num_rows(res);
    VALUE ret = ary_new2(n);
    for (unsigned int i=0; i<n; i++)
	ary_store(ret, i, str_new2(mysql_fetch_row(res)[0]));
    return ret;
}

//	ping()
//
static VALUE ping(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_ping(&myp->handler) != 0)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return TRUE;
}

//	reload()
//
static VALUE reload(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_reload(&myp->handler) != 0)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return TRUE;
}

//	select_db(db)
//
static VALUE select_db(VALUE obj, VALUE db)
{
    struct mysql* myp;
    Check_SafeStr(db);
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_select_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return TRUE;
}

//	shutdown()
//
static VALUE my_shutdown(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_shutdown(&myp->handler) != 0)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return TRUE;
}

//	stat()
//
static VALUE my_stat(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    char* s = mysql_stat(&myp->handler);
    if (s == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return str_new2(s);
}

//	store_result()
//
static VALUE store_result(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    MYSQL_RES* res = mysql_store_result(&myp->handler);
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return Data_Wrap_Struct(cMysqlRes, 0, free_mysqlres, res);
}

//	thread_id()
//
static VALUE thread_id(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_thread_id(&myp->handler));
}

//	use_result()
//
static VALUE use_result(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    MYSQL_RES* res = mysql_use_result(&myp->handler);
    if (res == NULL)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return Data_Wrap_Struct(cMysqlRes, 0, free_mysqlres, res);
}

//	query(sql)
//
static VALUE query(VALUE obj, VALUE sql)
{
    Check_SafeStr(sql);
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_real_query(&myp->handler, RSTRING(sql)->ptr, RSTRING(sql)->len) != 0)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    if (myp->query_with_result == false)
	return TRUE;
    if (mysql_num_fields(&myp->handler) == 0)
	return TRUE;
    return store_result(obj);
}

//	query_with_result()
//
static VALUE query_with_result(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return myp->query_with_result? TRUE: FALSE;
}

//	query_with_result=(bool)
//
static VALUE query_with_result_set(VALUE obj, VALUE flag)
{
    bool f = true;
    if (TYPE(flag) == T_TRUE)
	f = true;
    else if (TYPE(flag) == T_NIL || TYPE(flag) == T_FALSE)
	f = false;
    else
	TypeError("invalid type, required true or false.");

    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    bool old = myp->query_with_result;
    myp->query_with_result = f;
    return old? TRUE: FALSE;
}

//-------------------------------
// MysqlRes object method
//

//	data_seek(offset)
//
static VALUE data_seek(VALUE obj, VALUE offset)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    mysql_data_seek(resp, NUM2INT(offset));
    return TRUE;
}

//	duplicate field (internal)
//	return value needed free
//
static MYSQL_FIELD* duplicate_field(MYSQL_FIELD* f)
{
    MYSQL_FIELD* f2 = malloc(sizeof(*f2));
    *f2 = *f;
    f2->name = f->name? strdup(f->name): NULL;
    f2->table = f->table? strdup(f->table): NULL;
    f2->def = f->def? strdup(f->def): NULL;
    return f2;
}

//	fetch_field()
//
static VALUE fetch_field(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return Data_Wrap_Struct(cMysqlField, 0, free_mysqlfield, duplicate_field(mysql_fetch_field(resp)));
}

//	fetch_fields()
//
static VALUE fetch_fields(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    MYSQL_FIELD* f = mysql_fetch_fields(resp);
    unsigned int n = mysql_num_fields(resp);
    VALUE ret = ary_new2(n);
    for (unsigned int i=0; i<n; i++)
	ary_store(ret, i, Data_Wrap_Struct(cMysqlField, 0, free_mysqlfield, duplicate_field(&f[i])));
    return ret;
}

//	fetch_field_direct(nr)
//
static VALUE fetch_field_direct(VALUE obj, VALUE nr)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    unsigned int max = mysql_num_fields(resp);
    unsigned int n = NUM2INT(nr);
    if (n >= max)
	Raise(eMysql, "%d: out of range (max: %d)", n, max-1);
    MYSQL_FIELD* f = duplicate_field(&mysql_fetch_field_direct(resp, n));
    return Data_Wrap_Struct(cMysqlField, 0, free_mysqlfield, f);
}

//	fetch_lengths()
//
static VALUE fetch_lengths(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    unsigned int n = mysql_num_fields(resp);
    unsigned long* lengths = mysql_fetch_lengths(resp);
    VALUE ary = ary_new2(n);
    for (unsigned int i=0; i<n; i++)
	ary_store(ary, i, INT2NUM(lengths[i]));
    return ary;
}

//	fetch_row()
//
static VALUE fetch_row(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    MYSQL_ROW row = mysql_fetch_row(resp);
    if (row == NULL)
	return Qnil;
    unsigned int n = mysql_num_fields(resp);
    unsigned long* lengths = mysql_fetch_lengths(resp);
    VALUE ary = ary_new2(n);
    for (unsigned int i=0; i<n; i++)
	ary_store(ary, i, row[i]? str_new(row[i], lengths[i]): Qnil);
    return ary;
}

//	field_seek(offset)
//
static VALUE field_seek(VALUE obj, VALUE offset)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM(mysql_field_seek(resp, NUM2INT(offset)));
}

//	field_tell()
//
static VALUE field_tell(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM(mysql_field_tell(resp));
}

//	num_fields()
//
static VALUE num_fields(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM(mysql_num_fields(resp));
}

//	num_rows()
//
static VALUE num_rows(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM(mysql_num_rows(resp));
}

//	row_seek(offset)
//
static VALUE row_seek(VALUE obj, VALUE offset)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM((int)mysql_row_seek(resp, (MYSQL_ROWS*)NUM2INT(offset)));
}

//	row_tell()
//
static VALUE row_tell(VALUE obj)
{
    MYSQL_RES* resp;
    Data_Get_Struct(obj, MYSQL_RES, resp);
    return INT2NUM((int)mysql_row_tell(resp));
}

//	each {...}
//
static VALUE each(VALUE obj)
{
    VALUE row;
    while ((row = fetch_row(obj)) != Qnil)
	rb_yield(row);
    return TRUE;
}

//-------------------------------
// MysqlField object method
//

//	name
//
static VALUE field_name(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return f->name? str_new2(f->name): Qnil;
}

//	table
//
static VALUE field_table(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return f->table? str_new2(f->table): Qnil;
}

//	def
//
static VALUE field_def(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return f->def? str_new2(f->def): Qnil;
}

//	type
//
static VALUE field_type(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return INT2NUM(f->type);
}

//	length
//
static VALUE field_length(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return INT2NUM(f->length);
}

//	max_length
//
static VALUE field_max_length(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return INT2NUM(f->max_length);
}

//	flags
//
static VALUE field_flags(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return INT2NUM(f->flags);
}

//	decimals
//
static VALUE field_decimals(VALUE obj)
{
    MYSQL_FIELD* f;
    Data_Get_Struct(obj, MYSQL_FIELD, f);
    return INT2NUM(f->decimals);
}

//-------------------------------
//	Initialize
//

void Init_mysql(void)
{
    extern VALUE eStandardError;
    cMysql = rb_define_class("Mysql", cObject);
    cMysqlRes = rb_define_class("MysqlRes", cObject);
    cMysqlField = rb_define_class("MysqlField", cObject);
    eMysql = rb_define_class("MysqlError", eStandardError);

    // Mysql class method
    rb_define_singleton_method(cMysql, "real_connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "new", real_connect, -1);
    rb_define_singleton_method(cMysql, "escape_string", escape_string, 1);
    rb_define_singleton_method(cMysql, "quote", escape_string, 1);
    rb_define_singleton_method(cMysql, "client_info", client_info, 0);
    rb_define_singleton_method(cMysql, "get_client_info", client_info, 0);

    // Mysql object method
    rb_define_method(cMysql, "affected_rows", affected_rows, 0);
    rb_define_method(cMysql, "close", my_close, 0);
    rb_define_method(cMysql, "create_db", create_db, 1);
    rb_define_method(cMysql, "drop_db", drop_db, 1);
    rb_define_method(cMysql, "errno", my_errno, 0);
    rb_define_method(cMysql, "error", my_error, 0);
    rb_define_method(cMysql, "get_host_info", host_info, 0);
    rb_define_method(cMysql, "host_info", host_info, 0);
    rb_define_method(cMysql, "get_proto_info", proto_info, 0);
    rb_define_method(cMysql, "proto_info", proto_info, 0);
    rb_define_method(cMysql, "get_server_info", server_info, 0);
    rb_define_method(cMysql, "server_info", server_info, 0);
    rb_define_method(cMysql, "info", info, 0);
    rb_define_method(cMysql, "insert_id", insert_id, 0);
    rb_define_method(cMysql, "kill", my_kill, 1);
    rb_define_method(cMysql, "list_dbs", list_dbs, -1);
    rb_define_method(cMysql, "list_fields", list_fields, -1);
    rb_define_method(cMysql, "list_processes", list_processes, 0);
    rb_define_method(cMysql, "list_tables", list_tables, -1);
    rb_define_method(cMysql, "ping", ping, 0);
    rb_define_method(cMysql, "query", query, 1);
    rb_define_method(cMysql, "reload", reload, 0);
    rb_define_method(cMysql, "select_db", select_db, 1);
    rb_define_method(cMysql, "shutdown", my_shutdown, 0);
    rb_define_method(cMysql, "stat", my_stat, 0);
    rb_define_method(cMysql, "store_result", store_result, 0);
    rb_define_method(cMysql, "thread_id", thread_id, 0);
    rb_define_method(cMysql, "use_result", use_result, 0);
    rb_define_method(cMysql, "query_with_result", query_with_result, 0);
    rb_define_method(cMysql, "query_with_result=", query_with_result_set, 1);

    // Mysql constant
    rb_define_const(cMysql, "CLIENT_FOUND_ROWS", INT2NUM(CLIENT_FOUND_ROWS));
    rb_define_const(cMysql, "CLIENT_NO_SCHEMA", INT2NUM(CLIENT_NO_SCHEMA));
    rb_define_const(cMysql, "CLIENT_COMPRESS", INT2NUM(CLIENT_COMPRESS));
    rb_define_const(cMysql, "CLIENT_ODBC", INT2NUM(CLIENT_ODBC));

    // MysqlRes object method
    rb_define_method(cMysqlRes, "data_seek", data_seek, 1);
    rb_define_method(cMysqlRes, "fetch_field", fetch_field, 0);
    rb_define_method(cMysqlRes, "fetch_fields", fetch_fields, 0);
    rb_define_method(cMysqlRes, "fetch_field_direct", fetch_field_direct, 1);
    rb_define_method(cMysqlRes, "fetch_lengths", fetch_lengths, 0);
    rb_define_method(cMysqlRes, "fetch_row", fetch_row, 0);
    rb_define_method(cMysqlRes, "field_seek", field_seek, 1);
    rb_define_method(cMysqlRes, "field_tell", field_tell, 0);
    rb_define_method(cMysqlRes, "num_fields", num_fields, 0);
    rb_define_method(cMysqlRes, "num_rows", num_rows, 0);
    rb_define_method(cMysqlRes, "row_seek", row_seek, 1);
    rb_define_method(cMysqlRes, "row_tell", row_tell, 0);
    rb_define_method(cMysqlRes, "each", each, 0);

    // MysqlField object method
    rb_define_method(cMysqlField, "name", field_name, 0);
    rb_define_method(cMysqlField, "table", field_table, 0);
    rb_define_method(cMysqlField, "def", field_def, 0);
    rb_define_method(cMysqlField, "type", field_type, 0);
    rb_define_method(cMysqlField, "length", field_length, 0);
    rb_define_method(cMysqlField, "max_length", field_max_length, 0);
    rb_define_method(cMysqlField, "flags", field_flags, 0);
    rb_define_method(cMysqlField, "decimals", field_decimals, 0);

    // MysqlField constant: TYPE
    rb_define_const(cMysqlField, "TYPE_TINY", INT2NUM(FIELD_TYPE_TINY));
    rb_define_const(cMysqlField, "TYPE_ENUM", INT2NUM(FIELD_TYPE_ENUM));
    rb_define_const(cMysqlField, "TYPE_DECIMAL", INT2NUM(FIELD_TYPE_DECIMAL));
    rb_define_const(cMysqlField, "TYPE_SHORT", INT2NUM(FIELD_TYPE_SHORT));
    rb_define_const(cMysqlField, "TYPE_LONG", INT2NUM(FIELD_TYPE_LONG));
    rb_define_const(cMysqlField, "TYPE_FLOAT", INT2NUM(FIELD_TYPE_FLOAT));
    rb_define_const(cMysqlField, "TYPE_DOUBLE", INT2NUM(FIELD_TYPE_DOUBLE));
    rb_define_const(cMysqlField, "TYPE_NULL", INT2NUM(FIELD_TYPE_NULL));
    rb_define_const(cMysqlField, "TYPE_TIMESTAMP", INT2NUM(FIELD_TYPE_TIMESTAMP));
    rb_define_const(cMysqlField, "TYPE_LONGLONG", INT2NUM(FIELD_TYPE_LONGLONG));
    rb_define_const(cMysqlField, "TYPE_INT24", INT2NUM(FIELD_TYPE_INT24));
    rb_define_const(cMysqlField, "TYPE_DATE", INT2NUM(FIELD_TYPE_DATE));
    rb_define_const(cMysqlField, "TYPE_TIME", INT2NUM(FIELD_TYPE_TIME));
    rb_define_const(cMysqlField, "TYPE_DATETIME", INT2NUM(FIELD_TYPE_DATETIME));
    rb_define_const(cMysqlField, "TYPE_YEAR", INT2NUM(FIELD_TYPE_YEAR));
    rb_define_const(cMysqlField, "TYPE_SET", INT2NUM(FIELD_TYPE_SET));
    rb_define_const(cMysqlField, "TYPE_BLOB", INT2NUM(FIELD_TYPE_BLOB));
    rb_define_const(cMysqlField, "TYPE_STRING", INT2NUM(FIELD_TYPE_STRING));
    rb_define_const(cMysqlField, "TYPE_CHAR", INT2NUM(FIELD_TYPE_CHAR));

    // MysqlField constant: FLAG
    rb_define_const(cMysqlField, "NOT_NULL_FLAG", INT2NUM(NOT_NULL_FLAG));
    rb_define_const(cMysqlField, "PRI_KEY_FLAG", INT2NUM(PRI_KEY_FLAG));
    rb_define_const(cMysqlField, "UNIQUE_KEY_FLAG", INT2NUM(UNIQUE_KEY_FLAG));
    rb_define_const(cMysqlField, "MULTIPLE_KEY_FLAG", INT2NUM(MULTIPLE_KEY_FLAG));
    rb_define_const(cMysqlField, "BLOB_FLAG", INT2NUM(BLOB_FLAG));
    rb_define_const(cMysqlField, "UNSIGNED_FLAG", INT2NUM(UNSIGNED_FLAG));
    rb_define_const(cMysqlField, "ZEROFILL_FLAG", INT2NUM(ZEROFILL_FLAG));
    rb_define_const(cMysqlField, "BINARY_FLAG", INT2NUM(BINARY_FLAG));
    rb_define_const(cMysqlField, "ENUM_FLAG", INT2NUM(ENUM_FLAG));
    rb_define_const(cMysqlField, "AUTO_INCREMENT_FLAG", INT2NUM(AUTO_INCREMENT_FLAG));
    rb_define_const(cMysqlField, "TIMESTAMP_FLAG", INT2NUM(TIMESTAMP_FLAG));
}
