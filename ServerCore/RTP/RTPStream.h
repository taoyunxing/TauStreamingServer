
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPStream.h
Description: Represents a single client stream (audio, video, etc), contains 
             all stream-specific data & resources & API, used by RTPSession when it
			 wants to send out or receive data for this stream, also implements 
			 the RTP stream dictionary for QTSS API.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 

#ifndef __RTPSTREAM_H__
#define __RTPSTREAM_H__

#include "QTSS.h"
#include "QTSServerInterface.h"
#include "QTSSDictionary.h"
#include "QTSS_Private.h"

#include "UDPDemuxer.h"
#include "UDPSocketPool.h"

#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTPPacketResender.h"/* �����ش��� */


class RTPStream : public QTSSDictionary, public UDPDemuxerTask //ע��RTPStream��Ϊ��ϣ��Ԫ
{
    public:
        
        // Initializes dictionary resources
        static void Initialize();

        //
        // CONSTRUCTOR / DESTRUCTOR
        
        RTPStream(UInt32 inSSRC, RTPSessionInterface* inSession);
        virtual ~RTPStream();
        
        //
        //ACCESS FUNCTIONS
        
        UInt32      GetSSRC()                   { return fSsrc; }
        UInt8       GetRTPChannelNum()          { return fRTPChannel; }
        UInt8       GetRTCPChannelNum()         { return fRTCPChannel; }

		/* �õ��ش����ݰ����ָ�� */
        RTPPacketResender* GetResender()        { return &fResender; }
        QTSS_RTPTransportType GetTransportType(){ return fTransportType; }
        UInt32      GetStalePacketsDropped()    { return fStalePacketsDropped; }
        UInt32      GetTotalPacketsRecv()       { return fTotalPacketsRecv; }

        // Setup uses the info in the RTSPRequestInterface to associate
        // all the necessary resources, ports, sockets, etc, etc, with this
        // stream.
        QTSS_Error Setup(RTSPRequestInterface* request, QTSS_AddStreamFlags inFlags);
        
        // Write sends RTP/SR data to the client. Caller must specify
        // either qtssWriteFlagsIsRTP or qtssWriteFlagsIsRTCP
        virtual QTSS_Error  Write(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, QTSS_WriteFlags inFlags);
 
        
        //UTILITY FUNCTIONS:
        //These are not necessary to call and do not manipulate the state of the
        //stream. They may, however, be useful services exported by the server
        
        // Formats a standard setup response.
		/* ����SETUP��Ӧ */
        void            SendSetupResponse(RTSPRequestInterface* request);

        //Formats a transport header for this stream. 
		/* ���ɴ���ͷ */
        void            AppendTransport(RTSPRequestInterface* request);
        
        //Formats a RTP-Info header for this stream.
        //Isn't useful unless you've already called Play()
		/* ���� RTP-Infoͷ */
        void            AppendRTPInfo(QTSS_RTSPHeader inHeader,
                                        RTSPRequestInterface* request, UInt32 inFlags, Bool16 lastInfo);

        //
        // When we get an incoming Interleaved Packet for this stream, this
        // function should be called
		/* �����յ���Interleaved packet */
        void ProcessIncomingInterleavedData(UInt8 inChannelNum, RTSPSessionInterface* inRTSPSession, StrPtrLen* inPacket);

        //When we get a new RTCP packet, we can directly invoke the RTP session and tell it
        //to process the packet right now!
		/* ����RTCP�� */
        void ProcessIncomingRTCPPacket(StrPtrLen* inPacket);

        // Send a RTCP SR on this stream. Pass in true if this SR should also have a BYE
		/* need by RTPSession::run() */
        void SendRTCPSR(const SInt64& inTime, Bool16 inAppendBye = false);
        
        //
        // Retransmits get sent when there is new data to be sent, but this function
        // should be called periodically even if there is no new packet data, as
        // the pipe���ܵ��� should have a steady stream of data in it. 
		/* need by RTPSession::run() */
        void SendRetransmits();

        //
        // Update the thinning parameters for this stream to match current prefs
        void SetThinningParams();
        
