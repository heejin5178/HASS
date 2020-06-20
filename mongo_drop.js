use ycsb
db.usertable.drop()
sh.shardCollection("ycsb.usertable",{_id:1})
