# for compatibility with 1.x
# $Id: mysql-compat.rb,v 1.2 1998/11/29 13:02:15 tommy Exp $

require 'mysql.o'

class Mysql
  def Mysql.connect(host=nil, db=nil, user=nil, pass=nil)
    Mysql.real_connect(host, user, pass, db)
  end
  def Mysql.new(host=nil, db=nil, user=nil, pass=nil)
    Mysql.real_connect(host, user, pass, db)
  end

  alias selectdb select_db
  alias affectedrows affected_rows
  alias insertid insert_id
  alias createdb create_db
  alias dropdb drop_db

  def listdbs()
    ret = []
    query("show databases").each {|x| ret << x[0]}
    ret
  end

  def listtables()
    ret = []
    query("show tables").each {|x| ret << x[0]}
    ret
  end

  def listfields(table)
    query("show fields from #{table}")
  end
end

class MysqlRes
  alias numrows num_rows
  alias numfields num_fields
  alias fetchrow fetch_row
  alias dataseek data_seek

  def fetchfields()
    ret = []
    fetch_fields.each{|f| ret << f.name}
    ret
  end

  def fetchhash()
    row = fetchrow or return nil
    fields = fetchfields
    ret = {}
    fields.each_index {|i| ret[fields[i]] = row[i]}
    ret
  end

  def each_hash()
    while hash = fetchhash
      yield hash
    end
  end
end
