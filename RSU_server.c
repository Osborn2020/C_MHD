//#include <curl/curl.h>

#include "server.h"


static enum MHD_Result 
readJSON (void *cls, 
          const char *upload_data, 
          size_t *upload_data_size)  // todo: use temp string to read as proper string
{
  printf("upload data: %s\n",upload_data);
  printf("upload data size: %zu\n",upload_data_size);
  struct JSONhandler *cbc = cls;
  int x = upload_data_size;
  printf("current JSONreader pos: %d\n",cbc->pos);
  printf("JSONreader pos+ x: %d\n",cbc->pos+x);
  printf("jason->size: %d\n",cbc->size);
  if (cbc->pos + x > cbc->size) { /* overflow */
    //printf("JSONreader pos+ x: %d\n",cbc->pos+x);
    printf("overflow while reading JSON \n");
    return MHD_NO;
  }
  memcpy (&cbc->data[0], upload_data, x+1);
  cbc->pos += x+1;
  printf("JSONreader data: %s\n",cbc->data);
  printf("JSON is updated\n");
  return MHD_YES;
}


/* parse string sent with POST */
static enum MHD_Result
iterate_post_string (void *cls,
               enum MHD_ValueKind kind,
               const char *key,
               const char *filename,
               const char *content_type,
               const char *transfer_encoding,
               const char *value, uint64_t off, size_t size)
{
  struct connection_info_struct *con_info = cls;

  if (key==NULL || strlen(key)==0) 
  {
    printf("error: cannot accept empty key\n");
    return MHD_NO;
  }
  //printf("key: %s\n",key);
  //printf("value: %s\n",value);
  //printf("content_type: %s\n",content_type);
  //printf("size=%zu\n",size);
  //const char* Jstring;

  /*read JSON string*/
  if (0 == strcasecmp(key,"json")) 
  {    
    printf("read JSON string from string\n");
    struct json_object *root; 
    struct json_object *tmp_obj, *name, *ID, *filename; // child nodes, freed when parent is frees
    root = json_tokener_parse(value);
    // Use is_error() to check the result, or "root == NULL" if bits.h is not in current json-c.
    if (is_error(root)) 
    {
      printf("parse failed: JSON string can't be parsed\n");
      return MHD_NO;
    }
    /*keys that must exist*/
    name = json_object_object_get(root, "RSUNAME");
    ID = json_object_object_get(root, "RSUID");
    filename = json_object_object_get(root, "WriteToFile");
    if (!name || !ID|| !filename ) 
    {
      printf("parse failed: missing any of these keys: RSUNAME/RSUID/WriteToFile\n");
      strncpy(con_info->filename, "NULL",strlen("NULL")+1);
      con_info->answercode = MHD_HTTP_OK;
      con_info->answerstring = illJSONError;
      /*
      char buf[200];
      strncat(buf,illJSONError,strlen(illJSONError)+1);
      if (!name) 
      {
        strncat(buf,MissKeyName,strlen(MissKeyName)+1);
        if (!ID || !filename) 
          strcat(buf,"/ ");
      }
      if (!ID) 
      {
        strncat(buf,MissKeyID,strlen(MissKeyID)+1);
        if (!filename) 
          strcat(buf,"/ ");
      }
      if (!filename) 
      {
        strncat(buf,MissKeyFile,strlen(MissKeyFile)+1);
      }      
      con_info->answerstring = buf;
      */
      json_object_put(root);  
      //free(buf);    
      return MHD_NO;
    }

    enum json_type type = json_object_get_type(root);
    char bufferNum[200];
    int tmpI;
    double tmpD;
    char s[10];
    
    /*loop through JSON object*/
    json_object_object_foreach(root, key, val) 
    {
      type = json_object_get_type(val);
      char *Jtype = json_type_to_name(type);
      printf("type: %s\n",Jtype);
      printf("key: %s\n",key);
      int is_legal = 0;
      for (int i=0; i<25; i++) 
      {
        if (0==strcmp(key,keys[i])) 
        {
          printf("key is legal\n");
          is_legal = 1;
          break;
        }
      }
      if (is_legal!=1) 
      {
        printf("key is not in base data, ignore\n");
      }
      else 
      {
        tmp_obj = json_object_object_get(root, key);      
        switch (type) 
        {
        case json_type_null:
          //printf("json_type_null\n");
          break;
        case json_type_boolean:
          //printf("json_type_boolean\n");
          
          if (json_object_get_boolean(tmp_obj) == true) 
          {
            strncpy(s,"yes",strlen("yes")+1);
          }
          else 
          {
            strncpy(s,"no",strlen("no")+1);
          }
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strncat(con_info->value,s,strlen(s)+1);
          strcat(con_info->value,"\n");
          break;
        case json_type_double:
          //printf("json_type_double\n");
          tmpD = json_object_get_double(tmp_obj);
          printf("double value: %f\n",tmpD);
          snprintf(bufferNum, sizeof(bufferNum),"%f", tmpD);
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
          strcat(con_info->value,"\n");
          strcpy(bufferNum,"");
          break;
        case json_type_int:
          //printf("json_type_int\n");
          tmpI = json_object_get_int(tmp_obj);
          //printf("int value: %d\n",tmpI);  
          snprintf(bufferNum, sizeof(bufferNum),"%d", tmpI);
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
          strcat(con_info->value,"\n");
          strcpy(bufferNum,"");
          break;
        case json_type_object:
          //printf("json_type_object\n");
          break;
        case json_type_array:
          //printf("json_type_array\n");
          //printf("array size = %d\n", json_object_array_length(tmp_obj));
          if (0==strcmp(key,"PHASE_WEIGHT")) 
          {
            json_object *tmp_obj1 = NULL;
            int i;
            char buffertemp[200];
            for (i=0; i<json_object_array_length(tmp_obj); i++){
              tmp_obj1 = json_object_array_get_idx(tmp_obj, i);
              tmpI = json_object_get_int(tmp_obj1);
              //printf("array int value at ID [%d]: %d\n",i,tmpI);
              if (i==0) 
              {
                snprintf(buffertemp, 200,"%d ", tmpI);
              }
              else 
              {
                snprintf(buffertemp+strlen(buffertemp), 200-strlen(buffertemp),"%d ", tmpI);
              }
            }
            snprintf(bufferNum, sizeof(bufferNum),"%s", buffertemp);
            strncat(con_info->value,key,strlen(key)+1);
            strcat(con_info->value,": ");
            strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
            strcat(con_info->value,"\n");
            strcpy(bufferNum,"");
          }
          break;
        case json_type_string:
          //printf("json_type_string\n");
          //printf("%s = %s\n", key, json_object_get_string(tmp_obj));
          if (0!=strcasecmp(key,"WriteToFile")) 
          {
            strncat(con_info->value,key,strlen(key)+1);
            strcat(con_info->value,": ");
            strcat(con_info->value,json_object_get_string(tmp_obj));
            strcat(con_info->value,"\n");
          }
          else 
          {
            strcat(con_info->filename,json_object_get_string(tmp_obj));
          }
          //*tmp = 0;
          break;
        }
      
      }
    }
    json_object_put(root);
  }
  else if (0 == strcasecmp(key,"WriteToFile"))
  { // key is config filename
    printf("bring file\n");
    strcat(con_info->filename,value);
  }
  else { // key is config data
    strncat(con_info->value,key,strlen(key)+1);
    strcat(con_info->value,": ");
    strcat(con_info->value,value);
    strcat(con_info->value,"\n");
  }
  
  return MHD_YES;
}

