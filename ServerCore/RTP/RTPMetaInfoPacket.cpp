/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 RTPMetaInfoPacket.cpp
Description: Some defs for RTP-Meta-Info payloads. This class also parses RTP meta info packets.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-16

****************************************************************************/ 



#include "RTPMetaInfoPacket.h"
#include "MyAssert.h"
#include "StringParser.h"
#include "OS.h"
#include <string.h>
#include <netinet/in.h>


/* ������Ҫ�����ݳ�Ա���� */
const RTPMetaInfoPacket::FieldName RTPMetaInfoPacket::kFieldNameMap[] =
{
    TW0_CHARS_TO_INT('p', 'p'),
    TW0_CHARS_TO_INT('t', 't'),
    TW0_CHARS_TO_INT('f', 't'),
    TW0_CHARS_TO_INT('p', 'n'),
    TW0_CHARS_TO_INT('s', 'q'),
    TW0_CHARS_TO_INT('m', 'd')
};

/* used in RTPMetaInfoPacket::ParsePacket() */
const UInt32 RTPMetaInfoPacket::kFieldLengthValidator[] =
{
    8,  //pp
    8,  //tt
    2,  //ft
    8,  //pn
    2,  //sq
    0,  //md
    0   //illegal / unknown
};


/* ��FieldName��ö�Ӧ��FieldIndex,�õ�RTPMetaInfoPacket::kFieldNameMap[]���� */
RTPMetaInfoPacket::FieldIndex RTPMetaInfoPacket::GetFieldIndexForName(FieldName inName)
{
    for (int x = 0; x < kNumFields; x++)
    {
        if (inName == kFieldNameMap[x])
            return x;
    }
    return kIllegalField;
}


/* �����RTP������Ԫ��Ϣ�ı�ͷ����ȡ������ݴ���FieldIDArray */
void RTPMetaInfoPacket::ConstructFieldIDArrayFromHeader(StrPtrLen* inHeader, FieldID* ioFieldIDArray)
{
	/* �ȳ�ʼ������ioFieldIDArray[]�ĸ�������SInt32,��6������ */
    for (UInt32 x = 0; x < kNumFields; x++)
        ioFieldIDArray[x] = kFieldNotUsed;
    
    //
    // Walk through the fields in this header
	/* ���ݽ���ε�ֵ��������ȡ������ݴ���FieldIDArray */
    StringParser theParser(inHeader);
    
    UInt16 fieldNameValue = 0;

    while (theParser.GetDataRemaining() > 0)
    {
        StrPtrLen theFieldP;
		/* ָ��fStartGet����';'ʱ���������ַ�����theFieldP,��Խ�� ';' */
        (void)theParser.GetThru(&theFieldP, ';');
        
        //
        // Corrupt or something... just bail
		/* ÿ��Field��������Ϊ3(2���ֽڵ�fieldNameValue(UInt16)+"="+FieldID(SInt32)+":"),�μ�QTSSModuleUtils::AppendRTPMetaInfoHeader()�õ� */
        if (theFieldP.Len < 2)
            break;
        
        //
        // Extract the Field Name and convert it to a Field Index
        ::memcpy (&fieldNameValue, theFieldP.Ptr, sizeof(UInt16));
        FieldIndex theIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(fieldNameValue));
//      FieldIndex theIndex = RTPMetaInfoPacket::GetFieldIndexForName(ntohs(*(UInt16*)theFieldP.Ptr));

        //
        // Get the Field ID if there is one.

		/* temp field ID with initial value of each Field */
        FieldID theID = kUncompressed;
		/* ����ȡ�õ�field����>3,�ͽ�һ���ֽ�����ȡField ID */
        if (theFieldP.Len > 3)
        {
            StringParser theIDExtractor(&theFieldP);
			/* ָ��fStartGet�Ⱦ���3���ֽ� */
            theIDExtractor.ConsumeLength(NULL, 3);
			/* ��ȡ��������� */
            theID = theIDExtractor.ConsumeInteger(NULL);
        }
        
		/* �����Ϸ�Field�Ÿ���Field ID */
        if (theIndex != kIllegalField)
            ioFieldIDArray[theIndex] = theID;
    }
}

