/*	ruby mysql module
 *	$Id: mysql.c,v 1.26 2001/10/12 16:54:17 tommy Exp $
 */

#include "ruby.h"
#include <mysql/mysql.h>
#include <mysql/errmsg.h>

#define GC_STORE_RESULT_LIMIT 20

#ifndef Qtrue		/* ruby 1.2.x ? */
#define	Qtrue		TRUE
#define	Qfalse		FALSE
#define	rb_exc_raise	rb_raise
#define	rb_exc_new2	exc_new2
#define	rb_str_new	str_new
#define	rb_str_new2	str_new2
#define	rb_ary_new2	ary_new2
#define	rb_ary_store	ary_store
#define	rb_obj_alloc	obj_alloc
#define	rb_hash_new	hash_new
#define	rb_hash_aset	hash_aset
#define	rb_eStandardError	eStandardError
#define	rb_cObject	cObject
#endif

#if MYSQL_VERSION_ID < 32224
#define	mysql_field_count	mysql_num_fields
#endif

#define NILorSTRING(obj)	(NIL_P(obj)? NULL: STR2CSTR(obj))
#define NILorINT(obj)		(NIL_P(obj)? 0: NUM2INT(obj))

#define GetMysqlStruct(obj)	(Check_Type(obj, T_DATA), (struct mysql*)DATA_PTR(obj))
#define GetHandler(obj)		(Check_Type(obj, T_DATA), &(((struct mysql*)DATA_PTR(obj))->handler))
#define GetMysqlRes(obj)	(Check_Type(obj, T_DATA), ((struct mysql_res*)DATA_PTR(obj))->res)

VALUE cMysql;
VALUE cMysqlRes;
VALUE cMysqlField;
VALUE eMysql;

static int store_result_count = 0;

struct mysql {
    MYSQL handler;
    char connection;
    char query_with_result;
};

struct mysql_res {
    MYSQL_RES* res;
    char freed;
};


/*	free Mysql class object		*/
static void free_mysql(struct mysql* my)
{
    if (my->connection == Qtrue)
	mysql_close(&my->handler);
    free(my);
}

static void free_mysqlres(struct mysql_res* resp)
{
    if (resp->freed == Qfalse) {
	mysql_free_result(resp->res);
	store_result_count--;
    }
    free(resp);
}

static void mysql_raise(MYSQL* m)
{
    VALUE e = rb_exc_new2(eMysql, mysql_error(m));
    rb_iv_set(e, "errno", INT2FIX(mysql_errno(m)));
    rb_exc_raise(e);
}

static VALUE mysqlres2obj(MYSQL_RES* res)
{
    VALUE obj;
    struct mysql_res* resp;
    obj = Data_Make_Struct(cMysqlRes, struct mysql_res, 0, free_mysqlres, resp);
    resp->res = res;
    resp->freed = Qfalse;
    rb_obj_call_init(obj, 0, NULL);
    if (++store_result_count > GC_STORE_RESULT_LIMIT)
	rb_gc();
    return obj;
}

/*	make MysqlField object	*/
static VALUE make_field_obj(MYSQL_FIELD* f)
{
    VALUE obj;
    VALUE hash;
    if (f == NULL)
	return Qnil;
    obj = rb_obj_alloc(cMysqlField);
    rb_iv_set(obj, "name", f->name? rb_str_freeze(rb_tainted_str_new2(f->name)): Qnil);
    rb_iv_set(obj, "table", f->table? rb_str_freeze(rb_tainted_str_new2(f->table)): Qnil);
    rb_iv_set(obj, "def", f->def? rb_str_freeze(rb_tainted_str_new2(f->def)): Qnil);
    rb_iv_set(obj, "type", INT2NUM(f->type));
    rb_iv_set(obj, "length", INT2NUM(f->length));
    rb_iv_set(obj, "max_length", INT2NUM(f->max_length));
    rb_iv_set(obj, "flags", INT2NUM(f->flags));
    rb_iv_set(obj, "decimals", INT2NUM(f->decimals));
    return obj;
}

/*-------------------------------
 * Mysql class method
 */

/*	init()	*/
static VALUE init(VALUE klass)
{
    struct mysql* myp;
    VALUE obj;

    obj = Data_Make_Struct(klass, struct mysql, 0, free_mysql, myp);
    mysql_init(&myp->handler);
    myp->connection = Qfalse;
    myp->query_with_result = Qtrue;
    rb_obj_call_init(obj, 0, NULL);
    return obj;
}

