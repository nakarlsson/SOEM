/** \file
* \brief Example code for Simple Open EtherCAT master EoE
*
* This example will run the follwing EoE functions
* SetIP
* GetIP
*
* Loop
*    Send fragment data (Layer 2 0x88A4)
*    Receive fragment data (Layer 2 0x88A4)
*
* For this to work, a special slave test code is running that
* will bounce the sent 0x88A4 back to receive.
*
* Usage : eoe_test [ifname1]
* ifname is NIC interface, f.e. eth0
*
* This is a minimal test.
*
* (c)Andreas Karlsson 2018
*/
#include <stdio.h>
#include <string.h>
#include "osal.h"
#include "ethercat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
volatile int rtcnt;
boolean inOP;
uint8 currentgroup = 0;
ec_mbxbuft mbx[32];
uint8 txbuf[1024];

/** Current RX fragment number */
uint8_t rxfragmentno = 0;
/** Complete RX frame size of current frame */
uint16_t rxframesize = 0;
/** Current RX data offset in frame */
uint16_t rxframeoffset = 0;
/** Current RX frame number */
uint16_t rxframeno = 0;
uint8 rxbuf[1024];

uint16 eoe_slave = 1;
uint16 eoe_frame_send_and_read = 0;

/** registered EoE hook */
int eoe_hook(ecx_contextt * context, uint16 slave, void * eoembx)
{
   int size_of_rx = sizeof(rxbuf);
   int eoe_hook_wkc;
   /* Pass received Mbx data to EoE recevive fragment function that
   * that will start/continue fill an Ethernet frame buffer
   */
   size_of_rx = sizeof(rxbuf);
   eoe_hook_wkc = ecx_EOEreadfragment(eoembx,
      &rxfragmentno,
      &rxframesize,
      &rxframeoffset,
      &rxframeno,
      &size_of_rx,
      rxbuf);

   printf("Read frameno %d, fragmentno %d\n", rxframeno, rxfragmentno);

   /* wkc == 1 would mean a frame is complete , last fragement flag have been set and all
   * other checks must have past
   */
   if (eoe_hook_wkc > 0)
   {
      ec_etherheadert *bp = (ec_etherheadert *)rxbuf;
      uint16 type = ntohs(bp->etype);
      printf("Frameno %d, type 0x%x complete\n", rxframeno, type);
      if (type == ETH_P_ECAT)
      {
         /* Sanity check that received buffer still is OK */
         if (sizeof(txbuf) != size_of_rx)
         {
            printf("Size differs, expected %d , received %d\n", sizeof(txbuf), size_of_rx);
         }
         else
         {
            printf("Size OK, expected %d , received %d\n", sizeof(txbuf), size_of_rx);
         }
         /* Check that the TX and RX frames are EQ */
         if (memcmp(rxbuf, txbuf, size_of_rx))
         {
            printf("memcmp result != 0\n");
         }
         else
         {
            printf("memcmp result == 0\n");
         }
         eoe_frame_send_and_read = 0;
      }
      else
      {
         printf("Skip type 0x%x\n", type);
      }
   }

   return (eoe_hook_wkc >= 0) ? 1 : 0;
}

void setup_eoe(ecx_contextt * context)
{
   int ixme;

   /* Set the HOOK */
   (void)ecx_EOEdefinehook(context, eoe_hook);

   eoe_param_t ipsettings, re_ipsettings;
   memset(&ipsettings, 0, sizeof(ipsettings));
   memset(&re_ipsettings, 0, sizeof(re_ipsettings));

   ipsettings.ip_set = 1;
   ipsettings.subnet_set = 1;
   ipsettings.default_gateway_set = 1;

   EOE_IP4_ADDR_TO_U32(&ipsettings.ip, 192, 168, 9, 200);
   EOE_IP4_ADDR_TO_U32(&ipsettings.subnet, 255, 255, 255, 0);
   EOE_IP4_ADDR_TO_U32(&ipsettings.default_gateway, 0, 0, 0, 0);

   /* Send a set IP request */
   ec_EOEsetIp(1, 0, &ipsettings, EC_TIMEOUTRXM);

   /* Send a get IP request, should return the expected IP back */
   ec_EOEgetIp(1, 0, &re_ipsettings, EC_TIMEOUTRXM);

   printf("recieved IP (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.ip),
      eoe_ip4_addr2(&re_ipsettings.ip),
      eoe_ip4_addr3(&re_ipsettings.ip),
      eoe_ip4_addr4(&re_ipsettings.ip));
   printf("recieved subnet (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.subnet),
      eoe_ip4_addr2(&re_ipsettings.subnet),
      eoe_ip4_addr3(&re_ipsettings.subnet),
      eoe_ip4_addr4(&re_ipsettings.subnet));
   printf("recieved gateway (%d.%d.%d.%d)\n",
      eoe_ip4_addr1(&re_ipsettings.default_gateway),
      eoe_ip4_addr2(&re_ipsettings.default_gateway),
      eoe_ip4_addr3(&re_ipsettings.default_gateway),
      eoe_ip4_addr4(&re_ipsettings.default_gateway));

   /* Send a made up frame to trigger a fragmented transfer
   * Used with a special bounce implementaion of SOES. Will
   * trigger a fragmented transfer back of the same frame.
   */
   ec_setupheader(&txbuf);
   for (ixme = ETH_HEADERSIZE; ixme < sizeof(txbuf); ixme++)
   {
      txbuf[ixme] = (uint8)rand();
   }
   ec_EOEsend(1, 0, sizeof(txbuf), txbuf, EC_TIMEOUTRXM);
   eoe_frame_send_and_read = 1;
}

