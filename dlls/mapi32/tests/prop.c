/*
 * Unit test suite for MAPI property functions
 *
 * Copyright 2004 Jon Griffiths
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "wine/test.h"
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winerror.h"
#include "winnt.h"
#include "mapiutil.h"
#include "mapitags.h"

HRESULT WINAPI MAPIInitialize(LPVOID);

static HMODULE hMapi32 = 0;

static SCODE        (WINAPI *pScInitMapiUtil)(ULONG);
static SCODE        (WINAPI *pPropCopyMore)(LPSPropValue,LPSPropValue,ALLOCATEMORE*,LPVOID);
static ULONG        (WINAPI *pUlPropSize)(LPSPropValue);
static BOOL         (WINAPI *pFPropContainsProp)(LPSPropValue,LPSPropValue,ULONG);
static BOOL         (WINAPI *pFPropCompareProp)(LPSPropValue,ULONG,LPSPropValue);
static LONG         (WINAPI *pLPropCompareProp)(LPSPropValue,LPSPropValue);
static LPSPropValue (WINAPI *pPpropFindProp)(LPSPropValue,ULONG,ULONG);
static SCODE        (WINAPI *pScCountProps)(INT,LPSPropValue,ULONG*);
static SCODE        (WINAPI *pScCopyProps)(int,LPSPropValue,LPVOID,ULONG*);
static SCODE        (WINAPI *pScRelocProps)(int,LPSPropValue,LPVOID,LPVOID,ULONG*);
static LPSPropValue (WINAPI *pLpValFindProp)(ULONG,ULONG,LPSPropValue);
static BOOL         (WINAPI *pFBadRglpszA)(LPSTR*,ULONG);
static BOOL         (WINAPI *pFBadRglpszW)(LPWSTR*,ULONG);
static BOOL         (WINAPI *pFBadRowSet)(LPSRowSet);
static ULONG        (WINAPI *pFBadPropTag)(ULONG);
static ULONG        (WINAPI *pFBadRow)(LPSRow);
static ULONG        (WINAPI *pFBadProp)(LPSPropValue);
static ULONG        (WINAPI *pFBadColumnSet)(LPSPropTagArray);

static ULONG ptTypes[] = {
    PT_I2, PT_I4, PT_R4, PT_R8, PT_CURRENCY, PT_APPTIME, PT_SYSTIME,
    PT_ERROR, PT_BOOLEAN, PT_I8, PT_CLSID, PT_STRING8, PT_BINARY,
    PT_UNICODE
};

static inline int strcmpW(const WCHAR *str1, const WCHAR *str2)
{
    while (*str1 && (*str1 == *str2)) { str1++; str2++; }
    return *str1 - *str2;
}

static void test_PropCopyMore(void)
{
    static const char *szHiA = "Hi!";
    static const WCHAR szHiW[] = { 'H', 'i', '!', '\0' };
    SPropValue *lpDest = NULL, *lpSrc = NULL;
    ULONG i;
    SCODE scode;
    
    pPropCopyMore = (void*)GetProcAddress(hMapi32, "PropCopyMore@16");

    if (!pPropCopyMore)
        return;

    scode = MAPIAllocateBuffer(sizeof(LPSPropValue), (LPVOID *)lpDest);
    if (FAILED(scode))
        return;
        
    scode = MAPIAllocateMore(sizeof(LPSPropValue), lpDest, (LPVOID *)lpSrc);
    if (FAILED(scode))
        return;

    for (i = 0; i < sizeof(ptTypes)/sizeof(ptTypes[0]); i++)
    {
        lpSrc->ulPropTag = ptTypes[i];
        
        switch (ptTypes[i])
        {
        case PT_STRING8:
            lpSrc->Value.lpszA = (char*)szHiA;
            break;
        case PT_UNICODE:
            lpSrc->Value.lpszW = (WCHAR*)szHiW;
            break;
        case PT_BINARY:
            lpSrc->Value.bin.cb = 4;
            lpSrc->Value.bin.lpb = (LPBYTE)szHiA;
            break;
        }

        memset(lpDest, 0xff, sizeof(SPropValue));
        
        scode = pPropCopyMore(lpDest, lpSrc, MAPIAllocateMore, lpDest);
        ok(!scode && lpDest->ulPropTag == lpSrc->ulPropTag,
           "PropCopyMore: Expected 0x0,%ld, got 0x%08lx,%ld\n",
           lpSrc->ulPropTag, scode, lpDest->ulPropTag);
        if (SUCCEEDED(scode))
        {
            switch (ptTypes[i])
            {
            case PT_STRING8:
                ok(lstrcmpA(lpDest->Value.lpszA, lpSrc->Value.lpszA) == 0,
                   "PropCopyMore: Ascii string differs\n");
                break;
            case PT_UNICODE:
                ok(strcmpW(lpDest->Value.lpszW, lpSrc->Value.lpszW) == 0,
                   "PropCopyMore: Unicode string differs\n");
                break;
            case PT_BINARY:
                ok(lpDest->Value.bin.cb == 4 && 
                   !memcmp(lpSrc->Value.bin.lpb, lpDest->Value.bin.lpb, 4),
                   "PropCopyMore: Binary array  differs\n");
                break;
            }
        }
    }
    
    /* Since all allocations are linked, freeing lpDest frees everything */
    MAPIFreeBuffer(lpDest);
}

