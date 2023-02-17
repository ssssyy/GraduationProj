# 2月17日
# Doing
## 毕业论文
写了整体设计以及多版本的协议、存储和垃圾清理方案，规划了几个章节的标题和下级标题内容（属性多版本和关系多版本的实现，持久化设计）。
## 系统设计
之前只设计了属性多版本的一系列存储，对于点和边的关系的多版本实现规划比较粗略，采用从db读取后过滤的样式。但实际上由于需要在内存中维护多版本索引，而查询一个点的所有临边本身就与数据库无关，因此采用差分版本存储了点和临边的关系，同样采用AOF方式进行持久化。遗留问题是，由于一条边需要存储在两个顶点处（出边集合和入边集合），而一条边的编码长度往往大于32字节，所以可以考虑给边编号以解决存两次带来的冗余，或使用指针方式，引入两个指针大小的额外数据来节省一份边存储荣誉。
## 代码实现
实现Transaction Manager以及AOF部分中。


# 2月16日
# Doing
梳理并写完事务管理模块以及事务读写提交的伪代码，对持久化日志的写入和读取进行了模块化设计；
在论文初稿中写上了大致的实现思路。
# Todo
实现事务管理模块，继续填写论文初稿。


# 2月10日
# Doing
今天修改了thrift文件准备生成新的代码的时候发现cmake崩了，所以只好重新拉取代码编译了一次，编译真的耗时非常非常久啊。
梳理了IndexMap以及对应的写入操作的具体实现步骤，在持久化和懒加载恢复中确定一个最终的方案。
看论文，看读写锁如何应用。
# Todo
接下来五天人在外地，无法写代码，就继续规划持久化的做法以及读写锁的实现。

# 2月9日
## Doing
### 多版本
底层kv依然是一个key对应一个属性props，为key增加版本号。采用MVTO思路，每个key增加txn-id作为写锁、read-ts作为读锁、以及一个modify-ts作为该条目的时间戳。
### 思路梳理
对于vertex和edge，对应的属性采用kv对存储，因此如果需要访问属性，则需要经过IndexTree来找到对应的时间戳，拼接成实际的key；
对于非属性的访问，即判断vertex相邻的vertex以及路径，也存在多版本问题。在nebula中，找一个点的邻居会需要用到getNeighbors方法，而具体底层执行是在storage/exec中，例如EdgeNode.h文件中的SingleEdgeNode类提供按照前缀（partID+srcID+EdgeType）扫描一个src的指定type的所有出边，返回一个迭代器，因此对于这一类方法，需要修改key的生成方法，同时对于返回的迭代器需要进行过滤：按照当前事务的txnid和返回的key的modify-ts进行比较，只返回可见的部分。
底层db中的key：rawKey+modify-ts拼接而成；
txn-id：由snowflake算法本地生成；
IndexTree的功能：给定一个rawKey，查询出对应的modify-ts，因此不需要存储对应的rawKey部分；IndexTree是一个内存中的结构，存在丢失数据的可能性，但底层kvstore提供prefix scan功能，所以如果需要数据恢复，则进行前缀扫描访问到的key，重新从磁盘中加载对应的ts序列。
### 雪花ID
使用millisecond级别的时间戳+10位机器号+10位序列号组成。一个server在整个流程中只需要持有一个生成器即可，使用storaged在metad中注册的编号作为机器id，因此需要添加这部分交互流程。
### TreeIndex
以内存BTree形式组织，key为rawKey，value为vector/deque，里面包含这个rawKey对应的版本号序列。对于开启事务的CRUD操作：
CREATE：写操作，需要先判断是否有相同key的部分存在（根据语义进行key替换/不做操作），然后新建一个key插入BTree，并在DB中写入value；
READ：读操作，反向遍历对应的vector，找到第一个符合版本要求的ts，用这个ts去拼接key；
UPDATE：更新操作，先读再写，写的时候会向vector末尾插入一个新的版本（此处需要考虑写-写冲突的解决）；
DELETE：更新操作，将IndexTree中对应条目标志为删除，并向db中发删除请求。这里需要考虑将多版本的kv都一并删除。
GC：根据索引判断，删除IndexTree中对应版本并同时批量发删除kv请求。

## Todo
明天需要仔细列出不同操作的实际情况（例如根据语义以及隔离等级来对可见度做一个判断）。
看metad和storaged的交互部分，并完善使用雪花算法的IDGenerator的使用；对tag和edge的数据结构做一个更改，以及调研一下这种思路下的BTree如何实现。



# 2月8日
## Doing
看了分布式系统时间戳的一些实现，以及对于底层使用kvstore的mvcc实现。
分布式系统时间戳的两种实现方式：中心化时间戳分配器/分布式时间戳。中心化时间戳分配器可以复用nebula的metad进行txnid的分配，但这就要求每次开启事务都rpc访问meta节点（并且这一过程还需要加锁，并行程度低）。分布式的时间戳中，考虑向量时钟/雪花id，其中向量时钟维护每个server的总事务版本号并不断广播，这可能会造成更大的存储空间要求，所以最后决定采用雪花id（时间戳精确到毫秒+2048序列号）
对于版本号的实现方式，参考了使用kvstore的etcd的实现：etcd有一个IndexTree，记录的是key与version的对应关系，底层kvstore实际是version--kvpairs。
## Todo
开始写代码，准备把雪花id的部分写好。


