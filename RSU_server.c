
//#include "MHD_config.h"
//#include "platform.h"
//#include <curl/curl.h>
#include <microhttpd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#endif

//#define POST_DATA "RSUNAME=S290001&RSUID=16004&SIGNAL_CONTROLLER_MANUFACTURER=cheng_long"

#define PORT            10168
//#define PORT            10003
#define POSTBUFFERSIZE  2048
#define MAXCLIENTS      10

/*write data buffer*/
struct CBC
{
  char *buf;
  size_t pos;
  size_t size;
};

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

enum ConnectionType
{
  GET = 0,
  POST = 1,
  PUT = 2,
  DELETE = 3,
  OPTIONS = 4
};

static unsigned int nr_of_uploading_clients = 0;
/**
 * Information we keep per connection.
 */
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
  char value[1024];

  int posttype;

  char *filename[100];
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


static enum MHD_Result
parse_url (void *cls,
	     enum MHD_ValueKind kind,
	     const char *key,
	     const char *value)
{
  int num_of_keys = 0;
  int matches = 0;

  printf("key: %s\n",key);
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
    matches = 0;
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
    printf("error: cannot accept empty key\n")
    return MHD_NO;
  }
  printf("key: %s\n",key);
  printf("value: %s\n",value);
  //printf("content_type: %s\n",content_type);
  //printf("size=%zu\n",size);
  if (0 != strcasecmp(key,"WriteToFile")) { // key is not WriteToFile (config filename)
    strncat(con_info->value,key,strlen(key)+1);
    strcat(con_info->value,": ");
    strcat(con_info->value,value);
    strcat(con_info->value,"\n");
  }
  else {
    printf("bring file\n");
    strcat(con_info->filename,value);
  }
  
  return MHD_YES;
}

/* send file through POST */
static enum MHD_Result
iterate_post_file (void *coninfo_cls,
              enum MHD_ValueKind kind,
              const char *key,
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data,
              uint64_t off,
              size_t size)
{
  struct connection_info_struct *con_info = coninfo_cls;
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

  response =
    MHD_create_response_from_buffer (strlen (page),
                                     (void *) page,
                                     MHD_RESPMEM_MUST_COPY);
  if (! response)
    return MHD_NO;
  MHD_add_response_header (response,
                           MHD_HTTP_HEADER_CONTENT_TYPE,
                           "text/html");
  ret = MHD_queue_response (connection,
                            status_code,
                            response);
  MHD_destroy_response (response);

  return ret;
}



static void
request_completed (void *cls,
                   struct MHD_Connection *connection,
                   void **con_cls,
                   enum MHD_RequestTerminationCode toe)
{
  printf("request complete\n");
  
  struct connection_info_struct *con_info = *con_cls;
  enum XToe = toe;
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
  //(void) cls;         /* Unused. Silent compiler warning. */
  //(void) connection;  /* Unused. Silent compiler warning. */
  //(void) toe;         /* Unused. Silent compiler warning. */
  
  if (NULL == con_info) {
    printf("connection is already freed or unavailable\n");
    *con_cls = NULL;
    return;
  }

  if (1 == (con_info->connectiontype == POST))// is POST
  {
    nr_of_uploading_clients--;
    printf("POST type post-complete:\n");
    memset(&con_info->value[0], 0, sizeof(con_info->value)); // reset connection info POST value
    memset(&con_info->filename[0], 0, sizeof(con_info->filename));
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

static enum MHD_result
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
  
  printf("URL: %s\n",url);
  printf("method: %s\n",method);
  printf("upload_data: %s\n",upload_data);
  
  
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
          NULL);
    if (0 == strcasecmp (method, MHD_HTTP_METHOD_POST))
    {
      
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
      else { // no "file" key, POST data is string type or form type
        printf("handle as string POST\n");
        printf("con_info->posttype: %d\n",con_info->posttype);
        con_info->pp =
          MHD_create_post_processor (connection,
                                     POSTBUFFERSIZE,
                                     &iterate_post_string,
                                     (void *) con_info);
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
      /* Upload not yet done */
      if (0 != con_info->answercode)
      {
        /* we already know the answer, skip rest of upload */
        *upload_data_size = 0;
        return MHD_YES;
      }
      if (MHD_YES !=
          MHD_post_process (con_info->pp,
                            upload_data,
                            *upload_data_size))
      {
        con_info->answerstring = postProcError;
        con_info->answercode = MHD_HTTP_INTERNAL_SERVER_ERROR;
      }
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
    if (con_info->value == "" || con_info->value == NULL) {
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
        //char ID[10] = "S290001";
        //strncat(filename,"RSUID_",strlen("RSUID_")+1);
        strncat(filename,con_info->filename,strlen(con_info->filename)+1);
        strncat(filename,".txt",strlen(".txt")+1);
        //strncat(con_info->filename,".txt",strlen(".txt")+1);
        printf(con_info->value);
        /*
        if (NULL != (fp = fopen (filename, "rb")))
        {
          fclose (fp);
          con_info->answerstring = fileExistsPage;
          con_info->answercode = MHD_HTTP_FORBIDDEN;
          return send_page (connection,
                      con_info->answerstring,
                      con_info->answercode);
        }
        */
        fp = fopen (filename, "wb");
        if (! fprintf (fp,con_info->value))
        {
          printf("write file error\n");
        }
        *filename = 0;
        free(filename);
        fclose(fp);
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



int
main (int argc, char *const *argv)
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
