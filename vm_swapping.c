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

#define CHILDNUM 3
#define PAGETNUM 1000
#define INDEXNUM 1000
#define FRAMENUM 32

int count = 0;
int i = 0;
int total_count = 0;
pid_t pid[CHILDNUM];
int front, rear = 0;
int run_queue[20];
int flag = 0;

int child_execution_time[CHILDNUM] ={10,6,5};
int child_execution_ctime[CHILDNUM];

struct msgbuf{
        long int  mtype;
        int pid_index;
        unsigned int  virt_mem[10];
};

typedef struct{
	int valid;
	int pfn;
	int counter;
	int disk;
}TABLE;

typedef struct{
	int valid;
	TABLE*  pt;
}DIR_TABLE;

typedef struct{
	int dir;
	int pgn;
	int pid;
	int sca;
}PH_TABLE;

DIR_TABLE dir_table[CHILDNUM][PAGETNUM];

PH_TABLE phy_mem [FRAMENUM];

int fpl[32] ; //free page list
int fpl_rear,fpl_front = 0;
//int fpn=0;

int msgq;
int ret;
int key = 0x12345;
struct msgbuf msg;

FILE* fptr;

void initialize_first_table()
{
	for( int l = 0; l < CHILDNUM ; l++){
		for(int j =0; j< PAGETNUM ; j++)
		{	
			dir_table[l][j].valid =0;
			dir_table[l][j].pt = NULL;
	
		}
	}
}

void child_signal_handler(int signum)  // sig child handler
{
	printf("pid:%d ,remaining cpu-burst%d\n",getpid(),child_execution_time[i]);
	child_execution_time[i] -- ;
	if(child_execution_time[i] <= 0)
		child_execution_time[i] = child_execution_ctime[i];
	printf("pid: %d get signal\n",getpid());
	memset(&msg,0,sizeof(msg));
	msg.mtype = IPC_NOWAIT;
	msg.pid_index = i;
	unsigned int addr;
	for (int k=0; k< 10 ; k++){
		addr = (rand() %5)<<22;
		addr |= (rand()%3)<<12;
		addr |= (rand()%0xfff);
		msg.virt_mem[k] = addr ;
	}
	ret = msgsnd(msgq, &msg, sizeof(msg),IPC_NOWAIT);
	if(ret == -1)
		perror("msgsnd error");


}
void clean_memory(DIR_TABLE* dtpt)
{
	DIR_TABLE* dir_ptf = dtpt;
	for(int j=0; j< PAGETNUM ; j++)
	{
		if(dir_ptf[j].valid == 1){
			dir_ptf[j].valid=0;
			for(int k=0 ; k < INDEXNUM ; k++)
				if(((dir_ptf[j].pt)[k].valid == 1)){
					if((dir_ptf[j].pt)[k].disk == 0){
						printf("add %d to fpl\n",(dir_ptf[j].pt)[k].pfn);
						fpl[(fpl_rear++)%FRAMENUM]=(dir_ptf[j].pt)[k].pfn;
					}
					else{
						(dir_ptf[j].pt)[k].disk = 0;
						//file 접근해서 free 시키기 
					} 
					phy_mem[(dir_ptf[j].pt)[k].pfn].sca =0;
					phy_mem[(dir_ptf[j].pt)[k].pfn].pid = -1 ;
					phy_mem[(dir_ptf[j].pt)[k].pfn].dir = -1 ;
					phy_mem[(dir_ptf[j].pt)[k].pfn].pgn =  -1; 
				}
			dir_ptf[j].pt =NULL;
			free(dir_ptf[j].pt);
		}
	}

	int pid_index = run_queue[(front-1)%20];
	FILE* pFile = fopen("disk.txt","r");
	if(pFile != NULL){
	FILE* ptemp = fopen("temp.txt","w");
	char* str;
	char cstr[256];
	char* pstr = cstr;
	int swap_out = 0;
	ssize_t read;
	ssize_t len=0;
	while( (read=getline(&str, &len, pFile)) != -1)
	{
		int p=0 ;
		for(int j = 0; j < 256; j ++)
			cstr[j] = str[j];
		char* token = strtok(pstr, " ");
		if(atoi(token) == pid_index)
			continue;
		else
			fprintf(ptemp,"%s",str);
	
	}
	fclose(pFile);
	fclose(ptemp);
	remove("disk.txt");
	rename("temp.txt","disk.txt");}
	
}

