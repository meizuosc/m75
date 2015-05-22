
static void test_instr_NEON(int printlog) 
{ 
    unsigned long copy_size; 
    unsigned long flags; 
    unsigned long i,j,k, pld_dst;
    unsigned long long t1, t2, t_temp, avg, avg_t;
    unsigned long temp;
    unsigned long long result[10];
    unsigned long long result_t[10];
    char output_line[320];
    char buf[32];
    copy_size = 256;

    if(printlog == 1)
        printk(KERN_EMERG"\n\n\r == Start test NEON ===\n\r");
    i = 0;
    //while(i< 128 + 16*4){
    while(i< 256 + 16*4 -2){
        int line_offset = 0;
        memset(output_line, 0, 320);
/*
        if(i<128){
            copy_size = 512 + i*512; // inc 256 byte from 0~64 KBytes [128 time]
        }else if (i< 128 +16*4){
            copy_size = 1024*64 + (i-128)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
        }
*/
        if(i<256){
            copy_size = 512 + i*512; // inc 256 byte from 0~64 KBytes [128 time]
        }else if (i< 256 +16*4 - 2){
            copy_size = 1024*128 + (i-256)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
            //copy_size = 1024*64 + (i-128)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
        }
        i++;
        mdelay(DELAY_MS);
        preempt_disable();
        local_irq_save(flags);
        inner_dcache_flush_all();
        if(printlog == 1){
            memset(buf, 0, 32);
            sprintf(buf," %lu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }
        avg =0;
        /* no pld */
        for(j = 0; j<8; j++){
            mdelay(DELAY_MS);
            if(flush_cache == 1)
                inner_dcache_flush_all();
#ifdef COUNT_TIME
                /* get timer */
                t1 = sched_clock();
#endif
            /* enable ARM CPU PMU */
            asm volatile(
                    "mov %0, #0\n"
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );

            asm volatile(
                    "push {r0,r1,r2,r9}\n"
                    "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                    "mov r0, %0\n"
                    "mov r1, %1\n"
                    "mov r2, %2\n"
                    "mov r3, %3\n"

                    "subs        r2, r2, #64\n"
                    "1:\n" 
                    // The main loop copies 64 bytes at a time.
                    ".word 0xf421020d\n"
                    //"vld1.8      {d0  - d3},   [r1]!\n"
                    ".word 0xf421420d\n"
                    //"vld1.8      {d4  - d7},   [r1]!\n"
                    //".word 0xf5d1f100\n"
                    //       "pld         [r1, #(64*4)]\n"
                    //".word 0xe2522040\n"
                    "subs        r2, r2, #64\n"
                    ".word 0xf400022d\n"
                    //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                    ".word 0xf400422d\n"
                    //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                    "bge         1b\n"


                    "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "pop {r0,r1,r2, r9}\n"
                    //"pop r2\n"
                    :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                    :"r0","r1","r2","r3"
                        );
            //#if defined(DEBUG_DRAMC_CALIB)
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            result[j] = temp;
#ifdef COUNT_TIME
            t2 = sched_clock();
            result_t[j] = nsec_low(t2 - t1);
#endif
        }// 10 times loop
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >> 3;
#ifdef COUNT_TIME
        avg_t = 0;
        for(j = 0; j<8; j++){
            avg_t += result_t[j];
        }
        avg_t = avg_t >>3;
#endif
        if(printlog == 1){
            //printk(KERN_CONT" %d ", avg );
            memset(buf, 0, 32);
#ifdef COUNT_TIME
            snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
            snprintf(buf,32, " %llu ",avg);
#endif
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
            line_offset += strlen(buf);
        }
        //for(pld_dst = 0; pld_dst<64*16; pld_dst+=64){
        for(pld_dst = 0; pld_dst<64*16*2; pld_dst+= (64*2)){
            avg = 0;
            for(j = 0; j<8; j++){
                mdelay(DELAY_MS);
                if(flush_cache == 1)
                    inner_dcache_flush_all();
            for(k=0; k<2;k++){
                if(k==0)
                    inner_dcache_flush_all();
            
#ifdef COUNT_TIME
                t1 = sched_clock();
#endif
                /* enable ARM CPU PMU */
                asm volatile(
                        "mov %0, #0\n"
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                        "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "ORR %0, %0, #1 << 0\n"   /* enable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c12, 1\n"
                        "ORR %0, %0, #1 << 31\n"
                        "MCR p15, 0, %0, c9, c12, 1\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );

                asm volatile(
                        "push {r0,r1,r2,r9}\n"
                        "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                        "mov r0, %0\n"
                        "mov r1, %1\n"
                        "mov r2, %2\n"
                        "mov r3, %3\n"

                        "subs        r2, r2, #64\n"
                        "1:\n" 
                        // The main loop copies 64 bytes at a time.
                        ".word 0xf421020d\n"
                        //"vld1.8      {d0  - d3},   [r1]!\n"
                        ".word 0xf421420d\n"
                        //"vld1.8      {d4  - d7},   [r1]!\n"
                       // ".word 0xf5d1f100\n" //"pld         [r1, r3]\n"
                        
                        "subs        r2, r2, #64\n"
                        ".word 0xf400022d\n"
                        //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                        ".word 0xf400422d\n"
                        //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                      //  ".word 0xf590f100\n" //pldw
                        "bge         1b\n"

                        "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "pop {r0,r1,r2, r9}\n"
                        //"pop r2\n"
                        :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst) 
                        :"r0","r1","r2","r3"
                            );

                //#if defined(DEBUG_DRAMC_CALIB)
                /* get CPU cycle count from the ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c13, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                } // for k <2
                result[j] = temp;
#ifdef COUNT_TIME
                t2 = sched_clock();
                result_t[j] = nsec_low(t2 - t1);
#endif
            }// 10 times loop
            avg = 0;
            for(j = 0; j<8; j++){
                avg += result[j];
            }
            avg = avg >> 3;
#ifdef COUNT_TIME
            avg_t = 0;
            for(j = 0; j<8; j++){
                avg_t += result_t[j];
            }
            avg_t = avg_t >>3;
#endif
            if(printlog == 1){
                //printk(KERN_EMERG" %d ", avg );
                memset(buf, 0, 32);
#ifdef COUNT_TIME
                snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
                snprintf(buf,32, " %llu ",avg);
#endif
            //    snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
                line_offset += strlen(buf);
            }
        }//pld dist loop
        if(printlog == 1)
            printk(KERN_EMERG"%s\n", output_line);
        local_irq_restore(flags); 
        preempt_enable();
    }
    if(printlog == 1)
        printk(KERN_EMERG"\n\r ====NEON test done ==== flush_cache:%d \n", flush_cache);
}
static void test_instr_NEON_pld(int printlog) 
{ 
    unsigned long copy_size; unsigned long flags; 
    unsigned long i,j,k, pld_dst;
    unsigned long long t1, t2, t_temp, avg, avg_t;
    unsigned long temp;
    unsigned long long result[10];
    unsigned long long result_t[10];
    char output_line[320];
    char buf[32];
    copy_size = 256;
    if(printlog == 1)
        printk(KERN_EMERG"\n\n\r == Start test NEON PLD ===\n\r");
    i = 0;
    //while(i< 128 + 16*4){
    while(i< 256 + 16*4 -2){
        int line_offset = 0;
        memset(output_line, 0, 320);
        //if(i<128){
        if(i<256){
            copy_size = 512 + i*512; // inc 256 byte from 0~64 KBytes [128 time]
        }else if (i< 256 +16*4-2){
            copy_size = 1024*128 + (i-256)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
       // }else if (i< 128 +16 + 32){
        //    copy_size = 1024*1024 + (i-128-16)*1024*128; // inc 64Kbyte form 64KB~1MB [32 time]
        }
        i++;
        mdelay(DELAY_MS);
        preempt_disable();
        local_irq_save(flags);
        inner_dcache_flush_all();
        if(printlog == 1){
            memset(buf, 0, 32);
            sprintf(buf," %lu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }
        avg =0;
        /* no pld */
        for(j = 0; j<8; j++){
            mdelay(DELAY_MS);
            if(flush_cache == 1)
                inner_dcache_flush_all();
#ifdef COUNT_TIME
                /* get timer */
                t1 = sched_clock();
#endif
            /* enable ARM CPU PMU */
            asm volatile(
                    "mov %0, #0\n"
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );

            asm volatile(
                    "push {r0,r1,r2,r9}\n"
                    "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                    "mov r0, %0\n"
                    "mov r1, %1\n"
                    "mov r2, %2\n"
                    "mov r3, %3\n"

                    "subs        r2, r2, #64\n"
                    "1:\n" 
                    // The main loop copies 64 bytes at a time.
                    ".word 0xf421020d\n"
                    //"vld1.8      {d0  - d3},   [r1]!\n"
                    ".word 0xf421420d\n"
                    //"vld1.8      {d4  - d7},   [r1]!\n"
                    //".word 0xf5d1f100\n"
                    //       "pld         [r1, #(64*4)]\n"
                    //".word 0xe2522040\n"
                    "subs        r2, r2, #64\n"
                    ".word 0xf400022d\n"
                    //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                    ".word 0xf400422d\n"
                    //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                    "bge         1b\n"

                    "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "pop {r0,r1,r2, r9}\n"
                    //"pop r2\n"
                    :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                    :"r0","r1","r2","r3"
                        );

            //#if defined(DEBUG_DRAMC_CALIB)
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            result[j] = temp;
#ifdef COUNT_TIME
            t2 = sched_clock();
            result_t[j] = nsec_low(t2 - t1);
#endif
        }// 10 times loop
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >> 3;
        if(printlog == 1){
            //printk(KERN_CONT" %d ", avg );
            memset(buf, 0, 32);
#ifdef COUNT_TIME
            snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
            snprintf(buf,32, " %llu ",avg);
#endif
            //snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
            line_offset += strlen(buf);
        }
        for(pld_dst = 0; pld_dst<64*16*2; pld_dst+= (64*2)){
            avg = 0;
            for(j = 0; j<8; j++){
                mdelay(DELAY_MS);
             //   if(flush_cache == 1)
                    inner_dcache_flush_all();
            for(k=0; k<2;k++){
                if(k==0)
                    inner_dcache_flush_all();
#ifdef COUNT_TIME
                t1 = sched_clock();
#endif
                /* enable ARM CPU PMU */
                asm volatile(
                        "mov %0, #0\n"
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                        "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "ORR %0, %0, #1 << 0\n"   /* enable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c12, 1\n"
                        "ORR %0, %0, #1 << 31\n"
                        "MCR p15, 0, %0, c9, c12, 1\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );

                asm volatile(
                        "push {r0,r1,r2,r9}\n"
                        "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                        "mov r0, %0\n"
                        "mov r1, %1\n"
                        "mov r2, %2\n"
                        "mov r3, %3\n"

                        "subs        r2, r2, #64\n"
                        "1:\n" 
                        // The main loop copies 64 bytes at a time.
                        ".word 0xf421020d\n"
                        //"vld1.8      {d0  - d3},   [r1]!\n"
                        ".word 0xf421420d\n"
                        //"vld1.8      {d4  - d7},   [r1]!\n"
                        ".word 0xf5d1f100\n" //"pld         [r1, r3]\n"
                        
                        "subs        r2, r2, #64\n"
                        ".word 0xf400022d\n"
                        //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                        ".word 0xf400422d\n"
                        //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                      //  ".word 0xf590f100\n" //pldw
                        "bge         1b\n"

                        "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "pop {r0,r1,r2, r9}\n"
                        //"pop r2\n"
                        :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst) 
                        :"r0","r1","r2","r3"
                            );

                //#if defined(DEBUG_DRAMC_CALIB)
                /* get CPU cycle count from the ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c13, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                } //for k < 2
                result[j] = temp;
#ifdef COUNT_TIME
                t2 = sched_clock();
                result_t[j] = nsec_low(t2 - t1);
#endif
            }// 10 times loop
            avg = 0;
            for(j = 0; j<8; j++){
                avg += result[j];
            }
            avg = avg >> 3;
#ifdef COUNT_TIME
            avg_t = 0;
            for(j = 0; j<8; j++){
                avg_t += result_t[j];
            }
            avg_t = avg_t >>3;
#endif
            if(printlog == 1){
                //printk(KERN_EMERG" %d ", avg );
                memset(buf, 0, 32);
#ifdef COUNT_TIME
                snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
                snprintf(buf,32, " %llu ",avg);
#endif
            //    snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            snprintf(output_line+line_offset, 320 - line_offset, "%s %d",buf, flush_cache);
                line_offset += strlen(buf);
            }
        }//pld dist loop
        if(printlog == 1)
            printk(KERN_EMERG"%s\n", output_line);
        local_irq_restore(flags); 
        preempt_enable();
    }
    if(printlog == 1)
        printk(KERN_EMERG"\n\r ====NEON PLD test done ==== flush_cache:%d \n", flush_cache);
}
static void test_instr_NEON_pld_pldw(int printlog) 
{ 
    unsigned long copy_size; 
    unsigned long flags; unsigned long i,j,k, pld_dst;
    unsigned long long t1, t2, t_temp, avg, avg_t;
    unsigned long temp;
    unsigned long long result[10];
    unsigned long long result_t[10];
    char output_line[320];
    char buf[32];
    copy_size = 256;

    if(printlog == 1)
        printk(KERN_EMERG"\n\n\r == Start test NEON PLD PLDW ===\n\r");
    i = 0;
    while(i< 128 + 16*4){
        int line_offset = 0;
        memset(output_line, 0, 320);
        if(i<128){
            copy_size = 512 + i*512; // inc 256 byte from 0~64 KBytes [128 time]
        }else if (i< 128 +16*4){
            copy_size = 1024*64 + (i-128)*1024*64; // inc 64Kbyte form 64KB~1MB [16*4 time]
       // }else if (i< 128 +16 + 32){
        //    copy_size = 1024*1024 + (i-128-16)*1024*128; // inc 64Kbyte form 64KB~1MB [32 time]
        }
        i++;
        mdelay(DELAY_MS);
        preempt_disable();
        local_irq_save(flags);
    //for(i = 0; i< 8; i++, copy_size += 1024*8){
        //for(i = 0; i< 16; i++, copy_size += 1024*64){
        //for(i = 0; i< 4; i++, copy_size += 1024*1024){
        inner_dcache_flush_all();
        if(printlog == 1){
            //printk(KERN_EMERG" %lu :",copy_size);
            memset(buf, 0, 32);
            sprintf(buf," %lu :",copy_size);
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }
        avg =0;
        /* no pld */
        for(j = 0; j<8; j++){
            mdelay(DELAY_MS);
            if(flush_cache == 1)
                inner_dcache_flush_all();
#ifdef COUNT_TIME
            /* get timer */
            t1 = sched_clock();
#endif
            /* enable ARM CPU PMU */
            asm volatile(
                    "mov %0, #0\n"
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                    "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "ORR %0, %0, #1 << 0\n"   /* enable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c12, 1\n"
                    "ORR %0, %0, #1 << 31\n"
                    "MCR p15, 0, %0, c9, c12, 1\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );

            asm volatile(
                    "push {r0,r1,r2,r9}\n"
                    "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                    "mov r0, %0\n"
                    "mov r1, %1\n"
                    "mov r2, %2\n"
                    "mov r3, %3\n"

                    "subs        r2, r2, #64\n"
                    "1:\n" 
                    // The main loop copies 64 bytes at a time.
                    ".word 0xf421020d\n"
                    //"vld1.8      {d0  - d3},   [r1]!\n"
                    ".word 0xf421420d\n"
                    //"vld1.8      {d4  - d7},   [r1]!\n"
                    //".word 0xf5d1f100\n"
                    //       "pld         [r1, #(64*4)]\n"
                    //".word 0xe2522040\n"
                    "subs        r2, r2, #64\n"
                    ".word 0xf400022d\n"
                    //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                    ".word 0xf400422d\n"
                    //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                    "bge         1b\n"


                    "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                    "pop {r0,r1,r2, r9}\n"
                    //"pop r2\n"
                    :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst)
                    :"r0","r1","r2","r3"
                        );

            //#if defined(DEBUG_DRAMC_CALIB)
            /* get CPU cycle count from the ARM CPU PMU */
            asm volatile(
                    "MRC p15, 0, %0, c9, c12, 0\n"
                    "BIC %0, %0, #1 << 0\n"   /* disable */
                    "MCR p15, 0, %0, c9, c12, 0\n"
                    "MRC p15, 0, %0, c9, c13, 0\n"
                    : "+r"(temp)
                    :
                    : "cc"
                    );
            result[j] = temp;
#ifdef COUNT_TIME
            t2 = sched_clock();
            result_t[j] = nsec_low(t2 - t1);
#endif
        }// 10 times loop
        avg = 0;
        for(j = 0; j<8; j++){
            avg += result[j];
        }
        avg = avg >> 3;
#ifdef COUNT_TIME
        avg_t = 0;
        for(j = 0; j<8; j++){
            avg_t += result_t[j];
        }
        avg_t = avg_t >>3;
#endif
        if(printlog == 1){
            memset(buf, 0, 32);
#ifdef COUNT_TIME
            snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
            snprintf(buf,32, " %llu ",avg);
#endif
            snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
            line_offset += strlen(buf);
        }
        for(pld_dst = 0; pld_dst<64*16*2; pld_dst+= (64*2)){
            avg = 0;
            for(j = 0; j<8; j++){
                mdelay(DELAY_MS);
                if(flush_cache == 1)
                    inner_dcache_flush_all();
            for(k=0; k<2;k++){
                if(k==0)
                    inner_dcache_flush_all();
#ifdef COUNT_TIME
                t1 = sched_clock();
#endif
                /* enable ARM CPU PMU */
                asm volatile(
                        "mov %0, #0\n"
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "ORR %0, %0, #1 << 2\n"   /* reset cycle count */
                        "BIC %0, %0, #1 << 3\n"   /* count every clock cycle */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "ORR %0, %0, #1 << 0\n"   /* enable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c12, 1\n"
                        "ORR %0, %0, #1 << 31\n"
                        "MCR p15, 0, %0, c9, c12, 1\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );

                asm volatile(
                        "push {r0,r1,r2,r9}\n"
                        "stmfd        sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n" 
                        "mov r0, %0\n"
                        "mov r1, %1\n"
                        "mov r2, %2\n"
                        "mov r3, %3\n"

                        "subs        r2, r2, #64\n"
                        "1:\n" 
                        // The main loop copies 64 bytes at a time.
                        ".word 0xf421020d\n"
                        //"vld1.8      {d0  - d3},   [r1]!\n"
                        ".word 0xf421420d\n"
                        //"vld1.8      {d4  - d7},   [r1]!\n"
                        ".word 0xf5d1f100\n" //"pld         [r1, r3]\n"
                        
                        "subs        r2, r2, #64\n"
                        ".word 0xf400022d\n"
                        //"vst1.8      {d0  - d3},   [r0, :128]!\n"
                        ".word 0xf400422d\n"
                        //"vst1.8      {d4  - d7},   [r0, :128]!\n"
                        ".word 0xf590f100\n" //pldw
                        "bge         1b\n"

                        "ldmfd       sp!, {r3, r4, r5, r6, r7, r8, r12, lr}\n"
                        "pop {r0,r1,r2, r9}\n"
                        //"pop r2\n"
                        :: "r"(&buffer_dst), "r"(&buffer_src), "r"(copy_size), "r"(pld_dst) 
                        :"r0","r1","r2","r3"
                            );

                //#if defined(DEBUG_DRAMC_CALIB)
                /* get CPU cycle count from the ARM CPU PMU */
                asm volatile(
                        "MRC p15, 0, %0, c9, c12, 0\n"
                        "BIC %0, %0, #1 << 0\n"   /* disable */
                        "MCR p15, 0, %0, c9, c12, 0\n"
                        "MRC p15, 0, %0, c9, c13, 0\n"
                        : "+r"(temp)
                        :
                        : "cc"
                        );
                } // for k < 2
                result[j] = temp;
#ifdef COUNT_TIME
                t2 = sched_clock();
                result_t[j] = nsec_low(t2 - t1);
#endif
            }// 10 times loop
            avg = 0;
            for(j = 0; j<8; j++){
                avg += result[j];
            }
            avg = avg >> 3;
#ifdef COUNT_TIME
            avg_t = 0;
            for(j = 0; j<8; j++){
                avg_t += result_t[j];
            }
            avg_t = avg_t >>3;
#endif
            if(printlog == 1){
                //printk(KERN_EMERG" %d ", avg );
                memset(buf, 0, 32);
#ifdef COUNT_TIME
                snprintf(buf,32, " %llu(%llu) ",avg, avg_t);
#else
                snprintf(buf,32, " %llu ",avg);
#endif
                snprintf(output_line+line_offset, 320 - line_offset, "%s",buf);
                line_offset += strlen(buf);
            }
        }//pld dist loop
        if(printlog == 1)
            printk(KERN_EMERG"%s\n", output_line);
        local_irq_restore(flags); 
        preempt_enable();
    }
    if(printlog == 1)
        printk(KERN_EMERG"\n\r ====NEON PLD PLDW test done ==== flush_cache:%d \n", flush_cache);
}
