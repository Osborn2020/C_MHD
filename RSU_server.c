//#include <curl/curl.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#endif
#include<json-c/json.h>
#include<json-c/bits.h>

#define PORT            10168
//#define PORT            10003
#define POSTBUFFERSIZE  2048
#define MAXCLIENTS      10
#define MAX_JSON_SIZE 4096
#define JSON_NUM_BUF 200

#define NULL_NULL		"NULL"
#define BOOL_BOOLEAN	"BOOLEAN"
#define STR_STRING		"STRING"
#define NUM_INTEGER		"NUMBER"
#define NUM_DOUBLE		"DOUBLE"
#define ARR_ARRAY		"ARRAY"
#define OBJ_OBJECT		"OBJECT"
#define NULL_MEMBER_NULL	"MEMBER_NULL"
#define BOOL_MEMBER_BOOLEAN	"MEMBER_BOOLEAN"
#define STR_MEMBER_STRING	"MEMBER_STRING"
#define NUM_MEMBER_INTEGER	"MEMBER_NUMBER"
#define NUM_MEMBER_DOUBLE	"MEMBER_DOUBLE"
#define ARR_MEMBER_ARRAY	"MEMBER_ARRAY"

/*write data buffer*/
struct JSONhandler
{
  char *data;
  size_t len;
  size_t size;
};

struct CBC
{
  char *buf;
  size_t pos;
  size_t size;
};

enum ConnectionType
{
  GET = 0,
  POST = 1,
  PUT = 2,
  DELETE = 3,
  OPTIONS = 4
};

enum JsonType
{
  STRING = 0,
  INT = 1,
  DOUBLE = 2,
  BOOL = 3,
  ARRAY = 4,
  OBJECT = 5,
  NULL_N = 6
};

static unsigned int nr_of_uploading_clients = 0;

/* Information we keep per connection. */
struct connection_info_struct
{
  enum ConnectionType connectiontype;

  /**
   * Handle to the POST processing state.
   */
  struct MHD_PostProcessor *pp;

  /**
   * File handle where we write uploaded data.
   */
  FILE *fp;

  /**
   * HTTP response body we will return, NULL if not yet known.
   */
  const char *answerstring;

  /**
   * HTTP status code we will return, 0 for undecided.
   */
  unsigned int answercode;

  /**
   * String submitted.
   */
  char value[2048];

  /**
   * Type of data posted.
   */
  int posttype;

  /**
   * File name to write uploaded data.
   */
  char *filename[100];

  /**
   * JSON string from client.
   */
  //struct json_object *json_root;
};

const char *askPage =
  "<html><body>\n\
                       Upload a file, please!<br>\n\
                       There are %u clients uploading at the moment.<br>\n\
                       <form action=\"/filepost\" method=\"post\" enctype=\"multipart/form-data\">\n\
                       <input name=\"file\" type=\"file\">\n\
                       <input type=\"submit\" value=\" Send \"></form>\n\
                       </body></html>";
const char *askFormPage =
  "<html><body>\n\
                       Upload a file, please!<br>\n\
                       There are %u clients uploading at the moment.<br>\n\
                       <form action=\"/formpost\" method=\"post\" enctype=\"multipart/form-data\">\n\
                       <input name=\"RSUNAME\" type=\"string\">\n\
                       <input name=\"RSUID\" type=\"string\">\n\
                       <input name=\"RSULAT\" type=\"string\">\n\
                       <input name=\"RSULONG\" type=\"string\">\n\
                       <input name=\"SIGNAL_CONTROLLER_MANUFACTURER\" type=\"string\">\n\
                       <input name=\"SIGNAL_STATUS_REPORT_ACTIVE\" type=\"string\">\n\ 
                       <input name=\"SIGNAL_ADJUST_UPPER_BOUND_ACTIVE\" type=\"string\">\n\ 
                       <input name=\"SIGNAL_ADJUST_LOWER_BOUND_ACTIVE\" type=\"string\">\n\
                       <input name=\"SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE\" type=\"string\">\n\
                       <input name=\"SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE\" type=\"string\">\n\
                       <input name=\"LOG_MIDDLEWARE_TIMER_EVENT\" type=\"string\">\n\                       
                       <input type=\"submit\" value=\" Send \"></form>\n\
                       </body></html>";