# 2月7日
## Doing
尝试使用gdb调试但出了一些问题，为了阅读方便，使用了explain来查看执行计划，基本弄清楚了query engine层面的东西；
发现props末尾是有ts的，但多版本链需要在key上直接找到对应版本再去拿，所以还是得给key加上mvcc所需的四个字段。
## Todo
看完CRUD操作的storage层次的逻辑和操作，整理笔记，以及出一个详细的实现计划。


# 2月6日
## Doing
继续阅读论文，阅读完了《Graph Database》这本书，更深入了解了事务的定义以及Neo4j中的实现；
对于原生图存储的方式进行了辨析：只要能够使用无索引邻接找到邻居节点就可，因此虽然nebula底层是用kvstore存储，但是将点和对应的出入边存在一起的实现同样可以方便地找到邻居（即一个点可以在O(1)代价内找到出边，并解码出边，得到对端vid，用这个vid+tagID去kvstore里取出来）。
仔细阅读了编码方式，做笔记。
## Todo
用调试功能走一遍流程，以及拟定一个底层存储的修改计划。


# 2月4日
## Doing
https://www.nebula-graph.com.cn/posts/how-to-read-nebula-graph-source-code继续读了一篇源码解析文章；
看完了所有有关多版本以及分布式事务的手上文章，对MVCC协议、版本存储、索引、GC的预期实现方案为：MVTO + append only N2O + tuple-level VAC / transaction-level COOP gc + 间接索引表；
看了neo4j的架构设计以及nebula的engine层设计，发现如果要增加transaction关键字并不难，但nebula的engine包含了parser-validator-planner-optimizer-executor几个步骤，对各类算子（例如DML/DDL）都进行了一系列的抽象和优化，而实际上作为事务只需要修改parser和executor即可，所以在考虑是否要自己根据storage的接口写一个简单的engine层来实现添加事务。
在不考虑迁移的情况下，nebula现有编码中，vertex字段的type、partID、vertexID、TagID都是不变的，edge字段也同理，因此生成的多版本实际是一个点的出边/入边的多版本以及点或边的属性的多版本（一条边的两个顶点是不变的，否则就是另外的边了）。
对于前者，拆分为RDF图的多版本存储，可以建立在现有的切边模式下进行存储；对于后者，对每个key（唯一标识一个顶点或一条边）都生成一条props的版本链，由于nebula的prop都是定长编码的（变长类型使用指针去后面寻找），而允许不同版本的props指针指向同一个数据实体（比如一个BLOB类型）可以优化多版本的存储——我们实际上只存储了定长数据类型以及指针的多版本。
## Todo
明天要出去玩，所以先不写代码，继续做一下时间戳的规划（MVTO论文里表示要加txn-id、read-ts、begin-ts、end-ts四个ts字段，参考一下能否节省），将事务的支持插入engine的方式，以及底层的多版本排布方式。


# 2月3日
## Doing
https://www.nebula-graph.com.cn/posts/nebula-graph-source-code-reading-01，阅读了源码解读系列文章，更好地定位代码功能
在common/datatypes文件下面有数据类型的定义，需要实现的增加时间戳的修改就要从这里入手；同时，还需要修改parser中的flex和bison文件，为其增加上transaction的关键字，以及在对应的AST中增加事务行为。
由于需要支持事务的运行，还需要修改存储层storage中的一些实现。但如果要出于简便，可能考虑非侵入型的增加一个版本链存储的方式会易于修改现有存储引擎。


# 2月2日
## Doing
在官网上找到了源码解读，于是开始看起了论坛
## Todo
昨日的Todo尚未完成，今日继续

# 2月1日
## Doing
今日出去见了老师同学，于是没有敲代码，只是继续看了网上文章的源码解读以及MVCC的论文
## Todo
明天开始阅读nebula存储部分的源码，争取将storage部分读懂，并且看完剩下两篇相关论文，列出可行的实现方案或方案组合


# 1月31日
完成nebula graph的本地编译以及使用客户端连接运行：编译总共花了8小时，考虑到之后可能会修改存储的数据的原始结构以及查询语言的parser，需要思考如何能优化这样的cpp工程的编译问题。
同时，由于使用docker挂载方式进行编译，我的目录与编译产物shell的目录不同，修改了半天shell文件才成功运行，后续也可以做调整。
今日还遇到了宿主机连接docker上运行的服务i/o超时问题，暂时未能解决，转为将console放到linux环境下运行的方式，访问localhost解决。但之后如果要多机部署，可能需要考虑这个问题。

读论文方面：
精读并总结《An Empirical Evaluation of In-Memory Multi-Version Concurrency Control》这篇综述文章，里面介绍了关于MVCC的协议、存储方式、GC、索引编排等方面的内容。
考虑从中选取适合的模式进行组合来更好地针对属性图实现不同隔离等级下的事务。


# 1月30日
下载nebula源码，找到编译镜像，本地安装环境，配置硬件。
继续看论文，学习MVCC实现以及现有的图数据库事务实现，思考对于属性图应该用什么数据结构处理多版本存储，以及考虑写时复制与MVCC来处理并发冲突的问题。





