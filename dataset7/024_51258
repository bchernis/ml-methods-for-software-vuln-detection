static int gettimerpid(char *name,int cpu){
  pid_t pid;
  char temp[500];

  if(name==NULL)
    name=&temp[0];

  sprintf(name,"softirq-timer/%d",cpu);

  pid=name2pid(name);

  if(pid==-1){
    sprintf(name,"ksoftirqd/%d",cpu);
    pid=name2pid(name);
  }

  return pid;
}
