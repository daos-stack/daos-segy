/*
 * su_helpers.c
 *
 *  Created on: Jun 29, 2020
 *      Author: mirnamoawad
 */

#include "su_helpers.h"

/*********************** self documentation **********************/
char *sdoc[] = {
        "									",
        " SEGYREAD - read an SEG-Y tape						",
            "									",
        NULL};

void ibm_to_float(int from[], int to[], int n, int endian, int verbose)
/***********************************************************************
ibm_to_float - convert between 32 bit IBM and IEEE floating numbers
************************************************************************
Input::
from		input vector
to		output vector, can be same as input vector
endian		byte order =0 little endian (DEC, PC's)
			    =1 other systems
*************************************************************************
Notes:
Up to 3 bits lost on IEEE -> IBM

Assumes sizeof(int) == sizeof(float) == 4

IBM -> IEEE may overflow or underflow, taken care of by
substituting large number or zero

*************************************************************************
Credits: SEP: Stewart A. Levin,  c.1995
*************************************************************************/

/* See if this fits the bill for your needs - Stew */
/* ibmflt.f -- translated by f2c (version 1995/10/25).
*/
/* Subroutine */
{
    /* Initialized data */

    static int first = 1;

    /* System generated locals */
    int i__1;
    int j,k;

    /* Local variables */
    int   *in;
    float *out;
    int eibm, i__, mhibm;
    static int m1[512];
    static float r1[512];

    unsigned int jj;
    union {
        float rrf;
        int iif;
        unsigned int uuf;
    } cvtmp;

    float r_infinity__;
    int et3e;

    if(endian == 0) {
        swab(from,to,n*sizeof(int));
        for(i__ = 0; i__<n; ++i__) {
            j = to[i__];
            k = j<<16;
            to[i__] = k+((j>>16)&65535);
        }
        in = to;
    } else {
        in = from;
    }
    /* Parameter adjustments */
    out = (float *) to;
    --out;
    --in;
    /* Function Body */

    if (first) {
        first = ! first;
        cvtmp.iif = 2139095039;
        r_infinity__ = cvtmp.rrf;
        for (i__ = 0; i__ <= 511; ++i__) {
            i__1 = i__ & 255;
            eibm = i__1 >> 1;
            mhibm = i__ & 1;
            et3e = (eibm << 2) - 130;
            if (et3e > 0 && et3e <= 255) {
                i__1 = et3e ^ (i__ & 255);
                m1[i__] = i__1 << 23;
                if (mhibm == 1) {
                    r1[i__] = 0.f;
                } else {
                    i__1 = et3e | (i__ & 256);
                    cvtmp.iif = i__1 << 23;
                    r1[i__] = -(cvtmp.rrf);
                }
            } else if (et3e <= 0) {
                m1[i__] = i__ << 23;
                r1[i__] = 0.f;
            } else {
                m1[i__] = i__ << 23;
                if (i__ < 256) {
                    r1[i__] = r_infinity__;
                } else {
                    r1[i__] = -r_infinity__;
                }
            }
/* L10: */
        }
    }

    for (i__ = 1; i__ <= n; ++i__) {
        cvtmp.iif = in[i__];
/* use 9 high bits for table lookup */
        jj = cvtmp.uuf>>23;
/* fix up exponent */
        cvtmp.iif = m1[jj] ^ cvtmp.iif;
/* fix up mantissa */
        out[i__] = cvtmp.rrf + r1[jj];
/* L20: */
    }
}

void tapebhed_to_bhed(const tapebhed *tapebhptr, bhed *bhptr)
/****************************************************************************
tapebhed_to_bhed -- converts the seg-y standard 2 byte and 4 byte
	integer header fields to, respectively, the
	machine's short and int types.
*****************************************************************************
Input:
tapbhed		pointer to array of
*****************************************************************************
Notes:
The present implementation assumes that these types are actually the "right"
size (respectively 2 and 4 bytes), so this routine is only a placeholder for
the conversions that would be needed on a machine not using this convention.
*****************************************************************************
Author: CWP: Jack  K. Cohen, August 1994
****************************************************************************/

{
    register int i;
    Value val;

    /* convert binary header, field by field */
    for (i = 0; i < BHED_NKEYS; ++i) {
        gettapebhval(tapebhptr, i, &val);
        putbhval(bhptr, i, &val);
    }
}