static void test_UlPropSize(void)
{
    static const char *szHiA = "Hi!";
    static const WCHAR szHiW[] = { 'H', 'i', '!', '\0' };
    LPSTR  buffa[2];
    LPWSTR buffw[2];
    SBinary buffbin[2];
    ULONG pt, exp, res;

    pUlPropSize = (void*)GetProcAddress(hMapi32, "UlPropSize@4");

    if (!pUlPropSize)
        return;

    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        SPropValue pv;

        memset(&pv, 0 ,sizeof(pv));
        pv.ulPropTag = pt;
        
        exp = 1u; /* Default to one item for non-MV properties */
        
        switch (PROP_TYPE(pt))
        {
        case PT_MV_I2:       pv.Value.MVi.cValues = exp = 2;
        case PT_I2:          exp *= sizeof(USHORT); break;
        case PT_MV_I4:       pv.Value.MVl.cValues = exp = 2;
        case PT_I4:          exp *= sizeof(LONG); break;
        case PT_MV_R4:       pv.Value.MVflt.cValues = exp = 2;
        case PT_R4:          exp *= sizeof(float); break;
        case PT_MV_DOUBLE:   pv.Value.MVdbl.cValues = exp = 2;
        case PT_R8:          exp *= sizeof(double); break;
        case PT_MV_CURRENCY: pv.Value.MVcur.cValues = exp = 2;
        case PT_CURRENCY:    exp *= sizeof(CY); break;
        case PT_MV_APPTIME:  pv.Value.MVat.cValues = exp = 2;
        case PT_APPTIME:     exp *= sizeof(double); break;
        case PT_MV_SYSTIME:  pv.Value.MVft.cValues = exp = 2;
        case PT_SYSTIME:     exp *= sizeof(FILETIME); break;
        case PT_ERROR:       exp = sizeof(SCODE); break;
        case PT_BOOLEAN:     exp = sizeof(USHORT); break;
        case PT_OBJECT:      exp = 0; break;
        case PT_MV_I8:       pv.Value.MVli.cValues = exp = 2;
        case PT_I8:          exp *= sizeof(LONG64); break;
#if 0        
        /* My version of native mapi returns 0 for PT_MV_CLSID even if a valid
         * array is given. This _has_ to be a bug, so Wine does 
         * the right thing(tm) and we don't test it here.
         */
        case PT_MV_CLSID:    pv.Value.MVguid.cValues = exp = 2;
#endif
        case PT_CLSID:       exp *= sizeof(GUID); break;
        case PT_STRING8:
            pv.Value.lpszA = (LPSTR)szHiA;
            exp = 4;
            break;
        case PT_UNICODE:
            pv.Value.lpszW = (LPWSTR)szHiW;
            exp = 4 * sizeof(WCHAR);
            break;
        case PT_BINARY:
            pv.Value.bin.cb = exp = 19;
            break;
        case PT_MV_STRING8:
            pv.Value.MVszA.cValues = 2;
            pv.Value.MVszA.lppszA = buffa;
            buffa[0] = (LPSTR)szHiA;
            buffa[1] = (LPSTR)szHiA;
            exp = 8;
            break;
        case PT_MV_UNICODE:
            pv.Value.MVszW.cValues = 2;
            pv.Value.MVszW.lppszW = buffw;
            buffw[0] = (LPWSTR)szHiW;
            buffw[1] = (LPWSTR)szHiW;
            exp = 8 * sizeof(WCHAR);
            break;
        case PT_MV_BINARY:
            pv.Value.MVbin.cValues = 2;
            pv.Value.MVbin.lpbin = buffbin;
            buffbin[0].cb = 19;
            buffbin[1].cb = 1;
            exp = 20;
            break;
        default:
            exp = 0;
        }

        res = pUlPropSize(&pv);
        ok(res == exp, "pt= %ld: Expected %ld, got %ld\n", pt, exp, res);
    }
}

