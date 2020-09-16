/*
 * daos_seis_internal_functions.c
 *
 *  Created on: Jun 29, 2020
 *      Author: mirnamoawad
 */

#include "daos_seis_internal_functions.h"

seis_root_obj_t*
daos_seis_open_root(dfs_t *dfs, dfs_obj_t *root)
{
	seis_root_obj_t 	*root_obj;
	seismic_entry_t 	 entry = {0};
	daos_handle_t 		 th = DAOS_TX_NONE;
	int 			 rc;
	int			 i;

	root_obj = malloc(sizeof(seis_root_obj_t));
	root_obj->coh = dfs->coh;
	root_obj->root_obj = root;

	/** Fetch number of keys */
	prepare_seismic_entry(&entry, root->oid, DS_D_KEYS,
			      DS_A_NUM_OF_KEYS,
			      (char*)&(root_obj->num_of_keys),
			      sizeof(int), DAOS_IOD_SINGLE);
	rc = daos_seis_fetch_entry(root->oh, th, &entry, NULL);
	if (rc != 0) {
		err("Fetching number of keys failed, "
		    "error code = %d \n", rc);
		return rc;
	}
	root_obj->keys = malloc(root_obj->num_of_keys * sizeof(char*));

	for(i = 0 ;i < root_obj->num_of_keys; i++) {
		root_obj->keys[i] = malloc(10 * sizeof(char));
		char temp[10]="";
		char akey[100]="";
		sprintf(temp, "%d", i);
		strcpy(akey, DS_A_KEYS);
		strcat(akey,temp);
		prepare_seismic_entry(&entry, root->oid, DS_D_KEYS, akey,
				      root_obj->keys[i], 10 * sizeof(char),
				      DAOS_IOD_ARRAY);
		rc = daos_seis_fetch_entry(root->oh, th, &entry, NULL);
		if (rc != 0) {
			err("Fetching array of keys failed, "
			    "error code = %d \n", rc);
			return rc;
		}

	}
	root_obj->gather_oids = malloc(root_obj->num_of_keys * sizeof(daos_obj_id_t));
	/** Fetch gather object ids */
	for(i = 0; i < root_obj->num_of_keys; i++){
		prepare_seismic_entry(&entry, root->oid, DS_D_SORTING_TYPES,
				      get_dkey(root_obj->keys[i]),
				      (char*)(&root_obj->gather_oids[i]),
				      sizeof(daos_obj_id_t),
				      DAOS_IOD_SINGLE);
		rc = daos_seis_fetch_entry(root->oh, th, &entry, NULL);
		if (rc != 0) {
			err("Fetching <%s> gather oid failed, "
			    "error code = %d \n", root_obj->keys[i], rc);
			return rc;
		}
	}

	/** fetch number of traces */
	prepare_seismic_entry(&entry, root->oid, DS_D_FILE_HEADER,
	DS_A_NTRACES_HEADER, (char*) (&root_obj->number_of_traces), sizeof(int),
			DAOS_IOD_SINGLE);
	rc = daos_seis_fetch_entry(root->oh, th, &entry, NULL);
	if (rc != 0) {
		err("Fetching number of traces failed, error code = %d \n", rc);
		return rc;
	}

	/** fetch number of extended text headers */
	prepare_seismic_entry(&entry, root->oid, DS_D_FILE_HEADER,
	DS_A_NEXTENDED_HEADER, (char*) (&root_obj->nextended), sizeof(int),
			DAOS_IOD_SINGLE);
	rc = daos_seis_fetch_entry(root->oh, th, &entry, NULL);
	if (rc != 0) {
		err("Fetching number of extended headers oid failed,"
				" error code = %d \n", rc);
		return rc;
	}


	return root_obj;
}

dfs_obj_t*
get_parent_of_file_new(dfs_t *dfs, const char *file_directory,
		       int allow_creation, char *file_name,
		       int verbose_output)
{
	daos_oclass_id_t 	cid = OC_SX;
	dfs_obj_t 	       *parent = NULL;
	char 			temp[2048];
	int 			array_len = 0;
	int 			err;
	strcpy(temp, file_directory);
	const char 		*sep = "/";
	char *token = strtok(temp, sep);
	while (token != NULL) {
		array_len++;
		token = strtok(NULL, sep);
	}
	char **array = malloc(sizeof(char*) * array_len);
	strcpy(temp, file_directory);
	token = strtok(temp, sep);
	int 			i = 0;
	while (token != NULL) {
		array[i] = malloc(sizeof(char) * (strlen(token) + 1));
		strcpy(array[i], token);
		token = strtok(NULL, sep);
		i++;
	}

	for (i = 0; i < array_len - 1; i++) {
		dfs_obj_t 	*temp_obj;
		err = dfs_lookup_rel(dfs, parent, array[i], O_RDWR,
				     &temp_obj, NULL, NULL);
		if (err == 0) {
			if (verbose_output) {
				warn("Subdirectory '%s' already exist \n",
				     array[i]);
			}
		} else if (allow_creation) {
			mode_t mode = 0666;
			err = dfs_mkdir(dfs, parent, array[i], mode, cid);
			if (err == 0) {
				if (verbose_output) {
					warn("Created directory '%s'\n",
					     array[i]);
				}
				check_error_code(dfs_lookup_rel(dfs, parent,
								array[i],
								O_RDWR,
								&temp_obj,
								NULL, NULL),
								"Lookup after"
								" mkdir");
			} else {
				warn("Mkdir on %s failed with error code :"
				     " %d \n", array[i], err);
			}
		} else {
			warn("Relative lookup on %s failed with error code :"
			     " %d \n", array[i], err);
		}
		parent = temp_obj;
	}
	strcpy(file_name, array[array_len - 1]);
	for (i = 0; i < array_len; i++) {
		free(array[i]);
	}
	free(array);

	return parent;
}

int
daos_seis_fetch_entry(daos_handle_t oh, daos_handle_t th,
			seismic_entry_t *entry, daos_event_t *ev)
{
	daos_recx_t 	recx;
	d_sg_list_t 	sgl;
	daos_iod_t 	iod;
	daos_key_t 	dkey;
	d_iov_t 	sg_iovs;
	int 		rc;

	d_iov_set(&dkey, (void*)entry->dkey_name, strlen(entry->dkey_name));
	d_iov_set(&iod.iod_name, (void*) entry->akey_name,
		  strlen(entry->akey_name));
	d_iov_set(&sg_iovs, entry->data, entry->size);

	if (entry->iod_type == DAOS_IOD_SINGLE) {
		recx.rx_nr = 1;
		iod.iod_size = entry->size;
	} else if (entry->iod_type == DAOS_IOD_ARRAY) {
		recx.rx_nr = entry->size;
		iod.iod_size = 1;
	}

	iod.iod_nr = 1;
	recx.rx_idx = 0;
	iod.iod_recxs = &recx;
	iod.iod_type = entry->iod_type;
	sgl.sg_nr = 1;
	sgl.sg_nr_out = 0;
	sgl.sg_iovs = &sg_iovs;
	/** insert task in event queue if event is passed
	 * otherwise function will run in blocking mode */
	if (ev != NULL) {
		rc = daos_obj_fetch(oh, th, 0, &dkey, 1, &iod, &sgl, NULL, ev);
		if (ev->ev_error) {
			err("Failed to fetch <%s> and <%s> entry, error"
			    " code = %d\n", entry->dkey_name,
			    entry->akey_name, ev->ev_error);
			return ev->ev_error;
		}
	} else {
		rc = daos_obj_fetch(oh, th, 0, &dkey, 1, &iod, &sgl, NULL,
				    NULL);
		if (rc != 0) {
			err("Failed to fetch <%s> and <%s> entry, error"
			    " code = %d\n", entry->dkey_name, entry->akey_name, rc);
			return rc;
		}
	}

	return rc;
}