static enum MHD_Result
handle_JSON (void *cls,
            const char *value)
{
  struct connection_info_struct *con_info = cls;

  if (value==NULL || strlen(value)==0) 
  {
    printf("error: cannot accept empty key\n");
    con_info->answerstring = postProcError;
    con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
    return MHD_NO;
  }

  /*read JSON string*/
     
  printf("read JSON string from JSON\n");
  struct json_object *root; 
  struct json_object *tmp_obj, *name, *ID, *filename; // child nodes, freed when parent is frees
  root = json_tokener_parse(value);
  // Use is_error() to check the result, or "root == NULL" if bits.h is not in current json-c.
  if (is_error(root)) 
  {
    printf("parse failed: JSON string can't be parsed\n");
    strncpy(con_info->filename, "NULL",strlen("NULL")+1);
    con_info->answerstring = postProcError;
    con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
    return MHD_NO;
  }
  /*keys that must exist*/
  name = json_object_object_get(root, "RSUNAME");
  ID = json_object_object_get(root, "RSUID");
  filename = json_object_object_get(root, "WriteToFile");
  int missKey[] = {0,0,0};
  if (!name || !ID|| !filename ) 
  {
    printf("parse failed: missing any of these keys: RSUNAME/RSUID/WriteToFile\n");
    strncpy(con_info->filename, "NULL",strlen("NULL")+1);  
    //con_info->answerstring = illJSONError;
    con_info->answercode = MHD_HTTP_OK;
    int many = 0;
    if (!name) 
    {
      missKey[0]=1;
      many++;
    }
    if (!ID) 
    {
      //strncpy(missBuf[1],"1",1);
      missKey[1]=1;
      many++;
    }
    if (!filename) 
    {
      //strncpy(missBuf[0],"1",1);
      missKey[2]=1;
      many++;
    }
    char *missBuf[200];
    strncpy(missBuf,illJSONError,strlen(illJSONError)+1);
    //printf(missBuf);
    //printf("\n");
    int length = sizeof(missKey) / sizeof(missKey[0]);
    for (int i=0; i<length; i++) 
    {
      if (missKey[i]==1) 
      {
        printf("missing key %s\n",mustHaveKeys[i]);
        strncat(missBuf,mustHaveKeys[i],strlen(mustHaveKeys[i])+1);
        if (many>1) 
        {
          strncat(missBuf,"/",strlen("/")+1);
          many--;
        }
      }
    }
    printf(missBuf);
    printf("\n");
    con_info->answerstring = missBuf;
    //printf(con_info->answerstring);
    //printf("\n");
    
    json_object_put(root);
    //free(missBuf);
    return MHD_NO;
  }

  enum json_type type = json_object_get_type(root);
  char bufferNum[200];
  int tmpI;
  double tmpD;
  char s[10]; 
  /*loop through JSON object*/
  json_object_object_foreach(root, key, val) {
    type = json_object_get_type(val);
    char *Jtype = json_type_to_name(type);
    printf("type: %s\n",Jtype);
    printf("key: %s\n",key);
    int is_legal = 0;
    for (int i=0; i<25; i++) {
      if (strcmp(key,keys[i])==0) {
        printf("key is legal\n");
        is_legal = 1;
        break;
      }
    }
    if (is_legal!=1) {
      printf("key is not in base data, ignore\n");
    }
    else {
      tmp_obj = json_object_object_get(root, key);      
      switch (type) {
      case json_type_null:
        //printf("json_type_null\n");
        break;
      case json_type_boolean:
        //printf("json_type_boolean\n");
        
        if (json_object_get_boolean(tmp_obj) == true) {
          strncpy(s,"yes",strlen("yes")+1);
        }
        else {
          strncpy(s,"no",strlen("no")+1);
        }
        strncat(con_info->value,key,strlen(key)+1);
        strcat(con_info->value,": ");
        strncat(con_info->value,s,strlen(s)+1);
        strcat(con_info->value,"\n");
        break;
      case json_type_double:
        //printf("json_type_double\n");
        tmpD = json_object_get_double(tmp_obj);
        printf("double value: %f\n",tmpD);
        snprintf(bufferNum, sizeof(bufferNum),"%f", tmpD);
        strncat(con_info->value,key,strlen(key)+1);
        strcat(con_info->value,": ");
        strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
        strcat(con_info->value,"\n");
        strcpy(bufferNum,"");
        break;
      case json_type_int:
        //printf("json_type_int\n");
        tmpI = json_object_get_int(tmp_obj);
        //printf("int value: %d\n",tmpI);  
        snprintf(bufferNum, sizeof(bufferNum),"%d", tmpI);
        strncat(con_info->value,key,strlen(key)+1);
        strcat(con_info->value,": ");
        strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
        strcat(con_info->value,"\n");
        strcpy(bufferNum,"");
        break;
      case json_type_object:
        //printf("json_type_object\n");
        break;
      case json_type_array:
        //printf("json_type_array\n");
        //printf("array size = %d\n", json_object_array_length(tmp_obj));
        if (0==strcmp(key,"PHASE_WEIGHT")) {
          json_object *tmp_obj1 = NULL;
          int i;
          char buffertemp[200];
          for (i=0; i<json_object_array_length(tmp_obj); i++){
            tmp_obj1 = json_object_array_get_idx(tmp_obj, i);
            tmpI = json_object_get_int(tmp_obj1);
            //printf("array int value at ID [%d]: %d\n",i,tmpI);
            if (i==0) {
              snprintf(buffertemp, 200,"%d ", tmpI);
            }
            else {
              snprintf(buffertemp+strlen(buffertemp), 200-strlen(buffertemp),"%d ", tmpI);
            }
          }
          snprintf(bufferNum, sizeof(bufferNum),"%s", buffertemp);
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
          strcat(con_info->value,"\n");
          strcpy(bufferNum,"");
        }
        break;
      case json_type_string:
        //printf("json_type_string\n");
        //printf("%s = %s\n", key, json_object_get_string(tmp_obj));
        if (0!=strcasecmp(key,"WriteToFile")) { // part of config data
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strcat(con_info->value,json_object_get_string(tmp_obj));
          strcat(con_info->value,"\n");
        }
        else { // filename
          strcat(con_info->filename,json_object_get_string(tmp_obj));
        }
        //*tmp = 0;
        break;
      }    
    }
  }
  json_object_put(root);
  
  return MHD_YES;
}