/* most basic RT thread for process data, just does IO transfer */
void CALLBACK RTthread(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,  DWORD_PTR dw2)
{
    IOmap[0]++;
    ec_send_processdata();
    wkc = ec_receive_processdata(EC_TIMEOUTRET);
    ecx_mbxhandler(&ecx_context, 0, 4);
    rtcnt++;
    /* do RT control stuff here */
}

void eoe_test(char *ifname)
{
   int i, j, oloop, iloop, wkc_count, chk, ixme, eoe_send_wkc;
   UINT mmResult;
   int sc = 0;
   uint16 mbxsl = 0;

   needlf = FALSE;
   inOP = FALSE;

   printf("Starting simple test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */


       if ( ec_config_init(FALSE) > 0 )
       {
         printf("%d slaves found and configured.\n",ec_slavecount);
            
         ec_config_map(&IOmap);
         ec_configdc();

         printf("Slaves mapped, state to SAFE_OP.\n");
         /* wait for all slaves to reach SAFE_OP state */
         ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

         oloop = ec_slave[0].Obytes;
         if ((oloop == 0) && (ec_slave[0].Obits > 0)) oloop = 1;
         if (oloop > 8) oloop = 8;
         iloop = ec_slave[0].Ibytes;
         if ((iloop == 0) && (ec_slave[0].Ibits > 0)) iloop = 1;
         if (iloop > 8) iloop = 8;

         printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments,
            ec_group[0].IOsegment[0],
            ec_group[0].IOsegment[1],
            ec_group[0].IOsegment[2],
            ec_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);

         /* start RT thread as periodic MM timer */
         mmResult = timeSetEvent(1, 0, RTthread, 0, TIME_PERIODIC);

         for(i = 1; i <= ec_slavecount; i++)
         {
            if(ec_slave[i].mbx_l > 0)
            {
//               printf("CoE slave handler\r\n");
               if(!mbxsl) mbxsl = i;
               ecx_slavembxcyclic(&ecx_context, i);
               if (ec_slave[i].mbx_proto & ECT_MBXPROT_EOE)
               {
                  ecx_EOEslavembxcyclic(&ecx_context, i);
               }
            }
         }

         /* Init EOE */
         setup_eoe(&ecx_context);

         /* request OP state for all slaves */
         ec_writestate(0);
         chk = 40;
         /* wait for all slaves to reach OP state */ 
         do
         {
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            wkc_count = 0;
            inOP = TRUE;

            /* cyclic loop, reads data from RT thread */
            for(i = 1; i <= 500; i++)
            {
                    if(wkc >= expectedWKC)
                    {
                        printf("Processdata cycle %4d, WKC %d , O:", rtcnt, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].outputs + j));
                        }

                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].inputs + j));
                        }
                        printf(" T:%lld\r",ec_DCtime);
                        needlf = TRUE;
                    }          
                    /* Continue sending frames when frame have been bounced back 
                     * and verified
                     */
                    if (eoe_frame_send_and_read == 0)
                    {
                       /* Send a new frame */
                       for (ixme = ETH_HEADERSIZE; ixme < sizeof(txbuf); ixme++)
                       {
                          txbuf[ixme] = (uint8)rand();
                       }
                       printf("Send a new frame\n");
                       eoe_send_wkc = ec_EOEsend(1, 0, sizeof(txbuf), txbuf, EC_TIMEOUTRXM);
                       if (eoe_send_wkc <= 0)
                       {
                          printf("ec_EOEsend frame send filed!\n");
                       }
                       
                       eoe_frame_send_and_read = 1;

                    }
                    osal_usleep(50000);
            }
            inOP = FALSE;
         }
         else
         {
                printf("Not all slaves reached operational state.\n");
                ec_readstate();
                for(i = 1; i<=ec_slavecount ; i++)
                {
                    if(ec_slave[i].state != EC_STATE_OPERATIONAL)
                    {
                        printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
                            i, ec_slave[i].state, ec_slave[i].ALstatuscode, ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
                    }
                }
         }
         /* stop RT thread */
         timeKillEvent(mmResult);
         printf("\nRequest init state for all slaves\n");
         ec_slave[0].state = EC_STATE_INIT;
         /* request INIT state for all slaves */
         ec_writestate(0);
        }
        else
        {
            printf("No slaves found!\n");
        }
        printf("End simple test, close socket\n");
        /* stop SOEM, close socket */
        ec_close();
    }
    else
    {
        printf("No socket connection on %s\nExcecute as root\n",ifname);
    }
}

