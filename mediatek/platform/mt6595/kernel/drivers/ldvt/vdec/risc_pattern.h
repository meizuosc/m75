
// 2013/03/28 Chia-Mao Hung For hevc early emulation

#include "vdec.h"

//HEVC HAL
#define HEVC_DRV_WriteReg(IO_BASE, dAddr, dVal)  mt65xx_reg_sync_writel((dVal),(IO_BASE)+(dAddr))
#define HEVC_DRV_ReadReg(IO_BASE, dAddr)        *(volatile UINT32 *)((IO_BASE)+(dAddr))
///////////
#define MAX_DPB_NUM  25

#define EC_SETTINGS
#define EC_VP_MODE


unsigned int width = 256;
unsigned int height = 192;

int debug_mode;
UINT32 error_rate = 0;
char bitstream_name[200];
unsigned int current_dpb_idx;
unsigned int pre_dpb_idx = 0;
int isDecodeTimeOut;

BOOL UFO_MODE;
BOOL Golden_UFO;


UINT32 Dpb_addr[MAX_DPB_NUM], mv_buf_addr[MAX_DPB_NUM];
// for UFO_MODE
UINT32 Dpb_ufo_Y_len_addr[MAX_DPB_NUM], Dpb_ufo_C_len_addr[MAX_DPB_NUM];

UINT32 u4RefCtlUFO;
UINT32 u4DpbUFO;

mm_segment_t oldfs; 

void RISCWrite_MC( UINT32 u4Addr, UINT32 u4Value );
void RISCRead_MC( UINT32 u4Addr, UINT32* pu4Value );
void RISCWrite_MV( UINT32 u4Addr, UINT32 u4Value );
void RISCWrite_PP( UINT32 u4Addr, UINT32 u4Value );
void RISCRead_PP( UINT32 u4Addr, UINT32* pu4Value );
void RISCWrite_HEVC_VLD( UINT32 u4Addr, UINT32 u4Value );
void RISCRead_HEVC_VLD ( UINT32 u4Addr , UINT32* pu4Value);
void RISCWrite_VLD_TOP( UINT32 u4Addr, UINT32 u4Value );
void RISCRead_VLD_TOP( UINT32 u4Addr, UINT32* pu4Value );
void RISCRead_MISC ( UINT32 u4Addr , UINT32* pu4Value);
void RISCWrite_MISC( UINT32 u4Addr, UINT32 u4Value );
void RISCRead_GCON ( UINT32 u4Addr , UINT32* pu4Value);
void RISCWrite_GCON( UINT32 u4Addr, UINT32 u4Value );
int Dump_reg( UINT32 base_r, UINT32 start_r, UINT32 end_r , char* pBitstream_name , UINT32 frame_number, BOOL bDecodeDone );
int Dump_mem( unsigned char* buf, unsigned int size , int  frame_num , unsigned int type , bool isTimeout);
void Dump_ESA_NBM_performane_log( char* pBName, BOOL bIsUFOmode);
void RISC_instructions();

extern void get_random_bytes(void *buf, int nbytes);

int  Wait_decode_finished( unsigned long  start_time ) {
    UINT32 u4RetRegValue = 0;

    RISCRead_MISC( 41 , &u4RetRegValue );
    while ( ((u4RetRegValue>>16) & 0x1) !=  1){
        RISCRead_MISC( 41, &u4RetRegValue );
        if ( ( jiffies -start_time > 1700) ){
            printk("Polling int time out!!!\n");
            return 1;
        }
    }

    RISCRead_MISC( 41, &u4RetRegValue );
    RISCWrite_MISC( 41, u4RetRegValue | 0x1 );
    RISCRead_MISC( 41, &u4RetRegValue );
    RISCWrite_MISC( 41, u4RetRegValue | (1 << 4));
    RISCWrite_MISC( 41, u4RetRegValue & 0xffffffef); 

    RISCRead_MISC( 41 , &u4RetRegValue );
    RISCWrite_MISC( 41 , u4RetRegValue | (0x1<<12) ) ; // clear for VP mode

    return 0;
}