/* send textfile through POST */
static enum MHD_Result
iterate_post_file (void *cls,
              enum MHD_ValueKind kind,
              const char *key,
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data,
              uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = cls;
  FILE *fp;
  (void) kind;               /* Unused. Silent compiler warning. */
  (void) content_type;       /* Unused. Silent compiler warning. */
  (void) transfer_encoding;  /* Unused. Silent compiler warning. */
  (void) off;                /* Unused. Silent compiler warning. */

  if (0 != strcmp (key, "file"))
  {
    con_info->answerstring = serverErrorPage;
    con_info->answercode = MHD_HTTP_BAD_REQUEST;
    return MHD_YES;
  }

  if (! con_info->fp)
  {
    if (0 != con_info->answercode)   /* something went wrong */
      return MHD_YES;
    if (NULL != (fp = fopen (filename, "rb")))
    {
      fclose (fp);
      con_info->answerstring = fileExistsMessage;
      con_info->answercode = MHD_HTTP_FORBIDDEN;
      return MHD_YES;
    }
    /* NOTE: This is technically a race with the 'fopen()' above,
       but there is no easy fix, short of moving to open(O_EXCL)
       instead of using fopen(). For the example, we do not care. */
    con_info->fp = fopen (filename, "ab");
    if (! con_info->fp)
    {
      con_info->answerstring = fileIOError;
      con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      return MHD_YES;
    }
  }

  if (size > 0)
  {
    if (! fwrite (data, sizeof (char), size, con_info->fp))
    {
      con_info->answerstring = fileIOError;
      con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      return MHD_YES;
    }
  }
  return MHD_YES;
}