void parent_signal_handler(int signum)  // sig parent handler
{
	if(flag == 1){
		clean_memory(dir_table[run_queue[(front-1)%20]]);
		flag = 0;
	}

        total_count ++;
        count ++;
        if(total_count >= 20){

		for(int k = 0; k < CHILDNUM ; k ++)
		{
			kill(pid[k],SIGKILL);
		}
		msgctl(msgq, IPC_RMID, NULL);
//		fclose(fptr);
		exit(0);
	}
	if((front%20) != (rear%20)){
		child_execution_time[run_queue[front%20]] --;
		printf("time %d:================================================\n",total_count);
		kill(pid[run_queue[front % 20]],SIGINT);
		if((count == 3)|(child_execution_time[run_queue[front%20]]==0)){
	                count  = 0;
			if(child_execution_time[run_queue[front%20]] != 0)
				 run_queue[(rear++)%20] = run_queue[front%20];
			if(child_execution_time[run_queue[front%20]] == 0 ){
				child_execution_time[run_queue[front%20]] = child_execution_ctime[run_queue[front%20]];
				run_queue[(rear++)%20] = run_queue[front%20];
				flag = 1;
			}
			front ++;
		}
	}
}

int find_victim(void)
{
	int vic= 0 ;
	int l = fpl_front;
	while(1)
	{			
		if(phy_mem[fpl[l%32]].sca == 0)
		{
			vic = l%32;
			break;
		}
		else
			phy_mem[fpl[l%32]].sca = 0;
		l++;
	}
	//if(lru == -1)
	//	printf("lru is -1 ");
	return vic;
}
void update_table(int vict_pfn){
	int pid;
	int dir, pgn ;
	pid = phy_mem[vict_pfn].pid;
	dir = phy_mem[vict_pfn].dir;
	pgn = phy_mem[vict_pfn].pgn;
	(dir_table[pid][dir].pt)[pgn].disk = 1;
	(dir_table[pid][dir].pt)[pgn].pfn = -1 ;
	return; 
}

int swapping (){
	int vict_pfn;
	vict_pfn = find_victim();
	printf("vict_pfm is %d\n",vict_pfn);
	int  fpl = vict_pfn;
	update_table(vict_pfn);
	printf("vict_pm %d goes to disk\n",vict_pfn);
	fpl_front ++;
	fptr = fopen("disk.txt","a");
	printf("disk : %d %d %d\n",phy_mem[vict_pfn].pid,phy_mem[vict_pfn].dir,phy_mem[vict_pfn].pgn);
	fprintf(fptr,"%d %d %d\n",phy_mem[vict_pfn].pid,phy_mem[vict_pfn].dir,phy_mem[vict_pfn].pgn);  
	fclose(fptr);
	return fpl;
}

