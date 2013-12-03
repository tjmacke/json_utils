#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <jansson.h>

#include "log.h"
#include "json_get.h"
#include "jg_result.h"
#include "parse_glist.h"
#include "exec_glist.h"

static	int
exec_glist(JG_RESULT_T *, const json_t *, const VALUE_T *, int);

static	int
exec_get(JG_RESULT_T *, const json_t *, const json_t *, const VALUE_T *, int, const VALUE_T *, int);

static	int
exec_object_get(JG_RESULT_T *, const json_t *, const json_t *, const VALUE_T *, int, const VALUE_T *, int, const VALUE_T *);

static	int
exec_array_get(JG_RESULT_T *, const json_t *, const json_t *, const VALUE_T *, int, const VALUE_T *, int, const VALUE_T *);

static	int
chk_json_type(const json_t *, int, int, int);

static	int
is_json_primitive(const json_t *);

static	int
jr_sprt_json_value(JG_RESULT_T *, const json_t *);

static	void
slice_to_indexes(const SLICE_T *, size_t, int *, int *);

int
JG_exec_glist(FILE *fp, const json_t *js_root, const VALUE_T *vp_glist, int n_arys)
{
	JG_RESULT_T	*jg_result = NULL;
	const VTAB_T	*vtab;
	const VALUE_T	*vp;
	int	err = 0;

	if(js_root == NULL){
		LOG_INFO("js_root is NULL");
		goto CLEAN_UP;
	}

	if(vp_glist == NULL){
		LOG_ERROR("vp_glist is NULL");
		err = 1;
		goto CLEAN_UP;
	}else if(vp_glist->v_type != VT_VLIST){
		LOG_ERROR("vp_glist has wrong type %d, expect %d", vp_glist->v_type, VT_VLIST);
		err = 1;
		goto CLEAN_UP;
	}

	jg_result = JG_result_new(fp, n_arys);
	if(jg_result == NULL){
		LOG_ERROR("JG_result_new() failed");
		err = 1;
		goto CLEAN_UP;
	}

	exec_glist(jg_result, js_root, vp_glist, 0);

CLEAN_UP : ;

	if(jg_result != NULL)
		JG_result_delete(jg_result);

	return err;
}

static	int
exec_glist(JG_RESULT_T *jg_result, const json_t *js_root, const VALUE_T *vp_glist, int c_glist)
{
	const VTAB_T	*vtab;
	const VALUE_T	*vp_get;
	int	err = 0;

	if(c_glist < vp_glist->v_value.v_vtab->vn_vtab){
		vtab = vp_glist->v_value.v_vtab;
		vp_get = vtab->v_vtab[c_glist];
		exec_get(jg_result, js_root, js_root, vp_glist, c_glist, vp_get, 0);
	}

	return err;
}

static	int
exec_get(JG_RESULT_T *jg_result, const json_t *js_root, const json_t *js_get, const VALUE_T *vp_glist, int c_glist, const VALUE_T *vp_get, int c_get)
{
	const VTAB_T	*vtab;
	const VALUE_T	*vp;
	int	err = 0;

	if(c_get < vp_get->v_value.v_vtab->vn_vtab){
		vtab = vp_get->v_value.v_vtab;
		vp = vtab->v_vtab[c_get];
		if(vp->v_type == VT_OBJ){
			exec_object_get(jg_result, js_root, js_get, vp_glist, c_glist, vp_get, c_get, vp);
		}else{
			exec_array_get(jg_result, js_root, js_get, vp_glist, c_glist, vp_get, c_get, vp);
		}
	}

	return err;
}

