# RepairBoost

This is the implementation of RepairBoost described in our paper "Boosting Full-Node Repair in Erasure-Coded Storage" appeared in USENIX ATC'21. RepairBoost is a scheduling framework that can assist existing linear erasure codes and repair algorithms to boost the full-node repair performance. 
Please contact [linesyoo@gmail.com](mailto:linesyoo@gmail.com) if you have any questions.

## 1. Install

We have tested RepairBoost on Ubuntu16.04 LTS.
 ### 1.1 Common

  - g++ & make & libtool & autoconf & git

  ```
  $ sudo apt-get install g++ cmake libtool autoconf git
  ```

  - gf-complete

  ```
  $ git clone https://github.com/ceph/gf-complete.git
  $ cd gf-complete
  $ ./autogen.sh && ./configure && make && sudo make install
  ```

  - redis-3.2.8

Download redis-3.2.8 and install it.

  ```
  $ sudo wget http://download.redis.io/releases/redis-3.2.8.tar.gz
  $ tar -zxvf redis-3.2.8.tar.gz
  $ cd redis-3.2.8
  $ make && sudo make install
  ```

  Install redis as a background daemon. You can just use the default settings.

  ```
  $ cd utils
  $ sudo ./install_server.sh
  ```

  Configure redis to be remotely accessible.

  ```
  $ sudo /etc/init.d/redis_6379 stop
  ```

  Edit /etc/redis/6379.conf. Find the line with bind 127.0.0.0 and modify it to bind 0.0.0.0, then start redis.

  ```
  $ sudo /etc/init.d/redis_6379 start
  ```

  - hiredis-1.0.0

  Download hiredis-1.0.0 and install it.

  ```
  $ sudo wget https://github.com/redis/hiredis/archive/refs/tags/v1.0.0.tar.gz 
  $ tar -zxvf v1.0.0.tar.gz
  $ cd hiredis-1.0.0
  $ make && sudo make install
  ```

### 1.2 Compile RepairBoost

  After finishing the preparations above, download and compile the source code.

  ```
  $ cd repairboost-code
  $ make
  ```


 ## 2. Standalone Test

 ### 2.1 Prerequisites

 #### Configuration File

 We configure the settings of RepairBoost via the configuration file config.xml in XML format. 

| Property  | Description |
| ---- | ---- |
| erasure.code.type | Three types are implemented: RS, LRC, and BUTTERFLY (only n=6, k=4). |
| erasure.code.k      | The number of data chunks.  |
| erasure.code.n     |  Total number of chunks in a stripe.    |
| lrc.code.l   |  The number of groups, only valid in LRC. Default is 0.    |
| encode.matrix.file | The path of the encoding matrix file. Absolute path is required. |
| packet.size | The size of a packet in units of bytes. |
| packet.count | The number of packets in a chunk. A chunk is partitioned into many smaller packets. |
| repair.method | Three single-chunk repair methods are implemented: cr (i.e., conventional repair), ppr and path(i.e., ecpipe). |
| file.system.type | Two types are supported: standalone and HDFS3. |
| meta.stripe.dir | The directory where the stripe metadata is stored. Absolute path is required. |
| block.directory | The directory where the coded chunks are stored. Absolute path is required. |
| coordinator.address| IP address of the coordinator. |
| helpers.address | IP addresses of all nodes in the system. |
| local.ip.address | IP address of the node itself.|

 #### Encoding Matrix File

 For RS codes and LRC codes, we read the encoding coefficients from the encoding matrix file. The file contains an (n-k)×k matrix that specifies the coefficients for generating n-k parity chunks from k data chunks. 

The file repairboost-code/conf/rsEncMat_6_9 shows a encoding matrix for RS(9, 6) (n=9, k=6):

