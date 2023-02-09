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