/* �������inPacketBuffer�е�����,������Field��ֵ,��Բ�ͬ����������Ӧ���ݳ�Ա��ֵ(��6��) */
Bool16 RTPMetaInfoPacket::ParsePacket(UInt8* inPacketBuffer, UInt32 inPacketLen, FieldID* inFieldIDArray)
{
	/* ��Ǵ�RTPMetaInfoPacket�ڻ����е������յ�ָ�� */
    UInt8* theFieldP = inPacketBuffer + 12; // skip RTP header
    UInt8* theEndP = inPacketBuffer + inPacketLen;
    
	/* temp variables */
    SInt64 sInt64Val = 0;
    UInt16 uInt16Val = 0;
    
	/* ����inPacketBuffer�е�����,��Բ�ͬ����������Ӧ���ݳ�Ա��ֵ */
    while (theFieldP < (theEndP - 2))
    {
        FieldIndex theFieldIndex = kIllegalField;
        UInt32 theFieldLen = 0;
		/* ȡǰ�����ֽڷ�������,���QTSS API Doc."���Ļ���" */
		/* ע�����standard RTP Metal Info�в���Field Name���� */
        FieldName* theFieldName = (FieldName*)theFieldP;//UInt16
        
		/* ��Ϊѹ����ʽ��RTPԪ����ʱ(ǰ���ֽڵĵ�һ��bitΪ1����0) */
        if (*theFieldName & 0x8000)
        {
			/* �����inFieldIDArray���Ҫ�� */
            Assert(inFieldIDArray != NULL);
            
            // If this is a compressed field, find to which field the ID maps
			/* ȡ��ͷ�����ֽڵ�2-8bit(��һ��bitΪ1),������FieldID */
            UInt8 theFieldID = *theFieldP & 0x7F;
             
			/* �������FieldID��Ϊ������FieldIDArray(����Ҫ�����inFieldIDArray�ǿ�)�����ж�Ӧ������(theFieldIndex),
			   ��0��5����,ע�ⲻ����RTPMetaInfoPacket::FieldIndex RTPMetaInfoPacket::GetFieldIndexForName()һ���㶨,
			   ��Ϊ��FieldID,����FieldName,���ǻ���˼������ͬ�� */
            for (int x = 0; x < kNumFields; x++)
            {
                if (theFieldID == inFieldIDArray[x])
                {
                    theFieldIndex = x;
                    break;
                }
            }
            
			/* ȡ���ڶ����ֽ���Ϊ����FieldLen,ע��theFieldP����UInt8* */
            theFieldLen = *(theFieldP + 1);
			/* ��ָ���Ƶ�Field Dataλ�� */
            theFieldP += 2;
        }
        else /* ��Ϊstandard(uncompressed)RTP Mata Info����ʱ */
        {
            // This is not a compressed field. Make sure there is enough room
            // in the packet for this to make sense
            if (theFieldP >= (theEndP - 4))
                break;

			/* �õ�FieldName */
            ::memcpy(&uInt16Val, theFieldP, sizeof(uInt16Val));
			/* �õ�FieldName��Ӧ��FieldIndex */
            theFieldIndex = this->GetFieldIndexForName(ntohs(uInt16Val));
            
			/* �õ�Field length */
            ::memcpy(&uInt16Val, theFieldP + 2, sizeof(uInt16Val));
            theFieldLen = ntohs(uInt16Val);
			/* ��ָ���Ƶ�Field Dataλ�ô� */
            theFieldP += 4;
        }
        
        //
        // Validate the length of this field if possible.
        // If the field is of the wrong length, return an error.
		/* ��������Field length�Ƿ���ָ���Ĵ�С(8/2/0�ֽ�)? */
        if ((kFieldLengthValidator[theFieldIndex] > 0) &&
            (kFieldLengthValidator[theFieldIndex] != theFieldLen))
            return false;
		/* ����Ƿ��ڴ�Խ��? */
        if ((theFieldP + theFieldLen) > theEndP)
            return false;
            
        //
        // We now know what field we are dealing with, so store off
        // the proper value depending on the field
		/* ���ݲ�ͬ��Field������Ӧ���ݳ�Ա��ֵ,ע��theFieldP����λ��Field Dataλ�ô� */
        switch (theFieldIndex)
        {
            case kPacketPosField:
            {
				/* ��ȡ8�ֽڵĸ�Fieldʵ�ʵ�Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
				/* ��ת�������õ�fPacketPosition */
                fPacketPosition = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kTransTimeField:
            {
				/* ��ȡ8�ֽڵĸ�Fieldʵ�ʵ�Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
				/* ��ת�������õ�fTransmitTime */
                fTransmitTime = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kFrameTypeField:
            {
				/* ��Ҫת��,ֱ�ӵõ�fFrameType */
                fFrameType = ntohs(*((FrameTypeField*)theFieldP));
                break;
            }
            case kPacketNumField:
            {
				/* ��ȡ8�ֽڵĸ�Fieldʵ�ʵ�Data */
                ::memcpy(&sInt64Val, theFieldP, sizeof(sInt64Val));
                fPacketNumber = (UInt64)OS::NetworkToHostSInt64(sInt64Val);
                break;
            }
            case kSeqNumField:
            {
                /* ��ȡ2�ֽڵĸ�Fieldʵ�ʵ�Data */
                ::memcpy(&uInt16Val, theFieldP, sizeof(uInt16Val));
                fSeqNum = ntohs(uInt16Val);
                break;
            }
            case kMediaDataField:
            {
				/* ֱ�ӻ��FieldP�����ĳ���,��������ǰ��������׼�� */
                fMediaDataP = theFieldP;
                fMediaDataLen = theFieldLen;
                break;
            }
            default:
                break;
        }
        
        //
        // Skip onto the next field
		/* ָ���ƹ�Field��data����,������һ����Ŀ�ͷ */
        theFieldP += theFieldLen;
    }
    return true;
}

/* ��RTPMetaInfoPacketת���RTP Packet.����:RTPMetaInfoPacket��RTP Packet��ϵ����? ����ʼǷ��� */
UInt8* RTPMetaInfoPacket::MakeRTPPacket(UInt32* outPacketLen)
{
	/* ȷ��Ҫ��media data */
    if (fMediaDataP == NULL)
        return NULL;
    
    //
    // Just move the RTP header to right before the media data.
	/* ��RTP header Info(12�ֽ�)����Media dataǰ�� */
    ::memmove(fMediaDataP - 12, fPacketBuffer, 12);
    
    //
    // Report the length of the resulting RTP packet 
	/* ��¼��������RTP��λ��? */
    if (outPacketLen != NULL)
        *outPacketLen = fMediaDataLen + 12;
    
	/* �����µ�RTP Packet����ʼλ�� */
    return fMediaDataP - 12;
}