void RISCWrite_MC( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4MC_BASE = 0xF6022000;
    HEVC_DRV_WriteReg( u4MC_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_MC( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCRead_MC( UINT32 u4Addr, UINT32* pu4Value ){
    UINT32 u4MC_BASE = 0xF6022000;
   (*pu4Value) = HEVC_DRV_ReadReg( u4MC_BASE, u4Addr*4  );
   if (debug_mode>0)
        printk("        RISCRead_MC( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_MV( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4MV_BASE = 0xF6024000;
    HEVC_DRV_WriteReg( u4MV_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_MV( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCWrite_PP( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4PP_BASE = 0xF6025000;
    HEVC_DRV_WriteReg( u4PP_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_PP( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCRead_PP( UINT32 u4Addr, UINT32* pu4Value ){
    UINT32 u4PP_BASE = 0xF6025000;
    (*pu4Value) = HEVC_DRV_ReadReg( u4PP_BASE, u4Addr*4  );
    if (debug_mode>0)
        printk("        RISCRead_PP( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_HEVC_VLD( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4HEVC_VLD_BASE = 0xF6028000;
    HEVC_DRV_WriteReg( u4HEVC_VLD_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_HEVC_VLD( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCRead_HEVC_VLD ( UINT32 u4Addr , UINT32* pu4Value){
    UINT32 u4HEVC_VLD_BASE = 0xF6028000;
    (*pu4Value) = HEVC_DRV_ReadReg( u4HEVC_VLD_BASE, u4Addr*4  );
    if (debug_mode>0)
        printk("        RISCRead_HEVC_VLD( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_VLD_TOP( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4VLD_TOP_BASE = 0xF6021800;    
    HEVC_DRV_WriteReg( u4VLD_TOP_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_VLD_TOP( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCRead_VLD_TOP( UINT32 u4Addr, UINT32* pu4Value ){
    UINT32 u4VLD_TOP_BASE = 0xF6021800;
    (*pu4Value) = HEVC_DRV_ReadReg( u4VLD_TOP_BASE, u4Addr*4  );
    if (debug_mode>0)
        printk("        RISCRead_VLD_TOP( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void HEVC_SHIFT_BITS( UINT32 u4Value ){
    UINT32 tempVal;
    int i;
    for ( i=0; i< u4Value/32; i++ ) 
        RISCRead_HEVC_VLD( 32 , &tempVal); 
        
    if( u4Value%32 != 0 ) 
        RISCRead_HEVC_VLD( u4Value%32 , &tempVal);
}

void RISCWrite_VLD( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4VLD_BASE = 0xF6021000;    
    HEVC_DRV_WriteReg( u4VLD_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_VLD( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}

void RISCRead_VLD ( UINT32 u4Addr , UINT32* pu4Value){
    UINT32 u4VLD_BASE = 0xF6021000;  
    (*pu4Value) = HEVC_DRV_ReadReg( u4VLD_BASE, u4Addr*4 );
    if (debug_mode>0)
        printk("        RISCRead_VLD( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}


void  INIT_SEARCH_START_CODE( UINT32 shift_bit ){

    UINT32 u4RetRegValue, bits_cnt;
/*
    RISCRead_HEVC_VLD( 33, &u4RetRegValue);
    RISCWrite_HEVC_VLD( 33, u4RetRegValue | 0x1<<4);
*/

    if ( shift_bit > 0){
        RISCWrite_HEVC_VLD( 37, 0x1<<8);
        RISCRead_HEVC_VLD( 37, &u4RetRegValue);
        while( (u4RetRegValue>>8) & 0x1 !=0  ){
            RISCRead_HEVC_VLD( 37, &u4RetRegValue);
        }
        RISCRead_HEVC_VLD( 72, &bits_cnt);
    }

    while( shift_bit > (bits_cnt+16) ){
        RISCWrite_HEVC_VLD( 37, 0x1<<8);
        RISCRead_HEVC_VLD( 37, &u4RetRegValue);
        while( (u4RetRegValue>>8) & 0x1 !=0  ){
            RISCRead_HEVC_VLD( 37, &u4RetRegValue);
        }
        RISCRead_HEVC_VLD( 72, &bits_cnt);
    }
    
}



void INIT_BARREL_SHIFTER( UINT32 r_ptr ){

    UINT32 u4RetRegValue;

    // HEVC_FLAG
    RISCWrite_HEVC_VLD( 33, 1);
    
    RISCRead_VLD( 59 , &u4RetRegValue );
    RISCWrite_VLD( 59, u4RetRegValue |(1 << 28) );

    // polling Sram stable 
    RISCRead_VLD( 61 , &u4RetRegValue );
    if( (u4RetRegValue >> 15 ) & 0x1 == 1) 
    {
       // polling VLD_61  [0] == 1
        RISCRead_VLD( 61 , &u4RetRegValue );
        while ( (u4RetRegValue & 0x1)   !=  1)    
            RISCRead_VLD( 61 , &u4RetRegValue );
    }

    // read pointer
    RISCWrite_VLD( 44, r_ptr);

    //BITstream DMA async_FIFO  local reset
    RISCRead_VLD( 66 , &u4RetRegValue );
    RISCWrite_VLD( 66, u4RetRegValue |(1 << 8) );
    RISCWrite_VLD( 66, 0);

    //initial fetch
    RISCRead_VLD( 35 , &u4RetRegValue );
    RISCWrite_VLD( 35, u4RetRegValue |(1 << 20) );

    if (debug_mode>0)
        printk("        wait(`VDEC_INI_FETCH_READY == 1);\n");
    
    RISCRead_VLD( 58 , &u4RetRegValue );
    while ( (u4RetRegValue & 0x1) !=  1)
        RISCRead_VLD( 58, &u4RetRegValue );

    //initial barrel shifter
    RISCRead_VLD( 35 , &u4RetRegValue );
    RISCWrite_VLD( 35, u4RetRegValue|(1 << 23) );

    //byte address
    //HEVC_SHIFT_BITS( (r_ptr&0xF) * 8 ); 
    INIT_SEARCH_START_CODE( (r_ptr&0xF) * 8 );

    //------------------------------
}

void PRED_WEIGHT_TABLE(){

    UINT32 u4RetRegValue;

    RISCRead_HEVC_VLD( 37, &u4RetRegValue );

    RISCWrite_HEVC_VLD( 37 , u4RetRegValue |0x1 );
    RISCRead_HEVC_VLD( 37, &u4RetRegValue );
    if (debug_mode>0)
        printk("        wait(`HEVC_FW_DEC_WP_BUSY == 0);\n");
    while ( (u4RetRegValue & 0x1) !=  0 )
        RISCRead_HEVC_VLD( 37, &u4RetRegValue );

}

void REF_PIC_LIST_MOD(){

    UINT32 u4RetRegValue;

    RISCRead_HEVC_VLD( 37, &u4RetRegValue );

    RISCWrite_HEVC_VLD( 37 , u4RetRegValue |(0x1<<4) );
    RISCRead_HEVC_VLD( 37, &u4RetRegValue );
    if (debug_mode>0)
        printk("        wait(`HEVC_FW_DEC_RPLM_BUSY == 0);\n");
    while ( (u4RetRegValue & (0x1<<4)) !=  0 )
        RISCRead_HEVC_VLD( 37, &u4RetRegValue );

}

void HEVC_VDEC_TRIG(){
    RISCWrite_HEVC_VLD( 36, 1);
}

void RISCRead_MISC ( UINT32 u4Addr , UINT32* pu4Value){
    UINT32 u4MISC_BASE = 0xF6020000;  
    (*pu4Value) = HEVC_DRV_ReadReg( u4MISC_BASE, u4Addr*4 );
    if (debug_mode>0)
        printk("        RISCRead_MISC( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_MISC( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4MISC_BASE = 0xF6020000;   
    HEVC_DRV_WriteReg( u4MISC_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk("        RISCWrite_MISC( %d, %d );  //u4val = 0x%08X\n", u4Addr, u4Value, u4Value);
}
void RISCRead_VDEC_TOP ( UINT32 u4Addr , UINT32* pu4Value){
    UINT32 u4MISC_BASE = 0xF6020000;  
    (*pu4Value) = HEVC_DRV_ReadReg( u4MISC_BASE, u4Addr*4 );
    if (debug_mode>0)
        printk("        RISCRead_MISC( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_VDEC_TOP( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4MISC_BASE = 0xF6020000;   
    HEVC_DRV_WriteReg( u4MISC_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk ("          RISCWrite_MISC( %u , 0x%08X );\n",u4Addr,u4Value );
}

void RISCRead_GCON ( UINT32 u4Addr , UINT32* pu4Value){
    UINT32 u4GCON_BASE = 0xF6000000;  
    (*pu4Value) = HEVC_DRV_ReadReg( u4GCON_BASE, u4Addr*4 );
    if (debug_mode>0)
        printk("        RISCRead_GCON( %d); /* return 0x%08X */\n", u4Addr, (*pu4Value));
}

void RISCWrite_GCON( UINT32 u4Addr, UINT32 u4Value ){
    UINT32 u4GCON_BASE = 0xF6000000;   
    HEVC_DRV_WriteReg( u4GCON_BASE , u4Addr*4 , u4Value);
    if (debug_mode>0)
        printk ("          RISCWrite_GCON( %u , 0x%08X );\n",u4Addr,u4Value );
}

void  HW_RESET( ){
    UINT32 u4RetRegValue;
    // HW reset
    RISCWrite_VLD(66,  0x101);
    RISCWrite_MISC(50,  0x0);
    RISCWrite_MISC(51, 0x0);
    RISCWrite_MISC(33, 0x2);
    RISCWrite_VLD(66 , 0x0);
    //RISCWrite_GCON(6, 0x1);

}

void Dump_Dram0x9_0xA(){
    char* Dram_dump;
    char* file_name[200];
    struct file *fd_bitstream;
    initKernelEnv(); 
    int i;

    //Dram_dump = ioremap_nocache( 0x90000000, 0x10000000 );

    sprintf( file_name, "/mnt/sdcard/Dram_dump0x9_0xA");
    fd_bitstream = openFile( file_name, O_CREAT|O_RDWR, 0 );

    for (i=0; i<0x100; i++){
        Dram_dump = ioremap_nocache( 0x90000000+ 0x10000000*i/0x100, 0x10000000/0x100 );
        fd_bitstream->f_op->write( fd_bitstream, Dram_dump, 0x10000000/0x100, &fd_bitstream->f_pos );        
        iounmap(Dram_dump);
    }

    closeFile( fd_bitstream );
    set_fs( oldfs );
}


void Dump_yuv( char*  PP_OUT_Y_ADDR , char*  PP_OUT_C_ADDR, UINT32 PIC_SIZE_Y ){
    //transform 16*32 block mode to yuv video

    UINT32 u4YLength, trans_addr;
    UINT32 mask = 0x00FFFFFF;
    struct file *EC_rec_filp;
    struct file *rdFd;
    char file_name[200];
    char* Y_out;
    char* U_out;
    char* V_out;
    char* buffY;
    char* buffC;
    int i,j;

    buffY  = PP_OUT_Y_ADDR;
    buffC = PP_OUT_C_ADDR;

    initKernelEnv(); 

    sprintf( file_name, "/mnt/sdcard/%s_er%d_rec.yuv", bitstream_name, error_rate );

    rdFd = openFile(file_name,O_RDONLY,0); 
    if (IS_ERR(rdFd) ){
        EC_rec_filp = filp_open( file_name , O_CREAT|O_RDWR,0777 );
    } else {
        closeFile(rdFd); 
        EC_rec_filp = filp_open( file_name , O_APPEND|O_RDWR,0777 );
    }

    u4YLength = width*( (height+31)>>5 ) <<5;

    Y_out = (char*) vmalloc(sizeof(char)*u4YLength);
    U_out = (char*) vmalloc(sizeof(char)*u4YLength/4);
    V_out = (char*) vmalloc(sizeof(char)*u4YLength/4);

    for (i =0; i<u4YLength; i+=16 ){
        trans_addr = (( (i>>4 & mask) % (width>>4& mask)) <<9 ) + ((((i>>4 & mask)/(width>>4& mask)) %32) <<4)
            + (((i>>4 & mask)/(width>>4& mask))/32)*  (((width+63) >>6 ) <<11);
        //printk( "PIC_SIZE_Y: 0x%08X,  u4YLength: 0x%08X, i(0x%08X) <- trans_addr(0x%08X)\n", PIC_SIZE_Y, u4YLength, i, trans_addr );
        memcpy( Y_out+i, buffY+trans_addr, 16);
    }

    j = 0;
    for (i =0; i<u4YLength/2; i+=16 ){
        trans_addr = (( (i>>4 & mask) % (width>>4& mask)) <<8 ) + ((((i>>4 & mask)/(width>>4& mask)) %16) <<4)
            + (((i>>4 & mask)/(width>>4& mask))/16)*  (((width+63) >>6 ) <<10);
        //printk( "PIC_SIZE_C: 0x%08X,  u4YLength/2: 0x%08X, j(0x%08X) <- trans_addr(0x%08X)\n", PIC_SIZE_Y/2, u4YLength/2, j, trans_addr );
        U_out[j] = buffC[trans_addr];
        U_out[j+1] = buffC[trans_addr+2];
        U_out[j+2] = buffC[trans_addr+4];
        U_out[j+3] = buffC[trans_addr+6];
        U_out[j+4] = buffC[trans_addr+8];
        U_out[j+5] = buffC[trans_addr+10];
        U_out[j+6] = buffC[trans_addr+12];
        U_out[j+7] = buffC[trans_addr+14];
        j += 8;
    }
    
    j = 0;
    for (i =0; i<u4YLength/2; i+=16 ){
        trans_addr = (( (i>>4 & mask) % (width>>4& mask)) <<8 ) + ((((i>>4 & mask)/(width>>4& mask)) %16) <<4)
            + (((i>>4 & mask)/(width>>4& mask))/16)*  (((width+63) >>6 ) <<10);
        //printk( "PIC_SIZE_C: 0x%08X,  u4YLength/2: 0x%08X, j(0x%08X) <- trans_addr(0x%08X)\n", PIC_SIZE_Y/2, u4YLength/2, j, trans_addr );
        V_out[j] = buffC[trans_addr+1];
        V_out[j+1] = buffC[trans_addr+3];
        V_out[j+2] = buffC[trans_addr+5];
        V_out[j+3] = buffC[trans_addr+7];
        V_out[j+4] = buffC[trans_addr+9];
        V_out[j+5] = buffC[trans_addr+11];
        V_out[j+6] = buffC[trans_addr+13];
        V_out[j+7] = buffC[trans_addr+15];
        j += 8;
    }

    printk("YUV dump bytes: %d \n", u4YLength*3/2);
    EC_rec_filp->f_op->write( EC_rec_filp, Y_out, u4YLength, &EC_rec_filp->f_pos );        
    EC_rec_filp->f_op->write( EC_rec_filp, U_out, u4YLength/4, &EC_rec_filp->f_pos );
    EC_rec_filp->f_op->write( EC_rec_filp, V_out, u4YLength/4, &EC_rec_filp->f_pos );

    vfree(Y_out);
    vfree(U_out);
    vfree(V_out);
   
    closeFile(EC_rec_filp); 
    set_fs( oldfs );
    
}

int Dump_mem( unsigned char* buf, unsigned int size , int  frame_num , unsigned int type , bool isTimeout){
    struct file *filp = NULL;
    static struct file *filp_error = NULL;
    char filename_error[256] = {0};
    char YCM;
    int ret,i;

    if ( type == 1 ){
        YCM = 'Y';
    }else if ( type == 2 ){
        YCM = 'C';
    }else if ( type == 3 ){
        YCM = 'M';
    }else if ( type == 4 ){
        YCM = 'L';
    }else if ( type == 5 ){
        YCM = 'K';
    }else {
        YCM = 'T';
    }

    initKernelEnv(); 
    if (UFO_MODE){
        if(error_rate>0)
            sprintf( filename_error, "/mnt/sdcard/%s_decoded_%d_UFO_ER%d_%c.dat", bitstream_name, frame_num, error_rate, YCM );
        else
            sprintf( filename_error, "/mnt/sdcard/%s_decoded_%d_UFO_%c.dat", bitstream_name, frame_num, YCM );
    }else{
        if(error_rate>0)
            sprintf( filename_error, "/mnt/sdcard/er%d/%s_decoded_%d_ER%d_%c.dat", error_rate, bitstream_name, frame_num, error_rate, YCM );
        else
            sprintf( filename_error, "/mnt/sdcard/%s_decoded_%d_%c.dat", bitstream_name, frame_num, YCM );
    }
    filp = filp_open( filename_error, O_CREAT|O_RDWR,0777 );

    if ( filp_error==NULL ){
        filp_error = filp_open( "/mnt/sdcard/HEVC_test_error_frames" , O_CREAT|O_RDWR,0777 );
    }else{
        filp_error = filp_open( "/mnt/sdcard/HEVC_test_error_frames" , O_APPEND|O_RDWR,0777 );
    }
    
    if(IS_ERR(filp) ||IS_ERR(filp_error) ){
        printk("\nFile Open Error:%s\n",filename_error);
        set_fs( oldfs );
        return -1;
    }
    
    // dump frame data
    ret = filp->f_op->write( filp, buf, size, &filp->f_pos );        
    
    if ( error_rate == 0 && !isTimeout ){
        sprintf(filename_error,"%s %c %d\n", bitstream_name, YCM, frame_num );
        ret = filp_error->f_op->write( filp_error, filename_error, strlen(filename_error), &filp_error->f_pos );
    } else if ( isTimeout ){
        sprintf(filename_error,"%s %c %d timeout\n", bitstream_name, YCM, frame_num );
        ret = filp_error->f_op->write( filp_error, filename_error, strlen(filename_error), &filp_error->f_pos );
    }

    closeFile( filp );
    closeFile( filp_error );

    set_fs( oldfs );
    return 0;
}

int Dump_reg( UINT32 base_r, UINT32 start_r, UINT32 end_r , char* pBitstream_name , UINT32 frame_number, BOOL bDecodeDone){
    
    unsigned char* buf;
    struct file* filp;
    struct file* filp_write;
    char file_name[200];
    char buffer[200];
    int ret,i;
    UINT32 u4Value;
   
    initKernelEnv(); 
    
    sprintf(file_name, "/mnt/sdcard/%s_%d_regDump", pBitstream_name ,frame_number  );
    filp = openFile(file_name,O_RDONLY,0); 
    if (IS_ERR(filp) ){   // if info file exitst-> append; not exitst -> create
        filp_write = filp_open( file_name, O_CREAT|O_RDWR,0777 );

    } else {
        closeFile(filp); 
        filp_write = filp_open( file_name , O_APPEND|O_RDWR,0777 );
    }

    if (bDecodeDone){
        sprintf( buffer,"================== Decode Done register dump ==================\n");
        ret = filp_write->f_op->write( filp_write, buffer , strlen(buffer) , &filp_write->f_pos );
    }else{
        sprintf( buffer,"================== Before trigger decode register dump ==================\n");
        ret = filp_write->f_op->write( filp_write, buffer , strlen(buffer) , &filp_write->f_pos );
    }

    for ( i=start_r ; i<=end_r ; i++ ){
        u4Value = HEVC_DRV_ReadReg( base_r, i*4 );
        if ( base_r == 0xF6021000 )
            sprintf( buffer,"VLD[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6022000 )
            sprintf( buffer,"MC[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6028000 )
            sprintf( buffer,"HEVC_VLD[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6025000 )
            sprintf( buffer,"PP[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6024000 )
            sprintf( buffer,"MV[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6020000 )
            sprintf( buffer,"MISC[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);
        if ( base_r == 0xF6021800 )
            sprintf( buffer,"VLD_TOP[%d] = 0x%08.0X    ",i,u4Value, base_r + 4*i);

        printk("//%s", buffer);
        ret = filp_write->f_op->write( filp_write, buffer , strlen(buffer) , &filp_write->f_pos );
       
        if ( i%2 == 0 ){
            printk("\n");
            ret = filp_write->f_op->write( filp_write, "\n" , strlen("\n") , &filp_write->f_pos );
        }
    }
    printk("\n");
    ret = filp_write->f_op->write( filp_write, "\n" , strlen("\n") , &filp_write->f_pos );
     
    msleep(1);
    closeFile( filp_write );
    set_fs( oldfs );
    return 0;
}

void Margin_padding(UINT32 Ptr_output_Y, UINT32 Ptr_output_C, UINT32 PIC_SIZE_Y )
{
    UINT32 u4RetRegValue;
    UINT32 u4LCUsize;
    UINT32 u4W_lcu, u4H_lcu;
    UINT32 u4Offset, u4CurrentPtr;
    UINT32 u4PadOffset, u4PadSize;
    UINT32 u4RealWidth, u4RealHeight;
    int i;

    RISCRead_VLD_TOP(28, &u4RetRegValue);
    u4RealWidth = u4RetRegValue & 0xFFFF;
    u4RealHeight = (u4RetRegValue>>16) & 0xFFFF;

    RISCRead_HEVC_VLD( 42, &u4RetRegValue);
    u4LCUsize = 1<<((u4RetRegValue>>4) & 0x7);
    u4W_lcu = ((u4RealWidth + 64-1)/64)*64;

    if (debug_mode>0)
        printk("u4RealWidth %d; u4RealHeight %d\n", u4RealWidth,u4RealHeight );

    if ( u4RealHeight%u4LCUsize != 0){

        // PP_OUT_Y_ADDR 0 padding
        u4Offset = (u4RealHeight/32)*32*u4W_lcu;
        u4PadOffset = (u4RealHeight%32)*16;
        u4PadSize = (32-(u4RealHeight%32)) * 16;
        if (debug_mode>0)
            printk("PP_OUT_Y_ADDR:\nu4LCUsize = %d; u4Offset = %d; u4PadOffset =  %d; u4PadSize = %d\n", u4LCUsize, u4Offset,  u4PadOffset, u4PadSize );

        u4CurrentPtr = Ptr_output_Y+u4Offset;
        if (debug_mode>0)
            printk("Ptr_output_Y = 0x%08x; u4CurrentPtr start = 0x%08x\n", Ptr_output_Y, u4CurrentPtr);
        for (i = 0; i < u4W_lcu/16; i++){

            memset ( u4CurrentPtr + u4PadOffset, 0, u4PadSize);
            u4CurrentPtr += (u4PadOffset+u4PadSize);
            if (debug_mode>0)
                printk("i : %d; u4CurrentPtr =  0x%08x\n", i,  u4CurrentPtr);

        }
        memset ( u4CurrentPtr, 0, PIC_SIZE_Y-(u4CurrentPtr-(UINT32)Ptr_output_Y));

        // PP_OUT_C_ADDR 0 padding
        u4Offset = ((u4RealHeight/2)/16)*16*u4W_lcu;
        u4PadOffset = ((u4RealHeight/2)%16)*16;
        u4PadSize = (16-((u4RealHeight/2)%16)) * 16;
        if (debug_mode>0)
            printk("PP_OUT_C_ADDR:\nu4LCUsize = %d; u4Offset = %d; u4PadOffset =  %d; u4PadSize = %d\n", u4LCUsize, u4Offset,  u4PadOffset, u4PadSize );

        u4CurrentPtr = Ptr_output_C+u4Offset;
        if (debug_mode>0)
            printk("Ptr_output_C = 0x%08x; u4CurrentPtr start = 0x%08x\n", Ptr_output_C, u4CurrentPtr);
        for (i = 0; i < u4W_lcu/16; i++){
            memset ( u4CurrentPtr + u4PadOffset, 0, u4PadSize);
            u4CurrentPtr += (u4PadOffset+u4PadSize);
            if (debug_mode>0)
                printk("i : %d; u4CurrentPtr =  0x%08x\n", i,  u4CurrentPtr);
        }
        memset ( u4CurrentPtr, 0, PIC_SIZE_Y/2-(u4CurrentPtr-(UINT32)Ptr_output_C));
        
    }

    if ( u4RealWidth%64 != 0){
        
         // PP_OUT_Y_ADDR 0 padding
        u4Offset = (u4RealWidth/16)*16*32;
        u4PadOffset = u4RealWidth%16;
        u4PadSize = 16-(u4RealWidth%16);
        if (debug_mode>0)
             printk("PP_OUT_Y_ADDR:\nu4Offset = %d; u4PadOffset =  %d; u4PadSize = %d\n", u4Offset,  u4PadOffset, u4PadSize );

        u4CurrentPtr = Ptr_output_Y+u4Offset;
        if (debug_mode>0)
            printk("Ptr_output_Y = 0x%08x; u4CurrentPtr start = 0x%08x\n", Ptr_output_Y, u4CurrentPtr);

        for (i = 1; i <= ((u4RealHeight+31)/32)*32; i++){
            if (debug_mode>0)
                printk("i : %d; u4CurrentPtr =  0x%08x\n", i,  u4CurrentPtr);
            memset( u4CurrentPtr + u4PadOffset, 0,  u4PadSize);
            if ( i%32==0 ){
                u4CurrentPtr += 16;
                memset (  u4CurrentPtr, 0,  ((u4W_lcu/16 - (u4RealWidth/16))-1) * 16*32);
                u4CurrentPtr += (u4Offset+((u4W_lcu/16 - (u4RealWidth/16))-1) * 16*32);
            }else{
                u4CurrentPtr += 16;
            }

        }

        // PP_OUT_C_ADDR 0 padding
        u4Offset = (u4RealWidth/16)*16*16;
        u4PadOffset = u4RealWidth%16;
        u4PadSize = 16-(u4RealWidth%16);
        if (debug_mode>0)
             printk("PP_OUT_C_ADDR:\nu4Offset = %d; u4PadOffset =  %d; u4PadSize = %d\n", u4Offset,  u4PadOffset, u4PadSize );

        u4CurrentPtr = Ptr_output_C+u4Offset;
        if (debug_mode>0)
            printk("Ptr_output_C = 0x%08x; u4CurrentPtr start = 0x%08x\n", Ptr_output_C, u4CurrentPtr);
        for (i = 1; i <= ((u4RealHeight/2+15)/16)*16 ; i++){
            if (debug_mode>0)
                printk("i : %d; u4CurrentPtr =  0x%08x\n", i,  u4CurrentPtr);
            memset( u4CurrentPtr + u4PadOffset, 0,  u4PadSize);
            if ( i%16==0 ){
                u4CurrentPtr += 16;
                memset (  u4CurrentPtr, 0,  ((u4W_lcu/16 - (u4RealWidth/16))-1) * 16*16);
                u4CurrentPtr += (u4Offset+((u4W_lcu/16 - (u4RealWidth/16))-1) * 16*16);
            } else {
                u4CurrentPtr += 16;
            }

        }
   }

    
}


int  Golden_comparison( int frame_num, unsigned int PIC_SIZE_Y, UINT32 Ptr_output_Y, UINT32 Ptr_output_C , UINT32 MV_COL_PIC_SIZE, bool isDump,
                                               UINT32 Dpb_ufo_Y_len_addr, UINT32 Dpb_ufo_C_len_addr, UINT32 UFO_LEN_SIZE_Y, UINT32 UFO_LEN_SIZE_C )
{
    //golden result comparison
    unsigned char *buf_Golden;
    unsigned char *buf_Golden_C;
    char *ptr_base = NULL;
    char file_name[200] = {0};
    struct file *fd; 
    int ret, file_num, file_len; 
    UINT32 u4CompareRange;
    UINT32 u4RetRegValue;
    UINT32 u4LCUsize;
    UINT32 u4W_lcu;
    UINT32 u4RealWidth, u4RealHeight;
    bool bCmpFailFlag = 0;


    RISCRead_VLD_TOP(28, &u4RetRegValue);
    u4RealWidth = u4RetRegValue & 0xFFFF;
    u4RealHeight = (u4RetRegValue>>16) & 0xFFFF;
    u4W_lcu = ((u4RealWidth + 64-1)/64)*64;
    
    initKernelEnv();

    // Load golden file
    buf_Golden = (unsigned char *) vmalloc( PIC_SIZE_Y );
    if (Golden_UFO){
        sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_bits_Y.out", bitstream_name, frame_num);
    }else{
        sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_Y.dat", bitstream_name, frame_num);
    }

    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buf_Golden , 0 ,PIC_SIZE_Y );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: frameY!!!!!!!!!!!!!\n");
    } else {  
        readFile(fd ,buf_Golden, PIC_SIZE_Y);
        closeFile(fd); 
    }

    buf_Golden_C = (unsigned char *) vmalloc( PIC_SIZE_Y/2 );
    if (Golden_UFO){
        sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_bits_CbCr.out", bitstream_name, frame_num);
    }else{
        sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_C.dat", bitstream_name, frame_num);
    }

    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buf_Golden_C , 0 ,PIC_SIZE_Y>>1 );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: frameC!!!!!!!!!!!!!\n");
    } else {  
        readFile(fd ,buf_Golden_C, PIC_SIZE_Y>>1 );
        closeFile(fd); 
    }

    if (Golden_UFO && isDump){
        Margin_padding(buf_Golden, buf_Golden_C, PIC_SIZE_Y);
    }

    //////////////Y golden comparison////////////////////
    u4CompareRange = (((u4RealHeight+31)>>5)<<5) * u4W_lcu;
    ret = memcmp(buf_Golden, Ptr_output_Y, u4CompareRange );
    if ( isDump || ret==0 )
        printk("\n======== Frame %d Golden Y test: %d ========\n", frame_num, ret );

    if (ret !=0 ){
        if (isDump)
            Dump_mem( Ptr_output_Y, PIC_SIZE_Y, frame_num , 1, isDecodeTimeOut); 
        bCmpFailFlag = 1;
    }

    //////////////C golden comparison////////////////////
    u4CompareRange = (((u4RealHeight/2+15)>>4)<<4) * u4W_lcu;
    ret =  memcmp(buf_Golden_C, Ptr_output_C, u4CompareRange );
    if ( isDump || ret==0 )
        printk("\n======== Frame %d Golden C test: %d ========\n", frame_num, ret );

    if (ret !=0 ){
        if (isDump)
            Dump_mem( Ptr_output_C, PIC_SIZE_Y>>1, frame_num, 2, isDecodeTimeOut );
        bCmpFailFlag = 1;
    }
    vfree(buf_Golden_C);


/*
    //////////////MV_buf golden comparison////////////////////
    if (ptr_base){ iounmap(ptr_base); }
    ptr_base = ioremap_nocache( mv_buf_addr[current_dpb_idx], MV_COL_PIC_SIZE );

    sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/mv_col_buf_%d.dat", bitstream_name, frame_num );

    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buf_Golden , 0 ,MV_COL_PIC_SIZE );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: frameC!!!!!!!!!!!!!\n");
    } else {  
        readFile(fd ,buf_Golden, MV_COL_PIC_SIZE );
        closeFile(fd); 
    }

    ret =  memcmp( buf_Golden, ptr_base, MV_COL_PIC_SIZE );
    if ( isDump || ret==0 )
        printk("\n======== Frame %d Golden MV_buff[%d] test: %d ========\n\n\n", frame_num, current_dpb_idx ,ret );

    if (ret !=0 ){
        if (isDump)
            Dump_mem( ptr_base, MV_COL_PIC_SIZE , frame_num, 3, isDecodeTimeOut );
        vfree(buf_Golden);
        iounmap(ptr_base);
        set_fs( oldfs );
        return 1;

    }
*/

    if (Golden_UFO){

        UINT32 cmp_size;

         //////////////Y LEN comparison////////////////////
        sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_len_Y.out", bitstream_name, frame_num);

        fd = openFile(file_name,O_RDONLY,0); 
        memset ( buf_Golden , 0 ,UFO_LEN_SIZE_Y );
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: ufo_len_Y.out!!!!!!!!!!!!!\n");
        } else {  
            cmp_size = readFileSize(fd ,buf_Golden, UFO_LEN_SIZE_Y);
            closeFile(fd); 
        }

        ret = memcmp(buf_Golden, Dpb_ufo_Y_len_addr, (PIC_SIZE_Y+255)>>8 );
        if ( isDump || ret==0 )
            printk("\n======== Frame %d UFO Y LEN test: %d ========\n", frame_num, ret );

        if (ret !=0 ){
            if (isDump)
                Dump_mem( Dpb_ufo_Y_len_addr, cmp_size, frame_num , 4, isDecodeTimeOut); 
            bCmpFailFlag = 1;
        }

        //////////////C LEN comparison////////////////////
        sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_len_CbCr.out", bitstream_name, frame_num);

        fd = openFile(file_name,O_RDONLY,0); 
        memset ( buf_Golden , 0 ,UFO_LEN_SIZE_C);
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: ufo_len_C.out!!!!!!!!!!!!!\n");
        } else {  
            cmp_size = readFileSize(fd ,buf_Golden, UFO_LEN_SIZE_C );
            closeFile(fd); 
        }

        ret =  memcmp(buf_Golden, Dpb_ufo_C_len_addr, UFO_LEN_SIZE_Y>>1 );
        if ( isDump || ret==0 )
            printk("\n======== Frame %d UFO C LEN test: %d ========\n", frame_num, ret );

        if (ret !=0 ){
            if (isDump)
                Dump_mem( Dpb_ufo_C_len_addr, cmp_size, frame_num, 5, isDecodeTimeOut );
            bCmpFailFlag = 1;
        }

    }

    vfree(buf_Golden);
    set_fs( oldfs );

    if ( bCmpFailFlag){
        return 1;    
    } else {
        return 0;    
    }
    
}


void Load_reference_frame( UINT32* Dpb_addr,  UINT32* mv_buf_addr, UINT32 frame_num, UINT32 MV_COL_PIC_SIZE, UINT32 PIC_SIZE, UINT32 PIC_SIZE_Y, UINT32 UFO_LEN_SIZE_Y, UINT32 UFO_LEN_SIZE_C   )
{
    char buff[512] = {0};
    char *ptr_base = NULL;
    char *ptr_base2 = NULL;  
    char *ptr_base3 = NULL;  
    char *ptr_base4 = NULL;  
    char file_name[200] = {0};
    struct file *fd; 
    int ret, ref_num; 
    int Dpb_frame_id[MAX_DPB_NUM];
    int i, file_num, file_len;

    initKernelEnv();
    // fd = ttyGS0_open();

    // read ref frame_num
    sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/dpb_size_%d.dat", bitstream_name, frame_num);
    
    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buff , 0 , 512 );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: dpb_size_%d!!!!!!!!!!!!!\n", frame_num );
    } else {            
        readFile(fd ,buff, 512 );
        closeFile(fd); 
    }

    sscanf (buff, "%i ", &ref_num);
    //printk("%d\n", ref_num);
    memset (buff, 0, 512 );


    // read Dpb_frame_id
    sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/dpb_info_%d.dat", bitstream_name, frame_num);

    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buff , 0 , 512 );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: dpb_info_%d!!!!!!!!!!!!!\n", frame_num);
    } else {            
        readFile(fd ,buff, 512 );
        closeFile(fd); 
    }
    
    sscanf (buff, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i"
        , &Dpb_frame_id[0], &Dpb_frame_id[1], &Dpb_frame_id[2], &Dpb_frame_id[3], &Dpb_frame_id[4], &Dpb_frame_id[5]
        , &Dpb_frame_id[6], &Dpb_frame_id[7], &Dpb_frame_id[8], &Dpb_frame_id[9], &Dpb_frame_id[10], &Dpb_frame_id[11]
        , &Dpb_frame_id[12], &Dpb_frame_id[13], &Dpb_frame_id[14], &Dpb_frame_id[15], &Dpb_frame_id[16], &Dpb_frame_id[17]
        , &Dpb_frame_id[18], &Dpb_frame_id[19], &Dpb_frame_id[20], &Dpb_frame_id[21], &Dpb_frame_id[22], &Dpb_frame_id[23], &Dpb_frame_id[24] );
    memset (buff, 0, 512 );


    for ( i = 0; i < ref_num; i++ ){

        if ( Dpb_frame_id[i] == frame_num ){
            printk("Frame %d Output:  Dpb buffer[%d] at 0x%08X -0x%08X\n\n", Dpb_frame_id[i], i , Dpb_addr[i], mv_buf_addr[i]+MV_COL_PIC_SIZE );
            continue;
        } 

        // read Dpb_addr ref frame
        ptr_base = ioremap_nocache( Dpb_addr[i] , PIC_SIZE );
        
        if (UFO_MODE)
            sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_bits_Y.out", bitstream_name, Dpb_frame_id[i]);
        else
            sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_Y.dat", bitstream_name, Dpb_frame_id[i]);

        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: frameY_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {             
            readFile(fd ,ptr_base, PIC_SIZE_Y );
            closeFile(fd); 
        }
        
        if (UFO_MODE)
            sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_bits_CbCr.out", bitstream_name, Dpb_frame_id[i]);
        else
            sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_C.dat", bitstream_name, Dpb_frame_id[i]);
        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: frameC_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {
            readFile(fd , ptr_base+PIC_SIZE_Y, PIC_SIZE -PIC_SIZE_Y );
            closeFile(fd); 
        }

        // read list0 mv_col_buf
        ptr_base2 = ioremap_nocache( mv_buf_addr[i] , MV_COL_PIC_SIZE );
        sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/mv_col_buf_%d.dat", bitstream_name, Dpb_frame_id[i]);

        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: mv_col_buf_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {
            readFile(fd ,ptr_base2, MV_COL_PIC_SIZE );
            closeFile(fd); 
        }

        if (UFO_MODE){
            // read UFO_LEN_SIZE_Y
            ptr_base3 = ioremap_nocache( Dpb_ufo_Y_len_addr[i] , UFO_LEN_SIZE_Y );
            sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_len_Y.out", bitstream_name, Dpb_frame_id[i]);
            
            fd = openFile( file_name, O_RDONLY,0); 
            if (IS_ERR(fd) ){
                printk("[Error] Miss file: ufo_%d_len_Y.out!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
            } else {
                readFile(fd ,ptr_base3, UFO_LEN_SIZE_Y );
                closeFile(fd); 
            }
            iounmap(ptr_base3);

            // read UFO_LEN_SIZE_C
            ptr_base4 = ioremap_nocache( Dpb_ufo_C_len_addr[i] , UFO_LEN_SIZE_C );
            sprintf(file_name, "/mnt/sdcard/%s_pat/ufo_pat/ufo_%d_len_CbCr.out", bitstream_name, Dpb_frame_id[i]);

            fd = openFile( file_name, O_RDONLY,0); 
            if (IS_ERR(fd) ){
                printk("[Error] Miss file: ufo_%d_len_CbCr.out!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
            } else {
                readFile(fd ,ptr_base4, UFO_LEN_SIZE_C );
                closeFile(fd); 
            }
            iounmap(ptr_base4);

        }

        printk("Frame loaded:%d!! Dpb buffer[%d] at 0x%08X -0x%08X\n\n", Dpb_frame_id[i], i , Dpb_addr[i], mv_buf_addr[i]+MV_COL_PIC_SIZE );


        iounmap(ptr_base);
        iounmap(ptr_base2);

    }
    
    set_fs( oldfs );

}


void Check_dpb( UINT32* Dpb_addr,  UINT32* mv_buf_addr, UINT32 frame_num, UINT32 MV_COL_PIC_SIZE, UINT32 PIC_SIZE, UINT32 PIC_SIZE_Y )
{
    char buff[512] = {0};
    char *buff_cmp = NULL;
    char *ptr_base = NULL;
    char *ptr_base2 = NULL;  
    char file_name[200] = {0};
    struct file *fd; 
    int ret, ref_num; 
    int Dpb_frame_id[MAX_DPB_NUM];
    int i, file_num, file_len;

    initKernelEnv();
    // fd = ttyGS0_open();

    // read ref frame_num
    sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/dpb_size_%d.dat", bitstream_name, frame_num);
    
    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buff , 0 , 512 );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: dpb_size_%d!!!!!!!!!!!!!\n", frame_num );
    } else {            
        readFile(fd ,buff, 512 );
        closeFile(fd); 
    }

    sscanf (buff, "%i ", &ref_num);
    //printk("%d\n", ref_num);
    memset (buff, 0, 512 );


    // read Dpb_frame_id
    sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/dpb_info_%d.dat", bitstream_name, frame_num);

    fd = openFile(file_name,O_RDONLY,0); 
    memset ( buff , 0 , 512 );
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: dpb_info_%d!!!!!!!!!!!!!\n", frame_num);
    } else {            
        readFile(fd ,buff, 512 );
        closeFile(fd); 
    }
    
    sscanf (buff, "%i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i %i"
        , &Dpb_frame_id[0], &Dpb_frame_id[1], &Dpb_frame_id[2], &Dpb_frame_id[3], &Dpb_frame_id[4], &Dpb_frame_id[5]
        , &Dpb_frame_id[6], &Dpb_frame_id[7], &Dpb_frame_id[8], &Dpb_frame_id[9], &Dpb_frame_id[10], &Dpb_frame_id[11]
        , &Dpb_frame_id[12], &Dpb_frame_id[13], &Dpb_frame_id[14], &Dpb_frame_id[15], &Dpb_frame_id[16], &Dpb_frame_id[17]
        , &Dpb_frame_id[18], &Dpb_frame_id[19], &Dpb_frame_id[20], &Dpb_frame_id[21], &Dpb_frame_id[22], &Dpb_frame_id[23], &Dpb_frame_id[24] );

    memset (buff, 0, 512 );


    for ( i = 0; i < ref_num; i++ ){

        if ( Dpb_frame_id[i] == frame_num ){
            printk("\nFrame %d Output:  Dpb buffer[%d] at 0x%08X -0x%08X\n\n", Dpb_frame_id[i], i , Dpb_addr[i], mv_buf_addr[i]+MV_COL_PIC_SIZE );
            continue;
        } 
        
        // read Dpb_addr ref frame
        // Y check
        buff_cmp = (char *) vmalloc( PIC_SIZE_Y );
        ptr_base = ioremap_nocache( Dpb_addr[i] , PIC_SIZE );
        sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_Y.dat", bitstream_name, Dpb_frame_id[i]);

        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: frameY_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {             
            readFile(fd ,buff_cmp, PIC_SIZE_Y );
            closeFile(fd); 
        }
        ret =  memcmp(buff_cmp, ptr_base, PIC_SIZE_Y );
        printk("\n======== DPB [%d] content frame_%d_Y check %d ========\n", i,Dpb_frame_id[i], ret );
        vfree(buff_cmp);
        
        if (ret !=0 ){
            Dump_mem( ptr_base, PIC_SIZE_Y, Dpb_frame_id[i] , 1, 0); 
        }


        //C check
        buff_cmp = (char *) vmalloc( PIC_SIZE -PIC_SIZE_Y );
        sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_C.dat", bitstream_name, Dpb_frame_id[i]);

        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: frameC_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {
            readFile(fd , buff_cmp , PIC_SIZE -PIC_SIZE_Y );
            closeFile(fd); 
        }        
        ret =  memcmp(buff_cmp, ptr_base+PIC_SIZE_Y, PIC_SIZE -PIC_SIZE_Y );
        printk("======== DPB [%d] content frame_%d_C check %d ========\n", i,Dpb_frame_id[i], ret );
        vfree(buff_cmp);

        if (ret !=0 ){
            Dump_mem( ptr_base+PIC_SIZE_Y, PIC_SIZE -PIC_SIZE_Y, Dpb_frame_id[i] , 2, 0); 
        }


        // read list0 mv_col_buf
        // MV check
        buff_cmp = (char *) vmalloc( MV_COL_PIC_SIZE );
        ptr_base2 = ioremap_nocache( mv_buf_addr[i] , MV_COL_PIC_SIZE );
        sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/mv_col_buf_%d.dat", bitstream_name, Dpb_frame_id[i]);

        fd = openFile( file_name, O_RDONLY,0); 
        if (IS_ERR(fd) ){
            printk("[Error] Miss file: mv_col_buf_%d!!!!!!!!!!!!!\n",Dpb_frame_id[i]);
        } else {
            readFile(fd ,buff_cmp, MV_COL_PIC_SIZE );
            closeFile(fd); 
        }
        ret =  memcmp(buff_cmp, ptr_base2 ,  MV_COL_PIC_SIZE );
        printk("======== DPB [%d] content mv_col_buf_%d check %d ========\n", i,Dpb_frame_id[i], ret );
        vfree(buff_cmp);
        
        if (ret !=0 ){
            Dump_mem( ptr_base2, MV_COL_PIC_SIZE, Dpb_frame_id[i] , 3, 0); 
        }

        iounmap(ptr_base);
        iounmap(ptr_base2);

    }
    
    set_fs( oldfs );

}



int  Set_error_bitstream( UINT32  bitstream_addr, UINT32 buffer_size, UINT32 file_size, UINT32 offset ){
    
    UINT32 u4tmp;
    UINT32 error_mask;
    int i , j;
    int error_count = 0;
    int rand_buffer_size;
    struct file *filp;
    char filename_EB[256] = {0};
    char *ptr_base = NULL;

    if ( error_rate >0 ){
        
        rand_buffer_size = ( file_size>buffer_size) ? buffer_size : file_size;      
        ptr_base = ioremap_nocache (bitstream_addr, buffer_size);
        error_count = (rand_buffer_size*8)/error_rate;
        
        for(i = 0 ; i < error_count ; i++) {
            get_random_bytes( &u4tmp, 4);
            ptr_base[(u4tmp>>3) % rand_buffer_size] ^= (1<< (u4tmp&0x7));
        }

        printk("Add error bits done, error_count = %d!!\n",error_count);
        
        //dump error bitstream
        initKernelEnv(); 
        
        sprintf( filename_EB, "/mnt/sdcard/%s_EB_0x%08X_%d", bitstream_name, offset, error_rate );
        filp = filp_open( filename_EB, O_CREAT|O_RDWR,0777 );
        
        filp->f_op->write( filp, ptr_base, rand_buffer_size, &filp->f_pos );
        closeFile( filp );
        set_fs( oldfs );

        iounmap(ptr_base);

    }
    return error_count;

}

bool UINT32_get_bit(UINT32 val, UINT32 index)
{
    return ((val >> index) & 0x1);
}

void UINT32_set_bit(UINT32* val, UINT32 index, bool bit){
    *val &= (~( 0x1<<index ));
    *val |= ( bit << index  );
}

void set_Dpb_UFO_ctl( UINT32 risc_val1, int Dpb_index )
{
    if(UFO_MODE && (risc_val1>=0) && (risc_val1<32)){
        UINT32_set_bit(&u4RefCtlUFO, risc_val1, UINT32_get_bit(u4DpbUFO, Dpb_index));
    }
}

void Dump_ESA_NBM_performane_log(char* pBName, BOOL bIsUFOmode)
{    
    char file_name[200];
    char info_buff[500];
    struct file *fd; 
    struct file *filp_info; 
    UINT32 NBM_DLE_NUM,ESA_REQ_DATA_NUM,MC_REQ_DATA_NUM,MC_MBX,MC_MBY,CYC_SYS,INTRA_CNT,LAT_BUF_BYPASS,Y_BLK_CNT,C_BLK_CNT,WAIT_CNT,REQ_CNT,MC_DLE_NBM,CYCLE_DRAM;

    if (bIsUFOmode)
        sprintf(file_name, "/mnt/sdcard/ESA_NBM_log/HEVC_UFO_%s.csv", pBName );
    else
        sprintf(file_name, "/mnt/sdcard/ESA_NBM_log/HEVC_%s.csv", pBName );
    
    fd = openFile(file_name,O_RDONLY,0); 
    if (IS_ERR(fd) ){   // if info file exitst-> append; not exitst -> create
        filp_info = filp_open( file_name, O_CREAT|O_RDWR,0777 );
        sprintf( info_buff, "NBM_DLE_NUM,ESA_REQ_DATA_NUM,MC_REQ_DATA_NUM,MC_MBX,MC_MBY,CYC_SYS,INTRA_CNT,LAT_BUF_BYPASS,Y_BLK_CNT,C_BLK_CNT,WAIT_CNT,REQ_CNT,MC_DLE_NBM,CYCLE_DRAM\n" );
        filp_info->f_op->write( filp_info, info_buff, strlen(info_buff), &filp_info->f_pos );
    } else {
        closeFile(fd); 
        filp_info = filp_open( file_name , O_APPEND|O_RDWR,0777 );
    }

    RISCRead_MC(476, &NBM_DLE_NUM);
    RISCRead_MC(558, &ESA_REQ_DATA_NUM); 
    RISCRead_MC(650, &MC_REQ_DATA_NUM);
    RISCRead_MC(10, &MC_MBX);
    RISCRead_MC(11, &MC_MBY);
    RISCRead_MC(632, &CYC_SYS);
    RISCRead_MC(633, &INTRA_CNT);
    RISCRead_VDEC_TOP(60, &LAT_BUF_BYPASS);
    LAT_BUF_BYPASS &= 0x1;
    RISCRead_MC(634, &Y_BLK_CNT);
    RISCRead_MC(635, &C_BLK_CNT);
    RISCRead_MC(636, &WAIT_CNT);
    RISCRead_MC(493, &REQ_CNT);
    RISCRead_MC(477, &MC_DLE_NBM);
    RISCRead_MC(478, &CYCLE_DRAM);
        
    sprintf( info_buff, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", 
        NBM_DLE_NUM,ESA_REQ_DATA_NUM,MC_REQ_DATA_NUM,MC_MBX,MC_MBY,CYC_SYS,INTRA_CNT,LAT_BUF_BYPASS,Y_BLK_CNT,C_BLK_CNT,WAIT_CNT,REQ_CNT,MC_DLE_NBM,CYCLE_DRAM);
    filp_info->f_op->write( filp_info, info_buff, strlen(info_buff), &filp_info->f_pos );

    closeFile(filp_info); 
    
}


void hevc_test( int frame_start , int frame_end )
{
    unsigned int frame_width, frame_height;
    int frame_num;
    UINT32 i;
    
    UINT32 PIC_SIZE_Y, PIC_SIZE, MV_COL_PIC_SIZE;
    UINT32 CBCR_ADDR_OFFSET, MV_COL_LIST_0_RD_ADDR, MV_COL_LIST_1_RD_ADDR;

    struct file *fd; 
    struct file *fd_bitstream; 
    struct file *filp_info;
    char *ptr_base;
    char *ptr_base_test;
    char * file_name[200] = {0};

    UINT32 risc_val1, risc_val2, risc_val3, temp;
    UINT32 u4UFOtemp;
    UINT32 maxMC = 0;
    UINT32 maxVLD = 0;
    const int buff_risc_size = 16384;
    
    int i4tmp = 0;
    char risc_temp[100];
    char risc_type[100];
    char risc_addr[100];
    char *buff_risc;

    //int file_num, file_len, read_len;
    int ret, Dpb_empty, isFail;
    int repeat_count = 0;
    
    UINT32 u4RetRegValue;
    UINT32 * ptr_temp;
       
    UINT32 physAlloc = 0x90100000;
    UINT32 u4FileSize;
    UINT32 BITSTREAM_OFFSET;
    UINT32 BitstreamOffsetBase;     // for shift bits
    UINT32 FRAME_BUFFER_OFFSET = 0x9166d000;  
    UINT32 BITSTREAM_BUFF_SIZE = 0x5000000;     //0x9700000; 0x5000000;  0x2300000
    UINT32 BITSTREAM_BUFF_SHIFT = 0x4600000;    //0x9600000; 0x4600000;  0x1E00000

    UINT32 PP_OUT_Y_ADDR;
    UINT32 PP_OUT_C_ADDR;
    UINT32 u4Align;

    // for UFO MODE
    UINT32 UFO_Y_LEN_ADDR, UFO_C_LEN_ADDR;
    UINT32 UFO_LEN_SIZE_Y, UFO_LEN_SIZE_C;
    UINT32 PIC_SIZE_REF, PIC_SIZE_BS, PIC_SIZE_Y_BS;
    UINT32 PIC_UFO_WIDTH, PIC_UFO_HEIGHT, PIC_W_H;

    // read pic_width
     sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/pic_width.dat", bitstream_name );
     
     fd = openFile(file_name,O_RDONLY,0); 
     memset ( risc_temp , 0 , 100 );
     if (IS_ERR(fd) ){
         printk("[Error] Miss file: pic_width.dat!!!!!!!!!!!!!\n" );
     } else {            
         readFile(fd ,risc_temp, 100 );
         closeFile(fd); 
     }
     sscanf (risc_temp, "%i ", &width);

    // read pic_height
     sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/pic_height.dat", bitstream_name );
     
     fd = openFile(file_name,O_RDONLY,0); 
     memset ( risc_temp , 0 , 100 );
     if (IS_ERR(fd) ){
         printk("[Error] Miss file: pic_height.dat!!!!!!!!!!!!!\n" );
     } else {            
         readFile(fd ,risc_temp, 100 );
         closeFile(fd); 
     }
     sscanf (risc_temp, "%i ", &height);

    if (UFO_MODE)
        printk("\n==================== UFO_MODE ====================\n\n");

    printk("\n%s\nFrame:%d-%d  Width:%d Height:%d ErrorRate:%d Debug:%d\n",  bitstream_name, frame_start, frame_end, width, height, error_rate ,debug_mode );
        
    frame_width = ((width + 63) >> 6) << 6; //64 align
    frame_height = ((height + 63) >> 6) << 6; //64 align
    
    PIC_SIZE_Y = frame_width*frame_height;
    PIC_SIZE = (( PIC_SIZE_Y + ( PIC_SIZE_Y >> 1 ) + 511 )>> 9) << 9; //512 align
    MV_COL_PIC_SIZE = (frame_width*frame_height >> 8) << 4;
    CBCR_ADDR_OFFSET = PIC_SIZE_Y; // C buffer followed Y buffer 

    if (UFO_MODE){
        UFO_LEN_SIZE_Y = ((((PIC_SIZE_Y+255) >>8) + 63 + (16*8)) >>6) << 6;
        UFO_LEN_SIZE_C = (((UFO_LEN_SIZE_Y>>1) + 15 + (16*8)) >>4) <<4;
        PIC_SIZE_Y_BS = ((PIC_SIZE_Y + 4095) >>12) <<12;
        PIC_SIZE_BS = ((PIC_SIZE_Y_BS + (PIC_SIZE_Y >>1) + 511) >>9) <<9;
        PIC_SIZE_REF = (((PIC_SIZE_BS + (UFO_LEN_SIZE_Y << 1)) + 4095) >> 12) <<12;
    }

    
    //VLD_PAT: BITSTREAM
    u4Align = 128;
    BITSTREAM_OFFSET = (physAlloc + u4Align - 1) & (~(u4Align - 1));    //128 align
    BitstreamOffsetBase = BITSTREAM_OFFSET;
    physAlloc = BITSTREAM_OFFSET + BITSTREAM_BUFF_SIZE;       //FIFO for bitstream 100MB

    if (debug_mode>0)
        printk("BITSTREAM_OFFSET 0x%08X\n",BITSTREAM_OFFSET);

  
    //load bitstream
    ptr_base = ioremap_nocache( BITSTREAM_OFFSET, BITSTREAM_BUFF_SIZE );
    memset(ptr_base, 0, BITSTREAM_BUFF_SIZE);
    if (debug_mode>0)
        printk("BITSTREAM_OFFSET ioremap ptr_base = 0x%08.0X\n",ptr_base);

    initKernelEnv(); 

    if ( error_rate > 0 && debug_mode !=0 ){
        sprintf( file_name, "/mnt/sdcard/%s_EB_0x%08X_%d", bitstream_name, 0, error_rate );
    }else{
        sprintf( file_name, "/mnt/sdcard/%s_pat/vld_pat/bitstream.bin", bitstream_name  );
    }

    fd = openFile(file_name,O_RDONLY,0); 
    if (IS_ERR(fd) ){
        printk("[Error] Miss file: bitstream!!!!!!!!!!!!!\n");
    } else {  
        u4FileSize = readFileSize(fd ,ptr_base,BITSTREAM_BUFF_SIZE);
        closeFile(fd); 
    }
    if ( debug_mode == 0 )
        Set_error_bitstream( BITSTREAM_OFFSET, BITSTREAM_BUFF_SIZE , u4FileSize , 0);

    if (debug_mode>0)
        printk("\n\n==== Bitstream %s, file size = %d ====\n\n", file_name ,u4FileSize);

    if ( debug_mode == 3 ){
        physAlloc = FRAME_BUFFER_OFFSET;
    }

    //  Dpb_addr[i], mv_buf_addr[i] ;
    for ( i=0; i<MAX_DPB_NUM; i++ ){
        if (UFO_MODE){
            u4Align = 4096;
            Dpb_addr[i] = (physAlloc + u4Align - 1) & (~(u4Align - 1));    //4096 align
            Dpb_ufo_Y_len_addr[i] = Dpb_addr[i] + PIC_SIZE_BS;
            Dpb_ufo_C_len_addr[i] = Dpb_ufo_Y_len_addr[i] + UFO_LEN_SIZE_Y;
            if (debug_mode>0){
                printk("Dpb_addr[%d]:0x%08X ", i, Dpb_addr[i] );
                printk("Dpb_ufo_Y_len_addr[%d]:0x%08X ", i, Dpb_ufo_Y_len_addr[i] );
            }
            physAlloc = Dpb_addr[i] + PIC_SIZE_REF;  //PIC_SIZE_REF 4096 aligned
        } else {
            u4Align = 512;
            Dpb_addr[i] = (physAlloc + u4Align - 1) & (~(u4Align - 1));    //512 align
            if (debug_mode>0){
                printk("Dpb_Y_addr[%d]:0x%08X ", i, Dpb_addr[i] );
                printk("Dpb_C_addr[%d]:0x%08X ", i, Dpb_addr[i]+ PIC_SIZE_Y);
            }
            physAlloc = Dpb_addr[i] + PIC_SIZE;  //PIC_SIZE 512 aligned
        }
        u4Align = 128;
        mv_buf_addr[i] = (physAlloc + u4Align - 1) & (~(u4Align - 1));    //128 align
        if (debug_mode>0)
            printk("mv_buf_addr[%d]:0x%08.0X ", i, mv_buf_addr[i] );
        physAlloc = mv_buf_addr[i] + MV_COL_PIC_SIZE;
        if (debug_mode>0)
            printk("End:0x%08.0X\n", physAlloc );

    }
    u4DpbUFO = 0;

     //Power on
    RISCWrite_MISC(61,  0x0);    

    if ( debug_mode == 5 ){
        // pattern test
        int output_bff_index = 1;
        RISC_instructions( );

    } else{

        sprintf(file_name, "/mnt/sdcard/%s_info", bitstream_name  );
        fd = openFile(file_name,O_RDONLY,0); 
        if (IS_ERR(fd) ){   // if info file exitst-> append; not exitst -> create
            filp_info = filp_open( file_name, O_CREAT|O_RDWR,0777 );
            sprintf( risc_temp, "Frame# MC476 VLD_TOP40\n" );
            ret = filp_info->f_op->write( filp_info, risc_temp, strlen(risc_temp), &filp_info->f_pos );
        } else {
            closeFile(fd); 
            filp_info = filp_open( file_name , O_APPEND|O_RDWR,0777 );
        }
   
        Dpb_empty = 1;
        maxMC = 0;
        maxVLD = 0;
        
        int steps = 0;              // for bitstream shift
        for ( frame_num = frame_start; frame_num<=frame_end; frame_num++ ){ //Loop over frames
            
            UINT32 max_mc476[3] = {0};        //val, co-vldTop40, frame_num
            UINT32 max_vldTop40[3]= {0};      //val, co-mc476, frame_num
            unsigned char ucNalUnitType;
            char* clear_output_buff_Y;
            char* clear_output_buff_C;
        
            if (debug_mode>0)
                printk("\n\n======== HEVC test frame %d ========\n\n" , frame_num );

            //verify frame_number risc exist
            sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_vld_risc_%d.pat", bitstream_name, frame_num);
            fd = openFile(file_name,O_RDONLY,0);

            if ( IS_ERR(fd) ){
                break;  //no frame# stop test
            }else{
                closeFile(fd); 
            }

            // get output buffer id for PP_out
            sprintf(file_name, "/mnt/sdcard/%s_pat/mv_pat/out_buf_id_%d.dat", bitstream_name, frame_num);
            
            fd = openFile(file_name,O_RDONLY,0); 
            memset ( risc_temp , 0 , 100 );
            if (IS_ERR(fd) ){
               printk("[Error] Miss file: out_buf_id_%d!!!!!!!!!!!!!\n", frame_num );
            } else {  
               readFile(fd ,risc_temp, 100 );
               closeFile(fd); 
            } 
            sscanf (risc_temp, "%i ", &current_dpb_idx);
            
            //PP_OUT_Y_ADDR 
            PP_OUT_Y_ADDR = Dpb_addr[current_dpb_idx];
            clear_output_buff_Y = ioremap_nocache( PP_OUT_Y_ADDR, PIC_SIZE_Y );
            memset ( clear_output_buff_Y , 0 ,PIC_SIZE_Y );
            iounmap(clear_output_buff_Y);

            if (debug_mode>0)
                printk("\nPP_OUT_Y_ADDR 0x%08X (Dpb_addr[%u])\n",PP_OUT_Y_ADDR, current_dpb_idx );
            
            //PP_OUT_C_ADDR
            if (UFO_MODE){

                PP_OUT_C_ADDR = Dpb_addr[current_dpb_idx] + PIC_SIZE_Y_BS;
                UFO_Y_LEN_ADDR = Dpb_ufo_Y_len_addr[current_dpb_idx];
                UFO_C_LEN_ADDR = Dpb_ufo_C_len_addr[current_dpb_idx];
                
                clear_output_buff_Y = ioremap_nocache( UFO_Y_LEN_ADDR, UFO_LEN_SIZE_Y );
                memset ( clear_output_buff_Y , 0 ,UFO_LEN_SIZE_Y );
                iounmap(clear_output_buff_Y);

                clear_output_buff_C = ioremap_nocache( UFO_C_LEN_ADDR, UFO_LEN_SIZE_C );
                memset ( clear_output_buff_C , 0 ,UFO_LEN_SIZE_C );
                iounmap(clear_output_buff_C);
            
            } else {
                PP_OUT_C_ADDR = Dpb_addr[current_dpb_idx] + PIC_SIZE_Y;
            }
            clear_output_buff_C = ioremap_nocache( PP_OUT_C_ADDR, PIC_SIZE_Y/2 );
            memset ( clear_output_buff_C , 0 ,PIC_SIZE_Y/2 );
            iounmap(clear_output_buff_C);

            // for EC reference DPB set black
            if (error_rate > 0 &&  frame_num == frame_start ){
                for (i=0; i<MAX_DPB_NUM; i++){
                    clear_output_buff_Y = ioremap_nocache( Dpb_addr[i], PIC_SIZE_Y );
                    clear_output_buff_C = ioremap_nocache( Dpb_addr[i]+PIC_SIZE_Y, PIC_SIZE_Y/2 );
                    memset ( clear_output_buff_Y , 0 ,PIC_SIZE_Y );
                    memset ( clear_output_buff_C , 0x80 ,PIC_SIZE_Y/2 );
                    iounmap(clear_output_buff_Y);
                    iounmap(clear_output_buff_C);
                }
            }

            if (debug_mode>0)
                printk("PP_OUT_C_ADDR 0x%08X (Dpb_addr[%u])\n\n",PP_OUT_C_ADDR, current_dpb_idx);


            //Dpb_empty = 1;  //always load reference golden test flag
            
            if ( frame_num != 0 && Dpb_empty ){
                Load_reference_frame( Dpb_addr, mv_buf_addr, frame_num, MV_COL_PIC_SIZE, PIC_SIZE, PIC_SIZE_Y, UFO_LEN_SIZE_Y, UFO_LEN_SIZE_C );
                Dpb_empty = 0;
            }
            
            // HW reset
            HW_RESET( );
            RISCWrite_HEVC_VLD( 33, 1);// HEVC FLAG
            
            //parse RISC

            if (UFO_MODE){

                PIC_UFO_WIDTH = width;
                PIC_UFO_HEIGHT = height;

                PIC_W_H = ((PIC_UFO_WIDTH/16-1)<<16) |(PIC_UFO_HEIGHT/16-1);
                RISCWrite_MC(700, PIC_W_H);
                RISCWrite_MC(664, 0x00000011);
                
                RISCWrite_MC(698, UFO_Y_LEN_ADDR);
                RISCWrite_MC(699, UFO_C_LEN_ADDR);

                RISCWrite_MC(663, PIC_SIZE_BS);
                RISCWrite_MC(701, UFO_LEN_SIZE_Y);
                RISCWrite_MC(343, PIC_SIZE_Y_BS);
                UINT32_set_bit(&u4DpbUFO, current_dpb_idx, 1);
                
                RISCWrite_PP(803, 1); //       error handling no hang

                Golden_UFO = 1;  
            } else {
                RISCWrite_MC(664, 0x0);
                Golden_UFO = 0;
            }
#ifdef EC_SETTINGS
             //EC settings for all error_rate 
            if (pre_dpb_idx==current_dpb_idx){
                pre_dpb_idx = current_dpb_idx+1;
            }
            RISCWrite_MC(247, Dpb_addr[pre_dpb_idx]); 
            RISCWrite_MC(279, Dpb_addr[pre_dpb_idx]); 
            RISCWrite_MC(311, Dpb_addr[pre_dpb_idx]);
            RISCWrite_MV(0, mv_buf_addr[pre_dpb_idx]>>4); 
            RISCWrite_MV(16, mv_buf_addr[pre_dpb_idx]>>4); 
            RISCWrite_HEVC_VLD(56, 0xff7efffb);
            //RISCWrite_HEVC_VLD(52, total_bytes_in_curr_pic); 
            RISCWrite_HEVC_VLD(53, 0x04011101); // 06172013, turn on slice_reconceal_sel
#endif                       
            ///////////////////////////////////////////////////////////////////Read MV risc
            buff_risc = (char *) vmalloc( buff_risc_size );
            temp = 0;

            sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_mv_risc_%d.pat", bitstream_name, frame_num);
            
            fd = openFile(file_name,O_RDONLY,0); 
            memset ( buff_risc , 0 ,buff_risc_size );
            if (IS_ERR(fd) ){
                printk("[Error] Miss file: mv_risc!!!!!!!!!!!!!\n");
            } else {              
                readFile(fd ,buff_risc, buff_risc_size);
                closeFile(fd); 
            }

            //parse MV
            while ( strlen(buff_risc+temp) > 5 ){

                sscanf(buff_risc+temp, "%s %i %s\n", risc_type, &risc_val1, risc_addr );
                i4tmp = simple_strtol(risc_addr, NULL,10);  //for signed overflow
                risc_val2 = (i4tmp<0)?  0x100000000+i4tmp : i4tmp ;
                
                if (debug_mode>0)
                    printk("//MV: %s ( %u , %s );\n", risc_type, risc_val1, risc_addr);
                if ( !strcmp( risc_type,"RISCWrite_MC" ) ){
                    if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_0" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[0] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_1" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[1] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_2" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[2] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_3" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[3] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_4" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[4] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_5" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[5] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_6" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[6] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_7" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[7] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_8" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[8] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_9" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[9] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_10" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[10] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_11" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[11] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_12" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[12] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_13" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[13] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_14" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[14] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_15" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[15] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_16" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[16]  );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_17" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[17] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_18" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[18] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_19" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[19] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_20" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[20] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_21" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[21] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_22" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[22] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_23" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[23] );
                    } else if ( !strcmp( risc_addr,"MC_BUF_Y_ADDR_24" ) ){
                        RISCWrite_MC(risc_val1, Dpb_addr[24] );
                    } else if ( !strncmp( risc_addr,"MC_BUF_C_ADDR", 13 ) ){
                        RISCWrite_MC(risc_val1, 0 );
                    } else{
                       printk("MV-mc parse Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                       printk("MV-mc: %s ( %u , %s );\n\n", risc_type, risc_val1, risc_addr );
                    }
                }else if ( !strcmp( risc_type,"RISCWrite_MV" ) ){
                    
                     if ( !strcmp( risc_addr,"MV_BUF_ADDR_0" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[0] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 0 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_1" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[1] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 1 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_2" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[2] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 2 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_3" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[3] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 3 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_4" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[4] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 4 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_5" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[5] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 5 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_6" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[6] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 6 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_7" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[7] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 7 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_8" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[8] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 8 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_9" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[9] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 9 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_10" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[10] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 10 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_11" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[11] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 11 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_12" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[12] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 12 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_13" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[13] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 13 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_14" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[14] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 14 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_15" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[15] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 15 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_16" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[16] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 16 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_17" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[17] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 17 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_18" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[18] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 18 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_19" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[19] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 19 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_20" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[20] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 20 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_21" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[21] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 21 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_22" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[22] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 22 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_23" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[23] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 23 );
                     } else if ( !strcmp( risc_addr,"MV_BUF_ADDR_24" ) ){
                        RISCWrite_MV(risc_val1, mv_buf_addr[24] >>4 );
                        set_Dpb_UFO_ctl( risc_val1, 24 );
                     } else{
                        RISCWrite_MV(risc_val1,  risc_val2 );
                     }
                }else{
                    printk("MV parse Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    printk("MV: %s ( %u , %s );\n\n", risc_type, risc_val1, risc_addr );
                }
                
                //buffer shift line
                while( (*(buff_risc+temp) != 0x0D) && (*(buff_risc+temp+1) != 0x0A) ){
                    temp ++;
                }
                temp += 2;

            }
            vfree(buff_risc);


              /////////////////////////////////////////////////////////////////////Read MC risc
              buff_risc = (char *) vmalloc( buff_risc_size );
              temp = 0;
             
              sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_mc_risc_%d.pat", bitstream_name, frame_num);
             
              fd = openFile(file_name,O_RDONLY,0); 
              memset ( buff_risc , 0 ,buff_risc_size );
              if (IS_ERR(fd) ){
                 printk("[Error] Miss file: mc_risc!!!!!!!!!!!!!\n");
              } else {  
                 readFile(fd ,buff_risc, buff_risc_size);
                 closeFile(fd); 
              } 
             
             //parse MC
             while ( strlen(buff_risc+temp) > 5 ){
             
                 sscanf(buff_risc+temp, "%s %i %s\n", risc_type, &risc_val1, risc_addr );
                 i4tmp = simple_strtol(risc_addr, NULL,10);  //for signed overflow
                 risc_val2 = (i4tmp<0)?  0x100000000+i4tmp : i4tmp ;
                 
                 if (debug_mode>0)
                     printk("//MC: %s ( %u , %s );\n", risc_type, risc_val1, risc_addr);
                 if ( risc_val1 == 589 ){
                     RISCWrite_MC(risc_val1, CBCR_ADDR_OFFSET );
                 }else{
                     RISCWrite_MC(risc_val1, risc_val2 );
                 }
                 //buffer shift line
                 while( (*(buff_risc+temp) != 0x0D) && (*(buff_risc+temp+1) != 0x0A) ){
                     temp ++;
                 }
                 temp += 2;
             }
             vfree(buff_risc);


               

             /////////////////////////////////////////////////////////////////////Read PP risc
             buff_risc = (char *) vmalloc( buff_risc_size );
             temp = 0;

             sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_pp_risc_%d.pat", bitstream_name, frame_num);
                   
             fd = openFile(file_name,O_RDONLY,0); 
             memset ( buff_risc , 0 ,buff_risc_size );
             if (IS_ERR(fd) ){
                printk("[Error] Miss file: pp_risc!!!!!!!!!!!!!\n");
             } else {  
                readFile(fd ,buff_risc, buff_risc_size);
                closeFile(fd); 
             }
            //parse PP
            while ( strlen(buff_risc+temp) > 5 ){

                sscanf(buff_risc+temp, "%s %i %s\n", risc_type, &risc_val1, risc_addr );
                i4tmp = simple_strtol(risc_addr, NULL,10);  //for signed overflow
                risc_val2 = (i4tmp<0)?  0x100000000+i4tmp : i4tmp ;
                
                if (debug_mode>0)
                    printk("//PP: %s ( %u , %s );\n", risc_type, risc_val1, risc_addr);
                if ( !strcmp(risc_addr, "PP_OUT_Y_ADDR" ) ){
                    RISCWrite_MC(risc_val1, PP_OUT_Y_ADDR>>9 );
                }else  if ( !strcmp(risc_addr, "PP_OUT_C_ADDR" ) ){
                    RISCWrite_MC(risc_val1, PP_OUT_C_ADDR>>8 );
                }else  if ( !strcmp(risc_type, "RISCWrite_MV" ) ){
                    RISCWrite_MV(risc_val1,  risc_val2 );
                }else  if ( !strcmp(risc_type, "RISCWrite_MC" ) ){
                    RISCWrite_MC(risc_val1,  risc_val2 );
                }else{
                    printk("PP parse Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    printk("PP: %s ( %u , %s );\n\n", risc_type, risc_val1, risc_addr );
                }

                //buffer shift line
                while( (*(buff_risc+temp) != 0x0D) && (*(buff_risc+temp+1) != 0x0A) ){
                    temp ++;
                }
                temp += 2;

            }
            vfree(buff_risc);


             /////////////////////////////////////////////////////////////////////Read SQT risc
             buff_risc = (char *) vmalloc( buff_risc_size );
             temp = 0;

             sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_sqt_risc_%d.pat", bitstream_name, frame_num);
             
             fd = openFile(file_name,O_RDONLY,0); 
             memset ( buff_risc , 0 ,buff_risc_size );
             if (IS_ERR(fd) ){
                printk("[Error] Miss file: sqt_risc!!!!!!!!!!!!!\n");
             } else {  
                 readFile(fd ,buff_risc, buff_risc_size);
                 closeFile(fd); 
             }
            //parse SQT
            while ( strlen(buff_risc+temp) > 5 ){

                sscanf(buff_risc+temp, "%s %i %s\n", risc_type, &risc_val1, risc_addr );
                i4tmp = simple_strtol(risc_addr, NULL,10);  //for signed overflow
                risc_val2 = (i4tmp<0)?  0x100000000+i4tmp : i4tmp ;

                if ( !strcmp(risc_type, "RISCWrite_PP" ) ){
                    if (debug_mode>0)
                        printk("//SQT: %s ( %u , %d );\n", risc_type, risc_val1,risc_val2 );
                    RISCWrite_PP(risc_val1, risc_val2 );
                } else if ( !strcmp(risc_type, "RISCWrite_VLD" ) ){
                    if (debug_mode>0)
                       printk("//SQT: %s ( %u , %d );\n", risc_type, risc_val1, risc_val2);
                    RISCWrite_VLD(risc_val1, risc_val2 );
                } else {
                    printk("SQT parse Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    printk("SQT: %s ( %u , %s );\n\n", risc_type, risc_val1, risc_addr );
                }

                //buffer shift line
                while( (*(buff_risc+temp) != 0x0D) && (*(buff_risc+temp+1) != 0x0A) ){
                    temp ++;
                }
                temp += 2;

            }
            vfree(buff_risc);


             /////////////////////////////////////////////////////////////////////Read VLD risc
         
             buff_risc = (char *) vmalloc( buff_risc_size );
             temp = 0;

             sprintf(file_name, "/mnt/sdcard/%s_pat/out/m_vld_risc_%d.pat", bitstream_name, frame_num);
             
             fd = openFile(file_name,O_RDONLY,0); 
             memset ( buff_risc , 0 ,buff_risc_size );
             if (IS_ERR(fd) ){
                printk("[Error] Miss file: vld_risc!!!!!!!!!!!!!\n");
             } else {  
                 readFile(fd ,buff_risc, buff_risc_size);
                 closeFile(fd); 
             }
            //parse VLD 
            while ( strlen(buff_risc+temp) > 5 ){

                sscanf(buff_risc+temp, "%s %i %s\n", risc_type, &risc_val1, risc_addr );
                i4tmp = simple_strtol(risc_addr, NULL,10);  //for signed overflow
                risc_val2 = (i4tmp<0)?  0x100000000+i4tmp : i4tmp ;
                
                if ( !strcmp(risc_type, "INIT_BARREL_SHIFTER" ) ){
                    sscanf(buff_risc+temp, "%s %s %i\n", risc_type, risc_temp, &risc_val1 );
                    if (debug_mode>0)
                        printk("//VLD: %s ( %s + %u );\n", risc_type, risc_temp, risc_val1 );
                    if ( risc_val1 > BITSTREAM_BUFF_SHIFT  && steps != ((risc_val1-(risc_val1%BITSTREAM_BUFF_SHIFT))/BITSTREAM_BUFF_SHIFT) ){
                        //load bitstream
                        steps = ((risc_val1-(risc_val1%BITSTREAM_BUFF_SHIFT))/BITSTREAM_BUFF_SHIFT);

                        memset(ptr_base, 0, BITSTREAM_BUFF_SIZE);

                        if ( error_rate >0 && debug_mode >0){
                            sprintf( file_name, "/mnt/sdcard/%s_EB_0x%08X_%d", bitstream_name+18, steps* BITSTREAM_BUFF_SHIFT, error_rate );
                            fd = openFile(file_name,O_RDONLY,0); 
                            readFile(fd ,ptr_base,BITSTREAM_BUFF_SIZE );
                        } else {
                            sprintf(file_name, "/mnt/sdcard/%s_pat/vld_pat/bitstream.bin", bitstream_name  );
                            fd = openFile(file_name,O_RDONLY,0); 
                            readFileOffset(fd ,ptr_base,BITSTREAM_BUFF_SIZE, steps* BITSTREAM_BUFF_SHIFT );
                        }
                        
                        BitstreamOffsetBase = BITSTREAM_OFFSET -steps*BITSTREAM_BUFF_SHIFT;
                        if ( debug_mode == 0 )
                            Set_error_bitstream( BITSTREAM_OFFSET, BITSTREAM_BUFF_SIZE , u4FileSize-steps* BITSTREAM_BUFF_SHIFT ,steps* BITSTREAM_BUFF_SHIFT);

                        if (debug_mode>0)
                            printk("\n\n======== bitstream shift 0x%08x (%d steps) done ========\n\n",  steps* BITSTREAM_BUFF_SHIFT , steps);

                    }
                    INIT_BARREL_SHIFTER( BitstreamOffsetBase + risc_val1 );

                } else if ( !strcmp(risc_type, "HEVC_SHIFT_BITS" ) ){
                    if (debug_mode>0)
                        printk("//VLD: %s ( %u);\n", risc_type, risc_val1 );
                    HEVC_SHIFT_BITS( risc_val1 );
                } else if ( !strcmp(risc_type, "RISCWrite_HEVC_VLD" ) ){
                    if (debug_mode>0)
                        printk("//VLD: %s ( %u , %d );\n", risc_type, risc_val1, risc_val2 );
#ifdef EC_SETTINGS
                    //EC settings
                    if ( risc_val1 == 45){       
                        if ( risc_val2 & 0x1){  //tiles_enabled_flag
                            RISCWrite_PP(16, 16793617); // 0x0100_4011
                        } else {
                            RISCWrite_PP(16, 16805905); // 0x0100_7011
                        }
                    }
#endif             
                    if ( error_rate > 0 && risc_val1 == 61){
                        ucNalUnitType = (risc_val2 >> 8) & 0xff;
                    }
                    if ( UFO_MODE && risc_val1 == 45){
                        if ( risc_val2 & 0x1){  //if tiles_enabled_flag turn off UFO encoder
                            UINT32_set_bit(&u4DpbUFO, current_dpb_idx, 0);
                            RISCWrite_MC(664, 0x10);     // UFO decoder on
                            Golden_UFO = 0;                           
                        }

                        printk("u4DpbUFO: 0x%08X;  u4RefCtlUFO: 0x%08X  \n", u4DpbUFO, u4RefCtlUFO);
                        RISCWrite_MC(722, 0x1 );
                        RISCWrite_MC(718, u4RefCtlUFO );
                        RISCWrite_MC(719, u4RefCtlUFO );
                        RISCWrite_MC(720, u4RefCtlUFO );
                        
                    }
                    RISCWrite_HEVC_VLD ( risc_val1, risc_val2 );
                } else if ( !strcmp(risc_type, "RISCWrite_VLD_TOP" ) ){
                    if (debug_mode>0)
                        printk("//VLD: %s ( %u, %d );\n", risc_type, risc_val1, risc_val2 );
                    RISCWrite_VLD_TOP  ( risc_val1, risc_val2 );
                } else if ( !strcmp(risc_type, "PRED_WEIGHT_TABLE" ) ){
                    if (debug_mode>0)
                        printk("//VLD: PRED_WEIGHT_TABLE();\n" );
                    PRED_WEIGHT_TABLE();
                } else if ( !strcmp(risc_type, "REF_PIC_LIST_MOD" ) ){
                    if (debug_mode>0)
                        printk("//VLD: REF_PIC_LIST_MOD();\n" );
                    REF_PIC_LIST_MOD();
                } else if ( !strcmp(risc_type, "HEVC_VDEC_TRIG" ) ){

                    if (UFO_MODE){
                        //Set UFO garbage remove mode
                        RISCWrite_PP( 706  , 0x1 );
                    }
                    RISCWrite_VLD_TOP  ( 21,  0x1 );    // turn on VLD TOP [40] information

                    if (debug_mode>0){
                        // dump bitstream buffer before trigger decode
                        if (debug_mode == 3){
                            sprintf( file_name, "/mnt/sdcard/bitstream_bf_trig.bin");
                            fd_bitstream = openFile( file_name, O_CREAT|O_RDWR, 0 );
                            ret = fd_bitstream->f_op->write( fd_bitstream, ptr_base, (BITSTREAM_BUFF_SIZE>u4FileSize)? u4FileSize : BITSTREAM_BUFF_SIZE, &fd_bitstream->f_pos );        
                            closeFile( fd_bitstream );
                        }
                        if (debug_mode == 6 && frame_num == frame_end){   //Dump Dram              
                            char* Dram_dump;
                            Dram_dump = ioremap_nocache( 0x90000000, 0x10000000 );

                            sprintf( file_name, "/mnt/sdcard/Dram_dump_0x9_0xA");
                            fd_bitstream = openFile( file_name, O_CREAT|O_RDWR, 0 );
                            ret = fd_bitstream->f_op->write( fd_bitstream, Dram_dump, 0x10000000, &fd_bitstream->f_pos );        
                            closeFile( fd_bitstream );

                            iounmap(Dram_dump);
                        }
                        
                        Dump_reg (0xF6028000, 0, 0, bitstream_name, frame_num, 0);      //HEVC
                        Dump_reg (0xF6028000, 33, 37, bitstream_name, frame_num, 0);  //HEVC
                        Dump_reg (0xF6028000, 40, 255, bitstream_name, frame_num, 0);     //HEVC
                        Dump_reg (0xF6021000, 33, 255, bitstream_name, frame_num, 0);      //VLD
                        Dump_reg (0xF6024000, 0, 255, bitstream_name, frame_num, 0);        //MV
                        Dump_reg (0xF6022000, 0, 702, bitstream_name, frame_num, 0);        //MC
                        Dump_reg (0xF6025000, 0, 1023, bitstream_name, frame_num, 0);       //PP
                        Dump_reg (0xF6021800, 41, 44, bitstream_name, frame_num, 0);       //VLD_TOP
                        printk("//VLD: HEVC_VDEC_TRIG();\n");
                    }
                    HEVC_VDEC_TRIG();
                    break;
                    
                } else {
                    printk("VLD parse Error!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                    printk("VLD: %s ( %u , %s );\n\n", risc_type, risc_val1, risc_addr );
                }
                
                //buffer shift line
                while( (*(buff_risc+temp) != 0x0D) && (*(buff_risc+temp+1) != 0x0A) ){
                    temp ++;
                }
                temp += 2;

            }
            vfree(buff_risc);

            //polling interrupt
            isDecodeTimeOut = Wait_decode_finished( jiffies );
            
            Dump_ESA_NBM_performane_log( bitstream_name, UFO_MODE );


            if (debug_mode>0 ||isDecodeTimeOut ){
                // dump bitstream buffer before trigger decode
                if (debug_mode == 3){
                    sprintf( file_name, "/mnt/sdcard/bitstream_af_trig.bin");
                    fd_bitstream = openFile( file_name, O_CREAT|O_RDWR,0777 );
                    ret = fd_bitstream->f_op->write( fd_bitstream, ptr_base, (BITSTREAM_BUFF_SIZE>u4FileSize)? u4FileSize : BITSTREAM_BUFF_SIZE , &fd_bitstream->f_pos );        
                    closeFile( fd_bitstream );
                }
                
                printk("Decode_finished!!\n");
                Dump_reg (0xF6028000, 0, 0, bitstream_name, frame_num, 1);      //HEVC
                Dump_reg (0xF6028000, 33, 37, bitstream_name, frame_num, 1);  //HEVC
                Dump_reg (0xF6028000, 40, 255, bitstream_name, frame_num, 1);     //HEVC
                Dump_reg (0xF6028000, 40, 255, bitstream_name, frame_num, 1);     //HEVC
                Dump_reg (0xF6021000, 33, 255, bitstream_name, frame_num, 1);      //VLD
                Dump_reg (0xF6024000, 0, 255, bitstream_name, frame_num, 1);        //MV
                Dump_reg (0xF6022000, 0, 702, bitstream_name, frame_num, 1);        //MC
                Dump_reg (0xF6025000, 0, 1023, bitstream_name, frame_num, 1);       //PP
                Dump_reg (0xF6021800, 41, 44, bitstream_name, frame_num, 1);       //VLD_TOP
             
            }


#ifdef EC_VP_MODE
            //VP mode for end of bitstream error
            RISCRead_HEVC_VLD( 57, &risc_val1 );     

            //if (frame_num == 1){  risc_val1 = 1; }      // force frame num turn on VP mode for debug
            if ( risc_val1 & 0x1 ){

                UINT32 SliceStartLCURow, u4LCUsize, u4RealWidth, u4W_Dram;
                UINT32 pic_real_wifth, pic_real_height, pic_width, pic_height;
                UINT32 MC_130, MC_131, MC_608, VLD_TOP_26, VLD_TOP_28;

                RISCRead_HEVC_VLD( 42, &risc_val1);
                u4LCUsize = 1<<((risc_val1>>4) & 0x7);             
                RISCRead_HEVC_VLD( 62, &risc_val1 );
                SliceStartLCURow = (risc_val1>>16)  & 0x3ff;
                RISCRead_VLD_TOP(28, &risc_val1);
                u4RealWidth = risc_val1 & 0xFFFF; 
                u4W_Dram = ((u4RealWidth + 64-1)/64)*64;

                if ((SliceStartLCURow%2)==1 && u4LCUsize==16 ){
                    SliceStartLCURow--;
                }

                if (UFO_MODE){    //UFO HW constrain
                    while(SliceStartLCURow*u4LCUsize*u4W_Dram % (8*4096) !=0 ||((SliceStartLCURow%2)==1 && u4LCUsize==16 )){
                        SliceStartLCURow--;
                    }
                }  

                // force full frame copy
                //SliceStartLCURow = 0;
              
                printk("//VP mode!!  SliceStartLCURow %d; u4LCUsize %d;\n", SliceStartLCURow, u4LCUsize);

                RISCRead_MC( 130, &pic_real_wifth );
                MC_130 =  ((pic_real_wifth+15)>>4)<<4;
                RISCRead_MC( 131, &pic_real_height);
                pic_real_height -= SliceStartLCURow*u4LCUsize;
                MC_131 = ((pic_real_height+15)>>4)<<4;
                RISCRead_MC( 608, &MC_608);
                VLD_TOP_26 = ((((pic_real_height+15)/16-1)& 0x7ff)<<16) |(((pic_real_wifth+15)/16-1)& 0x7ff);
                VLD_TOP_28 =  (((pic_real_height+15)>>4)<<20) | (((pic_real_wifth+15)>>4)<<4);

                HW_RESET( );

                if (UFO_MODE){
                    printk ("//VP UFO settings\n");
                    pic_width = ((pic_real_wifth + 63)>>6)<<6;
                    pic_height = ((pic_real_height + 31)>>5)<<5;   
                   
                    RISCWrite_MC(700, ((pic_width/16-1)<<16) |(pic_height/16-1));      
                    RISCWrite_MC(664, 0x11);
                    RISCWrite_MC(698, Dpb_ufo_Y_len_addr[current_dpb_idx]+(SliceStartLCURow*u4LCUsize*u4W_Dram/256));
                    RISCWrite_MC(699, Dpb_ufo_C_len_addr[current_dpb_idx]+(SliceStartLCURow*u4LCUsize*u4W_Dram/512));   
                    RISCWrite_MC(663, PIC_SIZE_BS+(SliceStartLCURow*u4LCUsize*u4W_Dram/256)-SliceStartLCURow*u4LCUsize*u4W_Dram);
                    RISCWrite_MC(701, UFO_LEN_SIZE_Y-(SliceStartLCURow*u4LCUsize*u4W_Dram/256)+(SliceStartLCURow*u4LCUsize*u4W_Dram/512));
                    RISCWrite_MC(343, PIC_SIZE_Y_BS-(SliceStartLCURow*u4LCUsize*u4W_Dram)/2);
                    RISCWrite_PP(706, 0x1 );   // UFO garbage remove

                    // bypass  PP setting
                    RISCWrite_MC(139, ((pic_real_wifth+15)>>4));
                    RISCWrite_MC(152, ((pic_real_wifth+15)>>4)-1);
                    RISCWrite_MC(153, ((pic_real_height+15)>>4)-1);
                    RISCWrite_MC(136, 0x1);
                    RISCRead_MC(142, &risc_val1);
                    RISCWrite_MC(142, risc_val1 & (~0x3) );
                    RISCWrite_MC(148, 0x1);
                    RISCRead_MC(525, &risc_val1);
                    RISCWrite_MC(525, risc_val1 & (~0x1) );
                    
/*  clear output buffer for debug
                    clear_output_buff_Y = ioremap_nocache( PP_OUT_Y_ADDR, PIC_SIZE_Y );
                    memset ( clear_output_buff_Y , 0 ,PIC_SIZE_Y );
                    iounmap(clear_output_buff_Y);
                    
                    clear_output_buff_C = ioremap_nocache( PP_OUT_C_ADDR, PIC_SIZE_Y/2 );
                    memset ( clear_output_buff_C , 0 ,PIC_SIZE_Y/2 );
                    iounmap(clear_output_buff_C);                  
*/                  
                    RISCWrite_MC(137, (PP_OUT_Y_ADDR + u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
                    RISCWrite_MC(138, (PP_OUT_C_ADDR + u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);

                }

                printk ("//VP settings: H*W 0x%08x; MC_0 buffer index %d;\n", u4W_Dram*SliceStartLCURow*u4LCUsize, pre_dpb_idx);
                RISCRead_VLD_TOP( 36, &risc_val1 );
                RISCWrite_VLD_TOP( 36,  risc_val1 | (0x1<<1) );  //Turn on VP mode flag

                RISCWrite_MC( 130, MC_130 );
                RISCWrite_MC( 131, MC_131 );
                RISCWrite_MC( 608, MC_608 );
                
                RISCWrite_VLD_TOP( 26, VLD_TOP_26 );
                RISCWrite_VLD_TOP( 28, VLD_TOP_28 );

                RISCWrite_MC( 9, 0x1);
                if (UFO_MODE){
                    RISCWrite_MC( 0, (Dpb_addr[pre_dpb_idx] + u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
                    RISCWrite_MC( 1, (Dpb_addr[pre_dpb_idx] + PIC_SIZE_Y_BS + u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);

                } else {
                    RISCWrite_MC( 0, (Dpb_addr[pre_dpb_idx] + u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
                    RISCWrite_MC( 1, (Dpb_addr[pre_dpb_idx] + PIC_SIZE_Y + u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);
                }
                RISCWrite_MC( 2, (PP_OUT_Y_ADDR + u4W_Dram*SliceStartLCURow*u4LCUsize)>>9);
                RISCWrite_MC( 3, (PP_OUT_C_ADDR + u4W_Dram*SliceStartLCURow*u4LCUsize/2)>>8);
                
                RISCRead_VLD_TOP( 36, &risc_val1 );
                RISCWrite_VLD_TOP( 36,  risc_val1 |0x1 );  // Trigger VP mode 

                RISCRead_MISC( 41 , &risc_val1 );
                RISCWrite_MISC( 41 , risc_val1 & (~(0x1<<12)) ) ;

                isDecodeTimeOut = Wait_decode_finished( jiffies );
                
            }
 
            if (debug_mode>0 || isDecodeTimeOut){
                Dump_reg (0xF6021800, 26, 44, bitstream_name, frame_num, 1);       //VLD_TOP
                Dump_reg (0xF6021000, 33, 255, bitstream_name, frame_num, 1);      //VLD
                Dump_reg (0xF6022000, 0, 702, bitstream_name, frame_num, 1);        //MC
            }
#endif   
            pre_dpb_idx = current_dpb_idx;

            // write mc476 & vld 40 to file
            RISCRead_MC( 476, &risc_val1);
            if ( maxMC < risc_val1 ){
                maxMC = risc_val1;
            }
            printk("max MC[476] = 0x%08X ;", maxMC );

            RISCRead_VLD_TOP( 40, &risc_val2);
            if ( maxVLD < risc_val2 ){
                maxVLD = risc_val2;
            }
            printk("max VLD_TOP[40] = 0x%08X \n", maxVLD );
            sprintf( risc_temp, "%d %u %u\n", frame_num ,risc_val1 ,risc_val2  );
            ret = filp_info->f_op->write( filp_info, risc_temp, strlen(risc_temp), &filp_info->f_pos );


            if ( error_rate > 0 ){
                printk("\n======== Frame %d decode done ========\n", frame_num );
 
                char* EC_output_Y;
                char* EC_output_C;

                EC_output_Y = ioremap_nocache( PP_OUT_Y_ADDR, PIC_SIZE_Y );
                EC_output_C = ioremap_nocache( PP_OUT_C_ADDR, PIC_SIZE_Y/2 );

                if (isDecodeTimeOut){
                //if (frame_num>=560){
                    Dump_mem( EC_output_Y, PIC_SIZE_Y, frame_num , 1, isDecodeTimeOut ); 
                    Dump_mem( EC_output_C, PIC_SIZE_Y/2, frame_num , 2, isDecodeTimeOut ); 
                    if (UFO_MODE){                      
                        char* EC_output_Y_len;
                        char* EC_output_C_len;
                        EC_output_Y_len = ioremap_nocache( Dpb_ufo_Y_len_addr[current_dpb_idx], PIC_SIZE_Y );
                        EC_output_C_len = ioremap_nocache( Dpb_ufo_C_len_addr[current_dpb_idx], PIC_SIZE_Y/2 );
                        Dump_mem( EC_output_Y_len, UFO_LEN_SIZE_Y, frame_num , 4, isDecodeTimeOut ); 
                        Dump_mem( EC_output_C_len, UFO_LEN_SIZE_C, frame_num , 5, isDecodeTimeOut ); 
                        iounmap(EC_output_Y_len);
                        iounmap(EC_output_C_len);
                    }
/*
                     //Dump Dram              
                        char* Dram_dump;
                        Dram_dump = ioremap_nocache( 0x90000000, 0x10000000 );
                    
                        sprintf( file_name, "/mnt/sdcard/Dram_dump_0x9_0xA_f%d_er%d", frame_num, error_rate);
                        fd_bitstream = openFile( file_name, O_CREAT|O_RDWR, 0 );
                        ret = fd_bitstream->f_op->write( fd_bitstream, Dram_dump, 0x10000000, &fd_bitstream->f_pos );        
                        closeFile( fd_bitstream );
                    
                        iounmap(Dram_dump);
 */                     
                }

                //Dump_yuv( EC_output_Y, EC_output_C, PIC_SIZE_Y);
                
                iounmap(EC_output_Y);
                iounmap(EC_output_C);
                if ( debug_mode == 4 )
                    Check_dpb( Dpb_addr, mv_buf_addr, frame_num, MV_COL_PIC_SIZE, PIC_SIZE, PIC_SIZE_Y );

                isFail = 0;
                
            } else {
                char* Ptr_output_Y;
                char* Ptr_output_C;
                char* Ptr_Y_LEN;
                char* Ptr_C_LEN;

                Ptr_output_Y = ioremap_nocache( PP_OUT_Y_ADDR, PIC_SIZE_Y );
                Ptr_output_C = ioremap_nocache( PP_OUT_C_ADDR, PIC_SIZE_Y/2 );
                if (UFO_MODE){
                    Ptr_Y_LEN = ioremap_nocache( UFO_Y_LEN_ADDR, UFO_LEN_SIZE_Y );
                    Ptr_C_LEN = ioremap_nocache( UFO_C_LEN_ADDR, UFO_LEN_SIZE_C );
                }

                /*
                isFail = Golden_comparison( frame_num, PIC_SIZE_Y, Ptr_output_Y, Ptr_output_C, MV_COL_PIC_SIZE, 0, 
                                                           Ptr_Y_LEN, Ptr_C_LEN, UFO_LEN_SIZE_Y, UFO_LEN_SIZE_C);
                */
                isFail = 1;
                if ( isFail ){
                    printk("\nOutput buffer zero padding:\n");
                    Margin_padding(Ptr_output_Y, Ptr_output_C, PIC_SIZE_Y );

                    isFail = Golden_comparison( frame_num, PIC_SIZE_Y, Ptr_output_Y, Ptr_output_C, MV_COL_PIC_SIZE, 1, 
                                                               Ptr_Y_LEN, Ptr_C_LEN, UFO_LEN_SIZE_Y, UFO_LEN_SIZE_C);
                    
                }
                //Dump_yuv( Ptr_output_Y, Ptr_output_C, PIC_SIZE_Y);
               
                iounmap(Ptr_output_Y);
                iounmap(Ptr_output_C);
                if (UFO_MODE){
                    iounmap(Ptr_Y_LEN);
                    iounmap(Ptr_C_LEN);
                }
               
            }
            
            if ( abs(debug_mode) ==2 ){       // single frame repeat test
                repeat_count++;
                frame_num--;
                printk("Repeat count = %d\n", repeat_count);
                if (repeat_count >=  frame_end -frame_start )
                    break;
            }

            if (isDecodeTimeOut ){
                break;
            }
            if (isFail ){
                //Check_dpb( Dpb_addr, mv_buf_addr, frame_num, MV_COL_PIC_SIZE, PIC_SIZE, PIC_SIZE_Y );
                //Dpb_empty = 1;  //reload reference frame
                break;
            }else{
                Dpb_empty = 0;
            }

            if ( error_rate > 0){
                if ( 16 <= ucNalUnitType &&  23 >= ucNalUnitType ){

                    char *IDR_ptr_base;
                    // read Golden for EC
                    IDR_ptr_base = ioremap_nocache( Dpb_addr[current_dpb_idx] , PIC_SIZE );
                    sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_Y.dat", bitstream_name, frame_num );
                    
                    fd = openFile( file_name, O_RDONLY,0); 
                    if (IS_ERR(fd) ){
                        printk("[Error] Miss file: frameY_%d!!!!!!!!!!!!!\n",frame_num);
                    } else {             
                        readFile(fd ,IDR_ptr_base, PIC_SIZE_Y );
                        closeFile(fd); 
                    }
                    sprintf(file_name, "/mnt/sdcard/%s_pat/sao_pat/frame_%d_C.dat", bitstream_name, frame_num );
                    
                    fd = openFile( file_name, O_RDONLY,0); 
                    if (IS_ERR(fd) ){
                        printk("[Error] Miss file: frameC_%d!!!!!!!!!!!!!\n",frame_num);
                    } else {
                        readFile(fd , IDR_ptr_base+PIC_SIZE_Y, PIC_SIZE -PIC_SIZE_Y );
                        closeFile(fd); 
                    }
                    iounmap(IDR_ptr_base);

                }
            }
             


        } 

        closeFile(filp_info);
        printk( "\n--%s test end #%d --\n\n", bitstream_name, frame_num );

        iounmap(ptr_base);
        set_fs( oldfs );

    }
    
}


void RISC_instructions(){
    int i4temp;



}
