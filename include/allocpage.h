#ifndef ALLOCPAGE_H
#define ALLOCPAGE_H

void pageallocator_init(void);

void *alloc_pages(int order);

#define alloc_page()  alloc_pages(0)

void free_pages(void *pages, int order);

#define free_page(p)  free_pages(p, 0)

#endif
