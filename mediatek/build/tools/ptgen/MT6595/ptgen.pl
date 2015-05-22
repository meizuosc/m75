#!/usr/local/bin/perl -w
#****************************************************************************
# Included Modules
#****************************************************************************
use strict;
use File::Basename;
use File::Path;
my $LOCAL_PATH;

BEGIN
{
    $LOCAL_PATH = dirname($0);
}
use lib "$LOCAL_PATH/../../Spreadsheet";
use lib "$LOCAL_PATH/../../";
require 'ParseExcel.pm';
use pack_dep_gen;
use lib "$LOCAL_PATH/../../YAML/lib";
use YAML qw(LoadFile DumpFile Load Dump);

#****************************************************************************
# Global Variables
#****************************************************************************
my $Version = 4.0;

#my $ChangeHistory="3.1 AutoDetect eMMC Chip and Set MBR_Start_Address_KB\n";
#my $ChangeHistory = "3.2 Support OTP\n";
#my $ChangeHistory = "3.3 Support Shared SD Card\n";
#my $ChangeHistory = "3.4 CIP support\n";
#my $ChangeHistory = "3.5 Fix bug\n";
#my $ChangeHistory = "3.6 Support YAML format scatter file\n";
my $ChangeHistory = "\t1.Support YAML partition table
\t2.redesign xls partition table
\t3.Set partition size by project
\t4.auto align system/cache/userdata partition start address
\t5.revise all the code";

my @partition_layout_raw;
my @partition_layout_cooked;
my @BR_INDEX = ();
my %download_files;
my %sepcial_operation_type;
my %kernel_alias;       #alias for kernel c  file modify
my %region_map;
my %ArgList;
my %AlignPartList;
my @GeneratedFile;
my $Used_Size = 0;
my $PMT_END_NAME;       #end partition of no reserved partition
my $DebugPrint = "no";  # yes for debug; no for non-debug
my $PRODUCT_OUT;
my $ptgen_location;
my $Partition_layout_xls;
my $custom_out_prefix;
my $configs_out_prefix;
my $preloader_out_prefix;

#****************************************************************************
# main flow : init_filepath
#****************************************************************************
&InitGlobalValue();
my $Partition_layout_yaml = "$ptgen_location/$ArgList{SHEET_NAME}.yaml";
my $COMBO_NAND_TOOLH      = "mediatek/external/mtd-utils/ubi-utils/combo_nand.h";

my $KernelH            = "$custom_out_prefix/kernel/partition.h";
my $LK_PartitionC      = "$custom_out_prefix/lk/partition.c";
my $PART_SIZE_LOCATION = "$configs_out_prefix/configs/partition_size.mk";
my $COMBO_NAND_KERNELH = "$custom_out_prefix/common/combo_nand.h";

my $SCAT_FILE = "$PRODUCT_OUT/$ArgList{PLATFORM}_Android_scatter.txt";

#****************************************************************************
# main flow
#****************************************************************************
PrintDependModule($0);
&ShowInfo(\%ArgList);
&clear_files();
if ($ArgList{PARTITION_TABLE_PLAIN_TEXT} ne "yes")
{
    &ReadExcelFile($Partition_layout_xls);
    &debug_print_layout("./out/1.log", \@partition_layout_raw);
    &GenPlainText($Partition_layout_yaml, \@partition_layout_raw);
}
@partition_layout_raw = @{&ReadPlainText($Partition_layout_yaml)};
&debug_print_layout("./out/2.log", \@partition_layout_raw);
@partition_layout_cooked = @{&ProcessRawPartitionLayoutData(\@partition_layout_raw)};
print "###################Final Data###################\n";
print Dump(\@partition_layout_cooked);

&GenYAMLScatFile();

if ($ArgList{EMMC_SUPPORT} eq "yes" || $ArgList{NAND_UBIFS_SUPPORT} eq "yes")
{
    &GenPartSizeFile_iniFile($PART_SIZE_LOCATION, $PRODUCT_OUT);
}

&GenLK_PartitionC();
if ($ArgList{EMMC_SUPPORT} ne "yes")
{
    &GenKernel_PartitionC();
}
if ($DebugPrint eq "yes")
{
    &print_rm_script();
}
print "^_^**********Ptgen Done********** ^_^\n";
printf "Generated files list:\n" . ("%s\n" x @GeneratedFile), @GeneratedFile;
exit 0;

