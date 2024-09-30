#include "kernel/types.h"
#include "user.h"
#define BUFNUM 100

int main(int argc,char* argv[]){

    if(argc !=1){
        printf("pingpong doesn't need argement!");
        exit(-1);
    }

    int ftoc[2];//�����̵��ӽ��̵Ĺܵ���0��ʾ�����ˣ�1��ʾд���
    int ctof[2];//�ӽ��̵������̵Ĺܵ���0��ʾ�����ˣ�1��ʾд���
    
    char buf[BUFNUM];

    //������йܵ��Ĵ���,ͬʱ�жϹܵ��Ƿ���ִ���
    if(pipe(ftoc)<0||pipe(ctof)<0){
        printf("Create pipe Error!");
        exit(-1);
    }
    int ppid = getpid();
    int pid = fork();
    if(pid == 0){
        //�ӽ���
        close(ftoc[1]);
        read(ftoc[0],buf,BUFNUM);

        printf("%d: received %s from pid %d\n", getpid(), buf,ppid);

        close(ctof[0]);
        write(ctof[1],"pong",BUFNUM); //��ܵ���д��pong
        
        close(ctof[1]);
        exit(0);
    }else if(pid>0){
        //������
        close(ftoc[0]);
        write(ftoc[1],"ping",BUFNUM);

        wait(0);
        close(ctof[1]);
        read(ctof[0],buf,BUFNUM);
        printf("%d: received %s from pid %d\n", getpid(), buf,pid);
       
    } 
    exit(0);

}