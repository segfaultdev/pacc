#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pacc.h>

uint32_t huffman_1[96], huffman_2[9216];

static inline void pacc_fill(uint32_t *huffman, const uint32_t *c_huffman) {
  uint8_t lengths[96], indexes[96];
  
  for (uint8_t i = 0; i < 16; i++) {
    uint32_t value = c_huffman[i];
    
    for (uint8_t j = 0; j < 6; j++) {
      lengths[i * 6 + j] = (value % 20) + 1;
      indexes[i * 6 + j] = i * 6 + j;
      
      value /= 20;
    }
  }
  
  for (uint8_t i = 0; i < 96; i++) {
    for (uint8_t j = i + 1; j < 96; j++) {
      if (lengths[i] > lengths[j] || (lengths[i] == lengths[j] && indexes[i] > indexes[j])) {
        uint8_t length = lengths[i];
        uint8_t index = indexes[i];
        
        lengths[i] = lengths[j];
        indexes[i] = indexes[j];
        
        lengths[j] = length;
        indexes[j] = index;
      }
    }
  }
  
  uint32_t value = (1 << lengths[0]);
  huffman[indexes[0]] = value;
  
  for (uint8_t i = 1; i < 96; i++) {
    value++;
    
    while (lengths[i] > lengths[i - 1]) {
      lengths[i - 1]++;
      value <<= 1;
    }
    
    huffman[indexes[i]] = (1 << lengths[i]);
    
    for (uint8_t j = 0; j < lengths[i]; j++) {
      if (!(value & (1 << j))) continue;
      huffman[indexes[i]] |= (1 << ((lengths[i] - 1) - j));
    }
  }
}

void pacc_init(void) {
  pacc_fill(huffman_1, c_huffman_1);
  
  for (uint8_t i = 0; i < 96; i++) {
    pacc_fill(huffman_2 + i * 96, c_huffman_2 + i * 16);
  }
}

static inline uint8_t pacc_to_ascii(uint8_t pacc) {
  if (pacc == 95) return '\n';
  else return pacc + 32;
}

static inline uint8_t ascii_to_pacc(uint8_t ascii) {
  if (ascii == '\n') return 95;
  else if (ascii < 32 || ascii >= 127) return 0;
  else return ascii - 32;
}

static inline uint8_t pacc_huffman_length(uint8_t value, const uint32_t *huffman) {
  uint32_t length = 31;
  
  while (length && !(huffman[value] & (1u << length))) {
    length--;
  }
  
  return length;
}

static inline void pacc_write_bits(pacc_t *output, uint32_t value, uint8_t count) {
  while (count) {
    uint8_t left = 8 - (output->bit_offset & 7);
    if (left > count) left = count;
    
    output->data[output->bit_offset >> 3] |= ((value & ((1 << left) - 1)) << (output->bit_offset & 7));
    value >>= left;
    
    output->bit_offset += left;
    count -= left;
  }
}

static inline void pacc_write_varint_14(pacc_t *output, uint16_t value) {
  if (value < 4096) {
    pacc_write_bits(output, 0, 1);
    pacc_write_bits(output, value, 12);
  } else {
    pacc_write_bits(output, 1, 1);
    pacc_write_bits(output, value - 4096, 13);
  }
}

static inline void pacc_write_varint_8(pacc_t *output, uint16_t value) {
  if (value < 8) {
    pacc_write_bits(output, 0, 1);
    pacc_write_bits(output, value, 3);
  } else {
    pacc_write_bits(output, 1, 1);
    pacc_write_bits(output, value - 8, 7);
  }
}

static inline void pacc_write_varint_7(pacc_t *output, uint16_t value) {
  if (value < 8) {
    pacc_write_bits(output, 0, 1);
    pacc_write_bits(output, value, 3);
  } else {
    pacc_write_bits(output, 1, 1);
    pacc_write_bits(output, value - 8, 6);
  }
}

static inline void pacc_write_huffman(pacc_t *output, uint8_t value, const uint32_t *huffman) {
  pacc_write_bits(output, huffman[value], pacc_huffman_length(value, huffman));
}

