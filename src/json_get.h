#ifndef	_JSON_GET_H_
#define	_JSON_GET_H_

// TODO: Allow regexp's for object selectors?
#define	VT_SLICE	0	// an array slice, an array element is i:i
#define	VT_KEY		1	// the key of a key:value pair in an object
#define	VT_STAR		2	// All keys of an object. {*} uses obj semantics, [*] uses array semantics
#define	VT_OBJ		3	// points to a value table holding the requested keys of an object
#define	VT_ARY		4	// points to a value table holding the requested slices of an array
#define	VT_VALS		5	// points to a value table holding another value table with selectors
#define	VT_VLIST	6	// points to a value table holding another value table with values

#define	ND_INDENT	2	// 2 spaces for each indent leven for node_dump()

typedef	struct	slice_t	{
	int	s_low;
	int	s_high;
} SLICE_T;

typedef	struct	value_t	{
	int	v_type;
	union	{
		SLICE_T	v_slice;
		char	*v_key;
		struct vtab_t	*v_vtab;
	} v_value;
} VALUE_T;

typedef	struct	vtab_t	{
	VALUE_T	**v_vtab;
	int	vn_vtab;
} VTAB_T;

#endif
