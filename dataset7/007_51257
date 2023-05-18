static int *get_userlist(struct proclistlist *pll, int *num_users){
  int *ret=calloc(sizeof(int),pll->length);
  int lokke;

  *num_users=0;

  for(lokke=0;lokke<pll->length;lokke++){
    glibtop_proc_uid uid;
    glibtop_get_proc_uid(&uid,pll->proclist[lokke].pid);
    if( ! is_a_member(uid.uid,ret,*num_users)){
      ret[*num_users]=uid.uid;
      (*num_users)++;
    }
  }
  return ret;
}
