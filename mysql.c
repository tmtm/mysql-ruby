/*	ruby mysql module
 *	$Id: mysql.c,v 1.14 1999/09/23 17:21:48 tommy Exp $
 */

#include "ruby.h"
#include <mysql/mysql.h>
#include <mysql/errmsg.h>

#ifdef str2cstr			/* checking ruby 1.3.x */
#define	TRUE		Qtrue
#define	FALSE		Qfalse
#define	exc_new2	rb_exc_new2
#define	_rb_raise	rb_exc_raise
#define	str_new		rb_str_new
#define	str_new2	rb_str_new2
#define	ary_new2	rb_ary_new2
#define	ary_store	rb_ary_store
#define obj_alloc	rb_obj_alloc
#define	hash_new	rb_hash_new
#define	hash_aset	rb_hash_aset
#define	hash_aref	rb_hash_aref
#define	hash_freeze	rb_hash_freeze
#define	eStandardError	rb_eStandardError
#define	TypeError(m)	rb_raise(rb_eTypeError, m)
#define	cObject		rb_cObject
#define	Raise		rb_raise
#else
#define	_rb_raise	rb_raise
#endif

#define NILorSTRING(obj)	(NIL_P(obj)? NULL: STR2CSTR(obj))
#define NILorINT(obj)		(NIL_P(obj)? 0: NUM2INT(obj))

#define GetMysqlStruct(obj)	(Check_Type(obj, T_DATA), (struct mysql*)DATA_PTR(obj))
#define GetHandler(obj)		(Check_Type(obj, T_DATA), &(((struct mysql*)DATA_PTR(obj))->handler))
#define GetMysqlRes(obj)	(Check_Type(obj, T_DATA), (MYSQL_RES*)DATA_PTR(obj))
#define MysqlRes2Obj(res)	(Data_Wrap_Struct(cMysqlRes, 0, mysql_free_result, res))

VALUE cMysql;
VALUE cMysqlRes;
VALUE cMysqlField;
VALUE eMysql;

struct mysql {
    MYSQL handler;
    char connection;
    char query_with_result;
};

/*	free Mysql class object		*/
static void free_mysql(struct mysql* my)
{
    if (my->connection)
	mysql_close(&my->handler);
    free(my);
}

static void mysql_raise(MYSQL* m)
{
    VALUE e = exc_new2(eMysql, mysql_error(m));
    rb_iv_set(e, "errno", INT2FIX(mysql_errno(m)));
    _rb_raise(e);
}

/*-------------------------------
 * Mysql class method
 */

/*	real_connect(host=nil, user=nil, passwd=nil, db=nil, port=nil, sock=nil, flag=nil)	*/
static VALUE real_connect(int argc, VALUE* argv, VALUE klass)
{
    VALUE host, user, passwd, db, port, sock, flag;
    char *h, *u, *p, *d, *s;
    uint pp, f;
    MYSQL my;
    struct mysql* myp;

#if MYSQL_VERSION_ID >= 32200
    rb_scan_args(argc, argv, "07", &host, &user, &passwd, &db, &port, &sock, &flag);
    d = NILorSTRING(db);
    f = NILorINT(flag);
#elif MYSQL_VERSION_ID >= 32115
    rb_scan_args(argc, argv, "06", &host, &user, &passwd, &port, &sock, &flag);
    f = NILorINT(flag);
#else
    rb_scan_args(argc, argv, "05", &host, &user, &passwd, &port, &sock);
#endif
    h = NILorSTRING(host);
    u = NILorSTRING(user);
    p = NILorSTRING(passwd);
    pp = NILorINT(port);
    s = NILorSTRING(sock);

#if MYSQL_VERSION_ID >= 32200
    mysql_init(&my);
    if (mysql_real_connect(&my, h, u, p, d, pp, s, f) == NULL)
#elif MYSQL_VERSION_ID >= 32115
    if (mysql_real_connect(&my, h, u, p, pp, s, f) == NULL)
#else
    if (mysql_real_connect(&my, h, u, p, pp, s) == NULL)
#endif
	mysql_raise(&my);

    myp = malloc(sizeof(*myp));
    myp->handler = my;
    myp->connection = TRUE;
    myp->query_with_result = TRUE;
    return Data_Wrap_Struct(klass, 0, free_mysql, myp);
}