/*	real_connect(host=nil, user=nil, passwd=nil, db=nil, port=nil, sock=nil, flag=nil)	*/
static VALUE real_connect(int argc, VALUE* argv, VALUE klass)
{
    VALUE host, user, passwd, db, port, sock, flag;
    char *h, *u, *p, *d, *s;
    uint pp, f;
    MYSQL my;
    struct mysql* myp;
    VALUE obj;

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

    obj = Data_Make_Struct(klass, struct mysql, 0, free_mysql, myp);
    myp->handler = my;
    myp->connection = Qtrue;
    myp->query_with_result = Qtrue;
    rb_obj_call_init(obj, argc, argv);

    return obj;
}

/*	escape_string(string)	*/
static VALUE escape_string(VALUE klass, VALUE str)
{
    VALUE ret;
    Check_Type(str, T_STRING);
    ret = rb_str_new(0, (RSTRING(str)->len)*2+1);
    RSTRING(ret)->len = mysql_escape_string(RSTRING(ret)->ptr, RSTRING(str)->ptr, RSTRING(str)->len);
    return ret;
}

/*	client_info()	*/
static VALUE client_info(VALUE klass)
{
    return rb_tainted_str_new2(mysql_get_client_info());
}

#if MYSQL_VERSION_ID >= 32332
/*	my_debug(string)	*/
static VALUE my_debug(VALUE obj, VALUE str)
{
    mysql_debug(STR2CSTR(str));
    return obj;
}
#endif

/*-------------------------------
 * Mysql object method
 */

#if MYSQL_VERSION_ID >= 32200
/*	real_connect(host=nil, user=nil, passwd=nil, db=nil, port=nil, sock=nil, flag=nil)	*/
static VALUE real_connect2(int argc, VALUE* argv, VALUE obj)
{
    VALUE host, user, passwd, db, port, sock, flag;
    char *h, *u, *p, *d, *s;
    uint pp, f;
    MYSQL* m = GetHandler(obj);
    rb_scan_args(argc, argv, "07", &host, &user, &passwd, &db, &port, &sock, &flag);
    d = NILorSTRING(db);
    f = NILorINT(flag);
    h = NILorSTRING(host);
    u = NILorSTRING(user);
    p = NILorSTRING(passwd);
    pp = NILorINT(port);
    s = NILorSTRING(sock);

    if (mysql_real_connect(m, h, u, p, d, pp, s, f) == NULL)
	mysql_raise(m);

    return obj;
}

/*	options(opt, value=nil)	*/
static VALUE options(int argc, VALUE* argv, VALUE obj)
{
    VALUE opt, val;
    int n;
    char* v;
    MYSQL* m = GetHandler(obj);

    rb_scan_args(argc, argv, "11", &opt, &val);
    switch(NUM2INT(opt)) {
    case MYSQL_OPT_CONNECT_TIMEOUT:
	if (val == Qnil)
	    rb_raise(rb_eArgError, "wrong # of arguments(1 for 2)");
	n = NUM2INT(val);
	v = (char*)&n;
	break;
    case MYSQL_INIT_COMMAND:
    case MYSQL_READ_DEFAULT_FILE:
    case MYSQL_READ_DEFAULT_GROUP:
	if (val == Qnil)
	    rb_raise(rb_eArgError, "wrong # of arguments(1 for 2)");
	v = STR2CSTR(val);
	break;
    default:
	v = NULL;
    }

    if (mysql_options(m, NUM2INT(opt), v) != 0)
	rb_raise(eMysql, "unknown option: %d", NUM2INT(opt));
    return obj;
}
#endif

#if MYSQL_VERSION_ID >= 32332
/*	real_escape_string(string)	*/
static VALUE real_escape_string(VALUE obj, VALUE str)
{
    MYSQL* m = GetHandler(obj);
    VALUE ret;
    Check_Type(str, T_STRING);
    ret = rb_str_new(0, (RSTRING(str)->len)*2+1);
    RSTRING(ret)->len = mysql_real_escape_string(m, RSTRING(ret)->ptr, RSTRING(str)->ptr, RSTRING(str)->len);
    return ret;
}
#endif

/*	initialize()	*/
static VALUE initialize(int argc, VALUE* argv, VALUE obj)
{
    return obj;
}

/*	affected_rows()	*/
static VALUE affected_rows(VALUE obj)
{
    return INT2NUM(mysql_affected_rows(GetHandler(obj)));
}

