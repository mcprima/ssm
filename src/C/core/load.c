/**************************************************************************
 *    This file is part of ssm.
 *
 *    ssm is free software: you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published
 *    by the Free Software Foundation, either version 3 of the
 *    License, or (at your option) any later version.
 *
 *    ssm is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public
 *    License along with ssm.  If not, see
 *    <http://www.gnu.org/licenses/>.
 *************************************************************************/

#include "ssm.h"

/**
 *load json from a stream
 */
json_t *ssm_load_json_stream(FILE *stream)
{
    json_error_t error;
    json_t *data = json_loadf(stream, JSON_DISABLE_EOF_CHECK, &error);
    if(!data) {
        ssm_print_err(error.text);
        exit(EXIT_FAILURE);
    }

    return data;
}


/**
 *load json from a path
 */
json_t *ssm_load_json_file(const char *path)
{
    json_error_t error;
    json_t *data = json_load_file(path, 0, &error);
    if(!data) {
        ssm_print_err(error.text);
        exit(EXIT_FAILURE);
    }

    return data;
}

json_t *ssm_load_data(ssm_options_t *opts)
{
    json_t *jdata = ssm_load_json_file(".data.json");

    return jdata;
}


void ssm_theta2input(ssm_input_t *input, ssm_theta_t *theta, ssm_nav_t *nav)
{
    int i;
    ssm_it_parameters_t *it = nav->theta_all;

    for(i=0; i< it->length; i++){
        gsl_vector_set(input, it->p[i]->offset, it->p[i]->f_inv(gsl_vector_get(theta, i)));
    }
}


void ssm_input2par(ssm_par_t *par, ssm_input_t *input, ssm_calc_t *calc, ssm_nav_t *nav)
{
    int i;
    ssm_it_parameters_t *it = nav->par_all; //iterate on all the parameters to take into account following relationships

    for(i=0; i< it->length; i++){
        gsl_vector_set(par, it->p[i]->offset, it->p[i]->f_user2par(gsl_vector_get(input, it->p[i]->offset), input, calc));
    }
}


void ssm_par2X(ssm_X_t *X, ssm_par_t *par, ssm_calc_t *calc, ssm_nav_t *nav)
{
    int i;

    ssm_it_states_t *sv = nav->states_sv;
    ssm_it_states_t *inc = nav->states_inc;
    ssm_it_states_t *diff = nav->states_diff;

    for(i=0; i<sv->length; i++){
	X->proj[ sv->p[i]->offset ] = sv->p[i]->f(gsl_vector_get(par, sv->p[i]->ic->offset));
	if(nav->implementation == SSM_PSR){
	    X->proj[ sv->p[i]->offset ] = round(X->proj[ sv->p[i]->offset ]);
	} 
    }

    for(i=0; i<inc->length; i++){
        X->proj[ inc->p[i]->offset ] = 0.0;
    }

    for(i=0; i<diff->length; i++){
        X->proj[ diff->p[i]->offset ] = diff->p[i]->f(gsl_vector_get(par, diff->p[i]->ic->offset));
    }
}


/**
 * Used for prediction 
 *
 * jprediction_j is an object mimating a resources
 * list with object with name "values" (coming from trace_*.csv) and
 * "states" (comming from X_*.csv) 
 * { resources : [{name:"values", "data": ..}, {name:"states", "data": ..}] }
 */ 