static	int
exec_object_get(JG_RESULT_T *jg_result, const json_t *js_root, const json_t *js_get, const VALUE_T *vp_glist, int c_glist, const VALUE_T *vp_get, int c_get, const VALUE_T *vp_obj)
{
	const VTAB_T	*vtab;
	int	i;
	const VALUE_T	*vp;
	const json_t	*js_value = NULL;
	int	is_primitive;	// no internal structure: null, false, true, int, real, string
	int	err = 0;

	if(chk_json_type(js_get, JSON_OBJECT, c_glist, c_get)){
		err = 1;
		goto CLEAN_UP;
	}

	// object gets:
	//
	// Let OF = Output Field, NOF = Next Ouput Field
	// In every case if if the request key does not exist, continue w/o output
	//
	// 1. {k}			Sets the value of NOF to the value of k.
	// 2. {k1, k2, ..., kN}		Sets the values of the next N OF's to the values of k1, k2, ..., kN
	// 3. {k1}{j1}			Get the value of k1, if it's an object set the value of NOF to the value of _its_ j1
	// 4. {k1, k2, ..., kN}{j1}	Apply Rule 3 to each k[i] in the first select.  For those that are objects and have member j1, set the value of the NOF's to j1's value.
	// 5. {k1}{j1, j2, ..., jM}	Get the value of k1, if it's an object set the values of the next M OF's to the values it's j1, j2, ..., jM
	// 7. {k1, k2, ..., kN}{j1, j2, ..., jM}
	//				Apply Rule 5 to each k[i] in the first select.  For those that are objects and members j1, j2, ..., jM, set the values of the next M OF's to their values.
	// 
	// Examples
	//
	// 1. json = {  "votes":{"funny":0, "useulf":5, "cool":2},
	//		"user_id":"...",
	//		"review_id":"...",
	//		"stars":5,
	//		"date":"2011-01-26",
	//		"text":"...",
	//		"type":"review",
	//		"business_id":"Bid"
	//	     }
	// 1. query = {business_id, date, stars} => Bid\t2011-01-26\t5
	//
	// 2. json = {	"name":"Angry Birds",
	//		"features":{..., "bundle_id":"Bid", ..., "category":"Games", ...},
	//		"icon_url":"URL",
	//		"description":"Awesome time waster!",
	//		...
	//	     }
	// 2. query = {name, features, icon_url, description}{bundle_id, catetory} => Angry Birds\tBid\tGames\tURL\tAwesome time waster

	vtab = vp_obj->v_value.v_vtab;
	for(i = 0; i < vtab->vn_vtab; i++){
		vp = vtab->v_vtab[i];
		js_value = json_object_get(js_get, vp->v_value.v_key);
		if(js_value != NULL){
			is_primitive = is_json_primitive(js_value);
			if(is_primitive || (c_get == vp_get->v_value.v_vtab->vn_vtab - 1))
				jr_sprt_json_value(jg_result, js_value);
			if(!is_primitive && (c_get + 1 < vp_get->v_value.v_vtab->vn_vtab))
				exec_get(jg_result, js_root, js_value, vp_glist, c_glist, vp_get, c_get + 1);
		}else{ 
			// TODO: how to handle missing keys? I'm thinking ignore them
			LOG_ERROR("no such key %s", vp->v_value.v_key);
		}
	}
	if(c_get + 1 == vp_get->v_value.v_vtab->vn_vtab){
		if(c_glist + 1 < vp_glist->v_value.v_vtab->vn_vtab)
			exec_glist(jg_result, js_root, vp_glist, c_glist + 1);
		else
			JG_result_print(jg_result);
	}

CLEAN_UP : ;

	return err;
}