#****************************************************************************
# subroutine:  InitAlians
# return:NONE
#****************************************************************************
sub InitGlobalValue
{
=alias
    #alias
    $preloader_alias{"SECCFG"}  = "SECURE";
    $preloader_alias{"SEC_RO"}  = "SECSTATIC";
    $preloader_alias{"ANDROID"} = "ANDSYSIMG";
    $preloader_alias{"USRDATA"} = "USER";

    $lk_xmodule_alias{"DSP_BL"}  = "DSP_DL";
    $lk_xmodule_alias{"SECCFG"}  = "SECURE";
    $lk_xmodule_alias{"SEC_RO"}  = "SECSTATIC";
    $lk_xmodule_alias{"EXPDB"}   = "APANIC";
    $lk_xmodule_alias{"ANDROID"} = "ANDSYSIMG";
    $lk_xmodule_alias{"USRDATA"} = "USER";

    $lk_alias{"BOOTIMG"} = "boot";
    $lk_alias{"ANDROID"} = "system";
    $lk_alias{"USRDATA"} = "userdata";

    $kernel_alias{"SECCFG"}  = "seccnfg";
    $kernel_alias{"BOOTIMG"} = "boot";
    $kernel_alias{"SEC_RO"}  = "secstatic";
    $kernel_alias{"ANDROID"} = "system";
    $kernel_alias{"USRDATA"} = "userdata";
=cut
    %region_map = (
                   EMMC_USER   => "EMMC_PART_USER",
                   EMMC_BOOT_1 => "EMMC_PART_BOOT1",
                   EMMC_BOOT_2 => "EMMC_PART_BOOT2",
                   EMMC_RPMB   => "EMMC_PART_RPMB",
                   EMMC_GP_1   => "EMMC_PART_GP1",
                   EMMC_GP_2   => "EMMC_PART_GP2",
                   EMMC_GP_3   => "EMMC_PART_GP3",
                   EMMC_GP_4   => "EMMC_PART_GP4",
                  );

    #Feature Options:parse argv from alps/mediatek/config/{project}/ProjectConfig.mk

    $ArgList{PLATFORM}                   = $ENV{MTK_PLATFORM};
    $ArgList{platform}                   = lc($ENV{MTK_PLATFORM});
    $ArgList{PROJECT}                    = $ENV{PROJECT};
    $ArgList{FULL_PROJECT}               = $ENV{FULL_PROJECT};
    $ArgList{PAGE_SIZE}                  = $ENV{MTK_NAND_PAGE_SIZE};
    $ArgList{EMMC_SUPPORT}               = $ENV{MTK_EMMC_SUPPORT};
    $ArgList{EMMC_SUPPORT_OTP}           = $ENV{MTK_EMMC_SUPPORT_OTP};
    $ArgList{SHARED_SDCARD}              = $ENV{MTK_SHARED_SDCARD};
    $ArgList{CIP_SUPPORT}                = $ENV{MTK_CIP_SUPPORT};
    $ArgList{FAT_ON_NAND}                = $ENV{MTK_FAT_ON_NAND};
    $ArgList{NAND_UBIFS_SUPPORT}         = $ENV{MTK_NAND_UBIFS_SUPPORT};
    $ArgList{COMBO_NAND_SUPPORT}         = $ENV{MTK_COMBO_NAND_SUPPORT};
    $ArgList{PL_MODE}                    = $ENV{PL_MODE};
    $ArgList{PARTITION_TABLE_PLAIN_TEXT} = $ENV{MTK_PARTITION_TABLE_PLAIN_TEXT};

    if ($ArgList{EMMC_SUPPORT} eq "yes")
    {
        $ArgList{PAGE_SIZE} = 2;
    }
    else
    {
        if ($ArgList{COMBO_NAND_SUPPORT} eq "yes")
        {
            $ArgList{PAGE_SIZE} = 4;
        }
        else
        {
            if ($ENV{MTK_NAND_PAGE_SIZE} =~ /(\d)K/)
            {
                $ArgList{PAGE_SIZE} = $1;
            }
        }
    }
    if (!$ENV{TARGET_BUILD_VARIANT})
    {
        $ArgList{TARGET_BUILD_VARIANT} = "eng";
    }
    else
    {
        $ArgList{TARGET_BUILD_VARIANT} = $ENV{TARGET_BUILD_VARIANT};
    }

    #filepath;
    $ptgen_location = "mediatek/build/tools/ptgen/$ArgList{PLATFORM}";
    if ($ArgList{PL_MODE})
    {
        $Partition_layout_xls = "$ptgen_location/test_partition_table_internal_$ArgList{PLATFORM}.xls";
        $ArgList{SHEET_NAME} = $ArgList{PL_MODE};
    }
    else
    {
        $Partition_layout_xls = "$ptgen_location/partition_table_$ArgList{PLATFORM}.xls";
        if ($ArgList{EMMC_SUPPORT} eq "yes")
        {
            $ArgList{SHEET_NAME} = "emmc";
        }
        else
        {
            $ArgList{SHEET_NAME} = "nand";
        }
    }
    if (exists $ENV{OUT_DIR})
    {
        $PRODUCT_OUT = "$ENV{OUT_DIR}/target/product/$ArgList{PROJECT}";
    }
    else
    {
        $PRODUCT_OUT = "out/target/product/$ArgList{PROJECT}";
    }
    $custom_out_prefix  = "$ENV{MTK_ROOT_OUT}/PTGEN";#"mediatek/custom/$ArgList{PROJECT}";
    $configs_out_prefix = "$ENV{MTK_ROOT_OUT}/PTGEN";#"mediatek/config/$ArgList{PROJECT}";
    $preloader_out_prefix = "$ENV{MTK_ROOT_OUT}/PRELOADER_OBJ";

    #download files
    %download_files = (
                       preloader  => "preloader_$ArgList{PROJECT}.bin",
                       srampreld => "sram_preloader_$ArgList{PROJECT}.bin",
                       mempreld  => "mem_preloader_$ArgList{PROJECT}.bin",
                       lk      => "lk.bin",
                       boot    => "boot.img",
                       recovery   => "recovery.img",
                       secro     => "secro.img",
                       logo       => "logo.bin",
                       system    => "system.img",
                       cache      => "cache.img",
                       ubunturootfs => "ubunturootfs.img",
                       ubuntucustom => "ubuntucustom.img",
                       userdata    => "userdata.img",
                       custom     => "custom.img"
                      );

    #operation type
    %sepcial_operation_type = (
                               preloader => "BOOTLOADERS",
                               nvram     => "BINREGION",
                               proinfo  => "PROTECTED",
                               protect1 => "PROTECTED",
                               protect2 => "PROTECTED",
                               otp       => "RESERVED",
                               flashinfo   => "RESERVED",
                              );

    #1MB=1048576 byte align
    #8MB=8388608 byte align
    %AlignPartList = (
                      system => 8388608,
                      cache   => 8388608,
                      userdata => 8388608,
                      intsd     => 8388608
                     );
}

