
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSessionInterface.cpp
Description: Provides an API interface for objects to access the attributes 
             related to a RTPSession, also implements the RTP Session Dictionary.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTSPRequestInterface.h"
#include "RTPSessionInterface.h"
#include "RTPStream.h"
#include "QTSServerInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "md5.h"
#include "md5digest.h"
#include "base64.h"



unsigned int            RTPSessionInterface::sRTPSessionIDCounter = 0;


QTSSAttrInfoDict::AttrInfo  RTPSessionInterface::sAttributes[] = 
{   /*fields:   fAttrName, fFuncPtr, fAttrDataType, fAttrPermission */
    /* 0  */ { "qtssCliSesStreamObjects",           NULL,   qtssAttrDataTypeQTSS_Object,    qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 1  */ { "qtssCliSesCreateTimeInMsec",        NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 2  */ { "qtssCliSesFirstPlayTimeInMsec",     NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 3  */ { "qtssCliSesPlayTimeInMsec",          NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 4  */ { "qtssCliSesAdjustedPlayTimeInMsec",  NULL,   qtssAttrDataTypeTimeVal,        qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 5  */ { "qtssCliSesRTPBytesSent",            NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 6  */ { "qtssCliSesRTPPacketsSent",          NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 7  */ { "qtssCliSesState",                   NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 8  */ { "qtssCliSesPresentationURL",         NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 9  */ { "qtssCliSesFirstUserAgent",          NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 10 */ { "qtssCliStrMovieDurationInSecs",     NULL,   qtssAttrDataTypeFloat64,        qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 11 */ { "qtssCliStrMovieSizeInBytes",        NULL,   qtssAttrDataTypeUInt64,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 12 */ { "qtssCliSesMovieAverageBitRate",     NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 13 */ { "qtssCliSesLastRTSPSession",         NULL,   qtssAttrDataTypeQTSS_Object,    qtssAttrModeRead | qtssAttrModePreempSafe } ,
    /* 14 */ { "qtssCliSesFullURL",                 NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe } ,
    /* 15 */ { "qtssCliSesHostName",                NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },

    /* 16 */ { "qtssCliRTSPSessRemoteAddrStr",      NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 17 */ { "qtssCliRTSPSessLocalDNS",           NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 18 */ { "qtssCliRTSPSessLocalAddrStr",       NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 19 */ { "qtssCliRTSPSesUserName",            NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 20 */ { "qtssCliRTSPSesUserPassword",        NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 21 */ { "qtssCliRTSPSesURLRealm",            NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 22 */ { "qtssCliRTSPReqRealStatusCode",      NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 23 */ { "qtssCliTeardownReason",             NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe | qtssAttrModeWrite },
    /* 24 */ { "qtssCliSesReqQueryString",          NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 25 */ { "qtssCliRTSPReqRespMsg",             NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    
    /* 26 */ { "qtssCliSesCurrentBitRate",          CurrentBitRate,     qtssAttrDataTypeUInt32,  qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 27 */ { "qtssCliSesPacketLossPercent",       PacketLossPercent,  qtssAttrDataTypeFloat32, qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 28 */ { "qtssCliSesTimeConnectedinMsec",     TimeConnected,      qtssAttrDataTypeSInt64,  qtssAttrModeRead | qtssAttrModePreempSafe },    
    /* 29 */ { "qtssCliSesCounterID",               NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 30 */ { "qtssCliSesRTSPSessionID",           NULL,   qtssAttrDataTypeCharArray,      qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 31 */ { "qtssCliSesFramesSkipped",           NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 32 */ { "qtssCliSesTimeoutMsec", 			NULL, 	qtssAttrDataTypeUInt32,		qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
	/* 33 */ { "qtssCliSesOverBufferEnabled",       NULL, 	qtssAttrDataTypeBool16,		qtssAttrModeRead | qtssAttrModeWrite | qtssAttrModePreempSafe },
    /* 34 */ { "qtssCliSesRTCPPacketsRecv",         NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
    /* 35 */ { "qtssCliSesRTCPBytesRecv",           NULL,   qtssAttrDataTypeUInt32,         qtssAttrModeRead | qtssAttrModePreempSafe },
	/* 36 */ { "qtssCliSesStartedThinning",         NULL, 	qtssAttrDataTypeBool16,		qtssAttrModeRead | qtssAttrModeWrite  | qtssAttrModePreempSafe }
    
};

/* ����Client Session Dictionaryָ����QTSS_ClientSessionAttributes,see QTSS.h */
void    RTPSessionInterface::Initialize()
{
    for (UInt32 x = 0; x < qtssCliSesNumParams; x++)
        QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex)->
            SetAttribute(x, sAttributes[x].fAttrName, sAttributes[x].fFuncPtr,
                sAttributes[x].fAttrDataType, sAttributes[x].fAttrPermission);
}

RTPSessionInterface::RTPSessionInterface()
:   QTSSDictionary(QTSSDictionaryMap::GetMap(QTSSDictionaryMap::kClientSessionDictIndex), NULL),
    Task(),
    fLastQualityCheckTime(0),
	fLastQualityCheckMediaTime(0),
	fStartedThinning(false),    
    fIsFirstPlay(true),/* Ĭ���״β��� */
    fAllTracksInterleaved(true), // assume true until proven false! Ĭ��ʹ��RTSP TCP channel����RTP/RTCP data
    fFirstPlayTime(0),
    fPlayTime(0),
    fAdjustedPlayTime(0),/* ���ü�RTPSession::Play() */
    fNTPPlayTime(0),
    fNextSendPacketsTime(0),
    fSessionQualityLevel(0),
    fState(qtssPausedState),/* Ĭ����ͣ״̬ */
    fPlayFlags(0),
    fLastBitRateBytes(0),
    fLastBitRateUpdateTime(0),
    fMovieCurrentBitRate(0),
    fRTSPSession(NULL),
    fLastRTSPReqRealStatusCode(200),/* Ĭ�� 200 OK */
    fTimeoutTask(NULL, QTSServerInterface::GetServer()->GetPrefs()->GetRTPTimeoutInSecs() * 1000),/* ʹ��Ԥ�賬ʱʱ�� */
    fNumQualityLevels(0),
    fBytesSent(0),
    fPacketsSent(0),
    fPacketLossPercent(0.0),
    fTimeConnected(0),
    fTotalRTCPPacketsRecv(0),    
    fTotalRTCPBytesRecv(0),    
    fMovieDuration(0),
    fMovieSizeInBytes(0),
    fMovieAverageBitRate(0),
    fTeardownReason(0),
    fUniqueID(0),
    fTracker(QTSServerInterface::GetServer()->GetPrefs()->IsSlowStartEnabled()),/* ʹ��Ԥ�������� */
	fOverbufferWindow(QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec(),kUInt32_Max, QTSServerInterface::GetServer()->GetPrefs()->GetMaxSendAheadTimeInSecs(),QTSServerInterface::GetServer()->GetPrefs()->GetOverbufferRate()),/* ��ʼ��OverbufferWindow����� */
    fAuthScheme(QTSServerInterface::GetServer()->GetPrefs()->GetAuthScheme()),/* ʹ��Ԥ����֤��ʽ */
    fAuthQop(RTSPSessionInterface::kNoQop),
    fAuthNonceCount(0),
    fFramesSkipped(0)
{
    //don't actually setup the fTimeoutTask until the session has been bound!
    //(we don't want to get timeouts before the session gets bound)

    fTimeoutTask.SetTask(this);/* ���õ�ǰRTPSessionInterfaceΪ��ʱ���� */
    fTimeout = QTSServerInterface::GetServer()->GetPrefs()->GetRTPTimeoutInSecs() * 1000;/* ʹ��Ԥ��ĳ�ʱʱ�� */
    fUniqueID = (UInt32)atomic_add(&sRTPSessionIDCounter, 1);/* ��ȡΨһ�ĵ�ǰRTPSession ID */
    
    // fQualityUpdate is a counter, the starting value is the unique ID, so every session starts at a different position
    fQualityUpdate = fUniqueID;
    
    //mark the session create time
    fSessionCreateTime = OS::Milliseconds();/* ��¼RTPSession���ɵ�ʱ�� */

    // Setup all dictionary attribute values
    
    // Make sure the dictionary knows about our preallocated memory for the RTP stream array
	//ע��Ԥ�ȷ�����ڴ�
    this->SetEmptyVal(qtssCliSesFirstUserAgent, &fUserAgentBuffer[0], kUserAgentBufSize);
    this->SetEmptyVal(qtssCliSesStreamObjects, &fStreamBuffer[0], kStreamBufSize);
    this->SetEmptyVal(qtssCliSesPresentationURL, &fPresentationURL[0], kPresentationURLSize);
    this->SetEmptyVal(qtssCliSesFullURL, &fFullRequestURL[0], kRequestHostNameBufferSize);
    this->SetEmptyVal(qtssCliSesHostName, &fRequestHostName[0], kFullRequestURLBufferSize);

    this->SetVal(qtssCliSesCreateTimeInMsec,    &fSessionCreateTime, sizeof(fSessionCreateTime));
    this->SetVal(qtssCliSesFirstPlayTimeInMsec, &fFirstPlayTime, sizeof(fFirstPlayTime));
    this->SetVal(qtssCliSesPlayTimeInMsec,      &fPlayTime, sizeof(fPlayTime));
    this->SetVal(qtssCliSesAdjustedPlayTimeInMsec, &fAdjustedPlayTime, sizeof(fAdjustedPlayTime));
    this->SetVal(qtssCliSesRTPBytesSent,        &fBytesSent, sizeof(fBytesSent));
    this->SetVal(qtssCliSesRTPPacketsSent,      &fPacketsSent, sizeof(fPacketsSent));
    this->SetVal(qtssCliSesState,               &fState, sizeof(fState));
    this->SetVal(qtssCliSesMovieDurationInSecs, &fMovieDuration, sizeof(fMovieDuration));
    this->SetVal(qtssCliSesMovieSizeInBytes,    &fMovieSizeInBytes, sizeof(fMovieSizeInBytes));
    this->SetVal(qtssCliSesLastRTSPSession,     &fRTSPSession, sizeof(fRTSPSession));
    this->SetVal(qtssCliSesMovieAverageBitRate, &fMovieAverageBitRate, sizeof(fMovieAverageBitRate));
    this->SetEmptyVal(qtssCliRTSPSessRemoteAddrStr, &fRTSPSessRemoteAddrStr[0], kIPAddrStrBufSize );
    this->SetEmptyVal(qtssCliRTSPSessLocalDNS, &fRTSPSessLocalDNS[0], kLocalDNSBufSize);
    this->SetEmptyVal(qtssCliRTSPSessLocalAddrStr, &fRTSPSessLocalAddrStr[0], kIPAddrStrBufSize);

    this->SetEmptyVal(qtssCliRTSPSesUserName, &fUserNameBuf[0],RTSPSessionInterface::kMaxUserNameLen);
    this->SetEmptyVal(qtssCliRTSPSesUserPassword, &fUserPasswordBuf[0], RTSPSessionInterface::kMaxUserPasswordLen);
    this->SetEmptyVal(qtssCliRTSPSesURLRealm, &fUserRealmBuf[0], RTSPSessionInterface::kMaxUserRealmLen);

    this->SetVal(qtssCliRTSPReqRealStatusCode, &fLastRTSPReqRealStatusCode, sizeof(fLastRTSPReqRealStatusCode));

    this->SetVal(qtssCliTeardownReason, &fTeardownReason, sizeof(fTeardownReason));
 //   this->SetVal(qtssCliSesCurrentBitRate, &fMovieCurrentBitRate, sizeof(fMovieCurrentBitRate));
    this->SetVal(qtssCliSesCounterID, &fUniqueID, sizeof(fUniqueID));
    this->SetEmptyVal(qtssCliSesRTSPSessionID, &fRTSPSessionIDBuf[0], QTSS_MAX_SESSION_ID_LENGTH + 4);
    this->SetVal(qtssCliSesFramesSkipped, &fFramesSkipped, sizeof(fFramesSkipped));
    this->SetVal(qtssCliSesRTCPPacketsRecv, &fTotalRTCPPacketsRecv, sizeof(fTotalRTCPPacketsRecv));
    this->SetVal(qtssCliSesRTCPBytesRecv, &fTotalRTCPBytesRecv, sizeof(fTotalRTCPBytesRecv));

	this->SetVal(qtssCliSesTimeoutMsec,	&fTimeout, sizeof(fTimeout));
	
	this->SetVal(qtssCliSesOverBufferEnabled, this->GetOverbufferWindow()->OverbufferingEnabledPtr(), sizeof(Bool16));
	this->SetVal(qtssCliSesStartedThinning, &fStartedThinning, sizeof(Bool16));
	
}

void RTPSessionInterface::SetValueComplete(UInt32 inAttrIndex, QTSSDictionaryMap* inMap,
							UInt32 inValueIndex, void* inNewValue, UInt32 inNewValueLen)
{
	if (inAttrIndex == qtssCliSesTimeoutMsec)
	{
		Assert(inNewValueLen == sizeof(UInt32));
		UInt32 newTimeOut = *((UInt32 *) inNewValue);
		fTimeoutTask.SetTimeout((SInt64) newTimeOut);/* ����������ó�ʱ����ĳ�ʱֵ */
	}
}

/* ʹ�ɵ�RTSPSession�Ķ�����м�����1,����fRTSPSession������1��������м��� */
void RTPSessionInterface::UpdateRTSPSession(RTSPSessionInterface* inNewRTSPSession)
{   
    if (inNewRTSPSession != fRTSPSession)
    {
        // If there was an old session, let it know that we are done
        if (fRTSPSession != NULL)
            fRTSPSession->DecrementObjectHolderCount();
        
        // Increment this count to prevent the RTSP session from being deleted
        fRTSPSession = inNewRTSPSession;
        fRTSPSession->IncrementObjectHolderCount();
    }
}

/* ��ָ���ĳ��ȴ���SR���Ļ���,����ָ����С2����SR���Ļ���,���ظû������ʼ��ַ */
char* RTPSessionInterface::GetSRBuffer(UInt32 inSRLen)
{
    if (fSRBuffer.Len < inSRLen)
    {
        delete [] fSRBuffer.Ptr;
        fSRBuffer.Set(NEW char[2*inSRLen], 2*inSRLen);
    }
    return fSRBuffer.Ptr;
}

/* �ӷ�����Ԥ��ֵ��ȡRTSP timeoutֵ,����Ϊ0,���һ��"Session:664885455621367225;timeout=20\r\n"��SETUP response��ȥ;
   ��Ϊ0,���һ��"Session:664885455621367225\r\n".
   �Ӷ�,������RTSPResponseStream������������:
   (1)���г�ʱֵʱ:
   RTSP/1.0 200 OK\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623;timeout=20\r\n
   \r\n
   (2)���޳�ʱֵʱ:
   RTSP/1.0 200 OK\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623\r\n
   \r\n
   ������"RTSP/1.0 304 Not Modified",��RTSPResponseStream�з���ָ����ʽ��RTSPHeader��Ϣ,�����ش���
   RTSP/1.0 304 Not Modified\r\n
   Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
   Cseq: 12\r\n
   Session: 1900075377083826623;timeout=20\r\n
   \r\n
*/
QTSS_Error RTPSessionInterface::DoSessionSetupResponse(RTSPRequestInterface* inRequest)
{
    // This function appends a session header to the SETUP response, and
    // checks to see if it is a 304 Not Modified. If it is, it sends the entire
    // response and returns an error
	/* �ӷ�����Ԥ��ֵ��ȡRTSP timeoutֵ,����Ϊ0,���һ��"Session:664885455621367225;timeout=20\r\n"��SETUP response��ȥ;
	   ��Ϊ0,���һ��"Session:664885455621367225\r\n"
	*/
    if ( QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutInSecs() > 0 )  // adv the timeout
        inRequest->AppendSessionHeaderWithTimeout( this->GetValue(qtssCliSesRTSPSessionID), QTSServerInterface::GetServer()->GetPrefs()->GetRTSPTimeoutAsString() );
    else
        inRequest->AppendSessionHeaderWithTimeout( this->GetValue(qtssCliSesRTSPSessionID), NULL ); // no timeout in resp.
    
	/* ������"RTSP/1.0 304 Not Modified",��RTSPResponseStream�з���ָ����ʽ��RTSPHeader��Ϣ,�����ش��� */
    if (inRequest->GetStatus() == qtssRedirectNotModified)
    {
        (void)inRequest->SendHeader();
        return QTSS_RequestFailed;
    }
    return QTSS_NoErr;
}

/* ����θ���ʱ��,����movieƽ�������� */
void RTPSessionInterface::UpdateBitRateInternal(const SInt64& curTime)
{   
	/* ��������ͣ״̬,����¼�����ʸ���ʱ�� */
    if (fState == qtssPausedState)
    {   
		/* �����ʵ�Ȼ��0 */
		fMovieCurrentBitRate = 0;
	     /* ���µ�ǰ�����ʵ�ʱ�� */
         fLastBitRateUpdateTime = curTime;
		 /* ��ǰ�������ͳ����ֽ���,��ԭ��һ�� */
         fLastBitRateBytes = fBytesSent;
    }
    else
    {
		/* ��ǰ���ʱ������ͳ��ı����� */
        UInt32 bitsInInterval = (fBytesSent - fLastBitRateBytes) * 8;
		/* ����ʱ��(ms) */
        SInt64 updateTime = (curTime - fLastBitRateUpdateTime) / 1000;
		/****************** ���㵱ǰ�����ʵĹ�ʽ ***************************************************/
        if (updateTime > 0) // leave Bit Rate the same if updateTime is 0 also don't divide by 0.
            fMovieCurrentBitRate = (UInt32) (bitsInInterval / updateTime);
		/****************** ���㵱ǰ�����ʵĹ�ʽ ***************************************************/

		/* ����Ack��������ʱ��(20-100ms��) */
        fTracker.UpdateAckTimeout(bitsInInterval, curTime - fLastBitRateUpdateTime);
		/* ??��ʱû���ֽ��ͳ�,��ԭ��һ��,���Ǹ��±����� */
        fLastBitRateBytes = fBytesSent;
        fLastBitRateUpdateTime = curTime;
    }
    qtss_printf("fMovieCurrentBitRate=%lu\n",fMovieCurrentBitRate);
    qtss_printf("Cur bandwidth: %d. Cur ack timeout: %d.\n",fTracker.GetCurrentBandwidthInBps(), fTracker.RecommendedClientAckTimeout());
}

/* �����RTPSessionInterface�Դ��������ڵ�����ʱ��,������ */
void* RTPSessionInterface::TimeConnected(QTSSDictionary* inSession, UInt32* outLen)
{
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
	/* �����RTPSessionInterface�Դ��������ڵ�����ʱ�� */
    theSession->fTimeConnected = (OS::Milliseconds() - theSession->GetSessionCreateTime());

    // Return the result
    *outLen = sizeof(theSession->fTimeConnected);
    return &theSession->fTimeConnected;
}

/* ���㵱ǰʱ���movieƽ��������,������ */
void* RTPSessionInterface::CurrentBitRate(QTSSDictionary* inSession, UInt32* outLen)
{
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
	/* ���㵱ǰʱ���movieƽ�������� */
    theSession->UpdateBitRateInternal(OS::Milliseconds());
    
    // Return the result
    *outLen = sizeof(theSession->fMovieCurrentBitRate);
    return &theSession->fMovieCurrentBitRate;
}

/* ������RTPSession��ÿ��RTPStream,����һ��RTCP Interval�ڵ�ǰ�Ķ��������ܰ���,���㲢���ض����ٷֱ� */
void* RTPSessionInterface::PacketLossPercent(QTSSDictionary* inSession, UInt32* outLen)
{   
    RTPSessionInterface* theSession = (RTPSessionInterface*)inSession;
    RTPStream* theStream = NULL;
    UInt32 theLen = sizeof(theStream);
            
    SInt64 packetsLost = 0;
    SInt64 packetsSent = 0;
    
	/* ������RTPSession��ÿ��RTPStream,����һ��RTCP Interval�ڵ�ǰ�Ķ��������ܰ��� */
    for (int x = 0; theSession->GetValue(qtssCliSesStreamObjects, x, (void*)&theStream, &theLen) == QTSS_NoErr; x++)
    {       
        if (theStream != NULL  )
        {
            UInt32 streamCurPacketsLost = 0;
            theLen = sizeof(UInt32);
            (void) theStream->GetValue(qtssRTPStrCurPacketsLostInRTCPInterval,0, &streamCurPacketsLost, &theLen);
            qtss_printf("stream = %d streamCurPacketsLost = %lu \n",x, streamCurPacketsLost);
            
            UInt32 streamCurPackets = 0;
            theLen = sizeof(UInt32);
            (void) theStream->GetValue(qtssRTPStrPacketCountInRTCPInterval,0, &streamCurPackets, &theLen);
            qtss_printf("stream = %d streamCurPackets = %lu \n",x, streamCurPackets);
                
            packetsSent += (SInt64)  streamCurPackets;
            packetsLost += (SInt64) streamCurPacketsLost;
            qtss_printf("stream calculated loss = %f \n",x, (Float32) streamCurPacketsLost / (Float32) streamCurPackets);
            
        }

        theStream = NULL;
        theLen = sizeof(UInt32);
    }
    
    //Assert(packetsLost <= packetsSent);
	/* ���㶪���ٷֱ� */
    if (packetsSent > 0)
    {   if  (packetsLost <= packetsSent)
            theSession->fPacketLossPercent =(Float32) (( ((Float32) packetsLost / (Float32) packetsSent) * 100.0) );
        else
            theSession->fPacketLossPercent = 100.0;
    }
    else
        theSession->fPacketLossPercent = 0.0;
    qtss_printf("Session loss percent packetsLost = %qd packetsSent= %qd theSession->fPacketLossPercent=%f\n",packetsLost,packetsSent,theSession->fPacketLossPercent);
    
	// Return the result
    *outLen = sizeof(theSession->fPacketLossPercent);

    return &theSession->fPacketLossPercent;
}

/* ��RTSPsessionid:timestamp���ɵ���֤���и�fAuthNonce��ֵ */
void RTPSessionInterface::CreateDigestAuthenticationNonce() 
{

    // Calculate nonce: MD5 of sessionid:timestamp
	/* �õ�ʱ����ַ��� */
    SInt64 curTime = OS::Milliseconds();
    char* curTimeStr = NEW char[128];
    qtss_sprintf(curTimeStr, "%"_64BITARG_"d", curTime);
    
    // Delete old nonce before creating a new one
	/* ɾȥ�ɵ���֤���� */
    if(fAuthNonce.Ptr != NULL)
        delete [] fAuthNonce.Ptr;
        
    MD5_CTX ctxt;
    unsigned char nonceStr[16];
    unsigned char colon[] = ":";
    MD5_Init(&ctxt);
	/* �õ�������ص�RTSPSession ID */
    StrPtrLen* sesID = this->GetValue(qtssCliSesRTSPSessionID);

	/* ����ctxt����ΪRTSPsessionid:timestamp */
    MD5_Update(&ctxt, (unsigned char *)sesID->Ptr, sesID->Len);
    MD5_Update(&ctxt, (unsigned char *)colon, 1);
    MD5_Update(&ctxt, (unsigned char *)curTimeStr, ::strlen(curTimeStr));

	/* ��ctxt������֤�ַ��� */
    MD5_Final(nonceStr, &ctxt);
	/* �����ɵ���֤��ֵ��fAuthNonce��ֵ */
    HashToString(nonceStr, &fAuthNonce);

    delete [] curTimeStr; // No longer required once nonce is created
        
    // Set the nonce count value to zero 
    // as a new nonce has been created  
    fAuthNonceCount = 0;

}

/* �����������֤��ʽ,������Digest��֤��ʽ:����û���µ���֤����,��������;����Ҫ����fAuthOpaque,���ɵ�ǰʱ����������������
�ַ������䳤�Ȼ���64bit�����õ�;����,ɾ���Ѵ��ڵ�fAuthOpaque,ʹNonce Count��1  */
void RTPSessionInterface::SetChallengeParams(QTSS_AuthScheme scheme, UInt32 qop, Bool16 newNonce, Bool16 createOpaque)
{   
    // Set challenge params 
    // Set authentication scheme
    fAuthScheme = scheme; /* �����������֤��ʽ */  
    
	/* ������Digest��֤��ʽ */
    if(fAuthScheme == qtssAuthDigest) 
	{
        // Set Quality of Protection 
        // auth-int (Authentication with integrity) not supported yet
        fAuthQop = qop;/* �����������������Qop */
    
		/* ����û���µ���֤����,�������� */
        if(newNonce || (fAuthNonce.Ptr == NULL))
            this->CreateDigestAuthenticationNonce();
    
		/* ����Ҫ����fAuthOpaque,���ɵ�ǰʱ�����������������ַ������䳤�Ȼ���64bit�����õ�;����,ɾ���Ѵ��ڵ�fAuthOpaque */
        if(createOpaque) 
		{
            // Generate a random UInt32 and convert it to a string 
            // The base64 encoded form of the string is made the opaque value
			/* �ɵ�ǰʱ������������ */
            SInt64 theMicroseconds = OS::Microseconds();
            ::srand((unsigned int)theMicroseconds);
            UInt32 randomNum = ::rand();
			/* �������������������ַ���,���䳤��64bit���� */
            char* randomNumStr = NEW char[128];
            qtss_sprintf(randomNumStr, "%lu", randomNum);
            int len = ::strlen(randomNumStr);
            fAuthOpaque.Len = Base64encode_len(len);
            char *opaqueStr = NEW char[fAuthOpaque.Len];
            (void) Base64encode(opaqueStr, randomNumStr,len);
			/* ��ɾ���Ѵ��ڵ�fAuthOpaque,������������������ַ���64bit����,��ΪfAuthOpaque */
            delete [] randomNumStr;                 // Don't need this anymore
            if(fAuthOpaque.Ptr != NULL)             // Delete existing pointer before assigning new one
                delete [] fAuthOpaque.Ptr;              
            fAuthOpaque.Ptr = opaqueStr;
        }
        else 
		{
            if(fAuthOpaque.Ptr != NULL) 
                delete [] fAuthOpaque.Ptr;
            fAuthOpaque.Len = 0;    
        }
        // Increase the Nonce Count by one
        // This number is a count of the next request the server
        // expects with this nonce. (Implies that the server
        // has already received nonce count - 1 requests that 
        // sent authorization with this nonce
		/* ʹNonce Count��1 */
        fAuthNonceCount ++;
    }
}

/* ����Ҫ����fAuthOpaque,���ɵ�ǰʱ�����������������ַ������䳤�Ȼ���64bit�����õ�;����,ɾ���Ѵ��ڵ�fAuthOpaque */
void RTPSessionInterface::UpdateDigestAuthChallengeParams(Bool16 newNonce, Bool16 createOpaque, UInt32 qop) 
{
	/* ����û���µ���֤����,�������� */
    if(newNonce || (fAuthNonce.Ptr == NULL))
        this->CreateDigestAuthenticationNonce();
    
            
    if(createOpaque) 
	{
        // Generate a random UInt32 and convert it to a string 
        // The base64 encoded form of the string is made the opaque value
        SInt64 theMicroseconds = OS::Microseconds();
        ::srand((unsigned int)theMicroseconds);
        UInt32 randomNum = ::rand();
        char* randomNumStr = NEW char[128];
        qtss_sprintf(randomNumStr, "%lu", randomNum);
        int len = ::strlen(randomNumStr);
        fAuthOpaque.Len = Base64encode_len(len);
        char *opaqueStr = NEW char[fAuthOpaque.Len];
        (void) Base64encode(opaqueStr, randomNumStr,len);
        delete [] randomNumStr;                 // Don't need this anymore
        if(fAuthOpaque.Ptr != NULL)             // Delete existing pointer before assigning new
            delete [] fAuthOpaque.Ptr;              // one
        fAuthOpaque.Ptr = opaqueStr;
        fAuthOpaque.Len = ::strlen(opaqueStr);
    }
    else 
	{
        if(fAuthOpaque.Ptr != NULL) 
            delete [] fAuthOpaque.Ptr;
        fAuthOpaque.Len = 0;    
    }

	/* ʹNonce Count��1 */
    fAuthNonceCount ++;
    /* ʹ���������fAuthQop */
    fAuthQop = qop;
}
