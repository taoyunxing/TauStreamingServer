/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPSession.cpp
Description: Provides a class to manipulate transmission of the media data, 
             also receive and respond to client's feedback.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#include "RTPSession.h"
#include "RTSPProtocol.h" 

#include "QTSServerInterface.h"
#include "QTSS.h"
#include "OS.h"
#include "OSMemory.h"
#include <errno.h>

#define RTPSESSION_DEBUGGING 1



RTPSession::RTPSession() :
    RTPSessionInterface(), /* invocation related interface */
    fModule(NULL),/* Ĭ��Moduleָ��Ϊ�� */
    fHasAnRTPStream(false),/* Ĭ��û��RTP Stream,������RTPSession::AddStream() */
    fCurrentModuleIndex(0),
    fCurrentState(kStart),/* rtp session current state */
    fClosingReason(qtssCliSesCloseClientTeardown),/* Ĭ����Client�Լ��رյ�,�������RTPSession::Run() */
    fCurrentModule(0),
    fModuleDoingAsyncStuff(false),
    fLastBandwidthTrackerStatsUpdate(0)
{
#if DEBUG
    fActivateCalled = false;
#endif

    this->SetTaskName("RTPSession"); /* inherited from Task::SetTaskName() */
	/* set QTSS module state vars,��һ���������RTPSession::Run() */
    fModuleState.curModule = NULL;
    fModuleState.curTask = this;
    fModuleState.curRole = 0;
}

RTPSession::~RTPSession()
{
    // Delete all the streams
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
    
	/* ����Ԥ��ֵ�ܴ�ӡRUDP��Ϣ */
    if (QTSServerInterface::GetServer()->GetPrefs()->GetReliableUDPPrintfsEnabled())
    {
        SInt32 theNumLatePacketsDropped = 0;/* �����ӳٰ����� */
        SInt32 theNumResends = 0; /* �ش������� */
        
		/* ������RTPSession��ÿ��RTPStream,���㶪���Ĺ�ʱ���������ش������� */
        for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
        {
            Assert(theStream != NULL);
            Assert(theLen == sizeof(RTPStream*));
            if (*theStream != NULL)
            {
				/* ���㶪���Ĺ�ʱ������ */
                theNumLatePacketsDropped += (*theStream)->GetStalePacketsDropped();
				/* �õ��ش������� */
                theNumResends += (*theStream)->GetResender()->GetNumResends();
            }
        }
        
		/* �õ��ͻ������󲥷��ļ���Full URL */
        char* theURL = NULL;
        (void)this->GetValueAsString(qtssCliSesFullURL, 0, &theURL);
        Assert(theURL != NULL);
        /* ���RTPBandwidthTracker�� */
        RTPBandwidthTracker* tracker = this->GetBandwidthTracker(); 
    
        qtss_printf("Client complete. URL: %s.\n",theURL);
        qtss_printf("Max congestion window: %ld. Min congestion window: %ld. Avg congestion window: %ld\n", tracker->GetMaxCongestionWindowSize(), tracker->GetMinCongestionWindowSize(), tracker->GetAvgCongestionWindowSize());
        qtss_printf("Max RTT: %ld. Min RTT: %ld. Avg RTT: %ld\n", tracker->GetMaxRTO(), tracker->GetMinRTO(), tracker->GetAvgRTO());
        qtss_printf("Num resends: %ld. Num skipped frames: %ld. Num late packets dropped: %ld\n", theNumResends, this->GetFramesSkipped(), theNumLatePacketsDropped);
        
        delete [] theURL;
    }
    
	/* ������RTPSession��ÿ��RTPStream,��һɾ������ */
    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
            delete *theStream;
    }
    
	/* ��ȡ�������ӿ���,��һ����RTPSession��qtssSvrClientSessions������ɾȥ */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    {
        OSMutexLocker theLocker(theServer->GetMutex());
        
        RTPSession** theSession = NULL;
        
        // Remove this session from the qtssSvrClientSessions attribute
        UInt32 y = 0;
        for ( ; y < theServer->GetNumRTPSessions(); y++)
        {
            QTSS_Error theErr = theServer->GetValuePtr(qtssSvrClientSessions, y, (void**)&theSession, &theLen, true);
            Assert(theErr == QTSS_NoErr);
            
            if (*theSession == this)
            {
                theErr = theServer->RemoveValue(qtssSvrClientSessions, y, QTSSDictionary::kDontObeyReadOnly);
                break;
            }
        }

        Assert(y < theServer->GetNumRTPSessions());
        theServer->AlterCurrentRTPSessionCount(-1);
        if (!fIsFirstPlay) // The session was started playing (the counter ignores additional pause-play changes while session is active)
            theServer->AlterRTPPlayingSessions(-1);
        
    }


    //we better not be in the RTPSessionMap anymore!
#if DEBUG
    Assert(!fRTPMapElem.IsInTable());
	StrPtrLen theRTSPSessionID(fRTSPSessionIDBuf,sizeof(fRTSPSessionIDBuf));
	/* ��RTPSession Map��ɾȥ���� */
    OSRef* theRef = QTSServerInterface::GetServer()->GetRTPSessionMap()->Resolve(&theRTSPSessionID);
    Assert(theRef == NULL);