const char *busyPage =
  "<html><body>This server is busy, please try again later.</body></html>";
const char *completePage =
  "<html><body>The upload has been completed.</body></html>";
const char *errorPage =
  "<html><body>This doesn't seem to be right.</body></html>";
const char *serverErrorPage =
  "<html><body>Invalid request.</body></html>";
const char *fileExistsPage =
  "<html><body>This file already exists.</body></html>";
const char *fileIOError =
  "<html><body>IO error writing to disk.</body></html>";
const char*const postProcError =
  "<html><head><title>Error</title></head><body>Error processing POST data</body></html>";
const char *GETPage =
  "<html><body>Use POST with string or form to update your config.</body></html>";
const char*const noParaError =
  "<html><head><title>ParaError</title></head><body>POST URL has no parameters</body></html>";

const char *keys[] = {"RSUNAME", "RSUID", "RSULONG", "RSULAT",\
                      "SIGNAL_CONTROL_MANUFACTURER", "SIGNAL_STATUS_REPORT_ACTIVE",\
                      "SIGNAL_ADJUST_UPPER_BOUND_ACTIVE", "SIGNAL_ADJUST_LOWER_BOUND_ACTIVE",\
                      "SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE", "SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE",\
                      "LOG_MIDDLEWARE_TIMER_EVENT", "LOG_APPLICATION_REGISTER_EVENT",\
                      "LOG_COMMAND_BUFFER", "LOG_SIGNAL_PACKET_RX",\
                      "LOG_SIGNAL_PACKET_TX", "LOG_SIGNAL_PACKET_INFO",\
                      "LOG_CLOUD_PACKET_RX", "LOG_CLOUD_PACKET_TX",\
                      "LOG_OBU_PACKET_RX", "LOG_OBU_PACKET_TX",\
                      "TRAFFIC_COMPENSATION_METHOD","TRAFFIC_COMPENSATION_CYCLE_NUMBER",\
                      "PHASE_WEIGHT","WriteToFile"};
/*write function for CURL*/
static size_t
copyBuffer (void *ptr, size_t size, size_t nmemb, void *ctx)
{
  struct CBC *cbc = ctx;

  if (cbc->pos + size * nmemb > cbc->size)
    return 0;                   /* overflow */
  memcpy (&cbc->buf[cbc->pos], ptr, size * nmemb);
  cbc->pos += size * nmemb;
  return size * nmemb;
}

/*analyze url to check POST data type*/
static enum MHD_Result
parse_url (void *cls,
	     enum MHD_ValueKind kind,
	     const char *key,
	     const char *value)
{
  int num_of_keys = 0;
  int matches = 0;

  printf("key: %s\n",key);
  printf("value: %s\n",key);

  if (0 == strcmp (key, "file"))
  {
    matches = 99; //unique value for FILE posts
    num_of_keys += 1;
    return matches;
  }
  else if (0 == strcmp (key, "")) {
    printf("error: no keys obtained!");
    matches = -1;
    return matches;
  }
  else 
  {
    matches = 0; // value for string/form posts
    num_of_keys += 1;
  }

  printf("amount of keys: %d\n",num_of_keys);
  
  if (num_of_keys==0) {
    printf("error: no keys obtained!");
    matches = -1;
    return matches;
  }
  return matches;
  
}