static inline uint32_t pacc_read_bits(pacc_t *input, uint8_t count) {
  uint32_t value = 0;
  uint8_t shift = 0;
  
  while (count) {
    uint8_t left = 8 - (input->bit_offset & 7);
    if (left > count) left = count;
    
    uint8_t bits_left = (input->data[input->bit_offset >> 3] >> (input->bit_offset & 7));
    value |= ((uint32_t)(bits_left & ((1 << left) - 1)) << shift);
    
    input->bit_offset += left;
    shift += left, count -= left;
  }
  
  return value;
}

static inline uint16_t pacc_read_varint_14(pacc_t *input) {
  if (pacc_read_bits(input, 1)) {
    return pacc_read_bits(input, 13) + 4096;
  } else {
    return pacc_read_bits(input, 12);
  }
}

static inline uint8_t pacc_read_varint_8(pacc_t *input) {
  if (pacc_read_bits(input, 1)) {
    return pacc_read_bits(input, 7) + 8;
  } else {
    return pacc_read_bits(input, 3);
  }
}

static inline uint8_t pacc_read_varint_7(pacc_t *input) {
  if (pacc_read_bits(input, 1)) {
    return pacc_read_bits(input, 6) + 8;
  } else {
    return pacc_read_bits(input, 3);
  }
}

static inline uint8_t pacc_read_huffman(pacc_t *input, const uint32_t *huffman) {
  uint32_t value = 0;
  uint8_t length = 0;
  
  for (;;) {
    value |= (pacc_read_bits(input, 1) << (length++));
    
    for (uint8_t i = 0; i < 96; i++) {
      if (huffman[i] == (value | (1u << length))) {
        return i;
      }
    }
  }
}

size_t pacc_inflate(char **output_ptr, const uint8_t *input_data, size_t input_size) {
  pacc_t input = (pacc_t){input_data, input_size, 0, 1};
  
  char *output = NULL;
  size_t output_size = 0, output_offset = 0;
  
  while ((input.bit_offset >> 3) < input.size) {
    uint8_t block_type;
    
    if (input.can_predict) {
      block_type = input.can_predict - 1;
    } else {
      block_type = pacc_read_bits(&input, 1);
    }
    
    if (block_type) {
      uint16_t offset = pacc_read_varint_14(&input) + 1;
      uint8_t count = pacc_read_varint_7(&input) + 1;
      
      if (offset == 1 && count == 1) {
        // This is always suboptimal, thus we consider it the exit case
        break;
      }
      
      if (offset > output_offset) {
        // I do not intend to segfault the heck out of my linux machine
        break;
      }
      
      if (output_offset + count > output_size) {
        output_size += count + 4096;
        output = realloc(output, output_size);
      }
      
      char *repeat_ptr = (output + output_offset) - offset;
      input.can_predict = 0;
      
      while (count--) {
        output[output_offset++] = *(repeat_ptr++);
      }
    } else {
      uint8_t count = pacc_read_varint_8(&input);
      uint8_t prev = pacc_read_huffman(&input, huffman_1);
      
      if (output_offset + count > output_size) {
        output_size += count + 4096;
        output = realloc(output, output_size);
      }
      
      output[output_offset++] = pacc_to_ascii(prev);
      input.can_predict = (count < 135) * 2;
      
      while (count--) {
        uint8_t curr = pacc_read_huffman(&input, huffman_2 + prev * 96);
        
        output[output_offset++] = pacc_to_ascii(curr);
        prev = curr;
      }
    }
  }
  
  if (output_size > output_offset) {
    output_size = output_offset;
    output = realloc(output, output_size);
  }
  
  *output_ptr = output;
  return output_size;
}

static inline const size_t find_diff(const uint8_t *a, const uint8_t *b, size_t size) {
  size_t count = 0;
  
  while (size-- && a[count] == b[count]) {
    count++;
  }
  
  return count;
}

static inline size_t min(size_t a, size_t b) {
  return a < b ? a : b;
}

static inline size_t max(size_t a, size_t b) {
  return a > b ? a : b;
}

static inline void pacc_deflate_literal(pacc_t *output, const char *input, size_t input_size) {
  if (!input_size) return;
  
  if (!output->can_predict) {
    pacc_write_bits(output, 0, 1);
  }
  
  output->can_predict = (input_size < 136);
  input_size--;
  
  pacc_write_varint_8(output, input_size);
  
  uint8_t prev = ascii_to_pacc(*(input++));
  pacc_write_huffman(output, prev, huffman_1);
  
  while (input_size--) {
    uint8_t curr = ascii_to_pacc(*(input++));
    pacc_write_huffman(output, curr, huffman_2 + prev * 96);
    
    prev = curr;
  }
}

