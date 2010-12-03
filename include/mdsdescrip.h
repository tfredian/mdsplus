#ifndef MDSDESCRIP_H_DEFINED
#define MDSDESCRIP_H_DEFINED 1

#ifndef HAVE_VXWORKS_H
#include <config.h>
#endif

#ifdef DTYPE_EVENT
#undef DTYPE_EVENT
#endif
#define DTYPE_MISSING	   0
#define DTYPE_Z            0		/* unspecified */
#define DTYPE_BU           2		/* byte (unsigned);  8-bit unsigned quantity */
#define DTYPE_WU           3		/* word (unsigned);  16-bit unsigned quantity */
#define DTYPE_LU           4		/* longword (unsigned);  32-bit unsigned quantity */
#define DTYPE_QU           5		/* quadword (unsigned);  64-bit unsigned quantity */
#define DTYPE_B	           6		/* byte integer (signed);  8-bit signed 2's-complement integer */
#define DTYPE_W	           7		/* word integer (signed);  16-bit signed 2's-complement integer */
#define DTYPE_L	           8		/* longword integer (signed);  32-bit signed 2's-complement integer */
#define DTYPE_Q	           9		/* quadword integer (signed);  64-bit signed 2's-complement integer */
#define DTYPE_F	          10		/* F_floating;  32-bit single-precision floating point */
#define DTYPE_D	          11		/* D_floating;  64-bit double-precision floating point */
#define DTYPE_FC	  12		/* F_floating complex */
#define DTYPE_DC	  13		/* D_floating complex */
#define DTYPE_T	          14		/* character string;  a single 8-bit character or a sequence of characters */
#define DTYPE_OU	  25		/* octaword (unsigned);  128-bit unsigned quantity */
#define DTYPE_O	          26		/* octaword integer (signed);  128-bit signed 2's-complement integer */
#define DTYPE_G	          27		/* G_floating;  64-bit double-precision floating point */
#define DTYPE_H	          28		/* H_floating;  128-bit quadruple-precision floating point */
#define DTYPE_GC	  29		/* G_floating complex */
#define DTYPE_HC	  30		/* H_floating complex */
#define DTYPE_FS          52		/* IEEE float basic single S */
#define DTYPE_FT          53              /* IEEE float basic double T */
#define DTYPE_FSC         54		/* IEEE float basic single S complex */
#define DTYPE_FTC         55		/* IEEE float basic double T complex */
#define DTYPE_DSC	  24		/* descriptor */
#define DTYPE_POINTER     51
#define DTYPE_IDENT	 191
#define DTYPE_NID	 192
#define DTYPE_PATH	 193
#define DTYPE_PARAM	 194
#define DTYPE_SIGNAL	 195
#define DTYPE_DIMENSION	 196
#define DTYPE_WINDOW	 197
#define DTYPE_SLOPE	 198 /* Do not use this deprecated type */
#define DTYPE_FUNCTION	 199
#define DTYPE_CONGLOM	 200
#define DTYPE_RANGE	 201
#define DTYPE_ACTION	 202
#define DTYPE_DISPATCH	 203
#define DTYPE_PROGRAM	 204
#define DTYPE_ROUTINE	 205
#define DTYPE_PROCEDURE	 206
#define DTYPE_METHOD	 207
#define DTYPE_DEPENDENCY 208
#define DTYPE_CONDITION	 209
#define DTYPE_EVENT	 210
#define DTYPE_WITH_UNITS 211
#define DTYPE_CALL	 212
#define DTYPE_WITH_ERROR 213
#define DTYPE_LIST       214
#define DTYPE_TUPLE      215
#define DTYPE_DICTIONARY 216

#ifdef BIG_DESC
typedef unsigned int descriptor_length;
typedef signed   int s_descriptor_length;
typedef unsigned long long descriptor_llength;
typedef signed   long long s_descriptor_llength;
typedef unsigned long long descriptor_a_arsize;
typedef unsigned long long descriptor_a_mult;
typedef long long descriptor_a_bounds;
typedef unsigned int descriptor_ndesc;
#else
typedef unsigned short descriptor_length;
typedef short s_descriptor_length;
typedef unsigned int descriptor_llength;
typedef int s_descriptor_llength;
typedef unsigned int descriptor_a_arsize;
typedef unsigned int descriptor_a_mult;
typedef int descriptor_a_bounds;
typedef unsigned char descriptor_ndesc;
#endif

