$my.query("update test set id=0, str='hoge'")
if $my.info != "Rows matched: 2  Changed: 2  Warnings: 0" then raise "update: failed" end