#****************************************************************************
# sub functions
#****************************************************************************
sub ShowInfo
{
    my $arglist = shift @_;
    printf "Partition Table Generator: Version=%.1f \nChangeHistory:\n%s\n", $Version, $ChangeHistory;
    print "*******************Arguments*********************\n";
    foreach my $key (sort keys %{$arglist})
    {
        if (!$arglist->{$key})
        {
            print "\t$key \t= -NO VALUE-\n";
        }
        else
        {
            print "\t$key \t= $arglist->{$key}\n";
        }
    }
    print "*******************Arguments*********************\n";
}

sub ReadPlainText
{
    my $filepath = shift @_;
    my $yaml;
    eval { $yaml = LoadFile($filepath); };
    if ($@)
    {
        &error_handler("Read YAML Partition layout fail", __FILE__, __LINE__, $@);
    }
    return $yaml;
}

sub GenPlainText
{
    my ($filepath, $list) = @_;
    if (-e $filepath)
    {
        my $dirpath = substr($filepath, 0, rindex($filepath, "/"));
        chmod(0777, $dirpath) or &error_handler("chmod 0777 $dirpath fail", __FILE__, __LINE__);
        if (!unlink $filepath)
        {
            &error_handler("remove $filepath fail ", __FILE__, __LINE__);
        }
    }
    eval { DumpFile($filepath, $list) };
    if ($@)
    {
        &error_handler("DumpFile from YAML fail ", __FILE__, __LINE__, $@);
    }
}

#****************************************************************************
# subroutine:  ReadExcelFile
# return:
#****************************************************************************

sub ReadExcelFile
{
    my $excelFilePath = shift @_;
    my @col_name      = ();
    my @col_sub_name  = ();
    my $PartitonBook  = Spreadsheet::ParseExcel->new()->Parse($excelFilePath);
    my $sheet         = $PartitonBook->Worksheet($ArgList{SHEET_NAME});
    PrintDependency($excelFilePath);
    if (!$sheet)
    {
        &error_handler("ptgen open sheet=$ArgList{SHEET_NAME} fail in $excelFilePath ", __FILE__, __LINE__);
    }
    my ($row_min, $row_max) = $sheet->row_range();    #$row_min=0
    my ($col_min, $col_max) = $sheet->col_range();    #$col_min=0
    my $row_cur = $row_min;
    foreach my $col_idx ($col_min .. $col_max)
    {
        push(@col_name,     &xls_cell_value($sheet, $row_cur,     $col_idx));
        push(@col_sub_name, &xls_cell_value($sheet, $row_cur + 1, $col_idx));
    }
    $row_cur += 2;
    foreach my $col_idx ($col_min .. $col_max)
    {
        foreach my $row_idx ($row_cur .. $row_max)
        {
            my $value = &xls_cell_value($sheet, $row_idx, $col_idx);
            if ($col_name[$col_idx - $col_min])
            {
                $partition_layout_raw[$row_idx - $row_cur]->{$col_name[$col_idx - $col_min]} = $value;
            }
            else
            {
                my $pre_col_value = $partition_layout_raw[$row_idx - $row_cur]->{$col_name[$col_idx - $col_min - 1]};
                if (!$value)
                {
                    $value = $pre_col_value;
                }
                $partition_layout_raw[$row_idx - $row_cur]->{$col_name[$col_idx - $col_min - 1]} = {$col_sub_name[$col_idx - $col_min - 1] => $pre_col_value, $col_sub_name[$col_idx - $col_min] => $value};
            }
        }
    }
}