#endif
}

/* �õ�ǰRTSPSession����һ��OSRefʵ��, ��RTPSession Map(Hash��)��ע�Ტ���������OSRefԪ(��keyֵΨһ),
ͬʱ����qtssSvrClientSessions������,��QTSServerInterface�е���RTPSession�� */
QTSS_Error  RTPSession::Activate(const StrPtrLen& inSessionID)
{
    //Set the session ID for this session

	/* ���������fRTSPSessionIDBuf��ֵ(��C-String) */
    Assert(inSessionID.Len <= QTSS_MAX_SESSION_ID_LENGTH);
    ::memcpy(fRTSPSessionIDBuf, inSessionID.Ptr, inSessionID.Len);
    fRTSPSessionIDBuf[inSessionID.Len] = '\0';

	/* ��C-String��fRTSPSessionIDBuf����qtssCliSesRTSPSessionID������ֵ */
    this->SetVal(qtssCliSesRTSPSessionID, &fRTSPSessionIDBuf[0], inSessionID.Len);
    
	/* ��qtssCliSesRTSPSessionID������ֵ���õ�һ��RTPSession Map������OSRefԪ */
    fRTPMapElem.Set(*this->GetValue(qtssCliSesRTSPSessionID), this);
    
	/* ��ȡ�������ӿ� */
    QTSServerInterface* theServer = QTSServerInterface::GetServer();
    
    //Activate puts the session into the RTPSession Map
	/* ��RTPSession Map(Hash��)��ע�Ტ����һ������OSRef,���ַ�����ʶΨһ,���ܳɹ�����OS_NoErr;����һ����ͬkeyֵ��Ԫ��,�ͷ��ش���EPERM  */
    QTSS_Error err = theServer->GetRTPSessionMap()->Register(&fRTPMapElem);
	//����ͬ����ֵ,��������
    if (err == EPERM)
        return err;

	/* ȷ��keyֵΨһ */
    Assert(err == QTSS_NoErr);
    
    //
    // Adding this session into the qtssSvrClientSessions attr and incrementing the number of sessions must be atomic
	/* �������������� */
    OSMutexLocker locker(theServer->GetMutex()); 

    //
    // Put this session into the qtssSvrClientSessions attribute of the server
#if DEBUG
    Assert(theServer->GetNumValues(qtssSvrClientSessions) == theServer->GetNumRTPSessions());//ȷ������qtssSvrClientSessions�е�RTPSession�����ͷ�������ʵ�ʵĸ�����ͬ
#endif
	/* ����qtssSvrClientSessions�ĵ�GetNumRTPSessions()��RTPSession������ */
    RTPSession* theSession = this;
    err = theServer->SetValue(qtssSvrClientSessions, theServer->GetNumRTPSessions(), &theSession, sizeof(theSession), QTSSDictionary::kDontObeyReadOnly);
    Assert(err == QTSS_NoErr);
    
#if DEBUG
    fActivateCalled = true;
#endif
	/* ��ʱ����QTSServerInterface�е���RTPSession��  */
    QTSServerInterface::GetServer()->IncrementTotalRTPSessions();
    return QTSS_NoErr;
}

/* ���TCP Intervead mode,����RTPSession�е�ÿ��RTPStream,�ҵ����ָ����RTPStream(��RTP/RTCP channel�ž���ָ��channel��)������ */
RTPStream*  RTPSession::FindRTPStreamForChannelNum(UInt8 inChannelNum)
{
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;

    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
            if (((*theStream)->GetRTPChannelNum() == inChannelNum) || ((*theStream)->GetRTCPChannelNum() == inChannelNum))
                return *theStream;
    }
    return NULL; // Couldn't find a matching stream
}

/* used in QTSSCallbacks::QTSS_AddRTPStream() */
/*  ѭ������һ����Ψһ��ʶ��RTPSession��RTPStream�������µ�RTPStream�������(��ΪSSRC),�һ���µ�RTPStream,����RTPStream������ */
QTSS_Error RTPSession::AddStream(RTSPRequestInterface* request, RTPStream** outStream, QTSS_AddStreamFlags inFlags)
{
    Assert(outStream != NULL);

    // Create a new SSRC for this stream. This should just be a random number unique
    // to all the streams in the session
	/* ѭ������һ����Ψһ��ʶ��RTPSession��RTPStream�������µ�RTPStream�������,��ΪSSRC(���͸��������ִ��ÿ��RTPStream��SSRC����ͬ) */
    UInt32 theSSRC = 0;
    while (theSSRC == 0)
    {
		/* ���������,���ڸ�RTPSession��RTPStream������Ψһ��ʶһ��RTPSream */
        theSSRC = (SInt32)::rand();

        RTPStream** theStream = NULL;
        UInt32 theLen = 0;
    
		/* ����RTPSession�е�ÿ��RTPStream,������SSRCǡ��theSSRC��ͬ,�ͽ�theSSRC��0,ʹ�ø�ѭ������ */
        for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
        {
            Assert(theStream != NULL);
            Assert(theLen == sizeof(RTPStream*));
            
            if (*theStream != NULL)
                if ((*theStream)->GetSSRC() == theSSRC)
                    theSSRC = 0;
        }
    }

	/***************** �½�һ��RTPStream����,����Ψһ��ʶ����SSRC(�����) *************/
    *outStream = NEW RTPStream(theSSRC, this);
	/***************** �½�һ��RTPStream���� *************/
    
	/* ��ָ�����뷽ʽ���RTSP Request��RTPStream */
    QTSS_Error theErr = (*outStream)->Setup(request, inFlags);
	/* �������ɹ�,��ɾȥ�½�RTPStream���� */
    if (theErr != QTSS_NoErr)
        // If we couldn't setup the stream, make sure not to leak memory!
        delete *outStream;
	/* �����ɹ�,�������RTPStream Array��,ע������qtssCliSesStreamObjects�Ǹ���ֵ���������� */
    else
    {
        // If the stream init succeeded, then put it into the array of setup streams
        theErr = this->SetValue(qtssCliSesStreamObjects, this->GetNumValues(qtssCliSesStreamObjects),
                                                    outStream, sizeof(RTPStream*), QTSSDictionary::kDontObeyReadOnly);
        Assert(theErr == QTSS_NoErr);
        fHasAnRTPStream = true;/* ������һ��RTPStream */
    }

    return theErr;
}

