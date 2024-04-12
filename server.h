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
#include <sys/stat.h>
#include<json-c/json.h>
#include<json-c/bits.h>

#define PORT            10168
//#define PORT            10003
#define POSTBUFFERSIZE  4096
#define MAXCLIENTS      10
#define MAX_JSON_SIZE 4096
#define JSON_NUM_BUF 200

static unsigned int nr_of_uploading_clients = 0;

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

const char *mustHaveKeys[] = {"RSUNAME", "RSUID", "WriteToFile"};
const char *fileDir = "./data_folder/";

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
  "This server is busy, please try again later.";
const char *completePage =
  "The upload has been completed.";
const char *completePUT =
  "Update completed. (warning: PUT/DELETE currently doesn't affect server data)";
const char *GETPage =
  "Please update future config with POST method.";
const char *errorPage =
  "This doesn't seem to be right.";
const char *serverErrorPage =
  "Invalid request.";
const char *fileExistsMessage =
  "This file already exists.";
const char *fileIOError =
  "IO error writing to disk.";
const char *postProcError =
  "Error processing POST data.";
const char *dataTypeError =
  "Unsupported data type.";
const char*const noParaError =
  "POST URL has no parameters.";
//const char*const illJSONError =
//  "JSON is missing one of the following keys: RSUNAME/RSUID/WriteToFile";
const char *illJSONError = "JSON is missing one of the following keys: ";

const char *MissKeyName = "JSON is missing RSUNAME";
const char *MissKeyID = "JSON is missing RSUID";
const char *MissKeyFile = "JSON is missing WriteToFile";
const char *MissKeyNameID = "JSON is missing RSUNAME and RSUID";
const char *MissKeyIDFile = "JSON is missing RSUID and WriteToFile";
const char *MissKeyNameFile = "JSON is missing RSUNAME and WriteToFile";


const char*const noContentError =
  "POST/PUT URL has no content.";
const char *GETMessage =
  "GET data acquired.";
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
  char value[MAX_JSON_SIZE];

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

  struct JSONhandler *JSONreader;
  /**
   * check missing JSON keys.
   */
  //int missKey[3];
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