void ssm_mcmc_results2X(ssm_X_t *X, json_t *jprediction_j, ssm_calc_t *calc, ssm_nav_t *nav)
{
    int i;

    ssm_it_states_t *sv = nav->states_sv;
    ssm_it_states_t *inc = nav->states_inc;
    ssm_it_states_t *diff = nav->states_diff;

    //get states resource
    json_t *jstates = NULL;
    json_t *jresources = json_object_get(jprediction_j, "resources");
    int index;
    for(index=0; index< json_array_size(jresources); index++){
        json_t *el = json_array_get(jresources, index);
        const char* name = json_string_value(json_object_get(el, "name"));
        if (strcmp(name, "states") == 0) {
            jstates = json_object_get(el, "data");
	    break;
        }
    }

    if(!jstates){
	ssm_print_err("error: no states object in prediction resource");
	exit(EXIT_FAILURE);
    }

    for(i=0; i<sv->length; i++){
	json_t *jval = json_object_get(jstates, sv->p[i]->name);
	if(json_is_number(jval)) {
	    X->proj[ sv->p[i]->offset ] = sv->p[i]->f(json_number_value(jval));
	} else {
	    char str[SSM_STR_BUFFSIZE];
	    sprintf(str, "error: prediction.states.%s is not a number\n", sv->p[i]->name);
	    ssm_print_err(str);
	    exit(EXIT_FAILURE);
	}
	if(nav->implementation == SSM_PSR){
	    X->proj[ sv->p[i]->offset ] = round(X->proj[ sv->p[i]->offset ]);
	}
    }


    for(i=0; i<inc->length; i++){
        X->proj[ inc->p[i]->offset ] = 0.0;
    }


    for(i=0; i<diff->length; i++){
	json_t *jval = json_object_get(jstates, diff->p[i]->name);
	if(json_is_number(jval)) {
	    X->proj[ diff->p[i]->offset ] = diff->p[i]->f(json_number_value(jval));	    
	} else {
	    char str[SSM_STR_BUFFSIZE];
	    sprintf(str, "error: prediction.states.%s is not a number\n", diff->p[i]->name);
	    ssm_print_err(str);
	    exit(EXIT_FAILURE);
	}
    }

}





unsigned int *ssm_load_ju1_new(json_t *container, char *name)
{
    char str[SSM_STR_BUFFSIZE];
    int i;
    json_t *array = json_object_get(container, name);
    unsigned int *tab = malloc(json_array_size(array) * sizeof (unsigned int));
    if(json_array_size(array) && tab==NULL) {
        sprintf(str, "Allocation impossible in file :%s line : %d",__FILE__,__LINE__);
        ssm_print_err(str);
        exit(EXIT_FAILURE);
    }

    for(i=0; i< json_array_size(array); i++) {
        json_t *array_i;
        array_i = json_array_get(array, i);

        if(json_is_number(array_i)){
            tab[i] = (unsigned int) json_integer_value(array_i);
        } else {
            sprintf(str, "error: %s[%d] is not a number\n", name, i);
            ssm_print_err(str);
            exit(EXIT_FAILURE);
        }
    }

    return tab;
}


double *ssm_load_jd1_new(json_t *container, char *name)
{
    char str[SSM_STR_BUFFSIZE];
    int i;
    json_t *array = json_object_get(container, name);
    double *tab = malloc(json_array_size(array) * sizeof (double));
    if(json_array_size(array) && tab==NULL){
        sprintf(str, "Allocation impossible in file :%s line : %d",__FILE__,__LINE__);
        ssm_print_err(str);
        exit(EXIT_FAILURE);
    }

    for(i=0; i< json_array_size(array); i++) {
        json_t *array_i;
        array_i = json_array_get(array, i);

        if(json_is_number(array_i)) {
            tab[i] = json_number_value(array_i);
        } else if(json_is_null(array_i)) {
            tab[i] = NAN;
        } else {
            sprintf(str, "error: %s[%d] is not a number nor null\n", name, i);
            ssm_print_err(str);
            exit(EXIT_FAILURE);
        }
    }

    return tab;
}


char **ssm_load_jc1_new(json_t *container, const char *name)
{

    char str[SSM_STR_BUFFSIZE];
    int i;
    json_t *array = json_object_get(container, name);
    char **tab = malloc(json_array_size(array) * sizeof (char *));
    if(tab==NULL){
        sprintf(str, "Allocation impossible in file :%s line : %d",__FILE__,__LINE__);
        ssm_print_err(str);
        exit(EXIT_FAILURE);
    }

    for(i=0; i< json_array_size(array); i++) {
        json_t *array_i;
        array_i = json_array_get(array, i);

        if(json_is_string(array_i)) {
            tab[i] = strdup(json_string_value(array_i));
        } else {
            sprintf(str, "error: %s[%d] is not a string\n", name, i);
            ssm_print_err(str);
            exit(EXIT_FAILURE);
        }
    }

    return tab;
}
