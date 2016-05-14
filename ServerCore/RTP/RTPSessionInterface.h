
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSessionInterface.h
Description: Provides an API interface for objects to access the attributes 
             related to a RTPSession, also implements the RTP Session Dictionary.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef _RTPSESSIONINTERFACE_H_
#define _RTPSESSIONINTERFACE_H_

#include "QTSSDictionary.h"
#include "QTSServerInterface.h"

#include "TimeoutTask.h"
#include "Task.h"
#include "RTCPSRPacket.h"
#include "RTSPSessionInterface.h"
#include "RTPBandwidthTracker.h"
#include "RTPOverbufferWindow.h"

#include "OSMutex.h"
#include "atomic.h"

class RTSPRequestInterface;

class RTPSessionInterface : public QTSSDictionary, public Task
{
    public:
    
        // Initializes dictionary resources
        static void Initialize();
        
        // CONSTRUCTOR / DESTRUCTOR   
        RTPSessionInterface();
        virtual ~RTPSessionInterface()
        {   
            if (GetQualityLevel() != 0)
                QTSServerInterface::GetServer()->IncrementNumThinned(-1);
            if (fRTSPSession != NULL)
                fRTSPSession->DecrementObjectHolderCount();
            delete [] fSRBuffer.Ptr;
            delete [] fAuthNonce.Ptr;       
            delete [] fAuthOpaque.Ptr;      
        }