/*return web page/JSON upon completed response*/
static enum MHD_Result
send_page (struct MHD_Connection *connection,
           const char *page,
           int status_code)
{
  enum MHD_Result ret;
  struct MHD_Response *response;
  struct json_object *json_obj = NULL;
  struct json_object *tmp_obj = NULL; //child to json_obj
  int JSONlen;
  
  printf("status: %d\n",status_code);
  printf("return message len: %d\n",strlen(page));
  printf("return message: %s\n",page);
  //new a base object
	json_obj = json_object_new_object();
	if (!json_obj)
	{
		printf("Cannot create object\n");
		ret = -1;
    json_object_put(json_obj);
    return MHD_NO;
		//goto error;
	}
  
  tmp_obj = json_object_new_string(page);
	if (!tmp_obj)
	{
		printf("Cannot create string object for %s\n", "message");
		ret = -1;
		json_object_put(json_obj);
		return MHD_NO;
	}
  json_object_object_add(json_obj, "message", tmp_obj);
	tmp_obj = NULL;
	
  char *buffer = json_object_to_json_string(json_obj);
  printf("JSON return: %s\n",buffer);
  JSONlen = strlen(buffer);
  printf("JSON length: %d\n",JSONlen);
  
  response =
    MHD_create_response_from_buffer (strlen(buffer),
                                     (void*)buffer,
                                     MHD_RESPMEM_MUST_COPY);
  if (! response)
    return MHD_NO;
  printf("created response\n"); 
  
  MHD_add_response_header (response,
                           "content-type",
                           "application/json");                         
  ret = MHD_queue_response (connection,
                            status_code,
                            response);
  printf("created ret\n"); 
  MHD_destroy_response (response);
  json_object_put(json_obj);
  printf("destroyed response\n");
  return ret;
}

