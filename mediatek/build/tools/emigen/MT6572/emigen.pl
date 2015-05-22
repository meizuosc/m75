#!/usr/local/bin/perl

#****************************************************************************
# Included Modules
#****************************************************************************


#   original design, but perl does not support array of structure, really?
#
#my $CustomChip = () ;
#
#
#       an array of following structure:
#
#       CustChip => NAND_ID
#                => CS0_PART_NUMBER
#                => CS1_PART_NUMBER
#
#
#
#
#
#




my $os = &OsName();
my $start_num;
if ($os eq "windows")
{
    use strict;
    &gen_pm;
    require 'ForWindows.pm';
    $Win32::OLE::Warn = 3; 
    $start_num = 1;
}
elsif ($os eq "linux")
{
    print "Os = linux\n";
#   push(@INC, '/usr/local/lib/perl5/site_perl/5.8.8/');
    push(@INC, 'mediatek/build/tools/Spreadsheet');
    push(@INC, 'mediatek/build/tools');
#   push(@INC, '.');
#   push(@INC, './Spreadsheet');
    require 'ParseExcel.pm';
    $start_num = 0; 
}
else
{
  die "unknow OS!\n";
}
use lib 'mediatek/build/tools';
use pack_dep_gen;
#****************************************************************************
# PLATFORM EMI support matrix
#****************************************************************************
my %BBtbl_LPSDRAM = 
(       
	'MT6589'  => 1,
	'MT6572'  => 1,
);

#****************************************************************************
# Constants
#****************************************************************************
my $EMIGEN_VERNO  = " V0.01";
                    # V0.01, Zhen Jiang, Porting emigen to DUMA project
                    #
my $DebugPrint    = 1; # 1 for debug; 0 for non-debug

my $COLUMN_VENDOR               = $start_num + 0;
my $COLUMN_PART_NUMBER	        = $COLUMN_VENDOR + 1 ;
my $COLUMN_TYPE	                = $COLUMN_PART_NUMBER + 1 ;
my $COLUMN_DENSITY	            = $COLUMN_TYPE + 1 ;
my $COLUMN_BOARD_ID	            = $COLUMN_DENSITY + 1 ;
my $COLUMN_NAND_EMMC_ID	        = $COLUMN_BOARD_ID + 1 ;
my $COLUMN_FW_ID	        = $COLUMN_NAND_EMMC_ID + 1 ;
my $COLUMN_NAND_PAGE_SIZE       = $COLUMN_FW_ID + 1 ;
my $COLUMN_PLATFORM             = $COLUMN_NAND_PAGE_SIZE + 1 ;

my $CUSTOM_MEMORY_DEVICE_HDR  = $ARGV[0]; # src\custom\<project>, need full path for now
#my $MEMORY_DEVICE_LIST_XLS    = Win32::GetCwd()."\\memorydevicelist\\".$ARGV[1];
my $MEMORY_DEVICE_LIST_XLS    = $ARGV[1];
my $PLATFORM                  = $ARGV[2]; # MTxxxx
my $PROJECT               = $ARGV[3];
my $MTK_EMIGEN_OUT_DIR = "$ENV{MTK_ROOT_OUT}/EMIGEN";

print "$CUSTOM_MEMORY_DEVICE_HDR\n$MEMORY_DEVICE_LIST_XLS\n$PLATFORM\n" if ($DebugPrint == 1);

# following parameters come from $CUSTOM_MEMORY_DEVICE_HDR
my $MEMORY_DEVICE_TYPE;

# data structure of $part_number if ($MEMORY_DEVICE_TYPE eq 'LPSDRAM')
#
# my $part_info =
# {
#    CS       => { "0" => { PART_NUMBER     => $part_number,
#                           EXCEL_ROW       => $excel_row,
#                           VENDOR          => $vendor,
my $part_info     = ();   # has different data structures for different $MEMORY_DEVICE_TYPE

my $bank_num = 0;         #  0: No memory is attached        
                          #  1: 1 is attached        
                          #  2: 2 are attached      
                  
# locations of output EMI settings
# src\custom\<project>\DRV\bootloader\EMI
my $CUSTOM_EMI_H = $CUSTOM_MEMORY_DEVICE_HDR;
my $CUSTOM_EMI_C = $CUSTOM_MEMORY_DEVICE_HDR;
my $INFO_TAG = $CUSTOM_MEMORY_DEVICE_HDR;

if ($os eq "windows")
{

    $CUSTOM_EMI_H = "$MTK_EMIGEN_OUT_DIR/inc/custom_emi.h";
	$CUSTOM_EMI_C = "$ENV{MTK_ROOT_OUT}/PRELOADER_OBJ/custom_emi.c";
	`mkdir output` unless (-d "output");
}
elsif ($os eq "linux")
{
	$CUSTOM_EMI_H = "$MTK_EMIGEN_OUT_DIR/inc/custom_emi.h";
	$CUSTOM_EMI_C = "$ENV{MTK_ROOT_OUT}/PRELOADER_OBJ/custom_emi.c";
	$INFO_TAG     = "$MTK_EMIGEN_OUT_DIR/MTK_Loader_Info.tag";
}
PrintDependModule($0);
print "$CUSTOM_EMI_H\n$CUSTOM_EMI_C\n$INFO_TAG\n" if ($DebugPrint ==1);

# check existance of custom_EMI.h and custom_EMI.c
&dependency_check($CUSTOM_EMI_C, "FALSE", "tools/emigen.pl",   $CUSTOM_MEMORY_DEVICE_HDR, $MEMORY_DEVICE_LIST_XLS);
&dependency_check($CUSTOM_EMI_H, "FALSE", "tools/emigen.pl",   $CUSTOM_MEMORY_DEVICE_HDR, $MEMORY_DEVICE_LIST_XLS);
# check existance of custom_EMI.h and custom_EMI.c
my $is_existed_h             = (-e $CUSTOM_EMI_H)?           1 : 0;
my $is_existed_c             = (-e $CUSTOM_EMI_C)?           1 : 0;
#
if ( $is_existed_c == 1 && $is_existed_h == 1)
{
   print "\n\ncustom_EMI\.c are existed!!!\n\n\n";
   #exit;
}
my $LPSDRAM_CHIP_SELECT = 0;
my $MAX_COMBO_MEM_ENTRY_COUNT = 0xFF;
my @MCP_LIST;  # list of PART_NUMBER hash references
my @MDL_INFO_LIST; 
my %CUSTOM_MEM_DEV_OPTIONS;
my $IDX_COUNT = 3;
my $EMI_CLK;

#****************************************************************************
# parse custom_MemoryDevice.h to extract MEMORY_DEVICE_TYPE & PART_NUMBER
#****************************************************************************
#open CUSTOM_MEMORY_DEVICE_HDR, "<$CUSTOM_MEMORY_DEVICE_HDR" or &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: file error!", __FILE__, __LINE__);


&Parse_Memory_Device(\$LPSDRAM_CHIP_SELECT,\%CUSTOM_MEM_DEV_OPTIONS, \@MCP_LIST);

 
#****************************************************************************
# read a excel file to get emi settings
#****************************************************************************
# get already active Excel application or open new
&Parse_MDL(\%CUSTOM_MEM_DEV_OPTIONS,\@MDL_INFO_LIST, \$MEMORY_DEVICE_LIST_XLS);

#****************************************************************************
# check emi setting valid
#****************************************************************************
&check_EMI_setting(\%CUSTOM_MEM_DEV_OPTIONS,\@MDL_INFO_LIST,\@MCP_LIST);