static void test_FPropContainsProp(void)
{
    static const char *szFull = "Full String";
    static const char *szFullLower = "full string";
    static const char *szPrefix = "Full";
    static const char *szPrefixLower = "full";
    static const char *szSubstring = "ll St";
    static const char *szSubstringLower = "ll st";
    SPropValue pvLeft, pvRight;
    ULONG pt;
    BOOL bRet;

    pFPropContainsProp = (void*)GetProcAddress(hMapi32, "FPropContainsProp@12");

    if (!pFPropContainsProp)
        return;

    /* Ensure that only PT_STRING8 and PT_BINARY are handled */
    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        if (pt == PT_STRING8 || pt == PT_BINARY)
            continue; /* test these later */

        memset(&pvLeft, 0 ,sizeof(pvLeft));
        memset(&pvRight, 0 ,sizeof(pvRight));
        pvLeft.ulPropTag = pvRight.ulPropTag = pt;

        bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
        ok(bRet == FALSE, "pt= %ld: Expected FALSE, got %d\n", pt, bRet);
    }

    /* test the various flag combinations */
    pvLeft.ulPropTag = pvRight.ulPropTag = PT_STRING8;
    pvLeft.Value.lpszA = (LPSTR)szFull;
    pvRight.Value.lpszA = (LPSTR)szFull;

    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == TRUE, "(full,full)[] match failed\n");
    pvRight.Value.lpszA = (LPSTR)szPrefix;
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == FALSE, "(full,prefix)[] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == TRUE, "(full,prefix)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == TRUE, "(full,prefix)[SUBSTRING] match failed\n");
    pvRight.Value.lpszA = (LPSTR)szPrefixLower;
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "(full,prefixlow)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == FALSE, "(full,prefixlow)[SUBSTRING] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX|FL_IGNORECASE);
    ok(bRet == TRUE, "(full,prefixlow)[PREFIX|IGNORECASE] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING|FL_IGNORECASE);
    ok(bRet == TRUE, "(full,prefixlow)[SUBSTRING|IGNORECASE] match failed\n");
    pvRight.Value.lpszA = (LPSTR)szSubstring;
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == FALSE, "(full,substr)[] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "(full,substr)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == TRUE, "(full,substr)[SUBSTRING] match failed\n");
    pvRight.Value.lpszA = (LPSTR)szSubstringLower;
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "(full,substrlow)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == FALSE, "(full,substrlow)[SUBSTRING] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX|FL_IGNORECASE);
    ok(bRet == FALSE, "(full,substrlow)[PREFIX|IGNORECASE] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING|FL_IGNORECASE);
    ok(bRet == TRUE, "(full,substrlow)[SUBSTRING|IGNORECASE] match failed\n");
    pvRight.Value.lpszA = (LPSTR)szFullLower;
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING|FL_IGNORECASE);
    ok(bRet == TRUE, "(full,fulllow)[IGNORECASE] match failed\n");

    pvLeft.ulPropTag = pvRight.ulPropTag = PT_BINARY;
    pvLeft.Value.bin.lpb = (LPBYTE)szFull;
    pvRight.Value.bin.lpb = (LPBYTE)szFull;
    pvLeft.Value.bin.cb = pvRight.Value.bin.cb = strlen(szFull);

    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == TRUE, "bin(full,full)[] match failed\n");
    pvRight.Value.bin.lpb = (LPBYTE)szPrefix;
    pvRight.Value.bin.cb = strlen(szPrefix);
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == FALSE, "bin(full,prefix)[] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == TRUE, "bin(full,prefix)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == TRUE, "bin(full,prefix)[SUBSTRING] match failed\n");
    pvRight.Value.bin.lpb = (LPBYTE)szPrefixLower;
    pvRight.Value.bin.cb = strlen(szPrefixLower);
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "bin(full,prefixlow)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == FALSE, "bin(full,prefixlow)[SUBSTRING] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX|FL_IGNORECASE);
    ok(bRet == FALSE, "bin(full,prefixlow)[PREFIX|IGNORECASE] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING|FL_IGNORECASE);
    ok(bRet == FALSE, "bin(full,prefixlow)[SUBSTRING|IGNORECASE] match failed\n");
    pvRight.Value.bin.lpb = (LPBYTE)szSubstring;
    pvRight.Value.bin.cb = strlen(szSubstring);
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING);
    ok(bRet == FALSE, "bin(full,substr)[] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "bin(full,substr)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == TRUE, "bin(full,substr)[SUBSTRING] match failed\n");
    pvRight.Value.bin.lpb = (LPBYTE)szSubstringLower;
    pvRight.Value.bin.cb = strlen(szSubstringLower);
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX);
    ok(bRet == FALSE, "bin(full,substrlow)[PREFIX] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING);
    ok(bRet == FALSE, "bin(full,substrlow)[SUBSTRING] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_PREFIX|FL_IGNORECASE);
    ok(bRet == FALSE, "bin(full,substrlow)[PREFIX|IGNORECASE] match failed\n");
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_SUBSTRING|FL_IGNORECASE);
    ok(bRet == FALSE, "bin(full,substrlow)[SUBSTRING|IGNORECASE] match failed\n");
    pvRight.Value.bin.lpb = (LPBYTE)szFullLower;
    pvRight.Value.bin.cb = strlen(szFullLower);
    bRet = pFPropContainsProp(&pvLeft, &pvRight, FL_FULLSTRING|FL_IGNORECASE);
    ok(bRet == FALSE, "bin(full,fulllow)[IGNORECASE] match failed\n");
}

typedef struct tagFPropCompareProp_Result
{
    SHORT lVal;
    SHORT rVal;
    ULONG relOp;
    BOOL  bRet;
} FPropCompareProp_Result;

static const FPropCompareProp_Result FPCProp_Results[] =
{
    { 1, 2, RELOP_LT, TRUE },
    { 1, 1, RELOP_LT, FALSE },
    { 2, 1, RELOP_LT, FALSE },
    { 1, 2, RELOP_LE, TRUE },
    { 1, 1, RELOP_LE, TRUE },
    { 2, 1, RELOP_LE, FALSE },
    { 1, 2, RELOP_GT, FALSE },
    { 1, 1, RELOP_GT, FALSE },
    { 2, 1, RELOP_GT, TRUE },
    { 1, 2, RELOP_GE, FALSE },
    { 1, 1, RELOP_GE, TRUE },
    { 2, 1, RELOP_GE, TRUE },
    { 1, 2, RELOP_EQ, FALSE },
    { 1, 1, RELOP_EQ, TRUE },
    { 2, 1, RELOP_EQ, FALSE }
};

static const char *relops[] = { "RELOP_LT", "RELOP_LE", "RELOP_GT", "RELOP_GE", "RELOP_EQ" };

