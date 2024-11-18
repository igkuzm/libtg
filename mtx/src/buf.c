//
//  buf.c
//  mtx
//
//  Created by Pavel Morozkin on 17.01.14.
//  Copyright (c) 2014 Pavel Morozkin. All rights reserved.
//

#include <stdlib.h>
#include <time.h>
#include "../include/types.h"
#include "../include/api.h"

void buf_init(buf_t *buf)
{
	buf->data = malloc(max_buf_size + 1); 
	buf->allo = max_buf_size;
	buf->size = 0;
}

void buf_realloc(buf_t *buf, ui32_t size)
{
	if (size > buf->allo){
		void *ptr = realloc(buf->data, size);
		if (ptr){
			buf->data = ptr;
			buf->allo = size;
		}
	}
}

buf_t buf_add(ui8_t data[], ui32_t size)
{
  buf_t b;
	buf_init(&b);

  if (size > b.allo) {
		buf_realloc(&b, size + 1);
  }

	ui32_t i;
  for (i = 0; i < size; ++i) {
    b.data[i] = data[i];
  }
	b.data[i] = 0;

  b.size = size;

  return b;
}

buf_t buf_cat(buf_t dest, buf_t src)
{
  ui32_t s = dest.size + src.size;

  if (s > dest.allo) {
		buf_realloc(&dest, s);
  }

  int offset = dest.size;

  for (ui32_t i = 0; i < src.size; ++i) {
    dest.data[i + offset] = src.data[i];
  }

  dest.size = s;

  return dest;
}

void buf_dump(buf_t b)
{
  if (b.size > b.allo) {
    api.log.error("Error: buf_dump: max size reached");
  } else if (!b.size) {
    api.log.error("Error: buf_dump: buffer is empty");
  }

  api.log.hex(b.data, b.size);
}

ui8_t buf_cmp(buf_t a, buf_t b)
{
  if (a.size != b.size) {
    api.log.error("Error: buf_cmp: different sizes");
  }

  for (ui32_t i = 0; i < a.size; ++i) {
    if (a.data[i] != b.data[i]) {
      return 0;
    }
  }

  return 1;
}

buf_t buf_swap(buf_t b)
{
  unsigned char * lo = (unsigned char *)b.data;
  unsigned char * hi = (unsigned char *)b.data + b.size - 1;
  unsigned char swap;

  while (lo < hi) {
    swap = *lo;
    *lo++ = *hi;
    *hi-- = swap;
  }

  return b;
}

buf_t buf_add_ui32(ui32_t v)
{
  return buf_add((ui8_t *)&v, 4);
}

buf_t buf_add_ui64(ui64_t v)
{
  return buf_add((ui8_t *)&v, 8);
}

ui32_t buf_get_ui32(buf_t b)
{
  return *(ui32_t *)b.data;
}

ui64_t buf_get_ui64(buf_t b)
{
  return *(ui64_t *)b.data;
}

buf_t buf_rand(ui32_t s)
{
  buf_t b = {};
	buf_init(&b);

  srand((unsigned int)time(NULL));

  for (ui32_t i = 0; i < s; i++) {
    b.data[i] = rand() % 256;
  }

  b.size = s;

  return b;
}

buf_t buf_xor(buf_t a, buf_t b)
{
  if (a.size != b.size) {
    api.log.error("Error: buf_cmp: different sizes");
  }

  buf_t r;
	buf_init(&r);

  for (ui32_t i = 0; i < a.size; ++i) {
    r.data[i] = a.data[i] ^ b.data[i];
  }

  r.size = a.size;
  
  return r;
}

buf_t buf_cat_ui32(buf_t dest, ui32_t i)
{
	buf_t src = buf_add_ui32(i);
	buf_t buf = buf_cat(dest, src);
	free(src.data);
	return buf; 
}

buf_t buf_cat_ui64(buf_t dest, ui64_t i){
	buf_t src = buf_add_ui64(i);
	buf_t buf = buf_cat(dest, src);
	free(src.data);
	return buf; 
}

buf_t buf_cat_data(buf_t dest, ui8_t *data, ui32_t len){
	buf_t src = buf_add(data, len);
	buf_t buf = buf_cat(dest, src);
	free(src.data);
	return buf; 
}
