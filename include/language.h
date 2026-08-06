#ifndef BEEROS_LANGUAGE_H
#define BEEROS_LANGUAGE_H

enum system_language {
    SYSTEM_LANGUAGE_ENGLISH,
    SYSTEM_LANGUAGE_TURKISH
};

void language_set(enum system_language language);
enum system_language language_get(void);

#endif