static void test_FPropCompareProp(void)
{
    SPropValue pvLeft, pvRight;
    GUID lguid, rguid;
    char lbuffa[2], rbuffa[2];
    WCHAR lbuffw[2], rbuffw[2];
    ULONG i, j;
    BOOL bRet, bExp;

    pFPropCompareProp = (void*)GetProcAddress(hMapi32, "FPropCompareProp@12");

    if (!pFPropCompareProp)
        return;

    lbuffa[1] = '\0';
    rbuffa[1] = '\0';
    lbuffw[1] = '\0';
    rbuffw[1] = '\0';

    for (i = 0; i < sizeof(ptTypes)/sizeof(ptTypes[0]); i++)
    {
        pvLeft.ulPropTag = pvRight.ulPropTag = ptTypes[i];

        for (j = 0; j < sizeof(FPCProp_Results)/sizeof(FPCProp_Results[0]); j++)
        {
            SHORT lVal = FPCProp_Results[j].lVal;
            SHORT rVal = FPCProp_Results[j].rVal;

            bExp = FPCProp_Results[j].bRet;

            switch (ptTypes[i])
            {
            case PT_BOOLEAN:
                /* Boolean values have no concept of less or greater than, only equality */
                if ((lVal == 1 && rVal == 2 && FPCProp_Results[j].relOp == RELOP_LT) ||
                    (lVal == 2 && rVal == 1 && FPCProp_Results[j].relOp == RELOP_LE)||
                    (lVal == 2 && rVal == 1 && FPCProp_Results[j].relOp == RELOP_GT)||
                    (lVal == 1 && rVal == 2 && FPCProp_Results[j].relOp == RELOP_GE)||
                    (lVal == 1 && rVal == 2 && FPCProp_Results[j].relOp == RELOP_EQ)||
                    (lVal == 2 && rVal == 1 && FPCProp_Results[j].relOp == RELOP_EQ))
                    bExp = !bExp;
                    /* Fall through ... */
            case PT_I2:
                pvLeft.Value.i = lVal;
                pvRight.Value.i = rVal;
                break;
            case PT_ERROR:
            case PT_I4:
                pvLeft.Value.l = lVal;
                pvRight.Value.l = rVal;
                break;
            case PT_R4:
                pvLeft.Value.flt = lVal;
                pvRight.Value.flt = rVal;
                break;
            case PT_APPTIME:
            case PT_R8:
                pvLeft.Value.dbl = lVal;
                pvRight.Value.dbl = rVal;
                break;
            case PT_CURRENCY:
                pvLeft.Value.cur.int64 = lVal;
                pvRight.Value.cur.int64 = rVal;
                break;
            case PT_SYSTIME:
                pvLeft.Value.ft.dwLowDateTime = lVal;
                pvLeft.Value.ft.dwHighDateTime = 0;
                pvRight.Value.ft.dwLowDateTime = rVal;
                pvRight.Value.ft.dwHighDateTime = 0;
                break;
            case PT_I8:
                pvLeft.Value.li.u.LowPart = lVal;
                pvLeft.Value.li.u.HighPart = 0;
                pvRight.Value.li.u.LowPart = rVal;
                pvRight.Value.li.u.HighPart = 0;
                break;
            case PT_CLSID:
                memset(&lguid, 0, sizeof(GUID));
                memset(&rguid, 0, sizeof(GUID));
                lguid.Data4[7] = lVal;
                rguid.Data4[7] = rVal;
                pvLeft.Value.lpguid = &lguid;
                pvRight.Value.lpguid = &rguid;
                break;
            case PT_STRING8:
                pvLeft.Value.lpszA = lbuffa;
                pvRight.Value.lpszA = rbuffa;
                lbuffa[0] = '0' + lVal;
                rbuffa[0] = '0' + rVal;
                break;
            case PT_UNICODE:
                pvLeft.Value.lpszW = lbuffw;
                pvRight.Value.lpszW = rbuffw;
                lbuffw[0] = '0' + lVal;
                rbuffw[0] = '0' + rVal;
                break;
            case PT_BINARY:
                pvLeft.Value.bin.cb = 1;
                pvRight.Value.bin.cb = 1;
                pvLeft.Value.bin.lpb = lbuffa;
                pvRight.Value.bin.lpb = rbuffa;
                lbuffa[0] = lVal;
                rbuffa[0] = rVal;
                break;
            }

            bRet = pFPropCompareProp(&pvLeft, FPCProp_Results[j].relOp, &pvRight);
            ok(bRet == bExp, "pt %ld (%d,%d,%s): expected %d, got %d\n", ptTypes[i],
               FPCProp_Results[j].lVal, FPCProp_Results[j].rVal,
               relops[FPCProp_Results[j].relOp], bExp, bRet);
        }
    }
}

typedef struct tagLPropCompareProp_Result
{
    SHORT lVal;
    SHORT rVal;
    INT   iRet;
} LPropCompareProp_Result;

static const LPropCompareProp_Result LPCProp_Results[] =
{
    { 1, 2, -1 },
    { 1, 1, 0 },
    { 2, 1, 1 },
};