/* used in RTPSession::Play() */
/* ����RTPSession�е�ÿ��RTPStream,�����ָ����LateToleranceֵ����LateTolerance,�������ֻ��ݻ����� */
void RTPSession::SetStreamThinningParams(Float32 inLateTolerance)
{
	// Set the thinning params in all the RTPStreams of the RTPSession
	// Go through all the streams, setting their thinning params
	RTPStream** theStream = NULL;
	UInt32 theLen = 0;
	
	for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
	{
		Assert(theStream != NULL);
		Assert(theLen == sizeof(RTPStream*));
		if (*theStream != NULL)
		{
			(*theStream)->SetLateTolerance(inLateTolerance);
			(*theStream)->SetThinningParams();
		}
	}
}

/* ���úø��ֲ������źſ�ʼ����RTPSession�ͷ���SR��.����ӰƬ,���������ò���:(1)��¼��������Ӧ�Ĳ���ʱ��;(2)���ò���״̬,���ű�־,���²��ŵ�RTPSession����;
(3)���ݵ�Ӱ��ƽ������������Client Windows size,���ù����崰;(4)����RTPSession�е�ÿ��RTPStream,���ô򱡲���,���ô��ӳٲ���,����ϸ�Play���ش����RTP��;(5)�����õ�movie
ƽ��������>0,��������TCP RTSP Socket�����С����Ӧmovie������,�ҽ���Ԥ������ֵ����Сֵ֮��;(6)���ź�������RTP Session����; */
QTSS_Error  RTPSession::Play(RTSPRequestInterface* request, QTSS_PlayFlags inFlags)
{
    //first setup the play associated session interface variables
    Assert(request != NULL);
	/* ����û��Module����,�������� */
    if (fModule == NULL)
        return QTSS_RequestFailed;//Can't play if there are no associated streams
    
    //what time is this play being issued(����) at? ��¼Play������ʱ��
	fLastBitRateUpdateTime = fNextSendPacketsTime = fPlayTime = OS::Milliseconds();
	/* �����ǵ�һ�β���,ͬ����ǰ����ʱ�� */
    if (fIsFirstPlay)
		fFirstPlayTime = fPlayTime;
	/* ��ȡ��ǰ����ʱ�����RTSP Request Header Range�е�fStartTime�Ĳ�ֵ */
    fAdjustedPlayTime = fPlayTime - ((SInt64)(request->GetStartTime() * 1000));
    //for RTCP SRs(RTCP���ͷ�����), we also need to store the play time in NTP
    fNTPPlayTime = OS::TimeMilli_To_1900Fixed64Secs(fPlayTime);
    
    //we are definitely playing now, so schedule(����) the object!
    fState = qtssPlayingState;
    fIsFirstPlay = false;/* ���ڲ��ǵ�һ�β��� */
    fPlayFlags = inFlags;/* ������SR������AppendServerInfo��SR�� */

	/* track how many sessions are playing,see QTSServerInterface.h,ʹfNumRTPPlayingSessions��1 */
    QTSServerInterface::GetServer()-> AlterRTPPlayingSessions(1);
    
	/* set Client window size according to bitrate,,�μ�RTPBandwidthTracker::SetWindowSize() */
	//���ݵ�Ӱ��ƽ������������Client Windows size
    UInt32 theWindowSize;
	/* ��ȡ��Ӱ��ƽ�������� */
    UInt32 bitRate = this->GetMovieAvgBitrate();
	/* �����Ӱ��ƽ��������Ϊ0�򳬹�1Mbps,������Windowsize Ϊ64Kbytes */
	if ((bitRate == 0) || (bitRate > QTSServerInterface::GetServer()->GetPrefs()->GetWindowSizeMaxThreshold() * 1024))
        theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetLargeWindowSizeInK();
	/* �����Ӱ��ƽ�������ʳ���200kbps,������Windowsize Ϊ48Kbytes */
	else if (bitRate > QTSServerInterface::GetServer()->GetPrefs()->GetWindowSizeThreshold() * 1024)
		theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetMediumWindowSizeInK();
	/* ����,,������Windowsize Ϊ24Kbytes */
    else
        theWindowSize = 1024 * QTSServerInterface::GetServer()->GetPrefs()->GetSmallWindowSizeInK();

    qtss_printf("bitrate = %d, window size = %d\n", bitRate, theWindowSize);

	/* ����Client���ڴ�С,�μ�RTPBandwidthTracker::SetWindowSize() */
    this->GetBandwidthTracker()->SetWindowSize(theWindowSize);

	/* ���ù����崰 */
	this->GetOverbufferWindow()->ResetOverBufferWindow();

    // Go through all the streams, setting their thinning params
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
    
	/* ����RTPSession�е�ÿ��RTPStream,���ô򱡲���,���ô��ӳٲ���,����ϸ�Play���ش����RTP�� */
    for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
    {
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        if (*theStream != NULL)
        {   /* ���ô򱡲��� */
            (*theStream)->SetThinningParams();
 			(*theStream)->ResetThinningDelayParams();
            
            // If we are using reliable UDP, then make sure to clear all the packets from the previous play spurt out of the resender 
            (*theStream)->GetResender()->ClearOutstandingPackets();
        }
    }

    qtss_printf("movie bitrate = %d, window size = %d\n", this->GetMovieAvgBitrate(), theWindowSize);
    Assert(this->GetBandwidthTracker()->BytesInList() == 0);
    
    // Set the size of the RTSPSession's send buffer to an appropriate max size
    // based on the bitrate of the movie. This has 2 benefits:
    // 1) Each socket normally defaults to 32 K. A smaller buffer prevents the
    // system from getting buffer starved if lots of clients get flow-controlled
    //
    // 2) We may need to scale up buffer sizes for high-bandwidth movies in order
    // to maximize thruput, and we may need to scale down buffer sizes for low-bandwidth
    // movies to prevent us from buffering lots of data that the client can't use
    //
    // If we don't know any better, assume maximum buffer size.

	/* ��ȡ������Ԥ��ֵ���õ����TCP��������С */
    QTSServerPrefs* thePrefs = QTSServerInterface::GetServer()->GetPrefs();
    UInt32 theBufferSize = thePrefs->GetMaxTCPBufferSizeInBytes();
    
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession GetMovieAvgBitrate %li\n",(SInt32)this->GetMovieAvgBitrate() );
#endif

	/* �����õ�movieƽ��������>0,��������TCP Socket�����С����Ӧmovie������,�ҽ���Ԥ������ֵ����Сֵ֮�� */
    if (this->GetMovieAvgBitrate() > 0)
    {
        // We have a bit rate... use it.
		/* �ɵ�Ӱ�����ʺ�Ԥ��Ļ�������(0.5s),�õ�ʵ�ʵĻ����С(�ֽ�) */
        Float32 realBufferSize = (Float32)this->GetMovieAvgBitrate() * thePrefs->GetTCPSecondsToBuffer();
        theBufferSize = (UInt32)realBufferSize;
        theBufferSize >>= 3; // Divide by 8 to convert from bits to bytes
        
        // Round down to the next lowest power of 2.
		/* �������뵽��ӽ���С������2����,�ú������������ */
		/* ����α�Ϊ2���ݵ������㷨:�Ƚ���α�ʾΪʮ������,�ӵ�29bitλ��ʼ�Ӹߵ��׿�ʼ��,������ֵΪ1��bitλ,�ͷ��ظ�bitλֵΪ1,����31��bitλֵȫΪ0����;��һֱû��1������0 */
        theBufferSize = this->PowerOf2Floor(theBufferSize);
        
		//����Ի����С���б���,ʹ��ʵֵ��ؽ���Ԥ������ֵ����Сֵ֮��

        // This is how much data we should buffer based on the scaling factor... if it is lower than the min, raise to min
        if (theBufferSize < thePrefs->GetMinTCPBufferSizeInBytes())
            theBufferSize = thePrefs->GetMinTCPBufferSizeInBytes();
            
        // Same deal for max buffer size
        if (theBufferSize > thePrefs->GetMaxTCPBufferSizeInBytes())
            theBufferSize = thePrefs->GetMaxTCPBufferSizeInBytes();
    }
    
	/* ��������Ľ��ջ����СΪTCP RTSP Socket�Ļ����С */
    Assert(fRTSPSession != NULL); // can this ever happen?
    if (fRTSPSession != NULL)
        fRTSPSession->GetSocket()->SetSocketBufSize(theBufferSize);/* set RTSP buffer size by above value */
           
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession %ld: In Play, about to call Signal\n",(SInt32)this);
#endif

	/* after set some parama, a rtp session task is about to start */
	//���ź�������RTP Session����
    this->Signal(Task::kStartEvent);
    
    return QTSS_NoErr;
}