        virtual void SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen);

        //Timeouts. This allows clients to refresh the timeout on this session
        void    RefreshTimeout()        { fTimeoutTask.RefreshTimeout(); }
        void    RefreshRTSPTimeout()    { if (fRTSPSession != NULL) fRTSPSession->RefreshTimeout(); }
        void    RefreshTimeouts()       { RefreshTimeout(); RefreshRTSPTimeout();}
        
        // ACCESSORS
        
        Bool16  IsFirstPlay()           { return fIsFirstPlay; }
        SInt64  GetFirstPlayTime()      { return fFirstPlayTime; }
        //Time (msec) most recent play was issued
        SInt64  GetPlayTime()           { return fPlayTime; }
        SInt64  GetNTPPlayTime()        { return fNTPPlayTime; }
		/* used in RTPStatsUpdaterTask::GetNewestSession() */
        SInt64  GetSessionCreateTime()  { return fSessionCreateTime; }
        //Time (msec) most recent play, adjusted for start time of the movie
        //ex: PlayTime() == 20,000. Client said start 10 sec into the movie,
        //so AdjustedPlayTime() == 10,000

        QTSS_PlayFlags GetPlayFlags()   { return fPlayFlags; } /* need by RTPSession::run() */
        OSMutex*        GetSessionMutex()   { return &fSessionMutex; }
        UInt32          GetPacketsSent()    { return fPacketsSent; }
        UInt32          GetBytesSent()  { return fBytesSent; }

		/* �õ�Hash Table��Ԫ(Ҳ����RTPSession) */
        OSRef*      GetRef()            { return &fRTPMapElem; }
		/* �õ�RTSP SessionInterface */
        RTSPSessionInterface* GetRTSPSession() { return fRTSPSession; }

        UInt32      GetMovieAvgBitrate(){ return fMovieAverageBitRate; }/* needed by RTPSession::Play() */
        QTSS_CliSesTeardownReason GetTeardownReason() { return fTeardownReason; }
        QTSS_RTPSessionState    GetSessionState() { return fState; }
        void    SetUniqueID(UInt32 theID)   {fUniqueID = theID;}
        UInt32  GetUniqueID()           { return fUniqueID; }


        RTPBandwidthTracker* GetBandwidthTracker() { return &fTracker; } /* needed by RTPSession::run() */
        RTPOverbufferWindow* GetOverbufferWindow() { return &fOverbufferWindow; }
        UInt32  GetFramesSkipped() { return fFramesSkipped; }
        
        // MEMORY FOR RTCP PACKETS
        
        // Class for easily building a standard RTCP SR
        RTCPSRPacket*   GetSRPacket()       { return &fRTCPSRPacket; }//ע������������RTPStream::SendRTCPSR()��

        // Memory if you want to build your own
        char*           GetSRBuffer(UInt32 inSRLen);
        
        // STATISTICS UPDATING
        
        //The session tracks the total number of bytes sent during the session.
        //Streams can update that value by calling this function
		/* ���¸�RTPSession���͵��ֽ����� */
        void            UpdateBytesSent(UInt32 bytesSent) { fBytesSent += bytesSent; }
                        
        //The session tracks the total number of packets sent during the session.
        //Streams can update that value by calling this function 
		/* ���¸�RTPSession���͵�RTP Packet���� */
        void            UpdatePacketsSent(UInt32 packetsSent) { fPacketsSent += packetsSent; }
                                        
        void            UpdateCurrentBitRate(const SInt64& curTime)
                         { if (curTime > (fLastBitRateUpdateTime + 10000)) this->UpdateBitRateInternal(curTime); }

		/* �����������all track�Ƿ�interleaved? */
        void            SetAllTracksInterleaved(Bool16 newValue) { fAllTracksInterleaved = newValue; }      
       
        // RTSP RESPONSE
        // This function appends a session header to the SETUP response, and
        // checks to see if it is a 304 Not Modified. If it is, it sends the entire
        // response and returns an error
        QTSS_Error DoSessionSetupResponse(RTSPRequestInterface* inRequest);
        
        // RTSP SESSION                    
        // This object has a current RTSP session. This may change over the
        // life of the RTSPSession, so update it. It keeps an RTSP session in
        // case interleaved data or commands need to be sent back to the client. 
		/* ����RTSPSession */
        void            UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession);
                
        // let's RTSP Session pass along it's query string
		/* ���ò�ѯ�ַ��� */
        void            SetQueryString( StrPtrLen* queryString );
        
        // SETTERS and ACCESSORS for auth information
        // Authentication information that needs to be kept around
        // for the duration of the session      
        QTSS_AuthScheme GetAuthScheme() { return fAuthScheme; }
        StrPtrLen*      GetAuthNonce() { return &fAuthNonce; }
        UInt32          GetAuthQop() { return fAuthQop; }
        UInt32          GetAuthNonceCount() { return fAuthNonceCount; }
        StrPtrLen*      GetAuthOpaque() { return &fAuthOpaque; }
        void            SetAuthScheme(QTSS_AuthScheme scheme) { fAuthScheme = scheme; }
        // Use this if the auth scheme or the qop has to be changed from the defaults 
        // of scheme = Digest, and qop = auth
        void            SetChallengeParams(QTSS_AuthScheme scheme, UInt32 qop, Bool16 newNonce, Bool16 createOpaque);
        // Use this otherwise...if newNonce == true, it will create a new nonce
        // and reset nonce count. If newNonce == false but nonce was never created earlier
        // a nonce will be created. If newNonce == false, and there is an existing nonce,
        // the nounce count will be incremented.
        void            UpdateDigestAuthChallengeParams(Bool16 newNonce, Bool16 createOpaque, UInt32 qop);
    
        Float32*        GetPacketLossPercent() { UInt32 outLen;return  (Float32*) this->PacketLossPercent(this, &outLen);}

        SInt32          GetQualityLevel()   { return fSessionQualityLevel; }
        SInt32*         GetQualityLevelPtr()    { return &fSessionQualityLevel; }
        void            SetQualityLevel(SInt32 level)   { if (fSessionQualityLevel == 0 && level != 0)
                                                                QTSServerInterface::GetServer()->IncrementNumThinned(1);
                                                            else if (fSessionQualityLevel != 0 && level == 0)
                                                                QTSServerInterface::GetServer()->IncrementNumThinned(-1);
                                                            fSessionQualityLevel = level; 
                                                         }

		//the server's thinning algorithm(quality level),�μ�RTPStream::UpdateQualityLevel()
        SInt64          fLastQualityCheckTime;          //�ϴ�quality level���ʱ��
		SInt64			fLastQualityCheckMediaTime;     //�ϴη��͵�RTP�趨�ķ��;���ʱ���
		Bool16			fStartedThinning;               //�Ƿ�ʼ�ݻ�?
		
        // Used by RTPStream to increment the RTCP packet and byte counts.
		/* ����RTCP�� */
        void            IncrTotalRTCPPacketsRecv()         { fTotalRTCPPacketsRecv++; }
        UInt32          GetTotalRTCPPacketsRecv()          { return fTotalRTCPPacketsRecv; }
        void            IncrTotalRTCPBytesRecv(UInt16 cnt) { fTotalRTCPBytesRecv += cnt; }
        UInt32          GetTotalRTCPBytesRecv()            { return fTotalRTCPBytesRecv; }

    protected:
    
        // These variables are setup by the derived RTPSession object when Play and Pause get called 
        //Some stream related information that is shared amongst all streams,�μ�RTPSession::Play()
        Bool16      fIsFirstPlay; /* �ǵ�һ�β�����? */
        SInt64      fFirstPlayTime;//in milliseconds,��һ�β��ŵ�ʱ��
        SInt64      fPlayTime;//RTP���ĵ�ǰ����ʱ���,�μ�RTPStream::UpdateQualityLevel()
        SInt64      fAdjustedPlayTime;/* ��ȡ��ǰ����ʱ�����RTSP Request Header Range�е�fStartTime�Ĳ�ֵ,������ǳ���Ҫ */
        SInt64      fNTPPlayTime;/* Ϊ����SR��,��fPlayTime���NTP��ʽ��ֵ */
		Bool16      fAllTracksInterleaved;/* all stream ͨ��RTSP channel���淢����? */

		/* very important! this is actual absolute timestamp of send the next RTP packets */
        SInt64      fNextSendPacketsTime;/* need by RTPSession::run()/RTPSession::Play() */
        
		/* RTPSession��quality level */
        SInt32      fSessionQualityLevel;
        
        //keeps track of whether we are playing or not
		/* Play or Pause,need by RTPSession::run() */
        QTSS_RTPSessionState fState;
        
        // have the server generate RTCP Sender Reports or have the server append the server info APP packet to your RTCP Sender Reports
		/* see QTSS.h */
        QTSS_PlayFlags  fPlayFlags;
        
        //Session mutex. This mutex should be grabbed before invoking the module
        //responsible for managing this session. This allows the module to be
        //non-preemptive-safe with respect to a session
        OSMutex     fSessionMutex; /* �Ự������,used in RTPSession::run() */

        //Stores the RTPsession ID
		/* RTPSession��ΪRTPSessionMap��HashTable��Ԫ */
        OSRef       fRTPMapElem;
		/* RTSPSession��IDֵ,�Ǹ�Ψһ�������,����"58147963412401" */
        char        fRTSPSessionIDBuf[QTSS_MAX_SESSION_ID_LENGTH + 4];
        
		/* �Ե�ǰ�������ͳ����ֽ��� */
        UInt32      fLastBitRateBytes;
		/* ��ǰ�����ʵĸ���ʱ�� */
        SInt64      fLastBitRateUpdateTime;
		/* movie�ĵ�ǰ������,������μ����? �μ�RTPSessionInterface::UpdateBitRateInternal() */
        UInt32      fMovieCurrentBitRate;
        
        // In order to facilitate sending data over the RTSP channel from
        // an RTP session, we store a pointer to the RTSP session used in
        // the last RTSP request.
		/* needed by RTPSession::Play(),��RTSP�Ự��ص�Ŧ�� */
		/* Ϊ�˱�����RTSP channel����Interleaved����,����һ��RTSPRequest�д��һ��RTSPSession��ָ��,������ǳ���Ҫ! */
        RTSPSessionInterface* fRTSPSession;

    private:
    
        // Utility function for calculating current bit rate
        void UpdateBitRateInternal(const SInt64& curTime);

		/* ������������������setup ClientSessionDictionary Attr */
        static void* CurrentBitRate(QTSSDictionary* inSession, UInt32* outLen);
        static void* PacketLossPercent(QTSSDictionary* inSession, UInt32* outLen);
        static void* TimeConnected(QTSSDictionary* inSession, UInt32* outLen);
         
        // Create nonce
		/* ����ժҪ��֤�������,Nonceָ������� */
        void CreateDigestAuthenticationNonce();

        // One of the RTP session attributes is an iterated list of all streams.
        // As an optimization, we preallocate a "large" buffer of stream pointers,
        // even though we don't know how many streams we need at first.
		/* ��Щ��������Ҫ�õ� */
        enum
        {
            kStreamBufSize              = 4,
            kUserAgentBufSize           = 256,
            kPresentationURLSize        = 256,
            kFullRequestURLBufferSize   = 256,
            kRequestHostNameBufferSize  = 80,
            
            kIPAddrStrBufSize           = 20,
            kLocalDNSBufSize            = 80,
            
            kAuthNonceBufSize           = 32,
            kAuthOpaqueBufSize          = 32,       
        };
        
        void*       fStreamBuffer[kStreamBufSize];

        
        // theses are dictionary items picked up by the RTSPSession
        // but we need to store copies of them for logging purposes.
		/* ��RTSPSession��ȡ������ֵ�����,Ϊ����־��¼,��Ҫ����,�μ�RTSPSession::SetupClientSessionAttrs() */
        char        fUserAgentBuffer[kUserAgentBufSize]; //eg VLC media player (LIVE555 Streaming Media v2007.02.20)
        char        fPresentationURL[kPresentationURLSize];         // eg /foo/bar.mov,/sample_300kbit.mp4
        char        fFullRequestURL[kFullRequestURLBufferSize];     // eg rtsp://172.16.34.22/sample_300kbit.mp4
        char        fRequestHostName[kRequestHostNameBufferSize];   // eg 172.16.34.22
    
        char        fRTSPSessRemoteAddrStr[kIPAddrStrBufSize];
        char        fRTSPSessLocalDNS[kLocalDNSBufSize];
        char        fRTSPSessLocalAddrStr[kIPAddrStrBufSize];
        
        char        fUserNameBuf[RTSPSessionInterface::kMaxUserNameLen];
        char        fUserPasswordBuf[RTSPSessionInterface::kMaxUserPasswordLen];
        char        fUserRealmBuf[RTSPSessionInterface::kMaxUserRealmLen];
        UInt32      fLastRTSPReqRealStatusCode;

        //for timing out this session
        TimeoutTask fTimeoutTask; /* ʹ�Ự��ʱ��TimeoutTask�� */	
        UInt32      fTimeout;/* ��ʱʱ��,Ĭ��1s */

		UInt32      fUniqueID;/* RTPSession ID */
        
        // Time when this session got created
		/* ����RTPSession��ʱ�� */
        SInt64      fSessionCreateTime;

        //Packet priority levels. Each stream has a current level, and
        //the module that owns this session sets what the number of levels is.
		/* ��ǰRTPSession��Packet���ȼ�,���Module�������� */
        UInt32      fNumQualityLevels;
        
        //Statistics
        UInt32 fBytesSent;/* ���͵��ֽ��� */
        UInt32 fPacketsSent; /* ���͵İ��� */   
        Float32 fPacketLossPercent; /* �����ٷֱ� */
        SInt64 fTimeConnected; /* ����ʱ�� */
        UInt32 fTotalRTCPPacketsRecv; /* ���յ���RTCP������ */
        UInt32 fTotalRTCPBytesRecv; /* ���յ���RTCP���ֽ����� */


        // Movie size & movie duration. It may not be so good to associate these
        // statistics with the movie, for a session MAY have multiple movies...
        // however, the 1 movie assumption is in too many subsystems at this point
		/* ��Ӱ�ĳ���ʱ�� */
        Float64     fMovieDuration;
		/* ��ǰmovie���ֽ��� */
        UInt64      fMovieSizeInBytes;
		/* movie��ƽ�������� */
        UInt32      fMovieAverageBitRate;
        
		/* RTPSession TEARDOWN��ԭ�� */
        QTSS_CliSesTeardownReason fTeardownReason;
        
        // So the streams can send sender reports(SR)
        RTCPSRPacket        fRTCPSRPacket;
        StrPtrLen           fSRBuffer;
        
		/* needed by RTPSession::run() */
		/* ׷�ٵ�ǰ������� */
        RTPBandwidthTracker fTracker;
		/* overbuffering Windows��ǰ���ͻ��� */
        RTPOverbufferWindow fOverbufferWindow;
        
        // Built in dictionary attributes
		/* ��̬���ڽ��ֵ����� */
        static QTSSAttrInfoDict::AttrInfo   sAttributes[];
        static unsigned int sRTPSessionIDCounter; /* RTPSessionID������ */

        // Authentication information that needs to be kept around
        // for the duration of the session      
        QTSS_AuthScheme             fAuthScheme; /* ʹ�õ���֤��ʽ */
        StrPtrLen                   fAuthNonce; /* ��֤���� */
        UInt32                      fAuthQop;
        UInt32                      fAuthNonceCount;                    
        StrPtrLen                   fAuthOpaque;
        UInt32                      fQualityUpdate;// fQualityUpdate is a counter, the starting value is the unique ID, so every session starts at a different position
        
        UInt32                      fFramesSkipped;
		/* �Ƿ�����overbuffering Window,�μ�RTPOverbufferWindow.h��ͬ���ֶ� */
        Bool16                      fOverBufferEnabled;
};

#endif //_RTPSESSIONINTERFACE_H_
