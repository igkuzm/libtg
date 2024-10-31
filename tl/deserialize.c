#include "tl.h"
#include "deserialize_table.h"
#include "../mtx/include/api.h"
#include <string.h>

ui32_t deserialize_ui32(buf_t *b){
	ui32_t c;
	c = buf_get_ui32(*b);
	*b = buf_add(b->data + 4, b->size - 4);
	return c;
}

ui32_t deserialize_ui64(buf_t *b){
	ui32_t c;
	c = buf_get_ui64(*b);
	*b = buf_add(b->data + 8, b->size - 8);
	return c;
}

buf_t deserialize_bytes(buf_t *b)
{
	//buf_dump(*b);
  buf_t s;
	//memset(&s, 0, sizeof(buf_t));
  buf_t byte = buf_add(b->data, 4);
  int offset = 0;
  ui32_t len = 0;

  if (byte.data[0] <= 253) {
    len = byte.data[0];
		// skip 1 byte
		*b = buf_add(b->data + 1, b->size - 1);
		s = buf_add(b->data, len);
		offset = 1;
  } else if (byte.data[0] >= 254) {
    ui8_t start = 0xfe;
    buf_t s1 = buf_add((ui8_t *)&start, 1);
    buf_t s2 = buf_add(b->data, 1);

    if (!buf_cmp(s1, s2)) {
      api.log.error("can't deserialize bytes");
    }

    buf_t len_ = buf_add(b->data + 1, 3);
    len_.size = 4; // hack
    len = buf_get_ui32(len_);
		// skip 4 bytes
		*b = buf_add(b->data + 4, b->size - 4);
    s = buf_add(b->data, len);
  } else {
    api.log.error("can't deserialize bytes");
  }
	
	*b = buf_add(b->data + len, b->size - len);
  
	// padding
	int pad = (4 - ((len + offset) % 4)) % 4;
	if (pad) {
		*b = buf_add(b->data + pad, b->size - pad);
	}

  return s;
}

static tl_deserialize_function *get_fun(unsigned int id){
	int i,
			len = sizeof(tl_deserialize_table)/
				    sizeof(*tl_deserialize_table);

	for (i = 0; i < len; ++i)
		if(tl_deserialize_table[i].id == id)
			return tl_deserialize_table[i].fun;

	return NULL;
}

tl_t * tl_deserialize(buf_t *buf)
{
	ui32_t *id = (ui32_t *)(buf->data);
	if (!*id)
		return NULL;

	// find id in deserialize table
	tl_deserialize_function *fun = get_fun(*id);
	if (!fun){
		printf("can't find deserialization"
				" function for id: %.8x\n", *id);
		return NULL;
	}

	// run deserialization function
	return fun(buf);
}


ui32_t id_from_tl_buf(buf_t tl_buf){
	return *(ui32_t *)tl_buf.data;
}

mtp_message_t deserialize_mtp_message(buf_t *b){
	mtp_message_t msg; 
	memset(&msg, 0, sizeof(mtp_message_t));
	
	msg.msg_id = buf_get_ui64(*b);
	*b = buf_add(b->data + 8, b->size - 8);
	printf("mtp_message msg_id: %0.16lx\n", msg.msg_id);

	msg.seqno = buf_get_ui32(*b);
	*b = buf_add(b->data + 4, b->size - 4);
	printf("mtp_message seqno: %0.8x\n", msg.seqno);

	msg.bytes = buf_get_ui32(*b);
	*b = buf_add(b->data + 4, b->size - 4);
	printf("mtp_message bytes: %d\n", msg.bytes);

	strncpy(msg.body, (char *)b->data, msg.bytes);
	printf("mtp_message body: %s\n", msg.body);
	*b = buf_add(b->data + msg.bytes, b->size - msg.bytes);
	return msg;
}