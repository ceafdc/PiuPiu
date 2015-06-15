#include <string.h>
#include <stdlib.h>

#include <stdio.h>

#include "str.h"
#include "refcnt.h"
#include "structures.h"


#define BUFFER_SIZE (1 << 8)


void str_free(void *obj);

String *
str_init()
{
    String *str = alloc(sizeof(String), str_free);
    str->string = malloc(sizeof(char) * BUFFER_SIZE);
    str->string[0] = 0;
    str->bufferSize = BUFFER_SIZE;
    str->len = 0;
    return str;
}


String *
str_create(const char * original)
{
    String *str = alloc(sizeof(String), str_free);
    str->len = strlen(original);
    str->bufferSize = ((str->len + 1)/BUFFER_SIZE + 1) * BUFFER_SIZE;
    str->string = malloc(sizeof(char) * str->bufferSize);
    memcpy(str->string, original, sizeof(char) * (str->len + 1));
    return str;
}

String *
str_from_file(FILE *fp, const char *stopchars)
{
    String *str = str_init();
    do{
        int c = getc(fp);
        if (!c || c == EOF || strchr(stopchars, c))
            break;
        str_append_char(str, c);
    }while(1);
    return str;
}

String *
str_from_stdin()
{
    return str_from_file(stdin, "\n\r");
}

void
str_center(String *str, int size) //   " asd  "
{
    int unicodeLen = str_unicode_len(str);
    int space_amnt = size - unicodeLen;
    if (space_amnt <= 0) return;

    int newSize = space_amnt + str->len;
    char *newString = malloc(sizeof(char) * (newSize + 1));
    int firstGap = space_amnt/2;


    for (int i = 0; i < space_amnt + str->len; i++)
    {
        if (i < firstGap)
        {
            newString[i] = ' ';
            continue;
        }
        int originalPos = i - firstGap;
        if (originalPos < str->len)
            newString[i] = str->string[originalPos];
        else
            newString[i] = ' ';
    }
    free(str->string);
    str->string = newString;
    str->len = newSize;
    str->bufferSize = newSize + 1;

    newString[newSize] = 0;
}

Vector *
str_wrap(String *str, int width)
{
    Vector *lines = vector_init();
    for (int i = 0; i < str->len; i++)
    {
        int line_number = i/width;

        if (line_number == lines->count)
        {
            String *s = str_init();
            vector_append(lines, s);
            release(s);
        }
        String *s = lines->objs[line_number];
        str_append_char(s, str->string[i]);
    }
    String *last_line = lines->objs[lines->count - 1];
    for (int j = last_line->len; j < width; j++)
    {
        str_append_char(last_line, ' ');
    }
    return lines;
}

String *
str_from_int(int i)
{
    char buffer[100];
    sprintf(buffer, "%d", i);
    String *s = str_create(buffer);
    return s;
}

String *
str_from_long(long int l)
{
    char buffer[100];
    sprintf(buffer, "%ld", l);
    String *s = str_create(buffer);
    return s;
}

int
str_unicode_len(String *str)
{
    int unicodeLen = 0;
    int toFinishSequence = 0;
    for (int i = 0; i < str->len; i++)
    {
        unsigned char c = str->ustring[i];
        if (toFinishSequence)
        {
            toFinishSequence--;
            continue;
        }
        unicodeLen++;
        if (c > 128)
        {
            bin8 b;
            b.uc = c;
            toFinishSequence = (b.b7?1:0) + (b.b6?1:0) + (b.b5?1:0);
            toFinishSequence -= 1;
        }
    }
    return unicodeLen;
}