#if MYSQL_VERSION_ID >= 32303
/*	change_user(user=nil, passwd=nil, db=nil)	*/
static VALUE change_user(int argc, VALUE* argv, VALUE obj)
{
    VALUE user, passwd, db;
    char *u, *p, *d;
    MYSQL* m = GetHandler(obj);
    rb_scan_args(argc, argv, "03", &user, &passwd, &db);
    u = NILorSTRING(user);
    p = NILorSTRING(passwd);
    d = NILorSTRING(db);
    if (mysql_change_user(m, u, p, d) != 0)
	mysql_raise(m);
    return obj;
}
#endif

#if MYSQL_VERSION_ID >= 32321
/*	character_set_name()	*/
static VALUE character_set_name(VALUE obj)
{
    return rb_tainted_str_new2(mysql_character_set_name(GetHandler(obj)));
}
#endif

/*	close()		*/
static VALUE my_close(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    mysql_close(m);
    if (mysql_errno(m))
	mysql_raise(m);
    GetMysqlStruct(obj)->connection = Qfalse;
    return obj;
}

/*	create_db(db)	*/
static VALUE create_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_create_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return obj;
}

/*	drop_db(db)	*/
static VALUE drop_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_drop_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return obj;
}

#if MYSQL_VERSION_ID >= 32332
/*	dump_debug_info()	*/
static VALUE dump_debug_info(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_dump_debug_info(m) != 0)
	mysql_raise(m);
    return obj;
}
#endif

/*	errno()		*/
static VALUE my_errno(VALUE obj)
{
    return INT2NUM(mysql_errno(GetHandler(obj)));
}

/*	error()		*/
static VALUE my_error(VALUE obj)
{
    return rb_str_new2(mysql_error(GetHandler(obj)));
}

/*	field_count()	*/
static VALUE field_count(VALUE obj)
{
    return INT2NUM(mysql_field_count(GetHandler(obj)));
}

/*	host_info()	*/
static VALUE host_info(VALUE obj)
{
    return rb_tainted_str_new2(mysql_get_host_info(GetHandler(obj)));
}

/*	proto_info()	*/
static VALUE proto_info(VALUE obj)
{
    return INT2NUM(mysql_get_proto_info(GetHandler(obj)));
}

/*	server_info()	*/
static VALUE server_info(VALUE obj)
{
    return rb_tainted_str_new2(mysql_get_server_info(GetHandler(obj)));
}

/*	info()		*/
static VALUE info(VALUE obj)
{
    char* p = mysql_info(GetHandler(obj));
    return p? rb_tainted_str_new2(p): Qnil;
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
    return obj;
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
    ret = rb_ary_new2(n);
    for (i=0; i<n; i++)
	rb_ary_store(ret, i, rb_tainted_str_new2(mysql_fetch_row(res)[0]));
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
    return mysqlres2obj(res);
}

/*	list_processes()	*/
static VALUE list_processes(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res = mysql_list_processes(m);
    if (res == NULL)
	mysql_raise(m);
    return mysqlres2obj(res);
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
    ret = rb_ary_new2(n);
    for (i=0; i<n; i++)
	rb_ary_store(ret, i, rb_tainted_str_new2(mysql_fetch_row(res)[0]));
    mysql_free_result(res);
    return ret;
}

/*	ping()		*/
static VALUE ping(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_ping(m) != 0)
	mysql_raise(m);
    return obj;
}

/*	refresh(r)	*/
static VALUE refresh(VALUE obj, VALUE r)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_refresh(m, NUM2INT(r)) != 0)
	mysql_raise(m);
    return obj;
}

/*	reload()	*/
static VALUE reload(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_reload(m) != 0)
	mysql_raise(m);
    return obj;
}

/*	select_db(db)	*/
static VALUE select_db(VALUE obj, VALUE db)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_select_db(m, STR2CSTR(db)) != 0)
	mysql_raise(m);
    return obj;
}

/*	shutdown()	*/
static VALUE my_shutdown(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    if (mysql_shutdown(m) != 0)
	mysql_raise(m);
    return obj;
}

/*	stat()		*/
static VALUE my_stat(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    char* s = mysql_stat(m);
    if (s == NULL)
	mysql_raise(m);
    return rb_tainted_str_new2(s);
}