static inline size_t pacc_literal_length(const char *input, size_t input_size) {
  if (!input_size) return 0;
  size_t length = 1;
  
  while (input_size > 136) {
    length += pacc_literal_length(input, 136);
    
    input += 136;
    input_size -= 136;
  }
  
  length += (input_size <= 8 ? 4 : 8);
  input_size--;
  
  uint8_t prev = ascii_to_pacc(*(input++));
  length += pacc_huffman_length(prev, huffman_1);
  
  while (input_size--) {
    uint8_t curr = ascii_to_pacc(*(input++));
    length += pacc_huffman_length(curr, huffman_2 + prev * 96);
    
    prev = curr;
  }
  
  // length += 4 * input_size;
  
  /*
  while (input_size--) {
    length += pacc_huffman_length(ascii_to_pacc(*(input++)), huffman_1);
  }
  */
  
  return length;
}

size_t pacc_deflate(uint8_t **output_ptr, const char *input, size_t input_size) {
  pacc_t output = (pacc_t){malloc(input_size * 6), input_size * 6, 0, 1};
  size_t literal_offset = 0, input_offset = 0;
  
  while (input_offset < input_size) {
    size_t best_match_offset = 0, best_match_size = 0;
    
    for (size_t match_offset = 1; match_offset <= min(12288, input_offset); match_offset++) {
      size_t match_size = find_diff(input + input_offset, (input + input_offset) - match_offset, min(input_size - input_offset, 72));
      
      if (match_size > best_match_size) {
        best_match_offset = match_offset;
        best_match_size = match_size;
      }
    }
    
    while (input_offset - literal_offset >= 136) {
      pacc_deflate_literal(&output, input + literal_offset, 136);
      literal_offset += 136;
    }
    
    // WTF-worthy note: These two commented out lines *should* make the compression ratio slightly better, but apparently
    // they don't (making it even dip below the 4.xxx's). Additionally, the line consisting of "match_length += 6 ..."
    // has a random 6 in there, which was previously a 1 (which made sense looking at the algorithm), but changing it to
    // a 6 makes the compression ratio peak at 4.617 compared to the 4.555 with a 1 or the 4.541 with the commented out
    // line.
    
    size_t next_packet_size = (input_offset + best_match_size) - literal_offset;
    // if (next_packet_size % 136) next_packet_size += 136 - (next_packet_size % 136);
    
    size_t literal_length = pacc_literal_length(input + literal_offset, next_packet_size);
    size_t match_length = pacc_literal_length(input + literal_offset, input_offset - literal_offset);
    
    // match_length += !(input_offset - literal_offset) + (best_match_offset <= 4096 ? 13 : 14) + (best_match_size <= 8 ? 4 : 7);
    match_length += 6 + (best_match_offset <= 4096 ? 13 : 14) + (best_match_size <= 8 ? 4 : 7);
    
    if (best_match_size && match_length < literal_length) {
      if (literal_offset < input_offset) {
        pacc_deflate_literal(&output, input + literal_offset, input_offset - literal_offset);
      }
      
      if (!output.can_predict) {
        pacc_write_bits(&output, 1, 1);
      }
      
      pacc_write_varint_14(&output, best_match_offset - 1);
      pacc_write_varint_7(&output, best_match_size - 1);
      
      input_offset += best_match_size;
      literal_offset = input_offset;
      
      output.can_predict = 0;
    } else {
      input_offset++;
    }
  }
  
  if (literal_offset < input_offset) {
    pacc_deflate_literal(&output, input + literal_offset, input_offset - literal_offset);
  }
  
  if (output.bit_offset & 7) {
    if (!output.can_predict) {
      pacc_write_bits(&output, 1, 1);
    }
    
    pacc_write_varint_14(&output, 0);
    pacc_write_varint_7(&output, 0);
  }
  
  output.size = ((output.bit_offset + 7) >> 3);
  output.data = realloc(output.data, output.size);
  
  *output_ptr = output.data;
  return output.size;
}
