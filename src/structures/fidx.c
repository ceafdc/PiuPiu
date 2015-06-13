#include "structures.h"
#include "fman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fman.h"
#include "vector.h"
#include "fidx.h"
#include "str.h"

void fidx_release(void *o);
void idx_release(void *o);
void fidx_sort(FieldIndex *fidx);
void idx_print(IndexEntry *e);

FieldIndex *
fidx_create(FileManager *fman, FieldType ftype, int field_idx)
{
    FieldIndex *fidx = alloc(sizeof(FieldIndex), fidx_release);
    fidx->fman = fman;
    fidx->ftype = ftype;
    fidx->field_idx = field_idx;
    fidx->index = NULL;

    return fidx;
}

IndexEntry *
idx_create(FieldType ftype, void *data, long int offset)
{
    IndexEntry *idx = alloc(sizeof(IndexEntry), idx_release);
    idx->ftype = ftype;
    idx->offset = offset;
    switch(ftype)
    {
        case str_f:    idx->str = ((String *)data);          break;
        case int_f:    idx->i   = *((int *)data);           break;
        case uint_f:   idx->ui  = *((unsigned int *)data);  break;
        case long_f:   idx->l   = *((long *)data);          break;
        case ulong_f:  idx->ul  = *((unsigned long *)data); break;
        case float_f:  idx->f   = *((float *)data);         break;
        case double_f: idx->lf  = *((double *)data);        break;
        case char_f:   idx->c   = *((char *)data);          break;
        case uchar_f:  idx->uc  = *((unsigned char *)data); break;
    }
    if (ftype == str_f)
    {
        retain(idx->str);
    }
    return idx;
}

void
fidx_create_from_data_file(FieldIndex *fidx)
{
    Vector *offset_vector = fman_list_all(fidx->fman);
    fidx->index = vector_init();
    for (int i = 0; i < offset_vector->count; ++i)
    {
        size_t entry_size = ffields_size(fidx->fman->ff, sizeof(String *));
        void *entry = malloc(entry_size);
        long int offset = *((long int *)offset_vector->objs[i]);
        fman_entry_at_offset(fidx->fman, offset, entry);
        void *field = entry + fidx->fman->ff->offsets[fidx->field_idx];
        if (fidx->ftype == str_f)
        {
            String *str = *(String **)field;
            retain(str);
            IndexEntry *e = idx_create(fidx->ftype, str, offset);
            vector_append(fidx->index, e);
            release(e);
        }
        else
        {
            IndexEntry *e = idx_create(fidx->ftype, field, offset);
            vector_append(fidx->index, e);
            release(e);
        }
        free(entry);
    }
    release(offset_vector);
    fidx_sort(fidx);
}

void
fidx_create_index(FieldIndex *fidx)
{
    if (fidx->index) //index already exists
        return;
    fidx->index = vector_init();
    String *f1_name = str_create(fidx->fman->db_name->string);
    str_append_char(f1_name, '.');
    str_append_char(f1_name, fidx->field_idx+'0');
    str_append(f1_name, ".idx");
    FILE *f1 = fopen(f1_name->string, "rb");

    String *f2_name = str_create(fidx->fman->db_name->string);
    str_append_char(f2_name, '.');
    str_append_char(f2_name, fidx->field_idx+'0');
    str_append(f2_name, ".lst");
    FILE *f2 = fopen(f2_name->string, "rb");

    if (!f1 || !f2)
    {
        if (f1)
        {
            fclose(f1);
            remove(f1_name->string);
        }
        if (f2)
        {
            fclose(f2);
            remove(f2_name->string);
        }
        release(f1_name);
        release(f2_name);
        fidx_create_from_data_file(fidx);
        return;
    }

    Vector *index = fidx->index;
    size_t field_size = ftype_size_of(fidx->ftype);
    while(!feof(f1))
    {
        if (fidx->ftype == str_f)
        {
            String *str = str_from_file(f1, "");
            long int next_entry = -1;
            fread(&next_entry, sizeof(next_entry), 1, f1);
            while (next_entry != -1)
            {
                long int offset = -1;
                fseek(f2, next_entry, SEEK_SET);
                fread(&offset, sizeof(offset), 1, f2);

                IndexEntry *entry = idx_create(fidx->ftype, str, offset);
                vector_append(index, entry);
                release(entry);
                fread(&next_entry, sizeof(next_entry), 1, f2);
            }
            release(str);
        }
        else
        {
            char this_value[30];
            fread(this_value, field_size, 1, f1);
            long int next_entry = -1;
            fread(&next_entry, sizeof(next_entry), 1, f1);
            while (next_entry != -1)
            {
                long int offset = -1;
                fseek(f2, next_entry, SEEK_SET);
                fread(&offset, sizeof(offset), 1, f2);

                IndexEntry *entry = idx_create(fidx->ftype, &this_value, offset);
                vector_append(index, entry);
                release(entry);
                fread(&next_entry, sizeof(next_entry), 1, f2);
            }
        }
    }
    fclose (f1);
    fclose (f2);
    remove(f1_name->string);
    remove(f2_name->string);
    release(f1_name);
    release(f2_name);
    fidx_sort(fidx);
}

