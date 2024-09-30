#include "kernel/types.h"
#include "user.h"
#define BUFNUM 100

int main(int argc,char* argv[]){

    if(argc !=1){
        printf("pingpong doesn't need argement!");
        exit(-1);
    }

    int ftoc[2];//父进程到子进程的管道，0表示读出端，1表示写入端
    int ctof[2];//子进程到父进程的管道，0表示读出端，1表示写入端
    
    char buf[BUFNUM];

    //下面进行管道的创建,同时判断管道是否出现错误
    if(pipe(ftoc)<0||pipe(ctof)<0){
        printf("Create pipe Error!");
        exit(-1);
    }
    int ppid = getpid();
    int pid = fork();
    if(pid == 0){
        //子进程
        close(ftoc[1]);
        read(ftoc[0],buf,BUFNUM);

        printf("%d: received %s from pid %d\n", getpid(), buf,ppid);

        close(ctof[0]);
        write(ctof[1],"pong",BUFNUM); //向管道中写入pong
        
        close(ctof[1]);
        exit(0);
    }else if(pid>0){
        //父进程
        close(ftoc[0]);
        write(ftoc[1],"ping",BUFNUM);

        wait(0);
        close(ctof[1]);
        read(ctof[0],buf,BUFNUM);
        printf("%d: received %s from pid %d\n", getpid(), buf,pid);
       
    } 
    exit(0);

}