void tapesegy_to_segy(const tapesegy *tapetrptr, segy *trptr)
/****************************************************************************
tapesegy_to_segy -- converts the seg-y standard 2 byte and 4 byte
		    integer header fields to, respectively, the machine's
		    short and int types.
*****************************************************************************
Input:
tapetrptr	pointer to trace in "tapesegy" (SEG-Y on tape) format

Output:
trptr		pointer to trace in "segy" (SEG-Y as in	 SU) format
*****************************************************************************
Notes:
Also copies float data byte by byte.  The present implementation assumes that
the integer types are actually the "right" size (respectively 2 and 4 bytes),
so this routine is only a placeholder for the conversions that would be needed
on a machine not using this convention.	 The float data is preserved as
four byte fields and is later converted to internal floats by ibm_to_float
(which, in turn, makes additonal assumptions).
*****************************************************************************
Author: CWP:Jack K. Cohen,  August 1994
****************************************************************************/
{
    register int i;
    Value val;

    /* convert header trace header fields */
    for (i = 0; i < SEGY_NKEYS; ++i) {
        gettapehval(tapetrptr, i, &val);
        puthval(trptr, i, &val);
    }

    /* copy the optional portion */
    memcpy((char *)&(trptr->otrav) + 2, tapetrptr->unass, 60);

    /* copy data portion */
    memcpy(trptr->data, tapetrptr->data, 4 * SU_NFLTS);
}

void int_to_float(int from[], float to[], int n, int endian)
/****************************************************************************
Author:	J.W. de Bruijn, May 1995
****************************************************************************/
{
    register int i;

    if (endian == 0) {
        for (i = 0; i < n; ++i) {
            swap_int_4(&from[i]);
            to[i] = (float) from[i];
        }
    } else {
        for (i = 0; i < n; ++i) {
            to[i] = (float) from[i];
        }
    }
}

void short_to_float(short from[], float to[], int n, int endian)
/****************************************************************************
short_to_float - type conversion for additional SEG-Y formats
*****************************************************************************
Author: Delft: J.W. de Bruijn, May 1995
Modified by: Baltic Sea Reasearch Institute: Toralf Foerster, March 1997
****************************************************************************/
{
    register int i;

    if (endian == 0) {
        for (i = n - 1; i >= 0 ; --i) {
            swap_short_2(&from[i]);
            to[i] = (float) from[i];
        }
    } else {
        for (i = n - 1; i >= 0 ; --i)
            to[i] = (float) from[i];
    }
}

void integer1_to_float(signed char from[], float to[], int n)
/****************************************************************************
integer1_to_float - type conversion for additional SEG-Y formats
*****************************************************************************
Author: John Stockwell,  2005
****************************************************************************/
{
    while (n--) {
        to[n] = from[n];
    }
}