/*handle finished requests*/
static void
request_completed (void *cls,
                   struct MHD_Connection *connection,
                   void **con_cls,
                   enum MHD_RequestTerminationCode toe)
{
  printf("request complete\n");
  
  struct connection_info_struct *con_info = *con_cls;
  //enum *XToe = toe;
  /*
  if (XToe==MHD_REQUEST_TERMINATED_WITH_ERROR) {
    printf("connection suffered an error\n");
    memset(&con_info->value[0], 0, sizeof(con_info->value)); // reset connection info POST value
    memset(&con_info->filename[0], 0, sizeof(con_info->filename));
    if (NULL != con_info->pp)
    {
      MHD_destroy_post_processor (con_info->pp);  
    }

    if (con_info->fp)
      fclose (con_info->fp);
    
    *con_cls = NULL;
    return;
  }
  */
  //(void) cls;         /* Unused. Silent compiler warning. */
  //(void) connection;  /* Unused. Silent compiler warning. */
  //(void) toe;         /* Unused. Silent compiler warning. */
  
  if (NULL == con_info) {
    printf("connection is already freed or unavailable\n");
    *con_cls = NULL;
    return;
  }

  if (1 == (con_info->connectiontype == POST)) // is POST
  {
    nr_of_uploading_clients--;
    printf("clients after freed: %d\n",nr_of_uploading_clients);
    printf("POST type post-complete:\n");
    /* reset connection info POST value */
    if (NULL != con_info->value) {
      memset(&con_info->value[0], 0, sizeof(con_info->value)); 
    }
    if (NULL != con_info->filename) {
      memset(&con_info->filename[0], 0, sizeof(con_info->filename));
    }
    printf( ("postprocessor is not freed: %s\n"),(NULL != con_info->pp)== true ? "yes" : "no");
    //json_object_put(con_info->json_root);
    if (NULL != con_info->pp)
    {
      printf("free postprocessor\n");
      //con_info->pp == NULL;
      if (NULL != con_info->pp) 
      {
        MHD_destroy_post_processor (con_info->pp);
      }
    }

    if (NULL != con_info->JSONreader) {
      printf("free JSON reader\n");
      con_info->JSONreader->pos = 0;
      if (NULL != con_info->JSONreader->data) 
      {
        memset(&con_info->JSONreader->data[0], 0, sizeof(con_info->JSONreader->data));
      }      
      con_info->JSONreader->size = 0;
      free (con_info->JSONreader);
    }

    if (con_info->fp)
      fclose (con_info->fp);
  }

  else {
      printf("connection type is not POST\n");
  }
  free (con_info);
  printf("freed connection\n");
  *con_cls = NULL;
}

