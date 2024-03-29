#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef WINDOWS
#include <unistd.h>
#endif

//#define POST_DATA "name=daniel&project=curl"
#define POST_DATA "RSUNAME=S290002&RSUID=16004&SIGNAL_CONTROLLER_MANUFACTURER=cheng_long\
                  &RSULAT=25.221324&RSULONG=124.958430"
#define EMPTY_POST ""
#define PORT            8888
#define POSTBUFFERSIZE  2048

//#define SERVER_URL "http://127.0.0.1:8888/api/"
#define SERVER_URL "http://140.116.245.163:10168/api/"
//#define SERVER_URL "http://127.0.0.1:10168/api/"
#define SERVER_URL_ex "https://example.com"

//empty curl request: curl --request POST '140.116.245.163:10168/api'
static int oneone;

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

static curl_mime *
make_form (void *curl)
{
  curl_mime *form = NULL;
  curl_mimepart *field = NULL;
  /* Create the form */
  form = curl_mime_init(curl);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "RSUNAME");
  curl_mime_data(field, "S290001   ", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "RSUID");
  curl_mime_data(field, "16001", CURL_ZERO_TERMINATED);
  
  field = curl_mime_addpart(form);
  curl_mime_name(field, "RSULONG");
  curl_mime_data(field, "125.673635", CURL_ZERO_TERMINATED);
  
  field = curl_mime_addpart(form);
  curl_mime_name(field, "RSULAT");
  curl_mime_data(field, "23.34212", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_CONTROL_MANUFACTURER");
  curl_mime_data(field, "shan_zhu", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_STATUS_REPORT_ACTIVE");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_ADJUST_UPPER_BOUND_ACTIVE");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_ADJUST_LOWER_BOUND_ACTIVE");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_ADJUST_UPPER_BOUND_PERCENTAGE");
  curl_mime_data(field, "100", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "SIGNAL_ADJUST_LOWER_BOUND_PERCENTAGE");
  curl_mime_data(field, "50", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_MIDDLEWARE_TIMER_EVENT");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_APPLICATION_REGISTER_EVENT");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_COMMAND_BUFFER");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_SIGNAL_PACKET_RX");
  curl_mime_data(field, "no", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_SIGNAL_PACKET_TX");
  curl_mime_data(field, "no", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_SIGNAL_PACKET_INFO");
  curl_mime_data(field, "no", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_CLOUD_PACKET_RX");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_CLOUD_PACKET_TX");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_OBU_PACKET_RX");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "LOG_OBU_PACKET_TX");
  curl_mime_data(field, "yes", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "TRAFFIC_COMPENSATION_METHOD");
  curl_mime_data(field, "1", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "TRAFFIC_COMPENSATION_CYCLE_NUMBER");
  curl_mime_data(field, "2", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "PHASE_WEIGHT");
  curl_mime_data(field, "50 50 0 0 0", CURL_ZERO_TERMINATED);

  field = curl_mime_addpart(form);
  curl_mime_name(field, "WriteToFile");
  curl_mime_data(field, "16001", CURL_ZERO_TERMINATED);
  return form;
}

static int
testPost ()
{
  CURL *c;
  char buf[2048];
  struct CBC cbc;
  CURLcode errornum;

  cbc.buf = buf;
  cbc.size = 2048;
  cbc.pos = 0;
  c = curl_easy_init ();
  curl_easy_setopt (c, CURLOPT_URL, SERVER_URL);
  curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &copyBuffer);
  curl_easy_setopt (c, CURLOPT_WRITEDATA, &cbc);
  curl_easy_setopt (c, CURLOPT_POSTFIELDS, POST_DATA);
  curl_easy_setopt (c, CURLOPT_POSTFIELDSIZE, strlen (POST_DATA));
  curl_easy_setopt (c, CURLOPT_POST, 1L);
  curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
  if (oneone)
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  else
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
  // NOTE: use of CONNECTTIMEOUT without also
  //   setting NOSIGNAL results in really weird
  //   crashes on my system!
  curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt (c, CURLOPT_VERBOSE, 1L);
  if (CURLE_OK != (errornum = curl_easy_perform (c)))
    {
      fprintf (stderr,
               "curl_easy_perform failed: `%s'\n",
               curl_easy_strerror (errornum));
      curl_easy_cleanup (c);
      return 2;
    }
  curl_easy_cleanup (c);
  printf("cbc.pos: %zu\n",cbc.pos);
  printf("cbc.buf: %s\n",cbc.buf);
  return 0;
}

