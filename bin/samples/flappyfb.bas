''
'' This game is released under GPL v3 license
'' https://www.gnu.org/licenses/gpl-3.0.en.html
'' 
'' Copyright (C) 2015 Grégori Macário Harbs 
'' https://freebasic.net/forum/viewtopic.php?f=8&t=23234#p204450
''
'' Donated by author to fbide project for IDE development and testing.
''
#define fbc -s gui

#include "crt.bi"
#include "fbgfx.bi"

const PI = atn(1)/45,PI128 = (atn(1)*4)/128
const ftBig = 0,ftSmall = 256,ftCenter = 512

enum ColorConstants
  cTrans   =  0
  cReady  : cReady2 : cOver   : cOver2   : cWall    : cWall2  : cWall3  : cWall4
  cWall5  : cWall6  : cWall7  : cWall8   : cFrame1  : cFrame2 : cFrame3 : cText
  cText2  : cBorder : cBlack  : cGray    : cWhite   : cRed
  cMedal0 : cMedal1 : cMedal2 : cMedal3
  cBirdA  : cBirdA1 : cBirdA2 : cBirdA3  : cBirdA4  : cBirdA5
  cBirdB  : cBirdB1 : cBirdB2 : cBirdB3  : cBirdB4  : cBirdB5 : cBirdB6
  cPipeA  : cPipeZ = cPipeA+47 ' ------------------------------------------------
  cSky    : cStars  : cClouds : cShadow1 : cShadow2 : cBuild1 : cBuild2 : cBuild3
  cLight1 : cLight2 : cGrass1 : cGrass2 
  CLast   : cOpaque  = 255
end enum
enum ActionConstants
  _NEXT   = -1  'advance to next Char (special meaning) 
  _ROUND  = -1  'Draw a filled Circle X,Y,RADIUS,COLOR   
  _ROUNDX = -2  'Draw a filled Circle X,Y,RADIUS,COLOR   
  _LINE   = -3  'Draws a horz filed line X,Y,XX,YY,COLOR
  _BOX    = -4  'Draw a filled box X,Y,XX,YY (exclusive)
  _ARC    = -5  'Draws an outlined partial circle X,Y,RADIUS,COLOR,START,END,RATIO?
  _AREA   = -6  'offsets to a clipping area X,Y,WID,HEI 
  _NOAREA = -7  'remove previous set clipping           
  _OFFSET = -8  'offsets to a point X,Y (or ID,Skips)   
  _BORDER = -9  'Borderize area with COLOR               
  _SHADOW = -10 'Cast Shadow on text COLOR,BACKGROUND   
  _LINE2  = -11 'Draws a horz filed line X,Y,XX,YY,COLOR
  _PAINT  = -12 'Paint area at X,Y,COLOR,BORDER         
  _FINISH = -7  'Done Drawing... same as _NOAREA_ last! 
end enum
enum Graphics 
  #define _gClouds_    pGfx,(v(  0),v( 28))-(v( 64)-1,v( 46)-1)
  #define _gGetReady_  pGfx,(v(  0),v( 64))-(v( 92)-1,v( 88)-1)
  #define _gGameOver_  pGfx,(v(  0),v( 88))-(v( 96)-1,v(109)-1)
  #define _gShadows_   pGfx,(v( 64),v( 28))-(v( 96)-1,v( 46)-1)
  #define _gBuildings_ pGfx,(v( 96),v( 28))-(v(128)-1,v( 46)-1)
  #define _gBushes_    pGfx,(v(  0),v( 46))-(v( 64)-1,v( 64)-1)
  #define _gWall_      pGfx,(v(116),v( 46))-(v(128)-1,v( 58)-1)
  #define _gPipeDown_  pGfx,(v( 90),v( 46))-(v(116)-1,v( 58)-1)
  #define _gPipeUp_    pGfx,(v( 64),v( 46))-(v( 90)-1,v( 58)-1)
  #define _gPipeLine_  pGfx,(v( 80),v( 58))-(v(104)-1,v( 59)-1)
  #define _gPipeBody_  pGfx,(v(104),v( 58))-(v(128)-1,v( 70)-1)
  #define _gFrameLT_   pGfx,(v(104),v( 70))-(v(112)-1,v( 78)-1)
  #define _gFrameU_    pGfx,(v(112),v( 70))-(v(120)-1,v( 78)-1)
  #define _gFrameRT_   pGfx,(v(120),v( 70))-(v(128)-1,v( 78)-1)
  #define _gFrameL_    pGfx,(v(104),v( 78))-(v(112)-1,v( 86)-1)
  #define _gFrameR_    pGfx,(v(120),v( 78))-(v(128)-1,v( 86)-1)
  #define _gFrameLD_   pGfx,(v(104),v( 86))-(v(112)-1,v( 94)-1)
  #define _gFrameD_    pGfx,(v(112),v( 86))-(v(120)-1,v( 94)-1)
  #define _gFrameRD_   pGfx,(v(120),v( 86))-(v(128)-1,v( 94)-1)
  #define _gMedal_     pGfx,(v( 70),v( 18))-(v( 93)-1,v( 24)-1)
  #define _gScore_     pGfx,(v( 94),v( 18))-(v(116)-1,v( 24)-1)
  #define _gNew_       pGfx,(v(  0),v(109))-(v( 16)-1,v(116)-1)
  #define _gBest_      pGfx,(v(  0),v(116))-(v( 17)-1,v(122)-1)
  #define _gNoMedal_   pGfx,(v( 96),v( 94))-(v(118)-1,v(116)-1)
  #define _gTapLeft_   pGfx,(v( 21),v(109))-(v( 42)-1,v(118)-1)
  #define _gTapRight_  pGfx,(v( 17),v(109))-(v( 38)-1,v(118)-1)
  #define _gKeys_      pGfx,(v( 42),v(109))-(v( 61)-1,v(128)-1)
  #define _gSign_      pGfx,(v(  0),v(122))-(v(  9)-1,v(126)-1)
  #define _gArrowUp_   pGfx,(v( 17),v(118))-(v( 25)-1,v(125)-1)
  gDummy
end enum

#define w(_NN_) (((_NN_)+iOffsetX)*iScale)
#define h(_NN_) (((_NN_)+iOffsetY)*iScale)
#define v(_NN_) (((_NN_))*iScale)

dim shared as integer iScale=0,iDay=0,iDebug=0,iTime,iFocus=1,iNewScale
dim shared as fb.image ptr pGfx,pDither,pBirdA,pBirdB,pCollision
dim shared as integer ScrW,ScrH,iHZ,iBest
dim shared as integer AA=1,iFull=0,iDither=1,iSync=0,iShowCrash=0
dim shared as integer iOffsetX,iOffsetY,iWid,iHei
dim shared as integer iBirdX,iBirdY

