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

int count = 0;
int i = 0;
int total_count = 0;
pid_t pid[CHILDNUM];
int front, rear = 0;
int run_queue[20];
int pid_index =0;
int flag = 0;
FILE* fptr;

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

TABLE table[CHILDNUM][INDEXNUM];
int phy_mem [FRAMENUM];
int fpl[32];
int fpl_rear, fpl_front =0;


unsigned int pageIndex[10];
unsigned int virt_mem[10];
unsigned int offset[10];


int msgq;
int ret;
int key = 0x12345;
struct msgbuf msg;

void initialize_table()
{
	for( int a = 0; a < CHILDNUM ; a++){
		for(int j =0; j< INDEXNUM ; j++)
		{	
			table[a][j].valid =0;
			table[a][j].pfn = 0;
	
		}
	}
}

void child_signal_handler(int signum)  // sig child handler
{

	printf("pid: %d remaining cpu-burst%d\n",getpid(), child_execution_time[i]);
	child_execution_time[i]--;
	if(child_execution_time[i] <= 0)
	{
		child_execution_time[i] = child_execution_ctime[i];
	}
	printf("pid: %d get signal\n",getpid());
	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	for (int k=0; k< 10 ; k++){
		unsigned int addr;
		addr = (rand() %0x4)<<12;
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
			fprintf(fptr,"%d is free to fpl\n", pageTable[a].pfn);
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

	total_count ++;
    count ++;
    if(total_count >= 100 )
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
       	fprintf(fptr,"time %d:==================================\n",total_count);
		fprintf(fptr,"pid: %d remaining cpu-burst%d\n",pid[run_queue[front% 20]], child_execution_time[run_queue[front% 20]]);

		kill(pid[run_queue[front% 20]],SIGINT);
       	if((count == 3)|(child_execution_time[run_queue[front%20]] == 0))
		{
			count =0;
			if(child_execution_time[run_queue[front%20]] != 0)
               	run_queue[(rear++)%20] = run_queue[front%20];
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
	fptr = fopen("vm_onelevel.txt","w");

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
		
        if (pid[i]== -1) {
                perror("fork error");
                return 0;
        }
        else if (pid[i]== 0) {
                //child
                struct sigaction old_sa;
                struct sigaction new_sa;
                memset(&new_sa, 0, sizeof(new_sa));
                new_sa.sa_handler = &child_signal_handler;
                sigaction(SIGINT, &new_sa, &old_sa);
                while(1);
                	return 0;
        }
        else {
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

			for(int l=0 ; l <10; l++ ) //accessing 10 memory addresses for one process
			{
				virt_mem[l]=msg.virt_mem[l]; 
				offset[l] = virt_mem[l] & 0xfff;
				pageIndex[l] = (virt_mem[l] & 0xf000)>>12;
				fprintf(fptr,"virtual memory: 0x%04x,",virt_mem[l]);
				fprintf(fptr," offset: 0x%04x, ", offset[l]);
				fprintf(fptr,"page index: %d\n", pageIndex[l]);
				sprintf(str2, "%x", offset[l]);

       			if(table[pid_index][pageIndex[l]].valid == 0) //if its invalid
   				{
					if(fpl_front != fpl_rear)
					{
           				table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
						sprintf(str, "%d", table[pid_index][pageIndex[l]].pfn);
						strcat(str,str2);
           				fprintf(fptr,"VA %d -> PA %s\n", pageIndex[l], str);
						table[pid_index][pageIndex[l]].valid = 1;
						fpl_front++;
					}
					else
					{
						printf("full");
						return 0;
					}
					
       			}
				else if(table[msg.pid_index][pageIndex[l]].valid == 1)
				{
					sprintf(str, "%d", table[pid_index][pageIndex[l]].pfn);
					strcat(str,str2);
					fprintf(fptr,"VA %d -> PA %s\n", pageIndex[l], str);
				}
			}				
		}
			memset(&msg, 0, sizeof(msg));
	}
	
        return 0;
}
