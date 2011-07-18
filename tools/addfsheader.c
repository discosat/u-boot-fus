#include <stdio.h>

struct FSHeader
{
    unsigned short magic;
    unsigned short os;
    unsigned int size;
    unsigned int reserved1;
    unsigned int reserved2;
};

int main(int argc, char *argv[])
{
    unsigned short checkSum = 0;
    int t;
    unsigned int size = 0;
    FILE *in, *out;
    struct FSHeader head;

    if (argc != 3)
    {
        printf("usage: %s <filename-in> <filename-out>\n", argv[0]);
        return 1;
    }

    in = fopen(argv[1], "rb");
    if (in == NULL)
    {
        printf("can't open in-file '%s'\n", argv[1]);
        return 2;
    }

    out = fopen(argv[2], "wb");
    if (out == NULL)
    {
        printf("can't open out-file '%s'\n", argv[2]);
        return 3;
    }

    head.magic = 0x5346; /* "FS" in little endian */
    head.os = 0x584C;    /* "LX" in little endian */
    head.size = size;
    head.reserved1 = 0;
    head.reserved2 = 0;

    fwrite(&head, sizeof(head), 1, out);

    while (1)
    {
        t = fgetc(in);
        if (feof(in))
            break;
        checkSum += t;
        fputc(t, out);
        size++;
    }

    /* Rewrite header, now with correct size information */
    head.size = size;
    rewind(out);
    fwrite(&head, sizeof(head), 1, out);
    fclose(out);
    fclose(in);

    printf("Size: %d bytes, Checksum: 0x%04x\n", size, checkSum);

    return 0;
}