#if defined(__VMS)
#pragma member_alignment save
#pragma nomember_alignment
#endif  /* __VMS */

#ifdef _WIN32
#define __char_align__ char
#else /* _WINDOWS */
#define __char_align__
#endif /* _WINDOWS */

#ifdef BIG_DESC
#define CLASS_S_SHORT	  1		/* fixed-length descriptor */
#define CLASS_D_SHORT	  2		/* dynamic string descriptor */
#define CLASS_A_SHORT	  4		/* array descriptor */
#define CLASS_P_SHORT     5		/* procedure descriptor */
#define CLASS_NCA        10
#define CLASS_XD_SHORT  192
#define CLASS_XS_SHORT  193
#define CLASS_R_SHORT   194
#define CLASS_CA_SHORT  195
#define CLASS_APD_SHORT 196
#define CLASS_S         101		/* fixed-length descriptor */
#define CLASS_D         102		/* dynamic string descriptor */
#define CLASS_A         104		/* array descriptor */
#define CLASS_P         105		/* procedure descriptor */
#define CLASS_XD        106
#define CLASS_XS        107
#define CLASS_R         108
#define CLASS_CA        109
#define CLASS_APD       110


#define DESCRIPTOR_HEAD(ptr_type)  \
  unsigned short unused; \
  unsigned char	dtype; \
  unsigned char	class; \
  ptr_type	*pointer; \
  descriptor_length length;
#define DESCRIPTOR_HEAD_INI(length,dtype,class,pointer) 0,dtype,class,pointer,length
#else
#define CLASS_S_SHORT	  1		/* fixed-length descriptor */
#define CLASS_D_SHORT	  2		/* dynamic string descriptor */
#define CLASS_A_SHORT	  4		/* array descriptor */
#define CLASS_P_SHORT	  5		/* procedure descriptor */
#define CLASS_NCA        10
#define CLASS_XD_SHORT  192
#define CLASS_XS_SHORT  193
#define CLASS_R_SHORT   194
#define CLASS_CA_SHORT  195
#define CLASS_APD_SHORT 196
#define CLASS_S           1		/* fixed-length descriptor */
#define CLASS_D           2		/* dynamic string descriptor */
#define CLASS_A	          4		/* array descriptor */
#define CLASS_P	          5		/* procedure descriptor */
#define CLASS_XD        192
#define CLASS_XS        193
#define CLASS_R         194
#define CLASS_CA        195
#define CLASS_APD       196

#define DESCRIPTOR_HEAD(ptr_type)  \
  descriptor_length length; \
  unsigned char	dtype; \
  unsigned char	class; \
  ptr_type	*pointer;
#define DESCRIPTOR_HEAD_INI(length,dtype,class,pointer) length,dtype,class,pointer
#endif

#define SHORT_DESCRIPTOR_HEAD(ptr_type) \
  unsigned short length; \
  unsigned char dtype; \
  unsigned char class; \
  ptr_type *pointer;

struct	descriptor {
  DESCRIPTOR_HEAD(char)
};

struct short_descriptor {
  SHORT_DESCRIPTOR_HEAD(char)
};

/*
 *	Fixed-Length Descriptor:
 */
struct	descriptor_s {
  DESCRIPTOR_HEAD(char)
};


/*
 *	Dynamic String Descriptor:
 */
struct	descriptor_d {
  DESCRIPTOR_HEAD(char)
};



/*
 *	Array Descriptor:
 */

typedef	struct _aflags {
  unsigned __char_align__	  : 3;	/* reserved;  must be zero */
  unsigned __char_align__ binscale : 1; /* if set, scale is a power-of-two, otherwise, -ten */
  unsigned __char_align__ redim	 : 1;	/* if set, indicates the array can be redimensioned */
  unsigned __char_align__ column : 1;	/* if set, indicates column-major order (FORTRAN) */
  unsigned __char_align__ coeff  : 1;	/* if set, indicates the multipliers block is present */
  unsigned __char_align__ bounds : 1;	/* if set, indicates the bounds block is present */
} aflags;	/* array flag bits */