		//
		// Reset the delay parameters that are stored for the thinning calculations
		void ResetThinningDelayParams() { fLastCurrentPacketDelay = 0; }
		
		void SetLateTolerance(Float32 inLateToleranceInSec) { fLateToleranceInSec = inLateToleranceInSec; }
		
		void EnableSSRC()  { fEnableSSRC = true; }
		void DisableSSRC() { fEnableSSRC = false; }
		
    private:
        
        enum
        {
            kMaxSsrcSizeInBytes         = 12,
            kMaxStreamURLSizeInBytes    = 32,
            kDefaultPayloadBufSize      = 32, /* Ĭ�ϵ�Payload�ַ�����С */
            kSenderReportIntervalInSecs = 7,  /* ÿ7s��������Client����һ��SR�� */
            kNumPrebuiltChNums          = 10, /* Ԥ���ɵ�Interleave channel�ŵĸ��� */
        };
    
		// Quality
        SInt64      fLastQualityChange;
        SInt32      fQualityInterval; /* Quality Level���Ƽ�� */
		SInt32      fQualityLevel;    /* Quality Level���Ƽ��� */
		UInt32      fNumQualityLevels;/* Quality Level���� */

        //either pointers to the statically allocated sockets (maintained by the server)
        //or fresh ones (only fresh in extreme special cases)
        UDPSocketPair*          fSockets; //ָ��QTSServer����ʱȫ�ַ����UDPSocketPair,�μ�QTSServer::SetupUDPSockets()
        RTPSessionInterface*    fSession;

        // info for kind a reliable UDP
        //DssDurationTimer      fInfoDisplayTimer;	
        SInt32                  fBytesSentThisInterval; /* ��ʱ�������͵��ֽ����� */
        SInt32                  fDisplayCount;          /* �μ�RTPPacketResender::SpillGuts() */
        Bool16                  fSawFirstPacket;	    /* ������һ��������?�μ�RTPStream::ReliableRTPWrite() */
        SInt64                  fStreamCumDuration;     /* RTPStream�ۼ�ʱ��,�μ�RTPStream::ReliableRTPWrite() */

        // manages UDP retransmits
		/************** ��֤RTP�������QoS���� *******************/
		
        RTPPacketResender       fResender;/* �����ش���,Ӧ�õ������RTPBandwidthTracker�� */
        RTPBandwidthTracker*    fTracker; /* ӵ�������� */
       
		/************** ��֤RTP�������QoS���� *******************/
        
        //who am i sending to?
        UInt32      fRemoteAddr;    //client�˵�ip
        UInt16      fRemoteRTPPort; //client�˽���RTP���Ķ˿�
        UInt16      fRemoteRTCPPort;//client�˷���RTCP���Ķ˿�
        UInt16      fLocalRTPPort;  //Server�˷���RTP���Ķ˿�

		// If we are interleaving RTP data over the TCP connection,
		// these are channel numbers to use for RTP & RTCP
		/* TCP/RTCPͨ���� */
		UInt8       fRTPChannel;
		UInt8       fRTCPChannel;
        
        //RTCP stuff,see RTPStream::ProcessIncomingRTCPPacket()/SendRTCPSR()/RTPStream::Write()  
        SInt64      fLastSenderReportTime; /* �ϴ�Server�˷���SR��ʱ��� */
        UInt32      fPacketCount;/* ��ǰ���͵�RTP��SR������, �ӿ�ʼ���䵽��SR������ʱ�÷����߷��͵�RTP���ݰ������� */
        UInt32      fLastPacketCount;/* �ϴη��͵�RTP������ */
        UInt32      fPacketCountInRTCPInterval;/* ��һ��RTCPʱ�����ڷ��͵�RTP������ */
        UInt32      fByteCount; /* Server�˷��ͳ������ֽ��� */
        
        // DICTIONARY ATTRIBUTES
        
        //Module assigns a streamID to this object
        UInt32      fTrackID; /* һ��track��Ӧһ��RTPStream */
        
