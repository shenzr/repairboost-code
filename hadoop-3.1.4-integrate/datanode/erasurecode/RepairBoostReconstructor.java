package org.apache.hadoop.hdfs.server.datanode.erasurecode;

import java.io.IOException;
import java.nio.ByteBuffer;

import org.apache.hadoop.classification.InterfaceAudience;
import org.apache.hadoop.hdfs.server.datanode.DataNodeFaultInjector;
import org.apache.hadoop.hdfs.server.datanode.metrics.DataNodeMetrics;
import org.apache.hadoop.util.Time;

// add 
import org.apache.hadoop.conf.Configuration;
import org.apache.hadoop.hdfs.server.datanode.erasurecode.RepairBoostFullNodeClientStream;
import org.apache.hadoop.hdfs.server.datanode.DataNode;
import java.util.Arrays;
///


/**
 * RepairBoostReconstructor reconstruct one or more missed striped block in the
 * striped block group.
 */
@InterfaceAudience.Private
class RepairBoostReconstructor extends StripedReconstructor implements Runnable {
    private StripedWriter stripedWriter;
    private DataNode datanode;

    // RepairBoostClient
    Configuration _conf;
    private int _packetSize = 0;
    private int _packetCnt = 0;
    private String[] lostFileName;
    private RepairBoostFullNodeClientStream[] repairBoostClient;
    ///

    RepairBoostReconstructor(ErasureCodingWorker worker, StripedReconstructionInfo stripedReconInfo) {
        super(worker, stripedReconInfo);
        this.datanode = worker.getDatanode();

        stripedWriter = new StripedWriter(this, getDatanode(), getConf(), stripedReconInfo);
        this._conf = getConf();
        this._packetSize = _conf.getInt("repairboost.packetsize", 262144);
        LOG.info("RepairBoostClient packet size:" + this._packetSize);
        this._packetCnt = _conf.getInt("repairboost.packetcnt", 128);
        LOG.info("RepairBoostClient packet cnt:" + this._packetCnt);
    }

    boolean hasValidTargets() {
        return stripedWriter.hasValidTargets();
    }

    
    @Override
    public void run() {
        try {
            this.lostFileName = stripedWriter.getLostFileName();
            int lostFileNum = lostFileName.length;
            this.repairBoostClient = new RepairBoostFullNodeClientStream[lostFileNum];
            for (int i = 0; i < lostFileNum; ++i) {
                this.repairBoostClient[i] = new RepairBoostFullNodeClientStream(lostFileName[i], getConf(), datanode,
                        _packetSize, _packetCnt);
            }
            initDecoderIfNecessary();
            getStripedReader().init();
            stripedWriter.init();

            reconstruct();
            stripedWriter.endTargetBlocks();
            long finishTime = Time.monotonicNow();
            LOG.info("LinesYao repairFinishTime: " + finishTime);

        } catch (Throwable e) {
            LOG.warn("Failed to reconstruct striped block: {}", getBlockGroup(), e);
            getDatanode().getMetrics().incrECFailedReconstructionTasks();
        } finally {
            getDatanode().decrementXmitsInProgress(getXmits());
            getStripedReader().close();
            stripedWriter.close();
            cleanup();
        }
    }

    @Override
    void reconstruct() throws IOException {
        // Measurement
        long startTime = Time.monotonicNow();
        while (getPositionInBlock() < getMaxTargetLength()) {
            // LOG.info("LinesYao getPositionInBlock(): " + getPositionInBlock() + ",  getMaxTargetLength(): " + getMaxTargetLength());
            DataNodeFaultInjector.get().stripedBlockReconstruction();

            final int toReconstructLen = this._packetSize;
            reconstructTargets(toReconstructLen);

            if (stripedWriter.transferData2Targets() == 0) {
                String error = "Transfer failed for all targets.";
                throw new IOException(error);
            }

            updatePositionInBlock(toReconstructLen);

            clearBuffers();
        }
        LOG.info("LinesYao readFromRepairBoost for blk: " + lostFileName[0] + "ok !!!");

    }

    private void reconstructTargets(int toReconstructLen) throws IOException {
        ByteBuffer[] outputs = stripedWriter.getRealTargetBuffers(toReconstructLen);

        // todo put data to targetbuffer outputs
        if (outputs.length != repairBoostClient.length) {
            LOG.info("LinesYao Error: the outputs cannot match repairBoostClien");
        }
        for (int i = 0; i < repairBoostClient.length; ++i) {
            byte[] recoverPkt = repairBoostClient[i].readFromRepairBoost(toReconstructLen);
            outputs[i].put(recoverPkt);
            outputs[i].rewind();
        }

        stripedWriter.updateRealTargetBuffers(toReconstructLen);
    }

    private void clearBuffers() {
        getStripedReader().clearBuffers();
        stripedWriter.clearBuffers();
    }

    public String[] getLostFileNames() {
        return this.lostFileName;
    }

}
