#pragma once

#include "task.h"

/*
 * mom_publish_init - Initialize internal workqueues for the MOM
 * @return 0 on success, negative error code on failure.
 */
int mom_publish_init(char *addresses_str);
/*
 * mom_publish_start - Start the MOM publish process
 * @s: socket to use for the publish process
 */
int mom_publish_start(struct socket *s);

void mom_publish_free_wq(void);