#define ARRAY_HEAD(ptr_type)		\
  DESCRIPTOR_HEAD(ptr_type) \
  char		scale;			\
  unsigned char	digits;		\
  aflags    aflags;				\
  unsigned char	dimct;			\
  descriptor_a_arsize	arsize;

#define SHORT_ARRAY_HEAD(ptr_type)		\
  SHORT_DESCRIPTOR_HEAD(ptr_type) \
  char		scale;			\
  unsigned char	digits;		\
  aflags    aflags;				\
  unsigned char	dimct;			\
  unsigned int	arsize;

struct	descriptor_a {
  ARRAY_HEAD(char)

  /*
   * One or two optional blocks of information may follow contiguously at this point;
   * the first block contains information about the dimension multipliers (if present,
   * aflags.coeff is set), the second block contains information about
   * the dimension bounds (if present, aflags.bounds is set).  If the
   * bounds information is present, the multipliers information must also be present.
   *
   * The multipliers block has the following format:
   *	char	*_DSCA_(a0);		Address of the element whose subscripts are all zero
   *	int	_DSCL_(m) [DIMCT];	Addressing coefficients (multipliers)
   *
   * The bounds block has the following format:
   *	struct
   *	{
   *		unsigned int	_DSCL_(l);	Lower bound
   *		unsigned int	_DSCL_(u);	Upper bound
   *	} bounds [DIMCT];
   *
   * (DIMCT represents the value contained in dimct.)
   */
};

struct short_descriptor_a {
  SHORT_ARRAY_HEAD(char)
};


/************************************************
  Supplementary definitons for array classes.
  Useful for classes A, CA, and APD.
*************************************************/


#define ARRAY(ptr_type)		struct {	ARRAY_HEAD(ptr_type)}

#define ARRAY_COEFF(ptr_type, dimct)	struct {\
  ARRAY_HEAD(ptr_type)		\
  ptr_type *a0;			\
  descriptor_a_mult  m[dimct];	\
  }

#define ARRAY_BOUNDS(ptr_type, dimct)	struct {\
  ARRAY_HEAD(ptr_type)			\
  ptr_type	*a0;			\
  descriptor_a_mult m[dimct];		\
  struct {				\
    descriptor_a_bounds l;		\
    descriptor_a_bounds u;		\
  } bounds[dimct];			\
  }

#define SHORT_ARRAY_COEFF(ptr_type, dimct)	struct {\
  SHORT_ARRAY_HEAD(ptr_type)		\
  ptr_type *a0;			\
  unsigned int  m[dimct];	\
  }


#define SHORT_ARRAY_BOUNDS(ptr_type, dimct)	struct {\
  SHORT_ARRAY_HEAD(ptr_type)			\
  ptr_type	*a0;			\
  unsigned int m[dimct];		\
  struct {				\
    int l;		\
    int u;		\
  } bounds[dimct];			\
  }

/************************************************
  CLASS_CA Compressed array descriptor definition.
	This describes an array's shape and type.
	The evaluated class is CLASS_A.
	The length, dtype, multipliers, and
	bounds are imposed on the result.

	The data is reconstructed (decompressed) by
	the evaluation of the pointed descriptor.
	The reconstruction must be self-contained.
	It must have all length and type info.
*************************************************/


/************************************************
  CLASS_APD Array of pointers to descriptors.
	This describes an array's shape and type.
	The evaluated class is CLASS_A.
	The multipliers and
	bounds are imposed on the result.
	The final dtype, length, and arsize are
	from evaluation.

	The pointer points to a list of addresses
	of descriptors.
	However, the length and arsize describe
	the list of pointers to descriptors.
*************************************************/


#define DESCRIPTOR_A(name, len, type, ptr, arsize) \
  ARRAY(char) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_A,(char *)ptr), 0, 0, {0,1,1,0,0}, 1, arsize}
#define DESCRIPTOR_A_COEFF(name, len, type, ptr, dimct, arsize) \
  ARRAY_COEFF(char, dimct) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_A,(char *)ptr), 0, 0, {0,1,1,1,0}, dimct, arsize}
