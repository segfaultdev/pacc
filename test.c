#include <sys/time.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <pacc.h>

static uint64_t time_us(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  
  return (tv.tv_sec * 1000000) + tv.tv_usec;
}

int main(int argc, const char **argv) {
  pacc_init();
  
  uint8_t *input = NULL;
  size_t input_size = 0;
  
  uint8_t *output = NULL;
  size_t output_size = 0;
  
  while (!feof(stdin)) {
    uint8_t byte;
    if (!fread(&byte, 1, 1, stdin)) break;
    
    input = realloc(input, input_size + 1);
    input[input_size++] = byte;
  }
  
  uint64_t start_time = time_us();
  
  if (argv[1][0] == 'd') {
    output_size = pacc_deflate(&output, (char *)(input), input_size);
  } else if (argv[1][0] == 'i') {
    output_size = pacc_inflate((char **)(&output), input, input_size);
  }
  
  uint64_t end_time = time_us();
  
  fwrite(output, 1, output_size, stdout);
  fflush(stdout);
  
  fprintf(stderr, "Input: %llu bytes\n", input_size);
  fprintf(stderr, "Output: %llu bytes\n", output_size);
  fprintf(stderr, "Ratio: %.3f\n", (float)(input_size) / output_size);
  fprintf(stderr, "Time: %.3f seconds\n", (float)(end_time - start_time) / 1000000.0f);
  fprintf(stderr, "Speed: %.3f bytes/second\n", (float)(input_size * 1000000.0f) / (float)(end_time - start_time));
  
  free(input);
  free(output);
  
  return 0;
}