sub ProcessRawPartitionLayoutData
{
    my @partition_layout_process = @{shift @_};
    my $partition_idx            = 0;
    #chose part attribute
    for ($partition_idx = 0 ; $partition_idx < @partition_layout_process ; $partition_idx++)
    {
        foreach my $col_name_idx (keys %{$partition_layout_process[$partition_idx]})
        {
            if (ref $partition_layout_process[$partition_idx]->{$col_name_idx})
            {
                if ($ArgList{TARGET_BUILD_VARIANT} eq "eng")
                {
                    $partition_layout_process[$partition_idx]->{$col_name_idx} = $partition_layout_process[$partition_idx]->{$col_name_idx}->{eng};
                }
                else
                {
                    $partition_layout_process[$partition_idx]->{$col_name_idx} = $partition_layout_process[$partition_idx]->{$col_name_idx}->{user};
                }
            }
        }
    }
    &debug_print_layout("./out/3.log", \@partition_layout_process);
    
    #modify size for some part by project
	my $board_config = &open_for_read("mediatek/config/$ArgList{PROJECT}/BoardConfig.mk");
    if ($board_config)
    {
        my $line;
        while (defined($line = <$board_config>))
        {
            foreach my $part (@partition_layout_process)
            {
                my $part_name = $part->{Partition_Name};
                if ($line =~ /\A\s*BOARD_MTK_${part_name}_SIZE_KB\s*:=\s*(\d+)/ || $line =~ /\A\s*BOARD_MTK_${part_name}_SIZE_KB\s*:=\s*(NA)/)
                {
                    $part->{Size_KB} = $1;
                    print "by project size $part_name = $1 KB\n";
                }
            }
        }
        close $board_config;
    }else{
    	print "This Project has no BoardConfig.mk \n";
    }
    if ($ArgList{FULL_PROJECT} ne $ArgList{PROJECT})
    {
        my $flavor_board_config = &open_for_read("mediatek/config/$ArgList{FULL_PROJECT}/BoardConfig.mk");
        if ($flavor_board_config)
        {
            my $line;
            while (defined($line = <$flavor_board_config>))
            {
                foreach my $part (@partition_layout_process)
                {
                    my $part_name = $part->{Partition_Name};
                    if ($line =~ /\A\s*BOARD_MTK_${part_name}_SIZE_KB\s*:=\s*(\d+)/ || $line =~ /\A\s*BOARD_MTK_${part_name}_SIZE_KB\s*:=\s*(NA)/)
                    {
                        $part->{Size_KB} = $1;
                        print "by flavor project size $part_name = $1 KB\n";
                    }
                }
            }
            close $flavor_board_config;
        }
    }else{
    	print "This flavor Project has no BoardConfig.mk \n";
    }
    &debug_print_layout("./out/5.log", \@partition_layout_process);
    
    #delete some partitions
    for ($partition_idx = 0 ; $partition_idx < @partition_layout_process ; $partition_idx++)
    {
        if ($partition_layout_process[$partition_idx]->{Partition_Name} eq "intsd")
        {
            if (($ArgList{EMMC_SUPPORT} eq "yes" && $ArgList{SHARED_SDCARD} eq "yes") || ($ArgList{EMMC_SUPPORT} ne "yes" && $ArgList{FAT_ON_NAND} ne "yes"))
            {
                splice @partition_layout_process, $partition_idx, 1;
                $partition_idx--;
            }
        }
        if ($partition_layout_process[$partition_idx]->{Partition_Name} eq "otp")
        {
            if ($ArgList{EMMC_SUPPORT} eq "yes" && $ArgList{EMMC_SUPPORT_OTP} ne "yes")
            {
                splice @partition_layout_process, $partition_idx, 1;
                $partition_idx--;
            }
        }
        if ($partition_layout_process[$partition_idx]->{Partition_Name} eq "custom")
        {
            if ($ArgList{CIP_SUPPORT} ne "yes")
            {
                splice @partition_layout_process, $partition_idx, 1;
                $partition_idx--;
            }
        }
        if ($partition_layout_process[$partition_idx]->{Size_KB} eq "NA")
        {
            splice @partition_layout_process, $partition_idx, 1;
            $partition_idx--;
        }
    }
    &debug_print_layout("./out/4.log", \@partition_layout_process);
  
    #calculate start_address of partition $partition_layout_process[$partition_idx]->{Start_Addr} by Byte
    #$partition_layout_process[$partition_idx]->{Start_Addr_Text} by byte in 0x format
    for ($partition_idx = @partition_layout_process - 1 ; $partition_idx >= 0 ; $partition_idx--)
    {
        if ($partition_layout_process[$partition_idx]->{Reserved} eq "Y")
        {
            if ($partition_idx != @partition_layout_process - 1)
            {
                $partition_layout_process[$partition_idx]->{Start_Addr} = $partition_layout_process[$partition_idx + 1]->{Start_Addr} + $partition_layout_process[$partition_idx]->{Size_KB} * 1024;
            }
            else
            {
                $partition_layout_process[$partition_idx]->{Start_Addr} = $partition_layout_process[$partition_idx]->{Size_KB} * 1024;
            }
        }
        else
        {
            $PMT_END_NAME = $partition_layout_process[$partition_idx]->{Partition_Name};
            last;
        }
    }

    for ($partition_idx = 0 ; $partition_idx < @partition_layout_process ; $partition_idx++)
    {
        if ($partition_layout_process[$partition_idx]->{Reserved} eq "N")
        {
            if ($partition_idx != 0 && $partition_layout_process[$partition_idx]->{Region} eq $partition_layout_process[$partition_idx - 1]->{Region})
            {
                my $st_addr = $partition_layout_process[$partition_idx - 1]->{Start_Addr} + $partition_layout_process[$partition_idx - 1]->{Size_KB} * 1024;

                #auto adjust to alignment
                if (exists $AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}})
                {
                    # print "got aligned partition $partition_layout_process[$partition_idx]->{Partition_Name}\n";
                    if ($st_addr % scalar($AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}}) != 0)
                    {
                        printf("Need adjust start address for %s, because it is 0x%x now. ", $partition_layout_process[$partition_idx]->{Partition_Name}, $st_addr % scalar($AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}}));
                        my $pad_size = $AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}} - $st_addr % scalar($AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}});
                        if ($pad_size % 1024 != 0)
                        {
                            &error_handler("pad size is not KB align,please review the size of $AlignPartList{$partition_layout_process[$partition_idx]->{Partition_Name}}", __FILE__, __LINE__);
                        }
                        $partition_layout_process[$partition_idx - 1]->{Size_KB} = $partition_layout_process[$partition_idx - 1]->{Size_KB} + $pad_size / 1024;
                        $st_addr = $partition_layout_process[$partition_idx - 1]->{Start_Addr} + $partition_layout_process[$partition_idx - 1]->{Size_KB} * 1024;
                        printf("pad size is 0x%x, and pre part [%s]size is 0x%x \n", $pad_size, $partition_layout_process[$partition_idx - 1]->{Partition_Name}, $partition_layout_process[$partition_idx - 1]->{Size_KB} * 1024);
                    }
                }
                $partition_layout_process[$partition_idx]->{Start_Addr} = $st_addr;
            }
            else
            {
                $partition_layout_process[$partition_idx]->{Start_Addr} = 0;
            }
            $partition_layout_process[$partition_idx]->{Start_Addr_Text} = sprintf("0x%x", $partition_layout_process[$partition_idx]->{Start_Addr});
        }
        else
        {
            if ($ArgList{EMMC_SUPPORT} eq "yes")
            {
                $partition_layout_process[$partition_idx]->{Start_Addr_Text} = sprintf("0xFFFF%04x", $partition_layout_process[$partition_idx]->{Start_Addr} / (128 * 1024));
            }
            else
            {
                $partition_layout_process[$partition_idx]->{Start_Addr_Text} = sprintf("0xFFFF%04x", $partition_layout_process[$partition_idx]->{Start_Addr} / (64 * $ArgList{PAGE_SIZE} * 1024));
            }
        }
        $Used_Size += $partition_layout_process[$partition_idx]->{Size_KB};
    }
    &debug_print_layout("./out/7.log", \@partition_layout_process);
    printf "\$Used_Size=0x%x KB = %d KB =%.2f MB\n", $Used_Size, $Used_Size, $Used_Size / (1024);

    #process AUTO flag and add index
    my $part;
    for ($partition_idx = 0 ; $partition_idx < @partition_layout_process ; $partition_idx++)
    {
    	$part=$partition_layout_process[$partition_idx];
        if ($part->{Download} eq "N")
        {
            $part->{Download_File} = "NONE";
        }
        elsif ($part->{Download_File} eq "AUTO")
        {
            if (exists $download_files{$part->{Partition_Name}})
            {
                $part->{Download_File} = $download_files{$part->{Partition_Name}};
            }
            else
            {
                if ($part->{Type} eq "Raw data")
                {
                    $part->{Download_File} = sprintf("%s.bin", lc($part->{Partition_Name}));
                }
                else
                {
                    $part->{Download_File} = sprintf("%s.img", lc($part->{Partition_Name}));
                }
            }
        }
        if ($part->{Operation_Type} eq "AUTO")
        {
            if (exists $sepcial_operation_type{$part->{Partition_Name}})
            {
                $part->{Operation_Type} = $sepcial_operation_type{$part->{Partition_Name}};
            }
            elsif ($part->{Reserved} eq "Y")
            {
                $part->{Operation_Type} = "RESERVED";
            }
            else
            {
                if ($part->{Download} eq "N")
                {
                    $part->{Operation_Type} = "INVISIBLE";
                }
                else
                {
                    $part->{Operation_Type} = "UPDATE";
                }
            }
        }
    }
    &debug_print_layout("./out/8.log", \@partition_layout_process);
    return \@partition_layout_process;
}


