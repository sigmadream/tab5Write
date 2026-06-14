#pragma once

#include <stddef.h>

struct Cursor {
  size_t position = 0; // UTF-8 codepoint offset in the document
  size_t line = 0;
  size_t column = 0;
};