#****************************************************************************
# generate custom_EMI.c
#****************************************************************************
#if ($is_existed_c == 0)
{
    if ($is_existed_c == 1)
    {
	unlink ($CUSTOM_EMI_C);
    }
    my $temp_path = `dirname $CUSTOM_EMI_C`;
    `mkdir -p $temp_path`;
    open (CUSTOM_EMI_C, ">$CUSTOM_EMI_C") or &error_handler("$CUSTOM_EMI_C: file error!", __FILE__, __LINE__);

    print CUSTOM_EMI_C &copyright_file_header();
    print CUSTOM_EMI_C &description_file_header(                      "custom_emi.c",
          "This Module defines the EMI (external memory interface) related setting.",
                                                 "EMI auto generator". $EMIGEN_VERNO);
    print CUSTOM_EMI_C &custom_EMI_c_file_body(\%CUSTOM_MEM_DEV_OPTIONS,\@MDL_INFO_LIST,\@MCP_LIST);
    close CUSTOM_EMI_C or &error_handler("$CUSTOM_EMI_C: file error!", __FILE__, __LINE__);

    print "\n$CUSTOM_EMI_C is generated\n";
} 

#****************************************************************************
# generate custom_emi.h
#****************************************************************************
#if ($is_existed_h == 0)
{
    if ($is_existed_h == 1)
    {
        unlink ($CUSTOM_EMI_H);
    }
    my $temp_path = `dirname $CUSTOM_EMI_H`;
    `mkdir -p $temp_path`;	
	
    open (CUSTOM_EMI_H, ">$CUSTOM_EMI_H") or &error_handler("CUSTOM_EMI_H: file error!", __FILE__, __LINE__);

   print CUSTOM_EMI_H &copyright_file_header();
    print CUSTOM_EMI_H &description_file_header(                      "custom_emi.h",
          "This Module defines the EMI (external memory interface) related setting.",
                                                 "EMI auto generator". $EMIGEN_VERNO);
    print CUSTOM_EMI_H &custom_EMI_h_file_body(\%CUSTOM_MEM_DEV_OPTIONS);
    close CUSTOM_EMI_H or &error_handler("$CUSTOM_EMI_H: file error!", __FILE__, __LINE__);

    print "\n$CUSTOM_EMI_H is generated\n";
  
} 


&write_tag(\%CUSTOM_MEM_DEV_OPTIONS,\@MDL_INFO_LIST,\@MCP_LIST, $PROJECT);
exit;

#****************************************************************************
# subroutine:  error_handler
# input:       $error_msg:     error message
#****************************************************************************
sub error_handler
{
	   my ($error_msg, $file, $line_no) = @_;
	   
	   my $final_error_msg = "[Error]EMIGEN ERROR: $error_msg at $file line $line_no\n";
	   print $final_error_msg;
	   die $final_error_msg;
}

#****************************************************************************
# subroutine:  copyright_file_header
# return:      file header -- copyright
#****************************************************************************
sub copyright_file_header
{
    my $template = <<"__TEMPLATE";
__TEMPLATE

   return $template;
}

#****************************************************************************
# subroutine:  description_file_header
# return:      file header -- description 
# input:       $filename:     filename
# input:       $description:  one line description
# input:       $author:       optional
#****************************************************************************
sub description_file_header
{
    my ($filename, $description, $author) = @_;
    my @stat_ar = stat $MEMORY_DEVICE_LIST_XLS;
    my ($day, $month, $year) = (localtime($stat_ar[9]))[3,4,5]; $month++; $year+=1900;
    my $template = <<"__TEMPLATE";
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   $filename
 *
 * Project:
 * --------
 *   Android
 *
 * Description:
 * ------------
 *   $description
 *
 * Author:
 * -------
 *  $author
 *
 *   Memory Device database last modified on $year/$month/$day
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * \$Revision\$
 * \$Modtime\$
 * \$Log\$
 *
 *------------------------------------------------------------------------------
 * WARNING!!!  WARNING!!!   WARNING!!!  WARNING!!!  WARNING!!!  WARNING!!! 
 * This file is generated by EMI Auto-gen Tool.
 * Please do not modify the content directly!
 * It could be overwritten!
 *============================================================================
 * Upper this line, this part is controlled by PVCS VM. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

__TEMPLATE

   return $template;
}

#****************************************************************************
# subroutine:  HeaderBody_for_lpsdram
# return:      content for custom_EMI.h 
#****************************************************************************
sub custom_EMI_h_file_body
{
	  my ($CUSTOM_MEM_DEV_OPTIONS_LOCAL) = @_; 
    ###
    my $template = <<"__TEMPLATE";
    
#ifndef __CUSTOM_EMI__
#define __CUSTOM_EMI__



#endif /* __CUSTOM_EMI__ */

__TEMPLATE

    return $template;
}
#****************************************************************************
# subroutine: check_EMI_setting
# return:
#****************************************************************************
sub check_EMI_setting
{
    my ($CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL, $MCP_LIST_LOCAL) = @_;
    my $mem_type,$flash_id,$dram_vendor_id,$emi_size;
    my $mem_type_cmp,$flash_id_cmp,$dram_vendor_id_cmp,$emi_size_cmp;
    my $idx = 0;
    for (1..$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT})
    {
        $mem_type = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Type'};
        $flash_id = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'};
        $emi_size = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Density (Mb)'};
        $dram_vendor_id = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'};
        if (!defined $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'})
        {
            print "no DRAM Vendor ID column";
            $dram_vendor_id = '0x0'
        }


#        print "1.count:$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}";
#        print "1.mem type :$mem_type\n";
#        print "1.vendor ID :$dram_vendor_id\n";
#        print "1.emi_size :$emi_size\n";
#        print "1.flash_id :$flash_id\n";
        for ($idx = ($_+1); $idx <=$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}; $idx++)
        {
            $mem_type_cmp = $MDL_INFO_LIST_LOCAL->[$idx]->{$LPSDRAM_CHIP_SELECT}->{'Type'};
            $flash_id_cmp = $MDL_INFO_LIST_LOCAL->[$idx]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'};
            $emi_size_cmp = $MDL_INFO_LIST_LOCAL->[$idx]->{$LPSDRAM_CHIP_SELECT}->{'Density (Mb)'};
            $dram_vendor_id_cmp = $MDL_INFO_LIST_LOCAL->[$idx]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'};
            if (!defined $MDL_INFO_LIST_LOCAL->[$idx]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'})
            {
                print "no DRAM Vendor ID column";
                $dram_vendor_id_cmp = '0x0'
            }
#            print "2.idx:$idx,count:$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}";
#            print "2.mem type :$mem_type_cmp\n";
#            print "2.vendor ID :$dram_vendor_id_cmp\n";
#            print "2.emi_size :$emi_size_cmp\n";
#            print "2.flash_id :$flash_id_cmp\n";
        # Limit 1: support only 1 discrete LPDDR1
            if ($mem_type eq "Discrete DDR1" && $mem_type_cmp eq "Discrete DDR1")
            {
                die "[Error]At most one discrete LPDDR1 is allowed in the Combo MCP list\n" ;
            }
        # Limit 2: support only 1 discrete PCDDR3
            if ($mem_type eq "Discrete PCDDR3" && $mem_type_cmp eq "Discrete PCDDR3")
            {
                die "[Error]At most one discrete PCDDR3 is allowed in the Combo MCP list\n" ;
            }
        # Limit 3: the vendor ID or dram size of discrete LPDDR2/LPDDR3 should be different.
            if ($mem_type eq "Discrete DDR2" || $mem_type eq "Discrete LPDDR3")
            {
               if (  ($mem_type eq $mem_type_cmp)
                  && ($dram_vendor_id eq $dram_vendor_id_cmp)
                  && ($emi_size eq $emi_size_cmp))
               {
                    die "[Error] Combo discrete DRAM cannot support two part number which the vendor ID and DRAM size are the same\n" ;
               }
            }
        # Limit 4: the flash ID or the vendor ID or dram size of MCP LPDDR2/LPDDR3 should be different.
            if (   $mem_type eq "MCP(NAND+DDR2)"
                || $mem_type eq "MCP(NAND+LPDDR3)"
                || $mem_type eq "MCP(eMMC+DDR2)"
                || $mem_type eq "MCP(eMMC+LPDDR3)")
            {
               if (   ($mem_type eq $mem_type_cmp)
                   && ($dram_vendor_id eq $dram_vendor_id_cmp)
                   && ($emi_size eq $emi_size_cmp)
                   && ($flash_id eq $flash_id_cmp))
               {
                    die "[Error] Combo MCP DRAM(LPDDR2,LPDDR3) cannot support two part number which the flash ID and the vendor ID and DRAM size are the same\n" ;
               }
            }
        # Limit 5: support more than 2 MCP LPDDR1 which the flash ID should be different
        # Limit 6: support more than 2 MCP PCDDR3 which the flash ID should be different
            if (   $mem_type eq "MCP(NAND+DDR1)"
                || $mem_type eq "MCP(NAND+PCDDR3)"
                || $mem_type eq "MCP(eMMC+DDR1)"
                || $mem_type eq "MCP(eMMC+PCDDR3)")
            {

               if (   ($mem_type eq $mem_type_cmp)
                   && ($flash_id eq $flash_id_cmp))
               {
                    die "[Error] Combo MCP DRAM(LPDDR1, PCDDR3) cannot support two part number which the flash ID are the same\n" ;
               }
            }
        }
    }


    return ;
}

