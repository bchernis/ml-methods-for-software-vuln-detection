u_read_undo(char_u *name, char_u *hash, char_u *orig_name)
{
    char_u	*file_name;
    FILE	*fp;
    long	version, str_len;
    char_u	*line_ptr = NULL;
    linenr_T	line_lnum;
    colnr_T	line_colnr;
    linenr_T	line_count;
    int		num_head = 0;
    long	old_header_seq, new_header_seq, cur_header_seq;
    long	seq_last, seq_cur;
    long	last_save_nr = 0;
    short	old_idx = -1, new_idx = -1, cur_idx = -1;
    long	num_read_uhps = 0;
    time_t	seq_time;
    int		i, j;
    int		c;
    u_header_T	*uhp;
    u_header_T	**uhp_table = NULL;
    char_u	read_hash[UNDO_HASH_SIZE];
    char_u	magic_buf[UF_START_MAGIC_LEN];
#ifdef U_DEBUG
    int		*uhp_table_used;
#endif
#ifdef UNIX
    stat_T	st_orig;
    stat_T	st_undo;
#endif
    bufinfo_T	bi;

    vim_memset(&bi, 0, sizeof(bi));
    if (name == NULL)
    {
	file_name = u_get_undo_file_name(curbuf->b_ffname, TRUE);
	if (file_name == NULL)
	    return;

#ifdef UNIX
	/* For safety we only read an undo file if the owner is equal to the
	 * owner of the text file or equal to the current user. */
	if (mch_stat((char *)orig_name, &st_orig) >= 0
		&& mch_stat((char *)file_name, &st_undo) >= 0
		&& st_orig.st_uid != st_undo.st_uid
		&& st_undo.st_uid != getuid())
	{
	    if (p_verbose > 0)
	    {
		verbose_enter();
		smsg((char_u *)_("Not reading undo file, owner differs: %s"),
								   file_name);
		verbose_leave();
	    }
	    return;
	}
#endif
    }
    else
	file_name = name;

    if (p_verbose > 0)
    {
	verbose_enter();
	smsg((char_u *)_("Reading undo file: %s"), file_name);
	verbose_leave();
    }

    fp = mch_fopen((char *)file_name, "r");
    if (fp == NULL)
    {
	if (name != NULL || p_verbose > 0)
	    EMSG2(_("E822: Cannot open undo file for reading: %s"), file_name);
	goto error;
    }
    bi.bi_buf = curbuf;
    bi.bi_fp = fp;

    /*
     * Read the undo file header.
     */
    if (fread(magic_buf, UF_START_MAGIC_LEN, 1, fp) != 1
		|| memcmp(magic_buf, UF_START_MAGIC, UF_START_MAGIC_LEN) != 0)
    {
	EMSG2(_("E823: Not an undo file: %s"), file_name);
	goto error;
    }
    version = get2c(fp);
    if (version == UF_VERSION_CRYPT)
    {
#ifdef FEAT_CRYPT
	if (*curbuf->b_p_key == NUL)
	{
	    EMSG2(_("E832: Non-encrypted file has encrypted undo file: %s"),
								   file_name);
	    goto error;
	}
	bi.bi_state = crypt_create_from_file(fp, curbuf->b_p_key);
	if (bi.bi_state == NULL)
	{
	    EMSG2(_("E826: Undo file decryption failed: %s"), file_name);
	    goto error;
	}
	if (crypt_whole_undofile(bi.bi_state->method_nr))
	{
	    bi.bi_buffer = alloc(CRYPT_BUF_SIZE);
	    if (bi.bi_buffer == NULL)
	    {
		crypt_free_state(bi.bi_state);
		bi.bi_state = NULL;
		goto error;
	    }
	    bi.bi_avail = 0;
	    bi.bi_used = 0;
	}
#else
	EMSG2(_("E827: Undo file is encrypted: %s"), file_name);
	goto error;
#endif
    }
    else if (version != UF_VERSION)
    {
	EMSG2(_("E824: Incompatible undo file: %s"), file_name);
	goto error;
    }

    if (undo_read(&bi, read_hash, (size_t)UNDO_HASH_SIZE) == FAIL)
    {
	corruption_error("hash", file_name);
	goto error;
    }
    line_count = (linenr_T)undo_read_4c(&bi);
    if (memcmp(hash, read_hash, UNDO_HASH_SIZE) != 0
				  || line_count != curbuf->b_ml.ml_line_count)
    {
	if (p_verbose > 0 || name != NULL)
	{
	    if (name == NULL)
		verbose_enter();
	    give_warning((char_u *)
		      _("File contents changed, cannot use undo info"), TRUE);
	    if (name == NULL)
		verbose_leave();
	}
	goto error;
    }

    /* Read undo data for "U" command. */
    str_len = undo_read_4c(&bi);
    if (str_len < 0)
	goto error;
    if (str_len > 0)
	line_ptr = read_string_decrypt(&bi, str_len);
    line_lnum = (linenr_T)undo_read_4c(&bi);
    line_colnr = (colnr_T)undo_read_4c(&bi);
    if (line_lnum < 0 || line_colnr < 0)
    {
	corruption_error("line lnum/col", file_name);
	goto error;
    }

    /* Begin general undo data */
    old_header_seq = undo_read_4c(&bi);
    new_header_seq = undo_read_4c(&bi);
    cur_header_seq = undo_read_4c(&bi);
    num_head = undo_read_4c(&bi);
    seq_last = undo_read_4c(&bi);
    seq_cur = undo_read_4c(&bi);
    seq_time = undo_read_time(&bi);

    /* Optional header fields. */
    for (;;)
    {
	int len = undo_read_byte(&bi);
	int what;

	if (len == 0 || len == EOF)
	    break;
	what = undo_read_byte(&bi);
	switch (what)
	{
	    case UF_LAST_SAVE_NR:
		last_save_nr = undo_read_4c(&bi);
		break;
	    default:
		/* field not supported, skip */
		while (--len >= 0)
		    (void)undo_read_byte(&bi);
	}
    }

    /* uhp_table will store the freshly created undo headers we allocate
     * until we insert them into curbuf. The table remains sorted by the
     * sequence numbers of the headers.
     * When there are no headers uhp_table is NULL. */
    if (num_head > 0)
    {
	uhp_table = (u_header_T **)U_ALLOC_LINE(
					     num_head * sizeof(u_header_T *));
	if (uhp_table == NULL)
	    goto error;
    }

    while ((c = undo_read_2c(&bi)) == UF_HEADER_MAGIC)
    {
	if (num_read_uhps >= num_head)
	{
	    corruption_error("num_head too small", file_name);
	    goto error;
	}

	uhp = unserialize_uhp(&bi, file_name);
	if (uhp == NULL)
	    goto error;
	uhp_table[num_read_uhps++] = uhp;
    }

    if (num_read_uhps != num_head)
    {
	corruption_error("num_head", file_name);
	goto error;
    }
    if (c != UF_HEADER_END_MAGIC)
    {
	corruption_error("end marker", file_name);
	goto error;
    }

#ifdef U_DEBUG
    uhp_table_used = (int *)alloc_clear(
				     (unsigned)(sizeof(int) * num_head + 1));
# define SET_FLAG(j) ++uhp_table_used[j]
#else
# define SET_FLAG(j)
#endif

    /* We have put all of the headers into a table. Now we iterate through the
     * table and swizzle each sequence number we have stored in uh_*_seq into
     * a pointer corresponding to the header with that sequence number. */
    for (i = 0; i < num_head; i++)
    {
	uhp = uhp_table[i];
	if (uhp == NULL)
	    continue;
	for (j = 0; j < num_head; j++)
	    if (uhp_table[j] != NULL && i != j
			      && uhp_table[i]->uh_seq == uhp_table[j]->uh_seq)
	    {
		corruption_error("duplicate uh_seq", file_name);
		goto error;
	    }
	for (j = 0; j < num_head; j++)
	    if (uhp_table[j] != NULL
				  && uhp_table[j]->uh_seq == uhp->uh_next.seq)
	    {
		uhp->uh_next.ptr = uhp_table[j];
		SET_FLAG(j);
		break;
	    }
	for (j = 0; j < num_head; j++)
	    if (uhp_table[j] != NULL
				  && uhp_table[j]->uh_seq == uhp->uh_prev.seq)
	    {
		uhp->uh_prev.ptr = uhp_table[j];
		SET_FLAG(j);
		break;
	    }
	for (j = 0; j < num_head; j++)
	    if (uhp_table[j] != NULL
			      && uhp_table[j]->uh_seq == uhp->uh_alt_next.seq)
	    {
		uhp->uh_alt_next.ptr = uhp_table[j];
		SET_FLAG(j);
		break;
	    }
	for (j = 0; j < num_head; j++)
	    if (uhp_table[j] != NULL
			      && uhp_table[j]->uh_seq == uhp->uh_alt_prev.seq)
	    {
		uhp->uh_alt_prev.ptr = uhp_table[j];
		SET_FLAG(j);
		break;
	    }
	if (old_header_seq > 0 && old_idx < 0 && uhp->uh_seq == old_header_seq)
	{
	    old_idx = i;
	    SET_FLAG(i);
	}
	if (new_header_seq > 0 && new_idx < 0 && uhp->uh_seq == new_header_seq)
	{
	    new_idx = i;
	    SET_FLAG(i);
	}
	if (cur_header_seq > 0 && cur_idx < 0 && uhp->uh_seq == cur_header_seq)
	{
	    cur_idx = i;
	    SET_FLAG(i);
	}
    }

    /* Now that we have read the undo info successfully, free the current undo
     * info and use the info from the file. */
    u_blockfree(curbuf);
    curbuf->b_u_oldhead = old_idx < 0 ? NULL : uhp_table[old_idx];
    curbuf->b_u_newhead = new_idx < 0 ? NULL : uhp_table[new_idx];
    curbuf->b_u_curhead = cur_idx < 0 ? NULL : uhp_table[cur_idx];
    curbuf->b_u_line_ptr = line_ptr;
    curbuf->b_u_line_lnum = line_lnum;
    curbuf->b_u_line_colnr = line_colnr;
    curbuf->b_u_numhead = num_head;
    curbuf->b_u_seq_last = seq_last;
    curbuf->b_u_seq_cur = seq_cur;
    curbuf->b_u_time_cur = seq_time;
    curbuf->b_u_save_nr_last = last_save_nr;
    curbuf->b_u_save_nr_cur = last_save_nr;

    curbuf->b_u_synced = TRUE;
    vim_free(uhp_table);

#ifdef U_DEBUG
    for (i = 0; i < num_head; ++i)
	if (uhp_table_used[i] == 0)
	    EMSGN("uhp_table entry %ld not used, leaking memory", i);
    vim_free(uhp_table_used);
    u_check(TRUE);
#endif

    if (name != NULL)
	smsg((char_u *)_("Finished reading undo file %s"), file_name);
    goto theend;

error:
    vim_free(line_ptr);
    if (uhp_table != NULL)
    {
	for (i = 0; i < num_read_uhps; i++)
	    if (uhp_table[i] != NULL)
		u_free_uhp(uhp_table[i]);
	vim_free(uhp_table);
    }

theend:
#ifdef FEAT_CRYPT
    if (bi.bi_state != NULL)
	crypt_free_state(bi.bi_state);
    vim_free(bi.bi_buffer);
#endif
    if (fp != NULL)
	fclose(fp);
    if (file_name != name)
	vim_free(file_name);
    return;
}
