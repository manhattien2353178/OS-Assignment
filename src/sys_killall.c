/*
 * Copyright (C) 2025 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Sierra release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

 #include "common.h"
 #include "syscall.h"
 #include "stdio.h"
 #include "libmem.h"
 
 #include "queue.h"
 #include <string.h>  // Thêm để dùng strcmp
 #include <stdlib.h>  // Thêm để dùng free
 
 int __sys_killall(struct pcb_t *caller, struct sc_regs *regs)
 {
     char proc_name[100];
     uint32_t data;
     uint32_t memrg = regs->a1;
 
     /* Đọc tên tiến trình từ vùng bộ nhớ */
     int i = 0;
     do {
        //libread(caller, memrg, i, &data) đọc dữ liệu từ vùng bộ nhớ memrg vào biến data tại vị trí i.
        //Dữ liệu được đọc byte-by-byte và lưu vào mảng proc_name.
        //Đoạn mã này tiếp tục đọc từng byte cho đến khi gặp ký tự NULL (\0), đánh dấu kết thúc của tên tiến trình.
         if (libread(caller, memrg, i, &data) != 0) {
             printf("Error: Failed to read from memory region %d at offset %d\n", memrg, i);
             return -1;
         }
         proc_name[i] = (BYTE)data;
         i++;
     } while (data != 0 && i < 100);
     proc_name[i - 1] = '\0';
 
     printf("The procname retrieved from memregionid %d is \"%s\"\n", memrg, proc_name);
 
     /* Duyệt và chấm dứt tiến trình trong running_list */
     struct queue_t *run_q = caller->running_list;
     for (int j = 0; j < run_q->size; j++) {
        //Nếu tên tiến trình (path) trong danh sách khớp với tên tiến trình cần kết thúc (proc_name),
        //Giải phóng bộ nhớ cho tiến trình (free(run_q->proc[j])).
        //Xóa tiến trình khỏi danh sách và điều chỉnh lại danh sách cho hợp lý.
         if (strcmp(run_q->proc[j]->path, proc_name) == 0) {
             free(run_q->proc[j]);
             for (int k = j; k < run_q->size - 1; k++) {
                 run_q->proc[k] = run_q->proc[k + 1];
             }
             run_q->size--;
             j--;
         }
     }
 
     /* Duyệt và chấm dứt tiến trình trong mlq_ready_queue */
     for (int prio = 0; prio < MAX_PRIO; prio++) {
         struct queue_t *q = &caller->mlq_ready_queue[prio];
         for (int j = 0; j < q->size; j++) {
             if (strcmp(q->proc[j]->path, proc_name) == 0) {
                 free(q->proc[j]);
                 for (int k = j; k < q->size - 1; k++) {
                     q->proc[k] = q->proc[k + 1];
                 }
                 q->size--;
                 j--;
             }
         }
     }
 
     return 0;
 }