#****************************************************************************
# subroutine:  custom_EMI_c_file_body
# return:      
#****************************************************************************
sub custom_EMI_c_file_body
{
   my ($CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL, $MCP_LIST_LOCAL) = @_;
   my $idx = 0;
   my $mem_type_ori, $flash_id_ori, ,$mem_type, $flash_id,$dram_vendor_id,$flash_length, $emi_clk, $emi_drva, $emi_drvb, $emi_odla, $emi_odlb, $emi_odlc, $emi_odld;
   my $emi_odle, $emi_odlf, $emi_odlg, $emi_odlh, $emi_odli, $emi_odlj, $emi_odlk, $emi_odll, $emi_odlm, $emi_odln;
   my $emi_coni, $emi_conj, $emi_conk, $emi_conl, $emi_conn, $emi_duta, $emi_dutb, $emi_dutc, $emi_duca, $emi_ducb, $emi_duce;
   my $emi_iocl, $emi_gend, $emi_size, $emi_size_str, $emi_reserved;
   my $combo_memory_struct_setting;
   if($CUSTOM_MEM_DEV_OPTIONS_LOCAL->{EMI_CLK} =~ /\d{3}/)
   {
      	$emi_clk = $&; 
      	print "custom_emi.c : $PROJECT, $LPSDRAM_CHIP_SELECT, $emi_clk\n";
   }
   
   for (1..$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT})
   {
       $emi_clk = $EMI_CLK[$_-1];
       print "EMI_CLK[$_-1]= $EMI_CLK[$_-1]";
   	    $flash_id = "";
  	    my $comma = ($_ < $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}) ? "," : "";
        $mem_type_ori = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Type'};
        # print "mem type:$mem_type_ori\n";
        $mem_type = sprintf("0x%X", &get_memory_type($mem_type_ori));
        #print "mem type:$mem_type\n";
        $flash_id_ori = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'};
        # print "flash id:$flash_id_ori, $flash_length\n";
        $flash_id = &parse_flash_id($flash_id_ori, \$flash_length);
         print "flash length: $flash_length, $flash_id\n";
        #print "emi setting : $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'}\n";
        $emi_drva = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DRV_A_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_drvb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DRV_B_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odla = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_A_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_B_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlc = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_C_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odld = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_D_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odle = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_E_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlf = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_F_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlg = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_G_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlh = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_H_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odli = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_I_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlj = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_J_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlk = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_K_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odll = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_L_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlm = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_M_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odln = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_N_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_coni = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONI_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conj = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONJ_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conk = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONK_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conl = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONL_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conn = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONN_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duta = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTA_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_dutb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTB_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_dutc = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTC_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duca = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCA_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_ducb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCB_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duce = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCE_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_iocl = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_IOCL_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_gend = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_GEND_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        print "emi setting $emi_drva, $emi_drvb\n";
        $emi_size_str = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Density (Mb)'};
        #print "mem size:$emi_size_str\n";
        #print "mem size:$emi_size\n";
        $emi_size = &memory_size_transfer($emi_size_str);
        $dram_vendor_id = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'};
        if (!defined $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'})
        {
            print "no DRAM Vendor ID column";
            $dram_vendor_id = '0x0'
        }elsif($dram_vendor_id eq '0x0' || $dram_vendor_id eq '')
        {
            $dram_vendor_id = "0xf0";
        }

        print "dram vendor ID:$dram_vendor_id\n";
        $emi_reserved = "0";
        
        ###
        $combo_memory_struct_setting .= <<"__TEMPLATE1";
        {
        	   $mem_type,  /* TYPE  eMMC + LPDDR2 */
        	   $flash_id,  /* NAND_EMMC_ID */
        	   $flash_length,  /* EMMC and NAND ID/FW ID checking length */
        	   $emi_clk,   /*EMI freq */
        	   $emi_drva,  /* EMI_DRVA_value */
        	   $emi_drvb,  /* EMI_DRVB_value */
        	   $emi_odla,  /* EMI_ODLA_value */
        	   $emi_odlb,  /* EMI_ODLB_value */
        	   $emi_odlc,  /* EMI_ODLC_value */
        	   $emi_odld,  /* EMI_ODLD_value */
        	   $emi_odle,  /* EMI_ODLE_value */
        	   $emi_odlf,  /* EMI_ODLF_value */
        	   $emi_odlg,  /* EMI_ODLG_value */
        	   $emi_odlh,  /* EMI_ODLH_value */
        	   $emi_odli,  /* EMI_ODLI_value */
        	   $emi_odlj,  /* EMI_ODLJ_value */
        	   $emi_odlk,  /* EMI_ODLK_value */
        	   $emi_odll,  /* EMI_ODLL_value */
        	   $emi_odlm,  /* EMI_ODLM_value */
        	   $emi_odln,  /* EMI_ODLN_value */
        	   
        	   $emi_coni,  /* EMI_CONI_value */  /* set DRAM driving */
        	   $emi_conj,  /* EMI_CONJ_value */
        	   $emi_conk,  /* EMI_CONK_value */
        	   $emi_conl,  /* EMI_CONL_value */
        	   $emi_conn,  /* EMI_CONN_value */  /* remember set bank number */
        	   
        	   $emi_duta,  /* EMI_DUTA_value */
        	   $emi_dutb,  /* EMI_DUTB_value */
        	   $emi_dutc,  /* EMI_DUTC_value */
        	   
        	   $emi_duca,  /* EMI_DUCA_value*/
        	   $emi_ducb,  /* EMI_DUCB_value*/
        	   $emi_duce,  /* EMI_DUCE_value*/
        	   
        	   $emi_iocl,  /* EMI_IOCL_value */
        	   
        	   $emi_gend,  /* EMI_GEND_value */
        	   
        	   $emi_size,  /* DRAM RANK SIZE  4Gb/rank */
        	   $dram_vendor_id,  /* DRAM vendor ID*/
                   0,  /* match flag*/

        }$comma
__TEMPLATE1
   
   }         
   my $template = <<"__TEMPLATE";
#include "mt_emi.h"
             
#define NUM_EMI_RECORD $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}
             
int num_of_emi_records = NUM_EMI_RECORD ;
             
EMI_SETTINGS emi_settings[] =
{            
  $combo_memory_struct_setting
}; 
__TEMPLATE

    return $template ;
}


#****************************************************************************************
# subroutine:  OsName
# return:      which os this script is running
# input:       no input
#****************************************************************************************
sub OsName {
  my $os = `set os`;
  if(!defined $os) { 
    $os = "linux";
  } 
  else {
    die "[Error]does not support windows now!!" ;
    $os = "windows";
  }
}
#*************************************************************************************************
# subroutine:  gen_pm
# return:      no return, but will generate a ForWindows.pm in "/perl/lib" where your perl install
#*************************************************************************************************
sub gen_pm {
  foreach (@INC) {
    if(/^.*:\/Perl\/lib$/) {
      open FILE, ">${_}\/ForWindows.pm";
      print FILE "package ForWindows;\n";
      print FILE "use Win32::OLE qw(in with);\n";
      print FILE "use Win32::OLE::Const 'Microsoft Excel';\n";
      print FILE "\$Win32::OLE::Warn = 3;\n";
      print FILE "1;";
      close(FILE);
      last;
    }
  }
}
#****************************************************************************************
# subroutine:  get_sheet
# return:      Excel worksheet no matter it's in merge area or not, and in windows or not
# input:       Specified Excel Sheetname
#****************************************************************************************
sub get_sheet {
  my $MEMORY_DEVICE_TYPE = $_[0];

  print $MEMORY_DEVICE_TYPE ;

  if ($os eq "windows") {
    return $Sheet     = $Book->Worksheets($MEMORY_DEVICE_TYPE);
  }
  else {
    return $Sheet     = $Book->Worksheet($MEMORY_DEVICE_TYPE);
  }
}


