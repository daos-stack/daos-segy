/*
 * seismic_obj_creation.c
 *
 *  Created on: May 19, 2020
 *      Author: mirnamoawad
 */
#include <dfs_helper_api.h>
#include <daos_seis.h>
#include <sys/stat.h>

int main(int argc, char *argv[]){

    /* Optional */
    int verbose =1;                    /* Flag to allow verbose output */
    int allow_container_creation =1;   /* Flag to allow container creation if not found */


    char pool_id[100]="ff4489e9-893f-442d-a355-7cee7240423c";
	char container_id[100]="ff4489e9-893f-442d-a355-7cee72404231";
	char svc_list[100]="0";


	printf(" PARSING SEGY FILE ============================== \n");
	init_dfs_api(pool_id, svc_list, container_id, allow_container_creation, verbose);
	DAOS_FILE *segyfile = open_dfs_file("/Test/segyobj", S_IFREG | S_IWUSR | S_IRUSR, 'r', 0);
	daos_seis_parse_segy(get_dfs(), NULL, "SEGY_ROOT_OBJECT", segyfile->file);

	    close_dfs_file(segyfile);
	printf(" BEFOREEEEEEEEEEEE OPEN SEGY ROOT OBJECT==================================== \n");


	printf(" OPEN SEGY ROOT OBJECT==================================== \n");
	segy_root_obj_t *segy_root_object = daos_seis_open_root_path(get_dfs(), NULL,"SEGY_ROOT_OBJECT");
	int number_of_traces;
	printf(" PRINTING NUMBER OF TRACES UNDER SEGY ROOT OBJECT==================================== \n");
	number_of_traces = daos_seis_get_trace_count(segy_root_object);
	printf("NUMBER OF TRACES ===== %d \n", number_of_traces);

	printf("READING SEGY ROOT BINARY HEADER KEY==================================== \n");
	daos_seis_read_binary_header(segy_root_object);

	printf("PRINTING NUMBER OF GATHERS IN CMP \n");
	int cmp_gathers;
	cmp_gathers = daos_seis_get_cmp_gathers(get_dfs(),segy_root_object);
	printf("NUMBER OF CMP GATHERSS===== %d \n", cmp_gathers);

	printf("PRINTING NUMBER OF GATHERS IN SHOT \n");
	int shot_gathers;
	shot_gathers = daos_seis_get_shot_gathers(get_dfs(),segy_root_object);
	printf("NUMBER OF SHOT GATHERSS===== %d \n", shot_gathers);

	printf("PRINTING NUMBER OF GATHERS IN OFFSET \n");
	int offset_gathers;
	offset_gathers = daos_seis_get_offset_gathers(get_dfs(),segy_root_object);
	printf("NUMBER OF OFFSET GATHERSS===== %d \n", offset_gathers);

	printf("READING TRACES SHOT====================================\n");
	int shot_id = 610;
	daos_seis_read_shot_traces(get_dfs(), shot_id, segy_root_object);

	printf("CLOSE SEGY ROOT OBJECT==================================== \n");
	daos_seis_close_root(segy_root_object);


    fini_dfs_api();




	return 0;
}