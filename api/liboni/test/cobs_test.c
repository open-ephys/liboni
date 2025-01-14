#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>

#include "testfunc.h"
#include "../oni.c" // Access static _oni_cobs_unstuff function

int main(int argc, char *argv[])
{
    // String to encode/decode
    const char *msg = "Hello world.";

    // Stuff
    const size_t msg_len = strlen(msg) + 1;
    uint8_t *en_buf = malloc(msg_len);
    int rc = cobs_stuff(en_buf, (uint8_t *)msg, msg_len);
    if (rc) { printf("Error stuffing packet: %s\n", oni_error_str(rc)); }
    assert(rc == 0);

    // Unstuff
    const size_t buf_len = 256;
    uint8_t *de_buf = malloc(buf_len);
    rc = _oni_cobs_unstuff(de_buf, en_buf, msg_len);
    if (rc) { printf("Error unstuffing packet: %s\n", oni_error_str(rc)); }
    char *de_msg = (char *)de_buf;
    assert(rc == 0);

    printf("Send (%zu bytes): %s\n", msg_len, msg);
    printf("Recv (%zu bytes): %s\n", strlen(de_msg) + 1, de_buf);
    assert(strcmp(msg, de_msg) == 0);

    free(en_buf);
    free(de_buf);

    printf("Success.\n");

    return 0;
}
