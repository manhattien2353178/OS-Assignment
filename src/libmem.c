/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c 
 */

 #include "string.h"
 #include "mm.h"
 #include "syscall.h"
 #include "libmem.h"
 #include <stdlib.h>
 #include <stdio.h>
 #include <pthread.h>
 
 //static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;
 
 /*enlist_vm_freerg_list - add new rg to freerg_list
  *@mm: memory region
  *@rg_elmt: new region
  *
  */
 int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
 {
   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;
 
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
     return -1;
 
   if (rg_node != NULL)
     rg_elmt->rg_next = rg_node;
 
   /* Enlist the new region */
   mm->mmap->vm_freerg_list = rg_elmt;
 
   return 0;
 }
 
 /*get_symrg_byid - get mem region by region ID
  *@mm: memory region
  *@rgid: region ID act as symbol index of variable
  *
  */
 struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
 {
   if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
     return NULL;
 
   return &mm->symrgtbl[rgid];
 }
 
 /*__alloc - allocate a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *@alloc_addr: address of allocated memory region
  *
  */
 int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
 {
   /* Allocate at the toproof */
struct vm_rg_struct rgnode;

/* Try getting free region from freelist */
if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
{
    caller->mm->symrgtbl[rgid] = rgnode;
    *alloc_addr = rgnode.rg_start;
    return 0;
}

/* Failed to get from freelist -> extend sbrk limit */
struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
if (cur_vma == NULL) return -1;

int old_sbrk = cur_vma->sbrk;

/* Align size to page size for allocation */
int aligned_sz = PAGING_PAGE_ALIGNSZ(size);

/* Increase limit (sbrk) */
if (inc_vma_limit(caller, vmaid, aligned_sz) < 0) return -1;

/* Invoke syscall to actually map new physical memory */
struct sc_regs regs;
regs.a1 = old_sbrk;             // start address
regs.a2 = aligned_sz;           // increase size
regs.a3 = SYSMEM_INC_OP;        // operation type

syscall(caller, 17, &regs);     // syscall 17 = sys_memmap

/* Update symbol table with new region info */
caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
caller->mm->symrgtbl[rgid].rg_end   = old_sbrk + size;

/* Return virtual start address */
*alloc_addr = old_sbrk;
 
   return 0;
 }
 
 /*__free - remove a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __free(struct pcb_t *caller, int vmaid, int rgid)
 {
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ) return -1;
 
   struct vm_rg_struct *freerg = malloc(sizeof(struct vm_rg_struct));
   if (!freerg) return -1;
 
   // Copy thông tin trước khi xóa symrgtbl
   freerg->rg_start = caller->mm->symrgtbl[rgid].rg_start;
   freerg->rg_end = caller->mm->symrgtbl[rgid].rg_end;
   freerg->rg_next = NULL;
 
   if (freerg->rg_start >= freerg->rg_end) { // nếu vùng invalid
     free(freerg);
     return -1;
   }
 
   // Xóa trong symrgtbl
   caller->mm->symrgtbl[rgid].rg_start = 0;
   caller->mm->symrgtbl[rgid].rg_end = 0;
   caller->mm->symrgtbl[rgid].rg_next = NULL;
 
   // Thêm vào freerg_list
   enlist_vm_freerg_list(caller->mm, freerg);
   return 0;
 }
 
 /*liballoc - PAGING-based allocate a region memory
  *@proc:  Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 int liballoc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
 {
   /* TODO Implement allocation on vm area 0 */
   int addr;
   int result = __alloc(proc, 0, reg_index, size, &addr);
 
   if (result == 0) {
     printf("===== PHYSICAL MEMORY AFTER ALLOCATION =====\n");
     printf("PID=%d - Region=%d - Address=%08X - Size=%d byte\n",
            proc->pid, reg_index, addr, size);
     print_pgtbl(proc, 0, proc->mm->mmap->vm_end);
     printf("================================================================\n");
   }
   return result;
 }
 
 /*libfree - PAGING-based free a region memory
  *@proc: Process executing the instruction
  *@size: allocated size
  *@reg_index: memory region ID (used to identify variable in symbole table)
  */
 
 int libfree(struct pcb_t *proc, uint32_t reg_index)
 {
   printf("===== PHYSICAL MEMORY AFTER DEALLOCATION =====\n");
   printf("PID=%d - Region=%d\n", proc->pid, reg_index);
   int result = __free(proc, 0, reg_index);
 
   uint32_t end = proc->mm->mmap->vm_end;
   print_pgtbl(proc, 0, end);
   printf("================================================================\n");
   return result;
 }
 
 /*pg_getpage - get the page in ram
  *@mm: memory region
  *@pagenum: PGN
  *@framenum: return FPN
  *@caller: caller
  *
  */
 int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
 {
   uint32_t pte = mm->pgd[pgn];
 
   if (!PAGING_PAGE_PRESENT(pte))
   { /* Page is not online, make it actively living */
     int vicpgn, swpfpn; 
     //int vicfpn;
     int vicfpn;
     //uint32_t vicpte;
     uint32_t vicpte;
     //int tgtfpn = PAGING_PTE_SWP(pte);//the target frame storing our variable
     int tgtfpn = PAGING_PTE_SWP(pte);
     /* TODO: Play with your paging theory here */
     /* Find victim page */
    int flag= find_victim_page(caller->mm, &vicpgn);
     if(flag==-1){
       perror("find_victim_page failed, maybe no page allocated\n");
       return flag;
     }
     /* Get free frame in MEMSWP */
     MEMPHY_get_freefp(caller->active_mswp, &swpfpn);
 
     /* TODO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/
     vicpte = caller->mm->pgd[vicpgn];
     vicfpn = PAGING_FPN(vicpte);
     /* TODO copy victim frame to swap 
      * SWP(vicfpn <--> swpfpn)
      * SYSCALL 17 sys_memmap 
      * with operation SYSMEM_SWP_OP
      */
     //struct sc_regs regs;
     //regs.a1 =...
     //regs.a2 =...
     //regs.a3 =..
    /* struct sc_regs regs;
     regs.a1 = vicfpn;
     regs.a2 = swpfpn;
     regs.a3 = SYSMEM_SWP_OP;*/
    // 4. Lấy frame lưu giá trị cần truy xuất từ swap (nếu có)
    tgtfpn = PAGING_PTE_SWP(pte);

    // 5. Gọi syscall để copy victim từ RAM -> SWAP
    struct sc_regs regs1;
    regs1.a1 = vicfpn;        // nguồn: RAM
    regs1.a2 = swpfpn;        // đích: SWAP
    regs1.a3 = SYSMEM_SWP_OP;
    syscall(caller, 17, &regs1);
     /* SYSCALL 17 sys_memmap */
 
     //syscall(17, &regs, caller);
 
     /* TODO copy target frame form swap to mem 
      * SWP(tgtfpn <--> vicfpn)
      * SYSCALL 17 sys_memmap
      * with operation SYSMEM_SWP_OP
      */
   
     /* TODO copy target frame form swap to mem 
     //regs.a1 =...
     //regs.a2 =...
     //regs.a3 =..
     */
     // 6. Gọi syscall để copy target từ SWAP -> RAM
     struct sc_regs regs2;
     regs2.a1 = tgtfpn;        // nguồn: SWAP
     regs2.a2 = vicfpn;        // đích: RAM
     regs2.a3 = SYSMEM_SWP_OP;
     syscall(caller, 17, &regs2);
     /* SYSCALL 17 sys_memmap */
 
     //syscall(17, &regs, caller);
 
     /* Update page table */
     //pte_set_swap() 
     //mm->pgd;
 
       // 7. Cập nhật lại page table
    pte_set_swap(&mm->pgd[vicpgn], 0, swpfpn); // victim => SWAP
    pte_set_fpn(&mm->pgd[pgn], vicfpn);        // target => RAM
     /* Update its online status of the target page */
     //pte_set_fpn() &
     //mm->pgd[pgn];
     //pte_set_fpn();
     enlist_pgn_node(&caller->mm->fifo_pgn,pgn);
     MEMPHY_put_freefp(caller->active_mswp, tgtfpn);
   }
 
   *fpn = PAGING_FPN(mm->pgd[pgn]);
 
   return 0;
 }
 
 /*pg_getval - read value at given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
 {
  
   //int off = PAGING_OFFST(addr);
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn;
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
 
   /* TODO 
    *  MEMPHY_read(caller->mram, phyaddr, data);
    *  MEMPHY READ 
    *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
    */
 
   int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
 
   
 
   // int phyaddr
   //struct sc_regs regs;
   //regs.a1 = ...
   //regs.a2 = ...
   //regs.a3 = ...
   /* Gọi syscall 17 để đọc */
   struct sc_regs regs;
   regs.a1 = SYSMEM_IO_READ;
    regs.a2 = phyaddr;
    regs.a3 = 0;

    syscall(caller, 17, &regs);

    *data = (BYTE)regs.a3;  // Lấy kết quả đọc từ regs
   /* SYSCALL 17 sys_memmap */
 
   // Update data
   // data = (BYTE)
 
   return 0;
 }
 
 /*pg_setval - write value to given offset
  *@mm: memory region
  *@addr: virtual address to acess
  *@value: value
  *
  */
 int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
 {
  
   //int off = PAGING_OFFST(addr);
   int pgn = PAGING_PGN(addr);
   int off = PAGING_OFFST(addr);
   int fpn;
 
   /* Get the page to MEMRAM, swap from MEMSWAP if needed */
   if (pg_getpage(mm, pgn, &fpn, caller) != 0)
     return -1; /* invalid page access */
 
   /* TODO
    *  MEMPHY_write(caller->mram, phyaddr, value);
    *  MEMPHY WRITE
    *  SYSCALL 17 sys_memmap with SYSMEM_IO_WRITE
    */
   // int phyaddr
   int phyaddr=(fpn << PAGING_ADDR_FPN_LOBIT) + off;
   //struct sc_regs regs;
   //regs.a1 = ...
   //regs.a2 = ...
   //regs.a3 = ...
 
   /* SYSCALL 17 sys_memmap */
     /* Thiết lập thanh ghi syscall */
     struct sc_regs regs;
     regs.a1 = SYSMEM_IO_WRITE;
     regs.a2 = phyaddr;
     regs.a3 = value;
 
     /* Gọi syscall 17 */
     syscall(caller, 17, &regs);
   // Update data
   // data = (BYTE) 
    
   return 0;
 }
 
 /*__read - read value in region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
     return -1;
 
   pg_getval(caller->mm, currg->rg_start + offset, data, caller);
 
   return 0;
 }
 
 /*libread - PAGING-based read a region memory */
 int libread(
     struct pcb_t *proc, // Process executing the instruction
     uint32_t source,    // Index of source register
     uint32_t offset,    // Source address = [source] + [offset]
     uint32_t* destination)
 {
   BYTE data;
   int val = __read(proc, 0, source, offset, &data);
 /* TODO update result of reading action*/
   //destination 
   *destination=( uint32_t ) data;
 #ifdef IODUMP
  printf("===== PHYSICAL MEMORY AFTER READING =====\n");
   printf("read region=%d offset=%d value=%d\n", source, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); // print max TBL
 #endif
 MEMPHY_dump(proc->mram);
 #endif
 
   return val;
 }
 
 /*__write - write a region memory
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@offset: offset to acess in memory region
  *@rgid: memory region ID (used to identify variable in symbole table)
  *@size: allocated size
  *
  */
 int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
 {
   struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
     return -1;
 
   pg_setval(caller->mm, currg->rg_start + offset, value, caller);
   
   return 0;
 }
 
 /*libwrite - PAGING-based write a region memory */
 int libwrite(
     struct pcb_t *proc,   // Process executing the instruction
     BYTE data,            // Data to be wrttien into memory
     uint32_t destination, // Index of destination register
     uint32_t offset)
 {
 #ifdef IODUMP
 printf("===== PHYSICAL MEMORY AFTER WRITING =====\n");
   printf("write region=%d offset=%d value=%d\n", destination, offset, data);
 #ifdef PAGETBL_DUMP
   print_pgtbl(proc, 0, -1); //print max TBL
 #endif
 MEMPHY_dump(proc->mram);
 #endif
 
   return __write(proc, 0, destination, offset, data);
 }
 
 /*free_pcb_memphy - collect all memphy of pcb
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@incpgnum: number of page
  */
 int free_pcb_memph(struct pcb_t *caller)
 {
   int pagenum, fpn;
   uint32_t pte;
 
 
   for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
   {
     pte= caller->mm->pgd[pagenum];
 
     if (!PAGING_PAGE_PRESENT(pte))
     {
       fpn = PAGING_PTE_FPN(pte);
       MEMPHY_put_freefp(caller->mram, fpn);
     } else {
       fpn = PAGING_PTE_SWP(pte);
       MEMPHY_put_freefp(caller->active_mswp, fpn);    
     }
   }
 
   return 0;
 }
 
 
 /*find_victim_page - find victim page
  *@caller: caller
  *@pgn: return page number
  *
  */
 int find_victim_page(struct mm_struct *mm, int *retpgn)
 {
   struct pgn_t *pg = mm->fifo_pgn;
 
   /* TODO: Implement the theorical mechanism to find the victim page */
   if (pg == NULL) {
     *retpgn = -1;
     return -1;
   }
 
   *retpgn = pg->pgn;
   mm->fifo_pgn = pg->pg_next;
   free(pg);
   return 0;
 }
 
 /*get_free_vmrg_area - get a free vm region
  *@caller: caller
  *@vmaid: ID vm area to alloc memory region
  *@size: allocated size
  *
  */
 int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
 {
   struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
 
   struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;
 
   if (rgit == NULL)
     return -1;
 
   /* Probe unintialized newrg */
   newrg->rg_start = newrg->rg_end = -1;
 
   /* TODO Traverse on list of free vm region to find a fit space */
   //while (...)
   // ..
   while (rgit != NULL) {
     if (rgit->rg_start + size <=rgit->rg_end) { 
       /* Current region has enough space */
       newrg->rg_start = rgit->rg_start;
       newrg->rg_end = rgit->rg_start + size;
 
       /* Update left space in chosen region */
       if (rgit->rg_start + size < rgit->rg_end) {
         rgit->rg_start = rgit->rg_start + size;
       } else { /*Use up all space, remove current node */
         /*Clone next rg node */
         struct vm_rg_struct *nextrg = rgit->rg_next;
 
         /*Cloning */
         if (nextrg != NULL) {
           rgit->rg_start = nextrg->rg_start;
           rgit->rg_end = nextrg->rg_end;
 
           rgit->rg_next = nextrg->rg_next;
 
           free(nextrg);
         } else {						   /*End of free list */
           rgit->rg_start = rgit->rg_end; // dummy, size 0 region
           rgit->rg_next = NULL;
         }
       }
       return 0;
     } else {
       rgit = rgit->rg_next; // Traverse next rg
     }
   }
 
   if (newrg->rg_start == -1) // new region not found
     return -1;
 
   return 0;
 }
 
 //#endif
 
