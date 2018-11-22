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
int freeList = 0;

int msgq;
int ret;
int key = 0x12345;
struct msgbuf msg;

void initialize_table()
{
	for( int i = 0; i < CHILDNUM ; i++){
		for(int j =0; j< INDEXNUM ; j++)
		{	
			table[i][j].valid =0;
			table[i][j].pfn = 0;
	
		}
	}
}

void child_signal_handler(int signum)  // sig child handler
{

	printf("pid: %d get signal",getpid());
	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	unsigned int addr;
	for (int k=0; k< 10 ; k++){
		addr = rand() %0xff;
        	addr |= (rand()&0xff)<<8;
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
        if(total_count >= 10 ){

		 for(int k = 0; k < CHILDNUM ; k ++)
                {
                        kill(pid[k],SIGKILL);
                }
                 msgctl(msgq, IPC_RMID, NULL);

                exit(0);
	}


        printf("time %d:\n",total_count);
        kill(pid[run_queue[front% 20]],SIGINT);
        if(count == 3){
                run_queue[(rear++)%20] = run_queue[front%20];
                front ++;
        }
}


int main(int argc, char *argv[])
{
        //pid_t pid;
	unsigned int virt_mem[10];
        unsigned int offset[10];
        unsigned int pageIndex[10];
	int pid_index;
	
	msgq = msgget( key, IPC_CREAT | 0666);
        while(i< CHILDNUM) {
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
                //parent
                //printf("my pid is %d\n", getpid());
                // iterative signal , timer --> alarm
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
			for(int k=0 ; k < 10 ; k ++ ){
				virt_mem[k]=msg.virt_mem[k]; 
				offset[k] = virt_mem[k] & 0xfff;
				pageIndex[k] = (virt_mem[k] & 0xf000)>>12;
				printf("message virtual memory: 0x%04x\n",msg.virt_mem[k]);
				printf("Offset: 0x%04x\n", offset[k]);
				printf("Page Index: 0x%x\n", pageIndex[k]);			
			}
			memset(&msg, 0, sizeof(msg));
	

		}
	}
        return 0;

}