/* used in RTPSession::Play() */
/* ����α�Ϊ2���ݵ������㷨:�Ƚ���α�ʾΪʮ������,�ӵ�29bitλ��ʼ�Ӹߵ��׿�ʼ��,������ֵΪ1��bitλ,�ͷ��ظ�bitλֵΪ1,����31��bitλֵȫΪ0����;��һֱû��1������0 */
UInt32 RTPSession::PowerOf2Floor(UInt32 inNumToFloor)
{
    UInt32 retVal = 0x10000000;
    while (retVal > 0)
    { /* ʮ��λ������μ���? */
        if (retVal & inNumToFloor) /* ���������λ��(4bit-wide)�ĵ��ĸ�bit��ֵ������1,�ͰѸ�bit�Ժ��28λ����Ϊ0 */
            return retVal;
        else
            retVal >>= 1;/* ����retVal���� */
    }
    return retVal;
}

/* ���ٸ�RTSP�Ự�ĳ��ж������,����RTPSession״̬Ϊ��ͣ,�ÿ�fRTSPSession,�ٷ��ź�ɾȥ��RTPSession */
void RTPSession::Teardown()
{
    // To proffer a quick death of the RTSP session, let's disassociate
    // ourselves with it right now.
    
    // Note that this function relies on the session mutex being grabbed, because
    // this fRTSPSession pointer could otherwise be being used simultaneously by
    // an RTP stream.
    if (fRTSPSession != NULL)
        fRTSPSession->DecrementObjectHolderCount();/* ���ٸ�RTSP�Ự�ĳ��ж������ */
    fRTSPSession = NULL;
    fState = qtssPausedState;
	/* ���ź�ɾȥ��RTPSession */
    this->Signal(Task::kKillEvent);
}

