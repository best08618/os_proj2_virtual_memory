#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>

#define CHILDNUM 10
#define INDEXNUM 16
#define FRAMENUM 32
#define TLBSIZE 8

int count = 0;
int i = 0;
int total_count = 0;
pid_t pid[CHILDNUM];
int front, rear = 0;
int run_queue[20];
int pid_index =0;
int flag = 0;
int m=0;
int min=0;
int check=0;

char str[12];
char str2[12];

int child_execution_time[CHILDNUM] = {2,6,5,8,3,2,9,1,3,10};
int child_execution_ctime[CHILDNUM];

struct msgbuf{
	long int  mtype;
    int pid_index;
    unsigned int  virt_mem[10];
};

typedef struct{
	int valid;
	int pfn;
}TABLE;

typedef struct
{
	unsigned int tag;
	int tlb_pfn;
	int tlb_flag;
	int counter;
}TLB;

int phy_mem[FRAMENUM];
int fpl[32];
int fpl_rear, fpl_front =0;
TABLE table[CHILDNUM][INDEXNUM];
TLB tlb[TLBSIZE];

unsigned int pageIndex[10];
unsigned int virt_mem[10];
unsigned int offset[10];

int msgq;
int ret;
int key = 0x12345;
struct msgbuf msg;

void initialize_table()
{
	for( int a = 0; a < CHILDNUM ; a++)
	{
		for(int j =0; j< INDEXNUM ; j++)
		{	
			table[a][j].valid =0;
			table[a][j].pfn = 0;
		}
	}
}

void initialize_tlb()
{
	for(int c =0; c<TLBSIZE; c++)
    {
        tlb[c].tag =0;
        tlb[c].tlb_pfn =0;
        tlb[c].tlb_flag =0;
		tlb[c].counter =0;
    }
	printf("tlb is initialized\n");
}


void child_signal_handler(int signum)  // sig child handler
{

	printf("pid: %d remaining cpu-burst%d\n",getpid(), child_execution_time[i]);
	child_execution_time[i]--;
	if(child_execution_time[i] <= 0)
	{
		child_execution_time[i] = child_execution_ctime[i];
	}
	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	for (int k=0; k< 10 ; k++){
		unsigned int addr;
		addr = (rand() %0xf)<<12;
       	addr |= (rand()%0xfff);
		msg.virt_mem[k] = addr ;
	}
	ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
	if(ret == -1)
		perror("msgsnd error");
}

void clean_memory(TABLE* page_table)
{
	TABLE* pageTable = page_table;

	for(int a = 0; a < INDEXNUM	; a++)
	{
		if(pageTable[a].valid == 1)
       	{
			fpl[(fpl_rear++)%FRAMENUM] = pageTable[a].pfn;
			printf("free %d into fpl\n", pageTable[a].pfn);
           	pageTable[a].valid =0;
           	pageTable[a].pfn =0;
       	}
	}
	return;
}


void parent_signal_handler(int signum)  // sig parent handler
{
	if(flag == 1)
	{
		clean_memory(table[run_queue[(front-1)%20]]);
		flag =0;
	}

	total_count++;
	if(count == 0)
	{
		initialize_tlb();
	}
	count++;
    
    if(total_count >= 10000 )
	{
	 	for(int k = 0; k < CHILDNUM ; k ++)
        {
			kill(pid[k],SIGKILL);
        }
        msgctl(msgq, IPC_RMID, NULL);
		exit(0);
	}
		
	if((front%20) != (rear%20))
	{
		child_execution_time[run_queue[front%20]] --;
       	printf("time %d:==================================\n",total_count);
		kill(pid[run_queue[front% 20]],SIGINT);
       	if((count == 3)|(child_execution_time[run_queue[front%20]] == 0))
		{
			count =0;
			if(child_execution_time[run_queue[front%20]] != 0)
			{
               	run_queue[(rear++)%20] = run_queue[front%20];
			}
			if(child_execution_time[run_queue[front%20]] == 0)
			{	
				child_execution_time[run_queue[front%20]] = child_execution_ctime[run_queue[front%20]];
				run_queue[(rear++)%20] = run_queue[front%20];
				flag=1;
            }
			front++;
       	}
	}
}

