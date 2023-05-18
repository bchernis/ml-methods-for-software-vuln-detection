bool EnhWMFReader::ReadEnhWMF()
{
    sal_uInt32  nStretchBltMode = 0;
    sal_uInt32  nNextPos(0),
                nW(0), nH(0), nColor(0), nIndex(0),
                nDat32(0), nNom1(0), nDen1(0), nNom2(0), nDen2(0);
    sal_Int32   nX32(0), nY32(0), nx32(0), ny32(0);

    bool    bStatus = ReadHeader();
    bool    bHaveDC = false;

    static bool bEnableEMFPlus = ( getenv( "EMF_PLUS_DISABLE" ) == nullptr );

    while( bStatus && nRecordCount-- && pWMF->good())
    {
        sal_uInt32  nRecType(0), nRecSize(0);
        pWMF->ReadUInt32(nRecType).ReadUInt32(nRecSize);

        if ( !pWMF->good() || ( nRecSize < 8 ) || ( nRecSize & 3 ) )     // Parameters are always divisible by 4
        {
            bStatus = false;
            break;
        }

        auto nCurPos = pWMF->Tell();

        if (nEndPos < nCurPos - 8)
        {
            bStatus = false;
            break;
        }

        const sal_uInt32 nMaxPossibleRecSize = nEndPos - (nCurPos - 8);
        if (nRecSize > nMaxPossibleRecSize)
        {
            bStatus = false;
            break;
        }

        nNextPos = nCurPos + (nRecSize - 8);

        if(  !aBmpSaveList.empty()
          && ( nRecType != EMR_STRETCHBLT )
          && ( nRecType != EMR_STRETCHDIBITS )
          ) {
            pOut->ResolveBitmapActions( aBmpSaveList );
        }

        bool bFlag = false;

        SAL_INFO ("vcl.emf", "0x" << std::hex << (nNextPos - nRecSize) <<  "-0x" << nNextPos << " " << record_type_name(nRecType) << " size: " <<  nRecSize << std::dec);

        if( bEnableEMFPlus && nRecType == EMR_GDICOMMENT ) {
            sal_uInt32 length;

            pWMF->ReadUInt32( length );

            SAL_INFO("vcl.emf", "\tGDI comment, length: " << length);

            if( pWMF->good() && length >= 4 && length <= pWMF->remainingSize() ) {
                sal_uInt32 id;

                pWMF->ReadUInt32( id );

                SAL_INFO ("vcl.emf", "\t\tbegin " << (char)(id & 0xff) << (char)((id & 0xff00) >> 8) << (char)((id & 0xff0000) >> 16) << (char)((id & 0xff000000) >> 24) << " id: 0x" << std::hex << id << std::dec);

                // EMF+ comment (FIXME: BE?)
                if( id == 0x2B464D45 && nRecSize >= 12 )
                    // [MS-EMF] 2.3.3: DataSize includes both CommentIdentifier and CommentRecordParm fields.
                    // We have already read 4-byte CommentIdentifier, so reduce length appropriately
                    ReadEMFPlusComment( length-4, bHaveDC );
                // GDIC comment, doesn't do anything useful yet
                else if( id == 0x43494447 && nRecSize >= 12 ) {
                    // TODO: ReadGDIComment()
                } else {
                    SAL_INFO ("vcl.emf", "\t\tunknown id: 0x" << std::hex << id << std::dec);
                }
            }
        }
        else if( !bEMFPlus || bHaveDC || nRecType == EMR_EOF )
        {
            switch( nRecType )
            {
                case EMR_POLYBEZIERTO :
                    ReadAndDrawPolygon<sal_Int32>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyBezier( rPolygon, aTo, aRecordPath ); }, true );
                break;
                case EMR_POLYBEZIER :
                    ReadAndDrawPolygon<sal_Int32>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyBezier( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYGON :
                    ReadAndDrawPolygon<sal_Int32>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolygon( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYLINETO :
                    ReadAndDrawPolygon<sal_Int32>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyLine( rPolygon, aTo, aRecordPath ); }, true );
                break;

                case EMR_POLYLINE :
                    ReadAndDrawPolygon<sal_Int32>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyLine( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYPOLYLINE :
                    ReadAndDrawPolyLine<sal_Int32>();
                break;

                case EMR_POLYPOLYGON :
                    ReadAndDrawPolyPolygon<sal_Int32>();
                break;

                case EMR_SETWINDOWEXTEX :
                {
                    pWMF->ReadUInt32( nW ).ReadUInt32( nH );
                    pOut->SetWinExt( Size( nW, nH ), true);
                }
                break;

                case EMR_SETWINDOWORGEX :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->SetWinOrg( Point( nX32, nY32 ), true);
                }
                break;

                case EMR_SCALEWINDOWEXTEX :
                {
                    pWMF->ReadUInt32( nNom1 ).ReadUInt32( nDen1 ).ReadUInt32( nNom2 ).ReadUInt32( nDen2 );
                    pOut->ScaleWinExt( (double)nNom1 / nDen1, (double)nNom2 / nDen2 );
                }
                break;

                case EMR_SETVIEWPORTORGEX :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->SetDevOrg( Point( nX32, nY32 ) );
                }
                break;

                case EMR_SCALEVIEWPORTEXTEX :
                {
                    pWMF->ReadUInt32( nNom1 ).ReadUInt32( nDen1 ).ReadUInt32( nNom2 ).ReadUInt32( nDen2 );
                    pOut->ScaleDevExt( (double)nNom1 / nDen1, (double)nNom2 / nDen2 );
                }
                break;

                case EMR_SETVIEWPORTEXTEX :
                {
                    pWMF->ReadUInt32( nW ).ReadUInt32( nH );
                    pOut->SetDevExt( Size( nW, nH ) );
                }
                break;

                case EMR_EOF :
                    nRecordCount = 0;
                break;

                case EMR_SETPIXELV :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->DrawPixel( Point( nX32, nY32 ), ReadColor() );
                }
                break;

                case EMR_SETMAPMODE :
                {
                    sal_uInt32 nMapMode;
                    pWMF->ReadUInt32( nMapMode );
                    pOut->SetMapMode( nMapMode );
                }
                break;

                case EMR_SETBKMODE :
                {
                    pWMF->ReadUInt32( nDat32 );
                    pOut->SetBkMode( static_cast<BkMode>(nDat32) );
                }
                break;

                case EMR_SETPOLYFILLMODE :
                break;

                case EMR_SETROP2 :
                {
                    pWMF->ReadUInt32( nDat32 );
                    pOut->SetRasterOp( (WMFRasterOp)nDat32 );
                }
                break;

                case EMR_SETSTRETCHBLTMODE :
                {
                    pWMF->ReadUInt32( nStretchBltMode );
                }
                break;

                case EMR_SETTEXTALIGN :
                {
                    pWMF->ReadUInt32( nDat32 );
                    pOut->SetTextAlign( nDat32 );
                }
                break;

                case EMR_SETTEXTCOLOR :
                {
                    pOut->SetTextColor( ReadColor() );
                }
                break;

                case EMR_SETBKCOLOR :
                {
                    pOut->SetBkColor( ReadColor() );
                }
                break;

                case EMR_OFFSETCLIPRGN :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->MoveClipRegion( Size( nX32, nY32 ) );
                }
                break;

                case EMR_MOVETOEX :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->MoveTo( Point( nX32, nY32 ), bRecordPath );
                }
                break;

                case EMR_INTERSECTCLIPRECT :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 );
                    pOut->IntersectClipRect( ReadRectangle( nX32, nY32, nx32, ny32 ) );
                }
                break;

                case EMR_SAVEDC :
                {
                    pOut->Push();
                }
                break;

                case EMR_RESTOREDC :
                {
                    pOut->Pop();
                }
                break;

                case EMR_SETWORLDTRANSFORM :
                {
                    XForm aTempXForm;
                    *pWMF >> aTempXForm;
                    pOut->SetWorldTransform( aTempXForm );
                }
                break;

                case EMR_MODIFYWORLDTRANSFORM :
                {
                    sal_uInt32  nMode;
                    XForm   aTempXForm;
                    *pWMF >> aTempXForm;
                    pWMF->ReadUInt32( nMode );
                    pOut->ModifyWorldTransform( aTempXForm, nMode );
                }
                break;

                case EMR_SELECTOBJECT :
                {
                    pWMF->ReadUInt32( nIndex );
                    pOut->SelectObject( nIndex );
                }
                break;

                case EMR_CREATEPEN :
                {
                    pWMF->ReadUInt32( nIndex );
                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                    {

                        LineInfo    aLineInfo;
                        sal_uInt32      nStyle;
                        Size        aSize;
                        // #fdo39428 Remove SvStream operator>>(long&)
                        sal_Int32 nTmpW(0), nTmpH(0);

                        pWMF->ReadUInt32( nStyle ).ReadInt32( nTmpW ).ReadInt32( nTmpH );
                        aSize.Width() = nTmpW;
                        aSize.Height() = nTmpH;

                        if ( aSize.Width() )
                            aLineInfo.SetWidth( aSize.Width() );

                        bool bTransparent = false;
                        switch( nStyle & PS_STYLE_MASK )
                        {
                            case PS_DASHDOTDOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 2 );
                            break;
                            case PS_DASHDOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 1 );
                            break;
                            case PS_DOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 0 );
                                aLineInfo.SetDotCount( 1 );
                            break;
                            case PS_DASH :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 0 );
                            break;
                            case PS_NULL :
                                bTransparent = true;
                                aLineInfo.SetStyle( LineStyle::NONE );
                            break;
                            case PS_INSIDEFRAME :
                            case PS_SOLID :
                            default :
                                aLineInfo.SetStyle( LineStyle::Solid );
                        }
                        switch( nStyle & PS_ENDCAP_STYLE_MASK )
                        {
                            case PS_ENDCAP_ROUND :
                                if ( aSize.Width() )
                                {
                                    aLineInfo.SetLineCap( css::drawing::LineCap_ROUND );
                                    break;
                                }
                                SAL_FALLTHROUGH;
                            case PS_ENDCAP_SQUARE :
                                if ( aSize.Width() )
                                {
                                    aLineInfo.SetLineCap( css::drawing::LineCap_SQUARE );
                                    break;
                                }
                                SAL_FALLTHROUGH;
                            case PS_ENDCAP_FLAT :
                            default :
                                aLineInfo.SetLineCap( css::drawing::LineCap_BUTT );
                        }
                        switch( nStyle & PS_JOIN_STYLE_MASK )
                        {
                            case PS_JOIN_ROUND :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Round );
                            break;
                            case PS_JOIN_MITER :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Miter );
                            break;
                            case PS_JOIN_BEVEL :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Bevel );
                            break;
                            default :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::NONE );
                        }
                        pOut->CreateObjectIndexed(nIndex, o3tl::make_unique<WinMtfLineStyle>( ReadColor(), aLineInfo, bTransparent ));
                    }
                }
                break;

                case EMR_EXTCREATEPEN :
                {
                    sal_Int32   elpHatch;
                    sal_uInt32  offBmi, cbBmi, offBits, cbBits, nStyle, nWidth, nBrushStyle, elpNumEntries;
                    Color       aColorRef;

                    pWMF->ReadUInt32( nIndex );
                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                    {
                        pWMF->ReadUInt32( offBmi ).ReadUInt32( cbBmi ).ReadUInt32( offBits ).ReadUInt32( cbBits ). ReadUInt32( nStyle ).ReadUInt32( nWidth ).ReadUInt32( nBrushStyle );
                         aColorRef = ReadColor();
                         pWMF->ReadInt32( elpHatch ).ReadUInt32( elpNumEntries );

                        LineInfo    aLineInfo;
                        if ( nWidth )
                            aLineInfo.SetWidth( nWidth );

                        bool bTransparent = false;

                        switch( nStyle & PS_STYLE_MASK )
                        {
                            case PS_DASHDOTDOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 2 );
                            break;
                            case PS_DASHDOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 1 );
                            break;
                            case PS_DOT :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 0 );
                                aLineInfo.SetDotCount( 1 );
                            break;
                            case PS_DASH :
                                aLineInfo.SetStyle( LineStyle::Dash );
                                aLineInfo.SetDashCount( 1 );
                                aLineInfo.SetDotCount( 0 );
                            break;
                            case PS_NULL :
                                bTransparent = true;
                                aLineInfo.SetStyle( LineStyle::NONE );
                            break;

                            case PS_INSIDEFRAME :
                            case PS_SOLID :
                            default :
                                aLineInfo.SetStyle( LineStyle::Solid );
                        }
                        switch( nStyle & PS_ENDCAP_STYLE_MASK )
                        {
                            case PS_ENDCAP_ROUND :
                                if ( aLineInfo.GetWidth() )
                                {
                                    aLineInfo.SetLineCap( css::drawing::LineCap_ROUND );
                                    break;
                                }
                                SAL_FALLTHROUGH;
                            case PS_ENDCAP_SQUARE :
                                if ( aLineInfo.GetWidth() )
                                {
                                    aLineInfo.SetLineCap( css::drawing::LineCap_SQUARE );
                                    break;
                                }
                                SAL_FALLTHROUGH;
                            case PS_ENDCAP_FLAT :
                            default :
                                aLineInfo.SetLineCap( css::drawing::LineCap_BUTT );
                        }
                        switch( nStyle & PS_JOIN_STYLE_MASK )
                        {
                            case PS_JOIN_ROUND :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Round );
                            break;
                            case PS_JOIN_MITER :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Miter );
                            break;
                            case PS_JOIN_BEVEL :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::Bevel );
                            break;
                            default :
                                aLineInfo.SetLineJoin ( basegfx::B2DLineJoin::NONE );
                        }
                        pOut->CreateObjectIndexed(nIndex, o3tl::make_unique<WinMtfLineStyle>( aColorRef, aLineInfo, bTransparent ));
                    }
                }
                break;

                case EMR_CREATEBRUSHINDIRECT :
                {
                    sal_uInt32  nStyle;
                    pWMF->ReadUInt32( nIndex );
                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                    {
                        pWMF->ReadUInt32( nStyle );
                        pOut->CreateObjectIndexed(nIndex, o3tl::make_unique<WinMtfFillStyle>( ReadColor(), ( nStyle == BS_HOLLOW ) ));
                    }
                }
                break;

                case EMR_DELETEOBJECT :
                {
                    pWMF->ReadUInt32( nIndex );
                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                        pOut->DeleteObject( nIndex );
                }
                break;

                case EMR_ELLIPSE :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 );
                    pOut->DrawEllipse( ReadRectangle( nX32, nY32, nx32, ny32 ) );
                }
                break;

                case EMR_RECTANGLE :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 );
                    pOut->DrawRect( ReadRectangle( nX32, nY32, nx32, ny32 ) );
                }
                break;

                case EMR_ROUNDRECT :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 ).ReadUInt32( nW ).ReadUInt32( nH );
                    Size aSize( Size( nW, nH ) );
                    pOut->DrawRoundRect( ReadRectangle( nX32, nY32, nx32, ny32 ), aSize );
                }
                break;

                case EMR_ARC :
                {
                    sal_uInt32 nStartX, nStartY, nEndX, nEndY;
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 ).ReadUInt32( nStartX ).ReadUInt32( nStartY ).ReadUInt32( nEndX ).ReadUInt32( nEndY );
                    pOut->DrawArc( ReadRectangle( nX32, nY32, nx32, ny32 ), Point( nStartX, nStartY ), Point( nEndX, nEndY ) );
                }
                break;

                case EMR_CHORD :
                {
                    sal_uInt32 nStartX, nStartY, nEndX, nEndY;
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 ).ReadUInt32( nStartX ).ReadUInt32( nStartY ).ReadUInt32( nEndX ).ReadUInt32( nEndY );
                    pOut->DrawChord( ReadRectangle( nX32, nY32, nx32, ny32 ), Point( nStartX, nStartY ), Point( nEndX, nEndY ) );
                }
                break;

                case EMR_PIE :
                {
                    sal_uInt32 nStartX, nStartY, nEndX, nEndY;
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 ).ReadUInt32( nStartX ).ReadUInt32( nStartY ).ReadUInt32( nEndX ).ReadUInt32( nEndY );
                    const Rectangle aRect( ReadRectangle( nX32, nY32, nx32, ny32 ));

                    // #i73608# OutputDevice deviates from WMF
                    // semantics. start==end means full ellipse here.
                    if( nStartX == nEndX && nStartY == nEndY )
                        pOut->DrawEllipse( aRect );
                    else
                        pOut->DrawPie( aRect, Point( nStartX, nStartY ), Point( nEndX, nEndY ) );
                }
                break;

                case EMR_LINETO :
                {
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 );
                    pOut->LineTo( Point( nX32, nY32 ), bRecordPath );
                }
                break;

                case EMR_ARCTO :
                {
                    sal_uInt32 nStartX, nStartY, nEndX, nEndY;
                    pWMF->ReadInt32( nX32 ).ReadInt32( nY32 ).ReadInt32( nx32 ).ReadInt32( ny32 ).ReadUInt32( nStartX ).ReadUInt32( nStartY ).ReadUInt32( nEndX ).ReadUInt32( nEndY );
                    pOut->DrawArc( ReadRectangle( nX32, nY32, nx32, ny32 ), Point( nStartX, nStartY ), Point( nEndX, nEndY ), true );
                }
                break;

                case EMR_BEGINPATH :
                {
                    pOut->ClearPath();
                    bRecordPath = true;
                }
                break;

                case EMR_ABORTPATH :
                    pOut->ClearPath();
                    SAL_FALLTHROUGH;
                case EMR_ENDPATH :
                    bRecordPath = false;
                break;

                case EMR_CLOSEFIGURE :
                    pOut->ClosePath();
                break;

                case EMR_FILLPATH :
                    pOut->StrokeAndFillPath( false, true );
                break;

                case EMR_STROKEANDFILLPATH :
                    pOut->StrokeAndFillPath( true, true );
                break;

                case EMR_STROKEPATH :
                    pOut->StrokeAndFillPath( true, false );
                break;

                case EMR_SELECTCLIPPATH :
                {
                    sal_Int32 nClippingMode;
                    pWMF->ReadInt32(nClippingMode);
                    pOut->SetClipPath(pOut->GetPathObj(), nClippingMode, true);
                }
                break;

                case EMR_EXTSELECTCLIPRGN :
                {
                    sal_Int32 nClippingMode, cbRgnData;
                    pWMF->ReadInt32(cbRgnData);
                    pWMF->ReadInt32(nClippingMode);

                    // This record's region data should be ignored if mode
                    // is RGN_COPY - see EMF spec section 2.3.2.2
                    if (nClippingMode == RGN_COPY)
                    {
                        pOut->SetDefaultClipPath();
                    }
                    else
                    {
                        tools::PolyPolygon aPolyPoly;
                        if (cbRgnData)
                            ImplReadRegion(aPolyPoly, *pWMF, nRecSize);
                        pOut->SetClipPath(aPolyPoly, nClippingMode, false);
                    }

                }
                break;

                case EMR_ALPHABLEND:
                {
                    sal_Int32 xDest(0), yDest(0), cxDest(0), cyDest(0);

                    BLENDFUNCTION aFunc;
                    sal_Int32 xSrc(0), ySrc(0), cxSrc(0), cySrc(0);
                    XForm xformSrc;
                    sal_uInt32 BkColorSrc(0), iUsageSrc(0), offBmiSrc(0);
                    sal_uInt32 cbBmiSrc(0), offBitsSrc(0), cbBitsSrc(0);

                    sal_uInt32   nStart = pWMF->Tell() - 8;
                    pWMF->SeekRel( 0x10 );

                    pWMF->ReadInt32( xDest ).ReadInt32( yDest ).ReadInt32( cxDest ).ReadInt32( cyDest );
                    *pWMF >> aFunc;
                    pWMF->ReadInt32( xSrc ).ReadInt32( ySrc );
                    *pWMF >> xformSrc;
                    pWMF->ReadUInt32( BkColorSrc ).ReadUInt32( iUsageSrc ).ReadUInt32( offBmiSrc ).ReadUInt32( cbBmiSrc )
                               .ReadUInt32( offBitsSrc ).ReadUInt32( cbBitsSrc ).ReadInt32( cxSrc ).ReadInt32( cySrc ) ;

                    sal_uInt32  dwRop = SRCAND|SRCINVERT;
                    Rectangle   aRect( Point( xDest, yDest ), Size( cxDest+1, cyDest+1 ) );

                    if ( (cbBitsSrc > (SAL_MAX_UINT32 - 14)) || ((SAL_MAX_UINT32 - 14) - cbBitsSrc < cbBmiSrc) )
                        bStatus = false;
                    else
                    {
                        const sal_uInt32 nSourceSize = cbBmiSrc + cbBitsSrc + 14;
                        if ( nSourceSize <= ( nEndPos - nStartPos ) )
                        {
                            // we need to read alpha channel data if AlphaFormat of BLENDFUNCTION is
                            // AC_SRC_ALPHA (==0x01). To read it, create a temp DIB-File which is ready
                            // for DIB-5 format
                            const bool bReadAlpha(0x01 == aFunc.aAlphaFormat);
                            const sal_uInt32 nDeltaToDIB5HeaderSize(bReadAlpha ? getDIBV5HeaderSize() - cbBmiSrc : 0);
                            const sal_uInt32 nTargetSize(cbBmiSrc + nDeltaToDIB5HeaderSize + cbBitsSrc + 14);
                            char* pBuf = new char[ nTargetSize ];
                            SvMemoryStream aTmp( pBuf, nTargetSize, StreamMode::READ | StreamMode::WRITE );

                            aTmp.ObjectOwnsMemory( true );

                            // write BM-Header (14 bytes)
                            aTmp.WriteUChar( 'B' )
                                .WriteUChar( 'M' )
                                .WriteUInt32( cbBitsSrc )
                                .WriteUInt16( 0 )
                                .WriteUInt16( 0 )
                                .WriteUInt32( cbBmiSrc + nDeltaToDIB5HeaderSize + 14 );

                            // copy DIBInfoHeader from source (cbBmiSrc bytes)
                            pWMF->Seek( nStart + offBmiSrc );
                            pWMF->ReadBytes(pBuf + 14, cbBmiSrc);

                            if(bReadAlpha)
                            {
                                // need to add values for all stuff that DIBV5Header is bigger
                                // than DIBInfoHeader, all values are correctly initialized to zero,
                                // so we can use memset here
                                memset(pBuf + cbBmiSrc + 14, 0, nDeltaToDIB5HeaderSize);
                            }

                            // copy bitmap data from source (offBitsSrc bytes)
                            pWMF->Seek( nStart + offBitsSrc );
                            pWMF->ReadBytes(pBuf + 14 + nDeltaToDIB5HeaderSize + cbBmiSrc, cbBitsSrc);
                            aTmp.Seek( 0 );

                            // prepare to read and fill BitmapEx
                            BitmapEx aBitmapEx;

                            if(bReadAlpha)
                            {
                                Bitmap aBitmap;
                                AlphaMask aAlpha;

                                if(ReadDIBV5(aBitmap, aAlpha, aTmp))
                                {
                                    aBitmapEx = BitmapEx(aBitmap, aAlpha);
                                }
                            }
                            else
                            {
                                Bitmap aBitmap;

                                if(ReadDIB(aBitmap, aTmp, true))
                                {
                                    if(0xff != aFunc.aSrcConstantAlpha)
                                    {
                                        // add const alpha channel
                                        aBitmapEx = BitmapEx(
                                            aBitmap,
                                            AlphaMask(aBitmap.GetSizePixel(), &aFunc.aSrcConstantAlpha));
                                    }
                                    else
                                    {
                                        // just use Bitmap
                                        aBitmapEx = BitmapEx(aBitmap);
                                    }
                                }
                            }

                            if(!aBitmapEx.IsEmpty())
                            {
                                // test if it is sensible to crop
                                if ( ( cxSrc > 0 ) && ( cySrc > 0 ) &&
                                    ( xSrc >= 0 ) && ( ySrc >= 0 ) &&
                                        ( xSrc + cxSrc < aBitmapEx.GetSizePixel().Width() ) &&
                                            ( ySrc + cySrc < aBitmapEx.GetSizePixel().Height() ) )
                                {
                                    const Rectangle aCropRect( Point( xSrc, ySrc ), Size( cxSrc, cySrc ) );

                                    aBitmapEx.Crop( aCropRect );
                                }

#ifdef DBG_UTIL
                                static bool bDoSaveForVisualControl(false);

                                if(bDoSaveForVisualControl)
                                {
                                    SvFileStream aNew(OUString("c:\\metafile_content.png"), StreamMode::WRITE|StreamMode::TRUNC);
                                    vcl::PNGWriter aPNGWriter(aBitmapEx);
                                    aPNGWriter.Write(aNew);
                                }
#endif
                                aBmpSaveList.emplace_back(new BSaveStruct(aBitmapEx, aRect, dwRop));
                            }
                        }
                    }
                }
                break;

                case EMR_BITBLT :   // PASSTHROUGH INTENDED
                case EMR_STRETCHBLT :
                {
                    sal_Int32   xDest, yDest, cxDest, cyDest, xSrc, ySrc, cxSrc, cySrc;
                    sal_uInt32  dwRop, iUsageSrc, offBmiSrc, cbBmiSrc, offBitsSrc, cbBitsSrc;
                    XForm   xformSrc;

                    sal_uInt32  nStart = pWMF->Tell() - 8;

                    pWMF->SeekRel( 0x10 );
                    pWMF->ReadInt32( xDest ).ReadInt32( yDest ).ReadInt32( cxDest ).ReadInt32( cyDest ).ReadUInt32( dwRop ).ReadInt32( xSrc ).ReadInt32( ySrc )
                            >> xformSrc;
                    pWMF->ReadUInt32( nColor ).ReadUInt32( iUsageSrc ).ReadUInt32( offBmiSrc ).ReadUInt32( cbBmiSrc )
                               .ReadUInt32( offBitsSrc ).ReadUInt32( cbBitsSrc );

                    if ( nRecType == EMR_STRETCHBLT )
                        pWMF->ReadInt32( cxSrc ).ReadInt32( cySrc );
                    else
                        cxSrc = cySrc = 0;

                    Bitmap      aBitmap;
                    Rectangle   aRect( Point( xDest, yDest ), Size( cxDest, cyDest ) );

                    if ( (cbBitsSrc > (SAL_MAX_UINT32 - 14)) || ((SAL_MAX_UINT32 - 14) - cbBitsSrc < cbBmiSrc) )
                        bStatus = false;
                    else
                    {
                        sal_uInt32 nSize = cbBmiSrc + cbBitsSrc + 14;
                        if ( nSize <= ( nEndPos - nStartPos ) )
                        {
                            char* pBuf = new char[ nSize ];
                            SvMemoryStream aTmp( pBuf, nSize, StreamMode::READ | StreamMode::WRITE );
                            aTmp.ObjectOwnsMemory( true );
                            aTmp.WriteUChar( 'B' )
                                .WriteUChar( 'M' )
                                .WriteUInt32( cbBitsSrc )
                                .WriteUInt16( 0 )
                                .WriteUInt16( 0 )
                                .WriteUInt32( cbBmiSrc + 14 );
                            pWMF->Seek( nStart + offBmiSrc );
                            pWMF->ReadBytes(pBuf + 14, cbBmiSrc);
                            pWMF->Seek( nStart + offBitsSrc );
                            pWMF->ReadBytes(pBuf + 14 + cbBmiSrc, cbBitsSrc);
                            aTmp.Seek( 0 );
                            ReadDIB(aBitmap, aTmp, true);

                            // test if it is sensible to crop
                            if ( ( cxSrc > 0 ) && ( cySrc > 0 ) &&
                                ( xSrc >= 0 ) && ( ySrc >= 0 ) &&
                                    ( xSrc + cxSrc <= aBitmap.GetSizePixel().Width() ) &&
                                        ( ySrc + cySrc <= aBitmap.GetSizePixel().Height() ) )
                            {
                                Rectangle aCropRect( Point( xSrc, ySrc ), Size( cxSrc, cySrc ) );
                                aBitmap.Crop( aCropRect );
                            }
                            aBmpSaveList.emplace_back(new BSaveStruct(aBitmap, aRect, dwRop));
                        }
                    }
                }
                break;

                case EMR_STRETCHDIBITS :
                {
                    sal_Int32   xDest, yDest, xSrc, ySrc, cxSrc, cySrc, cxDest, cyDest;
                    sal_uInt32  offBmiSrc, cbBmiSrc, offBitsSrc, cbBitsSrc, iUsageSrc, dwRop;
                    sal_uInt32  nStart = pWMF->Tell() - 8;

                    pWMF->SeekRel( 0x10 );
                    pWMF->ReadInt32( xDest )
                         .ReadInt32( yDest )
                         .ReadInt32( xSrc )
                         .ReadInt32( ySrc )
                         .ReadInt32( cxSrc )
                         .ReadInt32( cySrc )
                         .ReadUInt32( offBmiSrc )
                         .ReadUInt32( cbBmiSrc )
                         .ReadUInt32( offBitsSrc )
                         .ReadUInt32( cbBitsSrc )
                         .ReadUInt32( iUsageSrc )
                         .ReadUInt32( dwRop )
                         .ReadInt32( cxDest )
                         .ReadInt32( cyDest );

                    Bitmap      aBitmap;
                    Rectangle   aRect( Point( xDest, yDest ), Size( cxDest, cyDest ) );

                    if (  ((SAL_MAX_UINT32 - 14)             < cbBitsSrc)
                       || ((SAL_MAX_UINT32 - 14) - cbBitsSrc < cbBmiSrc )
                       )
                    {
                        bStatus = false;
                    }
                    else
                    {
                        sal_uInt32 nSize = cbBmiSrc + cbBitsSrc + 14;
                        if ( nSize <= ( nEndPos - nStartPos ) )
                        {
                            char* pBuf = new char[ nSize ];
                            SvMemoryStream aTmp( pBuf, nSize, StreamMode::READ | StreamMode::WRITE );
                            aTmp.ObjectOwnsMemory( true );
                            aTmp.WriteUChar( 'B' )
                               .WriteUChar( 'M' )
                               .WriteUInt32( cbBitsSrc )
                               .WriteUInt16( 0 )
                               .WriteUInt16( 0 )
                               .WriteUInt32( cbBmiSrc + 14 );
                            pWMF->Seek( nStart + offBmiSrc );
                            pWMF->ReadBytes(pBuf + 14, cbBmiSrc);
                            pWMF->Seek( nStart + offBitsSrc );
                            pWMF->ReadBytes(pBuf + 14 + cbBmiSrc, cbBitsSrc);
                            aTmp.Seek( 0 );
                            ReadDIB(aBitmap, aTmp, true);

                            // test if it is sensible to crop
                            if ( ( cxSrc > 0 ) && ( cySrc > 0 ) &&
                                ( xSrc >= 0 ) && ( ySrc >= 0 ) &&
                                    ( xSrc + cxSrc <= aBitmap.GetSizePixel().Width() ) &&
                                        ( ySrc + cySrc <= aBitmap.GetSizePixel().Height() ) )
                            {
                                Rectangle aCropRect( Point( xSrc, ySrc ), Size( cxSrc, cySrc ) );
                                aBitmap.Crop( aCropRect );
                            }
                            aBmpSaveList.emplace_back(new BSaveStruct(aBitmap, aRect, dwRop));
                        }
                    }
                }
                break;

                case EMR_EXTCREATEFONTINDIRECTW :
                {
                    pWMF->ReadUInt32( nIndex );
                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                    {
                        LOGFONTW aLogFont;
                        pWMF->ReadInt32( aLogFont.lfHeight )
                             .ReadInt32( aLogFont.lfWidth )
                             .ReadInt32( aLogFont.lfEscapement )
                             .ReadInt32( aLogFont.lfOrientation )
                             .ReadInt32( aLogFont.lfWeight )
                             .ReadUChar( aLogFont.lfItalic )
                             .ReadUChar( aLogFont.lfUnderline )
                             .ReadUChar( aLogFont.lfStrikeOut )
                             .ReadUChar( aLogFont.lfCharSet )
                             .ReadUChar( aLogFont.lfOutPrecision )
                             .ReadUChar( aLogFont.lfClipPrecision )
                             .ReadUChar( aLogFont.lfQuality )
                             .ReadUChar( aLogFont.lfPitchAndFamily );

                        sal_Unicode lfFaceName[LF_FACESIZE+1];
                        lfFaceName[LF_FACESIZE] = 0;
                        for (int i = 0; i < LF_FACESIZE; ++i)
                        {
                            sal_uInt16 nChar(0);
                            pWMF->ReadUInt16(nChar);
                            lfFaceName[i] = nChar;
                        }
                        aLogFont.alfFaceName = OUString( lfFaceName );

                        // #i123216# Not used in the test case of #121382# (always identity in XForm), also
                        // no hints in ms docu if FontSize should be scaled with WT. Using with the example
                        // from #i123216# creates errors, so removing.

                        // // #i121382# Need to apply WorldTransform to FontHeight/Width; this should be completely
                        // // changed to basegfx::B2DHomMatrix instead of 'struct XForm', but not now due to time
                        // // constraints and dangers
                        // const XForm& rXF = pOut->GetWorldTransform();
                        // const basegfx::B2DHomMatrix aWT(rXF.eM11, rXF.eM21, rXF.eDx, rXF.eM12, rXF.eM22, rXF.eDy);
                        // const basegfx::B2DVector aTransVec(aWT * basegfx::B2DVector(aLogFont.lfWidth, aLogFont.lfHeight));
                        // aLogFont.lfWidth = aTransVec.getX();
                        // aLogFont.lfHeight = aTransVec.getY();

                        pOut->CreateObjectIndexed(nIndex, o3tl::make_unique<WinMtfFontStyle>( aLogFont ));
                    }
                }
                break;

                case EMR_EXTTEXTOUTA :
                    bFlag = true;
                    SAL_FALLTHROUGH;
                case EMR_EXTTEXTOUTW :
                {
                    sal_Int32   nLeft, nTop, nRight, nBottom, ptlReferenceX, ptlReferenceY, nGfxMode, nXScale, nYScale;
                    sal_uInt32  nOffString, nOptions, offDx;
                    sal_Int32   nLen;
                    std::vector<long> aDX;

                    nCurPos = pWMF->Tell() - 8;

                    pWMF->ReadInt32( nLeft ).ReadInt32( nTop ).ReadInt32( nRight ).ReadInt32( nBottom ).ReadInt32( nGfxMode ).ReadInt32( nXScale ).ReadInt32( nYScale )
                       .ReadInt32( ptlReferenceX ).ReadInt32( ptlReferenceY ).ReadInt32( nLen ).ReadUInt32( nOffString ).ReadUInt32( nOptions );

                    pWMF->SeekRel( 0x10 );
                    pWMF->ReadUInt32( offDx );

                    ComplexTextLayoutFlags nTextLayoutMode = ComplexTextLayoutFlags::Default;
                    if ( nOptions & ETO_RTLREADING )
                        nTextLayoutMode = ComplexTextLayoutFlags::BiDiRtl | ComplexTextLayoutFlags::TextOriginLeft;
                    pOut->SetTextLayoutMode( nTextLayoutMode );
                    SAL_WARN_IF( ( nOptions & ( ETO_PDY | ETO_GLYPH_INDEX ) ) != 0, "vcl.emf", "SJ: ETO_PDY || ETO_GLYPH_INDEX in EMF" );

                    Point aPos( ptlReferenceX, ptlReferenceY );
                    bool bLenSane = nLen > 0 && nLen < static_cast<sal_Int32>( SAL_MAX_UINT32 / sizeof(sal_Int32) );
                    bool bOffStringSane = nOffString <= nEndPos - nCurPos;
                    if (bLenSane && bOffStringSane)
                    {
                        if ( offDx && (( nCurPos + offDx + nLen * 4 ) <= nNextPos ) )
                        {
                            pWMF->Seek( nCurPos + offDx );
                            if ( ( nLen * sizeof(sal_uInt32) ) <= ( nEndPos - pWMF->Tell() ) )
                            {
                                aDX.resize(nLen);
                                for (sal_Int32 i = 0; i < nLen; ++i)
                                {
                                    sal_Int32 val(0);
                                    pWMF->ReadInt32(val);
                                    aDX[i] = val;
                                }
                            }
                        }
                        pWMF->Seek( nCurPos + nOffString );
                        OUString aText;
                        if ( bFlag )
                        {
                            if ( nLen <= static_cast<sal_Int32>( nEndPos - pWMF->Tell() ) )
                            {
                                std::unique_ptr<sal_Char[]> pBuf(new sal_Char[ nLen ]);
                                pWMF->ReadBytes(pBuf.get(), nLen);
                                aText = OUString(pBuf.get(), nLen, pOut->GetCharSet());
                                pBuf.reset();

                                if ( aText.getLength() != nLen )
                                {
                                    std::vector<long> aOldDX(aText.getLength());
                                    aOldDX.swap(aDX);
                                    sal_Int32 nDXLen = std::min<sal_Int32>(nLen, aOldDX.size());
                                    for (sal_Int32 i = 0, j = 0; i < aText.getLength(); ++i)
                                    {
                                        sal_Unicode cUniChar = aText[i];
                                        OString aCharacter(&cUniChar, 1, pOut->GetCharSet());
                                        aDX[i] = 0;
                                        for (sal_Int32 k = 0; ( k < aCharacter.getLength() ) && ( j < nDXLen ) && ( i < aText.getLength() ); ++k)
                                            aDX[ i ] += aOldDX[j++];
                                    }
                                }
                            }
                        }
                        else
                        {
                            if ( ( nLen * sizeof(sal_Unicode) ) <= ( nEndPos - pWMF->Tell() ) )
                            {
                                std::unique_ptr<sal_Unicode[]> pBuf(new sal_Unicode[ nLen ]);
                                pWMF->ReadBytes(pBuf.get(), nLen << 1);
#ifdef OSL_BIGENDIAN
                                sal_Char nTmp, *pTmp = (sal_Char*)( pBuf.get() + nLen );
                                while ( pTmp-- != (sal_Char*)pBuf.get() )
                                {
                                    nTmp = *pTmp--;
                                    pTmp[ 1 ] = *pTmp;
                                    *pTmp = nTmp;
                                }
#endif
                                aText = OUString(pBuf.get(), nLen);
                            }
                        }
                        pOut->DrawText(aPos, aText, aDX.data(), bRecordPath, nGfxMode);
                    }
                }
                break;

                case EMR_POLYBEZIERTO16 :
                    ReadAndDrawPolygon<sal_Int16>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyBezier( rPolygon, aTo, aRecordPath ); }, true );
                break;

                case EMR_POLYBEZIER16 :
                    ReadAndDrawPolygon<sal_Int16>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyBezier( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYGON16 :
                    ReadAndDrawPolygon<sal_Int16>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolygon( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYLINETO16 :
                    ReadAndDrawPolygon<sal_Int16>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyLine( rPolygon, aTo, aRecordPath ); }, true );
                break;

                case EMR_POLYLINE16 :
                    ReadAndDrawPolygon<sal_Int16>( [] ( std::unique_ptr<WinMtfOutput> &pWinMtfOutput, tools::Polygon& rPolygon, bool aTo, bool aRecordPath )
                                                   { pWinMtfOutput->DrawPolyLine( rPolygon, aTo, aRecordPath ); }, false );
                break;

                case EMR_POLYPOLYLINE16 :
                    ReadAndDrawPolyLine<sal_Int16>();
                break;

                case EMR_POLYPOLYGON16 :
                    ReadAndDrawPolyPolygon<sal_Int16>();
                break;

                case EMR_FILLRGN :
                {
                    sal_uInt32 nLen;
                    tools::PolyPolygon aPolyPoly;
                    pWMF->SeekRel( 0x10 );
                    pWMF->ReadUInt32( nLen ).ReadUInt32( nIndex );

                    if ( ImplReadRegion( aPolyPoly, *pWMF, nRecSize ) )
                    {
                        pOut->Push();
                        pOut->SelectObject( nIndex );
                        pOut->DrawPolyPolygon( aPolyPoly );
                        pOut->Pop();
                    }
                }
                break;

                case EMR_CREATEDIBPATTERNBRUSHPT :
                {
                    sal_uInt32  nStart = pWMF->Tell() - 8;
                    Bitmap aBitmap;

                    pWMF->ReadUInt32( nIndex );

                    if ( ( nIndex & ENHMETA_STOCK_OBJECT ) == 0 )
                    {
                        sal_uInt32 usage, offBmi, cbBmi, offBits, cbBits;

                        pWMF->ReadUInt32( usage );
                        pWMF->ReadUInt32( offBmi );
                        pWMF->ReadUInt32( cbBmi );
                        pWMF->ReadUInt32( offBits );
                        pWMF->ReadUInt32( cbBits );

                        if ( (cbBits > (SAL_MAX_UINT32 - 14)) || ((SAL_MAX_UINT32 - 14) - cbBits < cbBmi) )
                           bStatus = false;
                        else if ( offBmi )
                        {
                            sal_uInt32  nSize = cbBmi + cbBits + 14;
                            if ( nSize <= ( nEndPos - nStartPos ) )
                            {
                                char*   pBuf = new char[ nSize ];

                                SvMemoryStream aTmp( pBuf, nSize, StreamMode::READ | StreamMode::WRITE );
                                aTmp.ObjectOwnsMemory( true );
                                aTmp.WriteUChar( 'B' )
                                    .WriteUChar( 'M' )
                                    .WriteUInt32( cbBits )
                                    .WriteUInt16( 0 )
                                    .WriteUInt16( 0 )
                                    .WriteUInt32( cbBmi + 14 );
                                pWMF->Seek( nStart + offBmi );
                                pWMF->ReadBytes(pBuf + 14, cbBmi);
                                pWMF->Seek( nStart + offBits );
                                pWMF->ReadBytes(pBuf + 14 + cbBmi, cbBits);
                                aTmp.Seek( 0 );
                                ReadDIB(aBitmap, aTmp, true);
                            }
                        }
                    }

                    pOut->CreateObjectIndexed(nIndex, o3tl::make_unique<WinMtfFillStyle>( aBitmap ));
                }
                break;

                case EMR_MASKBLT :                  SAL_INFO("vcl.emf", "not implemented 'MaskBlt'");                   break;
                case EMR_PLGBLT :                   SAL_INFO("vcl.emf", "not implemented 'PlgBlt'");                    break;
                case EMR_SETDIBITSTODEVICE :        SAL_INFO("vcl.emf", "not implemented 'SetDIBitsToDevice'");         break;
                case EMR_FRAMERGN :                 SAL_INFO("vcl.emf", "not implemented 'FrameRgn'");                  break;
                case EMR_INVERTRGN :                SAL_INFO("vcl.emf", "not implemented 'InvertRgn'");                 break;
                case EMR_PAINTRGN :                 SAL_INFO("vcl.emf", "not implemented 'PaintRgn'");                  break;
                case EMR_FLATTENPATH :              SAL_INFO("vcl.emf", "not implemented 'FlattenPath'");               break;
                case EMR_WIDENPATH :                SAL_INFO("vcl.emf", "not implemented 'WidenPath'");                 break;
                case EMR_POLYDRAW :                 SAL_INFO("vcl.emf", "not implemented 'Polydraw'");                  break;
                case EMR_SETARCDIRECTION :          SAL_INFO("vcl.emf", "not implemented 'SetArcDirection'");           break;
                case EMR_SETPALETTEENTRIES :        SAL_INFO("vcl.emf", "not implemented 'SetPaletteEntries'");         break;
                case EMR_RESIZEPALETTE :            SAL_INFO("vcl.emf", "not implemented 'ResizePalette'");             break;
                case EMR_EXTFLOODFILL :             SAL_INFO("vcl.emf", "not implemented 'ExtFloodFill'");              break;
                case EMR_ANGLEARC :                 SAL_INFO("vcl.emf", "not implemented 'AngleArc'");                  break;
                case EMR_SETCOLORADJUSTMENT :       SAL_INFO("vcl.emf", "not implemented 'SetColorAdjustment'");        break;
                case EMR_POLYDRAW16 :               SAL_INFO("vcl.emf", "not implemented 'PolyDraw16'");                break;
                case EMR_POLYTEXTOUTA :             SAL_INFO("vcl.emf", "not implemented 'PolyTextOutA'");              break;
                case EMR_POLYTEXTOUTW :             SAL_INFO("vcl.emf", "not implemented 'PolyTextOutW'");              break;
                case EMR_CREATECOLORSPACE :         SAL_INFO("vcl.emf", "not implemented 'CreateColorSpace'");          break;
                case EMR_SETCOLORSPACE :            SAL_INFO("vcl.emf", "not implemented 'SetColorSpace'");             break;
                case EMR_DELETECOLORSPACE :         SAL_INFO("vcl.emf", "not implemented 'DeleteColorSpace'");          break;
                case EMR_GLSRECORD :                SAL_INFO("vcl.emf", "not implemented 'GlsRecord'");                 break;
                case EMR_GLSBOUNDEDRECORD :         SAL_INFO("vcl.emf", "not implemented 'GlsBoundRecord'");            break;
                case EMR_PIXELFORMAT :              SAL_INFO("vcl.emf", "not implemented 'PixelFormat'");               break;
                case EMR_DRAWESCAPE :               SAL_INFO("vcl.emf", "not implemented 'DrawEscape'");                break;
                case EMR_EXTESCAPE :                SAL_INFO("vcl.emf", "not implemented 'ExtEscape'");                 break;
                case EMR_STARTDOC :                 SAL_INFO("vcl.emf", "not implemented 'StartDoc'");                  break;
                case EMR_SMALLTEXTOUT :             SAL_INFO("vcl.emf", "not implemented 'SmallTextOut'");              break;
                case EMR_FORCEUFIMAPPING :          SAL_INFO("vcl.emf", "not implemented 'ForceUFIMapping'");           break;
                case EMR_NAMEDESCAPE :              SAL_INFO("vcl.emf", "not implemented 'NamedEscape'");               break;
                case EMR_COLORCORRECTPALETTE :      SAL_INFO("vcl.emf", "not implemented 'ColorCorrectPalette'");       break;
                case EMR_SETICMPROFILEA :           SAL_INFO("vcl.emf", "not implemented 'SetICMProfileA'");            break;
                case EMR_SETICMPROFILEW :           SAL_INFO("vcl.emf", "not implemented 'SetICMProfileW'");            break;
                case EMR_TRANSPARENTBLT :           SAL_INFO("vcl.emf", "not implemented 'TransparenBlt'");             break;
                case EMR_TRANSPARENTDIB :           SAL_INFO("vcl.emf", "not implemented 'TransparenDib'");             break;
                case EMR_GRADIENTFILL :             SAL_INFO("vcl.emf", "not implemented 'GradientFill'");              break;
                case EMR_SETLINKEDUFIS :            SAL_INFO("vcl.emf", "not implemented 'SetLinkedUFIS'");             break;

                case EMR_SETMAPPERFLAGS :           SAL_INFO("vcl.emf", "not implemented 'SetMapperFlags'");            break;
                case EMR_SETICMMODE :               SAL_INFO("vcl.emf", "not implemented 'SetICMMode'");                break;
                case EMR_CREATEMONOBRUSH :          SAL_INFO("vcl.emf", "not implemented 'CreateMonoBrush'");           break;
                case EMR_SETBRUSHORGEX :            SAL_INFO("vcl.emf", "not implemented 'SetBrushOrgEx'");             break;
                case EMR_SETMETARGN :               SAL_INFO("vcl.emf", "not implemented 'SetMetArgn'");                break;
                case EMR_SETMITERLIMIT :            SAL_INFO("vcl.emf", "not implemented 'SetMiterLimit'");             break;
                case EMR_EXCLUDECLIPRECT :          SAL_INFO("vcl.emf", "not implemented 'ExcludeClipRect'");           break;
                case EMR_REALIZEPALETTE :           SAL_INFO("vcl.emf", "not implemented 'RealizePalette'");            break;
                case EMR_SELECTPALETTE :            SAL_INFO("vcl.emf", "not implemented 'SelectPalette'");             break;
                case EMR_CREATEPALETTE :            SAL_INFO("vcl.emf", "not implemented 'CreatePalette'");             break;
                case EMR_ALPHADIBBLEND :            SAL_INFO("vcl.emf", "not implemented 'AlphaDibBlend'");             break;
                case EMR_SETTEXTJUSTIFICATION :     SAL_INFO("vcl.emf", "not implemented 'SetTextJustification'");      break;

                case EMR_GDICOMMENT :
                case EMR_HEADER :               // has already been read at ReadHeader()
                break;

                default :                           SAL_INFO("vcl.emf", "Unknown Meta Action");                                     break;
            }
        }
        pWMF->Seek( nNextPos );
    }
    if( !aBmpSaveList.empty() )
        pOut->ResolveBitmapActions( aBmpSaveList );

    if ( bStatus )
        pWMF->Seek(nEndPos);

    return bStatus;
}
