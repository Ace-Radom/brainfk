#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>

#define DEFAULT_ARRAY_LEN 65536
#define DEFAULT_MAX_LINE_LEN 1024
#define DEFAULT_SRC_SIZE 4096

#define OK 0
#define NOT_OK 1
#define DIRSEP '/'
#define EOL "\n"

#define EXITONERR(call) if (call != OK) return NOT_OK

char *exename = NULL;
char *src_path = NULL;
char *src = NULL;
long src_len = 0;
long src_allocated_size = 0;

unsigned char *array = NULL;     // also array begin
unsigned char *array_end = NULL;
unsigned char *ptr = NULL;
long array_size = DEFAULT_ARRAY_LEN;

int chk_file_exists_empty(char *path){
    struct stat buf;
    if (stat(path, &buf) == 0)
    {
        if (!S_ISREG(buf.st_mode))
        {
            fprintf(stderr, "%s: source '%s' is not a regular file.\n", exename, path);
            return NOT_OK;
        }
        if (buf.st_size == 0)
        {
            fprintf(stderr, "%s: empty source '%s'.\n", exename, path);
            return NOT_OK;
        }
    }
    else
    {
        fprintf(stderr, "%s: source '%s' doesn't exist.\n", exename, path);
        return NOT_OK;
    }
    return OK;
}

int parse_args(int argc, char **argv){
    if (argc != 2)
    {
        fprintf(stderr, "%s: illegal argc: %d.\n", exename, argc);
        return NOT_OK;
    }
    src_path = argv[1];
    return OK;
}

int read_src(char *path){
    FILE *fp = fopen(path, "r");
    if (fp == NULL)
    {
        char *errmsg = strerror(errno);
        fprintf(stderr, "%s: failed to open source '%s': %s.\n", exename, path, errmsg);
        return NOT_OK;
    } // failed to open src

    src = (char*) malloc(DEFAULT_SRC_SIZE);
    if (src == NULL)
    {
        fprintf(stderr, "%s: failed to allocate memory when reading source.\n", exename);
        fclose(fp);
        return NOT_OK;
    } // failed to malloc
    *src = '\0';
    src_len = 0;
    src_allocated_size = DEFAULT_SRC_SIZE;
    // allocate src str

    char line[DEFAULT_MAX_LINE_LEN];
    while (fgets(line, sizeof(line), fp))
    {
        char *cmtptr = strstr(line, "//");
        if (cmtptr != NULL)
        {
            *cmtptr = '\0';
        } // remove comments
        line[strcspn(line, EOL)] = '\0';
        // remove EOL

        char *pchk = line;
        char *pwrite = line;
        while (*pchk != '\0')
        {
            if (strchr("<>+-.,[]", *pchk) != NULL)
            {
                *pwrite = *pchk;
                ++pwrite;
            }
            ++pchk;
        }
        *pwrite = '\0';
        // remove all not-allowed chars

        if (line[0] != '\0')
        {
            long new_len = src_len + strlen(line);
            if (new_len + 1 > src_allocated_size)
            {
                long new_allocated_size = src_allocated_size * 2;
                // it's impossible that the new needed size bigger than `src_allocated_size` * 2,
                // cuz max line length is 1024, but `src` starts with a 4096 bytes size.
                char *ptmp = (char*) realloc(src, new_allocated_size);
                if (ptmp == NULL)
                {
                    fprintf(stderr, "%s: failed to reallocate memory when reading source.\n", exename);
                    free(src);
                    fclose(fp);
                    return NOT_OK;
                } // failed to realloc
                src = ptmp;
                src_allocated_size = new_allocated_size;
            } // oversize, realloc
            strcat(src, line);
            src_len = new_len;
        }
    }

    fclose(fp);
    return OK;
}

int create_array(long size){
    array = (unsigned char*) malloc(size);
    if (array == NULL)
    {
        fprintf(stderr, "%s: failed to allocate memory when creating array.\n", exename);
        return NOT_OK;
    }
    memset(array, 0, size * sizeof(unsigned char));
    array_end = array + array_size - 1;
    ptr = array;
    return OK;
}

char* find_loop_end(char *start, char *end){
    int count = 1;
    while (start <= end)
    {
        if (*start == '[')
        {
            count++;
        }
        else if (*start == ']')
        {
            count--;
        }
        if (count == 0)
        {
            return start;
        }
        ++start;
    }
    return NULL;
}

int run(char *start, char *end){
    char *pc = start;
    // ptr to code executing now
    char *new_start = NULL;
    char *new_end = NULL;
    int ret = OK;

    while (pc != end)
    {
        switch (*pc)
        {
            case '>':
                ptr = (ptr < array_end) ? ptr + 1
                                        : array;
                break;
            case '<':
                ptr = (ptr > array) ? ptr - 1
                                    : array_end;
                break;
            case '+':
                ++*ptr;
                break;
            case '-':
                --*ptr;
                break;
            case '.':
                putchar(*ptr);
                break;
            case ',':
                *ptr = getchar();
                break;
            case '[':
                new_start = pc + 1;
                new_end = find_loop_end(new_start, end);
                if (new_end == NULL)
                {
                    fprintf(stderr, "%s: loop doesn't close.\n", exename);
                    return NOT_OK;
                }
                while (*ptr != 0)
                {
                    ret = run(new_start, new_end);
                    if (ret != OK)
                    {
                        return ret;
                    } // error occured in loop
                } // loop
                pc = new_end; // jmp
                break;
            case ']':
            default:
                printf("%s\n", pc);
                fprintf(stderr, "%s: illegal expression.\n", exename);
                return NOT_OK;
                break;
        }
        ++pc;
    }

    return OK;
}

int main(int argc, char **argv){
    if ((exename = strrchr(argv[0], DIRSEP)) != NULL)
    {
        ++exename;
    }
    else
    {
        exename = argv[0];
    }

    EXITONERR(parse_args(argc, argv));
    EXITONERR(chk_file_exists_empty(src_path));
    EXITONERR(read_src(src_path));

    if (src_len == 0)
    {
        fprintf(stderr, "%s: Source '%s' contains no valid brainfuck code. You fucked the interpreter's brain.\n", exename, src_path);
        free(src);
        return NOT_OK;
    }

    EXITONERR(create_array(array_size));

    int ret = run(src, src + src_len);

    free(src);
    free(array);

    return ret;
}