#define DESCRIPTOR_A_COEFF_2(name, len, type, ptr, arsize, rows, columns) \
  ARRAY_COEFF(char, 2) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_A,(char *)ptr), 0, 0, {0,1,1,1,0}, 2, arsize, (char *)ptr, \
            rows, columns}
#define DESCRIPTOR_A_BOUNDS(name, len, type, ptr, dimct, arsize) \
  ARRAY_BOUNDS(char, dimct) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_A,(char *)ptr), 0, 0, {0,1,1,1,1}, dimct, arsize}
#define DESCRIPTOR_CA(name, len, type, ptr, arsize) \
  ARRAY(struct descriptor) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_CA,(struct descriptor *)ptr), 0, 0, \
				   {0,1,1,0,0}, 1, arsize}
#define DESCRIPTOR_CA_COEFF(name, len, type, ptr, dimct, arsize) \
  ARRAY_COEFF(struct descriptor, dimct) name = {DESCRIPTOR_HEAD_INI(len,type,CLASS_CA,(struct descriptor *)ptr), 0, 0, \
						{0,1,1,1,0}, dimct, arsize}
#define DESCRIPTOR_CA_BOUNDS(name, len, type, ptr, dimct, arsize) \
  ARRAY_BOUNDS(struct descriptor, dimct) name = {DESCRIPTOR_HEAD(len,type,CLASS_CA,(struct descriptor *)ptr), 0, 0, \
						 {0,1,1,1,1}, dimct, arsize}
#define DESCRIPTOR_APD(name, type, ptr, ndesc) \
  ARRAY(struct descriptor *) name = {DESCRIPTOR_HEAD_INI(sizeof(struct descriptor *),type,CLASS_APD,(struct descriptor **)ptr), \
				     0, 0, {0,1,1,0,0}, 1, ndesc*sizeof(struct descriptor *)}
#define DESCRIPTOR_APD_COEFF(name, type, ptr, dimct, ndesc) \
  ARRAY_COEFF(struct descriptor *, dimct) name = \
  {DESCRIPTOR_HEAD_INI(sizeof(struct descriptor *),type,CLASS_APD,(struct descriptor **)ptr), \
   0, 0, {0,1,1,1,0}, dimct, ndesc*sizeof(struct descriptor *)}
#define DESCRIPTOR_APD_BOUNDS(name, type, ptr, dimct, ndesc) \
  ARRAY_BOUNDS(struct descriptor *, dimct) name = \
  {DESCRIPTOR_HEAD_INI(sizeof(struct descriptor *),type,CLASS_APD,(struct descriptor **)ptr), \
   0, 0, {0,1,1,1,1}, dimct, ndesc*sizeof(struct descriptor *)}

/************************************************
  CLASS_XD extended dynamic descriptor definition.
	Dynamic memory pointed to must be freed
	using the pointer and length.
	The descriptor is usually on the stack.
*************************************************/


struct descriptor_xd {
  DESCRIPTOR_HEAD(struct descriptor)
  descriptor_llength l_length;
};

struct short_descriptor_xd {
  SHORT_DESCRIPTOR_HEAD(struct short_descriptor)
  unsigned int l_length;
};

#define EMPTYXD(name) struct descriptor_xd name = {DESCRIPTOR_HEAD_INI(0,DTYPE_DSC,CLASS_XD,0), 0}

/************************************************
  CLASS_XS extended static descriptor definition.
*************************************************/


struct descriptor_xs {
  DESCRIPTOR_HEAD(struct descriptor)
  descriptor_llength l_length;
};

/************************************************
  CLASS_R Record descriptor definition.
*************************************************/


#define RECORD_HEAD				\
  DESCRIPTOR_HEAD(char) \
  descriptor_ndesc	ndesc;

#define SHORT_RECORD_HEAD				\
  SHORT_DESCRIPTOR_HEAD(char) \
  unsigned char	ndesc;

struct descriptor_r		{	RECORD_HEAD
					struct descriptor *dscptrs[1];
				};

struct short_descriptor_r	{ SHORT_RECORD_HEAD
                                  struct short_descriptor *dscptrs[1];
				};

#define RECORD(ndesc)	struct	{\
    RECORD_HEAD								\
    struct descriptor *dscptrs[ndesc];					\
  }