int main(int argc, char *argv[])
{
	for(int l = 0; l< CHILDNUM; l++)
	{
		child_execution_ctime[l]= child_execution_time[l];
	}

	for(int h=0; h<FRAMENUM; h++)
	{
		fpl[h] = h;
		fpl_rear++;
	}

	msgq = msgget( key, IPC_CREAT | 0666);
    while(i< CHILDNUM) 
	{
		srand(time(NULL));
 		initialize_table();
		pid[i] = fork();
		run_queue[(rear++)%20] = i ;
		
        if (pid[i]== -1) 
		{
        	perror("fork error");
            return 0;
        }
        else if (pid[i]== 0) 
		{
        	//child
            struct sigaction old_sa;
            struct sigaction new_sa;
            memset(&new_sa, 0, sizeof(new_sa));
            new_sa.sa_handler = &child_signal_handler;
            sigaction(SIGINT, &new_sa, &old_sa);
            while(1);
            	return 0;
        }
        else 
		{
        	struct sigaction old_sa;
            struct sigaction new_sa;
            memset(&new_sa, 0, sizeof(new_sa));
            new_sa.sa_handler = &parent_signal_handler;
            sigaction(SIGALRM, &new_sa, &old_sa);

            struct itimerval new_itimer, old_itimer;
            new_itimer.it_interval.tv_sec = 0;
            new_itimer.it_interval.tv_usec = 10000;
            new_itimer.it_value.tv_sec = 0;
            new_itimer.it_value.tv_usec = 10000;
            setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
       	}
       	i++;
	}

	while(1)
	{
		ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT); //to receive message
		if(ret != -1)
		{
			pid_index = msg.pid_index;
			printf("pid index: %d\n", pid_index);

			for(int l=0 ; l <10; l++ ) 
			{
				virt_mem[l]=msg.virt_mem[l]; 
				offset[l] = virt_mem[l] & 0xfff;
				pageIndex[l] = (virt_mem[l] & 0xf000)>>12;
				printf("virtual memory: 0x%04x,",msg.virt_mem[l]);
				printf(" offset: 0x%04x, ", offset[l]);
                printf("page index: %d\n", pageIndex[l]);
				sprintf(str2, "%03x", offset[l]);
					
				for(int m=0;m<TLBSIZE; m++)
				{
					if(tlb[m].tlb_flag == 0)
					{
						printf("tlb%d -> Empty miss!!\n",m);
						tlb[m].tag=pageIndex[l];
						tlb[m].tlb_flag =1;
						tlb[m].counter =1;

						if(table[pid_index][pageIndex[l]].valid == 0) //if its invalid 
                        {
                            if(fpl_front != fpl_rear)
                            {
                                table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
								sprintf(str, "%d", table[pid_index][pageIndex[l]].pfn);
								strcat(str,str2);
                                printf("VA %d -> PA %s (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], str, tlb[m].counter);
                                table[pid_index][pageIndex[l]].valid = 1;
								tlb[m].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
                                fpl_front++;
								break;
                            }
                            else
							{
									printf("full");
									for(int k = 0; k < CHILDNUM ; k ++)
									{
											kill(pid[k],SIGKILL);
									}
									msgctl(msgq, IPC_RMID, NULL);
									exit(0);

                                return 0;
                            }
                        }
                        else
                        {
                            tlb[m].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;
							sprintf(str, "%d", tlb[m].tlb_pfn);
                    		strcat(str,str2);
							printf("VA %d -> PA %s (pagetable)\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], str);
							break;
                        }
					}
	                else //if the line is taken, comapre the current one with the one in tlb
                	{

                    	if(tlb[m].tag == pageIndex[l]) // hit 
                    	{
                        	printf("tlb%d hit!! get pfn\n", m);
                        	tlb[m].tlb_pfn = table[pid_index][pageIndex[l]].pfn;
							tlb[m].counter++;
							sprintf(str, "%d", tlb[m].tlb_pfn);
                            strcat(str,str2);
                        	printf("VA %d -> PA %s (tlb)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], str ,tlb[m].counter);
                        	break;
                    	} 
                    	else  // miss 
                    	{
                        	printf("tlb%d miss!!\n", m);
							if(m==7) //all tlb taken
							{	
								printf("all tlb are taken, find a space\n");
								//find the least frequent used tlb
								for(int n=0; n<1; n++)
								{
									min = tlb[n].counter;
								}
								for(int n=0; n<TLBSIZE; n++)
								{
									if(tlb[n].counter < min)
									{
										min = tlb[n].counter;
									}
								}
								for(int n=0; n<TLBSIZE; n++)
								{
									if(tlb[((check%TLBSIZE)+n)%TLBSIZE].counter == min)
                                    {	
                                        tlb[((check%TLBSIZE)+n)%TLBSIZE].tag = pageIndex[l];
										if(table[pid_index][pageIndex[l]].valid == 0) //invalid, means not in page table
                        				{
                            				if(fpl_front != fpl_rear)
                            				{
	            	            		        table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
												sprintf(str, "%d", table[pid_index][pageIndex[l]].pfn);
                            					strcat(str,str2);
    	            	                		printf("tlb%d: VA %d -> PA %s (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n",((check%TLBSIZE)+n)%TLBSIZE, tlb[((check%TLBSIZE)+n)%TLBSIZE].tag, str, tlb[((check%TLBSIZE)+n)%TLBSIZE].counter);
        			            	            table[pid_index][pageIndex[l]].valid = 1;
            			            	        tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
                        				        fpl_front++;
												check++;
                                				break;
                            				}
                           					else
                            				{
													printf("full");
													for(int k = 0; k < CHILDNUM ; k ++)
													{
															kill(pid[k],SIGKILL);
													}
													msgctl(msgq, IPC_RMID, NULL);
													exit(0);
                                				return 0;
                            				}
                        				}
                        				else //means in pagetable,straight get pfn value 
                        				{
                            				tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;
											sprintf(str, "%d", tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn);
                                            strcat(str,str2);
                            				printf("tlb%d: VA %d -> PA %s (pagetable)\n~~~~~~~~~~~~~~~~~~~~\n", ((check%TLBSIZE)+n)%TLBSIZE, pageIndex[l], str);
                            				check++;
											break;
                        				}
                                    }
								}
							}
						}
                	}					
				}
			}
			memset(&msg, 0, sizeof(msg));
		}
	}
	return 0;
}