/*	escape_string(string)	*/
static VALUE escape_string(VALUE klass, VALUE str)
{
    VALUE ret;
    Check_Type(str, T_STRING);
    ret = str_new(0, (RSTRING(str)->len)*2+1);
    RSTRING(ret)->len = mysql_escape_string(RSTRING(ret)->ptr, RSTRING(str)->ptr, RSTRING(str)->len);
    return ret;
}

/*	client_info()	*/
static VALUE client_info(VALUE klass)
{
    return str_new2(mysql_get_client_info());
}

/*-------------------------------
 * Mysql object method
 */

/*	affected_rows()	*/
static VALUE affected_rows(VALUE obj)
{
    return INT2NUM(mysql_affected_rows(GetHandler(obj)));
}

/*	close()		*/
static VALUE my_close(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    mysql_close(m);
    if (mysql_errno(m))
	mysql_raise(m);
    GetMysqlStruct(obj)->connection = FALSE;
    return TRUE;
}

/*	create_db(db)	*/
static VALUE create_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_create_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	drop_db(db)	*/
static VALUE drop_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_drop_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	errno()		*/
static VALUE my_errno(VALUE obj)
{
    return INT2NUM(mysql_errno(GetHandler(obj)));
}

/*	error()		*/
static VALUE my_error(VALUE obj)
{
    return str_new2(mysql_error(GetHandler(obj)));
}

/*	host_info()	*/
static VALUE host_info(VALUE obj)
{
    return str_new2(mysql_get_host_info(GetHandler(obj)));
}

/*	proto_info()	*/
static VALUE proto_info(VALUE obj)
{
    return INT2NUM(mysql_get_proto_info(GetHandler(obj)));
}

/*	server_info()	*/
static VALUE server_info(VALUE obj)
{
    return str_new2(mysql_get_server_info(GetHandler(obj)));
}

/*	info()		*/
static VALUE info(VALUE obj)
{
    char* p = mysql_info(GetHandler(obj));
    return p? str_new2(p): Qnil;
}

/*	insert_id()	*/
static VALUE insert_id(VALUE obj)
{
    return INT2NUM(mysql_insert_id(GetHandler(obj)));
}