#define DESCRIPTOR_R(name, type, ndesc) \
  RECORD(ndesc) name = {DESCRIPTOR_HEAD_INI(0,type,CLASS_R, 0), ndesc}

/*****************************************************
  MISSING marks omitted argument, converts to default.
  IDENT is text-like variable name for compiler.
  NID is 4-bytes describing a node identifier.
  PATH is text-like tree location.
  Others are supported record data types.
*****************************************************/


struct descriptor_param	{
  RECORD_HEAD
  struct descriptor *value;
  struct descriptor *help;
  struct descriptor *validation;
};

#define DESCRIPTOR_PARAM(name, value, help, validation) \
  struct descriptor_param name = {\
    DESCRIPTOR_HEAD_INI(0,DTYPE_PARAM,CLASS_R, 0), 3,			\
    (struct descriptor *)value, (struct descriptor *)help, (struct descriptor *)validation}


struct descriptor_signal	{	RECORD_HEAD
					struct descriptor *data;
					struct descriptor *raw;
					struct descriptor *dimensions[1];
				};

#define SIGNAL(ndims)	struct	{	RECORD_HEAD \
					struct descriptor *data;		\
					struct descriptor *raw;		\
					struct descriptor *dimensions[ndims];	\
				}

#define DESCRIPTOR_SIGNAL(name, ndims, data, raw) \
  SIGNAL(ndims) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_SIGNAL, CLASS_R, 0), ndims + 2, \
			(struct descriptor *)data, (struct descriptor *)raw}
#define DESCRIPTOR_SIGNAL_1(name, data, raw, dimension) \
  SIGNAL(1) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_SIGNAL, CLASS_R, 0), 3, \
		    (struct descriptor *)data, (struct descriptor *)raw, (struct descriptor *)dimension}
#define DESCRIPTOR_SIGNAL_2(name, data, raw, dim_1, dim_2) \
  SIGNAL(2) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_SIGNAL, CLASS_R, 0), 4, \
		    (struct descriptor *)data, (struct descriptor *)raw, (struct descriptor *)dim_1, (struct descriptor *)dim_2}

struct descriptor_dimension {
  RECORD_HEAD
  struct descriptor *window;
  struct descriptor *axis;
};

#define DESCRIPTOR_DIMENSION(name, window, axis) \
  struct descriptor_dimension name = {DESCRIPTOR_HEAD_INI(0,DTYPE_DIMENSION, CLASS_R, 0), 2, \
				      (struct descriptor *)window, (struct descriptor *)axis}


struct descriptor_window  {
  RECORD_HEAD
  struct descriptor *startidx;
  struct descriptor *endingidx;
  struct descriptor *value_at_idx0;
};

#define DESCRIPTOR_WINDOW(name, start, iend, xref) \
  struct descriptor_window name = {\
    DESCRIPTOR_HEAD_INI(0, DTYPE_WINDOW, CLASS_R, 0), 3,		\
      (struct descriptor *)start, (struct descriptor *)iend, (struct descriptor *)xref}


struct descriptor_axis	{
  RECORD_HEAD
  struct descriptor *dscptrs[3];
};


/* This is to be replaced by RANGEs of scalars or vectors */

struct descriptor_slope	{
  RECORD_HEAD
  struct {
    struct descriptor *slope;
    struct descriptor *begin;
    struct descriptor *ending;
  } segment[1];
};

#define SLOPE(nsegs)	struct	{\
    RECORD_HEAD						\
    struct {								\
      struct descriptor *slope;					\
      struct descriptor *begin;					\
      struct descriptor *ending;				\
    } segment[nsegs];						\
  }


struct descriptor_function {
  RECORD_HEAD
  struct descriptor *arguments[1];
};

#define FUNCTION(nargs) struct	{\
    RECORD_HEAD								\
    struct descriptor *arguments[nargs];			\
  }

#define DESCRIPTOR_FUNCTION(name, op_code_ptr, nargs) \
  FUNCTION(nargs) name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned short), DTYPE_FUNCTION, CLASS_R, (unsigned char *)op_code_ptr), nargs}