![](https://tva1.sinaimg.cn/large/008i3skNly1gqj77wox3uj327c0cuq5k.jpg)

Our example uses the file repairboost-code/conf/rsEncMat_3_4 to construct a simple coding matrix for RS(4, 3)(n=4, k=3):

![](https://tva1.sinaimg.cn/large/008i3skNly1gqlaf86jc0j31jo02agli.jpg)


 #### Create Erasure-Coded Chunks

Before testing our standalone system, we need to create stripes of erasure-coded chunks and corresponding metadata. We have provided programs for generating chunks of different codes under repairboost-code/test/.

```
$ cd repairboost-code/test
$ make
$ dd if=/dev/urandom iflag=fullblock of=input.txt bs=64M count=3
$ ./createdata_rs ../conf/rsEncMat_3_4 input.txt 3 4
```

For more detail, you can run the command ./createdata_rs (or ./createdata_lrc, etc.) to get the usage of the programs.

Take RS(4, 3) (n=4, k=3) as an example. In repairboost-code/test, we create 3 files of uncoded chunks and 1 file of coded chunks. We can distribute the 4 chunks across 4 nodes, each of which stores one chunk under the path specified by block.directory. The coordinator stores the stripe metadata under the path specified by meta.stripe.dir. 

The naming of data files and metadata should meet the requirements. If there are 2 stripes, the data of the stripes are named as follows:

```
stripe_0_file_k1   stripe_0_file_k2   stripe_0_file_k3   stripe_0_file_m1   
stripe_1_file_k1   stripe_1_file_k2   stripe_1_file_k3   stripe_1_file_m1   
```

The coordinator stores 4 files for each stripe:   rs:stripe_0_file_k1_1001, rs:stripe_0_file_k2_1001, rs:stripe_0_file_k3_1001 and rs:stripe_0_file_m1_1002, all of which have the following content:

```
stripe_0_file_k1_1001:stripe_0_file_k2_1001:stripe_0_file_k3_1001:stripe_0_file_m1_1002
```

The file name rs:file_k1_1001 means that the chunk uses Reed-Solomon (RS) code and the chunk name is file_k1. The tail 1001 (resp. 1002) means the chunk is an uncoded chunk (resp. coded chunk).

### 2.2 Run

#### Start RepairBoost

The start script is in repairboost-code/scripts/.

```
$ python scripts/start.py
```

Tip: If an error occurs when loading the shared library libgf_complete.so.1, you may need to set up the environment as follows.

```
$ sudo cp /usr/local/lib/libgf_complete.so.1 /usr/lib/ && sudo ldconfig
```

#### Full-node Repair Test

Use ssh to connect to any node (not the coordinator) to send a full-node repair request.

```
$ ./ECClient
```

#### Stop RepairBoost

The stop script is in repairboost-code/scripts/.

```
$ python scripts/stop.py
```



 ## 3. Hadoop-3 Integration

### 3.1 Prerequisites

- isa-l

```
$ git clone https://github.com/01org/isa-l.git
$ cd isal
$ ./autogen.sh && ./configure && make && sudo make install
```

- java8

```
$ sudo apt-get purge openjdk*
$ sudo apt-get install software-properties-common
$ sudo add-apt-repository  ppa:ts.sch.gr/ppa 
$ sudo apt-get update 
$ sudo apt-get install oracle-java8-installer  
$ sudo apt install oracle-java8-set-default
```

Then configure the environment variables for java.

```
export JAVA_HOME=/usr/lib/jvm/java-8-oracle
export PATH=$JAVA_HOME/bin:$PATH
```

Test

```
$ java -version
java version "1.8.0_212"
```

- maven 

Download apache-maven-3.5.4 on [available mirror](https://downloads.apache.org/maven/maven-3/3.5.4/binaries/apache-maven-3.5.4-bin.tar.gz).

```
$ tar -zxvf apache-maven-3.5.4-bin.tar.gz
$ sudo mv apache-maven-3.5.4 /opt/
$ sudo ln -s /opt/apache-maven-3.5.4 /opt/apache-maven
```

Then configure the environment variables for maven.

```
export MAVEN_HOME=/opt/apache-maven
export PATH=$MAVEN_HOME/bin:$PATH
```

Test

```
$ mvn help:system
BUILD SUCESS
```

- protobuf-2.5.0

Download protobuf-2.5.0. (required)

```
$ tar -zxvf protobuf-2.5.0.tar.gz
$ cd protobuf-2.5.0
$ sudo mkdir /usr/local/protoc-2.5.0/
$ ./configure --prefix=/usr/local/protoc-2.5.0/
$ make && sudo make install
```

Then configure the environment variables for maven.

```
export PROTOC_HOME=/usr/local/protoc-2.5.0
export PATH=$PROTOC_HOME/bin:$PATH
```

Test

```
$ protoc --version
libprotoc 2.5.0
```

- hadoop-3.1.4-src

Download hadoop-3.1.4-src on [available mirror](https://apache.01link.hk/hadoop/common/hadoop-3.1.4/hadoop-3.1.4-src.tar.gz).

```
$ tar -zxvf hadoop-3.1.4-src.tar.gz
```

Then configure the environment variable.

```
export HADOOP_SRC_DIR=/path/to/hadoop-3.1.4-src
export HADOOP_HOME=$HADOOP_SRC_DIR/hadoop-dist/target/hadoop-3.1.4
export PATH=$HADOOP_HOME/bin:$HADOOP_HOME/sbin:$PATH
```

Tip: Native libs used to  rebuild hadoop.

```
$ sudo apt-get -y install build-essential autoconf automake libtool cmake zlib1g-dev pkg-config libssl-dev
```

Edit repairboostcode/hadoop-3.1.4-integrate/install.sh with the proper directory to where you installed hadoop-3.1.4-src. Then execute the script to rebuild hadoop.

```
$ ./install.sh
```

### 3.2 Hadoop Configuration

The following shows an example to configure Hadoop-3.1.4 with 6 nodes.

| IP Address    | Role in Hadoop | Role in standalone |
| ------------- | -------------- | ------------------ |
| 192.168.0.201 | NameNode       | Coordinator        |
| 192.168.0.202 | DataNode       | Helper  |
| 192.168.0.203 | DataNode       | Helper  |
| 192.168.0.204 | DataNode       | Helper  |
| 192.168.0.205 | DataNode       | Helper  |
| 192.168.0.206 | DataNode       | Helper  |
You should modify the following configure files in /path/to/hadoop-3.1.4-src/hadoop-dist/target/hadoop-3.1.1/etc/hadoop.
- core-site.xml

```
<property><name>fs.defaultFS</name><value>hdfs://192.168.0.201:9000</value></property>
<property><name>hadoop.tmp.dir</name><value>/path/to/hadoop-3.1.4</value></property>
```

- hadoop-env.sh

```
export JAVA_HOME="/usr/lib/jvm/java-8-oracle" 
```

- hdfs-site.xml

```
<property><name>dfs.client.use.datanode.hostname</name><value>true</value></property>
<property><name>dfs.replication</name><value>1</value></property>
<property><name>dfs.blocksize</name><value>67108864</value></property>
<property><name>repairboost.coordinator</name><value>192.168.0.201</value></property>
<property><name>dfs.datanode.ec.reconstruction.stripedread.buffer.size</name><value>1048576</value></property>
<property><name>dfs.datanode.ec.repairboost</name><value>true</value></property>
<property><name>repairboost.packetsize</name><value>1048576</value></property>
<property><name>repairboost.packetcnt</name><value>64</value></property>
```

- user_ec_policies.xml

We provide sample configuration for RS-3-1-1024k erasure code policy.

- workers

This file contains multiple lines, each of which is a DataNode IP address.

```
192.168.0.202
192.168.0.203
192.168.0.204
192.168.0.205
192.168.0.206
```

### 3.3 Run
#### Start Hadoop

- Format the Hadoop cluster.

```
$ hdfs namenode -format
$ start-dfs.sh
$ hdfs dfsadmin -report 
```
If the report result indicates that there are 5 DataNodes, then the Hadoop cluster starts correctly.

- Set erasure coding policy.

```
$ hdfs ec -addPolicies -policyFile /path/to/user_ec_policies.xml
$ hdfs ec -enablePolicy -policy RS-3-1-1024k
$ hdfs dfs -mkdir /ec_test
$ hdfs ec -setPolicy -path /ec_test -policy RS-3-1-1024k
```

- Write data into HDFS.

```
$ dd if=/dev/urandom iflag=fullblock of=file.txt bs=64M count=3
$ hdfs dfs -put file.txt /ec_test/testfile
```

Check the writed data.

```
$ hdfs fsck / -files -blocks -locations
```

#### Start RepairBoost

- Get the directory where the erasure-coded data is stored in Hadoop.

```
$ ssh datanode
$ find -name "finalized"
```

You can ssh to any DataNode to execute the command.

- Modify the value of file.system.type, block.directory, and helpers.address in the configuration file config.xml. 

- Start.
```
$ cd repairboost-code
$ python scripts/start.py
```

#### Full-node Repair Test

- Stop a datanode in Hadoop.
```
$ ssh datanode
$ hdfs --daemon stop datanode
```
You can ssh to any DataNode to execute the command.

- Check the repaired data.

```
$ hdfs fsck / -files -blocks -locations
```

#### Stop

```
$ stop-dfs.sh
$ cd repairboost-code && python scripts/stop.py
```