#****************************************************************************************
# subroutine:  xls_cell_value
# return:      Excel cell value no matter it's in merge area or not, and in windows or not
# input:       $Sheet:  Specified Excel Sheet
# input:       $row:    Specified row number
# input:       $col:    Specified column number
#****************************************************************************************
sub xls_cell_value {
   my ($sheet, $row, $col) = @_;

    if($^O eq "MSWin32")
    {    
    if ($sheet->Cells($row, $col)->{'MergeCells'})
    {
        my $ma = $sheet->Cells($row, $col)->{'MergeArea'};
        return ($ma->Cells(1, 1)->{'Value'});
    }
    else
    {
        return ($sheet->Cells($row, $col)->{'Value'})
    }
    }
    else
    {
        my $cell = $sheet->get_cell($row, $col);
        my $mareas =$sheet->get_merged_areas();
        if(($row == 0))
        {
        	   my $value;
        	   for(0..$#$mareas)
        	   {
                       if($col >=8 && $col <= 94)
                       {
                           $value = $sheet->get_cell(0,8)->value();
                           #print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
                           return $value;
                       }
                       elsif($col >= $mareas->[$_]->[1] && $col <= $mareas->[$_]->[3])
                       {
                           $value = $sheet->get_cell(0,$mareas->[$_]->[1])->value();
                           #print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
                           return $value;
                       }	
        	   }
        	
        }
        elsif($row == 1)
        {
        	 my $value;
        	   for(0..$#$mareas)
        	   {
        	   	  if($col >= 8 && $col <= 36)
        	   	  {
        	   	  	   	$value = $sheet->get_cell(1,8)->value();
        	           # print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
        	            return $value;
        	   	  }
        	   	  elsif($col >= 37 && $col <= 65)
        	   	  {
        	   	  	$value = $sheet->get_cell(1,37)->value();
        	   	  	#print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
        	   	  	return $value;
        	   	  }
        	   	  elsif($col >= 66 && $col <= 94)
        	   	  {
        	   	  	$value = $sheet->get_cell(1,66)->value();
        	   	  	#print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
        	   	  	return $value;
        	   	  }
        	   	  elsif(($col >= 0 && $col <= 7) || $col == 95)
        	   	  {
                                # no.95->for DRAM vendor ID title parsing
                                if (defined $sheet->get_cell(0,$col))
                                {
                                    $value = $sheet->get_cell(0,$col)->value();
                                }
                                else 
                                {
                                    $value = 0; 
                                }
                                #print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
        	   	  	return $value;
        	   	  }
                          elsif($col >= $mareas->[$_]->[1] && $col <= $mareas->[$_]->[3])
                          {
                              $value = $sheet->get_cell(1,$mareas->[$_]->[1])->value();
                              #print "col : $col, $mareas->[$_]->[1], $mareas->[$_]->[3], $value\n";
                              return $value;
                          }	
        	   }
        }
        if($cell){
        	my $value = $cell->value();
        	return $value;
        }
        else
        {
        	#print "cell is undef\n";
        	my $value;
        	return $value;
        }
	
    }
}
sub win_xls_cell_value
{
    my ($Sheet, $row, $col) = @_;

    if ($Sheet->Cells($row, $col)->{'MergeCells'})
    {
        my $ma = $Sheet->Cells($row, $col)->{'MergeArea'};
        return ($ma->Cells(1, 1)->{'Value'});
    }
    else
    {
        return ($Sheet->Cells($row, $col)->{'Value'})
    }
}
sub lin_xls_cell_value
{
  my ($Sheet, $row, $col) = @_;
  my $cell = $Sheet->get_cell($row, $col);
  return "" unless (defined $cell);
  my $value = $cell->Value();

}

sub write_tag()
{
	  my ($CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL, $MCP_LIST_LOCAL, $PROJECT_LOCAL) = @_;
    my $project = lc($PROJECT_LOCAL);
    my $filesize = 0x0 ;
    my $ddr = -1 ;

    if (-e $INFO_TAG)
    {
        unlink ($INFO_TAG);
    }
     my $temp_path = `dirname $INFO_TAG`;
    `mkdir -p $temp_path`;
    open FILE,">$INFO_TAG";
    print FILE pack("a24", "MTK_BLOADER_INFO_v11");
    $filesize = $filesize + 24 ;
    seek(FILE, 0x1b, 0);
    $pre_bin = "preloader_${project}.bin";
    print "PROJECT = $project, pre_bin = $pre_bin\n";
    print FILE pack("a64", $pre_bin); 
    $filesize = $filesize + 64 ;
    seek(FILE, 0x58, 0);
    print FILE pack("H8", 56313136);
    $filesize = $filesize + 4 ;
    print FILE pack("H8", 22884433);
    $filesize = $filesize + 4 ;
    print FILE pack("H8", "90007000");
    $filesize = $filesize + 4 ;
    print FILE pack("a8", "MTK_BIN");
    $filesize = $filesize + 8 ;
    
#    print FILE pack("H8", bc000000);
    
 
    seek(FILE,0x6c, 0);
    
    print FILE pack("L", hex($CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}));     # number of emi settings.
    $filesize = $filesize + 4 ;
    &write_tag_one_element (\$filesize, $CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL) ;
    
    #if ($ddr != -1)
    #{
    #    $filesize = $filesize + &write_tag_one_element ($ddr) ;
    #}
    
    print FILE pack("L", $filesize) ;
    
    close (FILE) ;
    print "$INFO_TAG is generated!\n";
    return ;
  
}

sub write_tag_one_element()
{
  my ($file_size_local, $CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL) = @_;
   my $idx = 0;
   my $mem_type_ori, $flash_id_ori, ,$mem_type, $dram_vendor_id,$flash_id,$flash_length, $emi_clk, $emi_drva, $emi_drvb, $emi_odla, $emi_odlb, $emi_odlc, $emi_odld;
   my $emi_odle, $emi_odlf, $emi_odlg, $emi_odlh, $emi_odli, $emi_odlj, $emi_odlk, $emi_odll, $emi_odlm, $emi_odln;
   my $emi_coni, $emi_conj, $emi_conk, $emi_conl, $emi_conn, $emi_duta, $emi_dutb, $emi_dutc, $emi_duca, $emi_ducb, $emi_duce;
   my $emi_iocl, $emi_gend, $emi_size, $emi_size_str, $emi_reserved, $emi_clk_str;
    if($CUSTOM_MEM_DEV_OPTIONS_LOCAL->{EMI_CLK} =~ /\d{3}/)
   {
      	$emi_clk = $&; 
      	$emi_clk_str = sprintf("0x%X", $emi_clk);
      	#print "write tag : $PROJECT, $LPSDRAM_CHIP_SELECT, $emi_clk, $emi_clk_str\n";
   }
   
  for (1..$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT})
   {
       $emi_clk = $EMI_CLK[$_-1];
       $emi_clk_str = sprintf("0x%X", $emi_clk);
       print "write tag : $PROJECT, $LPSDRAM_CHIP_SELECT, $emi_clk, $emi_clk_str\n";
       print "EMI_CLK[$_-1]= $EMI_CLK[$_-1]";
   	    #$flash_id = "";
   	    my $fs = 0;
  	    my $comma = ($_ < $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}) ? "," : "";
        $mem_type_ori = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Type'};
        # print "mem type:$mem_type_ori\n";
        $mem_type = sprintf("0x%X", &get_memory_type($mem_type_ori));
        #print "mem type:$mem_type\n";
        $flash_id_ori = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'};
        # print "flash id:$flash_id_ori, $flash_length\n";
        $flash_id = &parse_flash_id_tag($flash_id_ori, \$flash_length);
        # print "flash length tag: $flash_length, $flash_id\n";
        #print "emi setting : $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'NAND/eMMC ID'}\n";
        $emi_drva = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DRV_A_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_drvb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DRV_B_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odla = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_A_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_B_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlc = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_C_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odld = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_D_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odle = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_E_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlf = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_F_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlg = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_G_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlh = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_H_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odli = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_I_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlj = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_J_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlk = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_K_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odll = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_L_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odlm = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_M_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_odln = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_ODL_N_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_coni = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONI_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conj = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONJ_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conk = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONK_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conl = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONL_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_conn = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_CONN_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duta = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTA_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_dutb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTB_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_dutc = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUTC_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duca = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCA_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_ducb = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCB_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_duce = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_DUCE_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_iocl = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_IOCL_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
        $emi_gend = &Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG($_, $emi_clk, $PLATFORM, 'EMI_GEND_VAL', $LPSDRAM_CHIP_SELECT, $CUSTOM_MEM_DEV_OPTIONS_LOCAL);
       # print "emi setting tag $emi_drva, $emi_drvb\n";
        $dram_vendor_id = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'};
        if (!defined $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'DRAM Vendor ID'})
        {
            print "no DRAM Vendor ID column";
            $dram_vendor_id = '0x0';
        }elsif($dram_vendor_id eq '0x0' || $dram_vendor_id eq '')
        {
            $dram_vendor_id = "0xf0";
        }

        print "DRAM vendor ID:$dram_vendor_id";
        $emi_reserved = "0";
        
        print FILE pack("L", hex($mem_type));                           #type
        $fs = $fs + 4 ;
      #  print "mem type:$mem_type\n";
        if ($flash_id =~ /\{ (\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+),(\w+)\}/)
        {
            print FILE pack ("C", hex ($1)) ;            #id
            print FILE pack ("C", hex ($2)) ;
            print FILE pack ("C", hex ($3)) ;
            print FILE pack ("C", hex ($4)) ;
            print FILE pack ("C", hex ($5)) ;
            print FILE pack ("C", hex ($6)) ;
            print FILE pack ("C", hex ($7)) ;
            print FILE pack ("C", hex ($8)) ;
            print FILE pack ("C", hex ($9)) ;
            print FILE pack ("C", hex ($10)) ;
            print FILE pack ("C", hex ($11)) ;
            print FILE pack ("C", hex ($12)) ;
            print FILE pack ("C", hex ($13)) ;
            print FILE pack ("C", hex ($14)) ;
            print FILE pack ("C", hex ($15)) ;
            print FILE pack ("C", hex ($16)) ;
           # print "id : $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16\n";
            $fs = $fs + 16 ;
        }
        
        print FILE pack("L", hex($flash_length));                     #length
        $fs = $fs + 4 ;
        
        print FILE pack("L", hex($emi_clk_str));                     #length
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_drva))) ;         # EMI_CONA_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_drvb))) ;    # DRAMC_DRVCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odla))) ;    # DRAMC_DRVCTL1_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odlb))) ;        # DRAMC_DLE_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odlc))) ;      # DRAMC_ACTIM_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odld))) ;  # DRAMC_GDDR3CTL1_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odle))) ;      # DRAMC_CONF1_VAL
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_odlf))) ;    # DRAMC_DDR2CTL_VAL
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_odlg))) ;    # DRAMC_TEST2_3_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odlh))) ;      # DRAMC_CONF2_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odli))) ;    # DRAMC_PD_CTRL_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odlj))) ;    # DRAMC_PADCTL3_VAL
        
        $fs = $fs + 4 ;
        print FILE pack ("L", hex (lc($emi_odlk))) ;     # DRAMC_DQODLY_VAL
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_odll))) ;        # DRAMC_ADDR_OUTPUT_DLY
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_odlm))) ;        # DRAMC_CLK_OUTPUT_DLY
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_odln))) ;           # $DRAMC_ACTIM1_VAL
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_coni))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_conj))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_conk))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_conl))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_conn))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_duta))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_dutb))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_dutc))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_duca))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_ducb))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_duce))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
       
        print FILE pack ("L", hex (lc($emi_iocl))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
        print FILE pack ("L", hex (lc($emi_gend))) ;        # DRAMC_MISCTL0_VAL
        $fs = $fs + 4 ;
        
##
        $emi_size_str = $MDL_INFO_LIST_LOCAL->[$_]->{$LPSDRAM_CHIP_SELECT}->{'Density (Mb)'};
        print "mem size:$emi_size_str\n";
        $emi_size = &memory_size_transfer($emi_size_str);
                                                                                        #  DRAM_RANK_SIZE[4]
        if($emi_size_str =~ /(\d{4})\+(\d{4})/)
        {
                $emi_size = sprintf("0x%X",($1)*1024*1024/8);
                print FILE pack ("L", hex ($emi_size)) ;                                  
                $emi_size = sprintf("0x%X",($2)*1024*1024/8);
                print FILE pack ("L", hex ($emi_size)) ;
        }
        else
        {
                $emi_size = sprintf("0x%X",$emi_size_str*1024*1024/8);
                print FILE pack ("L", hex ($emi_size)) ;
                print FILE pack ("L", hex ("0")) ;
        }
        # print "mem size:$emi_size\n";

        print FILE pack ("L", hex ("0")) ;
        print FILE pack ("L", hex ("0")) ;
        $fs = $fs + 16;
       
        print FILE pack("L", hex($dram_vendor_id));                           #type
        $fs = $fs + 4 ;

        print FILE pack ("L", hex ("0")) ;        # match flag
        $fs = $fs + 4 ;
##        
        $$file_size_local = $$file_size_local + $fs;
   }
}

