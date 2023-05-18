static int
header_read (SF_PRIVATE *psf, void *ptr, int bytes)
{	int count = 0 ;

	if (psf->headindex >= SIGNED_SIZEOF (psf->header))
		return psf_fread (ptr, 1, bytes, psf) ;

	if (psf->headindex + bytes > SIGNED_SIZEOF (psf->header))
	{	int most ;

		most = SIGNED_SIZEOF (psf->header) - psf->headend ;
		psf_fread (psf->header + psf->headend, 1, most, psf) ;
		memcpy (ptr, psf->header + psf->headend, most) ;
		psf->headend = psf->headindex += most ;
		psf_fread ((char *) ptr + most, bytes - most, 1, psf) ;
		return bytes ;
		} ;

	if (psf->headindex + bytes > psf->headend)
	{	count = psf_fread (psf->header + psf->headend, 1, bytes - (psf->headend - psf->headindex), psf) ;
		if (count != bytes - (int) (psf->headend - psf->headindex))
		{	psf_log_printf (psf, "Error : psf_fread returned short count.\n") ;
			return count ;
			} ;
		psf->headend += count ;
		} ;

	memcpy (ptr, psf->header + psf->headindex, bytes) ;
	psf->headindex += bytes ;

	return bytes ;
} /* header_read */

static void
header_seek (SF_PRIVATE *psf, sf_count_t position, int whence)
{
	switch (whence)
	{	case SEEK_SET :
			if (position > SIGNED_SIZEOF (psf->header))
			{	/* Too much header to cache so just seek instead. */
				psf_fseek (psf, position, whence) ;
				return ;
				} ;
			if (position > psf->headend)
				psf->headend += psf_fread (psf->header + psf->headend, 1, position - psf->headend, psf) ;
			psf->headindex = position ;
			break ;

		case SEEK_CUR :
			if (psf->headindex + position < 0)
				break ;

			if (psf->headindex >= SIGNED_SIZEOF (psf->header))
			{	psf_fseek (psf, position, whence) ;
				return ;
				} ;

			if (psf->headindex + position <= psf->headend)
			{	psf->headindex += position ;
				break ;
				} ;

			if (psf->headindex + position > SIGNED_SIZEOF (psf->header))
			{	/* Need to jump this without caching it. */
				psf->headindex = psf->headend ;
				psf_fseek (psf, position, SEEK_CUR) ;
				break ;
				} ;

			psf->headend += psf_fread (psf->header + psf->headend, 1, position - (psf->headend - psf->headindex), psf) ;
			psf->headindex = psf->headend ;
			break ;

		case SEEK_END :
		default :
			psf_log_printf (psf, "Bad whence param in header_seek().\n") ;
			break ;
		} ;

	return ;
}