/*	store_result()	*/
static VALUE store_result(VALUE obj)
{
    MYSQL* m = GetHandler(obj);
    MYSQL_RES* res = mysql_store_result(m);
    if (res == NULL)
	mysql_raise(m);
    return mysqlres2obj(res);
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
    return mysqlres2obj(res);
}

/*	query(sql)	*/
static VALUE query(VALUE obj, VALUE sql)
{
    MYSQL* m = GetHandler(obj);
    Check_Type(sql, T_STRING);
    if (mysql_real_query(m, RSTRING(sql)->ptr, RSTRING(sql)->len) != 0)
	mysql_raise(m);
    if (GetMysqlStruct(obj)->query_with_result == Qfalse)
	return obj;
    if (mysql_field_count(m) == 0)
	return Qnil;
    return store_result(obj);
}

/*	query_with_result()	*/
static VALUE query_with_result(VALUE obj)
{
    return GetMysqlStruct(obj)->query_with_result? Qtrue: Qfalse;
}

/*	query_with_result=(flag)	*/
static VALUE query_with_result_set(VALUE obj, VALUE flag)
{
    if (TYPE(flag) != T_TRUE && TYPE(flag) != T_FALSE)
#ifndef str2cstr
	TypeError("invalid type, required true or false.");
#else
        rb_raise(rb_eTypeError, "invalid type, required true or false.");
#endif
    GetMysqlStruct(obj)->query_with_result = flag;
    return flag;
}

/*-------------------------------
 * MysqlRes object method
 */

/*	check if alread freed	*/
static void check_free(VALUE obj)
{
    struct mysql_res* resp = DATA_PTR(obj);
    if (resp->freed == Qtrue)
        rb_raise(eMysql, "MysqlRes object is already freed");
}

/*	data_seek(offset)	*/
static VALUE data_seek(VALUE obj, VALUE offset)
{
    check_free(obj);
    mysql_data_seek(GetMysqlRes(obj), NUM2INT(offset));
    return obj;
}

/*	fetch_field()	*/
static VALUE fetch_field(VALUE obj)
{
    check_free(obj);
    return make_field_obj(mysql_fetch_field(GetMysqlRes(obj)));
}

/*	fetch_fields()	*/
static VALUE fetch_fields(VALUE obj)
{
    MYSQL_RES* res;
    MYSQL_FIELD* f;
    unsigned int n;
    VALUE ret;
    unsigned int i;
    check_free(obj);
    res = GetMysqlRes(obj);
    f = mysql_fetch_fields(res);
    n = mysql_num_fields(res);
    ret = rb_ary_new2(n);
    for (i=0; i<n; i++)
	rb_ary_store(ret, i, make_field_obj(&f[i]));
    return ret;
}

/*	fetch_field_direct(nr)	*/
static VALUE fetch_field_direct(VALUE obj, VALUE nr)
{
    MYSQL_RES* res;
    unsigned int max;
    unsigned int n;
    check_free(obj);
    res = GetMysqlRes(obj);
    max = mysql_num_fields(res);
    n = NUM2INT(nr);
    if (n >= max)
#ifndef str2cstr
        Raise(eMysql, "%d: out of range (max: %d)", n, max-1);
#else
        rb_raise(eMysql, "%d: out of range (max: %d)", n, max-1);
#endif
#if MYSQL_VERSION_ID >= 32226
    return make_field_obj(mysql_fetch_field_direct(res, n));
#else
    return make_field_obj(&mysql_fetch_field_direct(res, n));
#endif
}

/*	fetch_lengths()		*/
static VALUE fetch_lengths(VALUE obj)
{
    MYSQL_RES* res;
    unsigned int n;
    unsigned long* lengths;
    VALUE ary;
    unsigned int i;
    check_free(obj);
    res = GetMysqlRes(obj);
    n = mysql_num_fields(res);
    lengths = mysql_fetch_lengths(res);
    if (lengths == NULL)
	return Qnil;
    ary = rb_ary_new2(n);
    for (i=0; i<n; i++)
	rb_ary_store(ary, i, INT2NUM(lengths[i]));
    return ary;
}