/* parse string sent with POST */
/**
 * Note that this post_iterator is not perfect
 * in that it fails to support incremental processing.
 * (to be fixed in the future)
 */
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

  if (key==NULL || strlen(key)==0) {
    printf("error: cannot accept empty key\n");
    return MHD_NO;
  }
  //printf("key: %s\n",key);
  //printf("value: %s\n",value);
  //printf("content_type: %s\n",content_type);
  //printf("size=%zu\n",size);

  //const char* Jstring;
  /*read JSON string*/
  if (0 == strcasecmp(key,"json")) {    
    printf("read JSON string\n");
    struct json_object *root; 
    struct json_object *tmp_obj, *name, *ID, *filename; // child nodes, freed when parent is frees
    root = json_tokener_parse(value);
    // Use is_error() to check the result, don't use "j_root == NULL".
    if (is_error(root)) {
      printf("parse failed.");
      exit(-1);
    }
    /*keys that must exist*/
    name = json_object_object_get(root, "RSUNAME");
    ID = json_object_object_get(root, "RSUID");
    filename = json_object_object_get(root, "WriteToFile");
    if (!name || !ID|| !filename ) {
      printf("parse failed.");
      json_object_put(root);
      return MHD_NO;
    }

    enum json_type type = json_object_get_type(root);
    char bufferNum[200];
    int tmpI;
    double tmpD;
    /*loop through JSON object*/
    json_object_object_foreach(root, key, val) {
      type = json_object_get_type(val);
      char *Jtype = json_type_to_name(type);
      printf("type: %s\n",Jtype);
      printf("key: %s\n",key);
      tmp_obj = json_object_object_get(root, key);
      
      switch (type) {
      case json_type_null:
        printf("json_type_null\n");
        break;
      case json_type_boolean:
        printf("json_type_boolean\n");
        const char* s = (json_object_get_boolean(tmp_obj) == true) ? "yes" : "no";
        strncat(con_info->value,key,strlen(key)+1);
        strcat(con_info->value,": ");
        strncat(con_info->value,s,strlen(s)+1);
        strcat(con_info->value,"\n");
        break;
      case json_type_double:
        printf("json_type_double\n");
        //char bufferD[100];
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
        printf("json_type_int\n");
        tmpI = json_object_get_int(tmp_obj);
        printf("int value: %d\n",tmpI);  
        snprintf(bufferNum, sizeof(bufferNum),"%d", tmpI);
        strncat(con_info->value,key,strlen(key)+1);
        strcat(con_info->value,": ");
        strncat(con_info->value,bufferNum,strlen(bufferNum)+1);
        strcat(con_info->value,"\n");
        strcpy(bufferNum,"");
        break;
      case json_type_object:
        printf("json_type_object\n");
        break;
      case json_type_array:
        printf("json_type_array\n");
        printf("array size = %d\n", json_object_array_length(tmp_obj));
        if (0==strcmp(key,"PHASE_WEIGHT")) {
          json_object *tmp_obj1 = NULL;
          int i;
          char buffertemp[200];
          for (i=0; i<json_object_array_length(tmp_obj); i++){
            tmp_obj1 = json_object_array_get_idx(tmp_obj, i);
            tmpI = json_object_get_int(tmp_obj1);
            printf("array int value at ID [%d]: %d\n",i,tmpI);
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
        printf("json_type_string\n");
        printf("%s = %s\n", key, json_object_get_string(tmp_obj));
        if (0!=strcasecmp(key,"WriteToFile")) {
          strncat(con_info->value,key,strlen(key)+1);
          strcat(con_info->value,": ");
          strcat(con_info->value,json_object_get_string(tmp_obj));
          strcat(con_info->value,"\n");
        }
        else {
          strcat(con_info->filename,json_object_get_string(tmp_obj));
        }
        //*tmp = 0;
        break;
      }
    }
    //char *str;
    //str = (char *) json_object_get_string(filename);
    //printf("JSON filename: %s\n",str);
    //strcat(con_info->filename,str);
    //json_object_put(con_info->json_root);
    json_object_put(root);
  }
  else if (0 == strcasecmp(key,"WriteToFile")){ // key is config filename
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

/* send file through POST */
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
      con_info->answerstring = fileExistsPage;
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

/*return web page upon completed response*/
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
  //new a integer
	tmp_obj = json_object_new_int(status_code);
	if (!tmp_obj)
	{
		printf("Cannot create number object for %s\n", "response");
		ret = -1;
    json_object_put(json_obj);
		return MHD_NO;
    //goto error;
	}
	json_object_object_add(json_obj, "response", tmp_obj);
	tmp_obj = NULL;

  JSONlen = json_object_get_string_len(json_obj);
  printf("JSON length: %d\n",JSONlen);
  char *buffer = json_object_to_json_string(json_obj);
  printf("JSON return: %s\n",buffer);
  
  response =
    MHD_create_response_from_buffer (strlen(buffer),
                                     (void*)buffer,
                                     MHD_RESPMEM_MUST_COPY);
  if (! response)
    return MHD_NO;
  /*
  MHD_add_response_header (response,
                           MHD_HTTP_HEADER_CONTENT_TYPE,
                           "text/html");
  */
  MHD_add_response_header (response,
                           "content-type",
                           "application/json");                         
  ret = MHD_queue_response (connection,
                            status_code,
                            response);
  printf("created response\n"); 
  MHD_destroy_response (response);
  json_object_put(json_obj);

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
    printf("POST type post-complete:\n");
    memset(&con_info->value[0], 0, sizeof(con_info->value)); // reset connection info POST value
    memset(&con_info->filename[0], 0, sizeof(con_info->filename));
    //json_object_put(con_info->json_root);
    if (NULL != con_info->pp)
    {
      MHD_destroy_post_processor (con_info->pp);  
    }

    if (con_info->fp)
      fclose (con_info->fp);
  }

  else {
      printf("connection type is not POST\n");
  }
  //con_info->connectiontype = NULL;
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
  //struct MHD_Response *response;
  //enum MHD_result ret;
  static int posttype;
  struct JSONhandler *jason;
  static int content_len=0;
  
  printf("URL: %s\n",url);
  printf("method: %s\n",method);
  //printf("size: %zu\n",upload_data_size);
  //printf("upload_data: %s\n",upload_data);
  //printf(connection->headers_received);
  
  
  if (NULL == *con_cls)
  {
    /* First call, setup data structures */
    struct connection_info_struct *con_info;

    if (nr_of_uploading_clients >= MAXCLIENTS)
      return send_page (connection,
                        busyPage,
                        MHD_HTTP_SERVICE_UNAVAILABLE);

    con_info = malloc (sizeof (struct connection_info_struct));
    if (NULL == con_info)
      return MHD_NO;
    con_info->answercode = 0;   /* none yet */
    con_info->fp = NULL;
    con_info->posttype = -1;
    strcpy(con_info->value,"");
    strcpy(con_info->filename,"");
    // parse url for "file" key
    con_info->posttype = MHD_get_connection_values (connection,
          MHD_GET_ARGUMENT_KIND,
          &parse_url,
          url);
    
    if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST))
    {
      const char* param = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
      content_len = atoi(param);
      const char* type = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_TYPE);
      const char* body = MHD_lookup_connection_value (connection, MHD_POSTDATA_KIND, NULL);
      printf("length of content: %d\n",content_len);
      printf("type of content: %s\n",type);
      //printf(strcmp(type,"application/json"));
      if (con_info->posttype==99) { // has "file" key
        printf("handle as file POST\n");
        con_info->pp =
          MHD_create_post_processor (connection,
                                     POSTBUFFERSIZE,
                                     &iterate_post_file,
                                     (void *) con_info);
      }
      else if (con_info->posttype == -1) { // empty URL without parameters
        printf("POST error: require parameters\n");
        return send_page (connection,
                        noParaError,
                        MHD_HTTP_SERVICE_UNAVAILABLE); // HTTP 503
      }
      else { // no "file" key, POST data is string/form/JSON
        printf("handle as string POST\n");
        printf("con_info->posttype: %d\n",con_info->posttype);
        if (0==strcmp(type,"application/json")) // new connection
        {
          con_info->posttype = 2;
          printf("accept type: JSON");
          printf("con_info->posttype: %d\n",con_info->posttype);
          jason = (char*)malloc (MAX_JSON_SIZE);
          if (NULL == jason)
              return MHD_NO;

          jason->len = 0; //actual data load
          jason->data = malloc(content_len + 1);
          memset(jason->data, 0, content_len+1);
        }
        else {
          con_info->pp =
          MHD_create_post_processor (connection,
                                     POSTBUFFERSIZE,
                                     &iterate_post_string,
                                     (void *) con_info);
        }        
      }
      if (content_len==0) { // empty POST
        printf("POST error: require parameters\n");
        return send_page (connection,
                        noParaError,
                        MHD_HTTP_SERVICE_UNAVAILABLE); // HTTP 503
      }
      if (NULL == con_info->pp)
      {
        free (con_info);
        return MHD_NO;
      }
      nr_of_uploading_clients++;
      con_info->connectiontype = POST;      
    }

    else
    {
      con_info->connectiontype = GET;
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
    con_info->answerstring = GETPage;
    printf("GET operation\n");
    //free (con_info);
    return send_page (connection,
                      GETPage,
                      con_info->answercode);
  }

  if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST)) {
    struct connection_info_struct *con_info = *con_cls;
    
    if (0 != *upload_data_size)
    {
      const char* param = MHD_lookup_connection_value (connection, MHD_HEADER_KIND, MHD_HTTP_HEADER_CONTENT_LENGTH);
      content_len = atoi(param);
      const char* body = MHD_lookup_connection_value (connection, MHD_POSTDATA_KIND, NULL);
      
      /* Upload not yet done */
      if (0 != con_info->answercode)
      {
        /* we already know the answer, skip rest of upload */
        *upload_data_size = 0;
        return MHD_YES;
      }
      /* use custom method for JSON */
      if (0 == strncasecmp(url, "/api/json", 9)) { 
        memcpy (jason->data + jason->len, upload_data, *upload_data_size);
        jason->len = jason->len + (*upload_data_size);
        ((char *)(jason->data))[jason->len] = '/0';
      }
      /* use MHD method for string and form type */
      else { 
        if (MHD_YES !=
          MHD_post_process (con_info->pp,
                            upload_data,
                            *upload_data_size))
        {
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
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
    if (con_info->value == NULL) {
      con_info->answerstring = postProcError;
      con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
    }
    if (0 == con_info->answercode)
    {
      /* No errors encountered, declare success */
      printf("string upload success\n");
      con_info->answerstring = completePage;
      con_info->answercode = MHD_HTTP_OK;
      if (con_info->posttype==0) 
      { // write string to file
        printf("writing file\n");
        FILE *fp;
        char *filename = (char*)malloc(sizeof(100));
        *filename = 0;
        //strncat(filename,"RSUID_",strlen("RSUID_")+1);
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
      else if (con_info->posttype==2) {
        // write JSON to file
        if (content_len != jason->len) {
          con_info->answerstring = postProcError;
          con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
        }
        else {
          printf(jason->data);
          json_object *test_obj = jason->data;
          json_object *tmp_obj = NULL;
          json_object *tmp_obj2 = NULL;
          tmp_obj = json_object_object_get(test_obj, "RSUID");
          if (!tmp_obj)
          {
            printf("Cannot get RSUID object\n");
            con_info->answerstring = postProcError;
            con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
          }
	        printf("RSUID = %s\n", json_object_get_string(tmp_obj));

          tmp_obj2 = json_object_object_get(test_obj, "WriteToFile");
          if (!tmp_obj2)
          {
            printf("Cannot get filename\n");
            con_info->answerstring = postProcError;
            con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
          }
          else {
            printf("filename = %s\n", json_object_get_string(tmp_obj2));
            char *filename = (char*)malloc(sizeof(100));
            *filename = 0;
            //strncat(filename,"RSUID_",strlen("RSUID_")+1);
            strncat(filename,tmp_obj2,strlen(tmp_obj2)+1);
            strncat(filename,".json",strlen(".json")+1);
            json_object_to_file(filename, test_obj);
            *filename = 0;
            free(filename);
          }
	        
        }
      }
      else {
        printf("file upload success");
      }        
    }
    return send_page (connection,
                      con_info->answerstring,
                      con_info->answercode);
    
  }
  /* Note a GET or a POST, generate error */
  return send_page (connection,
                    errorPage,
                    MHD_HTTP_BAD_REQUEST);
}

static int ahc_cancel (void *cls,
	    struct MHD_Connection *connection,
	    const char *url,
	    const char *method,
	    const char *version,
	    const char *upload_data, size_t *upload_data_size,
	    void **unused)
{
  struct MHD_Response *response;
  int ret;

  if (*unused == NULL)
    {
      *unused = "wibble";
      /* We don't want the body. Send a 500. */
      response = MHD_create_response_from_buffer (0, NULL, 
						  MHD_RESPMEM_PERSISTENT);
      ret = MHD_queue_response(connection, 500, response);
      if (ret != MHD_YES)
      {
	      fprintf(stderr, "Failed to queue response\n");
      }
      MHD_destroy_response(response);
      return ret;
    }
  else
    {
      fprintf(stderr, 
	      "In ahc_cancel again. This should not happen.\n");
      return MHD_NO;
    }
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