sub GenYAMLScatFile()
{
    my $ScatterFileFH = &open_for_rw($SCAT_FILE);
    my %Scatter_Info;
    my $iter;
    for ($iter = 0 ; $iter < @partition_layout_cooked ; $iter++)
    {
        my $part = $partition_layout_cooked[$iter];
        $Scatter_Info{$part->{Partition_Name}} = {
                                                  partition_index     => $iter,
                                                  physical_start_addr => $part->{Start_Addr_Text},
                                                  linear_start_addr   => $part->{Start_Addr_Text},
                                                  partition_size      => sprintf("0x%x", $part->{Size_KB} * 1024),
                                                  file_name           => $part->{Download_File},
                                                  operation_type      => $part->{Operation_Type}
                                                 };

        if ($ArgList{EMMC_SUPPORT} eq "yes")
        {
            if ($part->{Type} eq "Raw data")
            {
                $Scatter_Info{$part->{Partition_Name}}{type} = "NORMAL_ROM";
            }
            else
            {
                $Scatter_Info{$part->{Partition_Name}}{type} = "EXT4_IMG";
            }
        }
        else
        {
            if ($part->{Type} eq "Raw data")
            {
                $Scatter_Info{$part->{Partition_Name}}{type} = "NORMAL_ROM";
            }
            else
            {
                if ($ArgList{NAND_UBIFS_SUPPORT} eq "yes")
                {
                    $Scatter_Info{$part->{Partition_Name}}{type} = "UBI_IMG";
                }
                else
                {
                    $Scatter_Info{$part->{Partition_Name}}{type} = "YAFFS_IMG";
                }
            }
        }
        if ($part->{Partition_Name} eq "preloader")
        {
            $Scatter_Info{$part->{Partition_Name}}{type} = "SV5_BL_BIN";
        }

        if ($ArgList{EMMC_SUPPORT} eq "yes")
        {
            $Scatter_Info{$part->{Partition_Name}}{region} = $part->{Region};
        }
        else
        {
            $Scatter_Info{$part->{Partition_Name}}{region} = "NONE";
        }

        if ($ArgList{EMMC_SUPPORT} eq "yes")
        {
            $Scatter_Info{$part->{Partition_Name}}{storage} = "HW_STORAGE_EMMC";
        }
        else
        {
            $Scatter_Info{$part->{Partition_Name}}{storage} = "HW_STORAGE_NAND";
        }

        if ($part->{Reserved} eq "Y")
        {
            $Scatter_Info{$part->{Partition_Name}}{boundary_check} = "false";
            $Scatter_Info{$part->{Partition_Name}}{is_reserved}    = "true";
        }
        else
        {
            $Scatter_Info{$part->{Partition_Name}}{boundary_check} = "true";
            $Scatter_Info{$part->{Partition_Name}}{is_reserved}    = "false";
        }

        if ($part->{Download} eq "N")
        {
            $Scatter_Info{$part->{Partition_Name}}{is_download} = "false";
        }
        else
        {
            $Scatter_Info{$part->{Partition_Name}}{is_download} = "true";
        }
    }
    my $Head1 = <<"__TEMPLATE";
############################################################################################################
#
#  General Setting 
#    
############################################################################################################
__TEMPLATE

    my $Head2 = <<"__TEMPLATE";
############################################################################################################
#
#  Layout Setting
#
############################################################################################################
__TEMPLATE

    my ${FirstDashes}       = "- ";
    my ${FirstSpaceSymbol}  = "  ";
    my ${SecondSpaceSymbol} = "      ";
    my ${SecondDashes}      = "    - ";
    my ${colon}             = ": ";
    print $ScatterFileFH $Head1;
    print $ScatterFileFH "${FirstDashes}general${colon}MTK_PLATFORM_CFG\n";
    print $ScatterFileFH "${FirstSpaceSymbol}info${colon}\n";
    print $ScatterFileFH "${SecondDashes}config_version${colon}V1.1.2\n";
    print $ScatterFileFH "${SecondSpaceSymbol}platform${colon}$ArgList{PLATFORM}\n";
    print $ScatterFileFH "${SecondSpaceSymbol}project${colon}$ArgList{FULL_PROJECT}\n";

    if ($ArgList{EMMC_SUPPORT} eq "yes")
    {
        print $ScatterFileFH "${SecondSpaceSymbol}storage${colon}EMMC\n";
        print $ScatterFileFH "${SecondSpaceSymbol}boot_channel${colon}MSDC_0\n";
        printf $ScatterFileFH ("${SecondSpaceSymbol}block_size${colon}0x%x\n", 2 * 64 * 1024);
    }
    else
    {
        print $ScatterFileFH "${SecondSpaceSymbol}storage${colon}NAND\n";
        print $ScatterFileFH "${SecondSpaceSymbol}boot_channel${colon}NONE\n";
        printf $ScatterFileFH ("${SecondSpaceSymbol}block_size${colon}0x%x\n", $ArgList{PAGE_SIZE} * 64 * 1024);
    }
    print $ScatterFileFH $Head2;
    foreach my $part (@partition_layout_cooked)
    {
        print $ScatterFileFH "${FirstDashes}partition_index${colon}SYS$Scatter_Info{$part->{Partition_Name}}{partition_index}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}partition_name${colon}$part->{Partition_Name}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}file_name${colon}$Scatter_Info{$part->{Partition_Name}}{file_name}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}is_download${colon}$Scatter_Info{$part->{Partition_Name}}{is_download}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}type${colon}$Scatter_Info{$part->{Partition_Name}}{type}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}linear_start_addr${colon}$Scatter_Info{$part->{Partition_Name}}{linear_start_addr}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}physical_start_addr${colon}$Scatter_Info{$part->{Partition_Name}}{physical_start_addr}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}partition_size${colon}$Scatter_Info{$part->{Partition_Name}}{partition_size}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}region${colon}$Scatter_Info{$part->{Partition_Name}}{region}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}storage${colon}$Scatter_Info{$part->{Partition_Name}}{storage}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}boundary_check${colon}$Scatter_Info{$part->{Partition_Name}}{boundary_check}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}is_reserved${colon}$Scatter_Info{$part->{Partition_Name}}{is_reserved}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}operation_type${colon}$Scatter_Info{$part->{Partition_Name}}{operation_type}\n";
        print $ScatterFileFH "${FirstSpaceSymbol}reserve${colon}0x00\n\n";
    }
    close $ScatterFileFH;
}

