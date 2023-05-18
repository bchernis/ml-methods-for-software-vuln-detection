void SVMConverter::ImplConvertFromSVM1( SvStream& rIStm, GDIMetaFile& rMtf )
{
    const sal_uLong         nPos = rIStm.Tell();
    const SvStreamEndian    nOldFormat = rIStm.GetEndian();

    rIStm.SetEndian( SvStreamEndian::LITTLE );

    char    aCode[ 5 ];
    Size    aPrefSz;

    // read header
    rIStm.ReadBytes(aCode, sizeof(aCode));  // Identifier
    sal_Int16 nSize(0);
    rIStm.ReadInt16( nSize );                                 // Size
    sal_Int16 nVersion(0);
    rIStm.ReadInt16( nVersion );                              // Version
    sal_Int32 nTmp32(0);
    rIStm.ReadInt32( nTmp32 );
    aPrefSz.Width() = nTmp32;                       // PrefSize.Width()
    rIStm.ReadInt32( nTmp32 );
    aPrefSz.Height() = nTmp32;                      // PrefSize.Height()

    // check header-magic and version
    if( rIStm.GetError()
        || ( memcmp( aCode, "SVGDI", sizeof( aCode ) ) != 0 )
        || ( nVersion != 200 ) )
    {
        rIStm.SetError( SVSTREAM_FILEFORMAT_ERROR );
        rIStm.SetEndian( nOldFormat );
        rIStm.Seek( nPos );
        return;
    }

    LineInfo            aLineInfo( LineStyle::NONE, 0 );
    std::stack<std::unique_ptr<LineInfo>> aLIStack;
    ScopedVclPtrInstance< VirtualDevice > aFontVDev;
    rtl_TextEncoding    eActualCharSet = osl_getThreadTextEncoding();
    bool                bFatLine = false;

    tools::Polygon     aActionPoly;
    Rectangle   aRect;
    Point       aPt, aPt1;
    Size        aSz;
    Color       aActionColor;

    sal_uInt32  nUnicodeCommentStreamPos = 0;
    sal_Int32       nUnicodeCommentActionNumber = 0;

    rMtf.SetPrefSize(aPrefSz);

    MapMode aMapMode;
    if (ImplReadMapMode(rIStm, aMapMode))           // MapMode
        rMtf.SetPrefMapMode(aMapMode);

    sal_Int32 nActions(0);
    rIStm.ReadInt32(nActions);                      // Action count
    if (nActions < 0)
    {
        SAL_WARN("vcl.gdi", "svm claims negative action count (" << nActions << ")");
        nActions = 0;
    }

    const size_t nMinActionSize = sizeof(sal_uInt16) + sizeof(sal_Int32);
    const size_t nMaxPossibleActions = rIStm.remainingSize() / nMinActionSize;
    if (static_cast<sal_uInt32>(nActions) > nMaxPossibleActions)
    {
        SAL_WARN("vcl.gdi", "svm claims more actions (" << nActions << ") than stream could provide, truncating");
        nActions = nMaxPossibleActions;
    }

    size_t nLastPolygonAction(0);

    for (sal_Int32 i = 0; i < nActions && rIStm.good(); ++i)
    {
        sal_Int16 nType(0);
        rIStm.ReadInt16(nType);
        sal_Int32 nActBegin = rIStm.Tell();
        sal_Int32 nActionSize(0);
        rIStm.ReadInt32(nActionSize);

        SAL_WARN_IF( ( nType > 33 ) && ( nType < 1024 ), "vcl.gdi", "Unknown GDIMetaAction while converting!" );

        switch( nType )
        {
            case GDI_PIXEL_ACTION:
            {
                ReadPair( rIStm, aPt );
                ImplReadColor( rIStm, aActionColor );
                rMtf.AddAction( new MetaPixelAction( aPt, aActionColor ) );
            }
            break;

            case GDI_POINT_ACTION:
            {
                ReadPair( rIStm, aPt );
                rMtf.AddAction( new MetaPointAction( aPt ) );
            }
            break;

            case GDI_LINE_ACTION:
            {
                ReadPair( rIStm, aPt );
                ReadPair( rIStm, aPt1 );
                rMtf.AddAction( new MetaLineAction( aPt, aPt1, aLineInfo ) );
            }
            break;

            case GDI_LINEJOIN_ACTION :
            {
                sal_Int16 nLineJoin(0);
                rIStm.ReadInt16( nLineJoin );
                aLineInfo.SetLineJoin((basegfx::B2DLineJoin)nLineJoin);
            }
            break;

            case GDI_LINECAP_ACTION :
            {
                sal_Int16 nLineCap(0);
                rIStm.ReadInt16( nLineCap );
                aLineInfo.SetLineCap((css::drawing::LineCap)nLineCap);
            }
            break;

            case GDI_LINEDASHDOT_ACTION :
            {
                sal_Int16 a(0);
                sal_Int32 b(0);

                rIStm.ReadInt16( a ); aLineInfo.SetDashCount(a);
                rIStm.ReadInt32( b ); aLineInfo.SetDashLen(b);
                rIStm.ReadInt16( a ); aLineInfo.SetDotCount(a);
                rIStm.ReadInt32( b ); aLineInfo.SetDotLen(b);
                rIStm.ReadInt32( b ); aLineInfo.SetDistance(b);

                if(((aLineInfo.GetDashCount() && aLineInfo.GetDashLen())
                    || (aLineInfo.GetDotCount() && aLineInfo.GetDotLen()))
                    && aLineInfo.GetDistance())
                {
                    aLineInfo.SetStyle(LineStyle::Dash);
                }
            }
            break;

            case GDI_EXTENDEDPOLYGON_ACTION :
            {
                // read the tools::PolyPolygon in every case
                tools::PolyPolygon aInputPolyPolygon;
                ImplReadExtendedPolyPolygonAction(rIStm, aInputPolyPolygon);

                // now check if it can be set somewhere
                if(nLastPolygonAction < rMtf.GetActionSize())
                {
                    MetaPolyLineAction* pPolyLineAction = dynamic_cast< MetaPolyLineAction* >(rMtf.GetAction(nLastPolygonAction));

                    if(pPolyLineAction)
                    {
                        // replace MetaPolyLineAction when we have a single polygon. Do not rely on the
                        // same point count; the originally written GDI_POLYLINE_ACTION may have been
                        // Subdivided for better quality for older usages
                        if(1 == aInputPolyPolygon.Count())
                        {
                            MetaAction* pAction = rMtf.ReplaceAction(
                                new MetaPolyLineAction(
                                    aInputPolyPolygon.GetObject(0),
                                    pPolyLineAction->GetLineInfo()),
                                nLastPolygonAction);
                            if(pAction)
                                pAction->Delete();
                        }
                    }
                    else
                    {
                        MetaPolyPolygonAction* pPolyPolygonAction = dynamic_cast< MetaPolyPolygonAction* >(rMtf.GetAction(nLastPolygonAction));

                        if(pPolyPolygonAction)
                        {
                            // replace MetaPolyPolygonAction when we have a curved polygon. Do rely on the
                            // same sub-polygon count
                            if(pPolyPolygonAction->GetPolyPolygon().Count() == aInputPolyPolygon.Count())
                            {
                                MetaAction* pAction = rMtf.ReplaceAction(
                                    new MetaPolyPolygonAction(
                                        aInputPolyPolygon),
                                    nLastPolygonAction);
                                if(pAction)
                                    pAction->Delete();
                            }
                        }
                        else
                        {
                            MetaPolygonAction* pPolygonAction = dynamic_cast< MetaPolygonAction* >(rMtf.GetAction(nLastPolygonAction));

                            if(pPolygonAction)
                            {
                                // replace MetaPolygonAction
                                if(1 == aInputPolyPolygon.Count())
                                {
                                    MetaAction* pAction = rMtf.ReplaceAction(
                                        new MetaPolygonAction(
                                            aInputPolyPolygon.GetObject(0)),
                                        nLastPolygonAction);
                                    if(pAction)
                                        pAction->Delete();
                                }
                            }
                        }
                    }
                }
            }
            break;

            case GDI_RECT_ACTION:
            {
                ImplReadRect( rIStm, aRect );
                sal_Int32 nTmp(0), nTmp1(0);
                rIStm.ReadInt32( nTmp ).ReadInt32( nTmp1 );

                if( nTmp || nTmp1 )
                    rMtf.AddAction( new MetaRoundRectAction( aRect, nTmp, nTmp1 ) );
                else
                {
                    rMtf.AddAction( new MetaRectAction( aRect ) );

                    if( bFatLine )
                        rMtf.AddAction( new MetaPolyLineAction( aRect, aLineInfo ) );
                }
            }
            break;

            case GDI_ELLIPSE_ACTION:
            {
                ImplReadRect( rIStm, aRect );

                if( bFatLine )
                {
                    const tools::Polygon aPoly( aRect.Center(), aRect.GetWidth() >> 1, aRect.GetHeight() >> 1 );

                    rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR ) );
                    rMtf.AddAction( new MetaLineColorAction( COL_TRANSPARENT, false ) );
                    rMtf.AddAction( new MetaPolygonAction( aPoly ) );
                    rMtf.AddAction( new MetaPopAction() );
                    rMtf.AddAction( new MetaPolyLineAction( aPoly, aLineInfo ) );
                }
                else
                    rMtf.AddAction( new MetaEllipseAction( aRect ) );
            }
            break;

            case GDI_ARC_ACTION:
            {
                ImplReadRect( rIStm, aRect );
                ReadPair( rIStm, aPt );
                ReadPair( rIStm, aPt1 );

                if( bFatLine )
                {
                    const tools::Polygon aPoly( aRect, aPt, aPt1, PolyStyle::Arc );

                    rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR ) );
                    rMtf.AddAction( new MetaLineColorAction( COL_TRANSPARENT, false ) );
                    rMtf.AddAction( new MetaPolygonAction( aPoly ) );
                    rMtf.AddAction( new MetaPopAction() );
                    rMtf.AddAction( new MetaPolyLineAction( aPoly, aLineInfo ) );
                }
                else
                    rMtf.AddAction( new MetaArcAction( aRect, aPt, aPt1 ) );
            }
            break;

            case GDI_PIE_ACTION:
            {
                ImplReadRect( rIStm, aRect );
                ReadPair( rIStm, aPt );
                ReadPair( rIStm, aPt1 );

                if( bFatLine )
                {
                    const tools::Polygon aPoly( aRect, aPt, aPt1, PolyStyle::Pie );

                    rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR ) );
                    rMtf.AddAction( new MetaLineColorAction( COL_TRANSPARENT, false ) );
                    rMtf.AddAction( new MetaPolygonAction( aPoly ) );
                    rMtf.AddAction( new MetaPopAction() );
                    rMtf.AddAction( new MetaPolyLineAction( aPoly, aLineInfo ) );
                }
                else
                    rMtf.AddAction( new MetaPieAction( aRect, aPt, aPt1 ) );
            }
            break;

            case GDI_INVERTRECT_ACTION:
            case GDI_HIGHLIGHTRECT_ACTION:
            {
                ImplReadRect( rIStm, aRect );
                rMtf.AddAction( new MetaPushAction( PushFlags::RASTEROP ) );
                rMtf.AddAction( new MetaRasterOpAction( RasterOp::Invert ) );
                rMtf.AddAction( new MetaRectAction( aRect ) );
                rMtf.AddAction( new MetaPopAction() );
            }
            break;

            case GDI_POLYLINE_ACTION:
            {
                if (ImplReadPoly(rIStm, aActionPoly))
                {
                    nLastPolygonAction = rMtf.GetActionSize();

                    if( bFatLine )
                        rMtf.AddAction( new MetaPolyLineAction( aActionPoly, aLineInfo ) );
                    else
                        rMtf.AddAction( new MetaPolyLineAction( aActionPoly ) );
                }
            }
            break;

            case GDI_POLYGON_ACTION:
            {
                if (ImplReadPoly(rIStm, aActionPoly))
                {
                    if( bFatLine )
                    {
                        rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR ) );
                        rMtf.AddAction( new MetaLineColorAction( COL_TRANSPARENT, false ) );
                        rMtf.AddAction( new MetaPolygonAction( aActionPoly ) );
                        rMtf.AddAction( new MetaPopAction() );
                        rMtf.AddAction( new MetaPolyLineAction( aActionPoly, aLineInfo ) );
                    }
                    else
                    {
                        nLastPolygonAction = rMtf.GetActionSize();
                        rMtf.AddAction( new MetaPolygonAction( aActionPoly ) );
                    }
                }
            }
            break;

            case GDI_POLYPOLYGON_ACTION:
            {
                tools::PolyPolygon aPolyPoly;

                if (ImplReadPolyPoly(rIStm, aPolyPoly))
                {
                    if( bFatLine )
                    {
                        rMtf.AddAction( new MetaPushAction( PushFlags::LINECOLOR ) );
                        rMtf.AddAction( new MetaLineColorAction( COL_TRANSPARENT, false ) );
                        rMtf.AddAction( new MetaPolyPolygonAction( aPolyPoly ) );
                        rMtf.AddAction( new MetaPopAction() );

                        for( sal_uInt16 nPoly = 0, nCount = aPolyPoly.Count(); nPoly < nCount; nPoly++ )
                            rMtf.AddAction( new MetaPolyLineAction( aPolyPoly[ nPoly ], aLineInfo ) );
                    }
                    else
                    {
                        nLastPolygonAction = rMtf.GetActionSize();
                        rMtf.AddAction( new MetaPolyPolygonAction( aPolyPoly ) );
                    }
                }
            }
            break;

            case GDI_FONT_ACTION:
            {
                vcl::Font   aFont;
                char        aName[LF_FACESIZE+1];

                ImplReadColor( rIStm, aActionColor ); aFont.SetColor( aActionColor );
                ImplReadColor( rIStm, aActionColor ); aFont.SetFillColor( aActionColor );
                size_t nRet = rIStm.ReadBytes(aName, LF_FACESIZE);
                aName[nRet] = 0;
                aFont.SetFamilyName( OUString( aName, strlen(aName), rIStm.GetStreamCharSet() ) );

                sal_Int32 nWidth(0), nHeight(0);
                rIStm.ReadInt32(nWidth).ReadInt32(nHeight);
                sal_Int16 nCharOrient(0), nLineOrient(0);
                rIStm.ReadInt16(nCharOrient).ReadInt16(nLineOrient);
                sal_Int16 nCharSet(0), nFamily(0), nPitch(0), nAlign(0), nWeight(0), nUnderline(0), nStrikeout(0);
                rIStm.ReadInt16(nCharSet).ReadInt16(nFamily).ReadInt16(nPitch).ReadInt16(nAlign).ReadInt16(nWeight).ReadInt16(nUnderline).ReadInt16(nStrikeout);
                bool bItalic(false), bOutline(false), bShadow(false), bTransparent(false);
                rIStm.ReadCharAsBool(bItalic).ReadCharAsBool(bOutline).ReadCharAsBool(bShadow).ReadCharAsBool(bTransparent);

                aFont.SetFontSize( Size( nWidth, nHeight ) );
                aFont.SetCharSet( (rtl_TextEncoding) nCharSet );
                aFont.SetFamily( (FontFamily) nFamily );
                aFont.SetPitch( (FontPitch) nPitch );
                aFont.SetAlignment( (FontAlign) nAlign );
                aFont.SetWeight( ( nWeight == 1 ) ? WEIGHT_LIGHT : ( nWeight == 2 ) ? WEIGHT_NORMAL :
                                 ( nWeight == 3 ) ? WEIGHT_BOLD : WEIGHT_DONTKNOW );
                aFont.SetUnderline( (FontLineStyle) nUnderline );
                aFont.SetStrikeout( (FontStrikeout) nStrikeout );
                aFont.SetItalic( bItalic ? ITALIC_NORMAL : ITALIC_NONE );
                aFont.SetOutline( bOutline );
                aFont.SetShadow( bShadow );
                aFont.SetOrientation( nLineOrient );
                aFont.SetTransparent( bTransparent );

                eActualCharSet = aFont.GetCharSet();
                if ( eActualCharSet == RTL_TEXTENCODING_DONTKNOW )
                    eActualCharSet = osl_getThreadTextEncoding();

                rMtf.AddAction( new MetaFontAction( aFont ) );
                rMtf.AddAction( new MetaTextAlignAction( aFont.GetAlignment() ) );
                rMtf.AddAction( new MetaTextColorAction( aFont.GetColor() ) );
                rMtf.AddAction( new MetaTextFillColorAction( aFont.GetFillColor(), !aFont.IsTransparent() ) );

                // #106172# Track font relevant data in shadow VDev
                aFontVDev->SetFont( aFont );
            }
            break;

            case GDI_TEXT_ACTION:
            {
                sal_Int32 nIndex(0), nLen(0), nTmp(0);

                ReadPair( rIStm, aPt ).ReadInt32( nIndex ).ReadInt32( nLen ).ReadInt32( nTmp );
                if (nTmp > 0)
                {
                    OString aByteStr = read_uInt8s_ToOString(rIStm, nTmp);
                    sal_uInt8 nTerminator = 0;
                    rIStm.ReadUChar( nTerminator );
                    SAL_WARN_IF( nTerminator != 0, "vcl.gdi", "expected string to be NULL terminated" );

                    OUString aStr(OStringToOUString(aByteStr, eActualCharSet));
                    if ( nUnicodeCommentActionNumber == i )
                        ImplReadUnicodeComment( nUnicodeCommentStreamPos, rIStm, aStr );
                    rMtf.AddAction( new MetaTextAction( aPt, aStr, nIndex, nLen ) );
                }
                rIStm.Seek( nActBegin + nActionSize );
            }
            break;

            case GDI_TEXTARRAY_ACTION:
            {
                sal_Int32 nIndex(0), nLen(0), nAryLen(0), nTmp(0);

                ReadPair( rIStm, aPt ).ReadInt32( nIndex ).ReadInt32( nLen ).ReadInt32( nTmp ).ReadInt32( nAryLen );
                if (nTmp > 0)
                {
                    OString aByteStr = read_uInt8s_ToOString(rIStm, nTmp);
                    sal_uInt8 nTerminator = 0;
                    rIStm.ReadUChar( nTerminator );
                    SAL_WARN_IF( nTerminator != 0, "vcl.gdi", "expected string to be NULL terminated" );

                    OUString aStr(OStringToOUString(aByteStr, eActualCharSet));

                    std::unique_ptr<long[]> pDXAry;
                    sal_Int32 nDXAryLen = 0;
                    if (nAryLen > 0)
                    {
                        const size_t nMinRecordSize = sizeof(sal_Int32);
                        const size_t nMaxRecords = rIStm.remainingSize() / nMinRecordSize;
                        if (static_cast<sal_uInt32>(nAryLen) > nMaxRecords)
                        {
                            SAL_WARN("vcl.gdi", "Parsing error: " << nMaxRecords <<
                                     " max possible entries, but " << nAryLen << " claimed, truncating");
                            nAryLen = nMaxRecords;
                        }

                        sal_Int32 nStrLen( aStr.getLength() );

                        nDXAryLen = std::max(nAryLen, nStrLen);

                        if (nDXAryLen < nLen)
                        {
                            //MetaTextArrayAction ctor expects pDXAry to be >= nLen if set, so if this can't
                            //be achieved, don't read it, it's utterly broken.
                            SAL_WARN("vcl.gdi", "dxary too short, discarding completely");
                            rIStm.SeekRel(sizeof(sal_Int32) * nDXAryLen);
                            nLen = 0;
                            nIndex = 0;
                        }
                        else
                        {
                            pDXAry.reset(new long[nDXAryLen]);

                            for (sal_Int32 j = 0; j < nAryLen; ++j)
                                rIStm.ReadInt32( nTmp ), pDXAry[ j ] = nTmp;

                            // #106172# Add last DX array elem, if missing
                            if( nAryLen != nStrLen )
                            {
                                if (nAryLen+1 == nStrLen && nIndex >= 0)
                                {
                                    std::unique_ptr<long[]> pTmpAry(new long[nStrLen]);

                                    aFontVDev->GetTextArray( aStr, pTmpAry.get(), nIndex, nLen );

                                    // now, the difference between the
                                    // last and the second last DX array
                                    // is the advancement for the last
                                    // glyph. Thus, to complete our meta
                                    // action's DX array, just add that
                                    // difference to last elem and store
                                    // in very last.
                                    if( nStrLen > 1 )
                                        pDXAry[ nStrLen-3 ] = pDXAry[ nStrLen-2 ] + pTmpAry[ nStrLen-1 ] - pTmpAry[ nStrLen-2 ];
                                    else
                                        pDXAry[ nStrLen-1 ] = pTmpAry[ nStrLen-1 ]; // len=1: 0th position taken to be 0
                                }
#ifdef DBG_UTIL
                                else
                                    OSL_FAIL("More than one DX array element missing on SVM import");
#endif
                            }
                        }
                    }
                    if ( nUnicodeCommentActionNumber == i )
                        ImplReadUnicodeComment( nUnicodeCommentStreamPos, rIStm, aStr );
                    rMtf.AddAction( new MetaTextArrayAction( aPt, aStr, pDXAry.get(), nIndex, nLen ) );
                }
                rIStm.Seek( nActBegin + nActionSize );
            }
            break;

            case GDI_STRETCHTEXT_ACTION:
            {
                sal_Int32 nIndex(0), nLen(0), nWidth(0), nTmp(0);

                ReadPair( rIStm, aPt ).ReadInt32( nIndex ).ReadInt32( nLen ).ReadInt32( nTmp ).ReadInt32( nWidth );
                if (nTmp > 0)
                {
                    OString aByteStr = read_uInt8s_ToOString(rIStm, nTmp);
                    sal_uInt8 nTerminator = 0;
                    rIStm.ReadUChar( nTerminator );
                    SAL_WARN_IF( nTerminator != 0, "vcl.gdi", "expected string to be NULL terminated" );

                    OUString aStr(OStringToOUString(aByteStr, eActualCharSet));
                    if ( nUnicodeCommentActionNumber == i )
                        ImplReadUnicodeComment( nUnicodeCommentStreamPos, rIStm, aStr );
                    rMtf.AddAction( new MetaStretchTextAction( aPt, nWidth, aStr, nIndex, nLen ) );
                }
                rIStm.Seek( nActBegin + nActionSize );
            }
            break;

            case GDI_BITMAP_ACTION:
            {
                Bitmap aBmp;

                ReadPair( rIStm, aPt );
                ReadDIB(aBmp, rIStm, true);
                rMtf.AddAction( new MetaBmpAction( aPt, aBmp ) );
            }
            break;

            case GDI_BITMAPSCALE_ACTION:
            {
                Bitmap aBmp;

                ReadPair( rIStm, aPt );
                ReadPair( rIStm, aSz );
                ReadDIB(aBmp, rIStm, true);
                rMtf.AddAction( new MetaBmpScaleAction( aPt, aSz, aBmp ) );
            }
            break;

            case GDI_BITMAPSCALEPART_ACTION:
            {
                Bitmap  aBmp;
                Size    aSz2;

                ReadPair( rIStm, aPt );
                ReadPair( rIStm, aSz );
                ReadPair( rIStm, aPt1 );
                ReadPair( rIStm, aSz2 );
                ReadDIB(aBmp, rIStm, true);
                rMtf.AddAction( new MetaBmpScalePartAction( aPt, aSz, aPt1, aSz2, aBmp ) );
            }
            break;

            case GDI_PEN_ACTION:
            {
                sal_Int32 nPenWidth;
                sal_Int16 nPenStyle;

                ImplReadColor( rIStm, aActionColor );
                rIStm.ReadInt32( nPenWidth ).ReadInt16( nPenStyle );

                aLineInfo.SetStyle( nPenStyle ? LineStyle::Solid : LineStyle::NONE );
                aLineInfo.SetWidth( nPenWidth );
                bFatLine = nPenStyle && !aLineInfo.IsDefault();

                rMtf.AddAction( new MetaLineColorAction( aActionColor, nPenStyle != 0 ) );
            }
            break;

            case GDI_FILLBRUSH_ACTION:
            {
                sal_Int16 nBrushStyle;

                ImplReadColor( rIStm, aActionColor );
                rIStm.SeekRel( 6 );
                rIStm.ReadInt16( nBrushStyle );
                rMtf.AddAction( new MetaFillColorAction( aActionColor, nBrushStyle != 0 ) );
                rIStm.SeekRel( 2 );
            }
            break;

            case GDI_MAPMODE_ACTION:
            {
                if (ImplReadMapMode(rIStm, aMapMode))
                {
                    rMtf.AddAction(new MetaMapModeAction(aMapMode));

                    // #106172# Track font relevant data in shadow VDev
                    aFontVDev->SetMapMode(aMapMode);
                };
            }
            break;

            case GDI_CLIPREGION_ACTION:
            {
                vcl::Region  aRegion;
                sal_Int16   nRegType;
                sal_Int16   bIntersect;
                bool    bClip = false;

                rIStm.ReadInt16( nRegType ).ReadInt16( bIntersect );
                ImplReadRect( rIStm, aRect );

                switch( nRegType )
                {
                    case 0:
                    break;

                    case 1:
                    {
                        Rectangle aRegRect;

                        ImplReadRect( rIStm, aRegRect );
                        aRegion = vcl::Region( aRegRect );
                        bClip = true;
                    }
                    break;

                    case 2:
                    {
                        if (ImplReadPoly(rIStm, aActionPoly))
                        {
                            aRegion = vcl::Region( aActionPoly );
                            bClip = true;
                        }
                    }
                    break;

                    case 3:
                    {
                        bool bSuccess = true;
                        tools::PolyPolygon aPolyPoly;
                        sal_Int32 nPolyCount32(0);
                        rIStm.ReadInt32(nPolyCount32);
                        sal_uInt16 nPolyCount(nPolyCount32);

                        for (sal_uInt16 j = 0; j < nPolyCount && rIStm.good(); ++j)
                        {
                            if (!ImplReadPoly(rIStm, aActionPoly))
                            {
                                bSuccess = false;
                                break;
                            }
                            aPolyPoly.Insert(aActionPoly);
                        }

                        if (bSuccess)
                        {
                            aRegion = vcl::Region( aPolyPoly );
                            bClip = true;
                        }
                    }
                    break;
                }

                if( bIntersect )
                    aRegion.Intersect( aRect );

                rMtf.AddAction( new MetaClipRegionAction( aRegion, bClip ) );
            }
            break;

            case GDI_MOVECLIPREGION_ACTION:
            {
                sal_Int32 nTmp(0), nTmp1(0);
                rIStm.ReadInt32( nTmp ).ReadInt32( nTmp1 );
                rMtf.AddAction( new MetaMoveClipRegionAction( nTmp, nTmp1 ) );
            }
            break;

            case GDI_ISECTCLIPREGION_ACTION:
            {
                ImplReadRect( rIStm, aRect );
                rMtf.AddAction( new MetaISectRectClipRegionAction( aRect ) );
            }
            break;

            case GDI_RASTEROP_ACTION:
            {
                RasterOp    eRasterOp;
                sal_Int16       nRasterOp;

                rIStm.ReadInt16( nRasterOp );

                switch( nRasterOp )
                {
                    case 1:
                        eRasterOp = RasterOp::Invert;
                    break;

                    case 4:
                    case 5:
                        eRasterOp = RasterOp::Xor;
                    break;

                    default:
                        eRasterOp = RasterOp::OverPaint;
                    break;
                }

                rMtf.AddAction( new MetaRasterOpAction( eRasterOp ) );
            }
            break;

            case GDI_PUSH_ACTION:
            {
                aLIStack.push(o3tl::make_unique<LineInfo>(aLineInfo));
                rMtf.AddAction( new MetaPushAction( PushFlags::ALL ) );

                // #106172# Track font relevant data in shadow VDev
                aFontVDev->Push();
            }
            break;

            case GDI_POP_ACTION:
            {

                std::unique_ptr<LineInfo> xLineInfo;
                if (!aLIStack.empty())
                {
                    xLineInfo = std::move(aLIStack.top());
                    aLIStack.pop();
                }

                // restore line info
                if (xLineInfo)
                {
                    aLineInfo = *xLineInfo;
                    xLineInfo.reset();
                    bFatLine = ( LineStyle::NONE != aLineInfo.GetStyle() ) && !aLineInfo.IsDefault();
                }

                rMtf.AddAction( new MetaPopAction() );

                // #106172# Track font relevant data in shadow VDev
                aFontVDev->Pop();
            }
            break;

            case GDI_GRADIENT_ACTION:
            {
                Color   aStartCol;
                Color   aEndCol;
                sal_Int16   nStyle;
                sal_Int16   nAngle;
                sal_Int16   nBorder;
                sal_Int16   nOfsX;
                sal_Int16   nOfsY;
                sal_Int16   nIntensityStart;
                sal_Int16   nIntensityEnd;

                ImplReadRect( rIStm, aRect );
                rIStm.ReadInt16( nStyle );
                ImplReadColor( rIStm, aStartCol );
                ImplReadColor( rIStm, aEndCol );
                rIStm.ReadInt16( nAngle ).ReadInt16( nBorder ).ReadInt16( nOfsX ).ReadInt16( nOfsY ).ReadInt16( nIntensityStart ).ReadInt16( nIntensityEnd );

                Gradient aGrad( (GradientStyle) nStyle, aStartCol, aEndCol );

                aGrad.SetAngle( nAngle );
                aGrad.SetBorder( nBorder );
                aGrad.SetOfsX( nOfsX );
                aGrad.SetOfsY( nOfsY );
                aGrad.SetStartIntensity( nIntensityStart );
                aGrad.SetEndIntensity( nIntensityEnd );
                rMtf.AddAction( new MetaGradientAction( aRect, aGrad ) );
            }
            break;

            case GDI_TRANSPARENT_COMMENT:
            {
                tools::PolyPolygon aPolyPoly;
                sal_Int32       nFollowingActionCount;
                sal_Int16       nTrans;

                ReadPolyPolygon( rIStm, aPolyPoly );
                rIStm.ReadInt16( nTrans ).ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaTransparentAction( aPolyPoly, nTrans ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_FLOATTRANSPARENT_COMMENT:
            {
                GDIMetaFile aMtf;
                Point       aPos;
                Size        aSize;
                Gradient    aGradient;
                sal_Int32       nFollowingActionCount;

                ReadGDIMetaFile( rIStm, aMtf );
                ReadPair( rIStm, aPos );
                ReadPair( rIStm, aSize );
                ReadGradient( rIStm, aGradient );
                rIStm.ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaFloatTransparentAction( aMtf, aPos, aSize, aGradient ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_HATCH_COMMENT:
            {
                tools::PolyPolygon aPolyPoly;
                Hatch       aHatch;
                sal_Int32       nFollowingActionCount;

                ReadPolyPolygon( rIStm, aPolyPoly );
                ReadHatch( rIStm, aHatch );
                rIStm.ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaHatchAction( aPolyPoly, aHatch ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_REFPOINT_COMMENT:
            {
                Point   aRefPoint;
                bool    bSet;
                sal_Int32   nFollowingActionCount;

                ReadPair( rIStm, aRefPoint );
                rIStm.ReadCharAsBool( bSet ).ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaRefPointAction( aRefPoint, bSet ) );

                i += nFollowingActionCount;

                // #106172# Track font relevant data in shadow VDev
                if( bSet )
                    aFontVDev->SetRefPoint( aRefPoint );
                else
                    aFontVDev->SetRefPoint();
            }
            break;

            case GDI_TEXTLINECOLOR_COMMENT:
            {
                Color   aColor;
                bool    bSet;
                sal_Int32   nFollowingActionCount;

                ReadColor( rIStm, aColor );
                rIStm.ReadCharAsBool( bSet ).ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaTextLineColorAction( aColor, bSet ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_TEXTLINE_COMMENT:
            {
                Point   aStartPt;
                sal_Int32  nWidth(0);
                sal_uInt32 nStrikeout(0);
                sal_uInt32 nUnderline(0);
                sal_Int32  nFollowingActionCount(0);

                ReadPair( rIStm, aStartPt );
                rIStm.ReadInt32(nWidth ).ReadUInt32(nStrikeout).ReadUInt32(nUnderline).ReadInt32(nFollowingActionCount);
                ImplSkipActions(rIStm, nFollowingActionCount);
                rMtf.AddAction( new MetaTextLineAction( aStartPt, nWidth,
                                                        (FontStrikeout) nStrikeout,
                                                        (FontLineStyle) nUnderline,
                                                        LINESTYLE_NONE ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_GRADIENTEX_COMMENT:
            {
                tools::PolyPolygon aPolyPoly;
                Gradient    aGradient;
                sal_Int32       nFollowingActionCount;

                ReadPolyPolygon( rIStm, aPolyPoly );
                ReadGradient( rIStm, aGradient );
                rIStm.ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction( new MetaGradientExAction( aPolyPoly, aGradient ) );

                i += nFollowingActionCount;
            }
            break;

            case GDI_COMMENT_COMMENT:
            {
                sal_Int32   nValue;
                sal_uInt32  nDataSize;
                std::vector<sal_uInt8> aData;
                sal_Int32       nFollowingActionCount;

                OString aComment = read_uInt16_lenPrefixed_uInt8s_ToOString(rIStm);
                rIStm.ReadInt32( nValue ).ReadUInt32( nDataSize );

                if (nDataSize)
                {
                    aData.resize(nDataSize);
                    nDataSize = rIStm.ReadBytes(aData.data(), nDataSize);
                }

                rIStm.ReadInt32( nFollowingActionCount );
                ImplSkipActions( rIStm, nFollowingActionCount );
                rMtf.AddAction(new MetaCommentAction(aComment, nValue, aData.data(), nDataSize));

                i += nFollowingActionCount;
            }
            break;

            case GDI_UNICODE_COMMENT:
            {
                nUnicodeCommentActionNumber = i + 1;
                nUnicodeCommentStreamPos = rIStm.Tell() - 6;
                if (nActionSize < 4)
                    rIStm.SetError(SVSTREAM_FILEFORMAT_ERROR);
                else
                    rIStm.SeekRel(nActionSize - 4);
            }
            break;

            default:
                if (nActionSize < 4)
                    rIStm.SetError(SVSTREAM_FILEFORMAT_ERROR);
                else
                    rIStm.SeekRel(nActionSize - 4);
            break;
        }
    }

    rIStm.SetEndian( nOldFormat );
}