#define DESCRIPTOR_FUNCTION_0(name, op_code_ptr) \
  struct descriptor_function name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned short), DTYPE_FUNCTION, CLASS_R, \
							 (unsigned char *)op_code_ptr), 0,  0}
#define DESCRIPTOR_FUNCTION_1(name, op_code_ptr, arg) \
  struct descriptor_function name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned short), DTYPE_FUNCTION, CLASS_R, \
							 (unsigned char *)op_code_ptr), 1, 	(struct descriptor *)arg}
#define DESCRIPTOR_FUNCTION_2(name, op_code_ptr, arg_1, arg_2)		\
  FUNCTION(2) name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned short), DTYPE_FUNCTION, CLASS_R, (unsigned char *)op_code_ptr), 2, \
		      (struct descriptor *)arg_1, (struct descriptor *)arg_2}


struct descriptor_conglom {
  RECORD_HEAD
  struct descriptor *image;
  struct descriptor *model;
  struct descriptor *name;
  struct descriptor *qualifiers;
};

#define DESCRIPTOR_CONGLOM(sname, image, model, name, qualifiers) \
  struct descriptor_conglom sname = {DESCRIPTOR_HEAD_INI(0, DTYPE_CONGLOM, CLASS_R, 0), 4, \
				     (struct descriptor *)image, (struct descriptor *)model, (struct descriptor *)name, \
				     (struct descriptor *)qualifiers}


struct descriptor_range	{
  RECORD_HEAD
  struct descriptor *begin;
  struct descriptor *ending;
  struct descriptor *deltaval;
};

#define DESCRIPTOR_RANGE(name, begin, ending, delta) \
  struct descriptor_range name = {DESCRIPTOR_HEAD_INI(0, DTYPE_RANGE, CLASS_R, 0), 3, \
				  (struct descriptor *)begin, (struct descriptor *)ending, (struct descriptor *)delta}


struct descriptor_action {
  RECORD_HEAD
  struct descriptor *dispatch;
  struct descriptor *task;
  struct descriptor *errorlogs;
  struct descriptor *completion_message;
  struct descriptor_a *performance;
};

#define DESCRIPTOR_ACTION(name, dispatch, task, errorlogs) \
  struct descriptor_action name = {DESCRIPTOR_HEAD_INI(0, DTYPE_ACTION, CLASS_R, 0), 5, \
				   (struct descriptor *)dispatch, (struct descriptor *)task, (struct descriptor *)errorlogs, 0, 0}


struct descriptor_dispatch {
  RECORD_HEAD
  struct descriptor *ident;
  struct descriptor *phase;
  struct descriptor *when;
  struct descriptor *completion;
};

/*****************************************************
  So far three types of scheduling are supported.
*****************************************************/
#define TreeSCHED_NONE	0
#define TreeSCHED_ASYNC	1
#define TreeSCHED_SEQ	2
#define TreeSCHED_COND	3

#define DESCRIPTOR_DISPATCH(name, type, ident, phase, when, completion) \
  struct descriptor_dispatch name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned char), DTYPE_DISPATCH, CLASS_R, (unsigned char *)type), 4, \
	(struct descriptor *)ident, (struct descriptor *)phase, (struct descriptor *)when, \
	(struct descriptor *)completion}



struct descriptor_program {
  RECORD_HEAD
  struct descriptor *time_out;
  struct descriptor *program;
};

#define DESCRIPTOR_PROGRAM(name, program, timeout) \
  struct descriptor_program name = {DESCRIPTOR_HEAD_INI(0, DTYPE_PROGRAM, CLASS_R, 0), 2, \
				    (struct descriptor *)timeout, (struct descriptor *)program}


struct descriptor_routine {
  RECORD_HEAD
  struct descriptor *time_out;
  struct descriptor *image;
  struct descriptor *routine;
  struct descriptor *arguments[1];
};

#define ROUTINE(nargs)	struct	{\
    RECORD_HEAD								\
    struct descriptor *time_out;				\
    struct descriptor *image;					\
    struct descriptor *routine;					\
    struct descriptor *arguments[nargs];		\
  }

