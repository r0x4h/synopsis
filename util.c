#include <ctype.h>

char* strstr_case_insensitive (const char* haystack, const char* needle) {
  do {
    const char* h = haystack;
    const char* n = needle;

    while (tolower ((unsigned char) *h) == tolower ((unsigned char ) *n) && *n) {
      h++;
      n++;
    }

    if (*n == 0) {
      return (char *) haystack;
    }
  } while (*haystack++);

  return 0;
}
