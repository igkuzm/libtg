#ifndef TL_SERIALIZE_H
#define TL_SERIALIZE_H
#include "tl.h"
buf_t serialize_bytes(ui8_t *bytes, size_t size);
buf_t serialize_string(const char *string);
buf_t tl_serialize(tl_t *obj);
#endif /* ifndef TL_SERIALIZE_H */
