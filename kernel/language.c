#include "language.h"

static enum system_language active_language = SYSTEM_LANGUAGE_ENGLISH;

void language_set(enum system_language language)
{
    active_language = language;
}

enum system_language language_get(void)
{
    return active_language;
}