int
daos_seis_root_obj_create(dfs_t *dfs, seis_root_obj_t **obj,
			  daos_oclass_id_t cid, char *name,
			  dfs_obj_t *parent, int num_of_keys, char **keys)
{
	struct dfs_entry 	dfs_entry = {0};
	int 			daos_mode;
	int 			rc;
	int 			i;

	/*Allocate object pointer */
	D_ALLOC_PTR(*obj);
	if (*obj == NULL)
		return ENOMEM;

	D_ALLOC_PTR((*obj)->root_obj);
	if ((*obj)->root_obj == NULL)
		return ENOMEM;

	strncpy((*obj)->root_obj->name, name, DFS_MAX_PATH);
	(*obj)->root_obj->name[DFS_MAX_PATH] = '\0';
	(*obj)->root_obj->mode = S_IFDIR | S_IWUSR | S_IRUSR;
	(*obj)->root_obj->flags = O_RDWR;
	(*obj)->number_of_traces = 0;
	(*obj)->num_of_keys = num_of_keys;
	(*obj)->gather_oids = malloc(num_of_keys * sizeof(daos_obj_id_t));
	(*obj)->keys = malloc(num_of_keys * sizeof(char*));

	for(i = 0; i < num_of_keys; i++){
		(*obj)->keys[i] = malloc((strlen(keys[i]) + 1) * sizeof(char));
		strcpy((*obj)->keys[i], keys[i]);
	}

	if (parent == NULL)
		parent = &dfs->root;

	/** Get new OID for root object */
	rc = oid_gen(dfs, cid, false, &((*obj)->root_obj->oid));
	if (rc != 0) {
		err("Generating OID for seismic root object failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	daos_mode = get_daos_obj_mode((*obj)->root_obj->flags);

	rc = daos_obj_open(dfs->coh, (*obj)->root_obj->oid, daos_mode,
			   &((*obj)->root_obj->oh), NULL);
	if (rc != 0) {
		err("Opening seismic root object failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	dfs_entry.oid = (*obj)->root_obj->oid;
	dfs_entry.mode = (*obj)->root_obj->mode;
	dfs_entry.chunk_size = 0;
	dfs_entry.atime = dfs_entry.mtime = dfs_entry.ctime = time(NULL);

	/** insert Seismic root object created under parent */
	rc = insert_entry(parent->oh, DAOS_TX_NONE, (*obj)->root_obj->name,
			  dfs_entry);
	if (rc != 0) {
		err("Inserting seismic root object under parent failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	return rc;
}

int
daos_seis_obj_update(daos_handle_t oh, daos_handle_t th, seismic_entry_t entry)
{

	d_sg_list_t 		sgl;
	daos_recx_t 		recx;
	daos_iod_t 		iod;
	daos_key_t 		dkey;
	d_iov_t 		sg_iovs;
	int 			rc;

	d_iov_set(&dkey, (void*) entry.dkey_name, strlen(entry.dkey_name));
	d_iov_set(&iod.iod_name, (void*) entry.akey_name,
		  strlen(entry.akey_name));

	if (entry.iod_type == DAOS_IOD_SINGLE) {
		recx.rx_nr = 1;
		iod.iod_size = entry.size;
	} else if (entry.iod_type == DAOS_IOD_ARRAY) {
		recx.rx_nr = entry.size;
		iod.iod_size = 1;
	}

	iod.iod_nr = 1;
	recx.rx_idx = 0;
	iod.iod_recxs = &recx;
	iod.iod_type = entry.iod_type;

	d_iov_set(&sg_iovs, entry.data, entry.size);

	sgl.sg_nr = 1;
	sgl.sg_nr_out = 0;
	sgl.sg_iovs = &sg_iovs;

	rc = daos_obj_update(oh, th, 0, &dkey, 1, &iod, &sgl, NULL);
	if (rc != 0) {
		err("Updating daos object failed, error code = %d\n", rc);
		return rc;
	}

	return rc;
}

int
daos_seis_root_update(seis_root_obj_t *root_obj, char *dkey_name,
		      char *akey_name, char *databuf, int nbytes,
		      daos_iod_type_t iod_type)
{
	seismic_entry_t 	seismic_entry = {0};
	int 			rc;

	prepare_seismic_entry(&seismic_entry, root_obj->root_obj->oid,
			      dkey_name, akey_name, databuf, nbytes, iod_type);

	rc = daos_seis_obj_update(root_obj->root_obj->oh, DAOS_TX_NONE,
				  seismic_entry);
	if (rc != 0) {
		err("Updating root seismic object failed, "
		    "error code = %d \n", rc);
		return rc;
	}

	return rc;
}

void
add_gather(seis_gather_t **head, seis_gather_t *new_gather)
{
	if ((*head) == NULL) {
		(*head) = (seis_gather_t*) malloc(sizeof(seis_gather_t));
		(*head)->oids = (daos_obj_id_t*) malloc(50 *
							sizeof(daos_obj_id_t));
		(*head)->unique_key = new_gather->unique_key;
		(*head)->number_of_traces = new_gather->number_of_traces;
		memcpy((*head)->oids, new_gather->oids,
		       sizeof(daos_obj_id_t) * 50);
		(*head)->next_gather = NULL;
	} else {
		seis_gather_t *current = (*head);
		while (current->next_gather != NULL) {
			current = current->next_gather;
		}
		current->next_gather = (seis_gather_t*)
						malloc(sizeof(seis_gather_t));
		current->next_gather->oids = (daos_obj_id_t*)
						malloc(50 * sizeof(daos_obj_id_t));
		current->next_gather->unique_key = new_gather->unique_key;
		current->next_gather->number_of_traces =
						new_gather->number_of_traces;
		memcpy(current->next_gather->oids, new_gather->oids,
		       sizeof(daos_obj_id_t) * 50);
		current->next_gather->next_gather = NULL;
	}
}

void
merge_trace_lists(traces_list_t **headers, traces_list_t **temp_list)
{
	traces_headers_t 	*temp = (*headers)->head;

	if ((*temp_list)->head == NULL) {
		warn("Temp linked list of traces is empty.\n");
		return;
	}
	/** merge two linked lists in one */
	if ((*headers)->head == NULL) {
		(*headers)->head = (*temp_list)->head;
		(*headers)->tail = (*temp_list)->tail;
		(*headers)->size = (*temp_list)->size;
	} else {
		(*headers)->tail->next_trace = (*temp_list)->head;
		(*headers)->tail = (*temp_list)->tail;
		(*headers)->size = (*headers)->size + (*temp_list)->size;
	}
}

void
add_trace_header(trace_t *trace, traces_list_t **head)
{
	traces_headers_t 	*new_node;

	new_node = (traces_headers_t*) malloc(sizeof(traces_headers_t));
	new_node->trace = *trace;
	new_node->next_trace = NULL;

	if ((*head)->head == NULL) {
		(*head)->head = new_node;
		(*head)->tail = new_node;
		(*head)->size++;
	} else {
		(*head)->tail->next_trace = new_node;
		(*head)->tail = new_node;
		(*head)->size++;
	}
}

int
update_gather_traces(dfs_t *dfs, seis_gather_t *head, seis_obj_t *object,
		     char *dkey_name, char *akey_name)
{
	trace_oid_oh_t		gather_trace;
	if (head == NULL) {
		warn("No gathers exist in linked list \n");
		return 0;
	} else {
		int z = 0;
		while (head != NULL) {
			int ntraces = head->number_of_traces;
			int rc;
			char temp[200] = "";
			char gather_dkey_name[200] = "";
			strcat(gather_dkey_name, dkey_name);
			strcat(gather_dkey_name, "_");
			val_sprintf(temp, head->unique_key, object->name);
			strcat(gather_dkey_name, temp);

			gather_trace = object->seis_gather_trace_oids_obj[z];
			/** insert array object_id in gather object... */
			rc = daos_seis_gather_oids_array_update(&gather_trace, head);
			if (rc != 0) {
				err("Updating <%s> object trace object "
				    "array failed, error code = %d \n",
				    object->name, rc);
				return rc;
			}
			rc = update_gather_object(object, gather_dkey_name,
						  DS_A_GATHER_TRACE_OIDS,
						  (char*) &(gather_trace.oid),
						  sizeof(daos_obj_id_t),
						  DAOS_IOD_SINGLE);
			if (rc != 0) {
				err("Updating <%s> object trace object ids key"
				    "failed, error code = %d \n",
				    object->name, rc);
				return rc;
			}
			rc = update_gather_object(object, gather_dkey_name,
						  akey_name, (char*) &ntraces,
						  sizeof(int),DAOS_IOD_SINGLE);
			if (rc != 0) {
				err("Updating <%s> object number of traces key"
				    "failed, error code = %d \n",
				    object->name, rc);
				return rc;
			}
			head = head->next_gather;
			z++;
		}
	}
	return 0;
}

int
check_key_value(Value target, char *key, seis_gather_t *head,
		daos_obj_id_t trace_obj_id, int *ntraces)
{
	int 		exists = 0;
	if (head == NULL) {
		warn("No gathers exist in linked list. \n");
		exists = 0;
		return exists;
	} else {
		while (head != NULL) {
			if (valcmp(hdtype(key), head->unique_key, target)
			    == 0) {
				head->oids[head->number_of_traces] =
								trace_obj_id;
				head->number_of_traces++;
				*ntraces = head->number_of_traces;
				exists = 1;
				if (head->number_of_traces % 50 == 0) {
					head->oids = (daos_obj_id_t*)
							realloc(head->oids,
								(*ntraces + 50) * sizeof(daos_obj_id_t));
				}
				return exists;
			} else {
				head = head->next_gather;
			}
		}
	}
	return exists;
}

int
daos_seis_trace_oids_obj_create(dfs_t *dfs, daos_oclass_id_t cid,
				seis_obj_t *seis_obj)
{
	int 		rc;
	int 		i;

	seis_obj->seis_gather_trace_oids_obj =
			malloc(seis_obj->number_of_gathers *
			       sizeof(trace_oid_oh_t));

	for (i = 0; i < seis_obj->number_of_gathers; i++) {
		if (&seis_obj->seis_gather_trace_oids_obj[i] == NULL) {
			return ENOMEM;
		}

		/** Get new OID for shot object */
		rc = oid_gen(dfs, cid, true,
			     &(seis_obj->seis_gather_trace_oids_obj[i]).oid);
		if (rc != 0) {
			err("Generating object id for gather trace oids failed,"
			    " error code = %d \n", rc);
			return rc;
		}
		/** Open the array object for the gather oids */
		rc = daos_array_open_with_attr(dfs->coh, seis_obj->seis_gather_trace_oids_obj[i].oid,
					       DAOS_TX_NONE, DAOS_OO_RW, 1,
					       500 * sizeof(daos_obj_id_t),
					       &(seis_obj->seis_gather_trace_oids_obj[i]).oh, NULL);
		if (rc != 0) {
			err("Opening gather oids array object failed,"
			    " error code = %d \n", rc);
			return rc;
		}

	}
	return rc;
}

int
daos_seis_gather_obj_create(dfs_t *dfs, daos_oclass_id_t cid,
			    seis_root_obj_t *parent, seis_obj_t **obj,
			    char *key, int index)
{
	int 		daos_mode;
	int 		rc;

	/*Allocate shot object pointer */
	D_ALLOC_PTR(*obj);

	if (*obj == NULL){
		return ENOMEM;
	}
	strcpy((*obj)->name, key);
	(*obj)->gathers = NULL;
	(*obj)->number_of_gathers = 0;

	/** Get new OID for shot object */
	rc = oid_gen(dfs, cid, false, &(*obj)->oid);
	if (rc != 0) {
		err("GENERATING OBJECT ID for seismic object failed,"
		    " error code = %d \n", rc);
		return rc;
	}
	daos_mode = get_daos_obj_mode(O_RDWR);

	rc = daos_obj_open(dfs->coh, (*obj)->oid, daos_mode, &(*obj)->oh, NULL);
	if (rc != 0) {
		err("Opening seismic object failed,"
		    " error code = %d \n", rc);
		return rc;
	}

	oid_cp(&parent->gather_oids[index], (*obj)->oid);
	rc = daos_seis_root_update(parent, DS_D_SORTING_TYPES, get_dkey(key),
				   (char*) &(*obj)->oid, sizeof(daos_obj_id_t),
				   DAOS_IOD_SINGLE);
	if (rc != 0) {
		err("Updating seismic root object failed,"
		    " error code = %d \n", rc);
		return rc;
	}

	return rc;
}

int
daos_seis_trh_update(trace_oid_oh_t *tr_obj, trace_t *tr, int hdrbytes)
{
	seismic_entry_t 	tr_entry = {0};
	int 			rc;

	prepare_seismic_entry(&tr_entry, tr_obj->oid, DS_D_TRACE_HEADER,
			      DS_A_TRACE_HEADER, (char*) tr, hdrbytes,
			      DAOS_IOD_ARRAY);
	rc = daos_seis_obj_update(tr_obj->oh, DAOS_TX_NONE, tr_entry);
	if (rc != 0) {
		err("Updating trace header failed error code = %d\n", rc);
		return rc;
	}

	return rc;
}

int
daos_seis_tr_data_update(trace_oid_oh_t *trace_data_obj, segy *trace)
{
	daos_array_iod_t 	iod;
	daos_range_t 		rg;
	d_sg_list_t 		sgl;
	d_iov_t 		iov;
	int 			offset = 0;
	int 			rc;

	sgl.sg_nr = 1;
	sgl.sg_nr_out = 0;
	d_iov_set(&iov, (void*)(char*)(trace->data),
		  trace->ns * sizeof(float));

	sgl.sg_iovs = &iov;
	iod.arr_nr = 1;
	rg.rg_len = trace->ns * sizeof(float);
	rg.rg_idx = offset;
	iod.arr_rgs = &rg;

	rc = daos_array_write(trace_data_obj->oh, DAOS_TX_NONE, &iod,
			      &sgl, NULL);
	if (rc != 0) {
		err("Updating trace data failed, error code = %d \n", rc);
		return rc;
	}

	return rc;
}

int
daos_seis_gather_oids_array_update(trace_oid_oh_t *object,
				   seis_gather_t *gather)
{
	daos_array_iod_t 	iod;
	seismic_entry_t 	tr_entry = {0};
	daos_range_t 		rg;
	d_sg_list_t 		sgl;
	d_iov_t 		iov;
	int 			rc;

	tr_entry.data = (char*)(gather->oids);

	sgl.sg_nr = 1;
	sgl.sg_nr_out = 0;
	d_iov_set(&iov, (void*) (tr_entry.data), gather->number_of_traces *
		  sizeof(daos_obj_id_t));

	sgl.sg_iovs = &iov;
	iod.arr_nr = 1;
	rg.rg_len = gather->number_of_traces * sizeof(daos_obj_id_t);
	rg.rg_idx = 0;
	iod.arr_rgs = &rg;
	rc = daos_array_write(object->oh, DAOS_TX_NONE, &iod, &sgl, NULL);
	if (rc != 0) {
		err("Updating trace object ids array failed,"
		    "error code = %d \n", rc);
		return rc;
	}
	daos_array_close(object->oh, NULL);

	return rc;
}

daos_obj_id_t
get_tr_data_oid(daos_obj_id_t *tr_hdr, daos_oclass_id_t cid)
{

	daos_obj_id_t 		tr_data_oid;
	uint64_t 		ofeats;
	uint64_t 		hdr;

	tr_data_oid= *tr_hdr;
	tr_data_oid.hi++;

	ofeats = DAOS_OF_DKEY_UINT64 | DAOS_OF_KV_FLAT | DAOS_OF_ARRAY_BYTE;


	/* TODO: add check at here, it should return error if user specified
	 * bits reserved by DAOS
	 */
	tr_data_oid.hi &= (1ULL << OID_FMT_INTR_BITS) - 1;

	/**
	 * | Upper bits contain
	 * | OID_FMT_VER_BITS (version)		 |
	 * | OID_FMT_FEAT_BITS (object features) |
	 * | OID_FMT_CLASS_BITS (object class)	 |
	 * | 96-bit for upper layer ...		 |
	 */
	hdr = ((uint64_t) OID_FMT_VER << OID_FMT_VER_SHIFT);
	hdr |= ((uint64_t) ofeats << OID_FMT_FEAT_SHIFT);
	hdr |= ((uint64_t) cid << OID_FMT_CLASS_SHIFT);
	tr_data_oid.hi |= hdr;

	return tr_data_oid;
}

int
daos_seis_tr_obj_create(dfs_t *dfs, trace_obj_t **trace_hdr_obj, int index,
			segy *trace)
{
	trace_oid_oh_t 		*trace_data_obj;
	char 			 trace_hdr_name[200];
	char 			 trace_data_name[200];
	char 			 tr_index[50];
	int 			 daos_mode;
	int 			 rc;

	strcpy(trace_hdr_name, "Trace_hdr_obj_");
	strcpy(trace_data_name, "Trace_data_obj_");

	/** Allocate object pointer */
	D_ALLOC_PTR(*trace_hdr_obj);
	if ((*trace_hdr_obj) == NULL) {
		return ENOMEM;
	}

	sprintf(tr_index, "%d", index);
	strcat(trace_hdr_name, tr_index);

	strncpy((*trace_hdr_obj)->name, trace_hdr_name, SEIS_MAX_PATH);
	(*trace_hdr_obj)->name[SEIS_MAX_PATH] = '\0';
	(*trace_hdr_obj)->trace = malloc(sizeof(trace_t));
	memcpy((*trace_hdr_obj)->trace, trace, HDRBYTES);

	/** Get new OID for trace header object */
	rc = oid_gen(dfs, OC_SX, false, &(*trace_hdr_obj)->oid);
	if (rc != 0) {
		err("Generating Object id for trace header failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	(*trace_hdr_obj)->trace->trace_header_obj = (*trace_hdr_obj)->oid;
	daos_mode = get_daos_obj_mode(O_RDWR);

	rc = daos_obj_open(dfs->coh, (*trace_hdr_obj)->oid, daos_mode,
			   &(*trace_hdr_obj)->oh, NULL);
	if (rc != 0) {
		err("Opening trace header object failed,"
		    " error code = %d \n",rc);
		return rc;
	}
	trace_oid_oh_t 		trace_oid_oh;

	trace_oid_oh.oid = (*trace_hdr_obj)->oid;
	trace_oid_oh.oh = (*trace_hdr_obj)->oh;

	rc = daos_seis_trh_update(&trace_oid_oh, (*trace_hdr_obj)->trace,
				      HDRBYTES);
	if (rc != 0) {
		err("Updating trace header object failed, "
		    "error code = %d\n", rc);
		return rc;
	}

	D_ALLOC_PTR(trace_data_obj);
	if ((trace_data_obj) == NULL) {
		return ENOMEM;
	}

	/** Get new OID for trace data object */
	rc = oid_gen(dfs, OC_SX, true, &(trace_data_obj)->oid);
	if (rc != 0) {
		err("Generating Object id for trace data failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	/** Open the array object for the file */
	rc = daos_array_open_with_attr(dfs->coh, (trace_data_obj)->oid,
				       DAOS_TX_NONE, DAOS_OO_RW, 1,
				       200 * sizeof(float),
				       &trace_data_obj->oh, NULL);
	if (rc != 0) {
		err("Opening trace data object failed,"
		    " error code = %d \n",rc);
		return rc;
	}
	/** Update trace data object */
	rc = daos_seis_tr_data_update(trace_data_obj, trace);
	if (rc != 0) {
		err("Updating trace data object failed, "
		    "error code = %d\n", rc);
		return rc;
	}
	/** Close trace data object */
	rc = daos_array_close(trace_data_obj->oh, NULL);
	if (rc != 0) {
		err("Closing trace data object failed,"
		    " error code = %d \n",rc);
		return rc;
	}

	D_FREE_PTR(trace_data_obj);

	return rc;
}

void
prepare_seismic_entry(struct seismic_entry *entry, daos_obj_id_t oid,
		      char *dkey, char *akey, char *data, int size,
		      daos_iod_type_t iod_type)
{
	entry->oid = oid;
	entry->dkey_name = dkey;
	entry->akey_name = akey;
	entry->data = data;
	entry->size = size;
	entry->iod_type = iod_type;
}

int
update_gather_object(seis_obj_t *gather_obj, char *dkey_name,
		     char *akey_name, char *data, int nbytes,
		     daos_iod_type_t type)
{
	seismic_entry_t 	gather_entry = {0};
	int 			rc;
	prepare_seismic_entry(&gather_entry, gather_obj->oid, dkey_name,
			      akey_name, data, nbytes, type);
	rc = daos_seis_obj_update(gather_obj->oh, DAOS_TX_NONE, gather_entry);
	if (rc != 0) {
		err("Updating gather object failed, error code = %d\n", rc);
		return rc;
	}

	return rc;
}

int
daos_seis_tr_linking(trace_obj_t *trace_obj, seis_obj_t *seis_obj, char *key)
{
	seis_gather_t 	new_gather_data = {0};
	Value 		unique_value;
	int 		no_of_traces;
	int 		key_exists = 0;
	int 		ntraces;
	int 		rc = 0;

	get_header_value(*(trace_obj->trace), key, &unique_value);

	if (check_key_value(unique_value, key, seis_obj->gathers,
			    trace_obj->oid, &no_of_traces) == 1) {
		key_exists = 1;
	}

	/** if key value doesn't exist in the object gathers */
	if (key_exists == 0) {
		new_gather_data.oids = malloc(50 * sizeof(daos_obj_id_t));
		char temp[200] = "";
		new_gather_data.oids[0] = trace_obj->oid;
		new_gather_data.number_of_traces = 1;
		new_gather_data.unique_key = unique_value;
		char dkey_name[200] = "";
		strcat(dkey_name, get_dkey(key));
		strcat(dkey_name, "_");
		val_sprintf(temp, unique_value, key);
		strcat(dkey_name, temp);
		rc = update_gather_object(seis_obj, dkey_name, DS_A_UNIQUE_VAL,
					  (char*) &new_gather_data.unique_key,
					  sizeof(long), DAOS_IOD_SINGLE);
		if (rc != 0) {
			err("Adding unique value key to seismic object failed,"
			    " error code = %d\n", rc);
			return rc;
		}
		add_gather(&(seis_obj->gathers), &new_gather_data);
		seis_obj->number_of_gathers++;
		free(new_gather_data.oids);
	}

	return rc;
}

int
pcreate(int fds[2], const char *command, char *const argv[])
{
	/** Spawn a process running the command, returning it's pid.
	 *  The fds array passed will be filled with two descriptors:
	 *  fds[0] will read from the child process, and fds[1] will
	 *  write to it. Similarly, the child process will receive a
	 *  reading/writing fd set (in that same order) as arguments.
	 */
	int	 pid;
	int 	 pipes[4];

	/** Warning: I'm not handling possible errors in pipe/fork */

	pipe(&pipes[0]); /** Parent read/child write pipe */
	pipe(&pipes[2]); /** Child read/parent write pipe */

	if ((pid = fork()) > 0) {
		/** Parent process */
		fds[0] = pipes[0];
		fds[1] = pipes[3];

		close(pipes[1]);
		close(pipes[2]);

		return pid;

	} else {
		close(pipes[0]);
		close(pipes[3]);
		dup2(pipes[2], STDIN_FILENO);
		dup2(pipes[1], STDOUT_FILENO);
		execvp(command, argv);
		exit(-1);
	}

	return -1;
}

int
execute_command(char *const argv[], char *write_buffer, int write_bytes,
		char *read_buffer, int read_bytes)
{

	/** Executes the command given in argv, which is a string array
	 * For example if command is ls -l -a
	 * argv should be {"ls, "-l", "-a", NULL}
	 * descriptors to use for read/write.
	 */
	int 		fd[2];
	/** Setup pipe, redirections and execute command. */
	int pid = pcreate(fd, argv[0], argv);
	/** Check for error. */
	if (pid == -1) {
		return -1;
	}
	/** If user wants to write to subprocess STDIN, we write here. */
	if (write_bytes > 0) {
		write(fd[1], write_buffer, write_bytes);
	}
	/** Read cycle : read as many bytes as possible or until
	 *  we reach the maximum requested by user.
	 */
	char *buffer = read_buffer;
	ssize_t bytesread = 1;

	int total_bytes = 0;
	while ((bytesread = read(fd[0], buffer, read_bytes)) > 0) {
		buffer += bytesread;
		total_bytes += bytesread;
		if (bytesread >= read_bytes) {
			break;
		}
	}
	/** Return number of bytes actually read from the STDOUT
	 *  of the subprocess.
	 */
	return total_bytes;
}

segy*
trace_to_segy(trace_t *trace)
{
	segy 		*tp;

	tp = malloc(sizeof(segy));
	memcpy(tp, trace, HDRBYTES);
	memcpy(tp->data, trace->data, tp->ns * sizeof(float));
	return tp;
}

trace_t*
segy_to_trace(segy *segy, daos_obj_id_t hdr_oid)
{
	trace_t		*trace;
	trace = malloc(sizeof(trace_t));
	memcpy(trace, segy, HDRBYTES);
	trace->trace_header_obj = hdr_oid;
	memcpy(trace->data, segy->data, segy->ns * sizeof(float));
	return trace;
}

void
fetch_traces_header_read_traces(daos_handle_t coh, daos_obj_id_t *oids,
				read_traces *traces, int daos_mode)
{
	seismic_entry_t 	seismic_entry = {0};
	trace_oid_oh_t		trace_hdr_obj;
	int 			rc;
	int 			i;

	for (i = 0; i < traces->number_of_traces; i++) {
		trace_hdr_obj.oid = oids[i];
		/** Open trace header object */
		rc = daos_obj_open(coh, trace_hdr_obj.oid, daos_mode,
				   &trace_hdr_obj.oh, NULL);
		if (rc != 0) {
			err("Opening trace header object failed, error"
			    " code = %d \n", rc);
			exit(rc);
		}
		/** Fetch Trace headers */
		prepare_seismic_entry(&seismic_entry, trace_hdr_obj.oid,
				      DS_D_TRACE_HEADER, DS_A_TRACE_HEADER,
				      (char*)&(traces->traces[i]), HDRBYTES,
				      DAOS_IOD_ARRAY);
		rc = daos_seis_fetch_entry(trace_hdr_obj.oh, DAOS_TX_NONE,
					   &seismic_entry, NULL);
		if (rc != 0) {
			err("Fetching trace headers failed, error"
			    " code = %d \n", rc);
			exit(rc);
		}
		/** Close trace header object */
		daos_obj_close(trace_hdr_obj.oh, NULL);
		/** Write trace header object id */
		traces->traces[i].trace_header_obj = oids[i];
	}
}

void
fetch_traces_header_traces_list(daos_handle_t coh, daos_obj_id_t *oids,
				traces_list_t **head_traces, int daos_mode,
				int num_of_traces)
{
	seismic_entry_t 	seismic_entry = {0};
	trace_oid_oh_t		trace_hdr_obj;
	trace_t 	        temp_trace;
	int 			rc;
	int 			i;

	for (i = 0; i < num_of_traces; i++) {
		trace_hdr_obj.oid = oids[i];

		/** open trace header object */
		rc = daos_obj_open(coh, trace_hdr_obj.oid, daos_mode,
				   &trace_hdr_obj.oh, NULL);
		if (rc != 0) {
			err("Opening trace header object failed, error"
			    " code = %d \n", rc);
			return;
		}
		/** Fetch Trace header */
		prepare_seismic_entry(&seismic_entry, trace_hdr_obj.oid,
				      DS_D_TRACE_HEADER, DS_A_TRACE_HEADER,
				      (char*)&temp_trace, HDRBYTES, DAOS_IOD_ARRAY);
		rc = daos_seis_fetch_entry(trace_hdr_obj.oh, DAOS_TX_NONE,
					   &seismic_entry, NULL);
		if (rc != 0) {
			err("Fetching trace headers failed, error"
			    " code = %d \n", rc);
			return;
		}
		/** close header object */
		daos_obj_close(trace_hdr_obj.oh, NULL);
		temp_trace.trace_header_obj = oids[i];
		add_trace_header(&temp_trace, head_traces);
	}
}

void
fetch_traces_data(daos_handle_t coh, traces_list_t **head_traces,
		  int daos_mode)
{
	daos_array_iod_t 	iod;
	seismic_entry_t 	seismic_entry = {0};
	traces_headers_t       *current;
	trace_oid_oh_t 		trace_data_obj;
	daos_obj_id_t		hdr_obj_id;
	daos_range_t 		rg;
	d_sg_list_t 		sgl;
	d_iov_t 		iov;
	int 			rc;
	int 			i;

	current = (*head_traces)->head;
	if (current == NULL) {
		warn("Linked list of traces headers is empty.\n");
		return;
	}

	while (current != NULL) {
		/** calculate trace data oid from header oid*/
		hdr_obj_id = current->trace.trace_header_obj;
		trace_data_obj.oid = get_tr_data_oid(&hdr_obj_id, OC_SX);
		/** open trace data object */
		rc = daos_array_open_with_attr(coh, (trace_data_obj).oid,
					       DAOS_TX_NONE, DAOS_OO_RW, 1,
					       200 * sizeof(float),
					       &(trace_data_obj.oh), NULL);
		if (rc != 0) {
			err("Opening trace data object with attr() failed, "
			    "error code = %d \n", rc);
			return;
		}
		/** set scatter gather list and io descriptor */
		sgl.sg_nr = 1;
		sgl.sg_nr_out = 0;
		current->trace.data = malloc(current->trace.ns *
					     sizeof(float));
		seismic_entry.data = (char*) current->trace.data;
		d_iov_set(&iov, (void*) (seismic_entry.data),
			  current->trace.ns * sizeof(float));
		sgl.sg_iovs = &iov;
		iod.arr_nr = 1;
		rg.rg_len = current->trace.ns * sizeof(float);
		rg.rg_idx = 0;
		iod.arr_rgs = &rg;
		/** Read trace data */
		rc = daos_array_read(trace_data_obj.oh, DAOS_TX_NONE,
				     &iod, &sgl, NULL);
		if (rc != 0) {
			err("Reading trace data failed, error"
			    " code = %d \n", rc);
			return;
		}
		/** close trace data object */
		rc = daos_array_close(trace_data_obj.oh, NULL);
		if(rc != 0){
			err("Closing trace data object failed, error code "
					"= %d \n", rc);
			return;
		}
		current = current->next_trace;
	}

}

void
sort_dkeys_list(long *values, int number_of_gathers, char **unique_keys,
		int direction)
{

	const char	*sep = "_";
	char 		*token;
	char	       **sorted_keys;
	int 		*positive;
	int 		 i;
	int 		 j = 0;
	char 		 temp_key[4096];
	long		 temp;

	positive = malloc(number_of_gathers * sizeof(int));
	sorted_keys = malloc(number_of_gathers * sizeof(char*));
	/** loop on array of keys
	 *  and set each key type (+) or (-)
	 *  and insert unique value in array of values to be sorted
	 */
	for (i = 0; i < number_of_gathers; i++) {
		strcpy(temp_key, unique_keys[j]);
		token = strtok(temp_key, sep);
		while (token != NULL) {
			token = strtok(NULL, sep);
			if (token == NULL) {
				break;
			}
			sorted_keys[j] = malloc((strlen(token) + 1) *
						sizeof(char));
			if (token[0] == '-') {
				positive[j] = 0;
				strcpy(sorted_keys[j], &token[1]);
			} else {
				positive[j] = 1;
				strcpy(sorted_keys[j], token);
			}
			values[j] = atol(sorted_keys[j]);
		}
		j++;
	}
	/** Sort array of values based on direction of sorting
	 *  (ascending or descending)
	 */
	if (direction == 1) {
		for (i = 0; i < number_of_gathers; i++) {
			for (j = 0; j < number_of_gathers - i - 1; j++) {
				if (values[j] > values[j + 1]) {
					temp = values[j];
					values[j] = values[j + 1];
					values[j + 1] = temp;
				}
			}
		}
	} else {
		for (i = 0; i < number_of_gathers; ++i) {
			for (j = i + 1; j < number_of_gathers; ++j) {
				if (values[i] < values[j]) {
					temp = values[i];
					values[i] = values[j];
					values[j] = temp;
				}
			}
		}
	}

	for(i = 0; i < number_of_gathers; i++){
		free(sorted_keys[i]);
	}
	free(sorted_keys);
	free(positive);

}

void
sort_headers(read_traces *gather_traces, char **sort_key, int *direction,
	     int number_of_keys)
{

	MergeSort(gather_traces->traces, 0,
		  gather_traces->number_of_traces - 1,
		  sort_key, direction, number_of_keys);
}

void
Merge(trace_t *arr, int low, int mid, int high, char **sort_key,
      int *direction, int num_of_keys)
{
	trace_t 	*temp;
	Value 		 val1;
	Value 		 val2;
	int 		 mergePos;
	int 		 leftPos;
	int 		 rightPos;
	int 		 z;

	temp = (trace_t*) malloc((high - low + 1) * sizeof(trace_t));
	mergePos = 0;
	leftPos = low;
	rightPos = mid + 1;
	z = 1;

	while (leftPos <= mid && rightPos <= high) {
		while (z <= num_of_keys) {
			/** Get header values of key of each traces*/
			get_header_value(arr[leftPos], sort_key[z], &val1);
			get_header_value(arr[rightPos], sort_key[z], &val2);
			/** Compare values of trace headers*/
			if (valcmp(hdtype(sort_key[z]), val1, val2) == -1) {
				if (direction[z] == 1) {
					temp[mergePos++] = arr[leftPos++];
				} else {
					temp[mergePos++] = arr[rightPos++];
				}
				break;
			} else if (valcmp(hdtype(sort_key[z]), val1, val2)
				    == 1) {
				if (direction[z] == 1) {
					temp[mergePos++] = arr[rightPos++];
				} else {
					temp[mergePos++] = arr[leftPos++];
				}
				break;
			} else {
				z++;
			}
		}
	}

	while (leftPos <= mid) {
		temp[mergePos++] = arr[leftPos++];
	}

	while (rightPos <= high) {
		temp[mergePos++] = arr[rightPos++];
	}

	for (mergePos = 0; mergePos < (high - low + 1); ++mergePos) {
		arr[low + mergePos] = temp[mergePos];
	}

	free(temp);
}

void
MergeSort(trace_t *arr, int low, int high, char **sort_key, int *direction,
	  int num_of_keys)
{
	int		mid;
	if (low < high) {
		mid = (low + high) / 2;

		MergeSort(arr, low, mid, sort_key, direction, num_of_keys);
		MergeSort(arr, mid + 1, high, sort_key, direction,
			  num_of_keys);

		Merge(arr, low, mid, high, sort_key, direction,
		      num_of_keys);
	}
}

void
get_header_value(trace_t trace, char *sort_key, Value *value)
{

	if (strcmp(sort_key, "tracl") == 0) {
		value->i = trace.tracl;
	} else if (strcmp(sort_key, "tracr") == 0) {
		value->i = trace.tracr;
	} else if (strcmp(sort_key, "fldr") == 0) {
		value->i = trace.fldr;
	} else if (strcmp(sort_key, "tracf") == 0) {
		value->i = trace.tracf;
	} else if (strcmp(sort_key, "ep") == 0) {
		value->i = trace.ep;
	} else if (strcmp(sort_key, "cdp") == 0) {
		value->i = trace.cdp;
	} else if (strcmp(sort_key, "ns") == 0) {
		value->u = trace.ns;
	} else if (strcmp(sort_key, "gx") == 0) {
		value->i = trace.gx;
	} else if (strcmp(sort_key, "sx") == 0) {
		value->i = trace.sx;
	} else if (strcmp(sort_key, "offset") == 0) {
		value->i = trace.offset;
	} else if (strcmp(sort_key, "dt") == 0) {
		value->u = trace.dt;
	} else {
		return;
	}

}

void
set_header_value(trace_t *trace, char *sort_key, Value *value)
{
	if (strcmp(sort_key, "tracl") == 0) {
		trace->tracl = value->i;
	} else if (strcmp(sort_key, "tracr") == 0) {
		trace->tracr = value->i;
	} else if (strcmp(sort_key, "fldr") == 0) {
		trace->fldr = value->i;
	} else if (strcmp(sort_key, "tracf") == 0) {
		trace->tracf = value->i;
	} else if (strcmp(sort_key, "ep") == 0) {
		trace->ep = value->i;
	} else if (strcmp(sort_key, "cdp") == 0) {
		trace->cdp = value->i;
	} else if (strcmp(sort_key, "ns") == 0) {
		trace->ns = value->u;
	} else if (strcmp(sort_key, "gx") == 0) {
		trace->gx = value->i;
	} else if (strcmp(sort_key, "dt") == 0) {
		trace->dt = value->u;
	} else {
		return;
	}
//	} else if(strcmp(sort_key,"cdpt")==0){
//		trace.cdpt = value;
//	} else if(strcmp(sort_key,"nvs")==0){
//		trace.nvs = value;
//	} else if(strcmp(sort_key,"nhs")==0){
//		trace.nhs = value;
//	} else if(strcmp(sort_key,"offset")==0){
//		trace.offset = value;
//	} else if(strcmp(sort_key,"gelev")==0){
//		trace.gelev = value;
//	} else if(strcmp(sort_key,"selev")==0){
//		trace.selev = value;
//	} else if(strcmp(sort_key,"sdepth")==0){
//		trace.sdepth = value;
//	} else if(strcmp(sort_key,"gdel")==0){
//		trace.gdel = value;
//	} else if(strcmp(sort_key,"sdel")==0){
//		trace.sdel = value;
//	} else if(strcmp(sort_key,"swdep")==0){
//		trace.swdep = value;
//	} else if(strcmp(sort_key,"gwdep")==0){
//		trace.gwdep = value;
//	} else if(strcmp(sort_key,"scalel")==0){
//		trace.scalel = value;
//	} else if(strcmp(sort_key,"scalco")==0){
//		trace.scalco = value;
//	} else if(strcmp(sort_key,"sx")==0){
//		trace.sx = value;
//	} else if(strcmp(sort_key,"sy")==0){
//		trace.sy = value;
//	} else if(strcmp(sort_key,"gx")==0){
//		trace.gx = value;
//	} else if(strcmp(sort_key,"gy")==0){
//		trace.gy = value;
//	} else if(strcmp(sort_key,"wevel")==0){
//		trace.wevel = value;
//	} else if(strcmp(sort_key,"swevel")==0){
//		trace.swevel = value;
//	} else if(strcmp(sort_key,"sut")==0){
//		trace.sut = value;
//	} else if(strcmp(sort_key,"gut")==0){
//		trace.gut = value;
//	} else if(strcmp(sort_key,"sstat")==0){
//		trace.sstat = value;
//	} else if(strcmp(sort_key,"gstat")==0){
//		trace.gstat = value;
//	} else if(strcmp(sort_key,"tstat")==0){
//		trace.tstat = value;
//	} else if(strcmp(sort_key,"laga")==0){
//		trace.laga = value;
//	} else if(strcmp(sort_key,"lagb")==0){
//		trace.lagb = value;
//	} else if(strcmp(sort_key,"delrt")==0){
//		trace.delrt = value;
//	} else if(strcmp(sort_key,"muts")==0){
//		trace.muts = value;
//	} else if(strcmp(sort_key,"mute")==0){
//		trace.mute = value;
//	} else if(strcmp(sort_key,"ns")==0){
//		trace.ns = value;
//	} else if(strcmp(sort_key,"dt")==0){
//		trace.dt = value;
//	} else if(strcmp(sort_key,"gain")==0){
//		trace.gain = value;
//	} else if(strcmp(sort_key,"igc")==0){
//		trace.igc = value;
//	} else if(strcmp(sort_key,"igi")==0){
//		trace.igi = value;
//	} else if(strcmp(sort_key,"corr")==0){
//		trace.corr = value;
//	} else if(strcmp(sort_key,"sfs")==0){
//		trace.sfs = value;
//	} else if(strcmp(sort_key,"sfe")==0){
//		trace.sfe = value;
//	} else if(strcmp(sort_key,"slen")==0){
//		trace.slen = value;
//	} else if(strcmp(sort_key,"styp")==0){
//		trace.styp = value;
//	} else if(strcmp(sort_key,"stas")==0){
//		trace.stas = value;
//	} else if(strcmp(sort_key,"stae")==0){
//		trace.stae = value;
//	} else if(strcmp(sort_key,"tatyp")==0){
//		trace.tatyp = value;
//	} else if(strcmp(sort_key,"afilf")==0){
//		trace.afilf = value;
//	} else if(strcmp(sort_key,"afils")==0){
//		trace.afils = value;
//	} else if(strcmp(sort_key,"nofilf")==0){
//		trace.nofilf = value;
//	} else if(strcmp(sort_key,"nofils")==0){
//		trace.nofils = value;
//	} else if(strcmp(sort_key,"lcf")==0){
//		trace.lcf = value;
//	} else if(strcmp(sort_key,"hcf")==0){
//		trace.hcf = value;
//	} else if(strcmp(sort_key,"lcs")==0){
//		trace.lcs = value;
//	} else if(strcmp(sort_key,"hcs")==0){
//		trace.hcs = value;
//	} else if(strcmp(sort_key,"grnors")==0){
//		trace.grnors = value;
//	} else if(strcmp(sort_key,"grnofr")==0){
//		trace.grnofr = value;
//	} else if(strcmp(sort_key,"grnlof")==0){
//		trace.grnlof = value;
//	} else if(strcmp(sort_key,"gaps")==0){
//		trace.gaps = value;
//	} else if(strcmp(sort_key,"d1")==0){
//		trace.d1 = value;
//	} else if(strcmp(sort_key,"f1")==0){
//		trace.f1 = value;
//	} else if(strcmp(sort_key,"d2")==0){
//		trace.d2 = value;
//	} else if(strcmp(sort_key,"f2")==0){
//		trace.f2 = value;
//	} else if(strcmp(sort_key,"sfs")==0){
//		trace.sfs = value;
//	} else if(strcmp(sort_key,"ntr")==0){
//		trace.ntr = value;
//	} else {
//		return;
//	}
}

char*
get_dkey(char *key)
{

	if (strcmp(key, "tracl") == 0) {
		return "Tracl";
	} else if (strcmp(key, "tracr") == 0) {
		return "Tracr";
	} else if (strcmp(key, "fldr") == 0) {
		return "Fldr";
	} else if (strcmp(key, "tracf") == 0) {
		return "Tracf";
	} else if (strcmp(key, "ep") == 0) {
		return "Ep";
	} else if (strcmp(key, "cdp") == 0) {
		return "Cdp";
	} else if (strcmp(key, "cdpt") == 0) {
		return "Cdpt";
	} else if (strcmp(key, "nvs") == 0) {
		return "Nvs";
	} else if (strcmp(key, "nhs") == 0) {
		return "Nhs";
	} else if (strcmp(key, "offset") == 0) {
		return "Offset";
//	} else if (strcmp(key, "gelev") == 0) {
//		return "Gelev_";
//	} else if (strcmp(key, "selev") == 0) {
//		return "Selev_";
//	} else if (strcmp(key, "sdepth") == 0) {
//		return "Sdepth_";
//	} else if (strcmp(key, "gdel") == 0) {
//		return "Gdel_";
//	} else if (strcmp(key, "sdel") == 0) {
//		return "Sdel_";
//	} else if (strcmp(key, "swdep") == 0) {
//		return "Swdep_";
//	} else if (strcmp(key, "gwdep") == 0) {
//		return "Gwdep_";
//	} else if (strcmp(key, "scalel") == 0) {
//		return "Scalel_";
//	} else if (strcmp(key, "scalco") == 0) {
//		return "Scalco_";
//	} else if (strcmp(key, "sx") == 0) {
//		return "Sx_";
//	} else if (strcmp(key, "sy") == 0) {
//		return "Sy_";
//	} else if (strcmp(key, "gx") == 0) {
//		return "Gx_";
//	} else if (strcmp(key, "gy") == 0) {
//		return "Gy_";
//	} else if (strcmp(key, "wevel") == 0) {
//		return "Wevel_";
//	} else if (strcmp(key, "swevel") == 0) {
//		return "Swevel_";
//	} else if (strcmp(key, "sut") == 0) {
//		return "Sut_";
//	} else if (strcmp(key, "gut") == 0) {
//		return "Gut_";
//	} else if (strcmp(key, "sstat") == 0) {
//		return "Sstat_";
//	} else if (strcmp(key, "gstat") == 0) {
//		return "Gstat_";
//	} else if (strcmp(key, "tstat") == 0) {
//		return "Tstat_";
//	} else if (strcmp(key, "laga") == 0) {
//		return "Laga_";
//	} else if (strcmp(key, "lagb") == 0) {
//		return "Lagb_";
//	} else if (strcmp(key, "delrt") == 0) {
//		return "Delrt_";
//	} else if (strcmp(key, "muts") == 0) {
//		return "Muts_";
//	} else if (strcmp(key, "mute") == 0) {
//		return "Mute_";
//	} else if (strcmp(key, "ns") == 0) {
//		return "Ns_";
//	} else if (strcmp(key, "dt") == 0) {
//		return "Dt_";
//	} else if (strcmp(key, "gain") == 0) {
//		return "Gain_";
//	} else if (strcmp(key, "igc") == 0) {
//		return "Igc_";
//	} else if (strcmp(key, "igi") == 0) {
//		return "Igi_";
//	} else if (strcmp(key, "corr") == 0) {
//		return "Corr_";
//	} else if (strcmp(key, "sfs") == 0) {
//		return "Sfs_";
//	} else if (strcmp(key, "sfe") == 0) {
//		return "Sfe_";
//	} else if (strcmp(key, "slen") == 0) {
//		return "Slen_";
//	} else if (strcmp(key, "styp") == 0) {
//		return "Styp_";
//	} else if (strcmp(key, "stas") == 0) {
//		return "Stas_";
//	} else if (strcmp(key, "stae") == 0) {
//		return "Stae_";
//	} else if (strcmp(key, "tatyp") == 0) {
//		return "Tatyp_";
//	} else if (strcmp(key, "afilf") == 0) {
//		return "Afilf_";
//	} else if (strcmp(key, "afils") == 0) {
//		return "Afils_";
//	} else if (strcmp(key, "nofilf") == 0) {
//		return "Nofilf_";
//	} else if (strcmp(key, "nofils") == 0) {
//		return "Nofils_";
//	} else if (strcmp(key, "lcf") == 0) {
//		return "Lcf_";
//	} else if (strcmp(key, "hcf") == 0) {
//		return "Hcf_";
//	} else if (strcmp(key, "lcs") == 0) {
//		return "Lcs_";
//	} else if (strcmp(key, "hcs") == 0) {
//		return "Hcs_";
//	} else if (strcmp(key, "grnors") == 0) {
//		return "Grnors_";
//	} else if (strcmp(key, "grnofr") == 0) {
//		return "Grnofr_";
//	} else if (strcmp(key, "grnlof") == 0) {
//		return "Grnlof_";
//	} else if (strcmp(key, "gaps") == 0) {
//		return "Gaps_";
//	} else if (strcmp(key, "d1") == 0) {
//		return "D1_";
//	} else if (strcmp(key, "f1") == 0) {
//		return "F1_";
//	} else if (strcmp(key, "d2") == 0) {
//		return "D2_";
//	} else if (strcmp(key, "f2") == 0) {
//		return "F2_";
//	} else if (strcmp(key, "sfs") == 0) {
//		return "Sfs_";
//	} else if (strcmp(key, "ntr") == 0) {
//		return "Ntr_";
	} else {
		return -1;
	}
}

void
set_traces_header(daos_handle_t coh, int daos_mode, traces_list_t **head,
		  int num_of_keys, char **keys_1, char **keys_2, char **keys_3,
		  double *a, double *b, double *c, double *d, double *e,
		  double *f, double *j, header_operation_type_t type)
{
	traces_headers_t 	*current;
	trace_oid_oh_t 		 trace_hdr_obj;
	cwp_String 		 type_key1[num_of_keys];
	cwp_String 		 type_key2[num_of_keys];
	cwp_String 		 type_key3[num_of_keys];
	int 			 itr;
	int 			 rc;
	int 			 i;

	current = (*head)->head;

	if (current == NULL) {
		warn("linked list of traces is empty\n");
		return;
	} else {
		itr = 0;
		while (current != NULL) {
			trace_hdr_obj.oid =
					current->trace.trace_header_obj;
			rc = daos_obj_open(coh, trace_hdr_obj.oid,
					   daos_mode, &(trace_hdr_obj.oh),
					   NULL);
			if (rc != 0) {
				err("Opening trace header object failed, error"
				    " code = %d \n", rc);
				return;
			}
			for (i = 0; i < num_of_keys; i++) {
				type_key1[i] = hdtype(keys_1[i]);
				if (type == 0) {
					calculate_new_header_value(current,
								   keys_1[i],
								   NULL, NULL,
								   a[i], b[i],
								   c[i], d[i],
								   0, 0, j[i],
								   itr, type,
								   type_key1[i]
								   , NULL, NULL
								   );
				} else {
					type_key2[i] = hdtype(keys_2[i]);
					type_key3[i] = hdtype(keys_3[i]);
					calculate_new_header_value(current,
								   keys_1[i],
								   keys_2[i],
								   keys_3[i],
								   a[i], b[i],
								   c[i], d[i],
								   e[i], f[i],
								   0, 0, type,
								   type_key1[i]
								   ,type_key2[i],
								   type_key3[i]
								   );
				}
			}
			rc = daos_seis_trh_update(&trace_hdr_obj,
						      &(current->trace),
						      HDRBYTES);
			if (rc != 0) {
				err("Updating trace header failed error"
				    " code = %d \n", rc);
				return;
			}
			rc = daos_obj_close(trace_hdr_obj.oh, NULL);
			if(rc !=0) {
				err("Closing trace header object failed"
				    " error code = %d \n", rc);
				return;
			}
			itr++;
			current = current->next_trace;
		}
	}
}

void
calculate_new_header_value(traces_headers_t *current, char *key1, char *key2,
			   char *key3, double a, double b, double c, double d,
			   double e, double f, double j, int itr,
			   header_operation_type_t type,
			   cwp_String type_key1, cwp_String type_key2,
			   cwp_String type_key3)
{
	Value 		val1;
	Value 		val2;
	Value 		val3;
	double 		i;
	long 		temp;

	switch (type) {
	case SET_HEADERS:
		i = (double) itr + d;
		setval(type_key1, &val1, a, b, c, i, j);
		set_header_value(&(current->trace), key1, &val1);
		break;
	case CHANGE_HEADERS:
		get_header_value(current->trace, key2, &val2);
		get_header_value(current->trace, key3, &val3);
		changeval(type_key1, &val1, type_key2,
			  &val2, type_key3, &val3, a, b, c, d,
			  e, f);
		set_header_value(&(current->trace), key1, &val1);
		break;
	default:
		break;
	}
}

void
window_headers(traces_list_t **head, char **window_keys,
	       int number_of_keys, cwp_String *type,
	       Value *min_keys, Value *max_keys)
{
	traces_headers_t 	*current;
	traces_headers_t 	*previous;
	Value 			 val;
	int 			 i;
	int 			 break_loop;

	current = (*head)->head;
	previous = NULL;

	if (current == NULL) {
		warn("Linked list of traces headers is empty.\n");
		return;
	}
	while (current != NULL) {
		break_loop = 0;
		for (i = 0; i < number_of_keys && break_loop == 0; i++) {
			/** get the trace header value */
			get_header_value(current->trace, window_keys[i], &val);
			/** check the value of trace header if it falls
			 *  within the min and max values or not
			 *  if yes the continue to check the value
			 *  of trace header for the next key
			 *  else, delete the trace header from
			 *  the linked list of headers.			 *
			 */
			if (!(valcmp(type[i], val, min_keys[i]) == 1 ||
			      valcmp(type[i], val, min_keys[i]) == 0) ||
			    !(valcmp(type[i], val, max_keys[i]) == -1 ||
			      valcmp(type[i], val, max_keys[i]) == 0)) {
				if (current == (*head)->head) {
					(*head)->head =
						(*head)->head->next_trace;
					free(current);
					current = (*head)->head;
				} else {
					previous->next_trace =
							current->next_trace;
					free(current);
					current = previous->next_trace;
				}
				/** if the trace header value doesn't
				 *  fall in the range of min and max values
				 *  then break the loop.
				 *  otherwise, move to the next node.
				 */
				break_loop = 1;
			}
		}
		if (break_loop == 1) {
			continue;
		} else {
			previous = current;
			current = current->next_trace;
		}
	}
}

char**
daos_seis_fetch_dkeys(seis_obj_t *seismic_object, int sort, char *key,
		      int direction)
{
	daos_key_desc_t 	*kds;
	daos_anchor_t 		 anchor = {0};
	d_sg_list_t 		 sglo;
	d_iov_t 		 iov_temp;
	uint32_t 		 nr;
	char 			*temp_array;
	char 		       **dkeys_list;
	char 		       **unique_keys;
	int 			 temp_array_offset = 0;
	int 			 keys_read = 0;
	int 			 kds_i = 0;
	int 			 out;
	int 			 rc;
	int 			 z;
	/** temp arrays allocations */
	nr = seismic_object->number_of_gathers + 1;

	temp_array = malloc(nr * 2000 *
			    sizeof(char));
	kds = malloc((nr) * sizeof(daos_key_desc_t));
	dkeys_list = malloc((nr) * sizeof(char*));
	unique_keys = malloc(seismic_object->number_of_gathers * sizeof(char*));
	sglo.sg_nr_out = sglo.sg_nr = 1;	
	sglo.sg_iovs = &iov_temp;

	/** fetch list of dkeys */
	while (!daos_anchor_is_eof(&anchor)) {
		nr = seismic_object->number_of_gathers + 1 - keys_read;
		d_iov_set(&iov_temp, temp_array + temp_array_offset,
			  nr * 2000);
		rc = daos_obj_list_dkey(seismic_object->oh, DAOS_TX_NONE, &nr,
					&kds[keys_read], &sglo, &anchor, NULL);
		for (kds_i = 0; kds_i < nr; kds_i++) {
			temp_array_offset += kds[keys_read + kds_i].kd_key_len;
		}
		keys_read += nr;
		
	}
	if (rc != 0) {
		err("Listing <%s> seismic object dkeys failed,"
		    " error code = %d\n", seismic_object->name, rc);
	}
	/** Copy dkeys from temp array to dkeys list
	 * then check if the key contain digits or not
	 * if yes, key is copied to array of unique keys(gather keys)
	 * otherwise it is ignored.
	 */
	int 			 digit;
	int			 off = 0;
	int 			 u = 0;
	int			 k;

	for (z = 0; z < keys_read; z++) {
		digit = 0;
		dkeys_list[z] = malloc((kds[z].kd_key_len + 1) * sizeof(char));
		strncpy(dkeys_list[z], &temp_array[off], kds[z].kd_key_len);
		dkeys_list[z][kds[z].kd_key_len] = '\0';
		off += kds[z].kd_key_len;
		for (k = 0; k < strlen(dkeys_list[z]) + 1; k++) {
			if (isdigit(dkeys_list[z][k])) {
				digit = 1;
				unique_keys[u] = malloc(kds[z].kd_key_len *
							sizeof(char));
				strcpy(unique_keys[u], dkeys_list[z]);
				u++;
				break;
			}
		}
		if(digit == 0) {
			out = z;
		}
	}
	/** check sorting flag, if yes then sort dkeys fetched
	 *  based on direction(ascending or descending
	 *  and return the sorted list
	 *  otherwise return the array of unique keys as it is.
	 */
	if (sort == 1) {
		char 	**dkeys_sorted_list;
		long	 *first_array;

		first_array = malloc(seismic_object->number_of_gathers
				     * sizeof(long));
		dkeys_sorted_list = malloc(seismic_object->number_of_gathers
					   * sizeof(char*));

		sort_dkeys_list(first_array, seismic_object->number_of_gathers,
				unique_keys, direction);

		k = 0;
		for (z = 0; z < seismic_object->number_of_gathers; z++) {
			if (k == out) {
				k++;
			}
			dkeys_sorted_list[z] = malloc(kds[k].kd_key_len *
						      sizeof(char));
			char dkey_name[200] = "";
			char temp_st[200] = "";
			strcat(dkey_name, get_dkey(key));
			strcat(dkey_name, "_");
			sprintf(temp_st, "%ld", first_array[z]);
			strcat(dkey_name, temp_st);
			strcpy(dkeys_sorted_list[z], dkey_name);
			k++;
		}	
		free(first_array);
		return dkeys_sorted_list;
	}

	/** free allocated memory */
	for(z=0; z<seismic_object->number_of_gathers +1; z++) {
		free(dkeys_list[z]);
	}
	free(temp_array);
	free(kds);
	free(dkeys_list);

	return unique_keys;
}

void
daos_seis_replace_objects(dfs_t *dfs, int daos_mode, char *key,
			  traces_list_t *trace_list, seis_root_obj_t *root)
{
	seismic_entry_t 	seismic_entry = {0};
	seis_obj_t 	       *new_seis_obj;
	seis_obj_t 	       *existing_obj;
	int 			index;
	int 			rc;
	int 			i;

	existing_obj = malloc(sizeof(seis_obj_t));

	for(i = 0; i < root->num_of_keys; i++) {
		if(strcmp(root->keys[i],key) == 0) {
			existing_obj->oid = root->gather_oids[i];
			index = i;
			break;
		}
	}

	rc = daos_obj_open(dfs->coh, existing_obj->oid, daos_mode,
			&(existing_obj->oh), NULL);
	if (rc != 0) {
		err("Opening seismic object failed error code = %d \n", rc);
		return;
	}
	/** Fetch Number of Gathers Under opened Gather object */
	prepare_seismic_entry(&seismic_entry, existing_obj->oid, DS_D_NGATHERS,
			      DS_A_NGATHERS,
			      (char*) &(existing_obj->number_of_gathers),
			      sizeof(int), DAOS_IOD_SINGLE);
	rc = daos_seis_fetch_entry(existing_obj->oh, DAOS_TX_NONE,
				   &seismic_entry, NULL);
	if (rc != 0) {
		err("Fetching number of gathers failed, error "
		    "code = %d \n", rc);
		return;
	}
	char **temp_keys;
	temp_keys = daos_seis_fetch_dkeys(existing_obj, 0, key, 1);

	/** Destroy all trace headers oids objects in existing object */
	for (i = 0; i < existing_obj->number_of_gathers; i++) {
		trace_oid_oh_t 		temp;

		prepare_seismic_entry(&seismic_entry, existing_obj->oid,
				      temp_keys[i], DS_A_GATHER_TRACE_OIDS,
				      (char*) &temp.oid,
				      sizeof(daos_obj_id_t), DAOS_IOD_SINGLE);
		rc = daos_seis_fetch_entry(existing_obj->oh, DAOS_TX_NONE,
					   &seismic_entry, NULL);
		rc = daos_array_open_with_attr(dfs->coh, temp.oid,
					       DAOS_TX_NONE, DAOS_OO_RW, 1,
					       500 * sizeof(daos_obj_id_t),
					       &(temp.oh), NULL);
		if (rc != 0) {
			err("Opening array object with attr() failed, error"
			    " code = %d \n", rc);
			return;
		}
		rc = daos_array_destroy(temp.oh, DAOS_TX_NONE, NULL);
		if (rc != 0) {
			err("Destroying array object failed, "
			    "error code = %d \n", rc);
			return;
		}
		rc = daos_array_close(temp.oh, NULL);
		if (rc != 0) {
			err("Closing array object failed, "
			    "error code = %d \n", rc);
			return;
		}
		free(temp_keys[i]);
	}
	free(temp_keys);
	/** Punch exisiting object */
	rc = daos_obj_punch(existing_obj->oh, DAOS_TX_NONE, 0, NULL);
	if (rc != 0) {
		err("Punching existing seismic object failed, "
		    "error code = %d \n", rc);
		return;
	}
	/** Create new seismic object based on key type*/
	rc = daos_seis_gather_obj_create(dfs, OC_SX, root,
					 &new_seis_obj, key, index);

	if (rc != 0) {
		err("Creating new seismic object failed, "
		    "error code = %d \n", rc);
		return;
	}
	/** Start linking trace list to the created gather object */
	traces_headers_t 	*current = trace_list->head;
	while (current != NULL) {
		trace_obj_t *trace_obj = malloc(sizeof(trace_obj_t));
		trace_obj->trace = malloc(sizeof(trace_t));

		memcpy(trace_obj->trace, &(current->trace), sizeof(trace_t));

		trace_obj->oid = current->trace.trace_header_obj;

		rc = daos_seis_tr_linking(trace_obj, new_seis_obj,
					  root->keys[index]);
		if (rc != 0) {
			err("Linking trace to <%s> gather object failed,"
			    " error code = %d \n",root->keys[index], rc);
			return;
		}

		current = current->next_trace;
		free(trace_obj->trace);
		free(trace_obj);
	}
	/** Update new object number of gathers key */
	rc = update_gather_object(new_seis_obj, DS_D_NGATHERS, DS_A_NGATHERS,
			(char*) &new_seis_obj->number_of_gathers, sizeof(int),
			DAOS_IOD_SINGLE);
	if (rc != 0) {
		err("Adding number of gathers failed, "
		    "error code = %d \n", rc);
		return;
	}
	/** Create new trace oids object */
	rc = daos_seis_trace_oids_obj_create(dfs, OC_SX, new_seis_obj);
	if (rc != 0) {
		err("Creating new trace oids object failed, "
		    "error code = %d \n", rc);
		return;
	}
	/** Update gather keys */
	rc = update_gather_traces(dfs, new_seis_obj->gathers,
				  new_seis_obj, get_dkey(root->keys[index]),
				  DS_A_NTRACES);
	if (rc != 0) {
		err("Updating gather keys failed, "
		    "error code = %d \n", rc);
		return;
	}
	/** close new object */
	daos_obj_close(new_seis_obj->oh, NULL);
	free(existing_obj);
}

void
tokenize_str(void **str, char *sep, char *string, int type)
{
	double 	       *temp_d;
	char 		temp[4096];
	char	      **temp_c;
	char 	       *ptr;
	long 	       *temp_l;
	int 		i = 0;

	strcpy(temp, string);
	char *token = strtok(temp, sep);


	while (token != NULL) {
		switch (type) {
		case 0:
			temp_c = (char**) str;
			temp_c[i] = malloc((strlen(token) + 1) * sizeof(char));
			strcpy(temp_c[i], token);
			break;
		case 1:
			temp_l = *((long**) str);
			temp_l[i] = atol(token);
			break;
		case 2:
			temp_d = *((double**) str);
			temp_d[i] = strtod(token, &ptr);
			break;
		default:
			printf("ERROR\n");
			exit(0);
		}
		i++;
		token = strtok(NULL, sep);
	}
}

headers_ranges_t
range_traces_headers(traces_list_t *trace_list, int number_of_keys,
		     char **keys, int dim)
{
	traces_headers_t 	*current;
	headers_ranges_t	headers_ranges;
	trace_t 		*trmin;
	trace_t 		*trmax;
	trace_t 		*trfirst;
	trace_t 		*trlast;
	double 			east_shot[2];
	double 			west_shot[2];
	double			north_shot[2];
	double			south_shot[2];
	double 			east_rec[2];
	double			west_rec[2];
	double			north_rec[2];
	double			south_rec[2];
	double 			east_cmp[2];
	double			west_cmp[2];
	double			north_cmp[2];
	double			south_cmp[2];
	double 			dcoscal = 1.0;
	double 			sx = 0.0;
	double			sy = 0.0;
	double			gx = 0.0;
	double			gy = 0.0;
	double			mx = 0.0;
	double			my = 0.0;
	double 			mx1 = 0.0;
	double			my1 = 0.0;
	double 			mx2 = 0.0;
	double			my2 = 0.0;
	double			dm = 0.0;
	double			dmin = 0.0;
	double			dmax = 0.0;
	double			davg = 0.0;
	int 			coscal = 1;
	Value 			 val;
	Value 			 valmin;
	Value 			 valmax;
	int 			 i;

	current = trace_list->head;
	trmin = malloc(sizeof(trace_t));
	trmax = malloc(sizeof(trace_t));
	trfirst = malloc(sizeof(trace_t));
	trlast = malloc(sizeof(trace_t));

	north_shot[0] = south_shot[0] = east_shot[0] = west_shot[0] = 0.0;
	north_shot[1] = south_shot[1] = east_shot[1] = west_shot[1] = 0.0;
	north_rec[0] = south_rec[0] = east_rec[0] = west_rec[0] = 0.0;
	north_rec[1] = south_rec[1] = east_rec[1] = west_rec[1] = 0.0;
	north_cmp[0] = south_cmp[0] = east_cmp[0] = west_cmp[0] = 0.0;
	north_cmp[1] = south_cmp[1] = east_cmp[1] = west_cmp[1] = 0.0;

	if (number_of_keys == 0) {
		for (i = 0; i < SU_NKEYS; i++) {
			get_header_value(current->trace, keys[i], &val);
			set_header_value(trmin, keys[i], &val);
			set_header_value(trmax, keys[i], &val);
			set_header_value(trfirst, keys[i], &val);

			if (i == 20) {
				coscal = val.h;
				if (coscal == 0) {
					coscal = 1;
				} else if (coscal > 0) {
					dcoscal = 1.0 * coscal;
				} else {
					dcoscal = 1.0 / coscal;
				}
			} else if (i == 21) {
				sx = east_shot[0] = west_shot[0] = north_shot[0] =
						south_shot[0] = val.i * dcoscal;
			} else if (i == 22) {
				sy = east_shot[1] = west_shot[1] = north_shot[1] =
						south_shot[1] = val.i * dcoscal;
			} else if (i == 23) {
				gx = east_rec[0] = west_rec[0] = north_rec[0] =
						south_rec[0] = val.i * dcoscal;
			} else if (i == 24) {
				gy = east_rec[1] = west_rec[1] = north_rec[1] =
						south_rec[1] = val.i * dcoscal;
			} else {
				continue;
			}
		}
	} else {
		for (i = 0; i < number_of_keys; i++) {
			get_header_value(current->trace, keys[i], &val);
			set_header_value(trmin, keys[i], &val);
			set_header_value(trmax, keys[i], &val);
			set_header_value(trfirst, keys[i], &val);
		}
	}
	if (number_of_keys == 0) {
		mx = east_cmp[0] = west_cmp[0] = north_cmp[0] = south_cmp[0] = 0.5 *
				  (east_shot[0] + east_rec[0]);
		my = east_cmp[1] = west_cmp[1] = north_cmp[1] = south_cmp[1] = 0.5 *
				  (east_shot[1] + east_rec[1]);
	}

	int 		ntr = 1;
	current = current->next_trace;

	while (current != NULL) {
		sx = sy = gx = gy = mx = my = 0.0;
		if (number_of_keys == 0) {
			for (i = 0; i < SU_NKEYS; i++) {
				get_header_value(current->trace, keys[i],
						 &val);
				get_header_value(*trmin, keys[i], &valmin);
				get_header_value(*trmax, keys[i], &valmax);

				if (valcmp(hdr[i].type, val, valmin) < 0) {
					set_header_value(trmin, keys[i], &val);
				} else if (valcmp(hdr[i].type, val, valmax) >
					   0) {
					set_header_value(trmax, keys[i], &val);
				}

				set_header_value(trlast, keys[i], &val);

				if (i == 20) {
					coscal = val.h;
					if (coscal == 0) {
						coscal = 1;
					} else if (coscal > 0) {
						dcoscal = 1.0 * coscal;
					} else {
						dcoscal = 1.0 / coscal;
					}
				} else if (i == 21) {
					sx = val.i * dcoscal;
				} else if (i == 22) {
					sy = val.i * dcoscal;
				} else if (i == 23) {
					gx = val.i * dcoscal;
				} else if (i == 24) {
					gy = val.i * dcoscal;
				} else {
					continue;
				}
			}
		} else {
			for (i = 0; i < number_of_keys; i++) {
				get_header_value(current->trace, keys[i],
						 &val);
				get_header_value(*trmin, keys[i], &valmin);
				get_header_value(*trmax, keys[i], &valmax);
				if (valcmp(hdtype(keys[i]), val, valmin) < 0) {
					set_header_value(trmin, keys[i], &val);
				} else if (valcmp(hdtype(keys[i]), val, valmax)
						> 0) {
					set_header_value(trmax, keys[i], &val);
				}
				set_header_value(trlast, keys[i], &val);
			}
		}

		if (number_of_keys == 0) {
			mx = 0.5 * (sx + gx);
			my = 0.5 * (sy + gy);
			if (east_shot[0] < sx) {
				east_shot[0] = sx;
				east_shot[1] = sy;
			}
			if (west_shot[0] > sx) {
				west_shot[0] = sx;
				west_shot[1] = sy;
			}
			if (north_shot[1] < sy) {
				north_shot[0] = sx;
				north_shot[1] = sy;
			}
			if (south_shot[1] > sy) {
				south_shot[0] = sx;
				south_shot[1] = sy;
			}
			if (east_rec[0] < gx) {
				east_rec[0] = gx;
				east_rec[1] = gy;
			}
			if (west_rec[0] > gx) {
				west_rec[0] = gx;
				west_rec[1] = gy;
			}
			if (north_rec[1] < gy) {
				north_rec[0] = gx;
				north_rec[1] = gy;
			}
			if (south_rec[1] > gy) {
				south_rec[0] = gx;
				south_rec[1] = gy;
			}
			if (east_cmp[0] < mx) {
				east_cmp[0] = mx;
				east_cmp[1] = my;
			}
			if (west_cmp[0] > mx) {
				west_cmp[0] = mx;
				west_cmp[1] = my;
			}
			if (north_cmp[1] < my) {
				north_cmp[0] = mx;
				north_cmp[1] = my;
			}
			if (south_cmp[1] > my) {
				south_cmp[0] = mx;
				south_cmp[1] = my;
			}
		}

		if (ntr == 1) {
			/** get midpoint (mx1,my1) on trace 1 */
			mx1 = 0.5 * (current->trace.sx + current->trace.gx);
			my1 = 0.5 * (current->trace.sy + current->trace.gy);
		} else if (ntr == 2) {
			/** get midpoint (mx2,my2) on trace 2 */
			mx2 = 0.5 * (current->trace.sx + current->trace.gx);
			my2 = 0.5 * (current->trace.sy + current->trace.gy);
			/** midpoint interval between traces 1 and 2 */
			dm = sqrt((mx1 - mx2) * (mx1 - mx2) +
				  (my1 - my2) * (my1 - my2));
			/** set min, max and avg midpoint interval holders */
			dmin = dm;
			dmax = dm;
			davg = (dmin + dmax) / 2.0;
			/* hold this midpoint */
			mx1 = mx2;
			my1 = my2;
		} else if (ntr > 2) {
			/** get midpoint (mx,my) on this trace */
			mx2 = 0.5 * (current->trace.sx + current->trace.gx);
			my2 = 0.5 * (current->trace.sy + current->trace.gy);
			/** get midpoint (mx,my) between this
			 * and previous trace
			 */
			dm = sqrt((mx1 - mx2) * (mx1 - mx2) +
				  (my1 - my2) * (my1 - my2));
			/** reset min, max and avg midpoint interval holders,
			 *  if needed
			 */
			if (dm < dmin)
				dmin = dm;
			if (dm > dmax)
				dmax = dm;
			davg = (davg + (dmin + dmax) / 2.0) / 2.0;
			/* hold this midpoint */
			mx1 = mx2;
			my1 = my2;
		}
		ntr++;
		current = current->next_trace;
	}


	headers_ranges.east_cmp[0] = east_cmp[0];
	headers_ranges.east_cmp[1] = east_cmp[1];
	headers_ranges.east_rec[0] = east_rec[0];
	headers_ranges.east_rec[1] = east_rec[1];
	headers_ranges.east_shot[0] = east_shot[0];
	headers_ranges.east_shot[1] = east_shot[1];
	headers_ranges.north_cmp[0] = north_cmp[0];
	headers_ranges.north_cmp[1] = north_cmp[1];
	headers_ranges.north_rec[0] = north_rec[0];
	headers_ranges.north_rec[1] = north_rec[1];
	headers_ranges.north_shot[0] = north_shot[0];
	headers_ranges.north_shot[1] = north_shot[1];
	headers_ranges.south_cmp[0] = south_cmp[0];
	headers_ranges.south_cmp[1] = south_cmp[1];
	headers_ranges.south_rec[0] = south_rec[0];
	headers_ranges.south_rec[1] = south_rec[1];
	headers_ranges.south_shot[0] = south_shot[0];
	headers_ranges.south_shot[1] = south_shot[1];
	headers_ranges.west_cmp[0] = west_cmp[0];
	headers_ranges.west_cmp[1] = west_cmp[1];
	headers_ranges.west_rec[0] = west_rec[0];
	headers_ranges.west_rec[1] = west_rec[1];
	headers_ranges.west_shot[0] = west_shot[0];
	headers_ranges.west_rec[1] = west_rec[1];
	headers_ranges.number_of_keys = number_of_keys;
	headers_ranges.trfirst = trfirst;
	headers_ranges.trlast = trlast;
	headers_ranges.trmax = trmax;
	headers_ranges.trmin = trmin;
	headers_ranges.keys = keys;
	headers_ranges.davg = davg;
	headers_ranges.dmax = dmax;
	headers_ranges.dmin = dmin;
	headers_ranges.ntr = ntr;
	headers_ranges.dim = dim;

	print_headers_ranges(headers_ranges);

	return headers_ranges;
}

void
print_headers_ranges(headers_ranges_t headers_ranges)
{
	cwp_String 	key;
	cwp_String 	type;
	Value 		valmin;
	Value		valmax;
	Value		valfirst;
	Value		vallast;
	double 		dvalmin;
	double		dvalmax;
	int 		kmax;
	int 		i;

	if (headers_ranges.number_of_keys == 0) {
		kmax = SU_NKEYS;
	} else {
		kmax = headers_ranges.number_of_keys;
	}

	printf("%ld traces: \n", headers_ranges.ntr);

	for (i = 0; i < kmax; i++) {
		get_header_value(*headers_ranges.trmin,
				 headers_ranges.keys[i], &valmin);
		get_header_value(*headers_ranges.trmax,
				 headers_ranges.keys[i], &valmax);
		get_header_value(*headers_ranges.trfirst,
				 headers_ranges.keys[i], &valfirst);
		get_header_value(*headers_ranges.trlast,
				 headers_ranges.keys[i], &vallast);
		dvalmin = vtod(hdtype(headers_ranges.keys[i]), valmin);
		dvalmax = vtod(hdtype(headers_ranges.keys[i]), valmax);
		if (dvalmin || dvalmax) {
			if (dvalmin < dvalmax) {
				printf("%s ", headers_ranges.keys[i]);
				printfval(hdtype(headers_ranges.keys[i]),
					  valmin);
				printf(" ");
				printfval(hdtype(headers_ranges.keys[i]),
					  valmax);
				printf(" (");
				printfval(hdtype(headers_ranges.keys[i]),
					  valfirst);
				printf(" - ");
				printfval(hdtype(headers_ranges.keys[i]),
					  vallast);
				printf(")");
			} else {
				printf("%s ", headers_ranges.keys[i]);
				printfval(hdtype(headers_ranges.keys[i]),
					  valmin);
			}
			printf("\n");
		}
	}

	if (headers_ranges.number_of_keys == 0) {
		if ((headers_ranges.north_shot[1] != 0.0) ||
		    (headers_ranges.south_shot[1] != 0.0) ||
		    (headers_ranges.east_shot[0] != 0.0) ||
		    (headers_ranges.west_shot[0] != 0.0)) {
			printf("\nShot coordinate limits:\n" "\tNorth(%g,%g)"
			       " South(%g,%g) East(%g,%g) West(%g,%g)\n",
			       headers_ranges.north_shot[0],
			       headers_ranges.north_shot[1],
			       headers_ranges.south_shot[0],
			       headers_ranges.south_shot[1],
			       headers_ranges.east_shot[0],
			       headers_ranges.east_shot[1],
			       headers_ranges.west_shot[0],
			       headers_ranges.west_shot[1]);
		}
		if ((headers_ranges.north_rec[1] != 0.0) ||
		    (headers_ranges.south_rec[1] != 0.0) ||
		    (headers_ranges.east_rec[0] != 0.0) ||
		    (headers_ranges.west_rec[0] != 0.0)) {
			printf("\nReceiver coordinate limits:\n"
			       "\tNorth(%g,%g) South(%g,%g) East(%g,%g)"
			       " West(%g,%g)\n", headers_ranges.north_rec[0],
			       headers_ranges.north_rec[1],
			       headers_ranges.south_rec[0],
			       headers_ranges.south_rec[1],
			       headers_ranges.east_rec[0],
			       headers_ranges.east_rec[1],
			       headers_ranges.west_rec[0],
			       headers_ranges.west_rec[1]);
		}
		if ((headers_ranges.north_cmp[1] != 0.0) ||
		    (headers_ranges.south_cmp[1] != 0.0) ||
		    (headers_ranges.east_cmp[0] != 0.0) ||
		    (headers_ranges.west_cmp[0] != 0.0)) {
			printf("\nMidpoint coordinate limits:\n"
			       "\tNorth(%g,%g) South(%g,%g) East(%g,%g)"
			       " West(%g,%g)\n", headers_ranges.north_cmp[0],
			       headers_ranges.north_cmp[1],
			       headers_ranges.south_cmp[0],
			       headers_ranges.south_cmp[1],
			       headers_ranges.east_cmp[0],
			       headers_ranges.east_cmp[1],
			       headers_ranges.west_cmp[0],
			       headers_ranges.west_cmp[1]);
		}
	}

	if (headers_ranges.dim != 0) {
		if (headers_ranges.dim == 1) {
			printf("\n2D line: \n");
			printf("Min CMP interval = %g ft\n",
			       headers_ranges.dmin);
			printf("Max CMP interval = %g ft\n",
			       headers_ranges.dmax);
			printf("Line length = %g miles (using avg CMP"
			       " interval of %g ft)\n",	headers_ranges.davg *
			       headers_ranges.ntr / 5280, headers_ranges.davg);
		} else if (headers_ranges.dim == 2) {
			printf("ddim line: \n");
			printf("Min CMP interval = %g m\n",
			       headers_ranges.dmin);
			printf("Max CMP interval = %g m\n",
			       headers_ranges.dmax);
			printf("Line length = %g km (using avg CMP interval"
			       " of %g m)\n", headers_ranges.davg *
			       headers_ranges.ntr / 1000, headers_ranges.davg);
		}
	}

	return;
}

void
val_sprintf(char *temp, Value unique_value, char *key)
{

	switch (*hdtype(key)) {
	case 's':
		(void) sprintf(temp, "%s", unique_value.s);
		break;
	case 'h':
		(void) sprintf(temp, "%d", unique_value.h);
		break;
	case 'u':
		(void) sprintf(temp, "%d", unique_value.u);
		break;
	case 'i':
		(void) sprintf(temp, "%d", unique_value.i);
		break;
	case 'p':
		(void) sprintf(temp, "%d", unique_value.p);
		break;
	case 'l':
		(void) sprintf(temp, "%ld", unique_value.l);
		break;
	case 'v':
		(void) sprintf(temp, "%ld", unique_value.v);
		break;
	case 'f':
		(void) sprintf(temp, "%f", unique_value.f);
		break;
	case 'd':
		(void) sprintf(temp, "%f", unique_value.d);
		break;
	case 'U':
		(void) sprintf(temp, "%d", unique_value.U);
		break;
	case 'P':
		(void) sprintf(temp, "%d", unique_value.P);
		break;
	default:
		err("fprintfval: unknown type %s", *hdtype(key));
	}

	return;

}

int
fetch_array_of_trace_headers(seis_root_obj_t *root, daos_obj_id_t *oids,
			     trace_oid_oh_t *gather_oid_oh,
			     int number_of_traces)
{
	seismic_entry_t 	seismic_entry = {0};
	daos_array_iod_t 	iod;
	daos_range_t 		rg;
	d_sg_list_t 		sgl;
	d_iov_t 		iov;
	int 			offset;
	int 			rc;

	/** open array object */
	rc = daos_array_open_with_attr(root->coh, gather_oid_oh->oid,
				       DAOS_TX_NONE, DAOS_OO_RW, 1,
				       500 * sizeof(daos_obj_id_t),
				       &gather_oid_oh->oh, NULL);
	if (rc != 0) {
		err("Opening array object with attr() failed, error"
		    " code = %d \n", rc);
		return rc;
	}
	/** set scatter gather list and IO descriptor */
	d_iov_set(&iov, (void*)(char*)oids,number_of_traces * sizeof(daos_obj_id_t));
	sgl.sg_nr = 1;
	sgl.sg_nr_out = 0;
	offset = 0;
	sgl.sg_iovs = &iov;
	iod.arr_nr = 1;
	rg.rg_len = number_of_traces * sizeof(daos_obj_id_t);
	rg.rg_idx = offset;
	iod.arr_rgs = &rg;

	rc = daos_array_read(gather_oid_oh->oh,
			     DAOS_TX_NONE, &iod, &sgl, NULL);
	if (rc != 0) {
		err("Reading gather oids array failed, error"
		    " code = %d \n", rc);
		return rc;
	}

	rc = daos_array_close(gather_oid_oh->oh,
			      NULL);
	if (rc != 0) {
		err("Closing array object failed, error"
		    " code = %d \n", rc);
		return rc;
	}

	return rc;
}

void
release_traces_list(traces_list_t *trace_list)
{
	traces_headers_t 	*temp;

	temp = trace_list->head;

	if(temp == NULL){
		warn("list of traces is empty \n");
		return;
	}
	while(temp->next_trace != NULL ){
		free(temp);
		temp = temp->next_trace;
	}
	free(trace_list->tail);
	free(trace_list);

	return;
}
