#pragma once

#include "task.h"

#define MOM_PUBLISH_ACK_FLAG "PUBACK"
#define MOM_PUBLISH_ACK_FLAG_LEN 6

/*
 * mom_publish_init - Initialize internal workqueues for the MOM
 * @return 0 on success, negative error code on failure.
 */
int mom_publish_init(char *addresses_str);
/*
 * mom_publish_start - Start the MOM publish process
 * @s: socket to use for the publish process
 */
int mom_publish_start(struct socket *s, char *ack_flag_msg, int ack_flag_msg_len);

void mom_publish_free(void);