#define DESCRIPTOR_ROUTINE(name, image, routine, timeout, nargs) \
  ROUTINE(nargs) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_ROUTINE, CLASS_R, 0), nargs + 3, \
	(struct descriptor *)timeout, (struct descriptor *)image, (struct descriptor *)routine}


struct descriptor_procedure {
  RECORD_HEAD
  struct descriptor *time_out;
  struct descriptor *language;
  struct descriptor *procedure;
  struct descriptor *arguments[1];
};

#define PROCEDURE(nargs) struct { \
    RECORD_HEAD								\
    struct descriptor *time_out;				\
    struct descriptor *language;				\
    struct descriptor *procedure;				\
    struct descriptor *arguments[nargs];			\
  }

#define DESCRIPTOR_PROCEDURE(name, language, procedure, timeout, nargs) \
  PROCEDURE(nargs) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_PROCEDURE, CLASS_R, 0), nargs + 3, \
			   (struct descriptor *)timeout, (struct descriptor *)language, (struct descriptor *)procedure}


struct descriptor_method {
  RECORD_HEAD
  struct descriptor *time_out;
  struct descriptor *method;
  struct descriptor *object;
  struct descriptor *arguments[1];
};


#define METHOD(nargs)	struct	{\
    RECORD_HEAD								\
    struct descriptor *time_out;				\
    struct descriptor *method;					\
    struct descriptor *object;					\
    struct descriptor *arguments[nargs];			\
  }

#define METHOD_0 struct	{\
    RECORD_HEAD								\
      struct descriptor *time_out;				\
    struct descriptor *method;					\
    struct descriptor *object;					\
  }

#define DESCRIPTOR_METHOD(name, method, object, timeout, nargs) \
  METHOD(nargs) name = {DESCRIPTOR_HEAD_INI(0, DTYPE_METHOD, CLASS_R, 0), nargs + 3, \
			(struct descriptor *)timeout, (struct descriptor *)method, (struct descriptor *)object}
#define DESCRIPTOR_METHOD_0(name, method, object, timeout) \
  METHOD_0 name = {DESCRIPTOR_HEAD_INI(0, DTYPE_METHOD, CLASS_R, 0), 3,	\
		   (struct descriptor *)timeout, (struct descriptor *)method, (struct descriptor *)object}


struct descriptor_dependency {	
  RECORD_HEAD
  struct descriptor *arguments[2];
};

#define DESCRIPTOR_DEPENDENCY(name, op_code_ptr, arg_1, arg_2) \
  struct descriptor_dependency name = {\
    DESCRIPTOR_HEAD_INI(sizeof(unsigned char), DTYPE_DEPENDENCY, CLASS_R, (unsigned char *)op_code_ptr), 2, \
    (struct descriptor *)arg_1, (struct descriptor *)arg_2}


struct descriptor_condition {	
  RECORD_HEAD
  struct descriptor *condition;
};

#define DESCRIPTOR_CONDITION(name, modifier, condition) \
  struct descriptor_condition name = {\
    DESCRIPTOR_HEAD_INI(sizeof(unsigned char), DTYPE_CONDITION, CLASS_R, (unsigned char *)modifier), 1, \
    (struct descriptor *)condition}


#define TreeNEGATE_CONDITION 	7
#define TreeIGNORE_UNDEFINED 	8
#define TreeIGNORE_STATUS	9
#define TreeDEPENDENCY_AND	10
#define TreeDEPENDENCY_OR	11



struct descriptor_with_units {	
  RECORD_HEAD
  struct descriptor *data;
  struct descriptor *units;
};

#define DESCRIPTOR_WITH_UNITS(name, value, units) \
  struct descriptor_with_units name = {DESCRIPTOR_HEAD_INI(0,DTYPE_WITH_UNITS,CLASS_R,0),2, \
				       (struct descriptor *)value, (struct descriptor *)units}

/*********************************************
  A CALL invokes a routine in a shared image.
  TDI syntax is image->routine:type(arg0,...).
  The result (R0,R1) is given dtype, def=L.
*********************************************/


struct descriptor_call	{
  RECORD_HEAD
  struct descriptor *image;
  struct descriptor *routine;
  struct descriptor *arguments[1];
};

#define CALL(nargs) struct {\
    RECORD_HEAD								\
    struct descriptor *image;					\
    struct descriptor *routine;					\
    struct descriptor *arguments[nargs];			\
  }

