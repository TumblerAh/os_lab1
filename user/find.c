#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//ls函数的作用是输出当前目录下的所有内容，如果是文件的话就会输出文件名
//find函数要做到的是可以直接输出目录和文件的路径

//这个函数可以用来找到某个路径的最后一个分支
char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));
  // memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}


void find(char *path,char *filename) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }


  switch (st.type) {
    case T_FILE:
        if(strcmp(fmtname(path),filename)==0){
            printf("%s",path);
        }
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0) continue; //表示该条目无效

        //跳过. 和 ..
        if(strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0){
          continue;
        }
        memmove(p, de.name, DIRSIZ); 
        p[DIRSIZ] = 0;
        if (stat(buf, &st) < 0) {
          printf("find: cannot stat %s\n", buf);
          continue;
        }
        if(strcmp(de.name,filename)==0){
          printf("%s\n",buf);
        }
        find(buf,filename);
      }
      
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  
  if (argc < 3) {
    printf("Usage: find <path> <filename>\n");
    exit(0);
  }
  find(argv[1],argv[2]);

  exit(0);
}
