#ifndef __PACC_H__
#define __PACC_H__

#include <stdint.h>
#include <stdlib.h>

typedef struct pacc_t pacc_t;

struct pacc_t {
  union {
    const uint8_t *const_data;
    uint8_t *data;
  };
  
  size_t size;
  
  size_t bit_offset;
  uint8_t can_predict; // 0=unknown, 1=predict
};

extern const uint32_t c_huffman_1[16], c_huffman_2[1536];
extern uint32_t huffman_1[96], huffman_2[9216];

/*
uint8_t pacc_to_ascii(uint8_t pacc);
uint8_t ascii_to_pacc(uint8_t ascii);

uint8_t huffman_length(uint8_t value, const uint16_t *huffman);

void pacc_write_bits(pacc_t *output, uint32_t value, uint8_t count);
void pacc_write_varint_15(pacc_t *output, uint16_t value);
void pacc_write_varint_7(pacc_t *output, uint8_t value);
void pacc_write_huffman(pacc_t *output, uint8_t value, const uint16_t *huffman);

uint32_t pacc_read_bits(pacc_t *input, uint8_t count);
uint16_t pacc_read_varint_15(pacc_t *input);
uint8_t  pacc_read_varint_7(pacc_t *input);
uint8_t  pacc_read_huffman(pacc_t *input, const uint16_t *huffman);
*/

void pacc_init(void);

size_t pacc_inflate(char **output_ptr, const uint8_t *input, size_t input_size);
size_t pacc_deflate(uint8_t **output_ptr, const char *input, size_t input_size);

#endif
