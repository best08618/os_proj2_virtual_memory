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
FILE* fptr;

int child_execution_time[CHILDNUM] = {5,6,4};
int child_execution_ctime[CHILDNUM];

struct msgbuf{
        long int  mtype;
    int pid_index;
    unsigned int  virt_mem[10];
};

typedef struct{
        int valid;
        int pfn;
	int disk;
}TABLE;

typedef struct{
        int dir;
        int pgn;
        int pid;
        int sca;
}PH_TABLE;

typedef struct
{
        unsigned int tag;
        int tlb_pfn;
        int tlb_flag;
        int counter;
}TLB;

PH_TABLE phy_mem[FRAMENUM];
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
			table[a][j].disk = 0;
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
        printf("pid: %d get signal\n",getpid());
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

	for(int a = 0; a < INDEXNUM     ; a++)
	{
		if(pageTable[a].valid == 1)
		{	
			if(pageTable[a].disk == 0 ){
			fpl[(fpl_rear++)%FRAMENUM] = pageTable[a].pfn;
			printf("add %d to fpl\n", pageTable[a].pfn);
			}
			else{
				pageTable[a].disk = 0;
			}
			pageTable[a].valid =0;
			pageTable[a].pfn =0;
		}
	}

	int pid_index = run_queue[(front-1)%20];
        FILE* pFile = fopen("disk1.txt","r");
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
        remove("disk1.txt");
        rename("temp.txt","disk1.txt");}

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
	if(count == 0)
	{
		initialize_tlb();
	}
	count++;
    
    if(total_count >= 14 )
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
        //      printf("lru is -1 ");
        return vic;
}