static void test_LPropCompareProp(void)
{
    SPropValue pvLeft, pvRight;
    GUID lguid, rguid;
    char lbuffa[2], rbuffa[2];
    WCHAR lbuffw[2], rbuffw[2];
    ULONG i, j;
    INT iRet, iExp;

    pLPropCompareProp = (void*)GetProcAddress(hMapi32, "LPropCompareProp@8");

    if (!pLPropCompareProp)
        return;

    lbuffa[1] = '\0';
    rbuffa[1] = '\0';
    lbuffw[1] = '\0';
    rbuffw[1] = '\0';

    for (i = 0; i < sizeof(ptTypes)/sizeof(ptTypes[0]); i++)
    {
        pvLeft.ulPropTag = pvRight.ulPropTag = ptTypes[i];

        for (j = 0; j < sizeof(LPCProp_Results)/sizeof(LPCProp_Results[0]); j++)
        {
            SHORT lVal = LPCProp_Results[j].lVal;
            SHORT rVal = LPCProp_Results[j].rVal;

            iExp = LPCProp_Results[j].iRet;

            switch (ptTypes[i])
            {
            case PT_BOOLEAN:
                /* Boolean values have no concept of less or greater than, only equality */
                if (lVal && rVal)
                    iExp = 0;
                    /* Fall through ... */
            case PT_I2:
                pvLeft.Value.i = lVal;
                pvRight.Value.i = rVal;
                break;
            case PT_ERROR:
            case PT_I4:
                pvLeft.Value.l = lVal;
                pvRight.Value.l = rVal;
                break;
            case PT_R4:
                pvLeft.Value.flt = lVal;
                pvRight.Value.flt = rVal;
                break;
            case PT_APPTIME:
            case PT_R8:
                pvLeft.Value.dbl = lVal;
                pvRight.Value.dbl = rVal;
                break;
            case PT_CURRENCY:
                pvLeft.Value.cur.int64 = lVal;
                pvRight.Value.cur.int64 = rVal;
                break;
            case PT_SYSTIME:
                pvLeft.Value.ft.dwLowDateTime = lVal;
                pvLeft.Value.ft.dwHighDateTime = 0;
                pvRight.Value.ft.dwLowDateTime = rVal;
                pvRight.Value.ft.dwHighDateTime = 0;
                break;
            case PT_I8:
                pvLeft.Value.li.u.LowPart = lVal;
                pvLeft.Value.li.u.HighPart = 0;
                pvRight.Value.li.u.LowPart = rVal;
                pvRight.Value.li.u.HighPart = 0;
                break;
            case PT_CLSID:
                memset(&lguid, 0, sizeof(GUID));
                memset(&rguid, 0, sizeof(GUID));
                lguid.Data4[7] = lVal;
                rguid.Data4[7] = rVal;
                pvLeft.Value.lpguid = &lguid;
                pvRight.Value.lpguid = &rguid;
                break;
            case PT_STRING8:
                pvLeft.Value.lpszA = lbuffa;
                pvRight.Value.lpszA = rbuffa;
                lbuffa[0] = '0' + lVal;
                rbuffa[0] = '0' + rVal;
                break;
            case PT_UNICODE:
                pvLeft.Value.lpszW = lbuffw;
                pvRight.Value.lpszW = rbuffw;
                lbuffw[0] = '0' + lVal;
                rbuffw[0] = '0' + rVal;
                break;
            case PT_BINARY:
                pvLeft.Value.bin.cb = 1;
                pvRight.Value.bin.cb = 1;
                pvLeft.Value.bin.lpb = lbuffa;
                pvRight.Value.bin.lpb = rbuffa;
                lbuffa[0] = lVal;
                rbuffa[0] = rVal;
                break;
            }

            iRet = pLPropCompareProp(&pvLeft, &pvRight);
            ok(iRet == iExp, "pt %ld (%d,%d): expected %d, got %d\n", ptTypes[i],
               LPCProp_Results[j].lVal, LPCProp_Results[j].rVal, iExp, iRet);
        }
    }
}

static void test_PpropFindProp(void)
{
    SPropValue pvProp, *pRet;
    ULONG i;

    pPpropFindProp = (void*)GetProcAddress(hMapi32, "PpropFindProp@12");

    if (!pPpropFindProp)
        return;
    
    for (i = 0; i < sizeof(ptTypes)/sizeof(ptTypes[0]); i++)
    {
        pvProp.ulPropTag = ptTypes[i];

        pRet = pPpropFindProp(&pvProp, 1u, ptTypes[i]);
        ok(pRet == &pvProp, "PpropFindProp[%ld]: Didn't find existing propery\n",
           ptTypes[i]);

        pRet = pPpropFindProp(&pvProp, 1u, i ? ptTypes[i-1] : ptTypes[i+1]);
        ok(pRet == NULL, "PpropFindProp[%ld]: Found non-existing propery\n",
           ptTypes[i]);
    }

    pvProp.ulPropTag = PROP_TAG(PT_I2, 1u);
    pRet = pPpropFindProp(&pvProp, 1u, PROP_TAG(PT_UNSPECIFIED, 0u));
    ok(pRet == NULL, "PpropFindProp[UNSPECIFIED]: Matched on different id\n");
    pRet = pPpropFindProp(&pvProp, 1u, PROP_TAG(PT_UNSPECIFIED, 1u));
    ok(pRet == &pvProp, "PpropFindProp[UNSPECIFIED]: Didn't match id\n");
}

