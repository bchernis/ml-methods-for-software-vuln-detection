static void xmessage_fork(struct proclistlist *pll){
  char message[5000];

  set_pid_priority(0,SCHED_FIFO,sched_get_priority_min(SCHED_FIFO),"Unable to set SCHED_FIFO for %d (\"%s\"). (%s)", "the xmessage fork");

  setenv("DISPLAY",":0.0",1);

  if( ! xmessage_found)
    sprintf(message,"xmessage \"WARNING! das_watchdog pauses realtime operations for %d seconds.\"",waittime);
  else
    sprintf(message,"%s \"WARNING! das_watchdog pauses realtime operations for %d seconds.\"",WHICH_XMESSAGE,waittime);

  if(send_xmessage_using_uids(pll,message)==0){
    set_pid_priority(0,SCHED_OTHER,0,"Unable to set SCHED_OTHER for %d (\"%s\"). (%s)", "the xmessage fork");
    send_xmessage_using_XAUTHORITY(pll,0,message);
  }

  pll_delete(pll);
}
