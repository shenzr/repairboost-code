# RepairBoost

## Table of Contents

- [Install](#install)

- [Standalone Test of RepairBoost](#standalone-test-of-repairBoost)

  - [Prerequisites](#prerequisites)
    - [Configuration File](#configuration-file)
    - [Encoding Matrix File](#encoding-matrix-file)
    - [Create Erasure-Coded Chunks](#Create-Erasure-Coded-Chunks)
  - [Run](#run)  
    - [Start RepairBoost](#Start-RepairBoost)
    - [Test](#full-node-repair-test)
    - [Stop RepairBoost](#Stop-RepairBoost)



  ## Install

  We have tested RepairBoost on Ubuntu16.04 LTS.

  ### Common

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

  ### Compile RepairBoost

  After finishing the preparations above, download and compile the source code.

  ```
  $ cd repairboost-code
  $ make
  ```



 ## Standalone Test of RepairBoost

 ### Prerequisites

 #### Configuration File

 We configure the settings of RepairBoost via the configuration file config.xml in XML format. 

| Property  | Description |
| ---- | ---- |
| erasure.code.type | Three types are implemented: RS, LRC, and BUTTERFLY (only $n$=6, $k$=4). |
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

 For RS codes and LRC codes, we read the encoding coefficients from the encoding matrix file. The file contains an (n-k)*k matrix that specifies the coefficients for generating n-k parity chunks from k data chunks. 

For example, the file repairboost-code/conf/rsEncMat_6_9 shows a encoding matrix for RS (9, 6) (n=9, k=6).

![](https://tva1.sinaimg.cn/large/008i3skNly1gqj77wox3uj327c0cuq5k.jpg)

 #### Create Erasure-Coded Chunks

Before testing our standalone system, we need to create stripes of erasure-coded chunks and corresponding metadata. We have provided programs for generating chunks of different codes under repairboost-code/test/.

```
$ cd repairboost-code/test
$ make
$ dd if=/dev/urandom iflag=fullblock of=input.txt bs=64M count=6
$ ./createdata_rs ../conf/rsEncMat_6_9 input.txt 6 9
```

For more detail, you can run the command ./createdata_rs (or ./createdata_lrc, etc.) to get the usage of the programs.

Take RS(6, 4) (n=6, k=4) as an example. In repairboost-code/test, we create 4 files of uncoded chunks and 2 files of coded chunks. We can distribute the 6 chunks across 6 nodes, each of which stores one chunk under the path specified by block.directory. The coordinator stores the stripe metadata under the path specified by meta.stripe.dir. 

The naming of data files and metadata should meet the requirements. If there are 2 stripes, the data of the stripes are named as follows:

```
stripe_0_file_k1   stripe_0_file_k2   stripe_0_file_k3   stripe_0_file_k4   stripe_0_file_m1   stripe_0_file_m2 
stripe_1_file_k1   stripe_1_file_k2   stripe_1_file_k3   stripe_1_file_k4   stripe_1_file_m1   stripe_1_file_m2 
```

The coordinator stores 6 files:   rs:stripe_0_file_k1_1001, rs:stripe_0_file_k2_1001, rs:stripe_0_file_k3_1001, rs:stripe_0_file_k4_1001, rs:stripe_0_file_m1_1002 and rs:stripe_0_file_m2_1002, all of which have the following content:

```
stripe_0_file_k1_1001:stripe_0_file_k2_1001:stripe_0_file_k3_1001:stripe_0_file_k4_1001:stripe_0_file_m1_1002:stripe_0_file_m2_1002
```

The file name rs:file_k1_1001 means that the chunk uses Reed-Solomon (RS) code and the chunk name is file_k1. The tail 1001 (resp. 1002) means the chunk is an uncoded chunk (resp. coded chunk).

### Run

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





