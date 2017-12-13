#ifndef UTENSOR_VM
#define UTENSOR_VM
#include "tensor.hpp"
#include "uTensor_util.hpp"
#include <vector>
#include <iostream>
#include "mbed.h"

class vm {
  public:
    
    template<typename T>
    void load_data(string& filename, uint8_t unit_size, uint32_t cachesize, uint32_t arrsize, long int offset, T* data);

    template<typename T>
    void flush_data(string& filename, uint8_t unit_size, uint32_t cachesize, uint32_t arrsize, long int offset, T* data);

    FILE* createFile(string& filename);

  private:
    template<typename U>
    void load_impl(U* dst, uint8_t unit_size, uint32_t offset, uint32_t arrsize);
   
    template<typename U>
    void flush_impl(U* dst, uint8_t uint_size, uint32_t arrsize);

    FILE* buffer;
};


template <typename U>
void vm::load_impl(U* dst, uint8_t unit_size, uint32_t offset, uint32_t arrsize) {
  U* val = (U*)malloc(unit_size);
  for (uint32_t i = 0; i < arrsize; i++) {
    fread(val, unit_size, 1, buffer);
     dst[i] = *val;
  }
  free(val);
}
template <typename U>
void vm::flush_impl(U* dst, uint8_t unit_size, uint32_t arrsize) {
  fwrite(dst, unit_size, arrsize, buffer);
  fflush(buffer);
}

FILE* vm::createFile(std::string& filename) {
  buffer = fopen(filename.c_str(), "w");
  return buffer;
}

template<typename T>
void vm::load_data(string& filename, uint8_t unit_size, uint32_t cachesize, uint32_t arrsize, long int offset, T* data) {
  buffer = fopen(filename.c_str(), "r");
  int size = std::min(cachesize, arrsize);
  uint32_t offset_t = (uint32_t)offset;
  
  fseek(buffer, offset * unit_size, SEEK_SET);  // need error  handling
  load_impl(data, unit_size, offset_t, size);
  fclose(buffer);
  return;
}

template<typename T>
void vm::flush_data(string& filename, uint8_t unit_size, uint32_t cachesize, uint32_t arrsize, long int offset, T* data) {
  buffer = fopen(filename.c_str(), "w");
  int size = std::min(cachesize, arrsize);
  fseek(buffer, offset * unit_size, SEEK_SET);
  flush_impl(data, unit_size, size);
  fclose(buffer);
  return;
}
#endif