        //low-level RTP stuff 	
        UInt32      fSsrc; /* ͬ��Դ */
        char        fSsrcString[kMaxSsrcSizeInBytes];/* ͬ��Դ�ַ��� */
        StrPtrLen   fSsrcStringPtr;
        Bool16      fEnableSSRC;/* Server��Client����RTSP Responseʱ,����SSRC��? */
        
        //Payload name and codec type.
        char                    fPayloadNameBuf[kDefaultPayloadBufSize]; /* payload�����ַ���,"mpeg4-generic/22050/2"��"MP4V-ES/90000" */
        QTSS_RTPPayloadType     fPayloadType; /* audio & video */	
		Bool16                  fIsTCP;
		QTSS_RTPTransportType   fTransportType;/* �������ͣ�UDP/RUDP/TCP�� */
		QTSS_RTPNetworkMode     fNetworkMode;   //unicast | multicast

        //Media information.
        UInt16      fFirstSeqNumber;//used in sending the play response
        UInt32      fFirstTimeStamp;//RTP timestamp,see RTPStream::AppendRTPInfo()
        UInt32      fTimescale;     //��track��ʱ��̶�
		UInt32      fLastRTPTimestamp;
        
        //what is the URL for this stream?
        char        fStreamURL[kMaxStreamURLSizeInBytes]; //"trackID=3"
        StrPtrLen   fStreamURLPtr;
        SInt64      fStreamStartTimeOSms; //RTPStream��ʼ��ʱ��
		// Pointer to the stream ref (this is just a this pointer)
		QTSS_StreamRef  fStreamRef; /* stream��thisָ�� */
     
        // RTCP data,especially for APP
        UInt32      fFractionLostPackets;
        UInt32      fTotalLostPackets;
        UInt32      fJitter;
        UInt32      fReceiverBitRate;              //�����߱�����
        UInt16      fAvgLateMsec;                  //ƽ����ʱ(ms)
        UInt16      fPercentPacketsLost;           //�����ٷֱ�
        UInt16      fAvgBufDelayMsec;              //ƽ��������ʱ(ms)
        UInt16      fIsGettingBetter;              //�����������ڱ����?
        UInt16      fIsGettingWorse;               //�����������ڱ����?
        UInt32      fNumEyes;                      //������
        UInt32      fNumEyesActive;                //��Ծ�Ĺ�����
        UInt32      fNumEyesPaused;                //��ͣ�Ĺ�����
        UInt32      fTotalPacketsRecv;             //�յ����ܰ���
        UInt32      fPriorTotalPacketsRecv;        //ǰһ���յ����ܰ���
        UInt16      fTotalPacketsDropped;          //�������ܰ���
        UInt16      fTotalPacketsLost;             //��ʧ���ܰ���
        UInt32      fCurPacketsLostInRTCPInterval;
        UInt16      fClientBufferFill;             //�ͻ��˻��������
        UInt16      fFrameRate;                    //֡��
        UInt16      fExpectedFrameRate;            //������֡��
        UInt16      fAudioDryCount;                //��Ƶ����
        UInt32      fClientSSRC;                   //Client�˵�SSRC,�μ�RTCPSRPacket��
          
        // HTTP params ���TCP��ʽ�ı�������
		// Each stream has a set of thinning related tolerances,that are dependent on prefs and parameters in the SETUP. 
		// These params, as well as the current packet delay determine whether a packet gets dropped.
        SInt32      fTurnThinningOffDelay_TCP;
        SInt32      fIncreaseThinningDelay_TCP;
        SInt32      fDropAllPacketsForThisStreamDelay_TCP;
        UInt32      fStalePacketsDropped_TCP;
        SInt64      fTimeStreamCaughtUp_TCP;
        SInt64      fLastQualityLevelIncreaseTime_TCP;