#****************************************************************************************
# subroutine:  GenPartSizeFile;
# return:
#****************************************************************************************

sub GenPartSizeFile_iniFile
{
    my ($part_size, $inifilepath) = @_;
    my $part_size_fh = &open_for_rw($part_size);

    my $Total_Size = 512 * 1024 * 1024;    #Hard Code 512MB for 4+2 project FIX ME!!!!!
    my $temp;
    my $index = 0;
    my $vol_size;
    my $min_ubi_vol_size;
    my $PEB;
    my $LEB;
    my $IOSIZE;

    if ($ArgList{NAND_UBIFS_SUPPORT} eq "yes" && $ArgList{EMMC_SUPPORT} ne "yes")
    {
        $IOSIZE           = $ArgList{PAGE_SIZE} * 1024;
        $PEB              = $IOSIZE * 64;
        $LEB              = $IOSIZE * 62;
        $min_ubi_vol_size = $PEB * 28;
        printf $part_size_fh ("BOARD_UBIFS_MIN_IO_SIZE:=%d\n",             $IOSIZE);
        printf $part_size_fh ("BOARD_FLASH_BLOCK_SIZE:=%d\n",              $PEB);
        printf $part_size_fh ("BOARD_UBIFS_VID_HDR_OFFSET:=%d\n",          $IOSIZE);
        printf $part_size_fh ("BOARD_UBIFS_LOGICAL_ERASEBLOCK_SIZE:=%d\n", $LEB);

        if ($ArgList{COMBO_NAND_SUPPORT} eq "yes")
        {
            my $combo_nand_kernel = &open_for_rw($COMBO_NAND_KERNELH);
            printf $combo_nand_kernel ("#define COMBO_NAND_BLOCK_SIZE %d\n", $PEB);
            printf $combo_nand_kernel ("#define COMBO_NAND_PAGE_SIZE %d\n",  $IOSIZE);
            close $combo_nand_kernel;

            my $combo_nand_tool = &open_for_rw($COMBO_NAND_TOOLH);
            printf $combo_nand_tool ("#define COMBO_NAND_BLOCK_SIZE %d\n", $PEB);
            printf $combo_nand_tool ("#define COMBO_NAND_PAGE_SIZE %d\n",  $IOSIZE);
            close $combo_nand_tool;
        }
        $Total_Size = $Total_Size - $Used_Size - $PEB * 2;    #PMT need 2 block
        print "In UBIFS, auto size partition have byte\n";
    }

    foreach my $part (@partition_layout_cooked)
    {
        if ($part->{Type} eq "EXT4" || $part->{Type} eq "FAT")
        {
            $temp = $part->{Size_KB} * 1024;
            if ($part->{Partition_Name} eq "userdata")
            {
                $temp -= 1024 * 1024;
            }

             printf $part_size_fh ("BOARD_%sIMAGE_PARTITION_SIZE:=%d\n",uc($part->{Partition_Name}),$temp);
        }

        if ($ArgList{NAND_UBIFS_SUPPORT} eq "yes" && $part->{Type} eq "UBIFS" && $ArgList{EMMC_SUPPORT} ne "yes")
        {
            my $part_name = lc($part->{Partition_Name});
            if ($part->{Size_KB} == 0)
            {
                $part->{Size_KB} = $Total_Size / 1024;
            }

            # UBI reserve 6 block
            $vol_size = (int($part->{Size_KB} * 1024 / ${PEB}) - 6) * $LEB;
            if ($min_ubi_vol_size > $part->{Size_KB} * 1024)
            {
                &error_handler("$part->{Partition_Name} is too small, UBI partition is at least $min_ubi_vol_size byte, Now it is $part->{Size_KB} KiB", __FILE__, __LINE__);
            }
            my $inifd = &open_for_rw("$inifilepath/ubi_${part_name}.ini");
            print $inifd "[ubifs]\n";
            print $inifd "mode=ubi\n";
            print $inifd "image=$inifilepath/ubifs.${part_name}.img\n";
            print $inifd "vol_id=0\n";
            print $inifd "vol_size=$vol_size\n";
            print $inifd "vol_type=dynamic\n";

            if (exists $kernel_alias{$part->{Partition_Name}})
            {
                print $inifd "vol_name=$kernel_alias{$part->{Partition_Name}}\n";
            }
            else
            {
                print $inifd "vol_name=${part_name}\n";
            }
            if ($part->{Partition_Name} ne "secro")
            {
                print $inifd "vol_flags=autoresize\n";
            }
            print $inifd "vol_alignment=1\n";
            close $inifd;
            printf $part_size_fh ("BOARD_UBIFS_%s_MAX_LOGICAL_ERASEBLOCK_COUNT:=%d\n", $part->{Partition_Name}, int((${vol_size} / $LEB) * 1.1));
        }
    }
    close $part_size_fh;
}

