/*
 * su_helpers.h
 *
 *  Created on: Jun 29, 2020
 *      Author: mirnamoawad
 */

#ifndef LSU_SRC_CLIENT_SEIS_SU_HELPERS_H_
#define LSU_SRC_CLIENT_SEIS_SU_HELPERS_H_


#include <stdio.h>
#include <sys/types.h>
#include "su.h"
#include "segy.h"
#include "tapesegy.h"
#include "tapebhdr.h"
#include "bheader.h"
#include "header.h"
#include "daos_seis_datatypes.h"
/* TYPEDEFS */


static struct {
	char *key;	char *type;	int offs;
} hdr[] = {
	{   "tracl",		"i",		0},
	{   "tracr",		"i",		4},
	{    "fldr",		"i",		8},
	{   "tracf",		"i",		12},
	{      "ep",		"i",		16},
	{     "cdp",		"i",		20},
	{    "cdpt",		"i",		24},
	{    "trid",		"h",		28},
	{     "nvs",		"h",		30},
	{     "nhs",		"h",		32},
	{    "duse",		"h",		34},
	{  "offset",		"i",		36},
	{   "gelev",		"i",		40},
	{   "selev",		"i",		44},
	{  "sdepth",		"i",		48},
	{    "gdel",		"i",		52},
	{    "sdel",		"i",		56},
	{   "swdep",		"i",		60},
	{   "gwdep",		"i",		64},
	{  "scalel",		"h",		68},
	{  "scalco",		"h",		70},
	{      "sx",		"i",		72},
	{      "sy",		"i",		76},
	{      "gx",		"i",		80},
	{      "gy",		"i",		84},
	{  "counit",		"h",		88},
	{   "wevel",		"h",		90},
	{  "swevel",		"h",		92},
	{     "sut",		"h",		94},
	{     "gut",		"h",		96},
	{   "sstat",		"h",		98},
	{   "gstat",		"h",		100},
	{   "tstat",		"h",		102},
	{    "laga",		"h",		104},
	{    "lagb",		"h",		106},
	{   "delrt",		"h",		108},
	{    "muts",		"h",		110},
	{    "mute",		"h",		112},
	{      "ns",		"u",		114},
	{      "dt",		"u",		116},
	{    "gain",		"h",		118},
	{     "igc",		"h",		120},
	{     "igi",		"h",		122},
	{    "corr",		"h",		124},
	{     "sfs",		"h",		126},
	{     "sfe",		"h",		128},
	{    "slen",		"h",		130},
	{    "styp",		"h",		132},
	{    "stas",		"h",		134},
	{    "stae",		"h",		136},
	{   "tatyp",		"h",		138},
	{   "afilf",		"h",		140},
	{   "afils",		"h",		142},
	{  "nofilf",		"h",		144},
	{  "nofils",		"h",		146},
	{     "lcf",		"h",		148},
	{     "hcf",		"h",		150},
	{     "lcs",		"h",		152},
	{     "hcs",		"h",		154},
	{    "year",		"h",		156},
	{     "day",		"h",		158},
	{    "hour",		"h",		160},
	{  "minute",		"h",		162},
	{     "sec",		"h",		164},
	{  "timbas",		"h",		166},
	{    "trwf",		"h",		168},
	{  "grnors",		"h",		170},
	{  "grnofr",		"h",		172},
	{  "grnlof",		"h",		174},
	{    "gaps",		"h",		176},
	{   "otrav",		"h",		178},
	{      "d1",		"f",		180},
	{      "f1",		"f",		184},
	{      "d2",		"f",		188},
	{      "f2",		"f",		192},
	{  "ungpow",		"f",		196},
	{ "unscale",		"f",		200},
	{     "ntr",		"i",		204},
	{    "mark",		"h",		208},
	{"shortpad",		"h",		210},
};




void ibm_to_float(int from[], int to[], int n, int endian, int verbose);

void tapebhed_to_bhed(const tapebhed *tapebhptr, bhed *bhptr);

void tapesegy_to_segy(const tapesegy *tapetrptr, segy *trptr);

void int_to_float(int from[], float to[], int n, int endian);

void short_to_float(short from[], float to[], int n, int endian);

void integer1_to_float(signed char from[], float to[], int n);

void ugethval(cwp_String type1, Value *valp1, char type2, int ubyte,
              char *ptr2, int endian, int conv, int verbose);


void setval( cwp_String type, Value *valp, double a, double b,
		double c, double i, double j);

double mod(double x, double y);

void changeval(cwp_String type1, Value *valp1, cwp_String type2,
	       Value *valp2, cwp_String type3, Value *valp3,
		double a, double b, double c, double d, double e, double f);

void seis_gethval(trace_t *tr, int index, Value *valp);

void seis_puthval(trace_t *tr, int index, Value *valp);

#endif /* LSU_SRC_CLIENT_SEIS_SU_HELPERS_H_ */
