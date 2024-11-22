/**
 * File              : hsh.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 21.11.2024
 * Last Modified Date: 21.11.2024
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */
#ifndef TL_HSH_H
#define TL_HSH_H

#include "buf.h"

extern buf_t tl_hsh_sha1(buf_t b);
extern buf_t tl_hsh_sha256(buf_t b);

#endif /* defined(TL_HSH_H) */