static	int
exec_array_get (JG_RESULT_T *jg_result, const json_t *js_root, const json_t *js_get, const VALUE_T *vp_glist, int c_glist, const VALUE_T *vp_get, int c_get, const VALUE_T *vp_ary)
{
	size_t	s_ary;
	const VTAB_T	*vtab;
	int	i;
	const VALUE_T	*vp;
	int	s, s_low, s_high;
	const json_t	*js_value = NULL;
	int	err = 0;

	if(chk_json_type(js_get, JSON_ARRAY, c_glist, c_get)){
		err = 1;
		goto CLEAN_UP;
	}
	s_ary = json_array_size(js_get);
	JG_result_array_push(jg_result);
	vtab = vp_ary->v_value.v_vtab;
	for(i = 0; i < vtab->vn_vtab; i++){
		vp = vtab->v_vtab[i];
		// vp is a slice, so need to loop over the slice bounds
		slice_to_indexes(&vp->v_value.v_slice, s_ary, &s_low, &s_high);
		for(s = s_low; s <= s_high; s++){
			JG_result_array_init(jg_result);
			js_value = json_array_get(js_get, s - 1);
			if(c_get + 1 == vp_get->v_value.v_vtab->vn_vtab){
				jr_sprt_json_value(jg_result, js_value);
				if(c_glist + 1 == vp_glist->v_value.v_vtab->vn_vtab){
					JG_result_print(jg_result);
				}
			}else if(c_get + 1 < vp_get->v_value.v_vtab->vn_vtab)
				exec_get(jg_result, js_root, js_value, vp_glist, c_glist, vp_get, c_get + 1);
		}
	}
	JG_result_array_pop(jg_result);

CLEAN_UP : ;

	return err;
}

static	int
chk_json_type(const json_t *js_ptr, int type, int c_glist, int c_get)
{

	if(js_ptr == NULL){
		LOG_ERROR("glist(%d, %d): js_get is NULL", c_glist, c_get);
		return 1;
	}else{
		int	jp_type;

		jp_type = json_typeof(js_ptr);
		if(jp_type != type){
			LOG_ERROR("glist(%d, %d): js_get has type %d, expect array(%d)", c_glist, c_get, jp_type, type);
			return 1;
		}
	}

	return 0;
}

static	int
is_json_primitive(const json_t *js_ptr)
{
	int	jp_type;

	if(js_ptr == NULL){
		LOG_ERROR("js_ptr is NULL");
		return 1;
	}
	jp_type = json_typeof(js_ptr);
	return jp_type != JSON_OBJECT && jp_type != JSON_ARRAY;
}

static	int
jr_sprt_json_value(JG_RESULT_T *jg_result, const json_t *js_value)
{
	char	*str = NULL;
	int	err = 0;

	if(js_value == NULL){
		LOG_ERROR("js_value is NULL");
		err = 1;
		goto CLEAN_UP;
	}

	switch(json_typeof(js_value)){
	case JSON_STRING :
		JG_result_add(jg_result, json_string_value(js_value));
		break;
	case JSON_INTEGER :
	case JSON_REAL :
	case JSON_TRUE :
	case JSON_FALSE :
	case JSON_NULL :
		// these items by themselves are not json, so add the JSON_ENCODE_ANY flag
		str = json_dumps(js_value, JSON_INDENT(0)|JSON_COMPACT|JSON_ENCODE_ANY);
		if(str == NULL){
			LOG_ERROR("json_dumps failed for int/real");
			err = 1;
			goto CLEAN_UP;
		}
		JG_result_add(jg_result, str);
		break;
	case JSON_OBJECT :
	case JSON_ARRAY :
	default :
		str = json_dumps(js_value, JSON_INDENT(0)|JSON_COMPACT);
		if(str == NULL){
			LOG_ERROR("json_dumps failed for obj/ary/true/false/null");
			err = 1;
			goto CLEAN_UP;
		}
		JG_result_add(jg_result, str);
	}

CLEAN_UP : ;

	if(str != NULL)
		free(str);

	return err;
}

static	void
slice_to_indexes(const SLICE_T *slp, size_t s_ary, int *low, int *high)
{
	int	idx, off;

	if(slp->s_low > 0)
		*low = slp->s_low < s_ary ? slp->s_low : s_ary;
	else if(slp->s_low == 0)
		*low = s_ary;
	else{
		off = -slp->s_low;
		*low = off >= s_ary ? 1 : s_ary - off;
	}

	if(slp->s_high > 0)
		*high = slp->s_high < s_ary ? slp->s_high : s_ary;
	else if(slp->s_high == 0)
		*high = s_ary;
	else{
		off = -slp->s_high;
		*high = off >= s_ary ? 1 : s_ary - off;
	}

	// TODO: *low > *high
}