/*	fetch_row()	*/
static VALUE fetch_row(VALUE obj)
{
    MYSQL_RES* res;
    unsigned int n;
    MYSQL_ROW row;
    unsigned long* lengths;
    VALUE ary;
    unsigned int i;
    check_free(obj);
    res = GetMysqlRes(obj);
    n = mysql_num_fields(res);
    row = mysql_fetch_row(res);
    lengths = mysql_fetch_lengths(res);
    if (row == NULL)
	return Qnil;
    ary = rb_ary_new2(n);
    for (i=0; i<n; i++)
	rb_ary_store(ary, i, row[i]? rb_tainted_str_new(row[i], lengths[i]): Qnil);
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
    hash = rb_hash_new();
    for (i=0; i<n; i++) {
	VALUE col;
	if (row[i] == NULL)
	    continue;
	if (with_table == Qnil || with_table == Qfalse)
	    col = rb_tainted_str_new2(fields[i].name);
	else {
	    col = rb_tainted_str_new(fields[i].table, strlen(fields[i].table)+strlen(fields[i].name)+1);
	    RSTRING(col)->ptr[strlen(fields[i].table)] = '.';
	    strcpy(RSTRING(col)->ptr+strlen(fields[i].table)+1, fields[i].name);
	}
	rb_hash_aset(hash, col, row[i]? rb_tainted_str_new(row[i], lengths[i]): Qnil);
    }
    return hash;
}

/*	fetch_hash(with_table=false)	*/
static VALUE fetch_hash(int argc, VALUE* argv, VALUE obj)
{
    VALUE with_table;
    check_free(obj);
    rb_scan_args(argc, argv, "01", &with_table);
    if (with_table == Qnil)
	with_table = Qfalse;
    return fetch_hash2(obj, with_table);
}

/*	field_seek(offset)	*/
static VALUE field_seek(VALUE obj, VALUE offset)
{
    check_free(obj);
    return INT2NUM(mysql_field_seek(GetMysqlRes(obj), NUM2INT(offset)));
}

/*	field_tell()		*/
static VALUE field_tell(VALUE obj)
{
    check_free(obj);
    return INT2NUM(mysql_field_tell(GetMysqlRes(obj)));
}

/*	free()			*/
static VALUE res_free(VALUE obj)
{
    struct mysql_res* resp = DATA_PTR(obj);
    check_free(obj);
    mysql_free_result(resp->res);
    resp->freed = Qtrue;
    store_result_count--;
    return Qnil;
}

/*	num_fields()		*/
static VALUE num_fields(VALUE obj)
{
    check_free(obj);
    return INT2NUM(mysql_num_fields(GetMysqlRes(obj)));
}

/*	num_rows()	*/
static VALUE num_rows(VALUE obj)
{
    check_free(obj);
    return INT2NUM(mysql_num_rows(GetMysqlRes(obj)));
}

/*	row_seek(offset)	*/
static VALUE row_seek(VALUE obj, VALUE offset)
{
    check_free(obj);
    return INT2NUM((int)mysql_row_seek(GetMysqlRes(obj), (MYSQL_ROWS*)NUM2INT(offset)));
}

/*	row_tell()	*/
static VALUE row_tell(VALUE obj)
{
    check_free(obj);
    return INT2NUM((int)mysql_row_tell(GetMysqlRes(obj)));
}

/*	each {...}	*/
static VALUE each(VALUE obj)
{
    VALUE row;
    check_free(obj);
    while ((row = fetch_row(obj)) != Qnil)
	rb_yield(row);
    return obj;
}

/*	each_hash(with_table=false) {...}	*/
static VALUE each_hash(int argc, VALUE* argv, VALUE obj)
{
    VALUE with_table;
    VALUE hash;
    check_free(obj);
    rb_scan_args(argc, argv, "01", &with_table);
    if (with_table == Qnil)
	with_table = Qfalse;
    while ((hash = fetch_hash2(obj, with_table)) != Qnil)
	rb_yield(hash);
    return obj;
}

/*-------------------------------
 * MysqlField object method
 */

/*	hash	*/
static VALUE field_hash(VALUE obj)
{
    VALUE h = rb_hash_new();
    rb_hash_aset(h, rb_str_new2("name"), rb_iv_get(obj, "name"));
    rb_hash_aset(h, rb_str_new2("table"), rb_iv_get(obj, "table"));
    rb_hash_aset(h, rb_str_new2("def"), rb_iv_get(obj, "def"));
    rb_hash_aset(h, rb_str_new2("type"), rb_iv_get(obj, "type"));
    rb_hash_aset(h, rb_str_new2("length"), rb_iv_get(obj, "length"));
    rb_hash_aset(h, rb_str_new2("max_length"), rb_iv_get(obj, "max_length"));
    rb_hash_aset(h, rb_str_new2("flags"), rb_iv_get(obj, "flags"));
    rb_hash_aset(h, rb_str_new2("decimals"), rb_iv_get(obj, "decimals"));
    return h;
}

