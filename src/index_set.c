#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "log.h"
#include "index_set.h"

INDEX_SET_T	*
IS_new_iset(const INDEX_SPEC_T *ispec)
{
	INDEX_SET_T	*iset = NULL;
	const INDEX_SPEC_T	*isp, *isp1;
	INDEX_SPEC_T	*n_isp;
	int	i, n_ispecs;
	int	err = 0;

	iset = (INDEX_SET_T *)calloc((size_t)1, sizeof(INDEX_SET_T));
	if(iset == NULL){
		LOG_ERROR("can't allocate iset");
		err = 1;
		goto CLEAN_UP;
	}

	for(n_ispecs = 0, isp = ispec; isp; isp = isp->i_next)
		n_ispecs++;
	if(n_ispecs == 0){
		LOG_ERROR("no ispecs");
		err = 1;
		goto CLEAN_UP;
	}
	iset->in_specs = n_ispecs;
	iset->i_specs = (INDEX_SPEC_T **)calloc((size_t)n_ispecs, sizeof(INDEX_SET_T *));
	if(iset->i_specs == NULL){
		LOG_ERROR("can't allocate iset->i_specs");
		err = 1;
		goto CLEAN_UP;
	}
	for(i = 0, isp = ispec; isp; isp = isp->i_next, i++){
		n_isp = IS_new_ispec();
		if(n_isp == NULL){
			LOG_ERROR("can't allocate iset->i_specs[%d]", i);
			err = 1;
			goto CLEAN_UP;
		}
		n_isp->i_vname = strdup(isp->i_vname);
		if(n_isp->i_vname == NULL){
			LOG_ERROR("can't strdup iset->i_specs[%d]->i_vname", i);
			err = 1;
			goto CLEAN_UP;
		}
		n_isp->i_is_hashed = isp->i_is_hashed;
		n_isp->i_is_active = isp->i_is_active;
		n_isp->i_begin = isp->i_begin;
		n_isp->i_end = isp->i_end;
		n_isp->i_incr = isp->i_incr;
		n_isp->i_current = isp->i_current;
		iset->i_specs[i] = n_isp;
	}

CLEAN_UP : ;

	if(err){
		IS_delete_iset(iset);
		iset = NULL;
	}

	return iset;
}

void
IS_delete_iset(INDEX_SET_T *iset)
{

	if(iset == NULL)
		return;

	if(iset->i_specs != NULL){
		int	i;
		INDEX_SPEC_T	*ispec;

		for(i = 0; i < iset->in_specs; i++){
			ispec = iset->i_specs[i];
			if(ispec != NULL){
				if(ispec->i_vname != NULL)
					free(ispec->i_vname);
				free(ispec);
			}
		}
		free(iset->i_specs);
	}
	free(iset);
}

void
IS_dump_iset(FILE *fp, const INDEX_SET_T *iset)
{

	if(iset == NULL){
		fprintf(fp, "iset is NULL\n");
		return;
	}

	fprintf(fp, "iset = {\n");
	fprintf(fp, "\tis_active = %d\n", iset->i_is_active);
	fprintf(fp, "\tn_specs   = %d\n", iset->in_specs);
	if(iset->i_specs == NULL)
		fprintf(fp, "\tspecs     = NULL\n");
	else{
		int	i;
		const INDEX_SPEC_T	*isp;

		fprintf(fp, "\tspecs     = {\n");
		for(i = 0; i < iset->in_specs; i++){	
			isp = iset->i_specs[i];
			fprintf(fp, "\t\tvname     = %s\n", isp->i_vname ? isp->i_vname : "NULL");
			fprintf(fp, "\t\tis_hashed = %d\n", isp->i_is_hashed);
			fprintf(fp, "\t\tis_active = %d\n", isp->i_is_active);
			fprintf(fp, "\t\ti_begin   = %d\n", isp->i_begin);
			fprintf(fp, "\t\ti_end     = %d\n", isp->i_end);
			fprintf(fp, "\t\ti_incr    = %d\n", isp->i_incr);
			fprintf(fp, "\t\ti_current = %d\n", isp->i_current);
		}
		fprintf(fp, "\t}\n");
	}
	fprintf(fp, "}\n");
}

INDEX_SPEC_T	*
IS_new_ispec(void)
{
	INDEX_SPEC_T	*ispec = NULL;
	int	err = 0;

	ispec = (INDEX_SPEC_T *)calloc((size_t)1, sizeof(INDEX_SPEC_T));
	if(ispec == NULL){
		LOG_ERROR("can't allocate ispec");
		err = 1;
		goto CLEAN_UP;
	}

CLEAN_UP : ;

	if(err){
		IS_delete_ispec(ispec);
		ispec = NULL;
	}

	return ispec;
}

void
IS_delete_ispec(INDEX_SPEC_T *ispec)
{

	if(ispec == NULL)
		return;

	if(ispec->i_vname != NULL)
		free(ispec->i_vname);
	free(ispec);
}

void
IS_dump_ispec(FILE *fp, const INDEX_SPEC_T *ispec)
{

}