#****************************************************************************
# subroutine:  dependency check
# return:      None
#****************************************************************************
sub dependency_check
{
    my ($target, $force_del_new_file, @depends) = (@_);
    
    return unless (-e $target);

    ## Now check if the $target file check-in or auto-gen
    ## Read whole file ##
    open SRC_FILE_R , "<$target" or &error_handler("$target: file error!", __FILE__, __LINE__);
    my $saved_sep = $/;
    undef $/;
    my $reading = <SRC_FILE_R>; 
    close SRC_FILE_R;
    $/ = $saved_sep;

    ## Look for check-in pattern ##
    if ($reading =~ /MANUAL-CHECKIN/i)
    {
        print "$target: Check-in message is found. No need to update.\n";
        return;
    }

    ## for emiclean to force delete new files
    if ($force_del_new_file eq 'TRUE')
    {
      #  unlink $target if (-e $target);
        return;
    }

    # unlink $target if (-e $target);
        return;
   
}

#****************************************************************************
# subroutine:  fs_read_excel
# return:      hash of matching rows and indexing rows
# input:       $file:        excel file to be read
#              $sheet:       sheet to open
#              $idx_count:   the number of rows that are index rows
#              $search_col:  the column to be searched for the targets
#              $target_href: the target patterns to be searched
#****************************************************************************
sub fs_read_excel
{
   my ($file, $sheet, $idx_count, $search_col, $target_href) = @_;
    my %rows;

    ### report error directly if MDL does not exist
    unless (-e $file)
    {
        &error_handler("$file: File does not exist! Please check with MVG or EMI Gen owner!", __FILE__, __LINE__);
    }
    
    ### copy and rename the current excel file to prevent concurrency build problem
 #   $MDL_BACKUP_LOAD_PATH_Local =~ s/^.\\|^\\//;
 #   my $copied_file = $MDL_BACKUP_LOAD_PATH_Local;
 #   my $copied_file_temp = '';
 #   my $orifile = '';
 #   my $orifile_temp = '';
 #   my $curr_time_str = &get_CurrTime_str();
 #   if($MDL_BACKUP_LOAD_PATH_Local =~ /build/)
 #   {
 #   	 $MDL_BACKUP_LOAD_PATH_Local = $& . $'; #'
 #      print "MDL 1:$`,2:$&,3:$'\n";	
 #   }
 #   if($file =~ /^(\w+.*?)\.xls$/)
 #   {
 #       $orifile_temp = $1;
 #       if($orifile_temp =~ /^(\w*.*?)tools(\w*.*?)MemoryDeviceList_(\w+)$/)
 #       {
 #       	    print "1:$1,2:$2,3:$3\n";
 #           	$orifile = $1 . $MDL_BACKUP_LOAD_PATH_Local . "\/". "MemodeDeviceList_" . $3;
 #           	print "file:$orifile\n";
 #       } 
 #   }
 #   $copied_file = $orifile . "_" . $curr_time_str . $$ . ".xls";
    #system("cp \"$file\" \"$copied_file\"");
 #   $file = $copied_file;
    my $WorkSheet;
    my $Excel;
    my $Book;
    my $parser;
    my $workbook;
    my ($row_min, $row_max); 
    my ($col_min, $col_max); 
   
    	 $parser   = Spreadsheet::ParseExcel->new();
       $workbook = $parser->parse($file) or die "cannot open MDL $!\n";#$parser->parse('tools/MemoryDeviceList/MemoryDeviceList_MT6280_Since11BW1224.xls') or die "cannot open MDL $!\n";
       PrintDependency($file);
       #print "sheet : $sheet\n";
       # get the worksheet
       #my $sheet = $workbook->worksheet('SERIAL_FLASH');
       $WorkSheet = $workbook->worksheet($sheet);
       ($row_min, $row_max) = $WorkSheet->row_range();
	     ($col_min, $col_max) = $WorkSheet->col_range();
       
       # get the sheet name
       my $name = $WorkSheet->get_name();
      # print "[sheetname]: $name, $row_min, $row_max, $col_min, $col_max\n";
  
    ### fill in index rows
    ### $rows{'index'}->{1}->{1} = 'Vendor'
    ### $rows{'index'}->{2}->{12} = 'Region'
    for (0..$idx_count)
    {
          my $col = 0;
          while (defined &xls_cell_value($WorkSheet, $_, $col))
          {
                $rows{'index'}->{$_}->{$col} = &xls_cell_value($WorkSheet, $_, $col);
              #  print "[$_][$col]: " . $rows{'index'}->{$_}->{$col} . "\n" ;
                $col++;
          }
     

    }

    ### find rows where the target exists
    ### $rows{'target'}->{0}->{1} = 'Intel'
    ### $rows{'target'}->{0}->{12} = '*'
    ### $rows{'target'}->{1}->{113} = 'x'
    my $row = $idx_count - 1;
    my $empty_flag = 0;  # exit if two consecutive empty lines are encountered
    foreach (sort keys %$target_href)
    {
        my $tgt = $_;
        while (1)
        {
            if (!defined &xls_cell_value($WorkSheet, $row, $search_col))
            {
                if ($empty_flag == 0)
                {
                    $empty_flag = 1;
                }
                else
                {
                    last;
                }
            }
            else
            {
                $empty_flag = 0;
            }
            
            my $target_found = 0;  
            my $content = &xls_cell_value($WorkSheet, $row, $search_col);
           # print "content : $content,$search_col, $row\n";
            my @xls_content = split /\s/, $content;
            foreach (0..$#xls_content)
            {
                if ($xls_content[$_] eq $target_href->{$tgt})
                {
                    
                    my $col = 1;
                    while (defined &xls_cell_value($WorkSheet, 1, $col))
                    {
                        $rows{'target'}->{$tgt}->{$col} = &xls_cell_value($WorkSheet, $row, $col);
                       # print "target:$rows{'target'}->{$tgt}->{$col}, $tgt, $col\n";
                        $col++;
                    }
                   
                    $row++;  # start the next target searching from the next row, assuming that the excel sequence is the same as expected
                    $target_found = 1;
                    last;
                }
            }
            last if ($target_found == 1);
            $row++;
        }
        last if ($empty_flag == 1);
        
    }

   
    
    return %rows;
}

#****************************************************************************
# subroutine:  get_info
# input:       $file:        file path
#              $sheet:       sheet to open
#              $idx_count:   the number of rows that are index rows
#              $sesarch_col: the column to be searched for the targets
#              $target_href: the target patterns to be searched
#              $href:        output matching hash reference
#              $index_href:  output index hash reference
#****************************************************************************
sub get_info
{
    my ($file, $sheet, $idx_count, $search_col, $target_href, $href, $index_href) = @_;
    my $ret_str;
    my %rows;

    	%rows = &fs_read_excel($file, $sheet, $idx_count, $search_col, $target_href);
  
    #my %rows = &fs_read_excel($file, $sheet, $idx_count, $search_col, $target_href, $MDL_BACKUP_LOAD_PATH);
    if (!defined $rows{'target'})
    {
    	#  print "test reulst\n";
        $ret_str = "FALSE";
        return $ret_str;
    }
    
    ### fill in index_href
    foreach (1..$idx_count)
    {
       $index_href->{$_} = $rows{'index'}->{$_};
    }

    ### fill in output href
    ### $href->{0}->{'Vendor'} = 'Intel'
    ### $href->{0}->{'Last Bank'}->{'Region'} = '*'
    ### $href->{1}->{'General DRAM Settings'}->{'MT6229'}->{'EMI_DRAM_MODE'} = 'x'
    foreach (sort keys %$target_href)
    {
          my $tgt = $_;
          my $col = 1;

          while (defined $rows{'index'}->{1}->{$col})
          {
                my $content;
                if (defined $rows{'target'}->{$tgt}->{$col})
                {
                      $content = $rows{'target'}->{$tgt}->{$col};
                }
                else
                {
                      $content = " ";
                }
                if($OS_ENV eq "MSWin32")
                {
                if ($rows{'index'}->{1}->{$col} eq $rows{'index'}->{2}->{$col})
                {
                      $href->{$tgt}->{$rows{'index'}->{1}->{$col}} = $content;
                      print "href->{$tgt}->{$rows{'index'}->{1}->{$col}} = $href->{$tgt}->{$rows{'index'}->{1}->{$col}}\n" if ($DebugPrint == 1);
                }
                elsif (($idx_count == 2) or ($rows{'index'}->{2}->{$col} eq $rows{'index'}->{3}->{$col}))
                {
                      $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}} = $content;
                      print "href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}} = $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}}\n" if ($DebugPrint == 1);
                }
                else
                {
                      $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}}->{$rows{'index'}->{3}->{$col}} = $content;
                      print "href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}}->{$rows{'index'}->{3}->{$col}} = $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}}->{$rows{'index'}->{3}->{$col}}\n" if ($DebugPrint == 1);
                }
                $col++;
                }
                else
                {
                     	   
                     if ($rows{'index'}->{1}->{$col} eq "pattern")
                     {
                           $href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $content;
                           print "1 href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $href->{$tgt}->{$rows{'index'}->{0}->{$col}}\n" if ($DebugPrint == 1);
                     }
                     elsif (($idx_count == 2) or ($rows{'index'}->{2}->{$col} eq $rows{'index'}->{3}->{$col}))
                     {
                     	    if($col >=1 && $col <=7)
                     	    {
                     	    	$href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $content;
                     	    #	 print "1 href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $href->{$tgt}->{$rows{'index'}->{0}->{$col}}\n"
                     	    }
                     	    else
                    	    {
                              $href->{$tgt}->{$rows{'index'}->{0}->{$col}}->{$rows{'index'}->{1}->{$col}} = $content;
                           #   print "2 href->{$tgt}->{$rows{'index'}->{0}->{$col}}->{$rows{'index'}->{1}->{$col}} = $href->{$tgt}->{$rows{'index'}->{0}->{$col}}->{$rows{'index'}->{1}->{$col}}\n" if ($DebugPrint == 1);
                           }
                     }
                     else
                     {
                     	      if(($col >=1 && $col <=7) || $col == 95 )
                     	    {
                     	    	$href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $content;
                                #print "1 href->{$tgt}->{$rows{'index'}->{0}->{$col}} = $href->{$tgt}->{$rows{'index'}->{0}->{$col}}\n"
                     	    }
                     	    else
                    	    {
                               $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}} = $content;
                           #print "3 href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}} = $href->{$tgt}->{$rows{'index'}->{1}->{$col}}->{$rows{'index'}->{2}->{$col}}\n" if ($DebugPrint == 1);
                          }
                          
                     }
                     $col++;
                } 
              
          }
    }

    $ret_str = "TRUE";
    return $ret_str;
}

