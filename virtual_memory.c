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

int child_execution_time[CHILDNUM] = {2,6,5};
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
int fpl = 0;

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
		addr = rand() %0xff;
       	addr |= (rand()%0xff)<<8;
		msg.virt_mem[k] = addr ;
	}
	ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
	if(ret == -1)
		perror("msgsnd error");
}

void parent_signal_handler(int signum)  // sig parent handler
{
	total_count ++;
    count ++;
    if(total_count >= 6 )
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
       	if((count == 3)|(child_execution_time[run_queue[front%20]] != 0))
		{
			count =0;
			if(child_execution_time[run_queue[front%20]] != 0)
               	run_queue[(rear++)%20] = run_queue[front%20];
			if(child_execution_time[run_queue[front%20]] == 0)
			{	
				child_execution_time[run_queue[front%20]] = child_execution_ctime[run_queue[front%20]];
				run_queue[(rear++)%20] = run_queue[front%20];
            }
			front++;
       	}
	}
}


int main(int argc, char *argv[])
{
        //pid_t pid;

	for(int l = 0; l< CHILDNUM; l++)
	{
		child_execution_ctime[l]= child_execution_time[l];
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
                new_itimer.it_interval.tv_sec = 1;
                new_itimer.it_interval.tv_usec = 0;
                new_itimer.it_value.tv_sec = 1;
                new_itimer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
        	}
        	i++;
        }

        while(1){
		ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT); //to receive message
		if(ret != -1){
			printf("get message\n");
			pid_index = msg.pid_index;
			printf("pid index: %d\n", pid_index);

			for(int l=0 ; l <10; l++ ) //accessing 10 memory addresses for one process
			{
				virt_mem[l]=msg.virt_mem[l]; 
				offset[l] = virt_mem[l] & 0xfff;
				pageIndex[l] = (virt_mem[l] & 0xf000)>>12;
				printf("message virtual memory: 0x%04x\n",msg.virt_mem[l]);
				printf("Offset: 0x%04x\n", offset[l]);
				printf("Page Index: 0x%2d\n", pageIndex[l]);

       			if(table[msg.pid_index][pageIndex[l]].valid == 0) //if its invalid
   				{
           			printf("Invalid, get free page list \n");
           			table[msg.pid_index][pageIndex[l]].pfn=fpl;
           			printf("VA %d -> PA %d\n", pageIndex[l], fpl);
           			fpl++;
           			table[msg.pid_index][pageIndex[l]].valid = 1;
       			}
				else if(table[msg.pid_index][pageIndex[l]].valid == 1)
				{
					printf("Valid, get page frame number \n");
					printf("VA %d -> PA %d\n", pageIndex[l], table[msg.pid_index][pageIndex[l]].pfn);
				}
						
			}
			memset(&msg, 0, sizeof(msg));
		}
	}
        return 0;

}