/*	kill(pid)	*/
static VALUE my_kill(VALUE obj, VALUE pid)
{
    int p = NUM2INT(pid);
    MYSQL* m = GetHandler(obj);
    if (mysql_kill(m, p) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	list_dbs(db=nil)	*/
static VALUE list_dbs(int argc, VALUE* argv, VALUE obj)
{
    unsigned int i, n;
    VALUE db, ret;
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res;

    rb_scan_args(argc, argv, "01", &db);
    res = mysql_list_dbs(m, NILorSTRING(db));
    if (res == NULL)
	mysql_raise(m);

    n = mysql_num_rows(res);
    ret = ary_new2(n);
    for (i=0; i<n; i++)
	ary_store(ret, i, str_new2(mysql_fetch_row(res)[0]));
    mysql_free_result(res);
    return ret;
}

/*	list_fields(table, field=nil)	*/
static VALUE list_fields(int argc, VALUE* argv, VALUE obj)
{
    VALUE table, field;
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res;
    rb_scan_args(argc, argv, "11", &table, &field);
    res = mysql_list_fields(m, STR2CSTR(table), NILorSTRING(field));
    if (res == NULL)
	mysql_raise(m);
    return MysqlRes2Obj(res);
}

/*	list_processes()	*/
static VALUE list_processes(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res = mysql_list_processes(m);
    if (res == NULL)
	mysql_raise(m);
    return MysqlRes2Obj(res);
}

/*	list_tables(table=nil)	*/
static VALUE list_tables(int argc, VALUE* argv, VALUE obj)
{
    VALUE table;
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res;
    unsigned int i, n;
    VALUE ret;

    rb_scan_args(argc, argv, "01", &table);
    res = mysql_list_tables(m, NILorSTRING(table));
    if (res == NULL)
	mysql_raise(m);

    n = mysql_num_rows(res);
    ret = ary_new2(n);
    for (i=0; i<n; i++)
	ary_store(ret, i, str_new2(mysql_fetch_row(res)[0]));
    mysql_free_result(res);
    return ret;
}

/*	ping()		*/
static VALUE ping(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_ping(m) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	refresh(r)	*/
static VALUE refresh(VALUE obj, VALUE r)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_refresh(m, NUM2INT(r)) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	reload()	*/
static VALUE reload(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_reload(m) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	select_db(db)	*/
static VALUE select_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_select_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	shutdown()	*/
static VALUE my_shutdown(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_shutdown(m) != 0)
	mysql_raise(m);
    return TRUE;
}

/*	stat()		*/
static VALUE my_stat(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    char* s = mysql_stat(m);
    if (s == NULL)
	mysql_raise(m);
    return str_new2(s);
}

/*	store_result()	*/
static VALUE store_result(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res = mysql_store_result(m);
    if (res == NULL)
	mysql_raise(m);
    return MysqlRes2Obj(res);
}

/*	thread_id()	*/
static VALUE thread_id(VALUE obj)
{
    return INT2NUM(mysql_thread_id(GetHandler(obj)));
}

/*	use_result()	*/
static VALUE use_result(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res = mysql_use_result(m);
    if (res == NULL)
	mysql_raise(m);
    return MysqlRes2Obj(res);
}

/*	query(sql)	*/
static VALUE query(VALUE obj, VALUE sql)
{
    MYSQL* m = GetHandler(obj);
    Check_Type(sql, T_STRING);
    if (mysql_real_query(m, RSTRING(sql)->ptr, RSTRING(sql)->len) != 0)
	mysql_raise(m);
    if (GetMysqlStruct(obj)->query_with_result == FALSE)
	return TRUE;
    if (mysql_num_fields(m) == 0)
	return TRUE;
    return store_result(obj);
}

/*	query_with_result()	*/
static VALUE query_with_result(VALUE obj)
{
    return GetMysqlStruct(obj)->query_with_result? TRUE: FALSE;
}

/*	query_with_result=(flag)	*/
static VALUE query_with_result_set(VALUE obj, VALUE flag)
{
    if (TYPE(flag) != T_TRUE && TYPE(flag) != T_FALSE)
	TypeError("invalid type, required true or false.");
    GetMysqlStruct(obj)->query_with_result = flag;
    return flag;
}

/*-------------------------------
 * MysqlRes object method
 */

/*	data_seek(offset)	*/
static VALUE data_seek(VALUE obj, VALUE offset)
{
    mysql_data_seek(GetMysqlRes(obj), NUM2INT(offset));
    return TRUE;
}

/*	make MysqlField object (internal)	*/
static VALUE make_field_obj(MYSQL_FIELD* f)
{
    VALUE obj;
    VALUE hash;
    if (f == NULL)
	return Qnil;
    obj = obj_alloc(cMysqlField);
    hash = hash_new();
    if (f->name)
	hash_aset(hash, str_new2("name"), str_new2(f->name));
    if (f->table)
	hash_aset(hash, str_new2("table"), str_new2(f->table));
    if (f->def)
	hash_aset(hash, str_new2("def"), str_new2(f->def));
    hash_aset(hash, str_new2("type"), INT2NUM(f->type));
    hash_aset(hash, str_new2("length"), INT2NUM(f->length));
    hash_aset(hash, str_new2("max_length"), INT2NUM(f->max_length));
    hash_aset(hash, str_new2("flags"), INT2NUM(f->flags));
    hash_aset(hash, str_new2("decimals"), INT2NUM(f->decimals));
    hash_freeze(hash);
    rb_iv_set(obj, "hash", hash);
    return obj;
}

/*	fetch_field()	*/
static VALUE fetch_field(VALUE obj)
{
    return make_field_obj(mysql_fetch_field(GetMysqlRes(obj)));
}

/*	fetch_fields()	*/
static VALUE fetch_fields(VALUE obj)
{
    MYSQL_RES* res = GetMysqlRes(obj);
    MYSQL_FIELD* f = mysql_fetch_fields(res);
    unsigned int n = mysql_num_fields(res);
    VALUE ret = ary_new2(n);
    unsigned int i;
    for (i=0; i<n; i++)
	ary_store(ret, i, make_field_obj(&f[i]));
    return ret;
}

/*	fetch_field_direct(nr)	*/
static VALUE fetch_field_direct(VALUE obj, VALUE nr)
{
    MYSQL_RES* res = GetMysqlRes(obj);
    unsigned int max = mysql_num_fields(res);
    unsigned int n = NUM2INT(nr);
    if (n >= max)
	Raise(eMysql, "%d: out of range (max: %d)", n, max-1);
#if MYSQL_VERSION_ID >= 32226
    return make_field_obj(mysql_fetch_field_direct(res, n));
#else
    return make_field_obj(&mysql_fetch_field_direct(res, n));
#endif
}

/*	fetch_lengths()		*/
static VALUE fetch_lengths(VALUE obj)
{
    MYSQL_RES* res = GetMysqlRes(obj);
    unsigned int n = mysql_num_fields(res);
    unsigned long* lengths = mysql_fetch_lengths(res);
    VALUE ary;
    unsigned int i;
    if (lengths == NULL)
	return Qnil;
    ary = ary_new2(n);
    for (i=0; i<n; i++)
	ary_store(ary, i, INT2NUM(lengths[i]));
    return ary;
}

/*	fetch_row()	*/
static VALUE fetch_row(VALUE obj)
{
    MYSQL_RES* res = GetMysqlRes(obj);
    unsigned int n = mysql_num_fields(res);
    MYSQL_ROW row = mysql_fetch_row(res);
    unsigned long* lengths = mysql_fetch_lengths(res);
    VALUE ary;
    unsigned int i;
    if (row == NULL)
	return Qnil;
    ary = ary_new2(n);
    for (i=0; i<n; i++)
	ary_store(ary, i, row[i]? str_new(row[i], lengths[i]): Qnil);
    return ary;
}

/*	fetch_hash2 (internal)	*/
static VALUE fetch_hash2(VALUE obj, VALUE with_table)
{
    MYSQL_RES* res = GetMysqlRes(obj);
    unsigned int n = mysql_num_fields(res);
    MYSQL_ROW row = mysql_fetch_row(res);
    unsigned long* lengths = mysql_fetch_lengths(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    unsigned int i;
    VALUE hash;
    if (row == NULL)
	return Qnil;
    hash = hash_new();
    for (i=0; i<n; i++) {
	VALUE col;
	if (row[i] == NULL)
	    continue;
	if (with_table == Qnil || with_table == FALSE)
	    col = str_new2(fields[i].name);
	else {
	    col = str_new(fields[i].table, strlen(fields[i].table)+strlen(fields[i].name)+1);
	    RSTRING(col)->ptr[strlen(fields[i].table)] = '.';
	    strcpy(RSTRING(col)->ptr+strlen(fields[i].table)+1, fields[i].name);
	}
	hash_aset(hash, col, row[i]? str_new(row[i], lengths[i]): Qnil);
    }
    return hash;
}

/*	fetch_hash(with_table=false)	*/
static VALUE fetch_hash(int argc, VALUE* argv, VALUE obj)
{
    VALUE with_table;
    rb_scan_args(argc, argv, "01", &with_table);
    if (with_table == Qnil)
	with_table = FALSE;
    return fetch_hash2(obj, with_table);
}

/*	field_seek(offset)	*/
static VALUE field_seek(VALUE obj, VALUE offset)
{
    return INT2NUM(mysql_field_seek(GetMysqlRes(obj), NUM2INT(offset)));
}

/*	field_tell()		*/
static VALUE field_tell(VALUE obj)
{
    return INT2NUM(mysql_field_tell(GetMysqlRes(obj)));
}

/*	num_fields()		*/
static VALUE num_fields(VALUE obj)
{
    return INT2NUM(mysql_num_fields(GetMysqlRes(obj)));
}

/*	num_rows()	*/
static VALUE num_rows(VALUE obj)
{
    return INT2NUM(mysql_num_rows(GetMysqlRes(obj)));
}

/*	row_seek(offset)	*/
static VALUE row_seek(VALUE obj, VALUE offset)
{
    return INT2NUM((int)mysql_row_seek(GetMysqlRes(obj), (MYSQL_ROWS*)NUM2INT(offset)));
}

/*	row_tell()	*/
static VALUE row_tell(VALUE obj)
{
    return INT2NUM((int)mysql_row_tell(GetMysqlRes(obj)));
}

/*	each {...}	*/
static VALUE each(VALUE obj)
{
    VALUE row;
    while ((row = fetch_row(obj)) != Qnil)
	rb_yield(row);
    return TRUE;
}

/*	each_hash(with_table=false) {...}	*/
static VALUE each_hash(int argc, VALUE* argv, VALUE obj)
{
    VALUE with_table;
    VALUE hash;
    rb_scan_args(argc, argv, "01", &with_table);
    if (with_table == Qnil)
	with_table = FALSE;
    while ((hash = fetch_hash2(obj, with_table)) != Qnil)
	rb_yield(hash);
    return TRUE;
}

/*-------------------------------
 * MysqlField object method
 */

/*	hash	*/
static VALUE field_hash(VALUE obj)
{
    return rb_iv_get(obj, "hash");
}

/*	inspect	*/
static VALUE field_inspect(VALUE obj)
{
    VALUE n = hash_aref(field_hash(obj), str_new2("name"));
    VALUE s = str_new(0, RSTRING(n)->len + 14);
    sprintf(RSTRING(s)->ptr, "#<MysqlField:%s>", RSTRING(n)->ptr);
    return s;
}

#define DefineMysqlFieldMemberMethod(m)\
static VALUE field_##m(VALUE obj)\
{return hash_aref(field_hash(obj), str_new2(#m));}

DefineMysqlFieldMemberMethod(name)
DefineMysqlFieldMemberMethod(table)
DefineMysqlFieldMemberMethod(def)
DefineMysqlFieldMemberMethod(type)
DefineMysqlFieldMemberMethod(length)
DefineMysqlFieldMemberMethod(max_length)
DefineMysqlFieldMemberMethod(flags)
DefineMysqlFieldMemberMethod(decimals)

/*-------------------------------
 * MysqlError object method
 */

static VALUE error_error(VALUE obj)
{
    return rb_iv_get(obj, "mesg");
}

static VALUE error_errno(VALUE obj)
{
    return rb_iv_get(obj, "errno");
}

/*-------------------------------
 *	Initialize
 */

void Init_mysql(void)
{
    extern VALUE eStandardError;

    cMysql = rb_define_class("Mysql", cObject);
    cMysqlRes = rb_define_class("MysqlRes", cObject);
    cMysqlField = rb_define_class("MysqlField", cObject);
    eMysql = rb_define_class("MysqlError", eStandardError);

    /* Mysql class method */
    rb_define_singleton_method(cMysql, "real_connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "new", real_connect, -1);
    rb_define_singleton_method(cMysql, "escape_string", escape_string, 1);
    rb_define_singleton_method(cMysql, "quote", escape_string, 1);
    rb_define_singleton_method(cMysql, "client_info", client_info, 0);
    rb_define_singleton_method(cMysql, "get_client_info", client_info, 0);

    /* Mysql object method */
    rb_define_method(cMysql, "escape_string", escape_string, 1);
    rb_define_method(cMysql, "quote", escape_string, 1);
    rb_define_method(cMysql, "client_info", client_info, 0);
    rb_define_method(cMysql, "get_client_info", client_info, 0);
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
#if MYSQL_VERSION_ID >= 32200
    rb_define_method(cMysql, "ping", ping, 0);
#endif
    rb_define_method(cMysql, "query", query, 1);
    rb_define_method(cMysql, "refresh", refresh, 1);
    rb_define_method(cMysql, "reload", reload, 0);
    rb_define_method(cMysql, "select_db", select_db, 1);
    rb_define_method(cMysql, "shutdown", my_shutdown, 0);
    rb_define_method(cMysql, "stat", my_stat, 0);
    rb_define_method(cMysql, "store_result", store_result, 0);
    rb_define_method(cMysql, "thread_id", thread_id, 0);
    rb_define_method(cMysql, "use_result", use_result, 0);
    rb_define_method(cMysql, "query_with_result", query_with_result, 0);
    rb_define_method(cMysql, "query_with_result=", query_with_result_set, 1);

    /* Mysql constant */
    rb_define_const(cMysql, "REFRESH_GRANT", INT2NUM(REFRESH_GRANT));
    rb_define_const(cMysql, "REFRESH_LOG", INT2NUM(REFRESH_LOG));
    rb_define_const(cMysql, "REFRESH_TABLES", INT2NUM(REFRESH_TABLES));
#ifdef REFRESH_HOSTS
    rb_define_const(cMysql, "REFRESH_HOSTS", INT2NUM(REFRESH_HOSTS));
#endif
#ifdef REFRESH_STATUS
    rb_define_const(cMysql, "REFRESH_STATUS", INT2NUM(REFRESH_STATUS));
#endif
#ifdef CLIENT_FOUND_ROWS
    rb_define_const(cMysql, "CLIENT_FOUND_ROWS", INT2NUM(CLIENT_FOUND_ROWS));
#endif
#ifdef CLIENT_NO_SCHEMA
    rb_define_const(cMysql, "CLIENT_NO_SCHEMA", INT2NUM(CLIENT_NO_SCHEMA));
#endif
#ifdef CLIENT_COMPRESS
    rb_define_const(cMysql, "CLIENT_COMPRESS", INT2NUM(CLIENT_COMPRESS));
#endif
#ifdef CLIENT_ODBC
    rb_define_const(cMysql, "CLIENT_ODBC", INT2NUM(CLIENT_ODBC));
#endif

    /* MysqlRes object method */
    rb_define_method(cMysqlRes, "data_seek", data_seek, 1);
    rb_define_method(cMysqlRes, "fetch_field", fetch_field, 0);
    rb_define_method(cMysqlRes, "fetch_fields", fetch_fields, 0);
    rb_define_method(cMysqlRes, "fetch_field_direct", fetch_field_direct, 1);
    rb_define_method(cMysqlRes, "fetch_lengths", fetch_lengths, 0);
    rb_define_method(cMysqlRes, "fetch_row", fetch_row, 0);
    rb_define_method(cMysqlRes, "fetch_hash", fetch_hash, -1);
    rb_define_method(cMysqlRes, "field_seek", field_seek, 1);
    rb_define_method(cMysqlRes, "field_tell", field_tell, 0);
    rb_define_method(cMysqlRes, "num_fields", num_fields, 0);
    rb_define_method(cMysqlRes, "num_rows", num_rows, 0);
    rb_define_method(cMysqlRes, "row_seek", row_seek, 1);
    rb_define_method(cMysqlRes, "row_tell", row_tell, 0);
    rb_define_method(cMysqlRes, "each", each, 0);
    rb_define_method(cMysqlRes, "each_hash", each_hash, -1);

    /* MysqlField object method */
    rb_define_method(cMysqlField, "name", field_name, 0);
    rb_define_method(cMysqlField, "table", field_table, 0);
    rb_define_method(cMysqlField, "def", field_def, 0);
    rb_define_method(cMysqlField, "type", field_type, 0);
    rb_define_method(cMysqlField, "length", field_length, 0);
    rb_define_method(cMysqlField, "max_length", field_max_length, 0);
    rb_define_method(cMysqlField, "flags", field_flags, 0);
    rb_define_method(cMysqlField, "decimals", field_decimals, 0);
    rb_define_method(cMysqlField, "hash", field_hash, 0);
    rb_define_method(cMysqlField, "inspect", field_inspect, 0);

    /* MysqlField constant: TYPE */
    rb_define_const(cMysqlField, "TYPE_TINY", INT2NUM(FIELD_TYPE_TINY));
#if MYSQL_VERSION_ID >= 32115
    rb_define_const(cMysqlField, "TYPE_ENUM", INT2NUM(FIELD_TYPE_ENUM));
#endif
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
#if MYSQL_VERSION_ID >= 32130
    rb_define_const(cMysqlField, "TYPE_YEAR", INT2NUM(FIELD_TYPE_YEAR));
#endif
    rb_define_const(cMysqlField, "TYPE_SET", INT2NUM(FIELD_TYPE_SET));
    rb_define_const(cMysqlField, "TYPE_BLOB", INT2NUM(FIELD_TYPE_BLOB));
    rb_define_const(cMysqlField, "TYPE_STRING", INT2NUM(FIELD_TYPE_STRING));
    rb_define_const(cMysqlField, "TYPE_CHAR", INT2NUM(FIELD_TYPE_CHAR));

    /* MysqlField constant: FLAG */
    rb_define_const(cMysqlField, "NOT_NULL_FLAG", INT2NUM(NOT_NULL_FLAG));
    rb_define_const(cMysqlField, "PRI_KEY_FLAG", INT2NUM(PRI_KEY_FLAG));
    rb_define_const(cMysqlField, "UNIQUE_KEY_FLAG", INT2NUM(UNIQUE_KEY_FLAG));
    rb_define_const(cMysqlField, "MULTIPLE_KEY_FLAG", INT2NUM(MULTIPLE_KEY_FLAG));
    rb_define_const(cMysqlField, "BLOB_FLAG", INT2NUM(BLOB_FLAG));
    rb_define_const(cMysqlField, "UNSIGNED_FLAG", INT2NUM(UNSIGNED_FLAG));
    rb_define_const(cMysqlField, "ZEROFILL_FLAG", INT2NUM(ZEROFILL_FLAG));
    rb_define_const(cMysqlField, "BINARY_FLAG", INT2NUM(BINARY_FLAG));
#ifdef ENUM_FLAG
    rb_define_const(cMysqlField, "ENUM_FLAG", INT2NUM(ENUM_FLAG));
#endif
#ifdef AUTO_INCREMENT_FLAG
    rb_define_const(cMysqlField, "AUTO_INCREMENT_FLAG", INT2NUM(AUTO_INCREMENT_FLAG));
#endif
#ifdef TIMESTAMP_FLAG
    rb_define_const(cMysqlField, "TIMESTAMP_FLAG", INT2NUM(TIMESTAMP_FLAG));
#endif

    /* MysqlError object method */
    rb_define_method(eMysql, "error", error_error, 0);
    rb_define_method(eMysql, "errno", error_errno, 0);

    /* MysqlError constant */
    rb_define_const(eMysql, "CR_UNKNOWN_ERROR", INT2NUM(CR_UNKNOWN_ERROR));
    rb_define_const(eMysql, "CR_SOCKET_CREATE_ERROR", INT2NUM(CR_SOCKET_CREATE_ERROR));
    rb_define_const(eMysql, "CR_CONNECTION_ERROR", INT2NUM(CR_CONNECTION_ERROR));
    rb_define_const(eMysql, "CR_CONN_HOST_ERROR", INT2NUM(CR_CONN_HOST_ERROR));
    rb_define_const(eMysql, "CR_IPSOCK_ERROR", INT2NUM(CR_IPSOCK_ERROR));
    rb_define_const(eMysql, "CR_UNKNOWN_HOST", INT2NUM(CR_UNKNOWN_HOST));
    rb_define_const(eMysql, "CR_SERVER_GONE_ERROR", INT2NUM(CR_SERVER_GONE_ERROR));
    rb_define_const(eMysql, "CR_VERSION_ERROR", INT2NUM(CR_VERSION_ERROR));
    rb_define_const(eMysql, "CR_OUT_OF_MEMORY", INT2NUM(CR_OUT_OF_MEMORY));
    rb_define_const(eMysql, "CR_WRONG_HOST_INFO", INT2NUM(CR_WRONG_HOST_INFO));
#ifdef CR_LOCALHOST_CONNECTION
    rb_define_const(eMysql, "CR_LOCALHOST_CONNECTION", INT2NUM(CR_LOCALHOST_CONNECTION));
#endif
#ifdef CR_TCP_CONNECTION
    rb_define_const(eMysql, "CR_TCP_CONNECTION", INT2NUM(CR_TCP_CONNECTION));
#endif
#ifdef CR_SERVER_HANDSHAKE_ERR
    rb_define_const(eMysql, "CR_SERVER_HANDSHAKE_ERR", INT2NUM(CR_SERVER_HANDSHAKE_ERR));
#endif
#ifdef CR_SERVER_LOST
    rb_define_const(eMysql, "CR_SERVER_LOST", INT2NUM(CR_SERVER_LOST));
#endif
#ifdef CR_COMMANDS_OUT_OF_SYNC
    rb_define_const(eMysql, "CR_COMMANDS_OUT_OF_SYNC", INT2NUM(CR_COMMANDS_OUT_OF_SYNC));
#endif
}
