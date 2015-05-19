#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if ((argc <= 1) || !argv || !*argv) {
        printf("Usage: ./test xxx.svg\n");
        exit(-1);
    }
    //const char *name = "weather_icons/SVG/1.svg";
    const char *name = argv[1];

    FILE *fp = fopen(name, "r");
    if (!fp) {
        printf("fp is NULL\n");
        exit(-1);
    }

    char buf[4096];
    size_t size = 0;
    char *str = NULL;
    while (fgets(buf, 4095, fp)) {
        size += strlen(buf);
        str = realloc(str, size + 1);
        strncat(str, buf, strlen(buf));
    }

    char *start = strstr(str, " d=\"");
    start+=3;
    //printf("%s", start);
    char *end = strstr(start + 1, "\"");
    //printf("**********************************\n");
    //printf("%s", end);
    *(end + 1)='\0';
    printf("%s\n", argv[1]);
    printf("%s\n", start);

    free(str);

    fclose(fp);
    return 0;
}
