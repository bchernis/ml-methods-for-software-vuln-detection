rfbBool rfbProcessFileTransfer(rfbClientPtr cl, uint8_t contentType, uint8_t contentParam, uint32_t size, uint32_t length)
{
    char *buffer=NULL, *p=NULL;
    int retval=0;
    char filename1[MAX_PATH];
    char filename2[MAX_PATH];
    char szFileTime[MAX_PATH];
    struct stat statbuf;
    uint32_t sizeHtmp=0;
    int n=0;
    char timespec[64];
#ifdef LIBVNCSERVER_HAVE_LIBZ
    unsigned char compBuff[sz_rfbBlockSize];
    unsigned long nRawBytes = sz_rfbBlockSize;
    int nRet = 0;
#endif

    FILEXFER_ALLOWED_OR_CLOSE_AND_RETURN("", cl, FALSE);
        
    /*
    rfbLog("rfbProcessFileTransfer(%dtype, %dparam, %dsize, %dlen)\n", contentType, contentParam, size, length);
    */

    switch (contentType) {
    case rfbDirContentRequest:
        switch (contentParam) {
        case rfbRDrivesList: /* Client requests the List of Local Drives */
            /*
            rfbLog("rfbProcessFileTransfer() rfbDirContentRequest: rfbRDrivesList:\n");
            */
            /* Format when filled : "C:\<NULL>D:\<NULL>....Z:\<NULL><NULL>
             *
             * We replace the "\" char following the drive letter and ":"
             * with a char corresponding to the type of drive
             * We obtain something like "C:l<NULL>D:c<NULL>....Z:n\<NULL><NULL>"
             *  Isn't it ugly ?
             * DRIVE_FIXED = 'l'     (local?)
             * DRIVE_REMOVABLE = 'f' (floppy?)
             * DRIVE_CDROM = 'c'
             * DRIVE_REMOTE = 'n'
             */
            
            /* in unix, there are no 'drives'  (We could list mount points though)
             * We fake the root as a "C:" for the Winblows users
             */
            filename2[0]='C';
            filename2[1]=':';
            filename2[2]='l';
            filename2[3]=0;
            filename2[4]=0;
            retval = rfbSendFileTransferMessage(cl, rfbDirPacket, rfbADrivesList, 0, 5, filename2);
            if (buffer!=NULL) free(buffer);
            return retval;
            break;
        case rfbRDirContent: /* Client requests the content of a directory */
            /*
            rfbLog("rfbProcessFileTransfer() rfbDirContentRequest: rfbRDirContent\n");
            */
            if ((buffer = rfbProcessFileTransferReadBuffer(cl, length))==NULL) return FALSE;
            retval = rfbSendDirContent(cl, length, buffer);
            if (buffer!=NULL) free(buffer);
            return retval;
        }
        break;

    case rfbDirPacket:
        rfbLog("rfbProcessFileTransfer() rfbDirPacket\n");
        break;
    case rfbFileAcceptHeader:
        rfbLog("rfbProcessFileTransfer() rfbFileAcceptHeader\n");
        break;
    case rfbCommandReturn:
        rfbLog("rfbProcessFileTransfer() rfbCommandReturn\n");
        break;
    case rfbFileChecksums:
        /* Destination file already exists - the viewer sends the checksums */
        rfbLog("rfbProcessFileTransfer() rfbFileChecksums\n");
        break;
    case rfbFileTransferAccess:
        rfbLog("rfbProcessFileTransfer() rfbFileTransferAccess\n");
        break;

    /*
     * sending from the server to the viewer
     */

    case rfbFileTransferRequest:
        /*
        rfbLog("rfbProcessFileTransfer() rfbFileTransferRequest:\n");
        */
        /* add some space to the end of the buffer as we will be adding a timespec to it */
        if ((buffer = rfbProcessFileTransferReadBuffer(cl, length))==NULL) return FALSE;
        /* The client requests a File */
        if (!rfbFilenameTranslate2UNIX(cl, buffer, filename1, sizeof(filename1)))
        {
          if (buffer!=NULL) free(buffer);
          return FALSE;
        }
        cl->fileTransfer.fd=open(filename1, O_RDONLY, 0744);

        /*
        */
        if (DB) rfbLog("rfbProcessFileTransfer() rfbFileTransferRequest(\"%s\"->\"%s\") Open: %s fd=%d\n", buffer, filename1, (cl->fileTransfer.fd==-1?"Failed":"Success"), cl->fileTransfer.fd);
        
        if (cl->fileTransfer.fd!=-1) {
            if (fstat(cl->fileTransfer.fd, &statbuf)!=0) {
                close(cl->fileTransfer.fd);
                cl->fileTransfer.fd=-1;
            }
            else
            {
              /* Add the File Time Stamp to the filename */
              strftime(timespec, sizeof(timespec), "%m/%d/%Y %H:%M",gmtime(&statbuf.st_ctime));
              buffer=realloc(buffer, length + strlen(timespec) + 2); /* comma, and Null term */
              if (buffer==NULL) {
                  rfbLog("rfbProcessFileTransfer() rfbFileTransferRequest: Failed to malloc %d bytes\n", length + strlen(timespec) + 2);
                  return FALSE;
              }
              strcat(buffer,",");
              strcat(buffer, timespec);
              length = strlen(buffer);
              if (DB) rfbLog("rfbProcessFileTransfer() buffer is now: \"%s\"\n", buffer);
            }
        }

        /* The viewer supports compression if size==1 */
        cl->fileTransfer.compressionEnabled = (size==1);

        /*
        rfbLog("rfbProcessFileTransfer() rfbFileTransferRequest(\"%s\"->\"%s\")%s\n", buffer, filename1, (size==1?" <Compression Enabled>":""));
        */

        /* File Size in bytes, 0xFFFFFFFF (-1) means error */
        retval = rfbSendFileTransferMessage(cl, rfbFileHeader, 0, (cl->fileTransfer.fd==-1 ? -1 : statbuf.st_size), length, buffer);

        if (cl->fileTransfer.fd==-1)
        {
            if (buffer!=NULL) free(buffer);
            return retval;
        }
        /* setup filetransfer stuff */
        cl->fileTransfer.fileSize = statbuf.st_size;
        cl->fileTransfer.numPackets = statbuf.st_size / sz_rfbBlockSize;
        cl->fileTransfer.receiving = 0;
        cl->fileTransfer.sending = 0; /* set when we receive a rfbFileHeader: */

        /* TODO: finish 64-bit file size support */
        sizeHtmp = 0;        
        LOCK(cl->sendMutex);
        if (rfbWriteExact(cl, (char *)&sizeHtmp, 4) < 0) {
          rfbLogPerror("rfbProcessFileTransfer: write");
          rfbCloseClient(cl);
          UNLOCK(cl->sendMutex);
          if (buffer!=NULL) free(buffer);
          return FALSE;
        }
        UNLOCK(cl->sendMutex);
        break;

    case rfbFileHeader:
        /* Destination file (viewer side) is ready for reception (size > 0) or not (size = -1) */
        if (size==-1) {
            rfbLog("rfbProcessFileTransfer() rfbFileHeader (error, aborting)\n");
            close(cl->fileTransfer.fd);
            cl->fileTransfer.fd=-1;
            return TRUE;
        }

        /*
        rfbLog("rfbProcessFileTransfer() rfbFileHeader (%d bytes of a file)\n", size);
        */

        /* Starts the transfer! */
        cl->fileTransfer.sending=1;
        return rfbSendFileTransferChunk(cl);
        break;


    /*
     * sending from the viewer to the server
     */

    case rfbFileTransferOffer:
        /* client is sending a file to us */
        /* buffer contains full path name (plus FileTime) */
        /* size contains size of the file */
        /*
        rfbLog("rfbProcessFileTransfer() rfbFileTransferOffer:\n");
        */
        if ((buffer = rfbProcessFileTransferReadBuffer(cl, length))==NULL) return FALSE;

        /* Parse the FileTime */
        p = strrchr(buffer, ',');
        if (p!=NULL) {
            *p = '\0';
            strcpy(szFileTime, p+1);
        } else
            szFileTime[0]=0;



        /* Need to read in sizeHtmp */
        if ((n = rfbReadExact(cl, (char *)&sizeHtmp, 4)) <= 0) {
            if (n != 0)
                rfbLogPerror("rfbProcessFileTransfer: read sizeHtmp");
            rfbCloseClient(cl);
            /* NOTE: don't forget to free(buffer) if you return early! */
            if (buffer!=NULL) free(buffer);
            return FALSE;
        }
        sizeHtmp = Swap32IfLE(sizeHtmp);
        
        if (!rfbFilenameTranslate2UNIX(cl, buffer, filename1, sizeof(filename1)))
        {
          if (buffer!=NULL) free(buffer);
          return FALSE;
        }

        /* If the file exists... We can send a rfbFileChecksums back to the client before we send an rfbFileAcceptHeader */
        /* TODO: Delta Transfer */

        cl->fileTransfer.fd=open(filename1, O_CREAT|O_WRONLY|O_TRUNC, 0744);
        if (DB) rfbLog("rfbProcessFileTransfer() rfbFileTransferOffer(\"%s\"->\"%s\") %s %s fd=%d\n", buffer, filename1, (cl->fileTransfer.fd==-1?"Failed":"Success"), (cl->fileTransfer.fd==-1?strerror(errno):""), cl->fileTransfer.fd);
        /*
        */
        
        /* File Size in bytes, 0xFFFFFFFF (-1) means error */
        retval = rfbSendFileTransferMessage(cl, rfbFileAcceptHeader, 0, (cl->fileTransfer.fd==-1 ? -1 : 0), length, buffer);
        if (cl->fileTransfer.fd==-1) {
            free(buffer);
            return retval;
        }
        
        /* setup filetransfer stuff */
        cl->fileTransfer.fileSize = size;
        cl->fileTransfer.numPackets = size / sz_rfbBlockSize;
        cl->fileTransfer.receiving = 1;
        cl->fileTransfer.sending = 0;
        break;

    case rfbFilePacket:
        /*
        rfbLog("rfbProcessFileTransfer() rfbFilePacket:\n");
        */
        if ((buffer = rfbProcessFileTransferReadBuffer(cl, length))==NULL) return FALSE;
        if (cl->fileTransfer.fd!=-1) {
            /* buffer contains the contents of the file */
            if (size==0)
                retval=write(cl->fileTransfer.fd, buffer, length);
            else
            {
#ifdef LIBVNCSERVER_HAVE_LIBZ
                /* compressed packet */
                nRet = uncompress(compBuff,&nRawBytes,(const unsigned char*)buffer, length);
		if(nRet == Z_OK)
		  retval=write(cl->fileTransfer.fd, (char*)compBuff, nRawBytes);
		else
		  retval = -1;
#else
                /* Write the file out as received... */
                retval=write(cl->fileTransfer.fd, buffer, length);
#endif
            }
            if (retval==-1)
            {
                close(cl->fileTransfer.fd);
                cl->fileTransfer.fd=-1;
                cl->fileTransfer.sending   = 0;
                cl->fileTransfer.receiving = 0;
            }
        }
        break;

    case rfbEndOfFile:
        if (DB) rfbLog("rfbProcessFileTransfer() rfbEndOfFile\n");
        /*
        */
        if (cl->fileTransfer.fd!=-1)
            close(cl->fileTransfer.fd);
        cl->fileTransfer.fd=-1;
        cl->fileTransfer.sending   = 0;
        cl->fileTransfer.receiving = 0;
        break;

    case rfbAbortFileTransfer:
        if (DB) rfbLog("rfbProcessFileTransfer() rfbAbortFileTransfer\n");
        /*
        */
        if (cl->fileTransfer.fd!=-1)
        {
            close(cl->fileTransfer.fd);
            cl->fileTransfer.fd=-1;
            cl->fileTransfer.sending   = 0;
            cl->fileTransfer.receiving = 0;
        }
        else
        {
            /* We use this message for FileTransfer rights (<=RC18 versions)
             * The client asks for FileTransfer permission
             */
            if (contentParam == 0)
            {
                rfbLog("rfbProcessFileTransfer() File Transfer Permission DENIED! (Client Version <=RC18)\n");
                /* Old method for FileTransfer handshake perimssion (<=RC18) (Deny it)*/
                return rfbSendFileTransferMessage(cl, rfbAbortFileTransfer, 0, -1, 0, "");
            }
            /* New method is allowed */
            if (cl->screen->getFileTransferPermission!=NULL)
            {
                if (cl->screen->getFileTransferPermission(cl)==TRUE)
                {
                    rfbLog("rfbProcessFileTransfer() File Transfer Permission Granted!\n");
                    return rfbSendFileTransferMessage(cl, rfbFileTransferAccess, 0, 1 , 0, ""); /* Permit */
                }
                else
                {
                    rfbLog("rfbProcessFileTransfer() File Transfer Permission DENIED!\n");
                    return rfbSendFileTransferMessage(cl, rfbFileTransferAccess, 0, -1 , 0, ""); /* Deny */
                }
            }
            else
            {
                if (cl->screen->permitFileTransfer)
                {
                    rfbLog("rfbProcessFileTransfer() File Transfer Permission Granted!\n");
                    return rfbSendFileTransferMessage(cl, rfbFileTransferAccess, 0, 1 , 0, ""); /* Permit */
                }
                else
                {
                    rfbLog("rfbProcessFileTransfer() File Transfer Permission DENIED by default!\n");
                    return rfbSendFileTransferMessage(cl, rfbFileTransferAccess, 0, -1 , 0, ""); /* DEFAULT: DENY (for security) */
                }
                
            }
        }
        break;


    case rfbCommand:
        /*
        rfbLog("rfbProcessFileTransfer() rfbCommand:\n");
        */
        if ((buffer = rfbProcessFileTransferReadBuffer(cl, length))==NULL) return FALSE;
        switch (contentParam) {
        case rfbCDirCreate:  /* Client requests the creation of a directory */
            if (!rfbFilenameTranslate2UNIX(cl, buffer, filename1, sizeof(filename1)))
            {
              if (buffer!=NULL) free(buffer);
              return FALSE;
            }

            retval = mkdir(filename1, 0755);
            if (DB) rfbLog("rfbProcessFileTransfer() rfbCommand: rfbCDirCreate(\"%s\"->\"%s\") %s\n", buffer, filename1, (retval==-1?"Failed":"Success"));
            /*
            */
            retval = rfbSendFileTransferMessage(cl, rfbCommandReturn, rfbADirCreate, retval, length, buffer);
            if (buffer!=NULL) free(buffer);
            return retval;
        case rfbCFileDelete: /* Client requests the deletion of a file */
            if (!rfbFilenameTranslate2UNIX(cl, buffer, filename1, sizeof(filename1)))
            {
              if (buffer!=NULL) free(buffer);
              return FALSE;
            }

            if (stat(filename1,&statbuf)==0)
            {
                if (S_ISDIR(statbuf.st_mode))
                    retval = rmdir(filename1);
                else
                    retval = unlink(filename1);
            }
            else retval=-1;
            retval = rfbSendFileTransferMessage(cl, rfbCommandReturn, rfbAFileDelete, retval, length, buffer);
            if (buffer!=NULL) free(buffer);
            return retval;
        case rfbCFileRename: /* Client requests the Renaming of a file/directory */
            p = strrchr(buffer, '*');
            if (p != NULL)
            {
                /* Split into 2 filenames ('*' is a seperator) */
                *p = '\0';
                if (!rfbFilenameTranslate2UNIX(cl, buffer, filename1, sizeof(filename1)))
                {
                  if (buffer!=NULL) free(buffer);
                  return FALSE;
                }

                if (!rfbFilenameTranslate2UNIX(cl, p+1,    filename2, sizeof(filename2)))
                {
                  if (buffer!=NULL) free(buffer);
                  return FALSE;
                }

                retval = rename(filename1,filename2);
                if (DB) rfbLog("rfbProcessFileTransfer() rfbCommand: rfbCFileRename(\"%s\"->\"%s\" -->> \"%s\"->\"%s\") %s\n", buffer, filename1, p+1, filename2, (retval==-1?"Failed":"Success"));
                /*
                */
                /* Restore the buffer so the reply is good */
                *p = '*';
                retval = rfbSendFileTransferMessage(cl, rfbCommandReturn, rfbAFileRename, retval, length, buffer);
                if (buffer!=NULL) free(buffer);
                return retval;
            }
            break;
        }
    
        break;
    }

    /* NOTE: don't forget to free(buffer) if you return early! */
    if (buffer!=NULL) free(buffer);
    return TRUE;
}