/* ����RTPSession�е�ÿ��RTPStream,������ΪqtssPlayRespWriteTrackInfo,Ϊÿ��RTPStream����RTPʱ��������к���Ϣ,��Client��RTSPResponseStream�з����׼��Ӧ��������:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 11\r\n
Session: 7736802604597532330\r\n
Range: npt=0.00000-70.00000\r\n
RTP-Info: url=rtsp://172.16.34.22/sample_300kbit.mp4/trackID=3;seq=15724;rtptime=503370233,url=rtsp://172.16.34.22/sample_300kbit.mp4/trackID=4;seq=5452;rtptime=1925920323\r\n
\r\n
*/
void RTPSession::SendPlayResponse(RTSPRequestInterface* request, UInt32 inFlags)
{
    QTSS_RTSPHeader theHeader = qtssRTPInfoHeader;
    
    RTPStream** theStream = NULL;
    UInt32 theLen = 0;
	/* ��ø�RTPSession�����е�RTPStream�ĸ��� */
    UInt32 valueCount = this->GetNumValues(qtssCliSesStreamObjects);
	/* �����һ��RTPStream��? */
    Bool16 lastValue = false;

	/* ����RTPSession�е�ÿ��RTPStream,Ϊÿ��RTPStream����RTPʱ��������к���Ϣ */
    for (UInt32 x = 0; x < valueCount; x++)
    {
		/* ��ȡָ������ֵ��RTPStream */
        this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen);
        Assert(theStream != NULL);
        Assert(theLen == sizeof(RTPStream*));
        
        if (*theStream != NULL)
        {   
			/* �����һ��RTPStream,����lastValueΪtrue,��������⸽��"\r\n" */
			if (x == (valueCount -1))
                lastValue = true;

			/* ��QTSS_SendStandardRTSPResponse��,������ΪqtssPlayRespWriteTrackInfo,����RTPʱ��������к���Ϣ */
            (*theStream)->AppendRTPInfo(theHeader, request, inFlags,lastValue);
            theHeader = qtssSameAsLastHeader;//�����ḽ��","����"RTPInfo"
        }
    }

	/* ��Client��RTSPResponseStream�з�������ָ���õ���Ӧ���� */
    request->SendHeader();
}

