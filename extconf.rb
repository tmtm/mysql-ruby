require 'mkmf'
MYSQLDIR = "/usr/local"
$CFLAGS = "-I#{MYSQLDIR}/include"
$LDFLAGS = "-L#{MYSQLDIR}/lib/mysql"
$libs = "-lmysqlclient -lsocket -lnsl"
create_makefile("mysql")
