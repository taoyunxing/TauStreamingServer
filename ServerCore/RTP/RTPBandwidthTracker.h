
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPBandwidthTracker.h
Description: Uses Karns Algorithm to measure exact round trip times(RTT). This also
             tracks the current window size based on input from the caller.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __RTP_BANDWIDTH_TRACKER_H__
#define __RTP_BANDWIDTH_TRACKER_H__

#include "OSHeaders.h"

class RTPBandwidthTracker
{
    public:

        RTPBandwidthTracker(Bool16 inUseSlowStart)
         :  fRunningAverageMSecs(0),
            fRunningMeanDevationMSecs(0),
            fCurRetransmitTimeout( kMinRetransmitIntervalMSecs ),/* ����Ϊ600ms */
            fUnadjustedRTO( kMinRetransmitIntervalMSecs ),/* ����Ϊ600ms */
            fCongestionWindow(kMaximumSegmentSize),/* ������Ϊ1466,һ��RTP Packet�Ĵ�С */
            fSlowStartThreshold(0),/* ���òμ�RTPBandwidthTracker::SetWindowSize(),��1466�ֽں�client���ڴ�С֮�� */
            fSlowStartByteCount(0),
            fClientWindow(0),/* ע���ʼֵΪ0, used in RTPBandwidthTracker::SetWindowSize() */
            fBytesInList(0), /* RTPStream�з��͵�δ�õ�ȷ�ϵ��ֽ���,����Ϊ0 */
            fAckTimeout(kMinAckTimeout),   /* �ȳ�ʼ��Ϊ20ms */
            fUseSlowStart(inUseSlowStart), /* ������������Ƿ�ʹ���������㷨? */
            fMaxCongestionWindowSize(0),
            fMinCongestionWindowSize(1000000),
            fMaxRTO(0),
            fMinRTO(24000),
            fTotalCongestionWindowSize(0),
            fTotalRTO(0),
            fNumStatsSamples(0)
        {}
        
        ~RTPBandwidthTracker() {}
        
        // Initialization - give the client's window size.
        void SetWindowSize(SInt32 clientWindowSize);
 
        // Before sending new data, let the tracker know
        // how much data you are sending so it can adjust the window size by EmptyWindow.
        void FillWindow(UInt32 inNumBytes)      { fBytesInList += inNumBytes; fIsRetransmitting = false; }        
        
        // When data is acked, let the tracker know how much
        // data was acked so it can adjust the window
        void EmptyWindow(UInt32 inNumBytes, Bool16 updateBytesInList = true);
        
        // When retransmitting a packet, call this function so
        // the tracker can adjust the window sizes and back off.
        void AdjustWindowForRetransmit();

		// Each RTT sample you get, let the tracker know what it is so it can keep a good running average.
		/* �Ը������������,��Karn�㷨����RTO */
		void AddToRTTEstimate( SInt32 rttSampleMSecs );
        
        // ACCESSORS
		/* ׼���ý���Client Ack����? */
        const Bool16 ReadyForAckProcessing()    { return (fClientWindow > 0 && fCongestionWindow > 0); } // see RTPBandwidthTracker::EmptyWindow for requirements
        /* �Ƿ���Ҫ����?��RTPStream�з��͵�δ�õ�ȷ�ϵ��ֽ��������������Ĵ�Сʱ,�������� */
		const Bool16 IsFlowControlled()         { return ( (SInt32)fBytesInList >= fCongestionWindow ); }

        const SInt32 ClientWindowSize()         { return fClientWindow; }
        const UInt32 BytesInList()              { return fBytesInList; }
        const SInt32 CongestionWindow()         { return fCongestionWindow; }
        const SInt32 SlowStartThreshold()       { return fSlowStartThreshold; }

        const SInt32 RunningAverageMSecs()      { return fRunningAverageMSecs / 8; }  // fRunningAverageMSecs is stored scaled up 8x
        const SInt32 RunningMeanDevationMSecs() { return fRunningMeanDevationMSecs/ 4; } // fRunningMeanDevationMSecs is stored scaled up 4x

		/* ��ȡ��ǰrestransmit timeout */
        const SInt32 CurRetransmitTimeout()     { return fCurRetransmitTimeout; }
        
		/* ��ȡ��ǰ��������� */
		const SInt32 GetCurrentBandwidthInBps()
            { return (fUnadjustedRTO > 0) ? (fCongestionWindow * 1000) / fUnadjustedRTO : 0; }

		/* ��ȡClient��ack timeout,��20--100֮�� */
        inline const UInt32 RecommendedClientAckTimeout() { return fAckTimeout; }

		/* ���õ�Ӱ��ǰBitrate,����karn�㷨�õ���RTOֵ,������ȴ���ǰAck������ʱ��fAckTimeout,��ȷ������20��100֮�� */
        void UpdateAckTimeout(UInt32 bitsSentInInterval, SInt64 intervalLengthInMsec);

		/* ��congestion Window��RTO����ֵfUnadjustedRTO,�ֱ�ͳ�����ǵ������Сֵ,���ۼ����ǵ��ܺ� */ 
        void UpdateStats();/* needed by RTPSession::run() */

        // Stats access, ��UpdateStats()��������Щͳ����

