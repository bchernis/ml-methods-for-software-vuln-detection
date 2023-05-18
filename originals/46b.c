static FORCEINLINE NEDMALLOCNOALIASATTR NEDMALLOCPTRATTR void *CallMalloc(void *RESTRICT mspace, size_t size, size_t alignment, unsigned flags) THROWSPEC
{
	void *RESTRICT ret=0;
#if USE_MAGIC_HEADERS
	size_t _alignment=alignment;
	size_t *_ret=0;
	size+=alignment+3*sizeof(size_t);
	_alignment=0;
#endif
#if USE_ALLOCATOR==0
	ret=(flags & M2_ZERO_MEMORY) ? syscalloc(1, size) : sysmalloc(size);	/* magic headers takes care of alignment */
#elif USE_ALLOCATOR==1
	ret=mspace_malloc2((mstate) mspace, size, alignment, flags);
#ifndef ENABLE_FAST_HEAP_DETECTION
	if(ret)
	{
		mchunkptr p=mem2chunk(ret);
		size_t truesize=chunksize(p) - overhead_for(p);
		if(!leastusedaddress || (void *)((mstate) mspace)->least_addr<leastusedaddress) leastusedaddress=(void *)((mstate) mspace)->least_addr;
		if(!largestusedblock || truesize>largestusedblock) largestusedblock=(truesize+mparams.page_size) & ~(mparams.page_size-1);
	}
#endif
#endif
	if(!ret) return 0;
#if DEBUG
	if(flags & M2_ZERO_MEMORY)
	{
		const char *RESTRICT n;
		for(n=(const char *)ret; n<(const char *)ret+size; n++)
		{
			assert(!*n);
		}
	}
#endif
#if USE_MAGIC_HEADERS
	_ret=(size_t *) ret;
	ret=(void *)(_ret+3);
	if(alignment) ret=(void *)(((size_t) ret+alignment-1)&~(alignment-1));
	for(; _ret<(size_t *)ret-2; _ret++) *_ret=*(size_t *)"NEDMALOC";
	_ret[0]=(size_t) mspace;
	_ret[1]=size-3*sizeof(size_t);
#endif
	return ret;
}
