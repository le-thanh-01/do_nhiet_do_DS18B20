#include "../doc_nhiet_do_DS18B20.h"
void setup(){
  doc_nhiet_do.init();
}
void loop(){
  doc_nhiet_do.plot();
}