/* ���Client���͵�DESCRIBE,��RTSPResponseStream�и�����������:
Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
Cache-Control: must-revalidate\r\n
Content-length: 1209\r\n
Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Content-Type: application/sdp\r\n
x-Accept-Retransmit: our-retransmit\r\n
x-Accept-Dynamic-Rate: 1\r\n
Content-Base: rtsp://172.16.34.22/sample_300kbit.mp4/\r\n
����,��Client������������:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 8\r\n
Last-Modified: Wed, 25 Nov 2009 06:44:41 GMT\r\n
Cache-Control: must-revalidate\r\n
Content-length: 1209\r\n
Date: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Expires: Fri, 02 Jul 2010 05:03:08 GMT\r\n
Content-Type: application/sdp\r\n
x-Accept-Retransmit: our-retransmit\r\n
x-Accept-Dynamic-Rate: 1\r\n
Content-Base: rtsp://172.16.34.22/sample_300kbit.mp4/\r\n
\r\n
*/
void    RTPSession::SendDescribeResponse(RTSPRequestInterface* inRequest)
{
	/* ���統ǰRTSP Request��״̬��"304 Not Modified",������Client��RTSPResponseStream�з����׼��Ӧ����,���� */
    if (inRequest->GetStatus() == qtssRedirectNotModified)
    {
        (void)inRequest->SendHeader();
        return;
    }
    
    // write date and expires
	/*
	   ��Client��Describe��RTSPResponseStream�з�����������:
	   Date: Fri, 02 Jul 2010 05:22:36 GMT\r\n
	   Expires: Fri, 02 Jul 2010 05:22:36 GMT\r\n
	*/
    inRequest->AppendDateAndExpires();
    
    //write content type header
	//��������һ��:Content-Type: application/sdp\r\n
    static StrPtrLen sContentType("application/sdp");
    inRequest->AppendHeader(qtssContentTypeHeader, &sContentType);
    
    // write x-Accept-Retransmit header
	//��������һ��:x-Accept-Retransmit: our-retransmit\r\n
    static StrPtrLen sRetransmitProtocolName("our-retransmit");
    inRequest->AppendHeader(qtssXAcceptRetransmitHeader, &sRetransmitProtocolName);
	
	// write x-Accept-Dynamic-Rate header
	//��������һ��:x-Accept-Dynamic-Rate: 1\r\n
	static StrPtrLen dynamicRateEnabledStr("1");
	inRequest->AppendHeader(qtssXAcceptDynamicRateHeader, &dynamicRateEnabledStr);
    
    //write content base header
    //��������һ��:Content-Base: rtsp://172.16.34.22/sample_h264_1mbit.mp4/\r\n
    inRequest->AppendContentBaseHeader(inRequest->GetValue(qtssRTSPReqAbsoluteURL));
    
    //I believe the only error that can happen is if the client has disconnected.
    //if that's the case, just ignore it, hopefully the calling module will detect
    //this and return control back to the server ASAP 
	/* ��Client��RTSPResponseStream�з����׼��Ӧ���� */
    (void)inRequest->SendHeader();
}

/* ������Client��RTSPResponseStream�з����׼��Ӧ����:
RTSP/1.0 200 OK\r\n
Server: DSS/5.5.3.7 (Build/489.8; Platform/Linux; Release/Darwin; )\r\n
Cseq: 8\r\n
\r\n
*/
void    RTPSession::SendAnnounceResponse(RTSPRequestInterface* inRequest)
{
    //
    // Currently, no need to do anything special for an announce response
    (void)inRequest->SendHeader();
}

