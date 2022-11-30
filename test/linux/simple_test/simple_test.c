/** \file
 * \brief Example code for Simple Open EtherCAT master
 *
 * Usage : register_reader [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * This is a minimal test.
 *
 * (c)Arthur Ketels 2010 - 2011
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#include "ethercat.h"

#define EC_TIMEOUTMON 500

char IOmap[4096];
OSAL_THREAD_HANDLE thread1;
int expectedWKC;
boolean needlf;
volatile int wkc;
int wkc_last_cycle = 0;
boolean inOP;
uint8 currentgroup = 0;
uint8 register_buffer[8] = {0};
boolean clearLinkLossCounters = 0;

OSAL_THREAD_FUNC register_reader( void *ifname )
{
	int i;
	int chk;
	needlf = FALSE;
	inOP = FALSE;

	printf("Starting register_reader\n");

	/* initialise SOEM, bind socket to ifname */
	if (ec_init((char *)ifname)) {
		printf("ec_init on %s succeeded.\n",(char *)ifname);
		/* find and auto-config slaves */

		if ( ec_config_init(FALSE) > 0 ) {
			printf("%d slaves found and configured.\n",ec_slavecount);

            if(clearLinkLossCounters)
            {
                printf("Clear link loss counters\n");
                uint8_t llcnt[4];
                llcnt[0] = llcnt[1] = llcnt[2] = llcnt[3] = 0;
                /* Reset all slaves linkloss counter */
                ec_BWR(0x0000, ECT_REG_LLCNT, sizeof(llcnt), &llcnt, EC_TIMEOUTRET3);
            }
            
			ec_config_map(&IOmap);
			ec_configdc();

			printf("Slaves mapped, state to SAFE_OP.\n");
			/* wait for all slaves to reach SAFE_OP state */
			ec_statecheck(0, EC_STATE_SAFE_OP,  EC_TIMEOUTSTATE * 4);

			// printf("segments : %d : %d %d %d %d\n",ec_group[0].nsegments ,ec_group[0].IOsegment[0],ec_group[0].IOsegment[1],ec_group[0].IOsegment[2],ec_group[0].IOsegment[3]);

			printf("Request operational state for all slaves\n");
			expectedWKC = (ec_group[0].outputsWKC * 2) + ec_group[0].inputsWKC;
			printf("Calculated workcounter %d\n", expectedWKC);
			ec_slave[0].state = EC_STATE_OPERATIONAL;
			// /* send one valid process data to make outputs in slaves happy*/
			ec_send_processdata();
			ec_receive_processdata(EC_TIMEOUTRET);
			/* request OP state for all slaves */
			ec_writestate(0);
			chk = 200;
			/* wait for all slaves to reach OP state */
			do {
				ec_send_processdata();
				ec_receive_processdata(EC_TIMEOUTRET);
				ec_statecheck(0, EC_STATE_OPERATIONAL, 50000);
			}
			while (chk-- && (ec_slave[0].state != EC_STATE_OPERATIONAL));
			
			if (ec_slave[0].state == EC_STATE_OPERATIONAL ) {
				printf("Operational state reached for all slaves.\n");
				inOP = TRUE;
					 /* cyclic loop */
				for(i = 1; i > -1; i = (i % (1 << 16) + 1)) {
					// if(i < 100)
					// 	ec_send_processdata();
					// if(i > 120)
						ec_send_processdata();
					wkc = ec_receive_processdata(20 * 1000);

					uint16_t adr;
					if(wkc_last_cycle != wkc) {
						time_t current_time = time(NULL);
						char* time_str = ctime(&current_time);
						time_str[strlen(time_str)-1] = '\0';
						printf("[%s] - ", time_str);
						printf("PD cycle %5d, ", i);
						printf("WKC %2d, ", wkc);
						for (int slave = 1; slave <= ec_slavecount; slave++) {
							printf(" Sl %2d: ", slave);
							adr = 0x0310;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0311;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0300;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0301;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0302;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0303;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = %3d, ", adr, register_buffer[0]);
							adr = 0x0130;
							ec_FPRD(ec_slave[slave].configadr, adr, 1, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = x%02x, ", adr, register_buffer[0]);
							adr = 0x0134;
							ec_FPRD(ec_slave[slave].configadr, adr, 2, &register_buffer,
									EC_TIMEOUTRET);
							printf("x%03x = x%04x, ", adr, register_buffer[0] +
									(register_buffer[1] << 8));
						}
						printf("\n");
						fflush(stdout);
						wkc_last_cycle = wkc;
					}

					// for (int slave = 1; slave <= ec_slavecount; slave++) {
					// 	ec_FPRD(ec_slave[slave].configadr, 0x0310, register_buffer_size,
					// 			&register_buffer, EC_TIMEOUTRET);
					// 	printf("0x0310 ");
					// 	// uint32_t temp_num = etohl(register_buffer);
					// 	// printf("%d, ", temp_num);
					// 	uint16_t temp_num = 0;
					// 	for (int16_t k = (sizeof(register_buffer) /
					// 			sizeof(register_buffer[0])) - 1; k >= 0; k--) {
					// 		// printf("%0x ", register_buffer[k]);
					// 		temp_num += register_buffer[k] << (8 * k);
					// 	}
					// 	printf(" = %d, ", temp_num);
					// }
					
					needlf = FALSE;
					// printf("\n");

					// if(wkc < expectedWKC) {
					// 	printf("Error! WKC %d, expected: %d\n", wkc, expectedWKC);
					// }
					osal_usleep(5000);
				}
				inOP = FALSE;
				}
				else {
					printf("Not all slaves reached operational state.\n");
					ec_readstate();
					for(i = 1; i<=ec_slavecount ; i++) {
						if(ec_slave[i].state != EC_STATE_OPERATIONAL) {
							printf("Slave %d State=0x%2.2x StatusCode=0x%4.4x : %s\n",
									i, ec_slave[i].state, ec_slave[i].ALstatuscode,
									ec_ALstatuscode2string(ec_slave[i].ALstatuscode));
						}
					}
				}
				printf("\nRequest init state for all slaves\n");
				ec_slave[0].state = EC_STATE_INIT;
				/* request INIT state for all slaves */
				ec_writestate(0);
		}
		else {
			printf("No slaves found!\n");
		}
		printf("End register_reader, close socket\n");
		/* stop SOEM, close socket */
		ec_close();
	}
	else {
		printf("No socket connection on %s\nExecute as root\n",(char *)ifname);
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
	printf("SOEM (Simple Open EtherCAT Master)\nRegister reader\n");

	if (argc > 1) {
        
        if(argc > 2)
        {
            clearLinkLossCounters = 1;
        }
		/* create thread to handle slave error handling in OP */
		// osal_thread_create(&thread1, 128000, &ecatcheck, (void*) &ctime);
		/* start cyclic part */
        osal_thread_create_rt(&thread1, 128000, &register_reader, (void*) argv[1]);
        while(1)
        {
            osal_usleep(100*1000*1000);
        }
	}
	else {
		ec_adaptert * adapter = NULL;
		printf("Usage: register_reader ifname1\nifname = eth0 for example\n");

		printf ("\nAvailable adapters:\n");
		adapter = ec_find_adapters ();
		while (adapter != NULL)
		{
			printf ("    - %s  (%s)\n", adapter->name, adapter->desc);
			adapter = adapter->next;
		}
		ec_free_adapters(adapter);
	}

	printf("End program\n");
	return (0);
}