function SetPalette(iDay as integer=-1,iMinuteOfDay as integer=0,iBird as integer=0) as integer
  function = 0
  ' Palette Constant
  palette cTrans  ,255,  0,  0 : palette cBorder , 88, 56, 72 
  palette cBlack  ,  8,  8,  8 : palette cWhite  ,248,248,248
  palette cReady  , 80,208, 80 : palette cReady2 ,  0,152, 72
  palette cOver   ,248,152, 88 : palette cOver2  ,224,104, 32
  palette cWall   ,224,248,136 : palette cWall2  ,152,224, 96
  palette cWall3  ,128,192, 56 : palette cWall4  ,120,184, 48
  palette cWall5  ,136,216, 80 : palette cWall6  , 88,128, 40
  palette cWall7  ,208,168, 80 : palette cWall8  ,216,216,144
  palette cFrame1 ,240,232,160 : palette cFrame2 ,216,208,152
  palette cFrame3 ,192,192,136 : palette cRed    ,240, 40,  8
  palette cText   ,248,128, 96 : palette cText2  ,240,232,160
  palette cGray   ,200,200,200
 
  select case (iBird and 255)
  case 0 'Blue
    palette cBirdA  , 64,152,192 : palette cBirdA1 , 80,184,232
    palette cBirdA2 , 88,200,240 : palette cBirdA3 ,216, 96, 24
    palette cBirdA4 ,200,216,192 : palette cBirdA5 ,240,240,240
  case 1 'Orange
    palette cBirdA  ,208,120, 48 : palette cBirdA1 ,240,168, 40
    palette cBirdA2 ,240,200,128 : palette cBirdA3 ,240, 56, 24
    palette cBirdA4 ,206,222,198 : palette cBirdA5 ,240,240,240 
  case 2 'Red
    palette cBirdA  ,206, 49, 16 : palette cBirdA1 ,247, 57, 24
    palette cBirdA2 ,247,115, 33 : palette cBirdA3 ,247,173, 71
    palette cBirdA4 ,206,222,198 : palette cBirdA5 ,240,240,240 
  end select 
  select case (iBird shr 8)
  case 0 'No Medal
    palette cBirdB  ,200,200,200 : palette cBirdB1 ,200,200,200
    palette cBirdB2 ,240,240,240 : palette cBirdB3 ,200,200,200
    palette cBirdB4 ,200,200,200 : palette cBirdB5 ,240,240,240
    palette cBirdB6 , 88, 56, 72
    palette cMedal0 ,208,200,144 : palette cMedal1 ,208,200,144
    palette cMedal2 ,208,200,144 : palette cMedal3 ,208,200,144
  case 1 'Bronze Medal
    palette cBirdB  ,232,160, 72 : palette cBirdB1 ,240,176, 72
    palette cBirdB2 ,248,216,112 : palette cBirdB3 ,240,176, 72
    palette cBirdB4 ,232,160, 72 : palette cBirdB5 ,240,176, 72   
    palette cBirdB6 ,216,144, 72
    palette cMedal0 ,240,176, 72 : palette cMedal1 ,176,128, 56
    palette cMedal2 ,232,160, 72 : palette cMedal3 ,248,216,112
  case 2 'Silver Medal
    palette cBirdB  ,200,192,192 : palette cBirdB1 ,232,224,224
    palette cBirdB2 ,248,248,248 : palette cBirdB3 ,232,224,224
    palette cBirdB4 ,200,192,192 : palette cBirdB5 ,232,224,224
    palette cBirdB6 ,152,152,152
    palette cMedal0 ,232,224,224 : palette cMedal1 ,128,128,128
    palette cMedal2 ,200,192,192 : palette cMedal3 ,248,248,248
  case 3 'Gold Medal
    palette cBirdB  ,248,216,112 : palette cBirdB1 ,232,240,112
    palette cBirdB2 ,248,248,248 : palette cBirdB3 ,232,240,112
    palette cBirdB4 ,248,216,112 : palette cBirdB5 ,232,240,112
    palette cBirdB6 ,224,192, 96
    palette cMedal0 ,232,240,112 : palette cMedal1 ,184,160, 80
    palette cMedal2 ,248,216,112 : palette cMedal3 ,248,248,248
  case 4 'Platinum Medal
    palette cBirdB  ,232,232,232 : palette cBirdB1 ,248,248,248
    palette cBirdB2 ,248,248,248 : palette cBirdB3 ,248,248,248
    palette cBirdB4 ,232,232,232 : palette cBirdB5 ,248,248,248
    palette cBirdB6 ,208,200,200
    palette cMedal0 ,248,248,248 : palette cMedal1 ,128,128,128
    palette cMedal2 ,232,232,232 : palette cMedal3 ,248,248,248
  end select
 
  for CNT as integer = 0 to 47
    palette cPipeA+CNT,88+((224-88)/47)*CNT,128+((248-128)/47)*CNT,48+((144-48)/47)*CNT
  next CNT
  palette 255,32,64,128
 
  if iDay=-1 then
    dim as integer IR,IG,IB,IM=abs((iMinuteOfDay mod 1440)-720)
    iTime= IM
    static as integer iDo
    if iM<10 then
      iDo = 1
    elseif iM<20 then
      if iDo then iDo=0: function = 1       
    end if   
    #macro SetPal(_ATT_,_DR,_DG,_DB,_NR,_NG,_NB)
      IR = _DR+(((_NR-_DR)*IM)\720)
      IG = _DG+(((_NG-_DG)*IM)\720)
      IB = _DB+(((_NB-_DB)*IM)\720)
      palette _ATT_,IR,IG,IB
    #endmacro   
    SetPal( cSky     , 90,220,230 ,   6, 96,108 )
    var RR=0,GG=0,BB=0,R2=0,G2=0,B2=0
    palette get cSky,RR,GG,BB
    SetPal( cStars   , RR,GG,BB , 168,224,208 )   
    if IM < 256 then
      palette cStars,RR,GG,BB
    else
      palette get cStars,R2,G2,B2
      SetPal( cStars   , RR,GG,BB , R2,G2,B2 )
    end if
    SetPal( cClouds  ,224,240,208 ,   6,132,144 )
    SetPal( cShadow1 ,216,232,200 ,   6,108,120 )
    SetPal( cShadow2 ,200,224,184 ,   6,108,110 )
    SetPal( cBuild1  ,200,224,184 ,   6, 96,108 )
    SetPal( cBuild2  ,168,216,184 ,   6, 96,108 )
    SetPal( cBuild3  ,144,208,200 ,   6, 96,108 )
    if IM > 330 then     
      palette cLight1 ,248,184,  8
    else
      SetPal( cLight1  ,168,216,184 ,   4, 64, 72 )     
    end if
    if IM > 270 then     
      palette cLight2 ,248,184,  8
    else
      SetPal( cLight2  ,168,216,184 ,   4, 64, 72 )     
    end if
    SetPal( cGrass1  , 96,192,120 ,   6,120,  6 )
    SetPal( cGrass2  , 88,216,112 ,   6,132,  6 )   
  else   
    if iDay then
      ' Palette Day
      palette cSky    , 72,176,184 : palette cStars  , 72,176,184 : palette cClouds ,224,240,208
      palette cShadow1,216,232,200 : palette cShadow2,208,232,192 : palette cBuild1 ,200,224,184
      palette cBuild2 ,168,216,184 : palette cBuild3 ,144,208,200 : palette cLight1 ,168,216,184
      palette cLight2 ,168,216,184 : palette cGrass1 , 96,192,120 : palette cGrass2 , 88,216,112
    else 
      ' Palette Night
      palette cSky    ,  8,128,144 : palette cStars  ,168,224,208 : palette cClouds ,  8,176,192
      palette cShadow1,  8,144,160 : palette cShadow2,  8,144,152 : palette cBuild1 ,  8,128,144
      palette cBuild2 ,  8,128,144 : palette cBuild3 ,  8,128,144 : palette cLight1 ,248,184,  8
      palette cLight2 ,248,184, 68 : palette cGrass1 ,  8,160,  8 : palette cGrass2 ,  8,176,  8
    end if
  end if
 
  if iDither then   
    dim as integer R,G,B,R2,G2,B2,iV=iHei\40
    if iV < 8 then iV=8 else if iV > 32 then iV = 32
    for CNT as integer = cTrans+1 to cLast-1   
      palette get CNT,R,G,B
      R2=R+iV:G2=G+iV:B2=B+iV
      R-=iV:G-=iV:B-=iV     
      if R2>255 then R2=255
      if G2>255 then G2=255
      if B2>255 then B2=255     
      if R<0 then R=0
      if G<0 then G=0
      if B<0 then B=0
      palette CNT,R,G,B
      palette CNT or 128,R2,G2,B2
    next CNT
  end if 
 