sub Parse_MDL
{
	my ($CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MDL_INFO_LIST_LOCAL,$MEMORY_DEVICE_LIST_XLS_LOCAL) = @_;
	my ($result, $result0, $result1);
	
		 if($os eq "MSWin32")
	  {
        for (1..$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT})
        {
            $result = &get_info($$MEMORY_DEVICE_LIST_XLS_LOCAL, 'mt6572', $IDX_COUNT, 2, $MCP_LIST[$_], &ret_tmp_hash_ref($_), \%MEM_DEV_LIST_INDEX);
            if (($result ne 'TRUE'))
            {
                $result = &get_info($$MEMORY_DEVICE_LIST_INT_XLS_LOCAL, 'mt6572', $IDX_COUNT, 2, $MCP_LIST[$_], &ret_tmp_hash_ref($_), \%MEM_DEV_LIST_INDEX);
            }
            if ($result ne 'TRUE')
            {
                my $p_number;
                if (defined $MCP_LIST[$_]->{0})
                {
                    $p_number = $MCP_LIST[$_]->{0};
                }
                else
                {
                    $p_number = $MCP_LIST[$_]->{1};
                }
                &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: Part Number $p_number not found!", __FILE__, __LINE__);
            }
            $MDL_INFO_LIST_LOCAL->[$_] = &ret_tmp_hash_ref($_);
        }
    }
	  else
	  {
	  	    for (1..$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT})
         {
             $result = &get_info($$MEMORY_DEVICE_LIST_XLS_LOCAL, 'mt6572', $IDX_COUNT, 1, $MCP_LIST[$_], &ret_tmp_hash_ref($_), \%MEM_DEV_LIST_INDEX);
             if (($result ne 'TRUE'))
             {
             	  # print "result : $result\n";
                 $result = &get_info($$MEMORY_DEVICE_LIST_INT_XLS_LOCAL, 'mt6572', $IDX_COUNT, 1, $MCP_LIST[$_], &ret_tmp_hash_ref($_), \%MEM_DEV_LIST_INDEX);
             }
             if ($result ne 'TRUE')
             {
                 my $p_number;
                 if (defined $MCP_LIST[$_]->{0})
                 {
                     $p_number = $MCP_LIST[$_]->{0};
                 }
                 else
                 {
                     $p_number = $MCP_LIST[$_]->{1};
                 }
                 &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: Part Number $p_number not found!", __FILE__, __LINE__);
             }
             $MDL_INFO_LIST_LOCAL->[$_] = &ret_tmp_hash_ref($_);
         }
	  }
    
	
	
}

