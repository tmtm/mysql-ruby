require 'mkmf'
inc, lib = dir_config('mysql', '/usr/local')
find_library('mysqlclient', 'mysql_query', lib, "#{lib}/mysql") or exit 1
# If you have error such as 'undefined symbol', delete '#' mark follow
# lines:
#have_library('m')
#have_library('z')
if have_header('mysql.h') then
  src = '#include <errmsg.h>\n#include <mysqld_error.h>\n'
elsif have_header('mysql/mysql.h') then
  src = "#include <mysql/errmsg.h>\n#include <mysql/mysqld_error.h>\n"
else
  exit 1
end
create_makefile("mysql")

egrep_cpp("'errmsg\\.h|mysqld_error\\.h' > confout", src)
error_syms = []
IO::foreach('confout') do |l|
  fn = l.split(/\"/)[1]
  IO::foreach(fn) do |m|
    if m =~ /^#define\s+([CE]R_[A-Z_]+)/ then
      error_syms << $1
    end
  end
end
File::unlink 'confout'

system('cp -p mysql.c mysql.c.orig') unless File::exist? 'mysql.c.orig'
newf = File::open('mysql.c', 'w')
IO::foreach('mysql.c.orig') do |l|
  newf.puts l
  if l =~ /\/\* MysqlError constant \*\// then
    error_syms.each do |s|
      newf.puts "    rb_define_const(eMysql, \"#{s}\", INT2NUM(#{s}));"
    end
  end
end