String *
str_escape_cstring(char * string)
{
    String *esc = str_init();
    unsigned char utfSeq[4];
    utfSeq[3] = 0;
    int isBuildingSeq = 0;
    int seqSize = 0;
    for (int i = 0; string[i]; i++)
    {
        unsigned char c = string[i];
        if (c <= '~')
        {
            switch (c)
            {
                case '\"': str_append(esc, "\\\""); break;
                case '\n': str_append(esc, "\\n");  break;
                case '\r': str_append(esc, "\\r");  break;
                case '\t': str_append(esc, "\\t");  break;
                case '\b': str_append(esc, "\\b");  break;
                case '\f': str_append(esc, "\\f");  break;
                case '\\': str_append(esc, "\\\\"); break;
                case '/':  str_append(esc, "\\/");  break;
                default:   str_append_char(esc, c); break;
            }
        }
        else
        {
            if (!isBuildingSeq)
            {
                bin8 b;
                b.c = c;
                seqSize = (b.b7?1:0) + (b.b6?1:0) + (b.b5?1:0);
                utfSeq[0] =
                utfSeq[1] =
                utfSeq[2] =
                utfSeq[3] = 0;
            }
            utfSeq[isBuildingSeq] = c;
            isBuildingSeq += 1;
            if (isBuildingSeq >= seqSize)
            {
                bin32 b;
                b.u = 0;
                if (seqSize == 1)
                {
                    bin8 c1;
                    c1.uc = utfSeq[0];
                    b.b0 = c1.b0;
                    b.b1 = c1.b1;
                    b.b2 = c1.b2;
                    b.b3 = c1.b3;
                    b.b4 = c1.b4;
                    b.b5 = c1.b5;
                    b.b6 = c1.b6;
                }
                else
                if(seqSize == 2)
                {
                    bin8 c1, c2;
                    c1.uc = utfSeq[0];
                    c2.uc = utfSeq[1];

                    b.b0 = c2.b0;
                    b.b1 = c2.b1;
                    b.b2 = c2.b2;
                    b.b3 = c2.b3;
                    b.b4 = c2.b4;
                    b.b5 = c2.b5;
                    b.b6 = c1.b0;
                    b.b7 = c1.b1;

                    b.b8 = c1.b2;
                    b.b9 = c1.b3;
                    b.b10 = c1.b4;
                }
                else if(seqSize == 3)
                {
                    bin8 c0, c1, c2;
                    c2.uc = utfSeq[0];
                    c1.uc = utfSeq[1];
                    c0.uc = utfSeq[2];

                    b.b0 = c0.b0;
                    b.b1 = c0.b1;
                    b.b2 = c0.b2;
                    b.b3 = c0.b3;
                    b.b4 = c0.b4;
                    b.b5 = c0.b5;
                    b.b6 = c1.b0;
                    b.b7 = c1.b1;

                    b.b8 = c1.b2;
                    b.b9 = c1.b3;
                    b.b10 = c1.b4;
                    b.b11 = c1.b5;
                    b.b12 = c2.b0;
                    b.b13 = c2.b1;
                    b.b14 = c2.b2;
                    b.b15 = c2.b3;
                }
                char unicode[20];
                sprintf(unicode, "\\u%04x", b.u);
                str_append(esc, unicode);
                isBuildingSeq = 0;
            }
        }
    }
    return esc;
}

String *
str_escape(String *str)
{
    return str_escape_cstring(str->string);
}


void
str_append(String *str, const char * toAppend)
{
    size_t appLen = strlen(toAppend);
    if (str->len + appLen + 1 > str->bufferSize)
    {
        str->bufferSize = ((str->len + appLen + 1)/ BUFFER_SIZE + 1) * BUFFER_SIZE;
        str->string = realloc(str->string, sizeof(char) * str->bufferSize);
    }
//    strcat(str->string, toAppend); // menos eficiente
    memcpy(str->string+str->len, toAppend, (appLen + 1) * sizeof(char));
    str->len += appLen;
}

void
str_append_char(String *str, const unsigned char c)
{
    size_t appLen = 1;
    if (str->len + appLen + 1 > str->bufferSize)
    {
        str->bufferSize = ((str->len + appLen + 1)/ BUFFER_SIZE + 1) * BUFFER_SIZE;
        str->string = realloc(str->string, sizeof(char) * str->bufferSize);
    }
    *(str->string+str->len) = c;
    *(str->string+str->len+1) = 0;
    str->len += appLen;
}

void
str_free(void *obj)
{
    String *str = (String *) obj;
    free(str->string);
}
