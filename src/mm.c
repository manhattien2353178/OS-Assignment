// #ifdef MM_PAGING
/*
 * QUẢN LÝ BỘ NHỚ DỰA TRÊN PHÂN TRANG
 * Mô-đun quản lý bộ nhớ mm/mm.c
 */
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
/*
 * init_pte - Khởi tạo mục bảng trang (PTE)
 * @pte      : mục bảng trang cần khởi tạo
 * @pre      : hiện có (present)
 * @fpn      : số khung trang (FPN)
 * @drt      : bẩn (dirty)
 * @swp      : hoán đổi (swap)
 * @swptyp   : loại hoán đổi (swap type)
 * @swpoff   : offset hoán đổi (swap offset)
 */
int init_pte(uint32_t *pte,
             int pre,    // hiện có
             int fpn,    // số khung trang
             int drt,    // bẩn
             int swp,    // hoán đổi
             int swptyp, // loại hoán đổi
             int swpoff) // offset hoán đổi
{
    if (pre != 0) {
        if (swp == 0) { // Không hoán đổi ~ trang trực tuyến
            if (fpn == 0)
                return -1;  // Cài đặt không hợp lệ
            /* Cài đặt hợp lệ với FPN */
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
        }
        else { // trang đã hoán đổi
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
            SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
        }
    }
    return 0;
}
/*
 * pte_set_swap - Thiết lập mục PTE cho trang đã hoán đổi
 * @pte    : mục bảng trang (PTE) mục tiêu
 * @swptyp : loại hoán đổi
 * @swpoff : offset hoán đổi
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    return 0;
}
/*
 * pte_set_fpn - Thiết lập mục PTE cho trang trực tuyến
 * @pte   : mục bảng trang (PTE) mục tiêu
 * @fpn   : số khung trang (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
    return 0;
}
/*
 * vmap_page_range - ánh xạ một khoảng trang tại địa chỉ căn chỉnh
 * @caller           : tiến trình gọi
 * @addr             : địa chỉ bắt đầu đã căn chỉnh với kích thước trang
 * @pgnum            : số lượng trang cần ánh xạ
 * @frames           : danh sách các khung đã ánh xạ
 * @ret_rg           : vùng đã ánh xạ trả về, khung vật lý thực tế đã ánh xạ
 */
int vmap_page_range(struct pcb_t *caller,           // tiến trình gọi
                    int addr,                       // địa chỉ bắt đầu đã căn chỉnh
                    int pgnum,                      // số lượng trang ánh xạ
                    struct framephy_struct *frames, // danh sách các khung đã ánh xạ
                    struct vm_rg_struct *ret_rg)    // vùng đã ánh xạ trả về
{
    int pgit = 0;
    int pgn = PAGING_PGN(addr);
    ret_rg->rg_start = addr;
    ret_rg->rg_end = addr + pgnum * PAGING_PAGESZ; // Tính toán rg_end
    struct framephy_struct *fpit = frames;
    int ret_value = 0;
    /* Ánh xạ khoảng khung vào không gian địa chỉ */
    for (pgit = 0; pgit < pgnum; pgit++) {
        if (fpit == NULL) {
            ret_value = -1;
            break;
        }
        // Xác định địa chỉ bảng trang
        uint32_t *pte = &caller->mm->pgd[pgn + pgit]; // Truy cập mục bảng trang tương ứng
        pte_set_fpn(pte, fpit->fpn);    // Cập nhật mục bảng trang với khung tương ứng
        fpit = fpit->fp_next;
        /* Theo dõi cho các hoạt động thay thế trang sau này (nếu cần) */
        enlist_pgn_node(&caller->mm->fifo_pgn, pgn + pgit);
    }
    return ret_value;
}
/*
 * alloc_pages_range - cấp phát req_pgnum khung trong RAM
 * @caller    : tiến trình gọi
 * @req_pgnum : số lượng trang yêu cầu
 * @frm_lst   : danh sách khung
 */
