# $Id: mysql.rb,v 1.1 1998/08/13 17:19:10 tommy Exp $

require 'mysql.o'

class Mysql
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
  def fetchhash()
    row = fetchrow or return nil
    fields = fetchfields
    ret = {}
    fields.each_index {|i| ret[fields[i]] = row[i]}
    ret
  end

  def each()
    while row = fetchrow
      yield row
    end
  end

  def each_hash()
    while hash = fetchhash
      yield hash
    end
  end
end
