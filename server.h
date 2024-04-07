#include <microhttpd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <time.h>
#include <stdio.h>
#ifndef WINDOWS
#include <unistd.h>
#endif
#include<json-c/json.h>
#include<json-c/bits.h>

#define PORT            10168
//#define PORT            10003
#define POSTBUFFERSIZE  4096
#define MAXCLIENTS      10
#define MAX_JSON_SIZE 4096
#define JSON_NUM_BUF 200

/*write data buffer*/
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
const char *fileExistsMessage =
  "This file already exists.";
const char *fileIOError =
  "<html><body>IO error writing to disk.</body></html>";
const char*const postProcError =
  "Error processing POST data.";
const char *GETPage =
  "<html><body>Use POST with string or form to update your config.</body></html>";
const char*const noParaError =
  "POST URL has no parameters.";
const char*const noContentError =
  "POST URL has no content.";
const char *GETMessage =
  "Use POST with string/form/JSON to update your config.";
const char*const GetErrorMessage =
  "GET URL should not have Post data.";
struct CBC
{
  char *buf;
  size_t pos;
  size_t size;
};

/*write JSON buffer*/
struct JSONhandler
{
  char *data;
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

static enum MHD_Result 
readJSON (void *cls, 
          const char *upload_data, 
          size_t *upload_data_size);

static enum MHD_Result
parse_url (void *cls,
	     enum MHD_ValueKind kind,
	     const char *key,
	     const char *value);

static enum MHD_Result
iterate_post_string (void *cls,
               enum MHD_ValueKind kind,
               const char *key,
               const char *filename,
               const char *content_type,
               const char *transfer_encoding,
               const char *value, 
               uint64_t off, 
               size_t size);

static enum MHD_Result
iterate_post_file (void *cls,
              enum MHD_ValueKind kind,
              const char *key,
              const char *filename,
              const char *content_type,
              const char *transfer_encoding,
              const char *data,
              uint64_t off,
              size_t size);

static enum MHD_Result
send_page (struct MHD_Connection *connection,
           const char *page,
           int status_code);

static void
request_completed (void *cls,
                   struct MHD_Connection *connection,
                   void **con_cls,
                   enum MHD_RequestTerminationCode toe);

static int
echo_response (void *cls,
          struct MHD_Connection *connection,
          const char *url,
          const char *method,
          const char *version,
          const char *upload_data, size_t *upload_data_size,
          void **con_cls);

static int 
ahc_cancel (void *cls,
	    struct MHD_Connection *connection,
	    const char *url,
	    const char *method,
	    const char *version,
	    const char *upload_data, size_t *upload_data_size,
	    void **unused);