int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst)
{
    int pgit, fpn;
    struct framephy_struct *newfp_str = NULL;
    int ret_value = 0;
    for (pgit = 0; pgit < req_pgnum; pgit++) {
        if (MEMPHY_get_freefp(caller->mram, &fpn) == 0) {
            struct framephy_struct *newfp_node = malloc(sizeof(struct framephy_struct));
            newfp_node->fpn = fpn;
            newfp_node->owner = caller->mm;
            newfp_node->fp_next = newfp_str;
            newfp_str = newfp_node;
        } else { // Mã lỗi nếu không đủ khung
            ret_value = -3000;
            break;
        }
    }
    *frm_lst = newfp_str;
    return ret_value;
}
/*
 * vm_map_ram - thực hiện ánh xạ tất cả các vùng bộ nhớ ảo vào thiết bị lưu trữ RAM
 * @caller    : tiến trình gọi
 * @astart    : bắt đầu vùng bộ nhớ ảo
 * @aend      : kết thúc vùng bộ nhớ ảo
 * @mapstart  : điểm bắt đầu ánh xạ
 * @incpgnum  : số lượng trang đã ánh xạ
 * @ret_rg    : vùng trả về
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{
    struct framephy_struct *frm_lst = NULL;
    int ret_alloc;
    ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);
    if (ret_alloc < 0 && ret_alloc != -3000)
        return -1;
    // Hết bộ nhớ
    if (ret_alloc == -3000) {
#ifdef MMDBG
        printf("OOM: vm_map_ram out of memory \n");
#endif
        return -1;
    }
    // Ánh xạ tất cả vào RAM
    vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);
    return 0;
}
/* Hoán đổi nội dung trang từ khung nguồn sang khung đích
 * @mpsrc  : bộ nhớ vật lý nguồn
 * @srcfpn : số khung vật lý nguồn (FPN)
 * @mpdst  : bộ nhớ vật lý đích
 * @dstfpn : số khung vật lý đích (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                   struct memphy_struct *mpdst, int dstfpn)
{
    int cellidx;
    int addrsrc, addrdst;
    for (cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++) {
        addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
        addrdst = dstfpn * PAGING_PAGESZ + cellidx;
        BYTE data;
        MEMPHY_read(mpsrc, addrsrc, &data);
        MEMPHY_write(mpdst, addrdst, data);
    }
    return 0;
}
/*
 * Khởi tạo một thể hiện quản lý bộ nhớ trống
 * @mm:     chính nó
 * @caller: chủ sở hữu mm
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
    mm->pgd = malloc(PAGING_MAX_PGN * sizeof(uint32_t));
    
    // Mặc định chủ sở hữu sẽ có ít nhất một vma
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;
    struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
    enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);
    
    // Cập nhật VMA0 tiếp theo
    vma0->vm_next = NULL;
    // Trỏ ngược về chủ sở hữu vma
    vma0->vm_mm = mm;
    
    // Cập nhật mmap
    mm->mmap = vma0;
    return 0;
}
struct vm_rg_struct *init_vm_rg(int rg_start, int rg_end)
{
    struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
    rgnode->rg_start = rg_start;
    rgnode->rg_end = rg_end;
    rgnode->rg_next = NULL;
    return rgnode;
}
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode)
{
    rgnode->rg_next = *rglist;
    *rglist = rgnode;
    return 0;
}
int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
    struct pgn_t *pnode = malloc(sizeof(struct pgn_t));
    pnode->pgn = pgn;
    pnode->pg_next = *plist;
    *plist = pnode;
    return 0;
}
int print_list_fp(struct framephy_struct *ifp)
{
    struct framephy_struct *fp = ifp;
    printf("print_list_fp: ");
    if (fp == NULL) { printf("NULL list\n"); return -1; }
    printf("\n");
    while (fp != NULL) {
        printf("fp[%d]\n", fp->fpn);
        fp = fp->fp_next;
    }
    printf("\n");
    return 0;
}
int print_list_rg(struct vm_rg_struct *irg)
{
    struct vm_rg_struct *rg = irg;
    printf("print_list_rg: ");
    if (rg == NULL) { printf("NULL list\n"); return -1; }
    printf("\n");
    while (rg != NULL) {
        printf("rg[%ld->%ld]\n", rg->rg_start, rg->rg_end);
        rg = rg->rg_next;
    }
    printf("\n");
    return 0;
}
int print_list_vma(struct vm_area_struct *ivma)
{
    struct vm_area_struct *vma = ivma;
    printf("print_list_vma: ");
    if (vma == NULL) { printf("NULL list\n"); return -1; }
    printf("\n");
    while (vma != NULL) {
        printf("va[%ld->%ld]\n", vma->vm_start, vma->vm_end);
        vma = vma->vm_next;
    }
    printf("\n");
    return 0;
}
int print_list_pgn(struct pgn_t *ip)
{
    printf("print_list_pgn: ");
    if (ip == NULL) { printf("NULL list\n"); return -1; }
    printf("\n");
    while (ip != NULL) {
        printf("va[%d]-\n", ip->pgn);
        ip = ip->pg_next;
    }
    printf("\n");
    return 0;
}
int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
    if (caller == NULL) {
        printf("NULL caller\n");
        return -1;
    }

    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    if (end == -1 && cur_vma != NULL) {
        end = cur_vma->vm_end;
    }

    // Align end to next page boundary to ensure full coverage
    if (end % PAGING_PAGESZ != 0) {
        end = ((end / PAGING_PAGESZ) + 1) * PAGING_PAGESZ;
    }

    int pgn_start = PAGING_PGN(start);
    int pgn_end = PAGING_PGN(end);

    printf("print_pgtbl: %d - %d\n", start, end);

    for (int pgit = pgn_start; pgit < pgn_end; pgit++) {
        printf("%08ld: %08X\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
    }

    for (int pgit = pgn_start; pgit < pgn_end; pgit++) {
        uint32_t pte = caller->mm->pgd[pgit];
        if (PAGING_PAGE_PRESENT(pte)) {
            int fpn = PAGING_FPN(pte);
            printf("Page Number: %d -> Frame Number: %d\n", pgit, fpn);
        }
    }
    printf("================================================================\n");
  
    return 0;
}
// #endif
