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

#define CHILDNUM 2
#define INDEXNUM 16
#define FRAMENUM 32

int count = 0;
int child_count = 0;
int i = 0;
int total_count = 0;
pid_t pid[CHILDNUM];
int front, rear = 0;
int run_queue[20];
int pid_index =0;
int flag = 0;

int fork_check = 0;

int child_execution_time[CHILDNUM] = {10,6};
int child_execution_ctime[CHILDNUM];

struct msgbuf{
        long int  mtype;
    	int pid_index;
    	unsigned int  virt_mem[10];
	int w_flag[10];
	int w_data[10];
	int fork_check;
};

typedef struct{
        int valid;
        int pfn;
	int read_only;
}TABLE;

typedef struct{
	int data;
}PHY_TABLE;

TABLE table[CHILDNUM][INDEXNUM];
PHY_TABLE phy_mem [FRAMENUM];
//int fpl = 0;
int fpl[32];
int fpl_rear, fpl_front =0;


unsigned int pageIndex[10];
unsigned int virt_mem[10];
unsigned int offset[10];
int w_flag[10];
int w_data[10];

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
			table[a][j].read_only = 0;
                }
        }
}
void copy_pagetable()
{	
	
	for(int n=0; n< INDEXNUM; n++)
	{
		table[0][n].read_only =1;
		table[1][n]=table[0][n];
	}
	return;	
}

void child_signal_handler(int signum)  // sig child handler
{
	if(fork_check >0)
		fork_check ++;
        printf("pid: %d remaining cpu-burst%d\n",getpid(), child_execution_time[i]);
        child_execution_time[i]--;
	child_count ++;
	memset(&msg,0,sizeof(msg));
        msg.mtype = IPC_NOWAIT;
        msg.pid_index = i;
        if(child_execution_time[i] <= 0)
        {
                child_execution_time[i] = child_execution_ctime[i];
        }
	if(child_count == 3){ // fork check number 
		child_count = 0;
		fork_check ++;
	}
	printf("fork_Check : %d",fork_check);
	msg.fork_check = fork_check;
        printf("pid: %d get signal\n",getpid());
        for (int k=0; k< 10 ; k++){
                unsigned int addr;
                addr = (rand() %0x09)<<12;
        	addr |= (rand()%0xfff);
                msg.virt_mem[k] = addr ;
		int write_flag = rand()%2;
		msg.w_flag[k] = write_flag;
		if(write_flag == 1 ){
			int write_data = rand()%31 + 1000 ;
			msg.w_data[k] = write_data;
		}
		else
			msg.w_data[k] = 0;

        }
        ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
        if(ret == -1)
                perror("msgsnd error");
}

void clean_memory(TABLE* page_table)
{
        TABLE* pageTable = page_table;

        for(int a = 0; a < INDEXNUM ; a++)
        {
                if(pageTable[a].valid == 1)
        {
		fpl[(fpl_rear++)%FRAMENUM] = pageTable[a].pfn;
		printf("add %d to fpl\n", pageTable[a].pfn);
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
 	if(total_count >= 35 )
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
			if(child_execution_time[run_queue[front%20]] != 0){
				run_queue[(rear++)%20] = run_queue[front%20];
				printf("run_queue : %d. pid : %d\n", run_queue[(rear-1)%20], run_queue[front%20]);
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

void initialize_phymem(){

	for(int l =0 ; l <FRAMENUM ; l++)
		phy_mem[l].data = l;
}

int main(int argc,char* argv[])
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
	srand(time(NULL));
	initialize_table();
	initialize_phymem();
	pid[i]= fork();
	run_queue[(rear++)%20] = i ;
	if(pid[0] == -1){
		perror("fork error");
		return 0;
	}
	else if( pid[i] == 0){
	
		struct sigaction old_sa;
		struct sigaction new_sa;
		memset(&new_sa, 0, sizeof(new_sa));
                new_sa.sa_handler = &child_signal_handler;
		sigaction(SIGINT, &new_sa, &old_sa);
		while(1);
		return 0;
	}
	else{
		i++;
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

	while(1)
	{
		ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT); //to receive message
		if(ret != -1)
		{
			printf("get message\n");
			pid_index = msg.pid_index;
			printf("pid index: %d\n", pid_index);
			int f_check = msg.fork_check;
			printf("fork_check value : %d\n",f_check);
			for(int l=0 ; l <10; l++ ) //accessing 10 memory addresses for one process
			{
				virt_mem[l]=msg.virt_mem[l];
				offset[l] = virt_mem[l] & 0xfff;
				pageIndex[l] = (virt_mem[l] & 0xf000)>>12;
				w_flag[l] = msg.w_flag[l];
				w_data[l] = msg.w_data[l];
				printf("message virtual memory: 0x%04x\n",msg.virt_mem[l]);
				if(table[pid_index][pageIndex[l]].valid == 0) //if its invalid
				{
					//              printf("Invalid, get free page list \n");
					if(fpl_front != fpl_rear)
					{
						table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
						//fpl++;
						table[pid_index][pageIndex[l]].valid = 1;
						fpl_front++;
					}
					else
					{
						return 0;
					}

				}
				/*else*/if(table[msg.pid_index][pageIndex[l]].valid == 1)
				{
					if((w_flag[l] == 1)&&(table[msg.pid_index][pageIndex[l]].read_only==0 )){// write 하고 싶고, not read only
						int data = phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data;
						phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data = w_data[l];
						printf("VA %d -> PA %d, Data %d changed to %d\n ", pageIndex[l], table[msg.pid_index][pageIndex[l]].pfn,data,phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data );
					}
					else if((w_flag[l] == 1)&&(table[msg.pid_index][pageIndex[l]].read_only==1 )){//want to write but read only
						if(fpl_front != fpl_rear)
						{
							table[msg.pid_index][pageIndex[l]].read_only = 0;
							int imm_pa = table[pid_index][pageIndex[l]].pfn;
							table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
							printf("PA %d is changed to  PA %d\n", imm_pa, table[pid_index][pageIndex[l]].pfn);
							//fpl++;
							fpl_front++;
							phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data = w_data[l];
							printf("VA %d -> PA %d, Data %d is written\n", pageIndex[l], table[msg.pid_index][pageIndex[l]].pfn,phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data );
						}
						table[msg.pid_index][pageIndex[l]].read_only = 0;
						table[(msg.pid_index+1)%2][pageIndex[l]].read_only = 0; //change another process not to read only

					}
					else if (w_flag[l] == 0){ // only want to read
						printf("VA %d -> PA %d, Read data: %d\n ", pageIndex[l], table[msg.pid_index][pageIndex[l]].pfn,phy_mem[table[msg.pid_index][pageIndex[l]].pfn].data);

					}

				}
			}

			if(f_check == 1){
				fork_check ++;
				copy_pagetable();
				pid[i] = fork ();
				run_queue[(rear++)%20] = i;
				if(pid[i] == 0 ){
					struct sigaction old_sa;
					struct sigaction new_sa;
					memset(&new_sa, 0, sizeof(new_sa));
					new_sa.sa_handler = &child_signal_handler;
					sigaction(SIGINT, &new_sa, &old_sa);
					while(1);
					return 0;
				}

			}


		}
		memset(&msg, 0, sizeof(msg));
	}
	return 0;
}
