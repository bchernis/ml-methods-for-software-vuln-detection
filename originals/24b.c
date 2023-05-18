static int chkNum(void) {
  unsigned char	c = (unsigned char)yytext[yyleng-1];   /* last character */
  if (!isdigit(c) && (c != '.')) {  /* c is letter */
	char	buf[BUFSIZ];
	sprintf(buf,"syntax error - badly formed number '%s' in line %d of %s\n",yytext,line_num, InputFile);
    strcat (buf, "splits into two name tokens\n");
	agerr(AGWARN,buf);
    return 1;
  }
  else return 0;
}
