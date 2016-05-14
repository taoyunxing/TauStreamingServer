
/*************************************************************************** 

Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
2010-2020 DADI ORISTAR  TECHNOLOGY DEVELOPMENT(BEIJING)CO.,LTD

FileName:	 ev.cpp
Description: POSIX select implementation of MacOS X event queue functions.
Comment:     copy from Darwin Streaming Server 5.5.5
Author:		 taoyunxing@dadimedia.com
Version:	 v1.0.0.1
CreateDate:	 2010-08-16
LastUpdate:  2010-08-18

****************************************************************************/ 


#define EV_DEBUGGING 0 //Enables a lot of printfs

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#include "ev.h"
#include "OS.h"
#include "OSHeaders.h"
#include "OSThread.h"
#include "OSMutex.h"
#include "MyAssert.h"


static fd_set   sReadSet;
static fd_set   sWriteSet;
static fd_set   sReturnedReadSet;
static fd_set   sReturnedWriteSet;
static void**   sCookieArray = NULL;//Ϊÿ��sock fd���һ��event ID(��������ת��Ϊvoid *)
static int*     sFDsToCloseArray = NULL;/* Ҫ�رյ�fd���� */
static int      sPipes[2];/* �ܵ� */

static int      sCurrentFDPos = 0;/* ��ǰ�ļ������� */
static int      sMaxFDPos = 0;/* ����ļ�������,��ӳ���б�����������ļ������� */
static bool     sInReadSet = true;/* �ڶ�������? */
static int      sNumFDsBackFromSelect = 0;/* ��select()���ص�fd���� */
static UInt32   sNumFDsProcessed = 0;/* �Ѿ��������ļ����������� */
static OSMutex  sMaxFDPosMutex; /* �����socket fd��mutex */


static bool selecthasdata();
static int  constructeventreq(struct eventreq* req, int fd, int event);


void select_startevents()
{
    FD_ZERO(&sReadSet);
    FD_ZERO(&sWriteSet);
    FD_ZERO(&sReturnedReadSet);
    FD_ZERO(&sReturnedWriteSet);

    //We need to associate cookies (void*)'s with our file descriptors.
    //We do so by storing cookies in this cookie array. Because an fd_set is
    //a big array of bits, we should have as many entries in the array as
    //there are bits in the fd set  
    sCookieArray = new void*[sizeof(fd_set) * 8];
    ::memset(sCookieArray, 0, sizeof(void *) * sizeof(fd_set) * 8);
    
    //We need to close all fds from the select thread. Once an fd is passed into
    //removeevent, its added to this array so it may be deleted from the select thread
    sFDsToCloseArray = new int[sizeof(fd_set) * 8];
    for (int i = 0; i < (int) (sizeof(fd_set) * 8); i++)
        sFDsToCloseArray[i] = -1;
    
    //We need to wakeup select when the masks have changed. In order to do this,
    //we create a pipe that gets written to from modwatch, and read when select returns
    int theErr = ::pipe((int*)&sPipes);
    Assert(theErr == 0);
    
    //Add the read end of the pipe to the read mask
    FD_SET(sPipes[0], &sReadSet);
    sMaxFDPos = sPipes[0];
}

/* ɾ��ָ����socket fd,����sMaxFDPosMutex(��1),����socket fd����sFDsToCloseArray[],ͬʱд��pipe */
int select_removeevent(int which)
{

    {
        //Manipulating sMaxFDPos is not pre-emptive safe, so we have to wrap it in a mutex
        //I believe this is the only variable that is not preemptive safe....
        OSMutexLocker locker(&sMaxFDPosMutex);
        
    //Clear this fd out of both sets
        FD_CLR(which, &sWriteSet);
        FD_CLR(which, &sReadSet);
        
        FD_CLR(which, &sReturnedReadSet);
        FD_CLR(which, &sReturnedWriteSet);
    
        sCookieArray[which] = NULL; // Clear out the cookie
        /* ������ļ���������1 */
        if (which == sMaxFDPos)
        {
            //We've just deleted the highest numbered fd in our set,
            //so we need to recompute what the highest one is.
            while (!FD_ISSET(sMaxFDPos, &sReadSet) && !FD_ISSET(sMaxFDPos, &sWriteSet) &&
                (sMaxFDPos > 0))
                {
#if EV_DEBUGGING                  
                     qtss_printf("removeevent: reset MaxFDPos = %d to %d\n", sMaxFDPos , sMaxFDPos -1);
#endif                
                    sMaxFDPos--;
                }
        }

        //We also need to keep the mutex locked during any manipulation of the
        //sFDsToCloseArray, because it's definitely not preemptive safe.
            
        //put this fd into the fd's to close array, so that when select wakes up, it will
        //close the fd
		//����socket fdҲ���뽫ɾ����FD����,�Ա�select����ʱ�����ر�
        UInt32 theIndex = 0;
        while ((sFDsToCloseArray[theIndex] != -1) && (theIndex < sizeof(fd_set) * 8))
            theIndex++;
        Assert(sFDsToCloseArray[theIndex] == -1);
        sFDsToCloseArray[theIndex] = which;
#if EV_DEBUGGING
    qtss_printf("removeevent: Disabled %d \n", which);
#endif
    }
    
    //write to the pipe so that select wakes up and registers the new mask
	//��pipe��д��"p",�Ա�select����ʱע���µ�fd
    int theErr = ::write(sPipes[1], "p", 1);
    Assert(theErr == 1);

    return 0;
}

