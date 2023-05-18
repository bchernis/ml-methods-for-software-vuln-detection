void ReadJPEG( JPEGReader* pJPEGReader, void* pInputStream, long* pLines,
               Size const & previewSize )
{
    jpeg_decompress_struct cinfo;
    ErrorManagerStruct jerr;

    if ( setjmp( jerr.setjmp_buffer ) )
    {
        jpeg_destroy_decompress( &cinfo );
        return;
    }

    cinfo.err = jpeg_std_error( &jerr.pub );
    jerr.pub.error_exit = errorExit;
    jerr.pub.output_message = outputMessage;

    jpeg_create_decompress( &cinfo );
    jpeg_svstream_src( &cinfo, pInputStream );
    SourceManagerStruct *source = reinterpret_cast<SourceManagerStruct*>(cinfo.src);
    jpeg_read_header( &cinfo, TRUE );

    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    cinfo.output_gamma = 1.0;
    cinfo.raw_data_out = FALSE;
    cinfo.quantize_colors = FALSE;

    /* change scale for preview import */
    long nPreviewWidth = previewSize.Width();
    long nPreviewHeight = previewSize.Height();
    if( nPreviewWidth || nPreviewHeight )
    {
        if( nPreviewWidth == 0 )
        {
            nPreviewWidth = ( cinfo.image_width * nPreviewHeight ) / cinfo.image_height;
            if( nPreviewWidth <= 0 )
            {
                nPreviewWidth = 1;
            }
        }
        else if( nPreviewHeight == 0 )
        {
            nPreviewHeight = ( cinfo.image_height * nPreviewWidth ) / cinfo.image_width;
            if( nPreviewHeight <= 0 )
            {
                nPreviewHeight = 1;
            }
        }

        for( cinfo.scale_denom = 1; cinfo.scale_denom < 8; cinfo.scale_denom *= 2 )
        {
            if( cinfo.image_width < nPreviewWidth * cinfo.scale_denom )
                break;
            if( cinfo.image_height < nPreviewHeight * cinfo.scale_denom )
                break;
        }

        if( cinfo.scale_denom > 1 )
        {
            cinfo.dct_method            = JDCT_FASTEST;
            cinfo.do_fancy_upsampling   = FALSE;
            cinfo.do_block_smoothing    = FALSE;
        }
    }

    jpeg_start_decompress( &cinfo );

    long nWidth = cinfo.output_width;
    long nHeight = cinfo.output_height;

    bool bGray = (cinfo.output_components == 1);

    JPEGCreateBitmapParam aCreateBitmapParam;

    aCreateBitmapParam.nWidth = nWidth;
    aCreateBitmapParam.nHeight = nHeight;

    aCreateBitmapParam.density_unit = cinfo.density_unit;
    aCreateBitmapParam.X_density = cinfo.X_density;
    aCreateBitmapParam.Y_density = cinfo.Y_density;
    aCreateBitmapParam.bGray = bGray;

    bool bBitmapCreated = pJPEGReader->CreateBitmap(aCreateBitmapParam);

    if (bBitmapCreated)
    {
        Bitmap::ScopedWriteAccess pAccess(pJPEGReader->GetBitmap());

        if (pAccess)
        {
            int nPixelSize = 3;
            J_COLOR_SPACE best_out_color_space = JCS_RGB;
            ScanlineFormat eScanlineFormat = ScanlineFormat::N24BitTcRgb;
            ScanlineFormat eFinalFormat = pAccess->GetScanlineFormat();

            if (eFinalFormat == ScanlineFormat::N32BitTcBgra)
            {
                best_out_color_space = JCS_EXT_BGRA;
                eScanlineFormat = eFinalFormat;
                nPixelSize = 4;
            }
            else if (eFinalFormat == ScanlineFormat::N32BitTcRgba)
            {
                best_out_color_space = JCS_EXT_RGBA;
                eScanlineFormat = eFinalFormat;
                nPixelSize = 4;
            }
            else if (eFinalFormat == ScanlineFormat::N32BitTcArgb)
            {
                best_out_color_space = JCS_EXT_ARGB;
                eScanlineFormat = eFinalFormat;
                nPixelSize = 4;
            }

            if ( cinfo.jpeg_color_space == JCS_YCbCr )
                cinfo.out_color_space = best_out_color_space;
            else if ( cinfo.jpeg_color_space == JCS_YCCK )
                cinfo.out_color_space = JCS_CMYK;

            if (cinfo.out_color_space != JCS_CMYK &&
                cinfo.out_color_space != JCS_GRAYSCALE &&
                cinfo.out_color_space != best_out_color_space)
            {
                SAL_WARN("vcl.filter", "jpg with unknown out color space, forcing to :" << best_out_color_space);
                cinfo.out_color_space = best_out_color_space;
            }

            JSAMPLE* aRangeLimit = cinfo.sample_range_limit;

            std::vector<sal_uInt8> pScanLineBuffer(nWidth * (bGray ? 1 : nPixelSize));
            std::vector<sal_uInt8> pCYMKBuffer;

            if (cinfo.out_color_space == JCS_CMYK)
            {
                pCYMKBuffer.resize(nWidth * 4);
            }

            std::unique_ptr<BitmapColor[]> pCols;

            if (bGray)
            {
                pCols.reset(new BitmapColor[256]);

                for (sal_uInt16 n = 0; n < 256; n++)
                {
                    const sal_uInt8 cGray = n;
                    pCols[n] = pAccess->GetBestMatchingColor(BitmapColor(cGray, cGray, cGray));
                }
            }

            for (*pLines = 0; *pLines < nHeight && !source->no_data_available; (*pLines)++)
            {
                size_t yIndex = *pLines;

                sal_uInt8* p = (cinfo.out_color_space == JCS_CMYK) ? pCYMKBuffer.data() : pScanLineBuffer.data();
                jpeg_read_scanlines(&cinfo, reinterpret_cast<JSAMPARRAY>(&p), 1);

                if (bGray)
                {
                    for (long x = 0; x < nWidth; ++x)
                    {
                        sal_uInt8 nColorGray = pScanLineBuffer[x];
                        pAccess->SetPixel(yIndex, x, pCols[nColorGray]);
                    }
                }
                else if (cinfo.out_color_space == JCS_CMYK)
                {
                    // convert CMYK to RGB
                    for (long cmyk = 0, x = 0; cmyk < nWidth * 4; cmyk += 4, ++x)
                    {
                        int color_C = 255 - pCYMKBuffer[cmyk + 0];
                        int color_M = 255 - pCYMKBuffer[cmyk + 1];
                        int color_Y = 255 - pCYMKBuffer[cmyk + 2];
                        int color_K = 255 - pCYMKBuffer[cmyk + 3];

                        sal_uInt8 cRed = aRangeLimit[255L - (color_C + color_K)];
                        sal_uInt8 cGreen = aRangeLimit[255L - (color_M + color_K)];
                        sal_uInt8 cBlue = aRangeLimit[255L - (color_Y + color_K)];

                        pAccess->SetPixel(yIndex, x, BitmapColor(cRed, cGreen, cBlue));
                    }
                }
                else
                {
                    pAccess->CopyScanline(yIndex, pScanLineBuffer.data(), eScanlineFormat, pScanLineBuffer.size());
                }

                /* PENDING ??? */
                if (cinfo.err->msg_code == 113)
                    break;
            }
        }
    }

    if (bBitmapCreated)
    {
        jpeg_finish_decompress( &cinfo );
    }
    else
    {
        jpeg_abort_decompress( &cinfo );
    }

    jpeg_destroy_decompress( &cinfo );
}
