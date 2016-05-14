
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
              2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPOverbufferWindow.cpp
Description: Class that tracks packets that are part of the "overbuffer". That is,
			 packets that are being sent ahead of time. This class can be used
			 to make sure the server isn't overflowing the client's overbuffer size.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-17

****************************************************************************/ 



#include "RTPOverbufferWindow.h"
#include "OSMemory.h"
#include "MyAssert.h"



// ��βμ�RTPSessionInterface::RTPSessionInterface(),���Ƿ�������Ԥ��ֵ
RTPOverbufferWindow::RTPOverbufferWindow(UInt32 inSendInterval, UInt32 inInitialWindowSize, UInt32 inMaxSendAheadTimeInSec,Float32 inOverbufferRate)									
:   fWindowSize(inInitialWindowSize),/* kUInt32_Max,�μ�RTPStream::Setup()/ProcessIncomingRTCPPacket() */
    fBytesSentSinceLastReport(0),
    fSendInterval(inSendInterval),
    fBytesDuringLastSecond(0),
    fLastSecondStart(-1),
    fBytesDuringPreviousSecond(0),
    fPreviousSecondStart(-1),
    fBytesDuringBucket(0),
    fBucketBegin(0),/* ע�������ʼֵ!! */
    fBucketTimeAhead(0),/* ������ʱ */
    fPreviousBucketTimeAhead(0),
    fMaxSendAheadTime(inMaxSendAheadTimeInSec * 1000), /* 25s */
    fWriteBurstBeginning(false),/* ��û�п�ʼ����д�� */
	fOverbufferingEnabled(true),/* ����ʹ��Overbuffering��ǰ���ͻ��� */
	fOverbufferRate(inOverbufferRate), /* Ĭ������2.0 */
	fSendAheadDurationInMsec(1000),/* ע����ǰ���͵ĳ���ʱ����1s */
	fOverbufferWindowBegin(-1) /* ע�������ʼֵ!! */
{
	/* �����Ͱ����0,�Ͳ��ó�ǰ���ͻ���,�����Ͱ��������Ϊ200ms */
    if (fSendInterval == 0)
    {
		/* ע��رճ�ǰ���ͻ��� */
        fOverbufferingEnabled = false;
        fSendInterval = 200;
    }
	
	/* ȷ��overbufferRate������1.0,Ĭ��Ϊ2.0 */
	if(fOverbufferRate < 1.0)
		fOverbufferRate = 1.0;

}