end function
sub DrawArc(pBuff as fb.image ptr,x0 as integer,y0 as integer,radius as integer,iColor as integer,fStartAng as single,fEndAng as single,iSz as integer=0) 
    const sTemp = csng(16384/(atn(1)*8))
    const iTabSize = 512 '~(Radius/1.4) usage
    static as short iAtnTab(iTabSize),iInitTable 
    if iInitTable=0 then
      iInitTable = 1
      for CNT as integer = 0 to iTabSize
        iAtnTab(CNT) = atn(CNT/.iTabSize)*sTemp
      next CNT
    end if
    dim as integer x=radius,y=0,radiusError=1-x
    dim as integer iStartAng = (fStartAng*sTemp) and 16383
    dim as integer iEndAng   = (fEndAng*sTemp) and 16383   
    var iW = pBuff->Width, iH = pBuff->Height,iPitch = pBuff->Pitch 
    var pBuffStart = cast(ubyte ptr,pBuff+1),iBuffSz=(iPitch*iH) 
    var pMidPix = pBuffStart+(y0*iPitch)+(x0)
    if iStartAng > iEndAng then iEndAng += 16384   
    #macro SetOctant(_N,_ANGS,_ANGE,_ALGO)
    if (iStartAng >= (_ANGS) and iEndAng <= (_ANGE)) orelse (iEndAng>= (_ANGS) and iStartAng <= (_ANGE)) then p##_N = _ALGO
    if (iStartAng >= (_ANGS+16384) and iEndAng <= (_ANGE+16384)) orelse (iEndAng>= (_ANGS+16384) and iStartAng <= (_ANGE+16384)) then p##_N = _ALGO 
    #endmacro
    dim as ubyte ptr p0,p1,p2,p3,p4,p5,p6,p7
    SetOctant(0,14336,16383,pMidPix+(y*iPitch)+x) 'x0 + x , y0 + y
    SetOctant(1,12288,14335,pMidPix+(x*iPitch)+y) 'x0 + y , y0 + x
    SetOctant(2,10240,12887,pMidPix+(x*iPitch)-y) 'x0 - y , y0 + x
    SetOctant(3, 8192,10239,pMidPix+(y*iPitch)-x) 'x0 - x , y0 + y
    SetOctant(4, 6144, 8191,pMidPix-(y*iPitch)-x) 'x0 - x , y0 - y
    SetOctant(5, 4096, 6143,pMidPix-(x*iPitch)-y) 'x0 - y , y0 - x
    SetOctant(6, 2048, 4095,pMidPix-(x*iPitch)+y) 'x0 + y , y0 - x
    SetOctant(7,    0, 2047,pMidPix-(y*iPitch)+x) 'x0 + x , y0 - y
    #macro DoDrawArc()
      while x >= y   
        var i7 = iAtnTab((y*iTabSize)\x)   
        var i6=4095-i7,i5=i7+4096,i4=8192-i7,i3=i7+8192
        var i2=12288-i7,i1=i7+12288,i0=16383-i7
        if cuint(x0+x) < iW then : DrawPIxel(0) : DrawPIxel(7) : end if   
        if cuint(x0+y) < iW then : DrawPIxel(1) : DrawPIxel(6) : end if   
        if cuint(x0-x) < iW then : DrawPIxel(3) : DrawPIxel(4) : end if
        if cuint(x0-y) < iW then : DrawPIxel(2) : DrawPIxel(5) : end if
        y += 1: p0 += iPitch: p1 += 1: p2 -= 1: p3 += iPitch:
        p4 -= iPitch: p5 -= 1: p6 += 1: p7 -= iPitch
        if (radiusError<0) then   
          radiusError += 2 * y + 1     
        else
          x -= 1: p0 -= 1: p1 -= iPitch: p2 -= iPitch: p3 += 1
          p4 += 1: p5 += iPitch: p6 += iPitch: p7 -= 1
          radiusError += 2 * (y - x + 1)     
        end if   
      wend 
    #endmacro
  if iSz=0 then
    #macro DrawPixel(N)
      if cuint(p##n-pBuffStart)<iBuffSz then
        if i##N >= iStartAng and i##N <= iEndAng then *p##N = iColor
        if i##N >= (iStartAng-16384) and i##N <= (iEndAng-16384) then *p##N = iColor
      end if
    #endmacro
    DoDrawArc()
  else
    var iColor2 = iColor or (iColor shl 8)
    #macro DrawPixel(N)
      if cuint(p##n-pBuffStart)<iBuffSz then
        if i##N >= iStartAng and i##N <= iEndAng then *cptr(ushort ptr,p##N) = iColor2
        if i##N >= (iStartAng-16384) and i##N <= (iEndAng-16384) then *cptr(ushort ptr,p##N) = iColor2
      end if
    #endmacro
    DoDrawArc()
  end if
end sub
sub CreateGraphics()
  var iOffsetX=0,iOffsetY=0
  if pGfx then ImageDestroy(pGfx):pGfx=0
  pGfx = ImageCreate(v(128),v(128))
  const cBk=cBlack,cWt=cWhite,cTr=cTrans 
  dim as integer iScaleB=(iScale+1)\2,iScaleC=iScaleB-1,iScaleD=(iScale\2)-1
  dim as short iN1(...) = { _ ' >>> Level Numbers <<<
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,5,5,6,12,cBlack,                  _NEXT, _  ' ** 0 **
    2,1, 8,18,cBlack,1,1, 7,17,cBlack,0,0, 7, 6,cBlack,1,1,6, 5,cWhite,2, 2, 6,16,cWhite,_NEXT, _  ' ** 1 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,0,5,6, 6,cBlack,5,11,10,12,cBlack,_NEXT, _  ' ** 2 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,0,5,6, 6,cBlack,0,11, 6,12,cBlack,_NEXT, _  ' ** 3 **
    6,1,12,18,cBk,0,0,11,14,cBk,1,1,10,16,cWt,1,13,6,17,cBk,1,15,5,17,cTr,5,1,6,9,cBk,   _NEXT, _  ' ** 4 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,5,5,10,6,cBlack,0,11, 6,12,cBlack,_NEXT, _  ' ** 5 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,5,5,10,6,cBlack,5,11, 6,12,cBlack,_NEXT, _  ' ** 6 **
    6,1,12,18,cBk,0,0,11,10,cBk,1,1,10,16,cWt,1, 9,6,17,cBk,1,11,5,17,cTr,5,5,6,9,cBk,   _NEXT, _  ' ** 7 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,5,5,6, 6,cBlack,5,11, 6,12,cBlack,_NEXT, _  ' ** 8 **
    1,1,12,18,cBlack,0,0,11,17,cBlack,1,1,10,16,cWhite,5,5,6, 6,cBlack,0,11, 6,12,cBlack    }      ' ** 9 **
  dim as short iN2(...) = { _ ' >>> Progress Status Numbers <<<
    0,0,7,10,cBlack,1,1,6, 9,cWhite,3,3,4,7,cBlack,                _NEXT, _  ' ** 0 **
    0,0,5, 4,cBlack,1,4,5,10,cBlack,1,1,4,3,cWhite,2,3,4,9,cWhite, _NEXT, _  ' ** 1 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,1,3,4,4,cBlack,3,6,6,7,cBlack, _NEXT, _  ' ** 2 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,1,3,4,4,cBlack,1,6,4,7,cBlack, _NEXT, _  ' ** 3 **
    0,0,7, 7,cBk,3,7,7,10,cBk,1,1,6,6,cWt,4,6,6,9,cWt,3,1,4,4,cBk, _NEXT, _  ' ** 4 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,3,3,6,4,cBlack,1,6,4,7,cBlack, _NEXT, _  ' ** 5 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,3,3,6,4,cBlack,3,6,4,7,cBlack, _NEXT, _  ' ** 6 **
    0,0,7, 4,cBlack,3,3,7,10,cBlack,1,1,6,3,cWhite,4,3,6,9,cWhite, _NEXT, _  ' ** 7 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,3,3,4,4,cBlack,3,6,4,7,cBlack, _NEXT, _  ' ** 8 **
    0,0,7,10,cBlack,1,1,6, 9,cWhite,3,3,4,4,cBlack,1,6,4,7,cBlack    }       ' ** 9 **
  dim as short iGX(...) = { _ ' >>> Other Objects <<<
    _AREA,0,28,64,18,_ROUND,10,13,11,cClouds,_ROUND,26,13,10,cClouds,           _ ' *** Cloud *****
      _ROUND,40,13,13,cClouds,_ROUND,58,15, 8,cClouds, _
    _NOAREA,_OFFSET,64,28,_BOX,0,16,32,18,cShadow1,_ROUND,4,5,4,cShadow1,       _ ' *** Shadows ***
      _BOX,4,1,9,16,cShadow1,_BOX,0,5,4,16,cShadow1,_BOX,1,10,3,17,cShadow2,      _ 
      _BOX,3,5,8,17,cShadow2,_ROUND,5,6,3,cShadow2,_BOX,10,10,14,16,cShadow1,     _ 
      _BOX,11,11,13,18,cShadow2,_ROUND,15,15,2,cShadow1,_BOX,15,0,17,16,cShadow1, _ 
      _BOX,17,1,19,16,cShadow1,_ROUNDX,19,4,3,cShadow1,-1,0,_BOX,19,4,22,16,cShadow1,_ 
      _ROUND,18,5,2,cShadow2,_BOX,16,4,20,7,cShadow2,_BOX,18,8,22,11,cShadow2,    _
      _BOX,16,12,20,16,cShadow2,_BOX,23,6,29,12,cShadow1,_BOX,26,3,29,6,cShadow1, _
      _BOX,23,12,32,16,cShadow1,_BOX,24,8,27,12,cShadow2,_BOX,27,4,29,7,cShadow2, _
      _BOX,26,13,31,17,cShadow2, _
    _OFFSET,96,28,_BOX,0,9,27,18,cBuild1,_BOX,3,4,10,9,cBuild1,                 _ ' ** Buildings **
      _BOX, 0, 7, 3, 9,cBuild1,_BOX,11, 9,27, 6,cBuild1,_BOX,11, 4,23, 6,cBuild1, _
      _BOX,11, 2,20, 4,cBuild1,_BOX,15, 0,20, 2,cBuild1,_BOX, 7, 8, 8,11,cBuild2, _
      _BOX, 5,15, 8,16,cBuild2,_BOX, 8,10, 9,15,cBuild2,_BOX,10,10,11,15,cBuild2, _
      _BOX,13,15,14,16,cBuild2,_BOX,15,15,16,16,cBuild2,_BOX,17,15,18,16,cBuild2, _
      _BOX,20, 5,21,18,cBuild2,_BOX,22,15,23,17,cBuild2,_BOX,24,16,25,17,cBuild2, _
      _BOX, 3, 4,10, 5,cBuild3,_BOX, 3, 5, 4, 7,cBuild3,_BOX, 9, 5,10,11,cBuild3, _
      _BOX, 0, 7, 9, 8,cBuild3,_BOX, 0, 8, 1,18,cBuild3,_BOX, 6,11,11,12,cBuild3, _
      _BOX, 6,12, 7,18,cBuild3,_BOX,11, 3,12,18,cBuild3,_BOX,11, 2,16, 3,cBuild3, _
      _BOX,15, 1,16, 2,cBuild3,_BOX,15, 0,20, 1,cBuild3,_BOX,19, 1,20,18,cBuild3, _
      _BOX,20, 4,23, 5,cBuild3,_BOX,22, 5,23, 6,cBuild3,_BOX,20, 6,27, 7,cBuild3, _
      _BOX,26, 7,27,18,cBuild3,_BOX, 1, 9, 2,14,cLight1,_BOX, 3, 9, 4,14,cLight1, _
      _BOX, 5, 9, 6,14,cLight1,_BOX, 1,11, 6,12,cBuild1,_BOX, 5, 6, 6, 7,cLight2, _
      _BOX, 8, 5, 9, 7,cLight2,_BOX, 8, 8, 9,10,cLight2,_BOX, 8,11, 9,13,cLight2, _
      _BOX,17,12,18,13,cLight2,_BOX,24,15,25,16,cLight2,_BOX,13, 4,14,15,cLight1, _
      _BOX,15, 4,16,15,cLight1,_BOX,17, 4,18,15,cLight1,_BOX,13, 6,18, 7,cBuild1, _
      _BOX,13, 9,18,11,cBuild1,_BOX,13,13,18,14,cBuild1,_BOX,22, 9,23,14,cLight1, _
      _BOX,24, 9,25,14,cLight1,_BOX,22,11,25,12,cBuild1, _
    _AREA,0,46,64,18,_ROUND,19,7,7,cGrass1,_ROUND,19,7,6,cGrass2,               _ ' *** Bushes ****
      _ROUND,12, 7,7,cGrass1,_ROUND,12, 7,6,cGrass2,_ROUND,38, 7,7,cGrass1,_ROUND,38, 7,6,cGrass2, _
      _ROUND,60, 7,7,cGrass1,_ROUND,60, 7,6,cGrass2,_ROUND,53, 7,7,cGrass1,_ROUND,53, 7,6,cGrass2, _
      _ROUND, 0, 8,7,cGrass1,_ROUND, 0, 8,6,cGrass2,_ROUND,27, 9,7,cGrass1,_ROUND,27, 9,6,cGrass2, _
      _ROUND,47, 9,7,cGrass1,_ROUND,47, 9,6,cGrass2,_ROUND,63,10,7,cGrass1,_ROUND,63,10,6,cGrass2, _
      _ROUND, 0,10,7,cGrass1,_ROUND, 0,10,6,cGrass2,_ROUND, 8, 9,7,cGrass1,_ROUND, 8, 9,6,cGrass2, _
      _ROUND,20,12,7,cGrass1,_ROUND,20,12,6,cGrass2,_ROUND,38,12,7,cGrass1,_ROUND,38,12,6,cGrass2, _
      _ROUND,58,12,7,cGrass1,_ROUND,58,12,6,cGrass2,_BOX,0,9,64,18,cGrass2, _
    _AREA,0,64,96,24,_ROUND,4,4,2,cOpaque,_ROUNDX,4,16,2,cOpaque,0,-1,        _ ' ** [G]et Ready! **
      _BOX,6,5,11,15,cTrans,_BOX,4,2,11,5,cOpaque,_BOX,4,15,11,18,cOpaque,      _
      _BOX,2,4,6,16,cOpaque,_BOX,7,8,11,15,cOpaque,                             _
    _ROUND,16,8,4,cOpaque,_ROUNDX,17,8,4,cOpaque,-1,0,_BOX,12,8,17,16,cOpaque,_ ' ** G[e]t Ready! **
      _ROUNDX,14,16,2,cOpaque,0,-1,_BOX,14,16,17,18,cOpaque,                    _
      _BOX,16,8,17,12,cTrans,_BOX,17,8,21,14,cOpaque,                           _
    _BOX,24,2,28,16,cOpaque,_BOX,22,7,30,10,cOpaque,                          _ ' ** Ge[t] Ready! **
      _BOX,26,15,30,18,cOpaque,_ROUNDX,26,16,2,cOpaque,0,-1,                    _
    _BOX,36,2,40,18,cOpaque,_BOX,41,4,45,18,cOpaque,_BOX,40,2,43,16,cOpaque,  _ ' ** Get [R]eady! **
      _ROUNDX,43,4,2,cOpaque,-1,0,_BOX,40,5,41,10,cTr,_ROUNDX,47,14,3,cTr,-1,0, _
    _ROUND,50,8,4,cOpaque,_ROUNDX,51,8,4,cOpaque,-1,0,                        _ ' ** Get R[e]ady! **
      _BOX,46,8,51,16,cOpaque,_ROUNDX,48,16,2,cOpaque,0,-1,                     _   
      _BOX,48,16,51,18,cOpaque,_BOX,50,8,51,12,cTrans,_BOX,51,8,55,14,cOpaque,  _   
    _ROUND,60,10,4,cOpaque,_BOX,60,6,65,18,cOpaque,_BOX,58,14,65,18,cOpaque,  _ ' ** Get Re[a]dy! **
      _ROUNDX,58,16,2,cOpaque,0,-1,_BOX,56,9,59,16,cOpaque,_BOX,60,10,61,15,cTr,_
    _ROUND,68,9,2,cOpaque,_ROUNDX,68,16,2,cOpaque,0,-1,                       _ ' ** Get Rea[d]y! **
      _BOX,68,7,75,18,cOpaque,_BOX,66,9,70,16,cOpaque,_BOX,71,2,75,7,           _
      cOpaque,_BOX,71,2,75,7,cOpaque, _BOX,70,10,71,15,cTrans,                  _
    _ROUNDX,78,13,2,cOpaque,0,-1,_ROUNDX,83,19,2,cOpaque,-1,-1,               _ ' ** Get Read[y]! **
      _BOX,76,5,85,13,cOpaque,_BOX,78,12,85,15,cOpaque,                         _
      _BOX,81,15,85,19,cOpaque,_BOX,79,18,83,21,cOpaque,_BOX,80,5,81,12,cTrans, _
    _BOX,86,2,90,12,cOpaque,_BOX,86,15,90,18,cOpaque,_BORDER,cReady,          _ ' ** Get Ready[!] **
    _AREA,0,88,96,24,_ROUND,4,4,2,cOpaque,_ROUNDX,4,16,2,cOpaque,0,-1,        _ ' #### [G]ame Over ####
      _BOX,6,5,11,15,cTrans,_BOX,4,2,11,5,cOpaque,_BOX,4,15,11,18,cOpaque,      _
      _BOX,2,4,6,16,cOpaque,_BOX,7,8,11,15,cOpaque,                             _
    _ROUND,16,10,4,cOpaque,_BOX,16,6,21,18,cOpaque,_BOX,14,14,21,18,cOpaque,  _ ' #### G[a]me Over ####
      _ROUNDX,14,16,2,cOpaque,0,-1,_BOX,12,9,15,16,cOpaque,_BOX,16,10,17,15,cTr,_
    _ROUNDX,33,10,4,cOpaque,-1,0,_BOX,22,6,34,18,cOpaque,                     _ ' #### Ga[m]e Over ####
      _BOX,34,9,37,18,cOpaque,_BOX,26,10,27,18,cTrans,_BOX,32,10,33,18,cTrans,  _
    _ROUND,42,8,4,cOpaque,_ROUNDX,43,8,4,cOpaque,-1,0,                        _ ' #### Gam[e] Over ####
      _BOX,38,8,43,16,cOpaque,_ROUNDX,40,16,2,cOpaque,0,-1,                     _   
      _BOX,40,16,43,18,cOpaque,_BOX,42,8,43,12,cTrans,_BOX,43,8,47,14,cOpaque,  _
    _ROUND,58,4,2,cOpaque,_ROUNDX,63,16,2,cOpaque,-1,-1,                      _ ' #### Game [O]ver ####
      _ROUNDX,58,16,2,cOpaque,0,-1,_ROUNDX,63,4,2,cOpaque,-1,0,                 _
      _BOX,58,2,63,18,cOpaque,_BOX,56,4,65,16,cOpaque,_BOX,60,5,61,15,cTrans,   _
    _BOX,66,6,75,18,cOpaque,_ROUND,88,31,22,cTrans,_BOX,70,6,71,15,cTrans,    _ ' #### Game O[v]er ####
    _ROUND,80,8,4,cOpaque,_ROUNDX,81,8,4,cOpaque,-1,0,                        _ ' #### Game Ov[e]r ####
      _BOX,76,8,81,16,cOpaque,_ROUNDX,78,16,2,cOpaque,0,-1,                     _   
      _BOX,78,16,81,18,cOpaque,_BOX,80,8,81,12,cTrans,_BOX,81,8,85,14,cOpaque,  _   
    _ROUND,90,10,4,cOpaque,_BOX,90,9,95,18,cTrans,                            _ ' #### Game Ove[r] ####
      _BOX,89,6,94,10,cOpaque,_BOX,86,10,90,18,cOpaque,_BORDER,cOver,           _
    _OFFSET,64,46,_BOX,0,0,26,12,cBorder,_BOX,1,1,25,11,cWall6,               _ ' ** Pipe Down **
    _OFFSET,90,46,_BOX,0,0,26,12,cBorder,_BOX,1,1,25,11,cWall6,               _ ' **  Pipe Up  **
    _OFFSET,80,58,_BOX,0,0,24,1,cBorder,_BOX,1,0,23,1,cWall6,                 _ ' ** Pipe Line **
    _OFFSET,104,58,_BOX,0,0,24,12,cBorder,_BOX,1,0,23,12,cWall6,              _ ' ** Pipe Body **
    _OFFSET,116,46,_BOX,0,0,12,1,cBorder,_BOX,0,1,12,2,cWall,                 _ ' ** Wall Main **
      _BOX,0,2,12,9,cWall2,_BOX,0,9,12,10,cWall6,                               _
      _BOX,0,10,12,11,cWall7,_BOX,0,11,12,12,cWall8,                            _
    _AREA,104,70,24,24,_ROUND,7,7,7,cBorder,_ROUNDX,17,7,7,cBorder,-1,0,      _ ' ** Dialog Frame **
      _ROUNDX,7,17,7,cBorder,0,-1,_ROUNDX,17,17,7,cBorder,-1,-1,                _
      _ROUND,8,8,7,cFrame1,_ROUNDX,16,8,7,cFrame1,-1,0,_ROUNDX,8,16,7,cFrame3,0,-1, _
      _ROUNDX,16,16,7,cFrame3,-1,-1,_ROUNDX,9,9,8,cFrame2,1,0,_ROUNDX,15,9,8,cFrame2,-2,0, _
      _ROUNDX,9,15,8,cFrame2,0,-2,_ROUNDX,15,15,8,cFrame2,-2,-1,_BOX,7,0,17,24,cBorder, _
      _BOX,0,7,24,17,cBorder,_BOX,7,1,17,2,cFrame1,_BOX,7,22,17,2,cFrame1, _   
      _BOX,7,2,17,22,cFrame2,_BOX,1,7,23,17,cFrame2,_BOX,7,22,17,23,cFrame3, _
      _ROUNDX,9,9,8,cFrame2,0,1,_ROUNDX,12,9,8,cFrame2,0,1,_ROUNDX,15,9,8,cFrame2,-1,1, _
      _ROUNDX,9,15,8,cFrame2,0,-2,_ROUNDX,12,15,8,cFrame2,0,-2,_ROUNDX,15,15,8,cFrame2,-1,-2, _
    _AREA,70,18,24,6,_BOX,0,0,1,5,cOpaque,_LINE,0,0,2,3,cOpaque,                         _ '[M]EDAL
      _LINE,2,3,4,0,cOpaque,_BOX,4,0,5,5,cOpaque,                                          _
    _BOX,6,0,7,5,cOpaque,_BOX,7,0,9,1,cOpaque,_BOX,7,2,9,3,cOpaque,_BOX,7,4,9,5,cOpaque, _ 'M[E]DAL
    _ROUNDX,12,2,2,cOpaque,-1,0,_ROUNDX,12,3,2,cOpaque,-1,-1,_ROUNDX,12,2,1,cTrans,-1,0, _ 'ME[D]AL
      _ROUNDX,12,3,1,cTrans,-1,-1,_BOX,13,2,14,3,cOpaque,_BOX,9,0,12,5,cTrans,             _
      _BOX,10,0,11,5,cOpaque,_BOX,10,0,12,1,cOpaque,_BOX,10,4,12,5,cOpaque,                _   
    _LINE,16,0,15,2,cOpaque,_LINE,17,0,18,2,cOpaque,_BOX,15,2,16,5,cOpaque,              _ 'MED[A]L
      _BOX,16,0,18,1,cOpaque,_BOX,18,2,19,5,cOpaque,_BOX,15,3,19,4,cOpaque,              _
    _BOX,20,0,21,5,cOpaque,_BOX,20,4,23,5,cOpaque,_SHADOW,cText,cFrame2,                 _ 'MEDA[L]
    _AREA,94,18,22,6,_BOX,1,0,4,1,cOpaque,_LINE,0,1,1,0,cOpaque,                 _ '[S]CORE
      _LINE,0,1,3,4,cOpaque,_BOX,0,4,3,5,cOpaque,_LINE,3,4,2,5,cOpaque,            _   
    _ROUND,7,2,2,cOpaque,_ROUNDX,7,3,2,cOpaque,0,-1,_ROUND,7,2,1,cTrans,         _ 'S[C]ORE
      _ROUNDX,7,3,1,cTrans,0,-1,_BOX,7,0,10,5,cTrans,_BOX,7,0,8,1,cOpaque,         _
      _BOX,7,4,8,5,cOpaque,_ROUNDX,7,2,-1,cTrans,0,1,_ROUNDX,7,3,-1,cTrans,0,-2,   _
    _ROUND,11,2,2,cOpaque,_ROUNDX,11,3,2,cOpaque,0,-1,                           _ 'SC[O]RE
      _ROUND,11,2,1,cTrans,_ROUNDX,11,3,1,cTrans,0,-1,                             _
      _ROUNDX,11,2,-1,cTrans,0,1,_ROUNDX,11,3,-1,cTrans,0,-2,                      _
    _ROUND,16,2,2,cOpaque,_ROUND,16,2,1,cTrans,_BOX,14,5,18,6,cTrans,            _ 'SCO[R]E
      _BOX,14,0,16,5,cTrans,_BOX,14,0,15,5,cOpaque,_BOX,14,0,17,1,cOpaque,       _
      _BOX,14,3,17,4,cOpaque,_LINE,16,3,17,5,cOpaque,                            _
    _BOX,19,0,20,5,cOpaque,_BOX,20,0,22,1,cOpaque,                               _ 'SCOR[E]
      _BOX,20,2,22,3,cOpaque,_BOX,20,4,22,5,cOpaque,_SHADOW,cText,cFrame2,       _
    _OFFSET,0,109,_BOX,0,0,16,7,cRed,_BOX,1,1,2,6,cWhite,_BOX,4,1,5,6,cWhite,    _ 'NEW
      _LINE,1,1,4,5,cWhite,_BOX,6,1,9,6,cWhite,_BOX,7,2,9,3,cRed, _
      _BOX,7,4,9,5,cRed,_BOX,10,1,11,6,cWhite,_BOX,14,1,15,6,cWhite, _
      _LINE,12,2,10,6,cWhite,_LINE,12,2,14,6,cWhite, _
    _AREA,0,116,17,6,_ROUNDX,2,1,2,cOpaque,-1,0,_ROUNDX,2,1,1,cTrans,-1,0,  _ '[B]EST
      _ROUNDX,2,3,2,cOpaque,-1,0,_ROUNDX,2,3,1,cTrans,-1,0,                 _   
      _BOX,0,0,3,6,cTrans,_BOX,0,0,1,5,cOpaque, _BOX,0,0,3,1,cOpaque,       _
      _BOX,0,4,3,5,cOpaque,_BOX,0,2,3,3,cOpaque,                            _
    _BOX,5,0,8,5,cOpaque,_BOX,6,1,8,2,cTrans,_BOX,6,3,8,4,cTrans,           _ 'B[E]ST
    _BOX,10,0,13,1,cOpaque,_LINE,9,1,10,0,cOpaque,_LINE,9,1,12,4,cOpaque,   _ 'BE[S]T
      _BOX,9,4,12,5,cOpaque,_LINE,12,4,11,5,cOpaque,                        _
    _BOX,14,0,17,1,cOpaque,_BOX,15,1,16,5,cOpaque,_SHADOW,cText,cFrame2,    _ 'BES[T]
    _OFFSET,96,94,_BOX,0,0,22,22,cFrame2,_ROUND,11,11,10,cFrame1,           _ 'Medal
      _ROUND,11,10,10,cMedal1,_ROUND,11,10,9,cMedal0, _
      _ARC,11,10,9,cMedal3,16,111,_ARC,11,10,9,cMedal2,144,239,  _
    _AREA,17,109,25,9,_BOX,1,1,24,8,cRed,_BOX,0,4,25,5,cRed,             _ ' Tap's Frame
      _BOX,4,0,21,9,cWhite,_BOX,5,1,20,8,cRed,_ROUNDX,0,0,3,cTrans,0,-1, _
      _ROUNDX,0,9,3,cTrans,0,-1,_LINE,-1,5,4,0,cWhite,_LINE,-1,4,4,9,cWhite,   _
      _ROUNDX,25,0,3,cTrans,-1,-1,_ROUNDX,25,9,3,cTrans,-1,-1,           _
      _LINE,25,5,20,0,cWhite,_LINE,25,4,20,9,cWhite,                     _
    _BOX,6,2,9,3,cWhite, _BOX,7,3,8,7,cWhite,                            _ '[T]AP
    _LINE,11,2,10,4,cWhite,_LINE,12,2,13,4,cWhite,_BOX,10,4,11,7,cWhite, _ 'T[A]P
      _BOX,11,2,13,3,cWhite,_BOX,13,4,14,7,cWhite,_BOX,10,5,14,6,cWhite, _
    _ROUND,17,4,2,cWhite,_ROUND,17,4,1,cRed,_BOX,15,7,19,8,cRed,         _ 'TA[P]
      _BOX,15,2,17,7,cRed,_BOX,15,2,16,7,cWhite,_BOX,16,6,18,7,cRed,     _
      _BOX,15,2,18,3,cWhite,_BOX,15,5,18,6,cWhite,                       _
    _NOAREA,_OFFSET,42,109,_BOX,6,0,13,19,cBlack,_BOX,0,6,19,13,cBlack,  _ 'Keys 19x919
      _BOX,6,0,12,7,cGray,_BOX,0,6,7,12,cGray,_BOX,7,1,12,18,cBorder,    _
      _BOX,1,7,18,12,cBorder,_BOX,9,2,10,5,cGray,_BOX,14,9,17,10,cGray,  _
      _BOX,9,14,10,17,cGray, _BOX,2,9,5,10,cGray,                        _
    _OFFSET,0,122,_LINE,1,3,0,2,cBorder,_BOX,4,0,5,2,cBorder,_LINE,7,3,8,2,cBorder, _ 'Indicator Sign
    _AREA,17,118,8,7,_BOX,0,4,8,5,cBorder,_BOX,1,0,7,4,cWhite, _ 'Arrow UP
      _ROUNDX,1,0,3,cTrans,-1,-1,_ROUNDX,7,0,3,cTrans,1,-1,    _
      _LINE2,4,0,0,4,cBorder,_LINE2,4,0,8,4,cBorder,           _
      _BOX,1,4,7,7,cBorder,_BOX,2,4,6,6,cWhite,                _
    _
    _FINISH }   
   
  rem ========================================================================================================
   
  scope ' ****** Numbers BIG *********
    iOffsetX = 0: iOffsetY = 0
    for CNT as integer = 0 to ubound(iN1)-1 step 5
      if iN1(CNT)=-1 then iOffsetX += 12: CNT += 1 
      line pGfx,(w(iN1(CNT)),h(iN1(CNT+1)))-(w(iN1(CNT+2))-1,h(iN1(CNT+3))-1),iN1(CNT+4),bf
    next CNT 
  end scope 
  scope ' ******* Numbers Small ********
    iOffsetX = 0: iOffsetY = 18
    for CNT as integer = 0 to ubound(iN2)-1 step 5
      if iN2(CNT)=-1 then iOffsetX += 7: CNT += 1 
      line pGfx,(w(iN2(CNT)),h(iN2(CNT+1)))-(w(iN2(CNT+2))-1,h(iN2(CNT+3))-1),iN2(CNT+4),bf
    next CNT
  end scope
  scope ' ***** Generic other graphics *****
    var iOffsetX = 0,iOffsetY = 0,CNT=0
    var iAreaX=0,iAreaY=0,iAreaW=0,iAreaH=0
    dim as fb.image ptr pArea=0,pTemp=pGfx
    #define P(_N_) iGX(CNT+(_N_))
    do
      select case P(0)
      case _ROUND  'Draw a filled Circle X,Y,RADIUS,COLOR   
        if P(3)>=0 or iScale>1 then
          circle pTemp,(w(P(1)),h(P(2))),abs(v(P(3))),P(4),,,,f
        end if
        CNT += 5
      case _ROUNDX 'Draw a filled Circle X,Y,RADIUS,COLOR   
        if P(3)>=0 or iScale>1 then
          circle pTemp,(w(P(1))+P(5),h(P(2))+P(6)),abs(v(P(3))),P(4),,,,f
        end if
        CNT += 7
      case _LINE   'Draws a horz filed line X,Y,XX,YY,COLOR
        for X as integer = 0 to v(1)-1
          line pTemp,(w(P(1))+X,h(P(2)))-(w(P(3))+X,h(P(4))-1),P(5)
        next X
        CNT += 6
      case _LINE2  'Draws a horz filed line X,Y,XX,YY,COLOR
        for Y as integer = 0 to v(1)-1
          line pTemp,(w(P(1)),h(P(2))+Y)-(w(P(3)),h(P(4))+Y),P(5)
        next Y
        CNT += 6
      case _BOX    'Draw a filled box X,Y,XX,YY (exclusive)
        line pTemp,(w(P(1)),h(P(2)))-(w(P(3))-1,h(P(4))-1),P(5),bf
        CNT += 6
      case _ARC    'Draws an outlined partial circle X,Y,RADIUS,COLOR,START,END
        P(5) and= 255: P(6) and= 255
        var fAngS = csng(P(5)*PI128),fAngE = csng((P(6)+.5)*PI128)       
        if iScale=1 then         
          DrawArc(pTemp,w(P(1)),h(P(2)),abs(v(P(3))),P(4),fAngS,fAngE,0)         
        else       
          for iSz as single = -iScaleB to -1 step 1           
            DrawArc(pTemp,w(P(1))-1,h(P(2)),abs(v(P(3)))+iSz,P(4),fAngS,fAngE,1)
          next ISz         
        end if
        CNT += 7
      case _AREA   'offsets to a clipping area X,Y,WID,HEI 
        if pArea then
          put pGfx,(iAreaX,iAreaY),pArea,pset
          ImageDestroy(pArea):pArea=0
        end if       
        iAreaX = v(P(1)): iAreaY = v(P(2))
        iAreaW = v(P(3)): iAreaH = v(P(4))
        pArea = ImageCreate( iAreaW , iAreaH )
        put pArea,(-iAreaX,-iAreaY),pGfx,pset
        iOffsetX=0:iOffsetY=0:pTemp=pArea
        CNT += 5: continue do
      case _NOAREA 'remove previous set clipping           
        if pArea then
          put pGfx,(iAreaX,iAreaY),pArea,pset
          ImageDestroy(pArea):pArea=0
        end if
        iOffsetX=0:iOffsetY=0:pTemp=pGfx:CNT += 1
        if CNT > ubound(iGX) then exit do else continue do
      case _OFFSET 'offsets to a point X,Y                 
        iOffsetX=P(1):iOffsetY=P(2)
        CNT += 3: continue do
      case _BORDER 'Borderize aera with COLOR               
        dim as fb.image ptr pBorder = ImageCreate(iAreaW,iAreaH,cBorder)       
        put pBorder,(0,0),pTemp,and
        for Z as integer = 0 to 1
          if Z = 1 then line pBorder,(0,0)-(iAreaW,iAreaH),cWhite,bf:put pBorder,(0,0),pTemp,and
          for iY as integer = -2+Z to 3-Z
            for iX as integer = -2+Z to 2-Z
              put pGfx,(iAreaX+v(iX),iAreaY+v(iY)),pBorder,trans             
            next iX
          next iY
        next Z
        line pBorder,(0,0)-(iAreaW,iAreaH),P(1)+1,bf:put pBorder,(0,0),pTemp,and
        put pGfx,(iAreaX,iAreaY+v(1)),pBorder,trans       
        line pBorder,(0,0)-(iAreaW,iAreaH),P(1),bf:put pBorder,(0,0),pTemp,and
        put pGfx,(iAreaX,iAreaY),pBorder,trans       
        ImageDestroy(pArea):pArea=0       
        iOffsetX=0:iOffsetY=0:pTemp=pGfx:CNT += 2
      case _SHADOW 'Cast Shadow on text COLOR,BACKGROUND   
        dim as fb.image ptr pBorder = ImageCreate(iAreaW,iAreaH,P(1)+1)       
        line pGfx,(iAreaX,iAreaY)-(iAreaX+iAreaW-1,iAreaY+iAreaH-1),P(2),bf       
        put pBorder,(0,0),pTemp,and
        put pGfx,(iAreaX,iAreaY+v(1)),pBorder,trans       
        line pBorder,(0,0)-(iAreaW,iAreaH),P(1),bf:put pBorder,(0,0),pTemp,and
        put pGfx,(iAreaX,iAreaY),pBorder,trans       
        ImageDestroy(pArea):pArea=0       
        iOffsetX=0:iOffsetY=0:pTemp=pGfx:CNT += 3
      end select     
      if iDebug andalso pArea then put pGfx,(iAreaX,iAreaY),pArea,pset
    loop
  end scope
  scope ' ******* Diagonal Lines *******
    iOffsetX = 116: iOffsetY = 46
    for CNT as integer = 0 to v(1)-1
      line pGfx,(w(6)+CNT,h(2))-(w(0)+CNT,h(9)-1),cWall3
      line pGfx,(w(12)+CNT,h(2))-(w(6)+CNT,h(9)-1),cWall5
    next CNT
    for CNT as integer = 0 to v(5)-1
      line pGfx,(w(7)+CNT,h(2))-(w(1)+CNT,h(9)-1),cWall4
    next CNT   
  end scope
  scope ' ********* Pipes **********
    iOffsetY=46
    var fMul = (PI*90)/v(22),fMulB = (PI*90)/v(16)
    for CNT as integer = 0 to v(22)-1
      iOffsetX=64
      line pGfx,(w(1)+CNT,h(2))-(w(1)+CNT,h(10)-1),cPipeZ-abs(cos(fMul*((v(24)-1)-CNT)))*47
      line pGfx,(w(1)+CNT,h(1))-(w(1)+CNT,h(2)-1),cPipeA+abs(cos(fMul*((v(24)-1)-CNT)+PI*80))*34
      iOffsetX=90
      line pGfx,(w(1)+CNT,h(2))-(w(1)+CNT,h(10)-1),cPipeZ-abs(cos(fMul*((v(24)-1)-CNT)))*47
      line pGfx,(w(1)+CNT,h(10))-(w(1)+CNT,h(11)-1),cPipeA+abs(cos(fMul*((v(24)-1)-CNT)+PI*80))*34
    next CNT
    iOffsetX=104:iOffsetY=58
    for CNT as integer = 0 to v(22)-1
      line pGfx,(w(1)+CNT,h(0))-(w(1)+CNT,h(12)-1),cPipeZ-abs(cos(fMulB*((v(24)-1)-CNT)))*47
    next CNT     
  end scope
end sub
sub DrawNumber(iNumber as integer,iX as integer,iY as integer,iFormat as integer=0)
  var sText = iNumber & "",iSz=0,iBg=(iFormat and 255)
  dim as integer iSA=any,iSB=any,iPY=any,iSY=any
  if (iFormat and ftSmall) then
    iSA=v(7):iSB=v(5):iPY=v(18):iSY=v(10)   
  else
    iSA=v(12):iSB=v(8):iPY=v(0):iSY=v(18)
  end if
  if (iFormat and ftCenter) then   
    for CNT as integer = 0 to len(sText)-1
      select case sText[CNT]
      case asc("0"),asc("2") to asc("9"): iSz += iSA
      case else: iSz += iSB
      end select
    next CNT
    iX -= iSz\2
  end if
  for CNT as integer = 0 to len(sText)-1
    select case sText[CNT]
    case asc("0"),asc("2") to asc("9")
      var iPX = (sText[CNT]-asc("0"))*iSA
      if iBg then line(iX,iY)-(v(1)+iX+iSA-1,iY+iSY-1),iBg,bf
      put(iX,iY),pGfx,(iPX,iPY)-(iPX+iSA-1,iPY+iSY-1),trans
      iX += iSA+v(1): if iX > ScrW then exit sub
    case asc("1")     
      if iBg then line(iX,iY)-(v(1)+iX+iSA-1,iY+iSY-1),iBg,bf
      put(iX,iY),pGfx,(iSA,iPY)-(iSA+iSB-1,iPY+iSY-1),trans
      iX += iSB+v(1): if iX > ScrW then exit sub
    case else
      if iBg then line(iX,iY)-(v(1)+iX+iSA-1,iY+iSY-1),iBg,bf
      iX += iSB+v(1)
    end select
  next CNT
end sub
sub DrawBird(iPX as integer,iPY as integer,iAngle as integer,iWing as integer,iColor as integer,pTarget as fb.image ptr=0)
  static as integer iBird(...) = { _
    _ROUND,11,3,1,4,_ROUND,9,8,5,3,_ROUND,8,35,25,2,_ROUND,15,5,3,7,           _ 'Bird Main
      _ARC,10,5,8,1,145,204,_ARC,11,10,8,1,50,123,_ARC,12,7,5,1,246,63,        _
      _ARC,11,10,7,4,61,104,_ARC,18,9,1,1,173,24,_ROUND,14,10,1,5,             _
      _ROUND,15,10,1,5,_ROUND,16,10,1,5,_ROUND,17,10,1,5,_ARC,18,10,1,5,14,82, _
      _ARC,14,5,3,1,75,207,_ARC,17,30,22,1,66,72,_ARC,14,30,22,1,53,64,        _
      _ARC,15,5,7,1,171,204,_ARC,17,-12,22,1,182,191,_ARC,16,3,7,1,194,214,    _
      _ARC,14,-12,22,1,194,203,_ARC,14,11,3,1,74,119,_ARC,14,9,3,1,144,189,    _
      _ARC,16,10,2,1,177,9,_ARC,13,10,1,5,39,216,_ARC,15,5,3,6,122,161,        _
      _ARC,14,5,3,1,75,207,_ARC,12,6,3,1,250,22,_ARC,11,11,2,1,183,238,        _
      _PAINT,15,14,0,1,_PAINT,4,12,0,1,_ROUND,-1,8,3,0,                        _
    _OFFSET,0,41,_ROUNDX,6,7,2,7,_ARC,6,6,3,4,151,238,_ARC,6,7,3,1,177,255,    _ 'Bird Wing 0
      _ARC,6,8,3,1,11,76,_ARC,6,7,3,1,116,191,_ARC,5,7,2,1,61,145,_FINISH,     _
    _OFFSET,1,72,_ROUND,5,8,1,7,_ROUNDX,7,8,1,7,_ROUND,6,9,1,7,                _ 'Bird Wing 1
      _ARC,6,7,3,4,146,176,_ARC,6,7,3,4,210,240,_ARC,7,7,3,1,184,237,          _
      _ARC,7,10,3,1,20,73,_ARC,5,10,3,1,56,109,_ARC,5,7,3,1,145,198,           _
      _ARC,6,7,3,1,180,203,_ARC,6,10,3,1,52,75,_FINISH,                        _
    _OFFSET,2,65,_ROUNDX,5,10,2,7,_ROUNDX,6,9,1,7,_ROUNDX,7,10,1,7,            _ 'Bird Wing 2
      _ARC,6,12,4,4,29,48,_ARC,2,8,2,4,213,252,_ARC,7,13,2,4,59,98,            _
      _ARC,7,11,3,1,30,69,_ARC,6,10,3,1,91,160,_ARC,6,19,11,1,56,72,           _
      _ARC,4,6,6,1,190,237,_FINISH }
  rem ---------------------------------------------------------------------------
  static as integer iInit,iPivX,iPivY,iScaleB,iScaleC,iScaleD
  static as integer iOldAngA=-2^29,iOldAngB=-2^29,iOldWingA=-1,iOldWingB=-1
  static as integer ptr piLastAng,piLastWing
  static as fb.image ptr pBird
  const fpBits=16,fpMul=1 shl fpBits,PImul=csng((atn(1)*4)/(256\2))
  #define fBird(_N) *cptr(single ptr,@iBird(_N))
  'Initialize and convert array x,y to ang,dist
  if iInit=0 then
    iInit = 1: iScaleB = (iScale+1)\2:iScaleC=iScaleB-1:iScaleD=(iScale\2)-1
    iPivX = 7*iScale+iScaleB: iPivY = (8+2)*iScale+iScaleB   
    for CNT as integer = 0 to ubound(iBird)     
      if iBird(CNT) = _OFFSET then CNT += 2: continue for
      if iBird(CNT) = _FINISH then continue for
      var iX = cint(iBird(CNT+1)*iScale)+iScaleB
      var iY = cint((iBird(CNT+2)+2)*iScale)+iScaleB
      #ifdef UseFloats
        fBird(CNT+1) = atan2(iY-iPivY,iX-iPivX)
        fBird(CNT+2) = sqr(((iY-ipivY)*(iY-iPivY))+((iX-iPivX)*(iX-iPivX)))
      #else
        iBird(CNT+1) = cint(atan2(iY-iPivY,iX-iPivX)/PImul)
        iBird(CNT+2) = cint(sqr(((iY-ipivY)*(iY-iPivY))+((iX-iPivX)*(iX-iPivX)))*fpMul)
      #endif
      select case iBird(CNT)
      case _ROUND : iBird(CNT+3) = (iBird(CNT+3)*iScale)+iScaleD+1:CNT += 4
      case _ROUNDX: iBird(CNT+3) = (iBird(CNT+3)*iScale):CNT += 4
      case _ARC   : iBird(CNT+3) *= iScale:CNT += 6
      case _PAINT : CNT += 4     
      end select
    next CNT
  end if 
  if iColor = cBirdA then
    PiLastAng=@iOldAngB:PiLastWing=@iOldWingA:pBird=pBirdA
  else
    PiLastAng=@iOldAngB:PiLastWing=@iOldWingB:pBird=pBirdB
  end if
  'Draw Bird in temporary buffer
  if iAngle <> *piLastAng or iWing <> *piLastWing then
    *piLastAng = iAngle: *piLastWIng = iWing
    line pBird,(0,0)-(pBird->Width-1,pBird->Height-1),cTrans,bf
    dim as integer iPal(...) = { cTrans,cBorder,iColor, _
    iColor+1,iColor+2,iColor+3,iColor+4,iColor+5 }
    if iColor=cBirdB then iPal(1) = iColor+6
    for CNT as integer = 0 to ubound(iBird)
      if iBird(CNT) = _FINISH then exit for
      var fTempAng = csng((iBird(CNT+1)+iAngle)*PImul)
      var iX = (iPivX+(cos(fTempAng)*iBird(CNT+2)) shr fpBits)
      var iY = (iPivY+(sin(fTempAng)*iBird(CNT+2)) shr fpBits)
      select case iBird(CNT)
      case _ARC
        iBird(CNT+5) and= 255: iBird(CNT+6) and= 255
        var fAngS = csng(((iBird(CNT+5)-iAngle) and 255)*PImul)
        var fAngE = csng((((iBird(CNT+6)-iAngle) and 255)+.5)*PImul)       
        if iScale=1 then         
          DrawArc(pBird,iX,iY,iBird(CNT+3),iPal(iBird(CNT+4)),fAngS,fAngE,0)         
        else       
          for iSz as single = -iScaleC to iScaleD step 1           
            DrawArc(pBird,iX,iY,iBird(CNT+3)+iSz,iPal(iBird(CNT+4)),fAngS,fAngE,1)
          next ISz         
        end if
        CNT += 6
      case _ROUND       
        circle pBird,(iX,iY),iBird(CNT+3),iPal(iBird(CNT+4)),,,,f             
        CNT += 4
      case _ROUNDX       
        circle pBird,(iX,iY),iBird(CNT+3)-1,iPal(iBird(CNT+4)),,,,f             
        CNT += 4
      case _PAINT
        paint pBird,(iX,iY),iPal(iBird(CNT+3)),iPal(iBird(CNT+4))       
        CNT += 4
      case _OFFSET       
        if iBird(CNT+1) <> iWing then CNT += iBird(CNT+2)
        CNT += 2
      case else
        #ifdef Messagebox
        Messagebox(null,"Bad Parameters3","Bad Paramaters3",MB_SYSTEMMODAL or MB_ICONSTOP)
        #endif
      end select     
    next CNT   
  end if 
  'Put to target
  if pBird = pBirdA then iBirdX = iPX-iPivX: iBirdY = iPY-iPivY
  put pTarget,(iPX-iPivX,iPY-iPivY),pBird,trans
end sub
sub DrawMedal(iPX as integer,iPY as integer,iMedal as integer)
  static as integer iInit=0,iLastMedal=-1
  static as fb.image ptr pMedal,pTemp
  if iInit=0 then
    iInit = 1
    pMedal = ImageCreate(v(22),v(22))
    pTemp = ImageCreate(v(22),v(22))
  end if 
  if iLastMedal <> iMedal then
    iLastMedal = iMedal
    put pMedal,(v(0),v(0)),_gNoMedal_,pset
    if iMedal then     
      for iY as integer = -1 to 1 step 1
        line pTemp,(v(0),v(0))-(v(22),v(22)),cTrans,bf
        DrawBird(v(6)+iScale\2,v(10+iY)+iScale\2,0,1,cBirdB,pTemp)
        var pPix = cast(ubyte ptr,pTemp+1),iColor=cMedal2+(abs(iY+1)\2)
        for CNT as integer= 0 to (pTemp->Pitch*pTemp->Height)-1
          if pPix[CNT] <> cTrans then pPix[CNT] = iColor
        next CNT
        put pMedal,(v(0),v(0)),pTemp,trans
      next iY
      DrawBird(v(6)+iScale\2,v(10)+iScale\2,0,1,cBirdB,pMedal)
    end if
  end if 
  put (iPX,iPY),pMedal,pset
end sub
sub SaveConfig()
  var f = freefile()
  if open(exepath+"/FlappyFB.cfg" for binary access write as #f)=0 then
    var iTempA = iBest xor &h12345678, iTempB = ((-(2^30))+iBest) xor &h87654321
    put #f,,iTempA: put #f,,iTempB: put #f,,iFull: put #f,,iDither
    put #f,,iSync: put #f,,iShowCrash: put #f,,iNewScale\AA
    close #f
  end if
end sub
sub LoadConfig()
  var f = freefile()
  if open(exepath+"/FlappyFB.cfg" for binary access read as #f)=0 then
    var iTempA=0,iTempB=0
    get #f,,iTempA: get #f,,iTempB
    iTempA xor= &h12345678: iTempB = (iTempB xor &h87654321)+(2^30)
    if iTempA <> iTempB then close #f: exit sub
    iBest = iTempA: get #f,,iFull: get #f,,iDither
    get #f,,iSync: get #f,,iShowCrash: get #f,,iScale
    if iScale < 0 or iScale > 16 then iScale = 0 
    iNewScale = iScale
    close #f
  end if
end sub

randomize()
LoadConfig()

#ifdef __FB_DOS__
  ScrW=640:ScrH=480:iHz=60
#else
  screeninfo ScrW,ScrH,,,,iHZ
  if iFull=0 then ScrH *= .93
  if iHZ < 50 then iHZ = 60 
#endif
iHZ *= 2

' *** Create Graphic Screen ***
if iScale=0 then iScale = (ScrH*AA\240) else iScale *= AA
iWid = v(160): iHei = v(240)
ScrW *= AA: ScrH *= AA 
iOffsetX=iif(iFull,((ScrW-iWid)\2)\iScale,0)
iOffsetY=iif(iFull,((ScrH-iHei)\2)\iScale,0)
if iFull then
  screenres ScrW,ScrH,8,,fb.gfx_HIGH_PRIORITY or _
  fb.gfx_NO_FRAME or fb.GFX_ALWAYS_ON_TOP or fb.GFX_NO_SWITCH
else
  screenres iWid,iHei,8,,fb.gfx_HIGH_PRIORITY or fb.GFX_NO_SWITCH
end if
WindowTitle "FlappyFB v1.0 by Mysoft"
if iFull then
  setmouse ,,0
  line(0,0)-(ScrW,ScrH),cBlack,bf
  line(w(0)-1,h(0)-1)-(w(0)+iWid,h(0)+iHei),cWhite,b
  view screen (w(0),h(0))-(w(0)+iWid-1,h(0)+iHei-1)
end if
' *** Create Initial Graphics ***
pBirdA = ImageCreate(24*iScale,24*iScale)
pBirdB = ImageCreate(24*iScale,24*iScale)
pCollision = ImageCreate(24*iScale,24*iScale)
SetPalette(0) 'Night
CreateGraphics()

' *** Prepare screen and dither effect buffer ***
line(w(0),h(204))-(w(160)-1,h(240)-1),cWall8,bf
if iDither then
  pDither = ImageCreate(v(160),v(240),0)
  if AA=1 then
    for Y as integer = 0 to v(240)-1 step 2
      line pDither,(0,Y)-(v(160)-1,Y),128,,&h5555
      line pDither,(0,Y+1)-(v(160)-1,Y+1),128,,&hAAAA
    next Y
  else
    for Y as integer = 0 to v(240)-1 step 4
      line pDither,(0,Y)-(v(160)-1,Y),128,,&hCCCC
      line pDither,(0,Y+1)-(v(160)-1,Y+1),128,,&hCCCC
      line pDither,(0,Y+2)-(v(160)-1,Y+2),128,,&h3333
      line pDither,(0,Y+3)-(v(160)-1,Y+3),128,,&h3333
    next Y
  end if
end if

' *** Initial game variables ***
var dStart = timer,iFps=0,dFps=dStart,dTimer=dStart
var dDay=dStart-rnd*256,dPause=dStart
var iRandom=0,dPipe=dStart,dBird=dStart,iPipePosi=-v(73*3),iPipe=-1
var iIsReady=0,iIsOver=0,iScore=0,iIsBest=0,iIsCrash=0,iLastFps=iHz\2
var iPosiS=0,iPosiB=0,iPosiG=0,iPosiW=0,iBirdColor=int(rnd*3)
dim fBirdY as single,fBirdAng as single,iBirdUp as integer
dim as integer iScoreY(3),iPipeY(3) = {72,110,45,90}
dim as integer iStarX(19),iStarY(19),iStarSz(19)

' ************ Main Game Loop ***************
do 
  if iSync=1 then dTimer += 1/iLastFps else dTimer = timer
  var dTmr = (timer-dStart) 'dTmr += (1/240)   
  ' *** Pipes movement calculation ***
  if abs(dTmr-dPipe) > 1 then
    dPipe = dTmr
  else
    while (dTmr-dPipe) > (1/v(60))
      dPipe += (1/v(60))
      if iIsReady xor iIsOver then
        iPipePosi = iPipePosi+1         
        if iPipePosi >= v(73) then '73
          iPipePosi -= v(73)
          iPipe = (iPipe+1) and 3
          iPipeY((iPipe+3) and 3) = 40+rnd*60
          iScoreY((iPipe+3) and 3)=0
        end if       
      end if
    wend
  end if
  ' *** Bird Movement calculation
  if iIsReady<>0 andalso (iIsOver=0 or iIsCrash<>0) then
    while (dTmr-dBird) > (1/120)
      dBird += (1/120)
      if iBirdUp then
        fBirdAng = ((-25+(fBirdAng*3))\4)-1
        if fBirdAng <= -25 then fBirdAng=-25:iBirdUp=0
      else
        fBirdAng += .85     
        if fBirdAng > 64 then fBirdAng = 64
      end if
      fBirdY += sin(PI128*fBirdAng)*v(2)
      if fBirdY < h(-24) then fBirdY = h(-24)
      if fBirdY > h(182) then
        fBirdY = h(182)         
        if iIsOver=0 or iIsCrash>60 then           
          dPause=dTmr:iBirdUp=0:iIsCrash=60:iIsOver=1
          if iScore > iBest then iBest = iScore:iIsBest=1
        end if
      end if
      if iIsCrash then
        iIsCrash -= 1:if iIsCrash=0 then iIsReady=1
      end if
    wend
  end if
 
  screenlock 
    ' *** Update Palette and Day/Night effects ***
    var iMedal = iif(iScore>40,4,iScore\10)
    if SetPalette(,(timer-dDay)*24,iBirdColor or (iMedal shl 8)) or iStarY(0)=0 then
      for CNT as integer = 0 to 19
        iStarX(CNT) = w(rnd*160):iStarY(CNT) = h(40+rnd*40):iStarSz(CNT) = v(rnd)
      next CNT
    end if   
    ' *** Draw Sky ***
    line(w(0),h(0))-(w(160)-1,h(160)-1),cSky,bf   
    for CNT as integer = w(0) to w(160)-1 step v(64)
      put (CNT,h(147)),_gClouds_,trans
    next CNT
    ' *** Draw Stars (night) ***
    for CNT as integer = 0 to 19
      circle(iStarX(CNT),iStarY(CNT)),iStarSz(CNT),cStars,,,,f
    next CNT
    ' *** Draw far Building shadows ***
    line(w(0),h(162))-(w(160)-1,h(184)-1),cClouds,bf
    if iIsOver=0 then iPosiS = (dTmr*v(5)) mod v(32)
    for CNT as integer = w(0)-iPosiS to w(160)-1 step v(32)
      put (CNT,h(162)),_gShadows_,trans
    next CNT
    ' *** Draw buildings ***
    if iIsOver=0 then iPosiB = (dTmr*v(10)) mod v(34)
    for CNT as integer = w(0)-iPosiB to w(160)-1 step v(34)
      put (CNT,h(162)),_gBuildings_,trans
    next CNT
    ' *** Draw Bushes ***
    if iIsOver=0 then iPosiG = (dTmr*v(20)) mod v(64)
    for CNT as integer = w(0)-iPosiG to w(160)-1 step v(64)
      put (CNT,h(176)),_gBushes_,trans
    next CNT
    ' *** Draw Pipes ***
    for X as integer = 0 to 3           
      if (iPipe+X) >= 0 then
        var iY = iPipeY((iPipe+X) and 3)
        for Y as integer = h(0) to h(iY)-1 step v(12)
          put (w(X*73)-iPipePosi,Y),_gPipeBody_,pset
        next Y     
        put (w(X*73)-iPipePosi,h(iY)),_gPipeLine_,pset
        put (w(X*73-1)-iPipePosi,h(iY+1)),_gPipeUp_,pset
        put (w(X*73-1)-iPipePosi,h(iY+13+48)),_gPipeDown_,pset
        put (w(X*73)-iPipePosi,h(iY+25+48)),_gPipeLine_,pset
        for Y as integer = h(iY+50+24) to h(193)-1 step v(12)
          put (w(X*73)-iPipePosi,Y),_gPipeBody_,pset
        next Y
      end if
    next X
    ' *** Draw Get Ready Stage ***
    if iIsReady=0 then
      put(w(32),h(60)),_gGetReady_,trans
      put(w(52),h(130)),_gTapLeft_,trans
      line(w(68),h(131))-(w(69)-1,h(138)-1),cRed,bf
      put(w(88),h(130)),_gTapRight_,trans
      line(w(92),h(131))-(w(93)-1,h(138)-1),cRed,bf
      put(w(71),h(135)),_gKeys_,trans
      if ((dTmr*6) mod 3)<2 then put(w(76),h(128)),_gSign_,trans
      put(w(76)+(v(1)\2),h(116)),_gArrowUp_,trans     
      var iPosi = (dTmr*400) mod 360,iPosi2 = (dTmr*12) mod 4         
      DrawBird(w(48),v(117)+sin(iPosi*PI)*v(4),0,abs(-2+iPosi2),cBirdA) '(abs(iPosi-80)-40)
      DrawBird(w(76),v(102),0,1,cBirdB)
    elseif iIsOver=0 or IIsCrash<>0 then
      DrawNumber(iScore,w(80),h(22),ftBig+ftCenter)     
    end if
    ' *** Draw in Stage bird and check collision
    if iIsReady then
      var iPosi2 = (dTmr*12) mod 4,iAng = iif(fBirdAng<-18,-18,cint(fBirdAng))
      var iWing = iif(iIsOver,1,abs(-2+iPosi2))
      DrawBird(w(48),fBirdY,iAng,iWing,cBirdA) '(abs(iPosi-80)-40)
      if iIsOver=0 then 'Collision
        for X as integer = 0 to 3           
          if (iPipe+X) >= 0 then
            var iY = iPipeY((iPipe+X) and 3)
            var iX = (w(X*73)-iPipePosi)-iBirdX,iXX=iX+v(24)-1
            if iXX < v(-1) then 'totally passed a pipe
              if iScoreY((iPipe+X) and 3)=0 then
                iScore += 1: iScoreY((iPipe+X) and 3) = 1
              end if
              continue for
            end if
            if iX < v(25) then 'possible collision
              if iBirdY < h(0) then iIsOver=1:iIsCrash=1000:dPause=dTmr: exit for
              memcpy(pCollision+1,pBirdA+1,pBirdA->Pitch*pBirdA->Height)
              line pCollision,(iX,h(0)-iBirdY)-(iXX,(h(iY)-1)-iBirdY),cTrans,bf
              line pCollision,(iX-v(1),h(iY)-iBirdY)-(iXX+(v(1)-1),(h(iY+13)-1)-iBirdY),cTrans,bf
              line pCollision,(iX-v(1),h(iY+61)-iBirdY)-(iXX+(v(1)-1),(h(iY+74)-1)-iBirdY),cTrans,bf
              line pCollision,(iX,h(iY+74)-iBirdY)-(iXX,(h(193)-1)-iBirdY),cTrans,bf
              'put (iBirdX,iBirdY),pCollision,pset
              if memcmp(pCollision+1,pBirdA+1,pBirdA->Pitch*pBirdA->Height) then
                if iShowCrash then
                  put pCollision,(iBirdX,iBirdY),pBirdA,xor
                  put (iBirdX,iBirdY),pCollision,xor
                end if
                iIsOver=1:iIsCrash=1000:dPause=dTmr               
              end if             
              exit for
            end if
          end if
        next X               
      end if   
    end if
    ' *** Draw bottom wall ***
    if iIsOver=0 then iPosiW = (dTmr*v(60)) mod v(12)
    for CNT as integer = w(0)-iPosiW to w(160)-1 step v(12)
      put (CNT,h(193)),_gWall_,pset
    next CNT   
    ' *** Draw GameOver Stage with medal and score ***
    if iIsOver andalso iIsCrash=0 then
      put(w(32),h(48)),_gGameOver_,trans
      put (w(24),h(80)),_gFrameLT_,trans:put (w(128),h(80)),_gFrameRT_,trans
      put (w(24),h(128)),_gFrameLD_,trans:put (w(128),h(128)),_gFrameRD_,trans
      for X as integer = w(32) to w(120) step v(8)
        put (X,h(80)),_gFrameU_,pset: put (X,h(128)),_gFrameD_,pset
      next X
      for Y as integer = h(88) to h(120) step v(8)
        put (w(24),Y),_gFrameL_,pset
        put (w(128),Y),_gFrameR_,pset
      next Y
      line(w(32),h(88))-(w(128)-1,h(128)-1),cFrame2,bf
      put(w(37),h(88)),_gMedal_,pset
      put(w(104),h(88)),_gScore_,pset
      put(w(109),h(108)),_gBest_,pset
      DrawMedal(w(38),h(102),iMedal)     
      if iIsBest then put(w(92),h(107)),_gNew_,pset
      DrawNumber(iScore,w(116),h(96),ftSmall+ftCenter)
      DrawNumber(iBest,w(116),h(116),ftSmall+ftCenter)
    end if
    ' *** Draw and calc Fps ***
    if (timer-dFps) >= 1 then   
      line(w(0),h(204))-(w(160)-1,h(240)-1),cWall8,bf
      DrawNumber(iFps,w(1),h(229),cWall8 or ftSmall)
      iLastFps=iFps:iFps=-1:dFps=timer
    end if     
    iFps += 1     
    ' *** Apply dither Effect ***
    if iDither then put(w(0),h(0)),pDither,or
   
    ' *** Sync frame (pre-delay) ***
    if iFocus andalso iSync=1 or iSync=2 then screensync
   
  screenunlock
 
  ' *** Sync frame (pos-delay) ***
  select case iSync
  case 0
    static as double dSync
    if abs(timer-dSync) > .5 then
      dSync=timer
    else
      while (timer-dSync) < (1/iHz)       
        sleep 1,1
      wend
      dSync += 1/iHz
    end if
  case 2,3
    sleep 1,1
  end select
  if iFocus=0 then sleep 30,1
 
  if iShowCrash andalso iIsCrash=1000 then
    var dTemp = timer:sleep 100,1
    while len(inkey):wend:sleep 1000
    dStart += (timer-dTemp)
  end if
 
  ' *** Input Events ***
  do
    dim iPress as integer=0,InEvent as fb.event
    if screenevent( @InEvent )=0 then exit do
    select case InEvent.type   
    case fb.EVENT_KEY_PRESS
      dim as integer iKey = InEvent.ascii
      if iKey=0 then iKey = -InEvent.scancode   
      select case iKey
      case 27 'Escape
        if iIsReady andalso iIsOver=0 then       
          iIsOver=1:iIsCrash=1000:dPause=dTmr
        else
          SaveConfig(): exit do,do
        end if
      case -fb.SC_F1
        iDither xor= 1: iLastFps=iHz\2:iFps=0:dFps=timer
        line(w(2),h(208))-(w(160)-1,h(208)+19),cWall8,bf
        draw string (w(2),h(208)),"Dither: " & iif(iDither,"enabled","disabled"),cRed
        put(w(0),h(0)),pDither,or
      case -fb.SC_F2
        iSync = (iSync+1) mod 5
        dPause = dtimer-dStart: dtimer = timer: dStart = dtimer-dPause     
        iLastFps=iHz\2:iFps=0:dFps=timer     
        line(w(2),h(208))-(w(160)-1,h(208)+19),cWall8,bf
        dim as zstring ptr zSync(...) = {@"Timer+Sleep",@"VSYNC",@"VSYNC+Sleep 1",@"Sleep 1",@"None(max)"}
        draw string (w(2),h(208)),"Sync: " & *zSync(iSync),cRed     
      case -fb.SC_F3
        iFull xor= 1: iLastFps=iHz\2:iFps=0:dFps=timer
        line(w(2),h(208))-(w(160)-1,h(208)+19),cWall8,bf
        draw string (w(2),h(208)),"Fullscreen: " & iif(iFull,"enabled","disabled"),cRed       
        draw string (w(2),h(208)+10),"(requires restart)",cRed
      case -fb.SC_F4
        iNewScale = (iNewScale+1) and 7: iLastFps=iHz\2:iFps=0:dFps=timer
        line(w(2),h(208))-(w(160)-1,h(208)+19),cWall8,bf
        if iNewScale then
          draw string (w(2),h(208)),"Scale: " & iNewScale & "x",cRed
        else
          draw string (w(2),h(208)),"Scale: AUTO",cRed
        end if
        draw string (w(2),h(208)+10),"(requires restart)",cRed
      case -asc("k"),-asc("a"),-fb.SC_F9 'Button
        SaveConfig(): exit do,do
      case -fb.SC_UP: iPress = 1       
      end select   
    case fb.EVENT_MOUSE_BUTTON_PRESS: iPress = 1
    case fb.EVENT_MOUSE_DOUBLE_CLICK: iPress = 1
    case fb.EVENT_WINDOW_CLOSE: SaveConfig(): exit do,do           
    case fb.EVENT_WINDOW_GOT_FOCUS: iFocus = 1
    case fb.EVENT_WINDOW_LOST_FOCUS: iFocus = 0
    end select
   
    if iPress then
      if iIsOver=0 and iIsReady=0 then
        iIsReady = 1: iIsOver = 0
        iScore = 0 : iIsBest = 0
        fBirdY = v(117):fBirdAng=0
        dBird = dTmr:iBirdUp=1
        for CNT as integer = 0 to 3
          iPipeY(CNT) = 30+rnd*70: iScoreY(CNT)=0
        next CNT
      elseif iIsOver=0 and iIsCrash=0 then
        iBirdUp = 1
      end if
      if iIsOver andalso iIsReady andalso iIsCrash=0 then
        iIsOver = 0: iIsReady = 0: iScore = 0
        dStart = dtimer-dPause: iBirdColor=int(rnd*3)
        iPipePosi=-v(73*3):iPipe=-1
      end if
    end if   
  loop 
loop
