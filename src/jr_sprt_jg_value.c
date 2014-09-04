static	int
jr_sprt_jg_value(JG_RESULT_T *jg_result, const VALUE_T *vp, const json_t js_cntnr, const char *key, size_t idx, const json_t *js_value)
{
	int	i;
	char	*str = NULL;
	char	work[24];
	size_t	size;
	int	err = 0;

	for(i = 0; i < vp->vn_attr; i++){
		switch(vp->v_attr[i]){
		case VA_VALUE :
			if(js_value == NULL)
				JG_result_add(jg_result, "\1");
			else
				jr_sprt_json_value(jg_result, js_value);
			break;
		case VA_TYPE :
			if(js_value == NULL)
				JG_result_add(jg_result, "UNDEFINED");
			else{
				switch(json_typeof(js_value)){
				case JSON_NULL :
					JG_result_add(jg_result, "NULL");
					break;
				case JSON_FALSE :
					JG_result_add(jg_result, "FALSE");
					break;
				case JSON_TRUE :
					JG_result_add(jg_result, "TRUE");
					break;
				case JSON_INTEGER :
					JG_result_add(jg_result, "INTEGER");
					break;
				case JSON_REAL :
					JG_result_add(jg_result, "REAL");
					break;
				case JSON_STRING :
					JG_result_add(jg_result, "STRING");
					break;
				case JSON_OBJECT :
					JG_result_add(jg_result, "OBJECT");
					break;
				case JSON_ARRAY :
					JG_result_add(jg_result, "ARRAY");
					break;
				default :
					JG_result_add(jg_result, "ERROR");
					break;
				}
			}
			break;
		case VA_SELECTOR :
			if(key != NULL)
				JG_result_add(jg_result, key);
			else{
				sprintf(work, "%ld", idx);
				JG_result_add(jg_result, work);
			}
			break;
		case VA_SIZE :
			if(json_typeof(js_cntnr) == JSON_OBJECT)
				size = json_object_size(js_cntnr);
				sprintf(work, "%ld", size);
				JG_result_add(jg_result, work);
			else if(json_typefo(js_cntnr) == JSON_ARRAY)
				size = json_array_size(js_cntnr);
				sprintf(work, "%ld", size);
				JG_result_add(jg_result, work);
			else{
				LOG_ERROR("unexpected js_cntnr type = %d, must be array/object", json_type(js_cntnr));
				err = 1;
				goto CLEAN_UP;
			}
		default :
			LOG_ERROR("unexpected vattr %d", vp->v_attr[i]);
			err = 1;
			goto CLEAN_UP;
			break;
		}
	}

CLEAN_UP : ;

	if(str != NULL)
		free(str);

	return err;
}