static void test_ScCountProps(void)
{
    static const char *szHiA = "Hi!";
    static const WCHAR szHiW[] = { 'H', 'i', '!', '\0' };
    static const ULONG ULHILEN = 4; /* chars in szHiA/W incl. NUL */
    LPSTR  buffa[3];
    LPWSTR buffw[3];
    SBinary buffbin[3];
    GUID iids[4], *iid = iids;
    SCODE res;
    ULONG pt, exp, ulRet;

    pScCountProps = (void*)GetProcAddress(hMapi32, "ScCountProps@12");

    if (!pScCountProps)
        return;

    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        SPropValue pv;

        memset(&pv, 0 ,sizeof(pv));
        pv.ulPropTag = PROP_TAG(pt, 1u);

        switch (PROP_TYPE(pt))
        {
        case PT_I2:       
        case PT_I4:     
        case PT_R4:       
        case PT_R8:   
        case PT_CURRENCY: 
        case PT_APPTIME:  
        case PT_SYSTIME:  
        case PT_ERROR:    
        case PT_BOOLEAN:  
        case PT_OBJECT:   
        case PT_I8:       
            exp = sizeof(pv);
            break;
        case PT_CLSID:
            pv.Value.lpguid = iid;
            exp = sizeof(GUID) + sizeof(pv);
            break;
        case PT_STRING8:
            pv.Value.lpszA = (LPSTR)szHiA;
            exp = 4 + sizeof(pv);
            break;
        case PT_UNICODE:
            pv.Value.lpszW = (LPWSTR)szHiW;
            exp = 4 * sizeof(WCHAR) + sizeof(pv);
            break;
        case PT_BINARY:
            pv.Value.bin.cb = 2;
            pv.Value.bin.lpb = (LPBYTE)iid;
            exp = 2 + sizeof(pv);
            break;
        case PT_MV_I2:
            pv.Value.MVi.cValues = 3;
            pv.Value.MVi.lpi = (SHORT*)iid;
            exp = 3 * sizeof(SHORT) + sizeof(pv);
            break;
        case PT_MV_I4:
            pv.Value.MVl.cValues = 3;
            pv.Value.MVl.lpl = (LONG*)iid;
            exp = 3 * sizeof(LONG) + sizeof(pv);
            break;
        case PT_MV_I8:
            pv.Value.MVli.cValues = 3;
            pv.Value.MVli.lpli = (LARGE_INTEGER*)iid;
            exp = 3 * sizeof(LARGE_INTEGER) + sizeof(pv);
            break;
        case PT_MV_R4:
            pv.Value.MVflt.cValues = 3;
            pv.Value.MVflt.lpflt = (float*)iid;
            exp = 3 * sizeof(float) + sizeof(pv);
            break;
        case PT_MV_APPTIME:
        case PT_MV_R8:
            pv.Value.MVdbl.cValues = 3;
            pv.Value.MVdbl.lpdbl = (double*)iid;
            exp = 3 * sizeof(double) + sizeof(pv);
            break;
        case PT_MV_CURRENCY:
            pv.Value.MVcur.cValues = 3;
            pv.Value.MVcur.lpcur = (CY*)iid;
            exp = 3 * sizeof(CY) + sizeof(pv);
            break;
        case PT_MV_SYSTIME:
            pv.Value.MVft.cValues = 3;
            pv.Value.MVft.lpft = (FILETIME*)iid;
            exp = 3 * sizeof(CY) + sizeof(pv);
            break;
            break;
        case PT_MV_STRING8:
            pv.Value.MVszA.cValues = 3;
            pv.Value.MVszA.lppszA = buffa;
            buffa[0] = (LPSTR)szHiA;
            buffa[1] = (LPSTR)szHiA;
            buffa[2] = (LPSTR)szHiA;
            exp = ULHILEN * 3 + 3 * sizeof(char*) + sizeof(pv);
            break;
        case PT_MV_UNICODE:
            pv.Value.MVszW.cValues = 3;
            pv.Value.MVszW.lppszW = buffw;
            buffw[0] = (LPWSTR)szHiW;
            buffw[1] = (LPWSTR)szHiW;
            buffw[2] = (LPWSTR)szHiW;
            exp = ULHILEN * 3 * sizeof(WCHAR) + 3 * sizeof(WCHAR*) + sizeof(pv);
            break;
        case PT_MV_BINARY:
            pv.Value.MVbin.cValues = 3;
            pv.Value.MVbin.lpbin = buffbin;
            buffbin[0].cb = 17;
            buffbin[0].lpb = (LPBYTE)&iid;
            buffbin[1].cb = 2;
            buffbin[1].lpb = (LPBYTE)&iid;
            buffbin[2].cb = 1;
            buffbin[2].lpb = (LPBYTE)&iid;
            exp = 20 + sizeof(pv) + sizeof(SBinary) * 3;
            break;
        default:
            exp = 0;
        }

        ulRet = 0xffffffff;
        res = pScCountProps(1, &pv, &ulRet);
        if (!exp)
            ok(res == MAPI_E_INVALID_PARAMETER && ulRet == 0xffffffff,
               "pt= %ld: Expected failure, got %ld, ret=0x%08lX\n", pt, ulRet, res);
        else
            ok(res == S_OK && ulRet == exp, "pt= %ld: Expected %ld, got %ld, ret=0x%08lX\n", 
               pt, exp, ulRet, res);
    }

}

static void test_ScCopyRelocProps(void)
{
    static const char* szTestA = "Test";
    char buffer[512], buffer2[512], *lppszA[1];
    SPropValue pvProp, *lpResProp = (LPSPropValue)buffer;
    ULONG ulCount;
    SCODE sc;
       
    pScCopyProps = (void*)GetProcAddress(hMapi32, "ScCopyProps@16");
    pScRelocProps = (void*)GetProcAddress(hMapi32, "ScRelocProps@20");

    if (!pScCopyProps || !pScRelocProps)
        return;

    pvProp.ulPropTag = PROP_TAG(PT_MV_STRING8, 1u);
        
    lppszA[0] = (char *)szTestA;
    pvProp.Value.MVszA.cValues = 1;
    pvProp.Value.MVszA.lppszA = lppszA;
    ulCount = 0;
    
    sc = pScCopyProps(1, &pvProp, buffer, &ulCount);    
    ok(sc == S_OK && lpResProp->ulPropTag == pvProp.ulPropTag &&
       lpResProp->Value.MVszA.cValues == 1 && 
       lpResProp->Value.MVszA.lppszA[0] == buffer + sizeof(SPropValue) + sizeof(char*) &&
       !strcmp(lpResProp->Value.MVszA.lppszA[0], szTestA) &&
       ulCount == sizeof(SPropValue) + sizeof(char*) + 5,
       "CopyProps(str): Expected 0 {1,%lx,%p,%s} %d got 0x%08lx {%ld,%lx,%p,%s} %ld\n",
       pvProp.ulPropTag, buffer + sizeof(SPropValue) + sizeof(char*),
       szTestA, sizeof(SPropValue) + sizeof(char*) + 5, sc, 
       lpResProp->Value.MVszA.cValues, lpResProp->ulPropTag,
       lpResProp->Value.MVszA.lppszA[0], lpResProp->Value.MVszA.lppszA[0], ulCount);

    memcpy(buffer2, buffer, sizeof(buffer));
    
    /* Clear the data in the source buffer. Since pointers in the copied buffer
     * refer to the source buffer, this proves that native always assumes that
     * the copied buffers pointers are bad (needing to be relocated first).
     */
    memset(buffer, 0, sizeof(buffer));
    ulCount = 0;
       
    sc = pScRelocProps(1, (LPSPropValue)buffer2, buffer, buffer2, &ulCount);
    lpResProp = (LPSPropValue)buffer2;
    ok(sc == S_OK && lpResProp->ulPropTag == pvProp.ulPropTag &&
       lpResProp->Value.MVszA.cValues == 1 && 
       lpResProp->Value.MVszA.lppszA[0] == buffer2 + sizeof(SPropValue) + sizeof(char*) &&
       !strcmp(lpResProp->Value.MVszA.lppszA[0], szTestA) &&
       /* Native has a bug whereby it calculates the size correctly when copying
        * but when relocating does not (presumably it uses UlPropSize() which
        * ignores multivalue pointers). Wine returns the correct value.
        */
       (ulCount == sizeof(SPropValue) + sizeof(char*) + 5 || ulCount == sizeof(SPropValue) + 5),
       "RelocProps(str): Expected 0 {1,%lx,%p,%s} %d got 0x%08lx {%ld,%lx,%p,%s} %ld\n",
       pvProp.ulPropTag, buffer2 + sizeof(SPropValue) + sizeof(char*),
       szTestA, sizeof(SPropValue) + sizeof(char*) + 5, sc, 
       lpResProp->Value.MVszA.cValues, lpResProp->ulPropTag,
       lpResProp->Value.MVszA.lppszA[0], lpResProp->Value.MVszA.lppszA[0], ulCount);

    /* Native crashes with lpNew or lpOld set to NULL so skip testing this */   
}

