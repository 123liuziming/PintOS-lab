#ifndef VM_PAGE_H
#define VM_PAGE_H

enum SOURCE {
    FROM_SWAP,
    ON_FRAME,
    FROM_FILESYS
}

struct vm_sup_page_table_entry {
    void* u_addr;
    
}

#endif