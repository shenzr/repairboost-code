#!/bin/bash

echo "start to install RepairBoost in Hadoop-3.1.4"

echo "copy the source to hadoop-src"

HADOOP_SRC_DIR=~/your_path_to/hadoop-3.1.4-src

cp DFSConfigKeys.java ${HADOOP_SRC_DIR}/hadoop-hdfs-project/hadoop-hdfs/src/main/java/org/apache/hadoop/hdfs

cp -R datanode ${HADOOP_SRC_DIR}/hadoop-hdfs-project/hadoop-hdfs/src/main/java/org/apache/hadoop/hdfs/server

cp -R blockmanagement ${HADOOP_SRC_DIR}/hadoop-hdfs-project/hadoop-hdfs/src/main/java/org/apache/hadoop/hdfs/server

echo "update the pom.xml in hadoop-src"

cp pom.xml ${HADOOP_SRC_DIR}/hadoop-hdfs-project/hadoop-hdfs

echo "rebuild hadoop"

cd ${HADOOP_SRC_DIR}; mvn package -DskipTests -Dtar -Dmaven.javadoc.skip=true -Drequire.isal -Disal.lib=/usr/lib/ -Dbundle.isal=true -Pdist,native -DskipShade -e


