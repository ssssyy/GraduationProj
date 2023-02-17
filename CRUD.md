# CRUD操作
## 0. IndexMap
存储分为磁盘kvstore与内存IndexMap。称原始边/顶点的唯一表示为rawKey，带有版本号的key为trueKey，IndexMap维护rawKey与对应的版本号ID的关系，在内存中以HashMap形式组织：按照rawKey进行hash，而版本号以动态数组存储，每个item是rawKey--&vector。磁盘kvstore存储的是trueKey--Value的结构。

## 1. Insert Vertex 或 Insert Edge
底层调用doPut（asyncMultiPut）方法，批量写入kv。
### 无 IF_NOT_EXISTS
语义：新写入的会覆盖旧的。
查询IndexMap中是否存在rawKey，若不存在则插入rawKey和当前版本，并拼接一个rawKey+cnt_id的key，向底层db添加kv pair。若存在rawKey，对当前vector中每一版本拼接trueKey，向底层db删除对应kv pairs，然后替换vector中元素为当前版本，并添加db。
### 有 IF_NOT_EXISTS 
语义：已存在则不做操作。
查询IndexMap，若已存在rawKey，不做任何修改；若不存在rawKey，同不带IF_NOT_EXISTS操作。

## 2. Delete Vertex 或 Delete Edge
底层调用doRemove（asyncMultiPRemove）方法，批量删除kv。
### Vertex 无 WITH EDGE 
查询IndexMap，如果没找到对应的rawKey，返回错误信息；如果找到了，对当前vector中每一版本的trueKey进行底层db删除操作，然后删除rawKey。
### Vertex 有 WITH EDGE
在前者基础上增加两条批量删除边的操作（该点的出边和入边）。

## 3. Update Vertex 或 Update Edge
更新操作，如果没查到对应rawKey需要报错，此处使用该rawKey的读写锁，查到rawKey以及当前事务最新可见版本号后，访问db获得value，修改value后写入rawKey的新版本并向db添加kv pair。
底层调用AsyncAppendBatch方法进行写入和删除，batch使用batchHolder创建。

## 4. KV operations
Batch log操作：put、remove、removeRange，第三个暂未使用。

## 5. IndexMap恢复
IndexMap是一个内存中的map，对于一个rawKey，其不在IndexMap中有可能是因为对应的key的修改信息还未被加载，而不表示这个key不存在，因此需要使用类似与lazy load的方式获取版本号。
### 思路1
对IndexMap做持久化处理，用WAL批量写入和读出，优势在于内存结构和db结构一致，都是kv pair，并且恢复和写入都有现成做法。此处可以参考redis AOL。
### 思路2
使用db中的多版本key来恢复IndexMap，而本身IndexMap不做任何持久化处理。
底层kv store提供前缀扫描，将一个key的所有版本都复原，此处使用懒加载，如果db中不存在这个key则在IndexMap中生成一个空的value，以此来区分是否已经加载访问过。
缺点在于db的底层引擎LSM tree并不擅长做批量scan，特别是同一个key的不同版本，大概率是在不同的sst文件上，性能有一定问题。