static void test_LpValFindProp(void)
{
    SPropValue pvProp, *pRet;
    ULONG i;

    pLpValFindProp = (void*)GetProcAddress(hMapi32, "LpValFindProp@12");

    if (!pLpValFindProp)
        return;
    
    for (i = 0; i < sizeof(ptTypes)/sizeof(ptTypes[0]); i++)
    {
        pvProp.ulPropTag = PROP_TAG(ptTypes[i], 1u);

        pRet = pLpValFindProp(PROP_TAG(ptTypes[i], 1u), 1u, &pvProp);
        ok(pRet == &pvProp, "LpValFindProp[%ld]: Didn't find existing propery id/type\n",
           ptTypes[i]);

        pRet = pLpValFindProp(PROP_TAG(ptTypes[i], 0u), 1u, &pvProp);
        ok(pRet == NULL, "LpValFindProp[%ld]: Found non-existing propery id\n",
           ptTypes[i]);
           
        pRet = pLpValFindProp(PROP_TAG(PT_NULL, 0u), 1u, &pvProp);
        ok(pRet == NULL, "LpValFindProp[%ld]: Found non-existing propery id/type\n",
           ptTypes[i]);
        
        pRet = pLpValFindProp(PROP_TAG(PT_NULL, 1u), 1u, &pvProp);
        ok(pRet == &pvProp, "LpValFindProp[%ld]: Didn't find existing propery id\n",
           ptTypes[i]);
    }
}

static void test_FBadRglpszA(void)
{
    LPSTR lpStrs[4];
    char *szString = "A String";
    BOOL bRet;
    
    pFBadRglpszA = (void*)GetProcAddress(hMapi32, "FBadRglpszA@8");
    if (!pFBadRglpszA)
        return;
    
    bRet = pFBadRglpszA(NULL, 10);
    ok(bRet == TRUE, "FBadRglpszA(Null): expected TRUE, got FALSE\n"); 
    
    lpStrs[0] = lpStrs[1] = lpStrs[2] = lpStrs[3] = NULL;
    bRet = pFBadRglpszA(lpStrs, 4);
    ok(bRet == TRUE, "FBadRglpszA(Nulls): expected TRUE, got FALSE\n"); 

    lpStrs[0] = lpStrs[1] = lpStrs[2] = szString;
    bRet = pFBadRglpszA(lpStrs, 3);
    ok(bRet == FALSE, "FBadRglpszA(valid): expected FALSE, got TRUE\n"); 
    
    bRet = pFBadRglpszA(lpStrs, 4);
    ok(bRet == TRUE, "FBadRglpszA(1 invalid): expected TRUE, got FALSE\n"); 
}

static void test_FBadRglpszW(void)
{
    LPWSTR lpStrs[4];
    WCHAR szString[] = { 'A',' ','S','t','r','i','n','g','\0' };
    BOOL bRet;
    
    pFBadRglpszW = (void*)GetProcAddress(hMapi32, "FBadRglpszW@8");
    if (!pFBadRglpszW)
        return;
    
    bRet = pFBadRglpszW(NULL, 10);
    ok(bRet == TRUE, "FBadRglpszW(Null): expected TRUE, got FALSE\n"); 
    
    lpStrs[0] = lpStrs[1] = lpStrs[2] = lpStrs[3] = NULL;
    bRet = pFBadRglpszW(lpStrs, 4);
    ok(bRet == TRUE, "FBadRglpszW(Nulls): expected TRUE, got FALSE\n"); 

    lpStrs[0] = lpStrs[1] = lpStrs[2] = szString;
    bRet = pFBadRglpszW(lpStrs, 3);
    ok(bRet == FALSE, "FBadRglpszW(valid): expected FALSE, got TRUE\n"); 
    
    bRet = pFBadRglpszW(lpStrs, 4);
    ok(bRet == TRUE, "FBadRglpszW(1 invalid): expected TRUE, got FALSE\n"); 
}

static void test_FBadRowSet(void)
{
    ULONG ulRet;
    
    pFBadRowSet = (void*)GetProcAddress(hMapi32, "FBadRowSet@4");
    if (!pFBadRowSet)
        return;
    
    ulRet = pFBadRowSet(NULL);
    ok(ulRet != 0, "FBadRow(null): Expected non-zero, got 0\n");
    
    /* FIXME */
}