void ugethval(cwp_String type1, Value *valp1,
              char type2, int ubyte,
              char *ptr2, int endian, int conv, int verbose)
{	double dval1 = 0;
    char   c = 0;
    short  s = 0;
    int    l = 0;
    float  f = 0.0;
    char	*ptr1;

#if 0
    fprintf(stderr, "start ugethval %d %c\n", ubyte, type2);
#endif

    switch (type2) {
        case 'b':
            ptr1 = (char*) &c;
            ptr1[0] = ptr2[ubyte];
            dval1 = (double) c;
            break;
        case 's':
            ptr1 = (char*) &s;
            ptr1[0] = ptr2[ubyte];
            ptr1[1] = ptr2[ubyte+1];
            if (endian == 0)
                swap_short_2(&s);
            dval1 = (double) s;
            break;
        case 'l':
            ptr1 = (char*) &l;
            ptr1[0] = ptr2[ubyte];
            ptr1[1] = ptr2[ubyte+1];
            ptr1[2] = ptr2[ubyte+2];
            ptr1[3] = ptr2[ubyte+3];
            if (endian == 0)
/* segyread.c:903: warning: dereferencing type-punned pointer will break strict-aliasing rules */
/* 		   swap_long_4((long *)&l); */
/* note: long is 64-bits on 64-bit machine! */
                swap_int_4((int *)&l);
            dval1 = (double) l;
            break;
        case 'f':
            ptr1 = (char*) &f;
            ptr1[0] = ptr2[ubyte];
            ptr1[1] = ptr2[ubyte+1];
            ptr1[2] = ptr2[ubyte+2];
            ptr1[3] = ptr2[ubyte+3];
            if (conv)
/* get this message twice */
/* segyread.c:913: warning: dereferencing type-punned pointer will break strict-aliasing rules */
/* 		   ibm_to_float((int*) &f, (int*) &f, 1, endian, verbose); */
            {
                memcpy (&l, &f, 4);
                ibm_to_float(&l, &l, 1, endian, verbose);
            }
            else if (conv == 0 && endian == 0)
                swap_float_4(&f);
            dval1 = (double) f;
            break;
        default:
            err("unknown type %s", type2);
            break;
    }

#if 0
    fprintf(stderr, "value %lf\n", dval1);
#endif

    switch (*type1) {
        case 's':
            err("can't change char header word");
            break;
        case 'h':
            valp1->h = (short) dval1;
            break;
        case 'u':
            valp1->u = (unsigned short) dval1;
            break;
        case 'l':
            valp1->l = (long) dval1;
            break;
        case 'v':
            valp1->v = (unsigned long) dval1;
            break;
        case 'i':
            valp1->i = (int) dval1;
            break;
        case 'p':
            valp1->p = (unsigned int) dval1;
            break;
        case 'f':
            valp1->f = (float) dval1;
            break;
        case 'd':
            valp1->d = (double) dval1;
            break;
        default:
            err("unknown type %s", type1);
            break;
    }
}

//char *hdtype(const char *key)
//{
//	int index = getindex(key);
//
//	if (-1 == (index))
//		err("%s: key word not in segy.h: '%s'", __FILE__, key);
//
//	return hdr[index].type;
//}
//
//int getindex(const char *key)	/* get index for this key */
//{
//	register int i;
//
//	for (i = 0; i < SU_NKEYS; i++)
//		if (STREQ(hdr[i].key, key))
//			return i;	/* key found */
//
//	/* not found */
//	return -1;
//}

void setval( cwp_String type, Value *valp, double a, double b,
		double c, double i, double j)
{
	switch (*type) {
	case 's':
		err("can't set char header word");
	break;
	case 'h':
		valp->h = a + b * mod(i, j) + c * ((int) (i/j));
	break;
	case 'u':
		valp->u = (unsigned short)(a + b * mod(i, j) + c * ((int) (i/j)));
	break;
	case 'l':
		valp->l = (long) (a + b * mod(i, j) + c * ((int) (i/j)));
	break;
	case 'v':
		valp->v = (unsigned long) (a + b * mod(i, j) + c * ((int) (i/j)));
	break;
	case 'i':
		valp->i = a + b * mod(i, j) + c * ((int) (i/j));
	break;
	case 'p':
		valp->p = a + b * mod(i, j) + c * ((int) (i/j));
	break;
	case 'f':
		valp->f = a + b * mod(i, j) + c * ((int) (i/j));
	break;
	case 'd':
		valp->d = a + b * mod(i, j) + c * ((int) (i/j));
	break;
	default:
		err("unknown type %s", type);
	break;
	}
	return;
}

double mod(double x, double y)	/* As defined in Knuth, vol. 1	*/
{
	return y == 0.0 ? x : x - y * floor(x/y);
}


void changeval(cwp_String type1, Value *valp1, cwp_String type2,
	       Value *valp2, cwp_String type3, Value *valp3,
		double a, double b, double c, double d, double e, double f)
{
	double dval2=vtod( type2, *valp2);
	double dval3=vtod( type3, *valp3);
	double dval1=(a+b*pow(dval2,e)+c*pow(dval3,f))/d;

	switch (*type1) {
	case 's':
		err("can't change char header word");
	break;
	case 'h':
		valp1->h = (short) dval1;
	break;
	case 'u':
		valp1->u = (unsigned short) dval1;
	break;
	case 'l':
		valp1->l = (long) dval1;
	break;
	case 'v':
		valp1->v = (unsigned long) dval1;
	break;
	case 'i':
		valp1->i = (int) dval1;
	break;
	case 'p':
		valp1->p = (unsigned int) dval1;
	break;
	case 'f':
		valp1->f = (float) dval1;
	break;
	case 'd':
		valp1->d = (double) dval1;
	break;
	default:
		err("unknown type %s", type1);
	break;
	}
}