#****************************************************************************
# subroutine:  error_handler
# input:       $error_msg:     error message
#****************************************************************************
sub error_handler
{
    my ($error_msg, $file, $line_no, $sys_msg) = @_;
    if (!$sys_msg)
    {
        $sys_msg = $!;
    }
    print "Fatal error: $error_msg <file: $file,line: $line_no> : $sys_msg";
    die;
}

#****************************************************************************
# subroutine:  copyright_file_header_for_c
# return:      file header -- copyright
#****************************************************************************
sub copyright_file_header_for_c
{
    my $template = <<"__TEMPLATE";
__TEMPLATE

    return $template;
}

#****************************************************************************
# subroutine:  copyright_file_header_for_shell
# return:      file header -- copyright
#****************************************************************************
sub copyright_file_header_for_shell()
{
    my $template = <<"__TEMPLATE";
__TEMPLATE

    return $template;
}

#****************************************************************************************
sub GenKernel_PartitionC()
{
    my $temp;
    my $kernel_h_fd = &open_for_rw($KernelH);
    print $kernel_h_fd &copyright_file_header_for_c();
    my $template = <<"__TEMPLATE";

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include "partition_define.h"


/*=======================================================================*/
/* NAND PARTITION Mapping                                                  */
/*=======================================================================*/
static struct mtd_partition g_pasStatic_Partition[] = {

__TEMPLATE
    print $kernel_h_fd $template;
    foreach my $part (@partition_layout_cooked)
    {
        last if ($part->{Reserved} eq "Y");
        print $kernel_h_fd "\t{\n";
        if (exists $kernel_alias{$part->{Partition_Name}})
        {
            print $kernel_h_fd "\t\t.name = \"$kernel_alias{$part->{Partition_Name}}\",\n";
        }
        else
        {
            printf $kernel_h_fd "\t\t.name = \"%s\",\n", lc($part->{Partition_Name});
        }
        if ($part->{Start_Addr} == 0)
        {
            print $kernel_h_fd "\t\t.offset = 0x0,\n";
        }
        else
        {
            print $kernel_h_fd "\t\t.offset = MTDPART_OFS_APPEND,\n";
        }
        if ($part->{Size_KB} != 0)
        {
            print $kernel_h_fd "\t\t.size = PART_SIZE_$part->{Partition_Name},\n";
        }
        else
        {
            print $kernel_h_fd "\t\t.size = MTDPART_SIZ_FULL,\n";
        }
        if ($part->{Partition_Name} eq "PRELOADER" || $part->{Partition_Name} eq "DSP_BL" || $part->{Partition_Name} eq "UBOOT" || $part->{Partition_Name} eq "SEC_RO")
        {
            print $kernel_h_fd "\t\t.mask_flags  = MTD_WRITEABLE,\n";
        }
        print $kernel_h_fd "\t},\n";
    }
    print $kernel_h_fd "};\n";

    $template = <<"__TEMPLATE";
#define NUM_PARTITIONS ARRAY_SIZE(g_pasStatic_Partition)
extern int part_num;	// = NUM_PARTITIONS;
__TEMPLATE
    print $kernel_h_fd $template;
    close $kernel_h_fd;

}


#****************************************************************************************
# subroutine:  GenLK_PartitionC
# return:
#****************************************************************************************
sub GenLK_PartitionC()
{
    my $iter=0;
    my $lk_part_c_fh = &open_for_rw($LK_PartitionC);
    print $lk_part_c_fh &copyright_file_header_for_c();
    print $lk_part_c_fh "#include \"platform/partition.h\"\n";
    print $lk_part_c_fh "\n\nstruct part_name_map g_part_name_map[] = {\n";
    foreach my $part (@partition_layout_cooked)
    {
        next if ($part->{Partition_Name} eq "pgpt");
        last if ($part->{Reserved} eq "Y");
        printf $lk_part_c_fh (
                              "\t{\"%s\",\t\"%s\",\t\"%s\",\t%d,\t%d,\t%d},\n",
                              ($part->{Partition_Name}),
                              ($part->{Partition_Name}),
                              lc($part->{Type}),
                              $iter,
                              ($part->{FastBoot_Erase} eq "Y")    ? (1) : (0),
                              ($part->{FastBoot_Download} eq "Y") ? (1) : (0)
                             );
        $iter++;    
    }
    print $lk_part_c_fh "};\n";
    close $lk_part_c_fh;
}


#delete some obsolete file
sub clear_files
{
	my @ObsoleteFile;
	
    opendir (DIR,"mediatek/custom/$ArgList{PROJECT}/common");
	push @ObsoleteFile,readdir(DIR);
    close DIR;
    
    my $custom_out_prefix_obsolete  = "mediatek/custom/$ENV{PROJECT}";
    my $configs_out_prefix_obsolete = "mediatek/config/$ENV{PROJECT}";    
    push @ObsoleteFile, "$configs_out_prefix_obsolete/configs/EMMC_partition_size.mk";
    push @ObsoleteFile, "$configs_out_prefix_obsolete/configs/partition_size.mk";;
    push @ObsoleteFile, "$custom_out_prefix_obsolete/preloader/cust_part.c";
    push @ObsoleteFile, "mediatek/kernel/drivers/dum-char/partition_define.c";
	push @ObsoleteFile, "mediatek/external/mtd-utils/ubi-utils/combo_nand.h";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/kernel/core/src/partition.h";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/lk/inc/mt_partition.h";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/lk/partition.c";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/common/pmt.h";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/common/partition_define.h";
	push @ObsoleteFile, "$custom_out_prefix_obsolete/common/combo_nand.h";

	foreach my $filepath (@ObsoleteFile){
   		if (-e $filepath && !-d $filepath){
   	    	if (!unlink $filepath)
        	{
            	&error_handler("remove $filepath fail ", __FILE__, __LINE__);
        	}else{
   				print "clean $filepath: clean done \n"
   			}
  		}
	}
}
#****************************************************************************************
# subroutine:  xls_cell_value
# return:      Excel cell value no matter it's in merge area or not, and in windows or not
# input:       $Sheet:  Specified Excel Sheet
# input:       $row:    Specified row number
# input:       $col:    Specified column number
#****************************************************************************************
sub xls_cell_value
{
    my ($Sheet, $row, $col) = @_;
    my $cell = $Sheet->get_cell($row, $col);
    if (defined $cell)
    {
        #return $cell->Value();
        return $cell->unformatted();
    }
    else
    {
        &error_handler("excel read fail,(row=$row,col=$col) undefine", __FILE__, __LINE__);
    }
}

sub open_for_rw
{
    my $filepath = shift @_;
    if (-e $filepath)
    {
        chmod(0777, $filepath) or &error_handler("chmod 0777 $filepath fail", __FILE__, __LINE__);
        if (!unlink $filepath)
        {
            &error_handler("remove $filepath fail ", __FILE__, __LINE__);
        }
    }
    else
    {
        my $dirpath = substr($filepath, 0, rindex($filepath, "/"));
        eval { mkpath($dirpath) };
        if ($@)
        {
            &error_handler("Can not make dir $dirpath", __FILE__, __LINE__, $@);
        }
    }
    open my $filehander, "> $filepath" or &error_handler(" Can not open $filepath for read and write", __FILE__, __LINE__);
    push @GeneratedFile, $filepath;
    return $filehander;
}

sub open_for_read
{
    my $filepath = shift @_;
    if (-e $filepath)
    {
        chmod(0777, $filepath) or &error_handler("chmod 777 $filepath fail", __FILE__, __LINE__);
    }
    else
    {
        printf ("No such file : %s, file: %s, line:%s\n",$filepath,__FILE__,__LINE__);
        return undef;
    }
    open my $filehander, "< $filepath" or &error_handler(" Can not open $filepath for read", __FILE__, __LINE__);
    return $filehander;
}

sub debug_print_layout
{
    if ($DebugPrint eq "yes")
    {
        my ($filepath, $list) = @_;
        my $fd = &open_for_rw($filepath);
        print $fd Dump($list);
        close $fd;
    }
}

sub print_rm_script
{
    my $out = "./remove_ptgen_autogen_files.pl";
    my $fd  = &open_for_rw($out);

    print $fd "#!/usr/local/bin/perl -w\n";
    foreach my $file (@GeneratedFile)
    {
        if ($file ne $out)
        {
            printf $fd "system \"rm -fr --verbose %s\";\n", $file;
        }
    }
    print $fd "system \"rm -fr --verbose mediatek/custom/out \";\n";
    print $fd "system \"rm -fr --verbose mediatek/config/out \";\n";
    print $fd "system \"rm -fr --verbose out\";\n";
    chmod(0777, $out);
    close $fd;
}
