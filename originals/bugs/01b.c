int32_t WelsRequestMem (PWelsDecoderContext pCtx, const int32_t kiMbWidth, const int32_t kiMbHeight) {
  const int32_t kiPicWidth	= kiMbWidth << 4;
  const int32_t kiPicHeight	= kiMbHeight << 4;
  int32_t iErr = ERR_NONE;

  int32_t iListIdx			= 0;	//, mb_blocks	= 0;
  int32_t	iPicQueueSize		= 0;	// adaptive size of picture queue, = (pSps->iNumRefFrames x 2)
  bool  bNeedChangePicQueue	= true;

  WELS_VERIFY_RETURN_IF (ERR_INFO_INVALID_PARAM, (NULL == pCtx || kiPicWidth <= 0 || kiPicHeight <= 0))

  // Fixed the issue about different gop size over last, 5/17/2010
  // get picture queue size currently
  iPicQueueSize	= GetTargetRefListSize (pCtx);	// adaptive size of picture queue, = (pSps->iNumRefFrames x 2)
  pCtx->iPicQueueNumber = iPicQueueSize;
  if (pCtx->pPicBuff[LIST_0] != NULL
      && pCtx->pPicBuff[LIST_0]->iCapacity ==
      iPicQueueSize)	// comparing current picture queue size requested and previous allocation picture queue
    bNeedChangePicQueue	= false;
  // HD based pic buffer need consider memory size consumed when switch from 720p to other lower size
  WELS_VERIFY_RETURN_IF (ERR_NONE, pCtx->bHaveGotMemory && (kiPicWidth == pCtx->iImgWidthInPixel
                         && kiPicHeight == pCtx->iImgHeightInPixel) && (!bNeedChangePicQueue))	// have same scaled buffer

  // sync update pRefList
  WelsResetRefPic (pCtx);	// added to sync update ref list due to pictures are free

  // for Recycled_Pic_Queue
  for (iListIdx = LIST_0; iListIdx < LIST_A; ++ iListIdx) {
    PPicBuff* ppPic = &pCtx->pPicBuff[iListIdx];
    if (NULL != ppPic && NULL != *ppPic) {
      DestroyPicBuff (ppPic);
    }
  }

  // currently only active for LIST_0 due to have no B frames
  iErr = CreatePicBuff (pCtx, &pCtx->pPicBuff[LIST_0], iPicQueueSize, kiPicWidth, kiPicHeight);
  if (iErr != ERR_NONE)
    return iErr;