/****************************** ����������ĵ�һ������ ***********************************************************************/
/* ���������������:��Ե�ǰ���趨�ķ���ʱ���,��鵱ǰ���ݰ��Ƿ������ǰ����,��ǰ�೤ʱ�䷢��?��������ǰ�����Ľ����������һ��������ʾ���ݰ����趨����ʱ�䣬�ڶ���������ʾ�����ĵ�ǰʱ�䣬������������ʾ�������ݰ���С��
����������أ�1�������ݻ����̱����ͣ��������ֵ���ڵ�ǰʱ�������ݻ�ȴ���һ�α����͡� */
SInt64 RTPOverbufferWindow::CheckTransmitTime(const SInt64& inTransmitTime, const SInt64& inCurrentTime, SInt32 inPacketSize)
{
    // if this is the beginning of a bucket interval, roll over figures from last time.�����ϴε�����
    // accumulate statistics over the period of a second �ۼ�һ���ڵ�ͳ������

	/* ���統ǰʱ�����Bucket��ʼʱ���,����һ��bucket interval,���������:��ǰBucket interval��ʼʱ��,��ǰʱ��,�����ֽ��� */
    if (inCurrentTime - fBucketBegin > fSendInterval)
    {
        fPreviousBucketBegin = fBucketBegin;
        fBucketBegin = inCurrentTime;
		/* ��¼��һ��Bucket interval��ʼ�ľ���ʱ�� */
		if (fPreviousBucketBegin == 0)
			fPreviousBucketBegin = fBucketBegin - fSendInterval;
        fBytesDuringBucket = 0;

		/* ÿ��1000ms,������һ���ڷ��͵��ֽ�������ʼʱ������� */
        if (inCurrentTime - fLastSecondStart > 1000)
        {
			/* ��¼��һ���ڷ��͵��ֽ��� */
            fBytesDuringPreviousSecond = fBytesDuringLastSecond;
			/* ������һ�뷢�͵��ֽ��� */
            fBytesDuringLastSecond = 0;
			/* ��¼��һ����ʼ��ʱ�� */
            fPreviousSecondStart = fLastSecondStart;
            /* ������һ����ʼ��ʱ�� */
            fLastSecondStart = inCurrentTime;
        }
        
		/* ��¼��ǰBucket interval��ǰ��ʱ��,�����Ǹð��ķ����ӳ� */
        fPreviousBucketTimeAhead = fBucketTimeAhead;
    }
    
	/* ����Overbuffer���ڵ���ʼʱ�� */
	if (fOverbufferWindowBegin == -1)
		fOverbufferWindowBegin = inCurrentTime;
	
	/* �����RTP���趨�ķ���ʱ���ڵ�ǰ����ʱ�����,�������ڿ�����ǰ����,���Ҹ�RTP��������ǰ���ͺ��������͵�ʱ�����,���������ͳ��ð� */
	if ((inTransmitTime <= inCurrentTime + fSendInterval) || 
		(fOverbufferingEnabled && (inTransmitTime <= inCurrentTime + fSendInterval + fSendAheadDurationInMsec)))
    {
        // If this happens, this packet needs to be sent regardless of overbuffering
        return -1;
    }
    
	//���Overbuffering���عر�,����overbuffer����СΪ0,��ֱ�ӷ��ظð��趨�ķ���ʱ��,���ó�ǰ����
    if (!fOverbufferingEnabled || (fWindowSize == 0))
        return inTransmitTime;
    
	//���overbuffer���ʹ���ʣ��ռ䲻��5*���ݰ���С,���ӻ���5�����ͼ�����͸ð�
    // if the client is running low on memory, wait a while for it to be freed up
    // there's nothing magic bout these numbers, we're just trying to be conservative����Щ
    if ((fWindowSize != -1) && (inPacketSize * 5 > fWindowSize - fBytesSentSinceLastReport))
    {
        return inCurrentTime + (fSendInterval * 5);  // client reports don't come that often
    }
    
	//fMaxSendAheadTimeΪ25�룬�����ʾ���ͳ�ǰʱ�䲻�ܴ���25�룬���������ٵȴ�һ�����ͼ����ǰ����
    // if we're far enough ahead, then wait until it's time to send more packets
    if (inTransmitTime - inCurrentTime > fMaxSendAheadTime)
        return inTransmitTime - fMaxSendAheadTime + fSendInterval;
        
    // during the first second just send packets normally
//    if (fPreviousSecondStart == -1)
//        return inCurrentTime + fSendInterval;
    
    // now figure if we want to send this packet during this bucket.  We have two limitations.
    // First we scale up bitrate slowly, so we should only try and send a little more than we
    // sent recently (averaged over a second or two).  However, we always try and send at
    // least the current bitrate and never more than double.
//    SInt32 currentBitRate = fBytesDuringBucket * 1000 / (inCurrentTime - fPreviousBucketBegin);
//    SInt32 averageBitRate = (fBytesDuringPreviousSecond + fBytesDuringLastSecond) * 1000 / (inCurrentTime - fPreviousSecondStart);
//    SInt32 averageBitRate = fBytesDuringPreviousSecond * 1000 / (fLastSecondStart - fPreviousSecondStart);

	/* ��ǰBucket interval����ǰ�ķ���ʱ��(������ʱ) */
	fBucketTimeAhead = inTransmitTime - inCurrentTime;
//	printf("Current br = %d, average br = %d (cta = %qd, pta = %qd)\n", currentBitRate, averageBitRate, currentTimeAhead, fPreviousBucketTimeAhead);
    
	//������γ�ǰʱ��С���ϴγ�ǰʱ��(�����ӳ��м�С����),��ֱ�ӷ��͸ð�
    // always try and stay as far ahead as we were before
	if (fBucketTimeAhead < fPreviousBucketTimeAhead)
        return -1;
      
	//���ΰ��ĳ�ʱ�����֮�ϴβ��ܹ���,������Ϊ�˱������ݷ��͵��ȶ���,���̫��(����һ�����ͼ��),���ӻ�һ�����ͼ���ٷ���
	/* ��Ҫ�Ը���2��Bitrate����,���ǽ���ָ��ʱ���Կ�һ�㷢�� */
    // but don't send at more that double the bitrate (for any given time we should only get further
    // ahead by that amount of time)
	//printf("cta - pta = %qd, ct - pbb = %qd\n", fBucketTimeAhead - fPreviousBucketTimeAhead, SInt64((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0)));
	if (fBucketTimeAhead - fPreviousBucketTimeAhead > ((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0)))
	{ 
		fBucketTimeAhead = fPreviousBucketTimeAhead + SInt64((inCurrentTime - fPreviousBucketBegin) * (fOverbufferRate - 1.0));
		return inCurrentTime + fSendInterval;		// this will get us to the next bucket
	}
        
	// don't send more than 10% over the average bitrate for the previous second
//    if (currentBitRate > averageBitRate * 11 / 10)
//        return inCurrentTime + fSendInterval;       // this will get us to the next bucket
    
    return -1;  // send this packet �����ͳ��ð�
}

/* RTPSession::Play() */
/* ����Overbuffering�е������ */
void RTPOverbufferWindow::ResetOverBufferWindow()
{
    fBytesDuringLastSecond = 0;
    fLastSecondStart = -1;
    fBytesDuringPreviousSecond = 0;
    fPreviousSecondStart = -1;
    fBytesDuringBucket = 0;
    fBucketBegin = 0;
    fBucketTimeAhead = 0;
    fPreviousBucketTimeAhead = 0;
	fOverbufferWindowBegin = -1;
}

/* used in RTPStream::Write() */
/* �Ե�ǰ�ͳ���RTP��,�õ�ǰ����С��������� */
void RTPOverbufferWindow::AddPacketToWindow(SInt32 inPacketSize)
{
    fBytesDuringBucket += inPacketSize;
    fBytesDuringLastSecond += inPacketSize;
    fBytesSentSinceLastReport += inPacketSize;
}

/* used in RTPStream::Write() */
void RTPOverbufferWindow::EmptyOutWindow(const SInt64& inCurrentTime)
{
    // no longer needed
}

/* used  in RTPStream::Setup()/RTPStream::ProcessIncomingRTCPPacket() */
/* ���������Overbuffering��Client���ڵĴ�С */
void RTPOverbufferWindow::SetWindowSize(UInt32 inWindowSizeInBytes)
{
    fWindowSize = inWindowSizeInBytes;
    fBytesSentSinceLastReport = 0;
}