		/* ��ȡ���/��С/ƽ��congestion Windows��С */
        SInt32   GetMaxCongestionWindowSize()    { return fMaxCongestionWindowSize; }
        SInt32   GetMinCongestionWindowSize()    { return fMinCongestionWindowSize; }
        SInt32   GetAvgCongestionWindowSize()    { return (SInt32)(fTotalCongestionWindowSize / (SInt64)fNumStatsSamples); }

        /* ��ȡ���/��С/ƽ��RTO(Retransmit Timeout) */
        SInt32   GetMaxRTO()                     { return fMaxRTO; }
        SInt32   GetMinRTO()                     { return fMinRTO; }
        SInt32   GetAvgRTO()                     { return (SInt32)(fTotalRTO / (SInt64)fNumStatsSamples); }
        
        enum
        {
			/* ���(TCP���ݶ�)���ĳ���,����һ��RTP���ĳ��� */
            kMaximumSegmentSize = 1466,  // enet - just a guess!
            
            // Our algorithm for telling the client what the ack timeout
            // is currently not too sophisticated. This could probably be made better.
            // ���ǵ��㷨Ҫ����Client ack timeoutֵ,���ǵ��㷨����̫����,���������ø���Щ
			// During slow start algorithm, we just use 20, and afterwards, just use 100
			/* Ack�ĳ�ʱ��Χ(ms) */
            kMinAckTimeout = 20,
            kMaxAckTimeout = 100
        };
        
    private:
  
        // For computing the round-trip estimate using Karn's algorithm,�μ�RTPBandwidthTracker::AddToRTTEstimate()
        SInt32  fRunningAverageMSecs;     //��ƽ����RTT�����ڹ�����һ�ε�RTO
        SInt32  fRunningMeanDevationMSecs;//��ֵƫ����ڹ�����һ��RTO

		/************* ���������ڱ����м�����Ҫ!! *****************************/
        SInt32  fCurRetransmitTimeout;//��ǰ������ش���ʱRTO,���òμ�RTPBandwidthTracker::AddToRTTEstimate(),ע������ǳ���Ҫ!!	 
        SInt32  fUnadjustedRTO;       //δ������Χ��������RTO,����fCurRetransmitTimeoutֵ���Ա�,����һ����600ms-24000ms��Χ��
		Bool16  fIsRetransmitting;    // are we in the re-transmit 'state' ( started resending, but have yet to send 'new' data) //�Ƿ��������ش�״̬?�μ�RTPBandwidthTracker::AdjustWindowForRetransmit()
        
        // Tracking our window sizes
        SInt64  fLastCongestionAdjust;  //�ϴ�ӵ������ʱ��,�μ�RTPBandwidthTracker::AdjustWindowForRetransmit()
        SInt32  fCongestionWindow;      // implentation of VJ congestion avoidance  //��ǰ��������ӵ�����ڴ�С
        SInt32  fSlowStartThreshold;    // point at which we stop adding to the window for each ack, and add to the window for each window full of acks // ����������
		SInt32  fSlowStartByteCount;    // counts window a full of acks when past ss(slow start) thresh //?? ������������ֵ��,�����ڱ��ļ���,�μ�RTPBandwidthTracker::AdjustWindowForRetransmit()
		Bool16  fUseSlowStart;          //ʹ���������㷨ss(slow start) algorithm��?

		
        /*********** NOTE:������ǳ���Ҫ!! ***************/
        SInt32  fClientWindow;  // max window size based on client UDP buffer,Client���ڴ�С,Ҳ����Client�˽��ջ����С	
        UInt32  fBytesInList;   // how many unacked bytes on this stream,RTPStream�з��͵�δ�õ�ȷ�ϵ��ֽ���,�μ�RTPBandwidthTracker::EmptyWindow()
		/*********** NOTE:������ǳ���Ҫ!! ***************/

		/* used in RTCPSRPacket::SetAckTimeout(),RTPBandwidthTracker::UpdateAckTimeout() */
        UInt32  fAckTimeout; //Client Ack�ͻ���Ӧ��ʱʱ��(�ȴ��೤ʱ��,��ǰAck�ᵽ��?),�μ�����ĳ���kMinAckTimeout,kMaxAckTimeout
        
        // Stats
		/* ��UpdateStats()��������Щͳ���� */
        SInt32  fMaxCongestionWindowSize;   //��ǰcwnd��fClientWindow�������ֵ
        SInt32  fMinCongestionWindowSize;   //��ǰcwnd��fClientWindow������Сֵ
		SInt64  fTotalCongestionWindowSize; //��cwnd��С��Ϊͳ������������cwndƽ����С����
        SInt32  fMaxRTO;   //��ǰRTO��fUnadjustedRTO�������ֵ
        SInt32  fMinRTO;   //��ǰRTO��fUnadjustedRTO������Сֵ  
        SInt64  fTotalRTO; //RTO������Ϊͳ������������RTO��ƽ����С
        SInt32  fNumStatsSamples; //���cwnd�Ĵ�����Ϊͳ������������cwndƽ����С����
        
		/* RTO��Χ(��λ��ms),�μ�RTPBandwidthTracker::AddToRTTEstimate() */
        enum
        {
            kMinRetransmitIntervalMSecs = 600,
            kMaxRetransmitIntervalMSecs = 24000
        };
};

#endif // __RTP_BANDWIDTH_TRACKER_H__