#****************************************************************************
# subroutine:  ret_tmp_hash_ref
# input:       $idx: The index of the current tmp hash
# return:      the hash reference of the input index
#****************************************************************************
my (%tmp_hash1, %tmp_hash2, %tmp_hash3, %tmp_hash4, %tmp_hash5, %tmp_hash6, %tmp_hash7, %tmp_hash8, %tmp_hash9, %tmp_hash10, %tmp_hash11, %tmp_hash12, %tmp_hash13, %tmp_hash14, %tmp_hash15, %tmp_hash16, %tmp_hash17, %tmp_hash18, %tmp_hash19, %tmp_hash20);
sub ret_tmp_hash_ref
{
    my ($idx) = @_;

    if ($idx == 1)
    {
        return \%tmp_hash1;
    }
    elsif ($idx == 2)
    {
        return \%tmp_hash2;
    }
    elsif ($idx == 3)
    {
        return \%tmp_hash3;
    }
    elsif ($idx == 4)
    {
        return \%tmp_hash4;
    }
    elsif ($idx == 5)
    {
        return \%tmp_hash5;
    }
    elsif ($idx == 6)
    {
        return \%tmp_hash6;
    }
    elsif ($idx == 7)
    {
        return \%tmp_hash7;
    }
    elsif ($idx == 8)
    {
        return \%tmp_hash8;
    }
    elsif ($idx == 9)
    {
        return \%tmp_hash9;
    }
    elsif ($idx == 10)
    {
        return \%tmp_hash10;
    }
    elsif ($idx == 11)
    {
        return \%tmp_hash11;
    }
    elsif ($idx == 12)
    {
        return \%tmp_hash12;
    }
    elsif ($idx == 13)
    {
        return \%tmp_hash13;
    }
    elsif ($idx == 14)
    {
        return \%tmp_hash14;
    }
    elsif ($idx == 15)
    {
        return \%tmp_hash15;
    }
    elsif ($idx == 16)
    {
        return \%tmp_hash16;
    }
    elsif ($idx == 17)
    {
        return \%tmp_hash17;
    }
    elsif ($idx == 18)
    {
        return \%tmp_hash18;
    }
    elsif ($idx == 19)
    {
        return \%tmp_hash19;
    }
    elsif ($idx == 20)
    {
        return \%tmp_hash20;
    }
    
    #&error_handler("$CUSTOM_MEMORY_DEVICE_HDR: COMBO_MEM_ENTRY_COUNT has exceeded its upper limit $MAX_COMBO_MEM_ENTRY_COUNT! Please reduce COMBO_MEM_ENTRY_COUNT!", __FILE__, __LINE__);
}

