/*
 *	$Id: mysql.c,v 1.3 1998/08/13 17:19:09 tommy Exp $
 */

#include "ruby.h"
#include <mysql/mysql.h>

VALUE cMysql;
VALUE cMysqlRes;
VALUE eMysql;

struct mysql {
    MYSQL handler;
};

struct mysqlres {
    MYSQL_RES* res;
};

/*
 *	free Mysql class object
 */
static void free_mysql(struct mysql* my)
{
    mysql_close(&my->handler);
    free(my);
}

/*
 *	free MysqlRes class object
 */
static void free_mysqlres(struct mysqlres* res)
{
    mysql_free_result(res->res);
    free(res);
}

/*
 *	connect([host, [db, [user, passwd]]])
 */
static VALUE connect(int argc, VALUE* argv, VALUE class)
{
    VALUE host, db, user, passwd;
    MYSQL my;
    struct mysql* myp;
    VALUE obj;
    char *h, *d, *u, *p;

    rb_scan_args(argc, argv, "04", &host, &db, &user, &passwd);
    if (NIL_P(host))
	h = NULL;
    else {
	Check_SafeStr(host);
	h = RSTRING(host)->ptr;
    }
    if (NIL_P(user))
	u = NULL;
    else {
	Check_SafeStr(user);
	u = RSTRING(user)->ptr;
    }
    if (NIL_P(passwd))
	p = NULL;
    else {
	Check_SafeStr(passwd);
	p = RSTRING(passwd)->ptr;
    }
    if (mysql_connect(&my, h, u, p) == NULL)
	Raise(eMysql, "%s", mysql_error(&my));
    if (!NIL_P(db)) {
	Check_SafeStr(db);
	d = RSTRING(db)->ptr;
	if (mysql_select_db(&my, d) != 0)
	    Raise(eMysql, "%s: %s", d, mysql_error(&my));
    }
    obj = Data_Make_Struct(class, struct mysql, 0, free_mysql, myp);
    myp->handler = my;
    obj_call_init(obj);
    return obj;
}

/*
 *	selectdb(db)
 */
static VALUE selectdb(VALUE obj, VALUE db)
{
    struct mysql* myp;
    Check_SafeStr(db);
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_select_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return Qnil;
}

/*
 *	quote(string)
 */
static VALUE quote(VALUE class, VALUE str)
{
    VALUE ret;
    Check_SafeStr(str);
    ret = str_new(0, (RSTRING(str)->len)*2+1);
    RSTRING(ret)->len = mysql_escape_string(RSTRING(ret)->ptr, RSTRING(str)->ptr, RSTRING(str)->len);
    return ret;
}

/*
 *	query(sql)
 */
static VALUE query(VALUE obj, VALUE sql)
{
    struct mysql* myp;
    struct mysqlres* resp;
    MYSQL_RES* res;
    VALUE resobj;

    Check_SafeStr(sql);
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_query(&myp->handler, RSTRING(sql)->ptr) != 0)
	Raise(eMysql, "Mysql: %s", mysql_error(&myp->handler));
    if (mysql_num_fields(&myp->handler) == 0)
	return Qnil;
    if ((res = mysql_store_result(&myp->handler)) == NULL)
	Raise(eMysql, "Mysql: %s", mysql_error(&myp->handler));
    resobj = Data_Make_Struct(cMysqlRes, struct mysqlres, 0, free_mysqlres, resp);
    resp->res = res;
    obj_call_init(resobj);
    return resobj;
}

/*
 *	affectedrows()
 */
static VALUE affectedrows(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_affected_rows(&myp->handler));
}

/*
 *	insertid()
 */
static VALUE insertid(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    return INT2NUM(mysql_insert_id(&myp->handler));
}

/*
 *	shutdown()
 */
static VALUE shutdown(VALUE obj)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    if (mysql_shutdown(&myp->handler) != 0)
	Raise(eMysql, "%s", mysql_error(&myp->handler));
    return Qnil;
}