static void test_FBadPropTag(void)
{
    ULONG pt, res;

    pFBadPropTag = (void*)GetProcAddress(hMapi32, "FBadPropTag@4");
    if (!pFBadPropTag)
        return;

    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        BOOL bBad = TRUE;

        switch (pt & (~MV_FLAG & PROP_TYPE_MASK))
        {
        case PT_UNSPECIFIED:
        case PT_NULL: case PT_I2: case PT_I4: case PT_R4:
        case PT_R8: case PT_CURRENCY: case PT_APPTIME:
        case PT_ERROR: case PT_BOOLEAN: case PT_OBJECT:
        case PT_I8: case PT_STRING8: case PT_UNICODE:
        case PT_SYSTIME: case PT_CLSID: case PT_BINARY:
            bBad = FALSE;
        }

        res = pFBadPropTag(pt);
        if (bBad)
            ok(res != 0, "pt= %ld: Expected non-zero, got 0\n", pt);
        else
            ok(res == 0, "pt= %ld: Expected zero, got %ld\n", pt, res);
    }
}

static void test_FBadRow(void)
{
    ULONG ulRet;
    
    pFBadRow = (void*)GetProcAddress(hMapi32, "FBadRow@4");
    if (!pFBadRow)
        return;
    
    ulRet = pFBadRow(NULL);
    ok(ulRet != 0, "FBadRow(null): Expected non-zero, got 0\n");

    /* FIXME */
}

static void test_FBadProp(void)
{
    WCHAR szEmpty[] = { '\0' };
    GUID iid;
    ULONG pt, res;
    SPropValue pv;
    
    pFBadProp = (void*)GetProcAddress(hMapi32, "FBadProp@4");
    if (!pFBadProp)
        return;

    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        BOOL bBad = TRUE;

        memset(&pv, 0, sizeof(pv));
        pv.ulPropTag = pt;

        /* Note that MV values are valid below because their array count is 0,
         * so no pointers are validated.
         */        
        switch (PROP_TYPE(pt))
        {
        case (MV_FLAG|PT_UNSPECIFIED):
        case PT_UNSPECIFIED:
        case (MV_FLAG|PT_NULL): 
        case PT_NULL: 
        case PT_MV_I2:
        case PT_I2:
        case PT_MV_I4:
        case PT_I4:
        case PT_MV_I8:
        case PT_I8:
        case PT_MV_R4:
        case PT_R4:
        case PT_MV_R8:
        case PT_R8:
        case PT_MV_CURRENCY:
        case PT_CURRENCY:
        case PT_MV_APPTIME:
        case PT_APPTIME:
        case (MV_FLAG|PT_ERROR):
        case PT_ERROR:
        case (MV_FLAG|PT_BOOLEAN):
        case PT_BOOLEAN:
        case (MV_FLAG|PT_OBJECT):
        case PT_OBJECT:
        case PT_MV_STRING8:
        case PT_MV_UNICODE:
        case PT_MV_SYSTIME:
        case PT_SYSTIME:
        case PT_MV_BINARY:
        case PT_BINARY:
        case PT_MV_CLSID:
            bBad = FALSE;
            break;
        case PT_STRING8:
        case PT_UNICODE:
            pv.Value.lpszW = szEmpty;
            bBad = FALSE;
            break;
        case PT_CLSID:
            pv.Value.lpguid = &iid;
            bBad = FALSE;
            break;
        }

        res = pFBadProp(&pv);
        if (bBad)
            ok(res != 0, "pt= %ld: Expected non-zero, got 0\n", pt);
        else
            ok(res == 0, "pt= %ld: Expected zero, got %ld\n", pt, res);
    }
}

static void test_FBadColumnSet(void)
{
    SPropTagArray pta;
    ULONG pt, res;

    pFBadColumnSet = (void*)GetProcAddress(hMapi32, "FBadColumnSet@4");
    if (!pFBadColumnSet)
        return;

    res = pFBadColumnSet(NULL);
    ok(res != 0, "(null): Expected non-zero, got 0\n");

    pta.cValues = 1;

    for (pt = 0; pt < PROP_ID_INVALID; pt++)
    {
        BOOL bBad = TRUE;

        pta.aulPropTag[0] = pt;

        switch (pt & (~MV_FLAG & PROP_TYPE_MASK))
        {
        case PT_UNSPECIFIED:
        case PT_NULL:
        case PT_I2:
        case PT_I4:
        case PT_R4:
        case PT_R8:
        case PT_CURRENCY:
        case PT_APPTIME:
        case PT_BOOLEAN:
        case PT_OBJECT:
        case PT_I8:
        case PT_STRING8:
        case PT_UNICODE:
        case PT_SYSTIME:
        case PT_CLSID:
        case PT_BINARY:
            bBad = FALSE;
        }
        if (pt == (MV_FLAG|PT_ERROR))
            bBad = FALSE;

        res = pFBadColumnSet(&pta);
        if (bBad)
            ok(res != 0, "pt= %ld: Expected non-zero, got 0\n", pt);
        else
            ok(res == 0, "pt= %ld: Expected zero, got %ld\n", pt, res);
    }
}

START_TEST(prop)
{  
    hMapi32 = LoadLibraryA("mapi32.dll");
    
    pScInitMapiUtil = (void*)GetProcAddress(hMapi32, "ScInitMapiUtil@4");
    if (!pScInitMapiUtil)
        return;
    pScInitMapiUtil(0);

    test_PropCopyMore();
    test_UlPropSize();
    test_FPropContainsProp();
    test_FPropCompareProp();
    test_LPropCompareProp();
    test_PpropFindProp();
    test_ScCountProps();
    test_ScCopyRelocProps();
    test_LpValFindProp();
    test_FBadRglpszA();
    test_FBadRglpszW();
    test_FBadRowSet();
    test_FBadPropTag();
    test_FBadRow();
    test_FBadProp();
    test_FBadColumnSet();
}
