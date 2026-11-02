"""Exercise the actual upload callback with HTTP/1.1-style header delivery."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class UploadTest(unittest.TestCase):
    def test_fragmented_upload_and_header_reset(self):
        source = (ROOT / 'src/configurator_network.c').read_text()
        callback = source[source.index('static int upload_handler('):
                          source.index('static struct http_resource_detail_dynamic upload_resource')]
        harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#define KEYBOARD_CONFIG_MAX_BLOB_SIZE 2496
#define ARG_UNUSED(x) (void)(x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))
#define LOG_ERR(...) ((void)0)
enum http_transaction_status { HTTP_SERVER_REQUEST_DATA_MORE,
 HTTP_SERVER_REQUEST_DATA_FINAL, HTTP_SERVER_TRANSACTION_ABORTED,
 HTTP_SERVER_TRANSACTION_COMPLETE };
enum { HTTP_200_OK=200, HTTP_400_BAD_REQUEST=400,
 HTTP_413_PAYLOAD_TOO_LARGE=413, HTTP_415_UNSUPPORTED_MEDIA_TYPE=415,
 HTTP_500_INTERNAL_SERVER_ERROR=500 };
struct http_client_ctx { int unused; };
struct http_header { const char *name; const char *value; };
struct http_request_ctx { uint8_t *data; size_t data_len;
 struct http_header *headers; size_t header_count; };
struct http_response_ctx { int status; const struct http_header *headers;
 size_t header_count; const uint8_t *body; size_t body_len; bool final_chunk; };
static int saves;
static int keyboard_config_import(const uint8_t *data, size_t len) {
 assert(len==4 && memcmp(data,"EKCF",4)==0); ++saves; return 0;
}
'''
        cases = r'''
int main(void) {
 struct http_client_ctx client = {0};
 struct http_response_ctx response = {0};
 struct http_header header = {"content-type", "Application/Octet-Stream"};
 struct http_request_ctx request = {(uint8_t *)"EK",2,&header,1};
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_MORE,&request,&response,NULL);
 assert(saves==0 && !response.final_chunk);
 request=(struct http_request_ctx){(uint8_t *)"CF",2,NULL,0};
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_FINAL,&request,&response,NULL);
 assert(saves==1 && response.status==200);
 upload_handler(&client,HTTP_SERVER_TRANSACTION_COMPLETE,NULL,NULL,NULL);
 request=(struct http_request_ctx){(uint8_t *)"EKCF",4,NULL,0};
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_FINAL,&request,&response,NULL);
 assert(saves==1 && response.status==415);
 upload_handler(&client,HTTP_SERVER_TRANSACTION_COMPLETE,NULL,NULL,NULL);
 header.value="application/json"; request.headers=&header; request.header_count=1;
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_FINAL,&request,&response,NULL);
 assert(saves==1 && response.status==415);
 upload_handler(&client,HTTP_SERVER_TRANSACTION_COMPLETE,NULL,NULL,NULL);
 header.value="application/octet-stream";
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_MORE,&request,&response,NULL);
 upload_handler(&client,HTTP_SERVER_TRANSACTION_ABORTED,NULL,NULL,NULL);
 request.headers=NULL; request.header_count=0;
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_FINAL,&request,&response,NULL);
 assert(saves==1 && response.status==415);
 upload_handler(&client,HTTP_SERVER_TRANSACTION_COMPLETE,NULL,NULL,NULL);
 request.headers=&header; request.header_count=1; request.data_len=2497;
 upload_handler(&client,HTTP_SERVER_REQUEST_DATA_FINAL,&request,&response,NULL);
 assert(saves==1 && response.status==413);
 return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            source_path = Path(directory) / 'upload.c'
            binary = Path(directory) / 'upload'
            source_path.write_text(harness + callback + cases)
            subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                            str(source_path), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