/*
 *	createdb(db)
 */
static VALUE createdb(VALUE obj, VALUE db)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    Check_SafeStr(db);
    if (mysql_create_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return Qnil;
}

/*
 *	dropdb(db)
 */
static VALUE dropdb(VALUE obj, VALUE db)
{
    struct mysql* myp;
    Data_Get_Struct(obj, struct mysql, myp);
    Check_SafeStr(db);
    if (mysql_drop_db(&myp->handler, RSTRING(db)->ptr) != 0)
	Raise(eMysql, "%s: %s", RSTRING(db)->ptr, mysql_error(&myp->handler));
    return Qnil;
}

/*
 *	numrows()
 */
static VALUE numrows(VALUE obj)
{
    struct mysqlres* resp;
    Data_Get_Struct(obj, struct mysqlres, resp);
    return INT2NUM(mysql_num_rows(resp->res));
}

/*
 *	numfields()
 */
static VALUE numfields(VALUE obj)
{
    struct mysqlres* resp;
    Data_Get_Struct(obj, struct mysqlres, resp);
    return INT2NUM(mysql_num_fields(resp->res));
}

/*
 *	fetchrow()
 */
static VALUE fetchrow(VALUE obj)
{
    struct mysqlres* resp;
    MYSQL_ROW row;
    int i;
    int n;
    VALUE ary;
    int* lengths;

    Data_Get_Struct(obj, struct mysqlres, resp);
    row = mysql_fetch_row(resp->res);
    if (row == NULL)
	return Qnil;
    n = mysql_num_fields(resp->res);
    lengths = mysql_fetch_lengths(resp->res);
    ary = ary_new2(n);
    for (i=0; i<n; i++)
	ary_push(ary, str_new(row[i], lengths[i]));
    return ary;
}

/*
 *	fetchfields()
 */
static VALUE fetchfields(VALUE obj)
{
    struct mysqlres* resp;
    MYSQL_FIELD* fields;
    int i;
    int n;
    VALUE ary;
    int* lengths;

    Data_Get_Struct(obj, struct mysqlres, resp);
    fields = mysql_fetch_fields(resp->res);
    n = mysql_num_fields(resp->res);
    ary = ary_new2(n);
    for (i=0; i<n; i++)
	ary_push(ary, str_new2(fields[i].name));
    return ary;
}

/*
 *	dataseek(row_num)
 */
static VALUE dataseek(VALUE obj, VALUE num)
{
    struct mysqlres* resp;
    Data_Get_Struct(obj, struct mysqlres, resp);
    mysql_data_seek(resp->res, NUM2INT(num));
    return TRUE;
}

/*
 *	Initialize
 */

Init_mysql()
{
    extern VALUE eStandardError;
    cMysql = rb_define_class("Mysql", cObject);
    cMysqlRes = rb_define_class("MysqlRes", cObject);
    eMysql = rb_define_class("MysqlError", eStandardError);

    rb_define_singleton_method(cMysql, "connect", connect, -1);
    rb_define_singleton_method(cMysql, "new", connect, -1);
    rb_define_singleton_method(cMysql, "quote", quote, 1);
    rb_define_method(cMysql, "query", query, 1);
    rb_define_method(cMysql, "selectdb", selectdb, 1);
    rb_define_method(cMysql, "affectedrows", affectedrows, 0);
    rb_define_method(cMysql, "insertid", insertid, 0);
    rb_define_method(cMysql, "shutdown", shutdown, 0);
    rb_define_method(cMysql, "createdb", createdb, 1);
    rb_define_method(cMysql, "dropdb", dropdb, 1);

    rb_define_method(cMysqlRes, "numrows", numrows, 0);
    rb_define_method(cMysqlRes, "numfields", numfields, 0);
    rb_define_method(cMysqlRes, "fetchrow", fetchrow, 0);
    rb_define_method(cMysqlRes, "fetchfields", fetchfields, 0);
    rb_define_method(cMysqlRes, "dataseek", dataseek, 1);
}