int select_watchevent(struct eventreq *req, int which)
{
    return select_modwatch(req, which);
}

/* ����EV_RE,��ָ��eventreq���ļ�������fd�������,��д����ɾ��;
   ����EV_WR,��ָ��eventreq���ļ�������fd����д��,�Ӷ�����ɾ��.
   ��������ļ�������sMaxFDPos,��¼�¼�ID req->er_data��sCookieArray[],дpipe
*/
int select_modwatch(struct eventreq *req, int which)
{
    {
        //Manipulating sMaxFDPos is not pre-emptive safe(����ռ��ȫ), so we have to wrap it in a mutex
        //I believe this is the only variable that is not preemptive safe....
        OSMutexLocker locker(&sMaxFDPosMutex);

        //Add or remove this fd from the specified sets
		/* ����EV_RE,��ָ���¼���fd�������,����Ӷ�������� */
        if (which & EV_RE)
        {
    #if EV_DEBUGGING
            qtss_printf("modwatch: Enabling %d in readset\n", req->er_handle);
    #endif
            FD_SET(req->er_handle, &sReadSet);
        }
        else
        {
    #if EV_DEBUGGING
            qtss_printf("modwatch: Disbling %d in readset\n", req->er_handle);
    #endif
            FD_CLR(req->er_handle, &sReadSet);
        }

		/* ����EV_WR,��ָ���¼���fd����д��,�����д������� */
        if (which & EV_WR)
        {
    #if EV_DEBUGGING
            qtss_printf("modwatch: Enabling %d in writeset\n", req->er_handle);
    #endif
            FD_SET(req->er_handle, &sWriteSet);
        }
        else
        {
    #if EV_DEBUGGING
            qtss_printf("modwatch: Disabling %d in writeset\n", req->er_handle);
    #endif
            FD_CLR(req->er_handle, &sWriteSet);
        }

		/* ��������ļ�������sMaxFDPos */
        if (req->er_handle > sMaxFDPos)
            sMaxFDPos = req->er_handle;

#if EV_DEBUGGING
        qtss_printf("modwatch: MaxFDPos=%d\n", sMaxFDPos);
#endif
        //
        // Also, modifying the cookie is not preemptive safe. This must be
        // done atomically wrt setting the fd in the set. Otherwise, it is
        // possible to have a NULL cookie on a fd.
        Assert(req->er_handle < (int)(sizeof(fd_set) * 8));
        Assert(req->er_data != NULL);//event ID
        sCookieArray[req->er_handle] = req->er_data;//ר�Ŵ��event ID
    }
    
    //write to the pipe so that select wakes up and registers the new mask
    int theErr = ::write(sPipes[1], "p", 1);
    Assert(theErr == 1);

    return 0;
}

/* �������fd��EV_RE������evetnreq */
int constructeventreq(struct eventreq* req, int fd, int event)
{
    req->er_handle = fd;/* ��select_waitevent()���ص�socket fd */
    req->er_eventbits = event;/* EV_RE */
    Assert(fd < (int)(sizeof(fd_set) * 8));
    req->er_data = sCookieArray[fd];/* �����fd��������� */
    sCurrentFDPos++;
    sNumFDsProcessed++;
    
    //don't want events on this fd until modwatch is called.
    FD_CLR(fd, &sWriteSet);
    FD_CLR(fd, &sReadSet);
    
    return 0;
}

