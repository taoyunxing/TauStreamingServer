
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPMetaInfoPacket.h
Description: Some defs for RTP-Meta-Info payloads. This class also parses RTP meta info packets.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 


#ifndef __QTRTP_META_INFO_PACKET_H__
#define __QTRTP_META_INFO_PACKET_H__

#include <stdlib.h>
#include "SafeStdLib.h"
#include "OSHeaders.h"
#include "StrPtrLen.h"

class RTPMetaInfoPacket
{
    public:
    
        //
        // RTP Meta Info Fields
        
		/* the following fields occupy 2 bytes each */
		/* ������FieldIndex�Ķ��� */
        enum
        {
            kPacketPosField         = 0, //TW0_CHARS_TO_INT('p', 'p'),
            kTransTimeField         = 1, //TW0_CHARS_TO_INT('t', 't'),
            kFrameTypeField         = 2, //TW0_CHARS_TO_INT('f', 't'),
            kPacketNumField         = 3, //TW0_CHARS_TO_INT('p', 'n'),
            kSeqNumField            = 4, //TW0_CHARS_TO_INT('s', 'q'),
            kMediaDataField         = 5, //TW0_CHARS_TO_INT('m', 'd'),
            
            kIllegalField           = 6,
            kNumFields              = 6
        };
        typedef UInt16 FieldIndex;
        
        //
        // Types
        
        typedef UInt16 FieldName;
        typedef SInt32 FieldID; /* cited by QTHintTrack.h,QTSSModuleUtils::AppendRTPMetaInfoHeader() */
        
        //
        // Special field IDs
		/* �μ�QTSSModuleUtils::AppendRTPMetaInfoHeader(),��ʾFieldIDArray�ķ���ֵ */
        enum
        {
			/* �ڴ���FieldIDʱ,���Բ���ParsePacket()�еķ����ж����Field�Ƿ�compressed,�μ�QTHintTrack::WriteMetaInfoField() */
            kUncompressed = -1,     // No field ID (not a compressed field) in a RTP Meta Info field 
            kFieldNotUsed = -2      // This field is not being used,this is the initial value of Field ID Array
        };
        
        //
        // Routine that converts the above enum items into real field names
        static FieldName GetFieldNameForIndex(FieldIndex inIndex) { return kFieldNameMap[inIndex]; }
        static FieldIndex GetFieldIndexForName(FieldName inName);
        
        //
        // Routine that constructs a standard FieldID Array out of a x-RTP-Meta-Info header
		/* �μ�QTSSModuleUtils::AppendRTPMetaInfoHeader() */
		/* �����RTP������Ԫ��Ϣ�ı�ͷ����ȡ������ݴ���FieldIDArray */
        static void ConstructFieldIDArrayFromHeader(StrPtrLen* inHeader, FieldID* ioFieldIDArray);
        
        //
        // Values for the Frame Type Field  
        enum
        {
            kUnknownFrameType   = 0,
            kKeyFrameType       = 1, /* this we want */
            kBFrameType         = 2,
            kPFrameType         = 3
        };
        typedef UInt16 FrameTypeField; /* very important */
        
        
        //
        // CONSTRUCTOR
        
        RTPMetaInfoPacket() :   fPacketBuffer(NULL),
                                fPacketLen(0),
                                fTransmitTime(0),
                                fFrameType(kUnknownFrameType),
                                fPacketNumber(0),
                                fPacketPosition(0),
                                fMediaDataP(NULL),
                                fMediaDataLen(0),
                                fSeqNum(0)          {}
        ~RTPMetaInfoPacket() {}
        
        //
        // Call this to parse the RTP-Meta-Info packet.
        // Pass in an array of FieldIDs, make sure it is kNumFields(see above) in length.
        // This function will use the array as a guide to tell which field IDs in the
        // packet refer to which fields.
		/* �������inPacketBuffer�е�����,������Field��ֵ,��Բ�ͬ����������Ӧ���ݳ�Ա��ֵ */
        Bool16  ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen, FieldID* inFieldIDArray);
        
        //
        // Call this if you would like to rewrite the Meta-Info packet
        // as a normal RTP packet (strip off the extensions). Note that
        // this will overwrite data in the buffer!
        // Returns a pointer to the new RTP packet, and its length
        UInt8*          MakeRTPPacket(UInt32* outPacketLen);
        
        //
        // Field Accessors
        SInt64          GetTransmitTime()       { return fTransmitTime; }
        FrameTypeField  GetFrameType()          { return fFrameType; }   /* very important */
        UInt64          GetPacketNumber()       { return fPacketNumber; }
        UInt64          GetPacketPosition()     { return fPacketPosition; }
        UInt8*          GetMediaDataP()         { return fMediaDataP; }
        UInt32          GetMediaDataLen()       { return fMediaDataLen; }
        UInt16          GetSeqNum()             { return fSeqNum; }
    
    private:
    
		/* RTP Meta Info Packet�Ļ������ͳ���,ע���RTP Packet�󲢰���RTP Packet */
        UInt8*          fPacketBuffer;
        UInt32          fPacketLen;
        
		/* 6 important elements in RTPMataInfo packet */
        SInt64          fTransmitTime;
        FrameTypeField  fFrameType; /* this we really want */
        UInt64          fPacketNumber;
        UInt64          fPacketPosition;

		/* ������������һ�� */
        UInt8*          fMediaDataP; /* pointer to media data */
        UInt32          fMediaDataLen;

        UInt16          fSeqNum;
        
		/* the following ���� are defined *.cpp */
		/* ע�⻹��һ������ FieldID* kFieldIDArray,�������Ϊ�������� */
        static const FieldName kFieldNameMap[];
        static const UInt32 kFieldLengthValidator[];
};

#endif // __QTRTP_META_INFO_PACKET_H__