/*	inspect	*/
static VALUE field_inspect(VALUE obj)
{
    VALUE n = rb_iv_get(obj, "name");
    VALUE s = rb_str_new(0, RSTRING(n)->len + 14);
    sprintf(RSTRING(s)->ptr, "#<MysqlField:%s>", RSTRING(n)->ptr);
    return s;
}

#define DefineMysqlFieldMemberMethod(m)\
static VALUE field_##m(VALUE obj)\
{return rb_iv_get(obj, #m);}

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
    extern VALUE rb_eStandardError;

    cMysql = rb_define_class("Mysql", rb_cObject);
    cMysqlRes = rb_define_class("MysqlRes", rb_cObject);
    cMysqlField = rb_define_class("MysqlField", rb_cObject);
    eMysql = rb_define_class("MysqlError", rb_eStandardError);

    /* Mysql class method */
    rb_define_singleton_method(cMysql, "init", init, 0);
    rb_define_singleton_method(cMysql, "real_connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "connect", real_connect, -1);
    rb_define_singleton_method(cMysql, "new", real_connect, -1);
    rb_define_singleton_method(cMysql, "escape_string", escape_string, 1);
    rb_define_singleton_method(cMysql, "quote", escape_string, 1);
    rb_define_singleton_method(cMysql, "client_info", client_info, 0);
    rb_define_singleton_method(cMysql, "get_client_info", client_info, 0);
#if MYSQL_VERSION_ID >= 32332
    rb_define_singleton_method(cMysql, "debug", my_debug, 1);
#endif

    /* Mysql object method */
#if MYSQL_VERSION_ID >= 32200
    rb_define_method(cMysql, "real_connect", real_connect2, -1);
    rb_define_method(cMysql, "connect", real_connect2, -1);
    rb_define_method(cMysql, "options", options, -1);
#endif
    rb_define_method(cMysql, "initialize", initialize, -1);
#if MYSQL_VERSION_ID >= 32332
    rb_define_method(cMysql, "escape_string", real_escape_string, 1);
    rb_define_method(cMysql, "quote", real_escape_string, 1);
#else
    rb_define_method(cMysql, "escape_string", escape_string, 1);
    rb_define_method(cMysql, "quote", escape_string, 1);
#endif
    rb_define_method(cMysql, "client_info", client_info, 0);
    rb_define_method(cMysql, "get_client_info", client_info, 0);
    rb_define_method(cMysql, "affected_rows", affected_rows, 0);
#if MYSQL_VERSION_ID >= 32303
    rb_define_method(cMysql, "change_user", change_user, -1);
#endif
#if MYSQL_VERSION_ID >= 32321
    rb_define_method(cMysql, "character_set_name", character_set_name, 0);
#endif
    rb_define_method(cMysql, "close", my_close, 0);
    rb_define_method(cMysql, "create_db", create_db, 1);
    rb_define_method(cMysql, "drop_db", drop_db, 1);
#if MYSQL_VERSION_ID >= 32332
    rb_define_method(cMysql, "dump_debug_info", dump_debug_info, 0);
#endif
    rb_define_method(cMysql, "errno", my_errno, 0);
    rb_define_method(cMysql, "error", my_error, 0);
    rb_define_method(cMysql, "field_count", field_count, 0);
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
#if MYSQL_VERSION_ID >= 32200
    rb_define_const(cMysql, "OPT_CONNECT_TIMEOUT", INT2NUM(MYSQL_OPT_CONNECT_TIMEOUT));
    rb_define_const(cMysql, "OPT_COMPRESS", INT2NUM(MYSQL_OPT_COMPRESS));
    rb_define_const(cMysql, "OPT_NAMED_PIPE", INT2NUM(MYSQL_OPT_NAMED_PIPE));
    rb_define_const(cMysql, "INIT_COMMAND", INT2NUM(MYSQL_INIT_COMMAND));
    rb_define_const(cMysql, "READ_DEFAULT_FILE", INT2NUM(MYSQL_READ_DEFAULT_FILE));
    rb_define_const(cMysql, "READ_DEFAULT_GROUP", INT2NUM(MYSQL_READ_DEFAULT_GROUP));
#endif
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
    rb_define_method(cMysqlRes, "free", res_free, 0);
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