		// the server's thinning algorithm(ms)
        // Each stream has a set of thinning related tolerances,that are dependent on prefs and parameters in the SETUP. 
        // These params, as well as the current packet delay determine whether a packet gets dropped.
        SInt32      fThinAllTheWayDelay;
        SInt32      fAlwaysThinDelay;
        SInt32      fStartThinningDelay;
        SInt32      fStartThickingDelay;
        SInt32      fThickAllTheWayDelay;
        SInt32      fQualityCheckInterval;             //���������
        SInt32      fDropAllPacketsForThisStreamDelay; //�������RTPStream�����а����ӳټ���,�μ�RTPStream::SetThinningParams()
        UInt32      fStalePacketsDropped;              //�ӵ���ʱ������
        SInt64      fLastCurrentPacketDelay;           //�ϴεķ�����ʱֵ
        Bool16      fWaitOnLevelAdjustment;             //�Ƿ�ȴ��������?
              
		/* ÿ����һ�����ݰ�DSS�������һ�β�����������������ֵ��ʾ��ǰ���Ƿ�Ӧ�÷���. fLateToleranceInSecΪ
		��һ���õ��Ŀͻ�����ʱ������ͻ���û��ͨ��SETUP������Ĭ��ֵΪ1.5�롣�μ�RTPStream::SetThinningParams()/Setup() */
        Float32     fLateToleranceInSec; //�ͻ����ӳ�,Ĭ��1.5s,��RTSPRequestInterface��RTSPͷ"x-RTP-Options: late-tolerance=3"�õ�
        Float32     fBufferDelay; // 3.0 from the sdp        
        
		/* Server���͵�Ack? */
        UInt32      fCurrentAckTimeout;
        SInt32      fMaxSendAheadTimeMSec; //�����ǰ����ʱ��(ms)
        
#if DEBUG	
        UInt32      fNumPacketsDroppedOnTCPFlowControl;/* TCP���صĶ����� */
        SInt64      fFlowControlStartedMsec; /* ���ؿ�ʼʱ���(ms) */
        SInt64      fFlowControlDurationMsec;/* ���س���ʱ��(ms) */
#endif
        		    
		/************************ ����write functions,������װ��RTPStream::Write()�� ******************************/
        // acutally write the data out that way in interleaved channels
        QTSS_Error  InterleavedWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, unsigned char channel );

        // implements the ReliableRTP protocol
        QTSS_Error  ReliableRTPWrite(void* inBuffer, UInt32 inLen, const SInt64& curPacketDelay);

        void        SetTCPThinningParams();
        QTSS_Error  TCPWrite(void* inBuffer, UInt32 inLen, UInt32* outLenWritten, UInt32 inFlags);

		/************************ write functions*******************************/

		//dictionary attr
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
        static StrPtrLen                    sChannelNums[];
        static QTSS_ModuleState             sRTCPProcessModuleState;

		//protocol TYPE str
        static char *noType;
        static char *UDP;
        static char *RUDP;
        static char *TCP;
        
        Bool16      UpdateQualityLevel(const SInt64& inTransmitTime, const SInt64& inCurrentPacketDelay,const SInt64& inCurrentTime, UInt32 inPacketSize);                                        
        SInt32      GetQualityLevel();
        void        SetQualityLevel(SInt32 level);
           
		/* RTP/RTCP Packet types */
        enum { rtp = 0, rtcpSR = 1, rtcpRR = 2, rtcpACK = 3, rtcpAPP = 4 };

        char*   GetStreamTypeStr();
        Float32 GetStreamStartTimeSecs() { return (Float32) ((OS::Milliseconds() - this->fSession->GetSessionCreateTime())/1000.0); }
       
		void PrintPacket(char *inBuffer, UInt32 inLen, SInt32 inType); 
        void PrintRTP(char* packetBuff, UInt32 inLen);
        void PrintRTCPSenderReport(char* packetBuff, UInt32 inLen);
inline  void PrintPacketPrefEnabled(char *inBuffer,UInt32 inLen, SInt32 inType) { if (QTSServerInterface::GetServer()->GetPrefs()->PacketHeaderPrintfsEnabled() ) this->PrintPacket(inBuffer,inLen, inType); }

        /* QTSSFileModule::SendPackets() encounter error */
        void SetOverBufferState(RTSPRequestInterface* request);

};

#endif // __RTPSTREAM_H__