int
idx_cmp(IndexEntry* e1, IndexEntry* e2)
{
    if (e1->ftype != e2->ftype)return 0;
    switch(e1->ftype)
    {
        case str_f:
        {
            return str_cmp(e1->str, e2->str);
        }
        break;
        case int_f:
        {
            return e1->i - e2->i;
        }
        break;
        case uint_f:
        {
            return e1->ui - e2->ui;
        }
        break;
        case long_f:
        {
            return e1->l - e2->l;
        }
        break;
        case ulong_f:
        {
            return e1->ul - e2->ul;
        }
        break;
        case float_f:
        {
            if (e1->f < e2->f)
                return -1;
            else if (e1->f > e2->f)
                return 1;
            else
                return 0;
        }
        break;
        case double_f:
        {
            if (e1->lf < e2->lf)
                return -1;
            else if (e1->lf > e2->lf)
                return 1;
            else
                return 0;
        }
        break;
        case char_f:
        {
            return e1->c - e2->c;
        }
        break;
        case uchar_f:
        {
            return e1->uc - e2->uc;
        }
        break;
    }
    return 0;
}

int
_idx_entry_sort(const void *v1, const void *v2)
{
    IndexEntry **p1 = (IndexEntry **)v1;
    IndexEntry **p2 = (IndexEntry **)v2;
    IndexEntry *e1 = *p1;
    IndexEntry *e2 = *p2;

    int result = idx_cmp(e1, e2);
    if (!result)
    {
        if (e1->offset < e2->offset)
            return -1;
        if (e1->offset > e2->offset)
            return 1;
        return 0;
    }
    return result;
}

void
fidx_sort(FieldIndex *fidx)
{
    vector_sort(fidx->index, _idx_entry_sort);
}

Vector *
fidx_search(FieldIndex *fidx, const void *value)
{
    if (!fidx->index)
    {
        return NULL;
    }

    Vector *offset_vector = vector_init();
    for (int i = 0; i < fidx->index->count; i++)
    {
        IndexEntry *e = fidx->index->objs[i];
        if (e->ftype == int_f)

        {
            int i1 = *(int *)value;
            printf("%d == %d ->", i1, e->i);
        }
        if (e->ftype == str_f)
        {
            String *s1 = (String *)value;
            printf("%s == %s ->", s1->string, e->str->string);
        }
        if ((e->ftype == str_f && str_eq((String *)value, e->str)) ||
            (e->ftype != str_f && !memcmp(value, &e->v, ftype_size_of(e->ftype))))
        {
            printf("yes\n");
            long int *entry_offset = alloc(sizeof(long int), NULL);
            *entry_offset = e->offset;
            vector_append(offset_vector, entry_offset);
            release(entry_offset);

        }
        else printf("no\n");

    }
    return offset_vector;
}

void
fidx_write_file(FieldIndex *fidx)
{
    if (!fidx->index)
        return;

    String *f1_name = str_create(fidx->fman->db_name->string);
    str_append_char(f1_name, '.');
    str_append_char(f1_name, fidx->field_idx+'0');
    str_append(f1_name, ".idx");
    FILE *f1 = fopen(f1_name->string, "wb");


    String *f2_name = str_create(fidx->fman->db_name->string);
    str_append_char(f2_name, '.');
    str_append_char(f2_name, fidx->field_idx+'0');
    str_append(f2_name, ".lst");
    FILE *f2 = fopen(f2_name->string, "wb");

    Vector *index = fidx->index;
    for (int i = 0; i < index->count; i++)
    {
        IndexEntry *e = index->objs[i];
        if (e->ftype == str_f)
        {
            fwrite(e->str->string, sizeof(char), e->str->len + 1, f1);
        }
        else
        {
            fwrite(&e->v, ftype_size_of(e->ftype), 1, f1);
        }
        long int last_offset = -1;
        for (; i < index->count; i++)
        {
            IndexEntry *newE = index->objs[i];
            if (idx_cmp(e, newE))
            {
                i--;
                break;
            }
            fwrite(&newE->offset, sizeof(long int), 1, f2);
            fwrite(&last_offset, sizeof(last_offset), 1, f2);
            last_offset = ftell(f2) - sizeof(long int) - sizeof(last_offset);
        }
        fwrite(&last_offset, sizeof(last_offset), 1, f1);
    }
    fclose (f1);
    fclose (f2);
    release(f1_name);
    release(f2_name);
}

void
idx_print(IndexEntry *e)
{
    switch (e->ftype)
    {
        case str_f:printf("%s @ %ld\n", e->str->string, e->offset); break;
        case int_f:printf("%d @ %ld\n", e->i, e->offset); break;
        case uint_f:printf("%u @ %ld\n", e->ui, e->offset); break;
        case long_f:printf("%ld @ %ld\n", e->l, e->offset); break;
        case ulong_f:printf("%lu @ %ld\n", e->ul, e->offset); break;
        case float_f:printf("%f @ %ld\n", e->f, e->offset); break;
        case double_f:printf("%lf @ %ld\n", e->lf, e->offset); break;
        case char_f:printf("%c @ %ld\n", e->c, e->offset); break;
        case uchar_f:printf("%c @ %ld\n", e->uc, e->offset); break;
    }
}

void
fidx_print(FieldIndex *fidx)
{
    if (!fidx->index)return;
    for (int i = 0; i < fidx->index->count; ++i) {
        IndexEntry *e = fidx->index->objs[i];
        idx_print(e);
    }
}

void
idx_release(void *o)
{
    IndexEntry *idx = o;
    if (idx->ftype == str_f)
        release(idx->str);
}

void
fidx_release(void *o)
{
    FieldIndex *fidx = o;

    fidx_write_file(fidx);

    release(fidx->index);
}
