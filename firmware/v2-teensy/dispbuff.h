/*
 * dispbuff.h
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

class DoubleBuffer {

  public:

    DoubleBuffer(unsigned int width, unsigned int height);
    ~DoubleBuffer();

    unsigned int getWidth();
    unsigned int getHeight();
    unsigned int setWidth(unsigned int newWidth);
    unsigned int setHeight(unsigned int newHeight);

    void put(unsigned int x, unsigned int y, char c);
    char get(unsigned int x, unsigned int y);
    void puts(unsigned int x, unsigned int y, size_t len, const char *s);
    char *gets(unsigned int x, unsigned int y, size_t len, char *s);
    void clear();
    void flush();

  protected:

    unsigned int width;
    unsigned int height;
    char *curr; // Pointer to current read / next write buffer
    char *next; // Pointer to next read / current write buffer

    bool check(unsigned int x, unsigned int y); // Check bounds
    void swap(); // Swap buffers

};