//int valcmp_(register cwp_String type, Value val1, Value val2)
//{
//	switch(*type) {
//		case 's': return strcmp(val1.s, val2.s);
//		case 'h':
//			if      ( val1.h <= val2.h ) return -1;
//			else if ( val1.h >= val2.h ) return  1;
//			else                        return  0;
//		case 'u':
//			if      ( val1.u <= val2.u ) return -1;
//			else if ( val1.u >= val2.u ) return  1;
//			else                        return  0;
//		case 'l':
//			if      ( val1.l <= val2.l ) return -1;
//			else if ( val1.l >= val2.l ) return  1;
//			else                        return  0;
//		case 'v':
//			if      ( val1.v <= val2.v ) return -1;
//			else if ( val1.v >= val2.v ) return  1;
//			else                        return  0;
//		case 'i':
//			if      ( val1.i <= val2.i ) return -1;
//			else if ( val1.i >= val2.i ) return  1;
//			else                        return  0;
//		case 'p':
//			if      ( val1.p <= val2.p ) return -1;
//			else if ( val1.p >= val2.p ) return  1;
//			else                        return  0;
//		case 'f':
//			if      ( val1.f <= val2.f ) return -1;
//			else if ( val1.f >= val2.f ) return  1;
//			else                        return  0;
//		case 'd':
//			if      ( val1.d <= val2.d ) return -1;
//			else if ( val1.d >= val2.d ) return  1;
//			else                        return  0;
//		case 'U':
//			if      ( val1.U <= val2.U ) return -1;
//			else if ( val1.U >= val2.U ) return  1;
//			else                        return  0;
//		case 'P':
//			if      ( val1.P <= val2.P ) return -1;
//			else if ( val1.P >= val2.P ) return  1;
//			else                        return  0;
//		default: err("valcmp: unknown type %s", type);
//			 return 0;	/* for lint */
//	}
//}
//

void seis_gethval(trace_t *tr, int index, Value *valp)
{
	char *tp = (char*) tr;

	switch(*(hdr[index].type)) {
	case 's': (void) strcpy(valp->s, tp + hdr[index].offs);  break;
	case 'h': valp->h = *((short*)  (tp + hdr[index].offs)); break;
	case 'u': valp->u = *((unsigned short*) (tp + hdr[index].offs)); break;
	case 'i': valp->i = *((int*)   (tp + hdr[index].offs)); break;
	case 'p': valp->p = *((unsigned int*)   (tp + hdr[index].offs)); break;
	case 'l': valp->l = *((long*)   (tp + hdr[index].offs)); break;
	case 'v': valp->v = *((unsigned long*)  (tp + hdr[index].offs)); break;
	case 'f': valp->f = *((float*)  (tp + hdr[index].offs)); break;
	case 'd': valp->d = *((double*) (tp + hdr[index].offs)); break;
	default: err("%s: %s: mysterious data type", __FILE__,__LINE__); break;
	}

	return;
}

void seis_puthval(trace_t *tr, int index, Value *valp)
{
	char *tp = (char*) tr;

	switch(*(hdr[index].type)) {
	case 's': (void) strcpy(tp + hdr[index].offs, valp->s);  break;
	case 'h': *((short*)  (tp + hdr[index].offs)) = valp->h; break;
	case 'u': *((unsigned short*) (tp + hdr[index].offs)) = valp->u; break;
	case 'i': *((int*)   (tp + hdr[index].offs)) = valp->i; break;
	case 'p': *((unsigned int*)   (tp + hdr[index].offs)) = valp->p; break;
	case 'l': *((long*)   (tp + hdr[index].offs)) = valp->l; break;
	case 'v': *((unsigned long*)  (tp + hdr[index].offs)) = valp->v; break;
	case 'f': *((float*)  (tp + hdr[index].offs)) = valp->f; break;
	case 'd': *((double*) (tp + hdr[index].offs)) = valp->d; break;
	default: err("%s: %s: mysterious data type", __FILE__,__LINE__); break;
	}

	return;
}