/* ��������Ҫ�ĺ���:ʹ��select()���Ƽ���������rd/wr socket,��ȡ������event,�������select()����(������) */
int select_waitevent(struct eventreq *req, void* /*onlyForMacOSX*/)
{
    //Check to see if we still have some select descriptors to process
    int theFDsProcessed = (int)sNumFDsProcessed;
    bool isSet = false;
    
    if (theFDsProcessed < sNumFDsBackFromSelect)
    {
        if (sInReadSet)
        {
            OSMutexLocker locker(&sMaxFDPosMutex);
#if EV_DEBUGGING
            qtss_printf("waitevent: Looping through readset starting at %d\n", sCurrentFDPos);
#endif     /* ��sCurrentFDPosС��sMaxFDPosʱ����sCurrentFDPos���� */
            while((!(isSet = FD_ISSET(sCurrentFDPos, &sReturnedReadSet))) && (sCurrentFDPos < sMaxFDPos)) 
                sCurrentFDPos++;        
            /* ��sCurrentFDPos�ڷ��صĶ����� */
            if (isSet)
            {   
#if EV_DEBUGGING
                qtss_printf("waitevent: Found an fd: %d in readset max=%d\n", sCurrentFDPos, sMaxFDPos);
#endif          
                FD_CLR(sCurrentFDPos, &sReturnedReadSet);//�ӷ��صĶ�����ɾ����fd
				/* �����Ϣ�󽻸�EventThread::Entry����  */
                return constructeventreq(req, sCurrentFDPos, EV_RE);//���ص��ļ�����������req
            }
            else/* ��sCurrentFDPos���ڷ��صĶ����� */
            {
#if EV_DEBUGGING   /*  ֹͣ��������  */
                qtss_printf("waitevent: Stopping traverse of readset at %d\n", sCurrentFDPos);
#endif
                sInReadSet = false;
                sCurrentFDPos = 0;/* reset 0 */
            }
        }

		/* ���粻�ڶ�����,����д���б��� */
        if (!sInReadSet)
        {
            OSMutexLocker locker(&sMaxFDPosMutex);
#if EV_DEBUGGING
            qtss_printf("waitevent: Looping through writeset starting at %d\n", sCurrentFDPos);
#endif
            while((!(isSet = FD_ISSET(sCurrentFDPos, &sReturnedWriteSet))) && (sCurrentFDPos < sMaxFDPos))
                sCurrentFDPos++;
            /* ��sCurrentFDPos�ڷ��ص�д���� */
            if (isSet)
            {
#if EV_DEBUGGING
                qtss_printf("waitevent: Found an fd: %d in writeset\n", sCurrentFDPos);
#endif
                FD_CLR(sCurrentFDPos, &sReturnedWriteSet);//�ӷ��ص�д����ɾ����fd
                return constructeventreq(req, sCurrentFDPos, EV_WR);
            }
            else
            {
                // This can happen if another thread calls select_removeevent at just the right
                // time, setting sMaxFDPos lower than it was when select() was last called.
                // Becase sMaxFDPos is used as the place to stop iterating over the read & write
                // masks, setting it lower can cause file descriptors in the mask to get skipped.
                // If they are skipped, that's ok, because those file descriptors were removed
                // by select_removeevent anyway. We need to make sure to finish iterating over
                // the masks and call select again, which is why we set sNumFDsProcessed
                // artificially here.
                sNumFDsProcessed = sNumFDsBackFromSelect;//�����Ѿ���������select���ص�fd
                Assert(sNumFDsBackFromSelect > 0);
            }
        }
    }
    
	/* ������select���ص�fd,û�л��к�,�������еļ������� */
	// ����select_waitevent�������ú�ִ�е�����˵�������Ѿ����������ֶ��Ѵ���
    if (sNumFDsProcessed > 0)
    {
        OSMutexLocker locker(&sMaxFDPosMutex);
#if DEBUG
        //
        // In a very bizarre circumstance (sMaxFDPos goes down & then back up again, these
        // asserts could hit.
        //
        //for (int x = 0; x < sMaxFDPos; x++)
        //  Assert(!FD_ISSET(x, &sReturnedReadSet));
        //for (int y = 0; y < sMaxFDPos; y++)
        //  Assert(!FD_ISSET(y, &sReturnedWriteSet));
#endif  
#if EV_DEBUGGING /*  ֹͣ����д��  */
        qtss_printf("waitevent: Finished with all fds in set. Stopped traverse of writeset at %d maxFD = %d\n", sCurrentFDPos,sMaxFDPos);
#endif
        //We've just cycled through one select result. Re-init all the counting states
        sNumFDsProcessed = 0;
        sNumFDsBackFromSelect = 0;
        sCurrentFDPos = 0;
        sInReadSet = true;
    }
    
    
    /* ��selecthasdata()����ֵΪfalse,�ͷ�������select();���� */
    while(!selecthasdata())
    {
        {
            OSMutexLocker locker(&sMaxFDPosMutex);
            //Prepare to call select. Preserve the read and write sets by copying their contents
            //into the corresponding "returned" versions, and then pass those into select
            ::memcpy(&sReturnedReadSet, &sReadSet, sizeof(fd_set));
            ::memcpy(&sReturnedWriteSet, &sWriteSet, sizeof(fd_set));
        }

        SInt64  yieldDur = 0;
        SInt64  yieldStart;
        
        //Periodically time out the select call just in case we
        //are deaf for some reason
        // on platforw's where our threading is non-preemptive, just poll select
        //���ó�ʱʱ��
        struct timeval  tv;
        tv.tv_usec = 0;

    #if THREADING_IS_COOPERATIVE
        tv.tv_sec = 0;
        
        if ( yieldDur > 4 )
            tv.tv_usec = 0;
        else
            tv.tv_usec = 5000;
    #else
        tv.tv_sec = 15;//��ʱ15s
    #endif

#if EV_DEBUGGING
        qtss_printf("waitevent: about to call select\n");
#endif

        yieldStart = OS::Milliseconds();
        OSThread::ThreadYield();
        
        yieldDur = OS::Milliseconds() - yieldStart;
#if EV_DEBUGGING
        static SInt64   numZeroYields;
        
        if ( yieldDur > 1 )
        {
            qtss_printf( "select_waitevent time in OSThread::Yield() %i, numZeroYields %i\n", (long)yieldDur, (long)numZeroYields );
            numZeroYields = 0;
        }
        else
            numZeroYields++;

#endif
        /* ʹ��select()���Ƽ���������rd/wr socket */
        sNumFDsBackFromSelect = ::select(sMaxFDPos+1, &sReturnedReadSet, &sReturnedWriteSet, NULL, &tv);
		// select�ķ���ֵ�����֣�
		// ����-1����ζ�ų��ִ����������κ������־���֮ǰ�ͳ������źš�
		// ����0����ζ�����κ������־���֮ǰ����ʱ�ѵ��ڡ������ּ��ϱ���0
		// ��������������ָ���Ѿ����������ָ����������ּ�����ֻ�����Ѿ����������֡�
#if EV_DEBUGGING
        qtss_printf("waitevent: back from select. Result = %d\n", sNumFDsBackFromSelect);
#endif
    }
    

    if (sNumFDsBackFromSelect >= 0)
        return EINTR;   //either we've timed out or gotten some events. Either way, force caller
                        //to call waitevent again.�μ�EventThread::Entry()
    return sNumFDsBackFromSelect;//���ظ�ֵ
}

