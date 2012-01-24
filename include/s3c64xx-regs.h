/* Include the correct S3C64xx register file */
#if defined(CONFIG_S3C6400)
#include <s3c6400.h>
#elif defined(CONFIG_S3C6410)
#include <s3c6410.h>
#elif defined(CONFIG_S3C6430)
#include <s3c6430.h>
#endif