static int
testPostForm ()
{
  CURL *c;
  char buf[2048];
  struct CBC cbc;
  CURLcode errornum;
  cbc.buf = buf;
  cbc.size = 2048;
  cbc.pos = 0;
  
  c = curl_easy_init ();
  curl_mime *form = NULL;
  form = make_form(c);
  char url[] = SERVER_URL;
  strncat(url,"form",strlen("form")+1);
  curl_easy_setopt (c, CURLOPT_URL, url);
  curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &copyBuffer);
  curl_easy_setopt (c, CURLOPT_WRITEDATA, &cbc);
  curl_easy_setopt (c, CURLOPT_MIMEPOST, form);
  curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
  if (oneone)
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  else
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
  // NOTE: use of CONNECTTIMEOUT without also
  //   setting NOSIGNAL results in really weird
  //   crashes on my system!
  curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt (c, CURLOPT_VERBOSE, 1L);
  if (CURLE_OK != (errornum = curl_easy_perform (c)))
    {
      fprintf (stderr,
               "curl_easy_perform failed: `%s'\n",
               curl_easy_strerror (errornum));
      curl_easy_cleanup (c);
      return 2;
    }
  curl_easy_cleanup (c);
  printf("cbc.pos: %zu\n",cbc.pos);
  printf("cbc.buf: %s\n",cbc.buf);
  
  return 0;
}

static int
testGet ()
{
  CURL *c;
  char buf[2048];
  struct CBC cbc;
  CURLcode errornum;

  cbc.buf = buf;
  cbc.size = 2048;
  cbc.pos = 0;
  
  c = curl_easy_init ();
  curl_easy_setopt (c, CURLOPT_URL, SERVER_URL);
  //curl_easy_setopt (c, CURLOPT_WRITEFUNCTION, &copyBuffer);
  //curl_easy_setopt (c, CURLOPT_WRITEDATA, &cbc);
  curl_easy_setopt (c, CURLOPT_FAILONERROR, 1);
  curl_easy_setopt (c, CURLOPT_TIMEOUT, 150L);
  if (oneone)
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
  else
    curl_easy_setopt (c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
  curl_easy_setopt (c, CURLOPT_CONNECTTIMEOUT, 150L);
  // NOTE: use of CONNECTTIMEOUT without also
  //   setting NOSIGNAL results in really weird
  //   crashes on my system!
  curl_easy_setopt (c, CURLOPT_NOSIGNAL, 1);
  curl_easy_setopt (c, CURLOPT_VERBOSE, 1L);
  if (CURLE_OK != (errornum = curl_easy_perform (c)))
    {
      fprintf (stderr,
               "curl_easy_perform failed: `%s'\n",
               curl_easy_strerror (errornum));
      curl_easy_cleanup (c);
      return 2;
    }
  curl_easy_cleanup (c);
  //printf("cbc.pos: %zu\n",cbc.pos);
  //printf("cbc.buf: %s\n",cbc.buf);
  return 0;
}

int
main (int argc, char *const *argv)
{
  unsigned int errorCount = 0;
  errorCount += testPost ();
  //errorCount += testGet ();
  //errorCount += testPostForm ();
  //errorCount += testGet ();
  if (errorCount != 0)
    fprintf (stderr, "Error (code: %u)\n", errorCount);
  curl_global_cleanup ();
  return errorCount != 0;
}