sub Parse_Memory_Device
{
	open CUSTOM_MEMORY_DEVICE_HDR, "<$CUSTOM_MEMORY_DEVICE_HDR" or &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: file error!", __FILE__, __LINE__);
	PrintDependency($CUSTOM_MEMORY_DEVICE_HDR);
my ($LPSDRAM_CHIP_SELECT_LOCAL, $CUSTOM_MEM_DEV_OPTIONS_LOCAL, $MCP_LIST_LOCAL) = @_;
my $cs;
$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT} = 0;
while (<CUSTOM_MEMORY_DEVICE_HDR>)
{
   # matching the following lines
   # "#define MEMORY_DEVICE_TYPE          NOR_RAM_MCP"
   # "#define FLASH_ACCESS_TYPE           ASYNC_ACCESS"
   # "#define RAM_ACCESS_TYPE             ASYNC_ACCESS"
   # "#define CS0_PART_NUMBER             RD38F3040L0ZBQ0"
   # "#define PROJECT_EXCEPTED_RAM_LIMIT  0x001C0000"
   # "#define PROJECT_EXCEPTED_CODE_LIMIT 0x00040000"

   # error-checking
   # error-checking
    if (/^#if|^#ifdef|^#ifndef|^#elif|^#else/)
    {
      &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: Not allowed to set conditional keywords $_ in custom_MemoryDevice.h!", __FILE__, __LINE__)
          unless (/^#ifndef\s+__CUSTOM_MEMORYDEVICE__/);
    }
    if (/^#define\s+(\w+)\[(\d+)\]\s+\((\w*)\)/ || /^#define\s+(\w+)\[(\d+)\]\s+(\w*)/ || 
        /^#define\s+(MEMORY_DEVICE_TYPE)\s+\((\w*)\)/ || /^#define\s+(MEMORY_DEVICE_TYPE)\s+(\w*)/ ||
        /^#define\s+(BOARD_ID)\s+\((\w*)\)/ || /^#define\s+(BOARD_ID)\s+(\w*)/ || /^#define\s+(EMI_CLK)\s+(\w*)/) 
    {
       # print "\n memorydevice: $1, $2, $3\n" ;
        
        my $content ;
         my %TMP_PART_NUMBER;
         my $mcp_idx ;
         $cs = 0;
        if ($1 eq "BOARD_ID")
        {
            $CustBoard_ID = $2 ;
        }
        elsif ($1 eq "CS_PART_NUMBER")
        {
            #print "\nCS0 $2, $3\n" ;
            #$CustCS_PART_NUMBER[$2] = $3 ;
            #$CustCS_CustemChips = $CustCS_CustemChips + 1 ;
            $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{$1} = $3;
            $content = $3;
            $mcp_idx = ($2+1);
            $TMP_PART_NUMBER{$cs} = $content;
            $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT} = $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT} + 1;
          #  print "part number, $CustCS_PART_NUMBER[$2], $CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT}, $1, $2, $3\n";
        }
        elsif ($1 eq "EMI_CLK")
        {
            my $emi_clk_index = $2;
            if ($3 =~ /\d{3}/)
            {
                $EMI_CLK[$emi_clk_index] = $&;
                print "EMI_CLK[$emi_clk_index] = $&\n";
            }
            else
            {
                print "[ERROR] EMI CLK value is not a 3 digit number."; 
            }  
        }
        #{
        #      	$CUSTOM_MEM_DEV_OPTIONS_LOCAL->{EMI_CLK} = $2;
        #}
        
        $LPSDRAM_CHIP_SELECT_LOCAL = $cs;
        
        if (defined $MCP_LIST_LOCAL->[$mcp_idx])
         {
            if (defined $MCP_LIST_LOCAL->[$mcp_idx]->{$cs})
            {
               &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: COMBO_MEM$mcp_idx\_CS$cs\_PART_NUMBER multiply defined!", __FILE__, __LINE__);
            }
            else
            {
               $MCP_LIST_LOCAL->[$mcp_idx]->{$cs} = $TMP_PART_NUMBER{$cs};
            }
         }
         else
         {
            $MCP_LIST_LOCAL->[$mcp_idx] = \%TMP_PART_NUMBER;
         }
    }

     
}

# COMBO_MEM_ENTRY_COUNT cannot exceed the upper limit MAX_COMBO_MEM_ENTRY_COUNT
if ($CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT} > $MAX_COMBO_MEM_ENTRY_COUNT)
{
    &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: COMBO_MEM_ENTRY_COUNT has exceeded its upper limit $MAX_COMBO_MEM_ENTRY_COUNT! Please reduce COMBO_MEM_ENTRY_COUNT!", __FILE__, __LINE__);
}
# COMBO_MEM_ENTRY_COUNT and the number of COMBO_MEMxx_CSx_PART_NUMBER does not match
if ($CUSTOM_MEM_DEV_OPTIONS_LOCAL->{COMBO_MEM_ENTRY_COUNT} != $#{$MCP_LIST_LOCAL})
{
    &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: COMBO_MEM_ENTRY_COUNT and the number of COMBO_MEMxx_CSx_PART_NUMBER does not match!", __FILE__, __LINE__);
}

close CUSTOM_MEMORY_DEVICE_HDR;

}

sub Lookup_LPDDR_EMI_setting_by_IDX_CLK_BB_REG
{
    my ($idx, $clk, $bb, $reg, $LPSDRAM_CHIP_SELECT_LOCAL, $CUSTOM_MEM_DEV_OPTIONS_LOCAL) = @_;
    my $clk_str = sprintf("%sMHZ", $clk);
    #print "bb:$bb,$LPSDRAM_CHIP_SELECT_LOCAL, $clk_str \n";
    if($bb eq 'MT6572') 
    {
       return $MDL_INFO_LIST[$idx]->{$LPSDRAM_CHIP_SELECT_LOCAL}->{$clk_str}->{$reg};
    }
    
    &error_handler("$CUSTOM_MEMORY_DEVICE_HDR: EMI setting of $bb is not supported!", __FILE__, __LINE__);
}

sub get_memory_type
{
    my 	($mem_type_ori) = @_;
    #print "mem : $mem_type_ori\n";
    if($mem_type_ori eq "Discrete DDR1")
    {
    	  return 0x0001;
    }
    elsif($mem_type_ori eq "Discrete DDR2")
    {
    	return 0x0002;
    }
    elsif($mem_type_ori eq "Discrete PCDDR3")
    {
    	return 0x0003;
    }
    elsif($mem_type_ori eq "Discrete LPDDR3")
    {
    	return 0x0004;
    }
    elsif($mem_type_ori eq "MCP(NAND+DDR1)")
    {
    	return 0x0101;
    }
    elsif($mem_type_ori eq "MCP(NAND+DDR1)")
    {
    	return 0x0101;
    }
    elsif($mem_type_ori eq "MCP(NAND+DDR2)")
    {
    	return 0x0102;
    }
    elsif($mem_type_ori eq "MCP(NAND+PCDDR3)")
    {
    	return 0x0103;
    }
    elsif($mem_type_ori eq "MCP(NAND+LPDDR3)")
    {
    	return 0x0104;
    }
    elsif($mem_type_ori eq "MCP(eMMC+DDR1)")
    {
    	return 0x0201;
    }
    elsif($mem_type_ori eq "MCP(eMMC+DDR2)")
    {
    	return 0x0202;
    }
    elsif($mem_type_ori eq "MCP(eMMC+PCDDR3)")
    {
    	return 0x0203;
    }
    elsif($mem_type_ori eq "MCP(eMMC+LPDDR3)")
    {
    	return 0x0204;
    }
    else
    {
       return 	0x0;
    }

}

sub parse_flash_id
{
   my ($flash_id_ori_local, $flash_length_local) = @_;
   my $flash_id_finally, $flash_id_str_temp;
   $$flash_length_local = (length($flash_id_ori_local)/2) - 1;
   my $flash_length_temp = $$flash_length_local + 1;
   #print "flash : $flash_id_ori_local, $flash_length_local\n";
   my $i = 2;
   for(2..$flash_length_temp)
   {
   	  my $comma = ($_ < $flash_length_temp) ? "," : "";
      my $flash_id_temp = sprintf("0x%s", substr($flash_id_ori_local, $i, 2));	
      $i = $i + 2;
      $flash_id_str_temp = $flash_id_str_temp . $flash_id_temp . $comma;
   }
   $flash_id_finally = "{" . $flash_id_str_temp . "}";
   $flash_id_str_temp = " ";
   return $flash_id_finally;   	
}

sub parse_flash_id_tag
{
   my ($flash_id_ori_local, $flash_length_local) = @_;
   my $flash_id_finally, $flash_id_str_temp;
   $$flash_length_local = (length($flash_id_ori_local)/2) - 1;
   my $flash_length_temp = $$flash_length_local + 1;
   #print "flash : $flash_id_ori_local, $flash_length_local\n";
   my $i = 2;
   for(2..$flash_length_temp)
   {
   	  my $comma = ($_ <= $flash_length_temp) ? "," : "";
      my $flash_id_temp = sprintf("0x%s", substr($flash_id_ori_local, $i, 2));	
      $i = $i + 2;
      $flash_id_str_temp = $flash_id_str_temp . $flash_id_temp . $comma;
   }
   for($$flash_length_local..15)
   {
   	   my $comma = ($_ < 15) ? "," : "";
   	   my $id = "0x00";
       $flash_id_str_temp = $flash_id_str_temp . $id . $comma;	
   }
   $flash_id_finally = "{" . $flash_id_str_temp . "}";
   $flash_id_str_temp = " ";
   return $flash_id_finally;   	
}

sub memory_size_transfer
{
   my ($emi_size_str_local) = @_;
   my $emi_size_finally;
   if ($emi_size_str_local =~ /(\d{4})\+(\d{4})/)
   {
   	    my $emi_size1 = sprintf("0x%X", ($1*1024*1024/8));
   	    my $emi_size2 = sprintf("0x%X", ($2*1024*1024/8));
   	  #  print "memory size : $1, $2\n";
   	    $emi_size_finally = "{$emi_size1, $emi_size2,0,0}";
       	return $emi_size_finally;
   }
   else
   {
   	    #print "memory size : $emi_size_str_local\n";
   	    my $emi_size = sprintf("0x%X", ($emi_size_str_local*1024*1024/8));
       	$emi_size_finally = "{$emi_size, 0x00000000,0,0}";
       	return $emi_size_finally;
   }	
}