//��select()����ֵ���з���,ͬʱ����false(���µ���select),��true(�õ�һ��������event)
bool selecthasdata()
{
	/* ��select()����ֵ < 0 */
    if (sNumFDsBackFromSelect < 0)
    {
        int err=OSThread::GetErrno();
        
#if EV_DEBUGGING
        if (err == ENOENT) 
        {
             qtss_printf("selectHasdata: found error ENOENT==2 \n");
        }
#endif

        if (   
            err == EBADF || //this might happen if a fd is closed right before calling select
            err == EINTR 
           ) // this might happen if select gets interrupted
             return false;
        return true;//if there is an error from select, we want to make sure and return to the caller
    }
    /* ��select()����ֵ = 0 */    
    if (sNumFDsBackFromSelect == 0)
        return false;//if select returns 0, we've simply timed out, so recall select
    
	/* ��select()����ֵ > 0 */
	/* ����pipe�е�fd��select()���صĶ����� */
    if (FD_ISSET(sPipes[0], &sReturnedReadSet))
    {
#if EV_DEBUGGING
        qtss_printf("selecthasdata: Got some data on the pipe fd\n");
#endif
        //we've gotten data on the pipe file descriptor. Clear the data.
        // increasing the select buffer fixes a hanging problem when the Darwin server is under heavy load
        // CISCO contribution
		/* ��pipe�е�fd���뻺�� */
        char theBuffer[4096]; 
        (void)::read(sPipes[0], &theBuffer[0], 4096);

		/* �Ӷ��������pipe�е�fd */
        FD_CLR(sPipes[0], &sReturnedReadSet);
        sNumFDsBackFromSelect--;
        
		/* ��һ�ر�sFDsToCloseArray[]�е�fd */
        {
            //Check the fds to close array, and if there are any in it, close those descriptors
            OSMutexLocker locker(&sMaxFDPosMutex);
            for (UInt32 theIndex = 0; ((sFDsToCloseArray[theIndex] != -1) && (theIndex < sizeof(fd_set) * 8)); theIndex++)
            {
                (void)::close(sFDsToCloseArray[theIndex]);
                sFDsToCloseArray[theIndex] = -1;
            }
        }
    }
    Assert(!FD_ISSET(sPipes[0], &sReturnedWriteSet));//ȷ��pipe�е�fd����select()���ص�д����
    
    if (sNumFDsBackFromSelect == 0)
        return false;//if the pipe file descriptor is the ONLY data we've gotten, recall select
    else
        return true;//we've gotten a real event, return that to the caller
}