//DWORD WINAPI ecatcheck( LPVOID lpParam )
OSAL_THREAD_FUNC ecatcheck(void *lpParam)
{
    int slave;

    while(1)
    {
        if( inOP && ((wkc < expectedWKC) || ec_group[currentgroup].docheckstate))
        {
            if (needlf)
            {
               needlf = FALSE;
               printf("\n");
            }
            /* one ore more slaves are not responding */
            ec_group[currentgroup].docheckstate = FALSE;
            ec_readstate();
            for (slave = 1; slave <= ec_slavecount; slave++)
            {
               if ((ec_slave[slave].group == currentgroup) && (ec_slave[slave].state != EC_STATE_OPERATIONAL))
               {
                  ec_group[currentgroup].docheckstate = TRUE;
                  if (ec_slave[slave].state == (EC_STATE_SAFE_OP + EC_STATE_ERROR))
                  {
                     printf("ERROR : slave %d is in SAFE_OP + ERROR, attempting ack.\n", slave);
                     ec_slave[slave].state = (EC_STATE_SAFE_OP + EC_STATE_ACK);
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state == EC_STATE_SAFE_OP)
                  {
                     printf("WARNING : slave %d is in SAFE_OP, change to OPERATIONAL.\n", slave);
                     ec_slave[slave].state = EC_STATE_OPERATIONAL;
                     ec_writestate(slave);
                  }
                  else if(ec_slave[slave].state > EC_STATE_NONE)
                  {
                     if (ec_reconfig_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d reconfigured\n",slave);
                     }
                  }
                  else if(!ec_slave[slave].islost)
                  {
                     /* re-check state */
                     ec_statecheck(slave, EC_STATE_OPERATIONAL, EC_TIMEOUTRET);
                     if (ec_slave[slave].state == EC_STATE_NONE)
                     {
                        ec_slave[slave].islost = TRUE;
                        printf("ERROR : slave %d lost\n",slave);
                     }
                  }
               }
               if (ec_slave[slave].islost)
               {
                  if(ec_slave[slave].state == EC_STATE_NONE)
                  {
                     if (ec_recover_slave(slave, EC_TIMEOUTMON))
                     {
                        ec_slave[slave].islost = FALSE;
                        printf("MESSAGE : slave %d recovered\n",slave);
                     }
                  }
                  else
                  {
                     ec_slave[slave].islost = FALSE;
                     printf("MESSAGE : slave %d found\n",slave);
                  }
               }
            }
            if(!ec_group[currentgroup].docheckstate)
               printf("OK : all slaves resumed OPERATIONAL.\n");
        }
        osal_usleep(10000);
    }
}

char ifbuf[1024];

int main(int argc, char *argv[])
{
   ec_adaptert * adapter = NULL;
   printf("SOEM (Simple Open EtherCAT Master)\nEOE test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
      osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime);
      strcpy(ifbuf, argv[1]);
      /* start cyclic part */
      eoe_test(ifbuf);
   }
   else
   {
      printf("Usage: simple_test ifname1\n");
   	/* Print the list */
      printf ("Available adapters\n");
      adapter = ec_find_adapters ();
      while (adapter != NULL)
      {
         printf ("Description : %s, Device to use for wpcap: %s\n", adapter->desc,adapter->name);
         adapter = adapter->next;
      }
   }

   printf("End program\n");
   return (0);
}