/*respond to client request*/
static int
echo_response (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data, size_t *upload_data_size,
          void **con_cls)
{
  //static int eok;
  //enum MHD_result ret;
  static int posttype;
  //struct JSONhandler *jason;
  static int content_len=0;
  
  printf("URL: %s\n",url);
  printf("method: %s\n",method);
  const char* param = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
  if (param!=NULL) 
  {
    content_len = atoi(param);
  }
  else 
  {
    content_len = 0;
  }
  const char* type = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
  printf("length of content: %d\n",content_len);
  printf("type of content: %s (is_null=%s)\n",type,(type==NULL ? "yes" : "no"));
  //const char* body = MHD_lookup_connection_value (connection, MHD_POSTDATA_KIND, NULL);
  //printf("data of content: %s (is_null=%s)\n",body,(body==NULL ? "yes" : "no"));
  
  //printf("size: %zu\n",upload_data_size);
  //printf("upload_data: %s\n",upload_data);
  //printf(connection->headers_received);

  if (NULL == *con_cls)
  {
    /* First call, setup data structures */
    struct connection_info_struct *con_info;
    printf("current amount of clients: %d\n",nr_of_uploading_clients);
    if (nr_of_uploading_clients >= MAXCLIENTS)
      return send_page (connection,
                        busyPage,
                        MHD_HTTP_SERVICE_UNAVAILABLE);
    /*initialize connection*/
    con_info = malloc (sizeof (struct connection_info_struct));
    if (NULL == con_info)
      return MHD_NO;
    con_info->answercode = 0;   /* none yet */
    con_info->fp = NULL;
    con_info->pp = NULL;
    con_info->JSONreader = NULL;
    con_info->posttype = -1;
    strcpy(con_info->value,"");
    strcpy(con_info->filename,"");
    //strcpy(con_info->missKey,"000");
    //con_info->missKey = {0,0,0};
    
    if (type==NULL) {
      printf("error: cannot get content type, cannot process request");
      con_info->posttype = -1;
    }
    else {
      if (0==strcmp(type,"application/json")) 
      {
        con_info->posttype = 2;
      }
      else if (0==strcmp(type,"application/x-www-form-urlencoded") || strstr(type, "multipart/form-data") != NULL)
      {
        con_info->posttype = 0;
      }
      else
      {
        con_info->posttype = -2;
      }
    }

    if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST) || content_len!=0)
    {
      
      if (con_info->posttype == 0)
      {
        printf("accept type: string\n");
        con_info->pp =
        MHD_create_post_processor (connection,
                                    POSTBUFFERSIZE,
                                    &iterate_post_string,
                                    (void *) con_info);
      }
      else if (con_info->posttype == 2) 
      { 
        printf("handle as JSON POST\n");
        printf("accept type: JSON\n");
        
        if (NULL == con_info->JSONreader)
        {
          char buf[MAX_JSON_SIZE];
          con_info->JSONreader = malloc (sizeof(struct JSONhandler));          
          con_info->JSONreader->pos = 0; //actual data load
          con_info->JSONreader->data = buf;
          con_info->JSONreader->size = MAX_JSON_SIZE;
          if (NULL == con_info->JSONreader)
          {
            printf("error setting JSONreader\n");
            return MHD_NO;
          }
          printf("set JSON\n");
        }
      }
      else if (con_info->posttype == -1) 
      { // empty URL without parameters
        printf("POST error: encountered an error within headers\n");
        return send_page (connection,
                        noParaError,
                        MHD_HTTP_SERVICE_UNAVAILABLE); // HTTP 503
      }
      else 
      { // empty URL without parameters (con_info->posttype == -2)
        printf("POST error: unsupported content type\n");
        return send_page (connection,
                        dataTypeError,
                        415); // HTTP 415
      }

      if (content_len==0) 
      { // empty POST
        printf("POST error: require content\n");
        return send_page (connection,
                        noContentError,
                        422); // HTTP 422 unprocessable
      }
      if (NULL == con_info->pp && 0!=strcmp(type,"application/json")) //pp is not needed for JSON
      {
        free (con_info);
        return MHD_NO;
      }
      nr_of_uploading_clients++;
      printf("new amount of clients: %d\n",nr_of_uploading_clients);
      con_info->connectiontype = POST;      
    }
    else if (0 == strcasecmp (method, MHD_HTTP_METHOD_GET))
    {
      con_info->connectiontype = GET;
    }
    else if (0 == strcasecmp (method, MHD_HTTP_METHOD_PUT))
    {
      con_info->connectiontype = PUT;
    }
    else if (0 == strcasecmp (method, MHD_HTTP_METHOD_DELETE))
    {
      con_info->connectiontype = DELETE;
    }
    else {
      con_info->connectiontype = -1; //undefined method
    }

    printf("confirmed request: %d\n",con_info->connectiontype);
    *con_cls = (void *) con_info;
    return MHD_YES;
  }
  
  if (0 == strcasecmp (method, MHD_HTTP_METHOD_GET))
  {
    /* We just return the standard form for uploads on all GET requests */
    struct connection_info_struct *con_info = *con_cls;
    //char* buffer= (char*)malloc(sizeof(2048));
    //*buffer = 0;
    
    con_info->answercode = MHD_HTTP_OK;
    con_info->answerstring = GETMessage;
    
    if (0 != *upload_data_size) 
    {
      printf("warning: extra data during GET ops\n");
      if (0 != con_info->answercode)
      {
        /* we already know the answer, skip rest of upload */
        *upload_data_size = 0;
        return MHD_YES;
      }
      if (con_info->posttype==2) 
      {
        printf("Getting JSON\n");
        
        if (MHD_YES != 
          readJSON (con_info->JSONreader,
                    upload_data,
                    *upload_data_size))
        {
          printf("JSON get error\n");
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
      }
      else if (con_info->posttype==0)
      {
        /* use MHD method for string, form and file type */       
        if (MHD_YES !=
          MHD_post_process (con_info->pp,
                            upload_data,
                            *upload_data_size))
        {
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
      }
      else
      {
        printf ("unaccepted upload type\n");
        con_info->answerstring = postProcError;
        con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      }
      /* indicate that we have processed */
      *upload_data_size = 0;
      return MHD_YES;      
      /* Upload finished */
      if (NULL != con_info->fp)
      {
        printf("Closing upload file");
        fclose (con_info->fp);
        con_info->fp = NULL;
      }
      if (con_info->value == NULL) 
      {
        con_info->answerstring = postProcError;
        con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      }
    }
    
    printf("GET operation\n");
    //free (con_info);
    return send_page (connection,
                      con_info->answerstring,
                      con_info->answercode);
  }

  if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST)) 
  {
    struct connection_info_struct *con_info = *con_cls;
    
    if (0 != *upload_data_size)
    {
      /* Upload not yet done */
      if (0 != con_info->answercode)
      {
        /* we already know the answer, skip rest of upload */
        *upload_data_size = 0;
        return MHD_YES;
      }
      if (0==strcmp(type,"application/json")) 
      { // posted using JSON type
        printf("Posting JSON\n");
        if (MHD_YES != 
          readJSON (con_info->JSONreader,
                    upload_data,
                    *upload_data_size)) 
        {
          printf("JSON read error\n");
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
        else 
        {
          printf("updated JSON: %s\n",con_info->JSONreader->data);
        }
      }
      else if (con_info->posttype==0)
      {
        /* use MHD method for string, form and file type */       
        if (MHD_YES !=
          MHD_post_process (con_info->pp,
                            upload_data,
                            *upload_data_size))
        {
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
      }
      else 
      {
        printf ("unaccepted upload type\n");
        con_info->answerstring = postProcError;
        con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      }
      /* indicate that we have processed */
      *upload_data_size = 0;
      return MHD_YES;
    }

    /* Upload finished */
    if (NULL != con_info->fp)
    {
      printf("Closing upload file");
      fclose (con_info->fp);
      con_info->fp = NULL;
    }
    if (con_info->value == NULL) 
    {
      con_info->answerstring = postProcError;
      con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
    if (0 == con_info->answercode)
    {
      /* No errors encountered, declare success */
      con_info->answerstring = completePage;
      con_info->answercode = MHD_HTTP_OK;
      if (con_info->posttype==0) 
      { // write string to file
        printf("string upload success\n");
        printf("writing file\n");
        FILE *fp;
        char *filename = (char*)malloc(sizeof(char)*200);
        *filename = 0;
        //strncat(filename,"RSUID_",strlen("RSUID_")+1);
        struct stat sb;
        //DIR* dir = opendir(fileDir);
        if (stat(fileDir,&sb)==0) {
          printf("%s exists\n",fileDir);
          strncat(filename,fileDir,strlen(fileDir)+1);
        } 
        strncat(filename,con_info->filename,strlen(con_info->filename)+1);
        strncat(filename,".txt",strlen(".txt")+1);
        //strncat(con_info->filename,".txt",strlen(".txt")+1);
        printf(con_info->value);
        fp = fopen (filename, "wb");
        if (! fprintf (fp,con_info->value))
        {
          printf("write file error\n");
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
        *filename = 0;
        free(filename);
        fclose(fp);
      }
      else if (con_info->posttype==2) 
      {
        printf("JSON upload success: %s\n",con_info->JSONreader->data);
        
        if (MHD_YES != (handle_JSON(con_info,con_info->JSONreader->data))) 
        {
          printf("error when writing JSON to file\n");
          //printf(con_info->answerstring);
          //con_info->answerstring = illJSONError;
          //con_info->answercode = MHD_HTTP_OK;
        }
        else 
        {
          printf("done\n");
          if (con_info->filename!="NULL" && con_info->filename!=NULL) 
          {
            printf("writing JSON file\n");
            FILE *fp;
            char *filename = (char*)malloc(sizeof(char)*200);
            *filename = 0;
            //strncat(filename,"RSUID_",strlen("RSUID_")+1);
            struct stat sb;
            //DIR* dir = opendir(fileDir);
            if (stat(fileDir,&sb)==0) {
              printf("%s exists\n",fileDir);
              strncat(filename,fileDir,strlen(fileDir)+1);
            }            
            strncat(filename,con_info->filename,strlen(con_info->filename)+1);
            strncat(filename,".txt",strlen(".txt")+1);
            //strncat(con_info->filename,".txt",strlen(".txt")+1);
            //printf(con_info->value);
            //printf(filename);
            fp = fopen (filename, "wb");
            if (! fprintf (fp,con_info->value))
            {
              printf("write JSON file error\n");
              con_info->answerstring = postProcError;
              con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
            }
            fclose(fp);
            *filename = 0;
            free(filename);
            
          }
        }        
      }
      else {
        printf("unsupported data type\n");
      }
    }
    else 
    {
      printf("an error occurred while uploading: %d\n",con_info->answercode);
    }        
    
    return send_page (connection,
                      con_info->answerstring,
                      con_info->answercode);
  }

  if (0 == strcasecmp (method, MHD_HTTP_METHOD_PUT) || 0 == strcasecmp (method, MHD_HTTP_METHOD_DELETE))
  {
    struct connection_info_struct *con_info = *con_cls;
    if (content_len==0) 
    { // empty PUT/DELETE
      printf("PUT/DELETE error: require content\n");
      con_info->answercode = 422;
      con_info->answerstring = "warning: PUT/DELETE require parameters to execute";
    }
    else {
      con_info->answercode = 200;
      con_info->answerstring = completePUT;
    }
    if (0 != *upload_data_size)
    {
      /* Upload not yet done */
      if (0 != con_info->answercode)
      {
        /* we already know the answer, skip rest of upload */
        *upload_data_size = 0;
        return MHD_YES;
      }
      if (0==strcmp(type,"application/json")) 
      { // posted using JSON type
        printf("Posting JSON\n");
        if (MHD_YES != 
          readJSON (con_info->JSONreader,
                    upload_data,
                    *upload_data_size)) 
        {
          printf("JSON read error\n");
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
        else 
        {
          printf("updated JSON: %s\n",con_info->JSONreader->data);
        }
      }
      else if (con_info->posttype==0)
      {
        /* use MHD method for string, form and file type */       
        if (MHD_YES !=
          MHD_post_process (con_info->pp,
                            upload_data,
                            *upload_data_size))
        {
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
      }
      else 
      {
        printf ("unaccepted upload type\n");
        con_info->answerstring = postProcError;
        con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      }
      /* indicate that we have processed */
      *upload_data_size = 0;
      return MHD_YES;
    }
    /* Upload finished */
    if (NULL != con_info->fp)
    {
      printf("Closing upload file");
      fclose (con_info->fp);
      con_info->fp = NULL;
    }
    if (con_info->value == NULL) 
    {
      con_info->answerstring = postProcError;
      con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
    
    
    printf("PUT/DELETE operation\n");    
    return send_page (connection,
                      con_info->answerstring,
                      con_info->answercode);
  }

  /* Not a legal response type, generate error */
  return send_page (connection,
                    errorPage,
                    MHD_HTTP_BAD_REQUEST);
}


int main (int argc, char *const *argv)
{
  //unsigned int errorCount = 0;
  struct MHD_Daemon *d;
  d = MHD_start_daemon (MHD_USE_THREAD_PER_CONNECTION | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_ERROR_LOG ,
                        PORT,
                        NULL, NULL, 
                        &echo_response, NULL,
                        MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int) 10,
                        MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL, 
                        MHD_OPTION_END);
  if (d == NULL)
    return 1;
  (void) getc (stdin);
  MHD_stop_daemon (d);
  return 0;
}