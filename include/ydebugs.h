#ifndef DEBUGS_H
#define DEBUGS_H

#include <stdio.h>
#include <string.h>
#include <errno.h>

#define Jperror(lbl, format) { fprintf(stdout, format " [%s:%d] : %s\n", __FILE__, __LINE__, strerror(errno)) ; goto lbl; }
#define Jperrori(lbl, format, ...) { fprintf(stdout, format " [%s:%d] : %s\n", ##__VA_ARGS__, __FILE__, __LINE__, strerror(errno)) ; goto lbl; }
#if DEBUG
#define Dprintf(...) fprintf(stderr, __VA_ARGS__)
#define Dperror(format) fprintf(stdout, format " [%s:%d] : %s\n", __FILE__, __LINE__, strerror(errno))
#define Dperrori(format, ...) fprintf(stdout, format " [%s:%d] : %s\n", ##__VA_ARGS__, __FILE__, __LINE__, strerror(errno))
#define DJperror(lbl, format) { fprintf(stdout, format " [%s:%d] : %s\n", __FILE__, __LINE__, strerror(errno)) ; goto lbl; }
#define DJperrori(lbl, format, ...) { fprintf(stdout, format " [%s:%d] : %s\n", ##__VA_ARGS__, __FILE__, __LINE__, strerror(errno)) ; goto lbl; }
#else
#define Dprintf(...) (void) 0
#define Dperror(...) (void) 0
#define Dperrori(...) (void) 0
#define DJperror(lbl, ...) goto lbl
#define DJperrori(lbl, ...) goto lbl
#endif /* DEBUG */

#endif /* DEBUGS_H */