void update_table(int vict_pfn){
        int pid;
        int pgn ;
        pid = phy_mem[vict_pfn].pid;
        pgn = phy_mem[vict_pfn].pgn;
        table[pid][pgn].disk = 1;
        table[pid][pgn].pfn = -1 ;
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
        fptr = fopen("disk1.txt","a");
	if(pid_index == phy_mem[vict_pfn].pid){	// 만약victim pid 와 current pid 같을경우
		for(int j = 0;  j < 8; 	j++){
			if((tlb[j].tlb_flag == 1) && (tlb[j].tag == phy_mem[vict_pfn].pgn)){
				tlb[j].tlb_flag = -1 ;
				break;
			}
		}

	}
        printf("disk : %d %d\n",phy_mem[vict_pfn].pid,phy_mem[vict_pfn].pgn);
        fprintf(fptr,"%d %d\n",phy_mem[vict_pfn].pid,phy_mem[vict_pfn].pgn);
        fclose(fptr);
        return fpl;
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
		new_itimer.it_interval.tv_sec = 1;
		new_itimer.it_interval.tv_usec = 0;
		new_itimer.it_value.tv_sec = 1;
		new_itimer.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &new_itimer, &old_itimer);
	}
	i++;
	}

    while(1)
    {
	    ret = msgrcv(msgq,&msg,sizeof(msg),IPC_NOWAIT,IPC_NOWAIT); //to receive message
	    if(ret != -1)
	    {
		    printf("get message\n");
		    pid_index = msg.pid_index;
		    printf("pid index: %d\n", pid_index);

		    for(int l=0 ; l <10; l++ ) 
		    {
			    virt_mem[l]=msg.virt_mem[l]; 
			    offset[l] = virt_mem[l] & 0xfff;
			    pageIndex[l] = (virt_mem[l] & 0xf000)>>12;
			    printf("message virtual memory: 0x%04x\n",msg.virt_mem[l]);					
			    //				printf("Offset: 0x%04x\n", offset[l]);
			    //				printf("Page Index: 0x%2d\n", pageIndex[l]);

			    for(int m=0;m<TLBSIZE; m++)
			    {
				    if(tlb[m].tlb_flag == 0) // nothing in tlb
				    {
					    printf("tlb%d\nEmpty miss!! get page index into tag:%d\n",m, pageIndex[l]);
					    tlb[m].tag=pageIndex[l];
					    tlb[m].tlb_flag =1;
					    tlb[m].counter =1;

					    if(table[pid_index][pageIndex[l]].valid == 0) //if its invalid 
					    {
						    printf("page fault\n");
						    if(fpl_front != fpl_rear)
						    {
							    table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
							    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], fpl[fpl_front%FRAMENUM], tlb[m].counter);
							    table[pid_index][pageIndex[l]].valid = 1;
							    tlb[m].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
							    phy_mem[fpl[(fpl_front%FRAMENUM)]].pid= pid_index;
							    phy_mem[fpl[(fpl_front%FRAMENUM)]].pgn= pageIndex[l];

							    fpl_front++;
							    break;
						    }
						    else
						    {
							    printf("full");
							    int sfn = swapping(); // swapping frame number
							    table[pid_index][pageIndex[l]].pfn = sfn ;
							    table[pid_index][pageIndex[l]].valid = 1;
							    fpl[(fpl_rear++)%32] = sfn ;
					 		    phy_mem[sfn].pid= pid_index;
                                                            phy_mem[sfn].pgn= pageIndex[l];
							    tlb[m].tlb_pfn= table[pid_index][pageIndex[l]].pfn;


							    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l],tlb[m].tlb_pfn, tlb[m].counter);
							   break;

							   /*==============swapping====================== */
						    }
					    }
					    else // get table vlue to tlb
					    {
						    if(table[pid_index][pageIndex[l]].disk == 1){  // if this data is in disk
								printf("get daa from disk\n");	
							    FILE* pFile = fopen("disk1.txt","r");
							    FILE* ptemp = fopen("temp.txt","w");
							    char* str;
							    char cstr[256];
							    char* pstr = cstr;
							    int swap_out = 0;
							    ssize_t read;
							    ssize_t len=0;
							    while( (read=getline(&str, &len, pFile)) != -1)
							    {
								    swap_out = 0;
								    printf("str : %s", str);
								    for(int j = 0; j < 256; j ++)
									    cstr[j] = str[j];
								    char* token = strtok(pstr, " ");
								    if(atoi(token) == pid_index){
										    token = strtok(NULL, " ");
										    if((atoi(token) == pageIndex[l]))
											    swap_out = 1;
								    }


								    if(swap_out == 1 )
									    continue;
								    else
									    fprintf(ptemp,"%s",str);
							    }

							    fclose(pFile);
							    fclose(ptemp);
							    remove("disk1.txt");
							    rename("temp.txt","disk1.txt");
								printf("swap out \n");
							    if(fpl_front != fpl_rear)
							    {
								    table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
								    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], fpl[fpl_front%FRAMENUM], tlb[m].counter);
								    table[pid_index][pageIndex[l]].valid = 1;
								    tlb[m].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
								    phy_mem[fpl[(fpl_front%FRAMENUM)]].pid= pid_index;
								    phy_mem[fpl[(fpl_front%FRAMENUM)]].pgn= pageIndex[l];
								   table[pid_index][pageIndex[l]].disk = 0;
								    fpl_front++;
								    break;
							    }
							    else
							    {
								    printf("full");
								    int sfn = swapping(); // swapping frame number
								    table[pid_index][pageIndex[l]].pfn = sfn ;
								    table[pid_index][pageIndex[l]].valid = 1;
								    fpl[(fpl_rear++)%32] = sfn ;
								    tlb[m].tlb_pfn= table[pid_index][pageIndex[l]].pfn;


								    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l],tlb[m].tlb_pfn, tlb[m].counter);

								    phy_mem[sfn].pid= pid_index;
								    phy_mem[sfn].pgn= pageIndex[l];
								    table[pid_index][pageIndex[l]].disk = 0;
								    break;

								   
							    }

								
						    }
						    else{
							    tlb[m].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;
							    printf("VA %d -> PA %d (pagetable)\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], table[pid_index][pageIndex[l]].pfn);
							    break;
						    }
					    }
				    }
				    else //if the line is taken, comapre the current one with the one in tlb
				    {

					    if(tlb[m].tag == pageIndex[l]) // hit 
					    {

						    printf("tlb%d hit!! get pfn\n", m);
						    
						    tlb[m].counter++;
						    printf("VA %d -> PA %d (tlb)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], tlb[m].tlb_pfn ,tlb[m].counter);
						    break;
					    } 
					    else  // miss 
					    {
						    //printf("tlb%d miss!!\n", m);
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
									 //   printf("check: %d\n", check);
									    tlb[((check%TLBSIZE)+n)%TLBSIZE].tag = pageIndex[l];
									    if(table[pid_index][pageIndex[l]].valid == 0) //invalid, means not in page table
									    {
										    printf("page fault\n");

										    if(fpl_front != fpl_rear)
										    {
											    table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
											    printf("tlb%d: VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n",((check%TLBSIZE)+n)%TLBSIZE, tlb[((check%TLBSIZE)+n)%TLBSIZE].tag, fpl[fpl_front%FRAMENUM], tlb[((check%TLBSIZE)+n)%TLBSIZE].counter);
											    table[pid_index][pageIndex[l]].valid = 1;
											    tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
											    phy_mem[fpl[(fpl_front%FRAMENUM)]].pid= pid_index;
											    phy_mem[fpl[(fpl_front%FRAMENUM)]].pgn= pageIndex[l];

												fpl_front++;
											    check++;
										//	    printf("check: %d\n", check);
											    break;
										    }
										    else
										    {
											    printf("full");
											    int sfn = swapping(); // swapping frame number
											    table[pid_index][pageIndex[l]].pfn = sfn ;
											    table[pid_index][pageIndex[l]].valid = 1;
											    fpl[(fpl_rear++)%32] = sfn ;
											    tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn= table[pid_index][pageIndex[l]].pfn;
											    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l],tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn, tlb[m].counter);

											    phy_mem[sfn].pid= pid_index;
											    phy_mem[sfn].pgn= pageIndex[l];

											    check++;
											    break;   
										 }
									    }
									    else //means in pagetable,straight get pfn value 
									    {
		
										    if(table[pid_index][pageIndex[l]].disk == 1){  // if this data is in disk
												     printf("get daa from disk\n");

											    FILE* pFile = fopen("disk1.txt","r");
											    FILE* ptemp = fopen("temp.txt","w");
											    char* str;
											    char cstr[256];
											    char* pstr = cstr;
											    int swap_out = 0;
											    ssize_t read;
											    ssize_t len=0;
											    while( (read=getline(&str, &len, pFile)) != -1)
											    {
												    swap_out = 0;
												    //      printf("str : %s", str);
												    for(int j = 0; j < 256; j ++)
													    cstr[j] = str[j];
												    char* token = strtok(pstr, " ");
												    if(atoi(token) == pid_index){
													    
														    token = strtok(NULL, " ");
														    if((atoi(token) == pageIndex[l]))
															    swap_out = 1;
													    
												    }


												    if(swap_out == 1 )
													    continue;
												    else
													    fprintf(ptemp,"%s",str);
											    }

											    fclose(pFile);
											    fclose(ptemp);
											    remove("disk1.txt");
											    rename("temp.txt","disk1.txt");

											    if(fpl_front != fpl_rear)
											    {
												    table[pid_index][pageIndex[l]].pfn=fpl[fpl_front%FRAMENUM];
												    table[pid_index][pageIndex[l]].valid = 1;
												    tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;

												    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l], tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn , tlb[m].counter);
												    fpl_front++;
													
												table[pid_index][pageIndex[l]].disk = 0;
												    break;
											    }
											    else
											    {
												    printf("full");
												    int sfn = swapping(); // swapping frame number
												    table[pid_index][pageIndex[l]].pfn = sfn ;
												    table[pid_index][pageIndex[l]].valid = 1;
												    fpl[(fpl_rear++)%32] = sfn ;
												     tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;

												    printf("VA %d -> PA %d (fpl)\ncounter -> %d\n~~~~~~~~~~~~~~~~~~~~\n", pageIndex[l],tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn, tlb[m].counter);
												    phy_mem[sfn].pid= pid_index;
												    phy_mem[sfn].pgn= pageIndex[l];
												table[pid_index][pageIndex[l]].disk = 0;

												    break;


											    }
										    }
										   else{
										    tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn =  table[pid_index][pageIndex[l]].pfn;
										    printf("tlb%d: VA %d -> PA %d (pagetable)\n~~~~~~~~~~~~~~~~~~~~\n", ((check%TLBSIZE)+n)%TLBSIZE, pageIndex[l], tlb[((check%TLBSIZE)+n)%TLBSIZE].tlb_pfn);
										    check++;

										    break;}
									    }
								    }
							    }

						    }
					    }


				    }					
			    }
		 phy_mem[table[pid_index][pageIndex[l]].pfn].sca = 1;    
		}
		    memset(&msg, 0, sizeof(msg));
	    }
    }
    return 0;
}
