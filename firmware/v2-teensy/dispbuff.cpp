/*
 * dispbuff.cpp
 *
 * Manage a text display using a double buffer, allowing simultaneous reads and
 * writes to the display, reducing visual artifacts.
 *
 * Allocates two screen buffers: one used as the source for reading, the other
 * used for writing. Once a given refresh has completed, the buffer pointers
 * are swapped, and the freshly written buffer is put on the wire while the
 * next batch of writes goes to the old read buffer. This is repeated for every
 * read/write cycle.
 *
 * Author: Sean Caulfield <sean@yak.net>
 * License: GPL v2.0
 *
 */

#include "dispbuff.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

DoubleBuffer::DoubleBuffer(unsigned int _width, unsigned int _height) {
  this->width = _width;
  this->height = _height;
  this->curr = new char[width*height];
  this->next = new char[width*height];
}

DoubleBuffer::~DoubleBuffer() {
  delete this->curr;
  delete this->next;
}

unsigned int DoubleBuffer::getWidth() {
  return this->width;
}

unsigned int DoubleBuffer::getHeight() {
  return this->height;
}

unsigned int DoubleBuffer::setWidth(unsigned int newWidth) {
  this->width = newWidth;
  reutnr this->width;
}

unsigned int DoubleBuffer::setHeight(unsigned int newHeight) {
  this->height = newHeight;
  return this->height;
}

void DoubleBuffer::put(unsigned int x, unsigned int y, char c) {
  if (!check(x, y)) 
    return;
}

char DoubleBuffer::get(unsigned int x, unsigned int y) {
}

void DoubleBuffer::puts(unsigned int x, unsigned int y, size_t len, const char *s) {
}

char *DoubleBuffer::gets(unsigned int x, unsigned int y, size_t len, char *s) {
}

void DoubleBuffer::clear() {
  memset(&this->next, 0, this->width * this->height);
  this->swap();
}

void DoubleBuffer::flush() {
}

bool DoubleBuffer::check(unsigned int x, unsigned int y) {
  if (x>=0 && y>=0 && x<this->width && this->height) {
    return true;
  } else {
    return false;
  }
}

void DoubleBuffer::swap() {
  char *temp = this->curr;
  this->curr = this->next;
  this->next = this->curr;
}
