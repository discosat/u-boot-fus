/* Include the correct S3C64xx register file */
#if defined(CONFIG_S3C6400)
#include <asm/arch/s3c6400.h>
#elif defined(CONFIG_S3C6410)
#include <asm/arch/s3c6410.h>
#elif defined(CONFIG_S3C6430)
#include <asm/arch/s3c6430.h>
#endif
