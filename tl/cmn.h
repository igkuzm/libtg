/**
 * File              : cmn.h
 * Author            : Igor V. Sementsov <ig.kuzm@gmail.com>
 * Date              : 22.11.2024
 * Last Modified Date: 22.11.2024
 * Last Modified By  : Igor V. Sementsov <ig.kuzm@gmail.com>
 */
#ifndef TL_CMN_H
#define TL_CMN_H

#include "buf.h"

extern void tl_cmn_fact(uint64_t pq, uint32_t * p, uint32_t * q);
extern buf_t tl_cmn_pow_mod(buf_t g, buf_t e, buf_t m);

#endif /* defined(TL_CMN_H) */
