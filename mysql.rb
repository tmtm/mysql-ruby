# supplement to mysql module
# $Id: mysql.rb,v 1.3 1998/11/24 19:09:38 tommy Exp $

require 'mysql.o'

class MysqlRes
  def fetch_hash(with_table=false)
    row = fetch_row or return nil
    fields = fetch_fields
    ret = {}
    fields.each_index {|i|
      n = if with_table then "#{fields[i].table}.#{fields[i].name}" else fields[i].name end
      ret[n] = row[i]
    }
    ret
  end
  def each_hash(with_table=false)
    while hash = fetch_hash(with_table)
      yield hash
    end
  end
end