#define DESCRIPTOR_CALL(name, dtype_ptr, nargs, image, routine) \
  CALL(nargs) name = {DESCRIPTOR_HEAD_INI(sizeof(unsigned short), DTYPE_CALL, CLASS_R, (unsigned char *)dtype_ptr), nargs+2, \
		      (struct descriptor *)image, (struct descriptor *)routine}


struct descriptor_with_error {	
  RECORD_HEAD
  struct descriptor *data;
  struct descriptor *error;
};

#define DESCRIPTOR_WITH_ERROR(name, data, error) \
  struct descriptor_with_error name = {DESCRIPTOR_HEAD_INI(0,DTYPE_WITH_ERROR,CLASS_R,0),2, \
				       (struct descriptor *)data, (struct descriptor *)error}

#define	 DESCRIPTOR_FLOAT(name, _float) struct descriptor name = {DESCRIPTOR_HEAD_INI(sizeof(float), DTYPE_NATIVE_FLOAT, CLASS_S, (char *)_float)}
#define	 DESCRIPTOR_LONG(name, _long) struct descriptor name = {DESCRIPTOR_HEAD_INI(sizeof(int), DTYPE_L, CLASS_S, (char *)_long)}
#define	 DESCRIPTOR_NID(name, _nid) struct descriptor name = {DESCRIPTOR_HEAD_INI(sizeof(int), DTYPE_NID, CLASS_S, (char *)_nid)}

/*
 *	Atomic data types:
 */

/*
 *	A simple macro to construct a 32-bit string descriptor:
 */
#define DESCRIPTOR(name,string)	struct descriptor_s name = { DESCRIPTOR_HEAD_INI(sizeof(string)-1, DTYPE_T, CLASS_S, string) }
#define DESCRIPTOR_INIT(length,dtype,class,pointer) { DESCRIPTOR_HEAD_INI(length,dtype,class,pointer)}
#define DESCRIPTOR_FROM_CSTRING(d,cstring) \
  struct descriptor d = {DESCRIPTOR_HEAD_INI(0,DTYPE_T,CLASS_S,0)};	\
  d.length=strlen(cstring); \
  d.pointer=cstring;


/*
 * array typedefs
 */

#define MAXDIM 8
typedef ARRAY_COEFF(char, MAXDIM) array_coeff;
typedef ARRAY_BOUNDS(char,MAXDIM) array_bounds;
typedef ARRAY_BOUNDS(struct descriptor *,MAXDIM) array_bounds_desc;
typedef ARRAY(char) array; 
typedef ARRAY(struct descriptor *) array_desc;
typedef SIGNAL(MAXDIM) signal_maxdim;
typedef SHORT_ARRAY_COEFF(char, MAXDIM) short_array_coeff;
typedef SHORT_ARRAY_BOUNDS(char, MAXDIM) short_array_bounds;

#ifdef __VMS
#pragma member_alignment restore
#endif /* __VMS */

#ifdef __VMS
#define DTYPE_NATIVE_FLOAT DTYPE_F
#define DTYPE_FLOAT_COMPLEX DTYPE_FC

#if __G_FLOAT
#define DTYPE_NATIVE_DOUBLE DTYPE_G
#define DTYPE_DOUBLE_COMPLEX DTYPE_GC
#else /* __G_FLOAT */
#define DTYPE_NATIVE_DOUBLE DTYPE_D
#define DTYPE_DOUBLE_COMPLEX DTYPE_DC
#endif /* __G_FLOAT */

#else /* __VMS */
#define DTYPE_NATIVE_FLOAT DTYPE_FS
#define DTYPE_NATIVE_DOUBLE DTYPE_FT
#define DTYPE_FLOAT_COMPLEX DTYPE_FSC
#define DTYPE_DOUBLE_COMPLEX DTYPE_FTC
#endif /* __VMS */

#ifndef DTYPE_FLOAT
#define DTYPE_FLOAT DTYPE_NATIVE_FLOAT
#endif

#ifndef DTYPE_DOUBLE
#define DTYPE_DOUBLE DTYPE_NATIVE_DOUBLE
#endif

#endif
