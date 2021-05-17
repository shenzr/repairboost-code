package org.apache.hadoop.hdfs.server.datanode.erasurecode;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hdfs.server.datanode.DataNode;
import org.slf4j.Logger;
import java.io.IOException;
import java.net.*;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.*;
import org.apache.hadoop.util.Time;

import redis.clients.jedis.BinaryJedis;
import redis.clients.jedis.JedisPool;
import redis.clients.jedis.JedisPoolConfig;
import redis.clients.jedis.Pipeline;
import redis.clients.jedis.Response;

public class RepairBoostFullNodeClientStream {

    private DataNode _dataNode;
    private static final Logger LOG = DataNode.LOG;

    private byte[] _localIp;
    private JedisPool _localJedisPool = null;
    private int _localJedisPoolSize = 2;

    private int _packetSize = 0;
    private int _packetCnt = 0;
    private int _blockSize = 0;

    private String _lostFileName;
    private byte[] _currPacket = null;
    private ArrayBlockingQueue<byte[]> _content = null;

    private DataCollector dc;

 
    public RepairBoostFullNodeClientStream(String filename, Configuration conf, DataNode dataNode, int packetSize,
            int packetcnt) {
        this._packetCnt = packetcnt;
        this._packetSize = packetSize;
        this._lostFileName = filename;
        this._dataNode = dataNode;

        JedisPoolConfig _localJedisPoolConfig = new JedisPoolConfig();
        _localJedisPoolConfig.setMaxTotal(_localJedisPoolSize);
        _localJedisPool = new JedisPool(_localJedisPoolConfig, "127.0.0.1");

        try {
            InetAddress tmpnetaddr = InetAddress.getByName(_dataNode.getDatanodeId().getIpAddr());
            LOG.info("RepairBoostClient is running on " + _dataNode.getDatanodeId().getIpAddr());
            _localIp = tmpnetaddr.getAddress();
        } catch (Exception e) {
            LOG.info("Exception here");
        }

        this._blockSize = this._packetSize * this._packetCnt;
        this._content = new ArrayBlockingQueue<byte[]>(this._packetCnt);

        this.dc = new DataCollector(this._lostFileName, this._packetCnt, this._packetSize);
        dc.start();
    }

    public String getLostFileName() {
        return this._lostFileName;
    }

    public byte[] readFromRepairBoost(int toReconstruct) throws IOException {
        int copied = 0;
        byte[] targetbuf = new byte[toReconstruct];
        
        // LOG.info("LinesYao Start to read a packet from RepairBoost.");
        if (toReconstruct != _packetSize) {
            LOG.info("The size of toread data can not fit the size of packet!");
        }
        if (_currPacket == null) {
            try {
                _currPacket = _content.take();
            } catch (InterruptedException e) {
                LOG.info("RepairBoostInputStream.read exception");
            }
        }
        System.arraycopy(_currPacket, 0, targetbuf, copied, toReconstruct);
        _currPacket = null;
        // LOG.info("Finish the reading of a packet from RepairBoost.");
        return targetbuf;
    }

    private class DataCollector implements Runnable {
        private byte[] _redisKeys;
        private int _packetcnt;
        private int _packetsize;
        private String _filename;
        private Thread t;

        DataCollector(String filename, int packetcnt, int packetsize) {
            _filename = filename;
            _packetcnt = packetcnt;
            _packetsize = packetsize;
            _redisKeys = filename.getBytes();

        }

        public void run() {
            try {
                BinaryJedis jedis = _localJedisPool.getResource();
                for(int i=0; i<_packetCnt; ++i) {
                    List<byte[]> reply = jedis.blpop(0, _redisKeys);

                    try {
                        _content.put(reply.get(1));
                    } catch (InterruptedException iex) {
                        LOG.info("Exception in collecting data");
                    }
                    if(i%8 == 0) LOG.info(_filename + "collected " + i +" th pkt~");
                }

                jedis.close();
                
            } catch (Exception e) {
                LOG.info("Exception localJedisConnect");
            }

            LOG.info(_filename + "collect the blk: " + _filename + "finished ~~~~~~~");

        }

        public void start() {
            t = new Thread(this, "DataCollector");
            t.start();
        }
    }
}
