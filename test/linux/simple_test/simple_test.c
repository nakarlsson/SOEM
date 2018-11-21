/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : simple_test [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "ethercat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
boolean inOP;
uint8 currentgroup = 0;
OSAL_THREAD_HANDLE thread3;
uint8 txbuf[1024];

OSAL_THREAD_FUNC eoe_handler_rx(void *lpParam)
{
   ecx_contextt *context = (ecx_contextt *)lpParam;
   ec_mbxt * mbxout;
   int wkc;

   /** Current RX fragment number */
   uint8_t rxfragmentno = 0;
   /** Complete RX frame size of current frame */
   uint16_t rxframesize = 0;
   /** Current RX data offset in frame */
   uint16_t rxframeoffset = 0;
   /** Current RX frame number */
   uint16_t rxframeno = 0;
   uint8 rxbuf[1024];
   int size_of_rx = sizeof(rxbuf);

   for (;;)
   {
      /* */
      os_mbox_fetch(context->EoEmbxq, (void **)&mbxout, OS_WAIT_FOREVER);

      if (mbxout->slaveidx == 1)
      {
         /* Pass received Mbx data to EoE recevive fragement function that 
          * that will start/continue fill a Ethernet frame buffer
          */
         wkc = ecx_EOEreadfragment(&mbxout->data,
            &rxfragmentno,
            &rxframesize,
            &rxframeoffset,
            &rxframeno,
            &size_of_rx,
            rxbuf);
         /* wkc == 1 would mean a frame is complete , last fragement flag have been set and all 
          * other checks must have past
          */
         if (wkc > 0)
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
         }
         else
         {
            /* We're not dune yet, trigger a read request, to replace with other Mbx full
             * trogger mechanism??? FMMU of mxb full 
             */
            ec_mbxrecvq_post(mbxout->slaveidx, EC_TIMEOUTRXM);
         }
      }
      else
      {
         printf("Don't know what to do with EoE data for slave: %d\n", mbxout->slaveidx);
      }      
      free(mbxout);
   }
}

void test_eoe(void)
{
   /* Create dummy layer 2 frame to send over EoE */
   int ixme;
   ec_setupheader(&txbuf);
   for (ixme = ETH_HEADERSIZE; ixme < sizeof(txbuf); ixme++)
   {
      txbuf[ixme] = (uint8)rand();
   }

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

   /* Send a made up frame to trigger a fragmented transfer 
    * Used with a special bound impelmentaion of SOES. Will
    * trigger a fragmented transfer back of the same frame.
    */
   ec_EOEsend(1, 0, sizeof(txbuf), txbuf, EC_TIMEOUTRXM);
   /* Trigger an MBX read request, to be replaced by slave Mbx
   * full notification via polling of FMMU status process data
   */
   ec_mbxrecvq_post(1, EC_TIMEOUTRXM);

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
}

void simpletest(char *ifname)
{
   int i, j, oloop, iloop, chk;
   needlf = FALSE;
   inOP = FALSE;
   printf("Starting simple test\n");

   /* initialise SOEM, bind socket to ifname */
   if (ec_init(ifname))
   {
      printf("ec_init on %s succeeded.\n",ifname);
      /* find and auto-config slaves */

      /* Create a asyncronous EoE handled */
      osal_thread_create(&thread3, 128000, &eoe_handler_rx, &ecx_context);

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

         printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

         printf("Request operational state for all slaves\n");
         expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
         printf("Calculated workcounter %d\n", expectedWKC);
         ec_slave[0].state = EC_STATE_OPERATIONAL;
         /* send one valid process data to make outputs in slaves happy*/
         ec_send_processdata();
         ec_receive_processdata(EC_TIMEOUTRET);

         /* Simple EoE test */
         test_eoe();
   
         /* request OP state for all slaves */
         ec_writestate(0);

         chk = 40;
         /* wait for all slaves to reach OP state */
         do
         {
            ec_send_processdata();
            ec_receive_processdata(EC_TIMEOUTRET);
            ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
         }
         while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
         if (ec_slave[0].state == EC_STATE_OPERATIONAL )
         {
            printf("Operational state reached for all slaves.\n");
            inOP = TRUE;
                /* cyclic loop */
            for(i = 1; i <= 10000; i++)
            {
               ec_send_processdata();
               wkc = ec_receive_processdata(EC_TIMEOUTRET);

                    if(wkc >= expectedWKC)
                    {
                        printf("Processdata cycle %4d, WKC %d , O:", i, wkc);

                        for(j = 0 ; j < oloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].outputs + j));
                        }

                        printf(" I:");
                        for(j = 0 ; j < iloop; j++)
                        {
                            printf(" %2.2x", *(ec_slave[0].inputs + j));
                        }
                        printf(" T:%"PRId64"\r",ec_DCtime);
                        needlf = TRUE;
                    }
                    osal_usleep(5000);

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

OSAL_THREAD_FUNC ecatcheck( void *ptr )
{
    int slave;
    (void)ptr;                  /* Not used */

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

int main(int argc, char *argv[])
{
   printf("SOEM (Simple Open EtherCAT Master)\nSimple test\n");

   if (argc > 1)
   {
      /* create thread to handle slave error handling in OP */
//      pthread_create( &thread1, NULL, (void *) &ecatcheck, (void*) &ctime);
      osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime);
      /* start cyclic part */
      simpletest(argv[1]);
   }
   else
   {
      printf("Usage: simple_test ifname1\nifname = eth0 for example\n");
   }

   printf("End program\n");
   return (0);
}