/* ����״̬��Run()�����ķ���ֵ�����֣��������ֵΪ������������һ�η������ݰ���ʱ�䣬�涨ʱ�䵽����ʱ��
   TaskThread�̻߳��Զ�����Run�������������ֵ����0�����´��κ��¼�����ʱ��Run�����ͻᱻ���ã��������
   �����������������ݶ��Ѿ�������ɻ��߸�RTPSession����Ҫ��ɱ����ʱ�򡣶��������ֵΪ��������TaskThread
   �̻߳�delete RTPSession����
*/
SInt64 RTPSession::Run()
{
#if DEBUG
    Assert(fActivateCalled);
#endif
	
	/* first acquire event flags and which event should be send to task(role) */
    EventFlags events = this->GetEvents();
    QTSS_RoleParams theParams;
	/* ע��QTSS_RoleParams��������,ÿ��ֻȡһ��.����ClientSession����ָRTPSession */
    theParams.clientSessionClosingParams.inClientSession = this; //every single role being invoked now has this as the first parameter
                                                       
#if RTPSESSION_DEBUGGING
    qtss_printf("RTPSession %ld: In Run. Events %ld\n",(SInt32)this, (SInt32)events);
#endif

    // Some callbacks look for this struct in the thread object,���õ�ǰ�̵߳�˽������
    OSThreadDataSetter theSetter(&fModuleState, NULL);

	/****************** CASE: Return -1 ************************************************************************/
    //if we have been instructed to go away(�߿�), then let's delete ourselves
    if ((events & Task::kKillEvent) || (events & Task::kTimeoutEvent) || (fModuleDoingAsyncStuff))
    {     /* ����Module��ͬ��(sync)ģʽ,��RTP Session map��ע����RTPSession�ı�Ԫ,��ʧ��,�ͷ��ź�ȥKill��RTPSession;
		  ���ɹ�,����RTPSession�е�ÿ��RTPStream,��һ����BYE�� */
		  if (!fModuleDoingAsyncStuff)
		  {  
			if (events & Task::kTimeoutEvent)
				fClosingReason = qtssCliSesCloseTimeout;//�ر�RTPSession����Ϊ��ʱ
	            
			//deletion is a bit complicated. For one thing, it must happen from within
			//the Run function to ensure that we aren't getting events when we are deleting
			//ourselves. We also need to make sure that we aren't getting RTSP requests
			//(or, more accurately, that the stream object isn't being used by any other
			//threads). We do this by first removing the session from the session map.
	        
	#if RTPSESSION_DEBUGGING
			qtss_printf("RTPSession %ld: about to be killed. Eventmask = %ld\n",(SInt32)this, (SInt32)events);
	#endif
			// We cannot block(����) waiting to UnRegister(ע��), because we have to
			// give the RTSPSessionTask a chance to release the RTPSession.
			/* ��ȡ������ȫ�ֵ� RTP Session map,�����Դ���ע����RTPSession�ı�Ԫ,�����ɹ��ͷ��ź�ȥKill��RTPSession */
			OSRefTable* sessionTable = QTSServerInterface::GetServer()->GetRTPSessionMap();
			Assert(sessionTable != NULL);
			if (!sessionTable->TryUnRegister(&fRTPMapElem))
			{
				//Send an event to this task.
				this->Signal(Task::kKillEvent);// So that we get back to this place in the code
				return kCantGetMutexIdleTime; /* 10 */
			}
	        
			// The ClientSessionClosing role is allowed to do async stuff
			fModuleState.curTask = this;

			/* ����RTP Session map�гɹ�ע���˱�RTPSession�ı�Ԫ,��Module�����첽,Ҫ�Ӵ����з���(�μ�RTSPSession::Run()),�Եȴ��´μ����������� */
			fModuleDoingAsyncStuff = true;  // So that we know to jump back to the right place in the code
			fCurrentModule = 0;              
	    
			// Set the reason parameter in object QTSS_ClientSessionClosing_Params
			theParams.clientSessionClosingParams.inReason = fClosingReason;
	        
			// If RTCP packets are being generated internally for this stream, Send a BYE now.
			RTPStream** theStream = NULL;
			UInt32 theLen = 0;
			/* ����RTPSession�е�ÿ��RTPStream,��һ����BYE��  */
			if (this->GetPlayFlags() & qtssPlayFlagsSendRTCP)
			{	
				SInt64 byePacketTime = OS::Milliseconds();
				for (int x = 0; this->GetValuePtr(qtssCliSesStreamObjects, x, (void**)&theStream, &theLen) == QTSS_NoErr; x++)
					if (theStream && *theStream != NULL)
						(*theStream)->SendRTCPSR(byePacketTime, true);//true means send BYE
			}
		  }
        
        //at this point, we know no one is using this session, so invoke the
        //session cleanup role. We don't need to grab the session mutex before
        //invoking modules here, because the session is unregistered(ע��) and
        //therefore there's no way another thread could get involved anyway
	    //����Module���첽ģʽ,����ȷ��û��Module���ø�RTPSession,�͵���ע��Session cleanup role��Moduleȥ�ر���,����ʹ�û�����
        UInt32 numModules = QTSServerInterface::GetNumModulesInRole(QTSSModule::kClientSessionClosingRole);
        {
            for (; fCurrentModule < numModules; fCurrentModule++)
            {  
                fModuleState.eventRequested = false;
                fModuleState.idleTime = 0;

                QTSSModule* theModule = QTSServerInterface::GetModule(QTSSModule::kClientSessionClosingRole, fCurrentModule);
                /* ����ģ���Client Session Closing��ɫ��ʹ��ģ������ڿͻ��Ự�ر�ʱ���б�Ҫ�Ĵ��� */
				(void)theModule->CallDispatch(QTSS_ClientSessionClosing_Role, &theParams);

                // If this module has requested an event, return and wait for the event to transpire(����,����)
                if (fModuleState.eventRequested)
                    return fModuleState.idleTime; // If the module has requested idle time...
            }
        }
        
        return -1;//doing this will cause the destructor to get called.
   }
    
	/****************** CASE: Return 0 ************************************************************************/
    //if the RTP stream is currently paused, just return without doing anything.We'll get woken up again when a play is issued
    if ((fState == qtssPausedState) || (fModule == NULL))
        return 0;
     
	/****************** CASE: Return any positive value ************************************************************************/
    //Make sure to grab the session mutex here, to protect the module against RTSP requests coming in while it's sending packets
    {
		/* ������RTPSession������QTSS_SendPackets_Params׼��send packets */
        OSMutexLocker locker(&fSessionMutex);

        //just make sure we haven't been scheduled before our scheduled play
        //time. If so, reschedule ourselves for the proper time. (if client
        //sends a play while we are already playing, this may occur)
		/* obtain the current time to send RTP packets */
		//�趨���ݰ�����ʱ�䣬��ֹ����ǰ����,ˢ��QTSS_RTPSendPackets_Params�еĵ�ǰʱ���
        theParams.rtpSendPacketsParams.inCurrentTime = OS::Milliseconds();
		/* fNextSendPacketsTime see RTPSessionInterface.h,��ʾsend Packets�ľ���ʱ��� */
		//δ������ʱ��ʱ�����ش������õȴ�������ʱ��
        if (fNextSendPacketsTime > theParams.rtpSendPacketsParams.inCurrentTime)
        {
			/* �ش�RTPStream�Ķ�ά���� */
            RTPStream** retransStream = NULL;
            UInt32 retransStreamLen = 0;

            // Send retransmits if we need to
			/* �Ȳ��Ҹ�RTPSession���ش�����,Ϊ��RTPSession��ÿ��RTPStream�����ش� */
            for (int streamIter = 0; this->GetValuePtr(qtssCliSesStreamObjects, streamIter, (void**)&retransStream, &retransStreamLen) == QTSS_NoErr; streamIter++)
				if (retransStream && *retransStream)
                    (*retransStream)->SendRetransmits(); 
            
			//���㻹��೤ʱ��ſ����С�
			/*outNextPacketTime�Ǽ��ʱ�䣬�Ժ���Ϊ��λ���������ɫ����֮ǰ��ģ����Ҫ�趨һ������
			  ��outNextPacketTimeֵ�����ֵ�ǵ�ǰʱ��inCurrentTime�ͷ������ٴ�Ϊ��ǰ�Ự����QTSS_RTPSendPackets_Role
			  ��ɫ��ʱ��fNextSendPacketsTime֮���ʱ������*/
			/*  Ϊ�ش��������ش���ʱ����,����ô��ʱ���ͷ��͸��ش��� */
            theParams.rtpSendPacketsParams.outNextPacketTime = fNextSendPacketsTime - theParams.rtpSendPacketsParams.inCurrentTime;
        }
        else /* retransmit scheduled data normally */
        {   /* ��һ���Ͱ�ʱ���ѹ�? ���Ͽ�ʼ������ */        
    #if RTPSESSION_DEBUGGING
            qtss_printf("RTPSession %ld: about to call SendPackets\n",(SInt32)this);
    #endif
			/* fLastBandwidthTrackerStatsUpdate see RTPSession.h,�Ƿ��������Ǹ���״̬��? */
			/* ������¼������1000����,������ȡ���µ�BandWidth */
            if ((theParams.rtpSendPacketsParams.inCurrentTime - fLastBandwidthTrackerStatsUpdate) > 1000)
				/* GetBandwidthTracker() see RTPSessionInterface.h,  UpdateStats() see RTPBandwidthTracker.h */
                this->GetBandwidthTracker()->UpdateStats();
        
			//�´�����ʱ���ȱʡֵΪ0,��������,���Ͻ��ð����ͳ�ȥ
			/* ���Ͱ�ʱ��������Ϊ0,׼������ */
            theParams.rtpSendPacketsParams.outNextPacketTime = 0;
            // Async event registration is definitely allowed from this role.
			/* ���ý��ܿ��Է�����֪ͨ�� */
            fModuleState.eventRequested = false;
			
            /* make assure that there is a QTSSModule, here we use QTSSFileModule */
			/* ��Ϊ��������Ҫ�������ģ�鷢�� */
			/* ����:������λ�֪QTSSModule���ĸ�ģ���?���ĵ�����RTSPSession::Run()�е�kPreprocessingRequest
			��kProcessingRequest��;������SetPacketSendingModule(),�õ���GetPacketSendingModule() */
			Assert(fModule != NULL);

			/* ����QTSSFileModuleDispatch(), refer to QTSSFileModule.cpp */
			/* QTSS_RTPSendPackets_Role��ɫ����������ͻ��˷���ý�����ݣ������߷�����ʲôʱ��ģ��(ֻ����QTSSFileModule)��QTSS_RTPSendPackets_Role��ɫӦ���ٴα����á�*/
            (void)fModule->CallDispatch(QTSS_RTPSendPackets_Role, &theParams);
    #if RTPSESSION_DEBUGGING
            qtss_printf("RTPSession %ld: back from sendPackets, nextPacketTime = %"_64BITARG_"d\n",(SInt32)this, theParams.rtpSendPacketsParams.outNextPacketTime);
    #endif

            //make sure not to get deleted accidently!
			/* ������������������´��Ͱ�����ȷ��ʱ���� */
			/* make sure that the returned value is nonnegative, otherwise will be deleted by TaskTheread  */
            if (theParams.rtpSendPacketsParams.outNextPacketTime < 0)
                theParams.rtpSendPacketsParams.outNextPacketTime = 0;
			/* fNextSendPacketsTime see RTPSessionInterface.h */
			/* QTSS_RTPSendPackets_Params����Ҫ��ʱ���ϵ */
			/* �����������´��Ͱ��ľ���ʱ��� �� */
            fNextSendPacketsTime = theParams.rtpSendPacketsParams.inCurrentTime + theParams.rtpSendPacketsParams.outNextPacketTime;
        }     
    }
    
    // Make sure the duration between calls to Run() isn't greater than the max retransmit delay interval.���ͼ��(50����)<=�ش����(�����RUDP,Ĭ��500����)

	/* obtain the preferred max retransmit delay time in Msec, see QTSServerInterface.h  */
    UInt32 theRetransDelayInMsec = QTSServerInterface::GetServer()->GetPrefs()->GetMaxRetransmitDelayInMsec();
	/* obtain the preferred duration between two calls to Run() in Msec, see QTSServerInterface.h  */
    UInt32 theSendInterval = QTSServerInterface::GetServer()->GetPrefs()->GetSendIntervalInMsec();
    
    // We want to avoid waking up to do retransmits, and then going back to sleep for like, 1 msec. So, 
    // only adjust the time to wake up if the next packet time is greater than the max retransmit delay +
    // the standard interval between wakeups.
	/* adjust the time to wake up  */
	/* in general,theRetransDelayInMsec is bigger than  theSendInterval ��Ҫʱ������һ����������ʱ���� */
    if (theParams.rtpSendPacketsParams.outNextPacketTime > (theRetransDelayInMsec + theSendInterval))
        theParams.rtpSendPacketsParams.outNextPacketTime = theRetransDelayInMsec;
    
    Assert(theParams.rtpSendPacketsParams.outNextPacketTime >= 0);//we'd better not get deleted accidently!
	/* return the next desired runtime  */
    return theParams.rtpSendPacketsParams.outNextPacketTime;
}