int main(int argc, char *argv[])
{
        //pid_t pid;
	unsigned int virt_mem[10];
        unsigned int offset[10];
	unsigned int pageTIndex[10];
        unsigned int pageIndex[10];
	int pid_index;
	//fptr = fopen("disk","a");
	for(int l=0; l<CHILDNUM;l++)
		child_execution_ctime[l]=child_execution_time[l]; 
	
	for(int l=0 ; l < FRAMENUM; l++){
		fpl[l] = l ;
		fpl_rear++ ;
	}
	msgq = msgget( key, IPC_CREAT | 0666);

	while(i< CHILDNUM) {
		initialize_first_table();
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
				pageTIndex[k]=(virt_mem[k] & 0xFFC00000)>>22;
				pageIndex[k] = (virt_mem[k] & 0x3FF000)>>12;

	//			      printf("message virtual memory: 0x%08x\n",msg.virt_mem[k]);
	//			    printf("Offset: 0x%04x\n", offset[k]);
				    
				      printf("DIR index : %d\n",pageTIndex[k]);
  					  printf("Page Index: %d\n", pageIndex[k]);
				//When page fault happened in first level 
				if( dir_table[pid_index][pageTIndex[k]].valid == 0 )
				{	
					printf("pagefault in first page \n");
					TABLE* table = (TABLE*) calloc(INDEXNUM, sizeof(TABLE));
					dir_table[pid_index][pageTIndex[k]].pt = table;
					dir_table[pid_index][pageTIndex[k]].valid = 1; 
				}

				TABLE* imm_tp = dir_table[pid_index][pageTIndex[k]].pt;

				if(imm_tp[pageIndex[k]].valid== 0)
				{
					printf("page fault in second page\n");
					if(fpl_front != fpl_rear){
						imm_tp[pageIndex[k]].pfn = fpl[(fpl_front%FRAMENUM)];
						imm_tp[pageIndex[k]].valid = 1;
						phy_mem[fpl[(fpl_front%FRAMENUM)]].pid= pid_index;
						phy_mem[fpl[(fpl_front%FRAMENUM)]].dir= pageTIndex[k];
						phy_mem[fpl[(fpl_front%FRAMENUM)]].pgn= pageIndex[k];
						fpl_front++;
					}
					else{
						printf("full");
						int sfn = swapping(); // swapping frame number
						imm_tp[pageIndex[k]].pfn = sfn ;
                                                imm_tp[pageIndex[k]].valid = 1;
						fpl[(fpl_rear++)%32] = sfn ;
						 
					}
				}
			
				if(imm_tp[pageIndex[k]].disk == 1 ){ // if information is in disk, load to memory

					//file에서 pid , pagen num으로 접근하는 코드 추가해야함
	  			     
		
					printf("get data from disk\n");
				
					FILE* pFile = fopen("disk.txt","r");
					FILE* ptemp = fopen("temp.txt","w");
					char* str;
					char cstr[256];
					char* pstr = cstr;
					int swap_out = 0;
					ssize_t read; 
					ssize_t len=0;
					while( (read=getline(&str, &len, pFile)) != -1)
					{	
						int p=0 ;
						swap_out = 0;
						printf("str : %s", str);
						for(int j = 0; j < 256; j ++)
							cstr[j] = str[j];
						char* token = strtok(pstr, " ");
						if(atoi(token) == pid_index){
							while (token!= NULL) {
								p++;
								token = strtok(NULL, " ");
								if((p == 1)&&(atoi(token) == pageTIndex[k]))
									continue;
								else if( p == 1) 
									break;
								if((p == 2) && (atoi(token) == pageIndex[k])){
									printf("swap out\n");
									swap_out = 1;
								}
							}
						}

						if(swap_out == 1 )
							continue;
						else
							fprintf(ptemp,"%s",str);
					}	
			
					fclose(pFile);
					fclose(ptemp);
					remove("disk.txt");
					rename("temp.txt","disk.txt");

					 if(fpl_front != fpl_rear){
                                                imm_tp[pageIndex[k]].pfn = fpl[(fpl_front%FRAMENUM)];
                                                imm_tp[pageIndex[k]].valid = 1;
						phy_mem[fpl[(fpl_front%FRAMENUM)]].pid= pid_index;
                                                phy_mem[fpl[(fpl_front%FRAMENUM)]].dir= pageTIndex[k];
                                                phy_mem[fpl[(fpl_front%FRAMENUM)]].pgn= pageIndex[k];
                                                fpl_front++;
                                        }
                                        else{
                                                printf("full");
                                                int sfn = swapping(); // swapping frame number
                                                imm_tp[pageIndex[k]].pfn = sfn ;
						fpl[(fpl_rear++)%32] = sfn ;
                                        }

					imm_tp[pageIndex[k]].disk =0;
	
				}		
				

				phy_mem[imm_tp[pageIndex[k]].pfn].sca = 1;
				printf("VM : 0x%08x , PFN : %d, sca:%d\n", virt_mem[k], imm_tp[pageIndex[k]].pfn,phy_mem[imm_tp[pageIndex[k]].pfn].sca);
				
			//	printf("message virtual memory: 0x%08x\n",msg.virt_mem[k]);
			//	printf("Offset: 0x%04x\n", offset[k]);
	//			printf("Page Index: %d\n", pageIndex[k]);
	//			printf("paget index : %d\n",pageTIndex[k]);			
			}
			memset(&msg, 0, sizeof(msg));
		}

		
	}
	return 0;

}
