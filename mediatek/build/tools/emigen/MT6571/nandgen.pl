#!/usr/local/bin/perl -w

use File::Basename;
#use strict;
my $LOCAL_PATH;
BEGIN
{
    $LOCAL_PATH = dirname($0);
}

use lib "$LOCAL_PATH/../../Spreadsheet";
use lib "$LOCAL_PATH/../../";
require 'ParseExcel.pm';
use pack_dep_gen;

#****************************************************************************
# Customization Field
#****************************************************************************
my $DebugPrint = "yes";
my $VersionAndChanglist = "2.0 support autodetect NAND ID and improvement\n";
my $PLATFORM = $ENV{MTK_PLATFORM};# MTxxxx
my $PROJECT = $ENV{PROJECT};
my $MLC_NAND_SUPPORT = $ENV{MTK_MLC_NAND_SUPPORT};
my $FULL_PROJECT = $ENV{FULL_PROJECT};
my $PAGE_SIZE = $ENV{MTK_NAND_PAGE_SIZE};
my $COMBO_NAND_SUPPORT = $ENV{MTK_COMBO_NAND_SUPPORT};
my $SPI_NAND_SUPPORT = $ENV{MTK_SPI_NAND_SUPPORT};
my $psize=2048;
my $bsize=64;
my $csize=512;
my @PartNumbers; #Part numbers got from custom_MemoryDevice.h
my @MemoryDeviceList;
my $MTK_NANDGEN_OUT_DIR = "$ENV{MTK_ROOT_OUT}/NANDGEN/common";
print "\$PLATFORM=$PLATFORM,\$PROJECT=$PROJECT,\$FULL_PROJECT=$FULL_PROJECT,\$PAGE_SIZE=$PAGE_SIZE\n";
PrintDependModule($0);
#****************************************************************************
# Main Thread Field
#****************************************************************************
    &ReadCustomMemoryDeviceFile();
    &ReadNANDExcelFile();
    &GenNANDHeaderFile();
    print "nandgen done\n";
    exit 0;


#****************************************************************************
# Subfunction Field
#****************************************************************************

sub ReadCustomMemoryDeviceFile
{
    my $CUSTOM_MEMORYDEVICE_H_NAME  = "mediatek/custom/$PROJECT/preloader/inc/custom_MemoryDevice.h";
    if (-e $CUSTOM_MEMORYDEVICE_H_NAME) {
        `chmod 777 $CUSTOM_MEMORYDEVICE_H_NAME`;
    }
    open (CUSTOM_MEMORYDEVICE_H_NAME, "<$CUSTOM_MEMORYDEVICE_H_NAME") or &error_handler("Nandgen open CUSTOM_MEMORYDEVICE_H_NAME fail!\n", __FILE__, __LINE__);
    PrintDependency($CUSTOM_MEMORYDEVICE_H_NAME);
    my $iter = 0;
    my %hash;
    while (<CUSTOM_MEMORYDEVICE_H_NAME>) {
        my($line) = $_;
        chomp($line);
        #if ($line =~ /^#define\sNAND_PART_NUMBER\[[0-9]\]/) {
        if($MLC_NAND_SUPPORT eq "yes")
        {
            if ($line =~ /^#define\s(NAND_PART_NUMBER\[[0-9]\])/) {
#               print "$'\n";
                $hash{$1}++;
                $PartNumbers[$iter] = $'; #'
                $PartNumbers[$iter] =~ s/\s+//g;
                if ($PartNumbers[$iter] =~ /(.*)\/\/(.*)/) { #skip //
                    $PartNumbers[$iter] =$1;
                }
                #print "$PartNumbers[$iter] \n";
                $iter ++;
            }
        }
        else
        {
        if ($line =~ /^#define\s(CS_PART_NUMBER\[[0-9]\])/) {
#           print "$'\n";
            $hash{$1}++;
                $PartNumbers[$iter] = $'; #'
            $PartNumbers[$iter] =~ s/\s+//g;
            if ($PartNumbers[$iter] =~ /(.*)\/\/(.*)/) { #skip //
                $PartNumbers[$iter] =$1;
            }
            #print "$PartNumbers[$iter] \n";
            $iter ++;
        }	
    }
    }
    #check CS_PART_NUMBER[*] field correctness
    while(($key,$value)=each(%hash))
    {
        &error_handler("Part Number: $key duplicates in custom_MemoryDevice.h\n", __FILE__, __LINE__) if($value >= 2);
    }
    my @array = sort keys(%hash);
    for($i=0;$i<@array;$i++)
    {   
        &error_handler("CS_PART_NUMBER[$i] order error\n", __FILE__, __LINE__) unless( $array[$i] =~ /\[$i\]/);
    }
   
}

sub ReadNANDExcelFile
{	my @all_column=[];#=qw(Vendor Part_Number Nand_ID AddrCycle IOWidth TotalSize_MB BlockSize_KB PageSize_B SpareSize_B Timing S_Timing S_Timing1 Freq  CacheRead RandomRead Set_Feature	Get_Feature	Int_Address	Sync	Async	Rrtry_Addr	Rrtry_Number	Rrtry_Default	Rrtry_Start	PP_Function);
    my $MEMORY_DEVICE_LIST_XLS = "mediatek/build/tools/emigen/${PLATFORM}/MemoryDeviceList_${PLATFORM}.xls";

    my $SheetName = "NAND";
    if($SPI_NAND_SUPPORT eq "yes")
    {
        $SheetName = "SPI_NAND";
    }
    if($MLC_NAND_SUPPORT eq "yes")
    {
    	$SheetName = "NAND_MLC";
    }
    my $parser = Spreadsheet::ParseExcel->new();
    my $Book = $parser->Parse($MEMORY_DEVICE_LIST_XLS);
    PrintDependency($MEMORY_DEVICE_LIST_XLS);
    my $sheet = $Book->Worksheet($SheetName);
    my %COLUMN_LIST;
    my $tmp;
    my $row;
    my $col;

    for($col = 0, $row = 0,$tmp = &xls_cell_value($sheet, $row, $col); $tmp; $col++, $tmp = &xls_cell_value($sheet, $row, $col))
    {
        $COLUMN_LIST{$tmp} = $col;
    }
    @all_column=sort (keys(%COLUMN_LIST));
    print "@all_column\n";
	
    for($row = 1,$tmp = &xls_cell_value($sheet, $row, $COLUMN_LIST{Part_Number});$tmp;$row++,$tmp = &xls_cell_value($sheet, $row, $COLUMN_LIST{Part_Number}))
    {
        foreach $i (@all_column){
            $MemoryDeviceList[$row-1]{$i}=&xls_cell_value($sheet, $row, $COLUMN_LIST{$i});
        }
    }

    if($DebugPrint eq "yes"){
        print "~~~~~~~~~EXCEL INFO~~~~~~~~~~~\n";
        for($index=0;$index<@MemoryDeviceList;$index++){
            print "index:$index\n";
            foreach $i (@all_column){
                printf ("%-15s:%-20s ",$i,$MemoryDeviceList[$index]->{$i});
            }
            print "\n";
        }
        print "~~~~~~~~~There are $index Nand Chips~~~~~~~~~~~\n";
    }
}

sub GenNANDHeaderFile()
{
    my $NAND_LIST_DEFINE_H_NAME = "$MTK_NANDGEN_OUT_DIR/nand_device_list.h";
    if($SPI_NAND_SUPPORT eq "yes")
    {
        $NAND_LIST_DEFINE_H_NAME = "$MTK_NANDGEN_OUT_DIR/snand_device_list.h";
    }
	my %InFileChip;
	my $Longest_ID=0;
	my $Chip_count=0;
    if(!-e $MTK_NANDGEN_OUT_DIR)
    {
        `mkdir -p $MTK_NANDGEN_OUT_DIR `;
    }
    if(-e $NAND_LIST_DEFINE_H_NAME)
    {
        `chmod 777 $NAND_LIST_DEFINE_H_NAME`;
    }
    if($COMBO_NAND_SUPPORT eq "yes")
    {
        check_PartNumber(); #check PartNumber valid in custom_MemoryDevice.h
    }

    for($iter=0;$iter<@MemoryDeviceList;$iter++){
        if($SPI_NAND_SUPPORT eq "yes") 
        {
#           if(search_PartNumber( $MemoryDeviceList[$iter]->{Part_Number} ) )
            {
                my $ID_length=0;
                my $advance_option=0;
                my $ID=$MemoryDeviceList[$iter]->{Nand_ID};
                if(!exists($InFileChip{$ID})){
                    if(length($ID)%2){
                        print "The chip:$ID have wrong number!\n";
                    }else{	
                        $ID_length=length($ID)/2-1;
                        if($ID_length > $Longest_ID){
                            $Longest_ID = $ID_length;
                        }
                        #print "\$Longest_ID=$Longest_ID\n";
                        $InFileChip{$ID}={'Index'=>$iter,'IDLength'=>$ID_length,'ADV_OPT'=>$advance_option};
                        $Chip_count++;
                    }
                }else{
                    print "There more than 1 chip have the ID:$MemoryDeviceList[$iter]->{Nand_ID},you should modify the excel\n";
                }
	    }	
        }
        elsif($COMBO_NAND_SUPPORT eq "yes") #if Combo nand support is enabled, only get MDL setting per custom_MemoryDevice.h part numbers
        {
            if(search_PartNumber( $MemoryDeviceList[$iter]->{Part_Number} ) )
            {
                my $ID_length=0;
                my $advance_option=0;
                my $ID=$MemoryDeviceList[$iter]->{Nand_ID};
                if(!exists($InFileChip{$ID})){
                    if(length($ID)%2){
                        print "The chip:$ID have wrong number!\n";
                    }else{	
                        $ID_length=length($ID)/2-1;
                        if($ID_length > $Longest_ID){
                            $Longest_ID = $ID_length;
                        }
                        #print "\$Longest_ID=$Longest_ID\n";
                        if ($MemoryDeviceList[$iter]->{CacheRead} eq "YES")
                        {
                            $advance_option += 2;
                        }
                        if ($MemoryDeviceList[$iter]->{RandomRead} eq "YES")
                        {
                            $advance_option += 1;
                        }
                        if($MemoryDeviceList[$iter]->{PageSize_B} > $psize)
                        {
                        	$psize = $MemoryDeviceList[$iter]->{PageSize_B};
                        }
                        if((($MemoryDeviceList[$iter]->{BlockSize_KB}*1024)/$MemoryDeviceList[$iter]->{PageSize_B}) > $bsize)
                        {
                        	$bsize = (($MemoryDeviceList[$iter]->{BlockSize_KB}*1024)/$MemoryDeviceList[$iter]->{PageSize_B});
                        	#print "~~~~~~~~~~~~~~~~~~bsize = $bsize, $MemoryDeviceList[$iter]->{BlockSize_KB} ,$MemoryDeviceList[$iter]->{PageSize_B}~~~~~~~~~~~~~";
                        }
                        if($MemoryDeviceList[$iter]->{TotalSize_MB} > $csize)
                        {
                        	$csize = $MemoryDeviceList[$iter]->{TotalSize_MB};
                        }
                        if ($MemoryDeviceList[$iter]->{IOWidth} eq "IO_8BIT" || $MemoryDeviceList[$iter]->{IOWidth} eq 8 )
	        {
		$chip_IO = "IO_8BIT";
	        }
	        elsif ($MemoryDeviceList[$iter]->{IOWidth} eq "IO_16BIT" || $MemoryDeviceList[$iter]->{IOWidth} eq 16 )
	        {
		$chip_IO = "IO_16BIT";
	        }
	        else
	        {
		$chip_IO = $MemoryDeviceList[$iter]->{IOWidth};
	        }
              if ($MemoryDeviceList[$iter]->{FeatureSet} eq "NO" || $MemoryDeviceList[$iter]->{FeatureSet} eq "VEND_NONE")
	        {
		          $feature = "VEND_NONE";
	        }
	        else
	        {
		          if($MLC_NAND_SUPPORT eq "yes"){
								$feature = "VEND_".uc($MemoryDeviceList[$iter]->{Vendor})
							}else{
								$feature = "VEND_NONE";
							}	
	        }
	        if($MLC_NAND_SUPPORT eq "yes"){
		        $sector = $MemoryDeviceList[$iter]->{SectorSize_B};
		      }else{
		      	$sector = 512;
		      }
	        $InFileChip{$ID}={'Index'=>$iter,'IDLength'=>$ID_length,'ADV_OPT'=>$advance_option,'IO'=>$chip_IO,'FEATURE'=>$feature,'SECTOR'=>$sector};
	        $Chip_count++;
                    }
                }else{
                    print "There more than 1 chip have the ID:$MemoryDeviceList[$iter]->{Nand_ID},you should modify the excel\n";
                }
	    }	
        }
        else
        {
           if(($PAGE_SIZE eq "4K" && $MemoryDeviceList[$iter]->{PageSize_B} eq 4096) || ($PAGE_SIZE eq "2K" && $MemoryDeviceList[$iter]->{PageSize_B} eq 2048))
            {
                my $ID_length=0;
                my $advance_option=0;
			my $chip_IO;
			my $feature;
			my $sector;
                my $ID=$MemoryDeviceList[$iter]->{Nand_ID};
                if(!exists($InFileChip{$ID})){
                    if(length($ID)%2){
                        print "The chip:$ID have wrong number!\n";
                    }else{	
                        $ID_length=length($ID)/2-1;
                        if($ID_length > $Longest_ID){
                            $Longest_ID = $ID_length;
                        }
                        #print "\$Longest_ID=$Longest_ID\n";
                        if ($MemoryDeviceList[$iter]->{CacheRead} eq "YES")
                        {
                            $advance_option += 2;
                        }
                        if ($MemoryDeviceList[$iter]->{RandomRead} eq "YES")
                        {
                            $advance_option += 1;
                        }
					if ($MemoryDeviceList[$iter]->{IOWidth} eq "IO_8BIT" || $MemoryDeviceList[$iter]->{IOWidth} eq 8 )
						{
							$chip_IO = "IO_8BIT";
						}
						elsif ($MemoryDeviceList[$iter]->{IOWidth} eq "IO_16BIT" || $MemoryDeviceList[$iter]->{IOWidth} eq 16 )
						{
							$chip_IO = "IO_16BIT";
						}
						else
						{
							$chip_IO = $MemoryDeviceList[$iter]->{IOWidth};
						}
					  if ($MemoryDeviceList[$iter]->{FeatureSet} eq "NO" || $MemoryDeviceList[$iter]->{FeatureSet} eq "VEND_NONE")
						{
							$feature = "VEND_NONE";
						}
						else
						{
							if($MLC_NAND_SUPPORT eq "yes"){
								$feature = "VEND_".uc($MemoryDeviceList[$iter]->{Vendor})
							}else{
								$feature = "VEND_NONE";
							}	
						}
						if($MLC_NAND_SUPPORT eq "yes")
						{
							$sector = $MemoryDeviceList[$iter]->{SectorSize_B};
            }else{
            	$sector = 512;
            }
						$InFileChip{$ID}={'Index'=>$iter,'IDLength'=>$ID_length,'ADV_OPT'=>$advance_option,'IO'=>$chip_IO,'FEATURE'=>$feature,'SECTOR'=>$sector};
                        $Chip_count++;
                    }
                }else{
                    print "There more than 1 chip have the ID:$MemoryDeviceList[$iter]->{Nand_ID},you should modify the excel\n";
                }
            }
        }
		
    }
    open(FD, ">$NAND_LIST_DEFINE_H_NAME") or &error_handler("open $NAND_LIST_DEFINE_H_NAME fail\n", __FILE__, __LINE__);
    if($SPI_NAND_SUPPORT eq "yes")
    {
    print FD "\n#ifndef __SNAND_DEVICE_LIST_H__\n#define __SNAND_DEVICE_LIST_H__\n\n";
    print FD "#define SNAND_MAX_ID\t\t$Longest_ID\n";
    print FD "#define SNAND_CHIP_CNT\t\t$Chip_count\n";
    print FD &struct_snandflashdev_info_define();
    print FD "static const snand_flashdev_info gen_snand_FlashTable[]={\n";
    }
    else
    {
    print FD "\n#ifndef __NAND_DEVICE_LIST_H__\n#define __NAND_DEVICE_LIST_H__\n\n";
    print FD "#define NAND_MAX_ID\t\t$Longest_ID\n";
    print FD "#define CHIP_CNT\t\t$Chip_count\n";
	print FD "#define P_SIZE\t\t$psize\n";
	print FD "#define P_PER_BLK\t\t$bsize\n";
	print FD "#define C_SIZE\t\t$csize\n";
    print FD &struct_flashdev_info_define();
    print FD "static const flashdev_info gen_FlashTable[]={\n";
    }
    foreach $ID (sort by_length (keys(%InFileChip))){
        my $it=$InFileChip{$ID}->{Index};
        #creat ID arry string
        my @ID_arry=($ID =~ m/([\dA-Fa-f]{2})/gs);
        my $arry_str="{";
        for($i=0;$i<$Longest_ID;$i++){
            if($i<@ID_arry){
                $arry_str.="0x$ID_arry[$i]";
            }else{
                $arry_str.="0x00";
            }
            if($i<$Longest_ID-1){
                $arry_str.=",";
            }
        }
        $arry_str.="}";
        print "ID=$arry_str\n";
        #print string to file
        if($SPI_NAND_SUPPORT eq "yes")
        {
            print FD "\t{$arry_str, $InFileChip{$ID}->{IDLength},$MemoryDeviceList[$it]->{TotalSize_MB},$MemoryDeviceList[$it]->{BlockSize_KB},$MemoryDeviceList[$it]->{PageSize_B},$MemoryDeviceList[$it]->{SpareSize_B},$MemoryDeviceList[$it]->{SNF_DLY_CTL1},$MemoryDeviceList[$it]->{SNF_DLY_CTL2},$MemoryDeviceList[$it]->{SNF_DLY_CTL3},$MemoryDeviceList[$it]->{SNF_DLY_CTL4},$MemoryDeviceList[$it]->{SNF_MISC_CTL},$MemoryDeviceList[$it]->{DRIVING},";
            printf FD ("\"%.30s\"",$MemoryDeviceList[$it]->{Part_Number});
            print FD ",$MemoryDeviceList[$it]->{Spare_Format}";
            print FD ",$MemoryDeviceList[$it]->{Advanced_Mode}}, \n";
        }
		elsif($MLC_NAND_SUPPORT eq "yes")
		{
			print FD "\t{$arry_str, $InFileChip{$ID}->{IDLength},$MemoryDeviceList[$it]->{AddrCycle},$InFileChip{$ID}->{IO},$MemoryDeviceList[$it]->{TotalSize_MB},$MemoryDeviceList[$it]->{BlockSize_KB},$MemoryDeviceList[$it]->{PageSize_B},$MemoryDeviceList[$it]->{SpareSize_B},$MemoryDeviceList[$it]->{Timing}, $MemoryDeviceList[$it]->{S_Timing},$MemoryDeviceList[$it]->{S_Timing1},$MemoryDeviceList[$it]->{Freq},$InFileChip{$ID}->{FEATURE},$InFileChip{$ID}->{SECTOR}, ";
			printf FD ("\"%.30s\",%d ,",$MemoryDeviceList[$it]->{Part_Number},$InFileChip{$ID}->{ADV_OPT});
			if($InFileChip{$ID}->{FEATURE} ne "VEND_NONE")
			{
			  printf FD "\n\t\t{$MemoryDeviceList[$it]->{PP_Function}, {$MemoryDeviceList[$it]->{Set_Feature},$MemoryDeviceList[$it]->{Get_Feature},$MemoryDeviceList[$it]->{Rrtry_Number},$MemoryDeviceList[$it]->{Rrtry_Addr},$MemoryDeviceList[$it]->{Rrtry_Default},$MemoryDeviceList[$it]->{Rrtry_Start},{$MemoryDeviceList[$it]->{Int_Address}, $MemoryDeviceList[$it]->{Sync}},{$MemoryDeviceList[$it]->{Int_Address}, $MemoryDeviceList[$it]->{Async}}},\n";
			  printf FD "\t\t{RAND_TYPE_SAMSUNG,{0x2D2D,1,1,1,1,1}}}";
			}
			print FD "},\n";
		}
        else
        {
			print FD "\t{$arry_str, $InFileChip{$ID}->{IDLength},$MemoryDeviceList[$it]->{AddrCycle},$InFileChip{$ID}->{IO},$MemoryDeviceList[$it]->{TotalSize_MB},$MemoryDeviceList[$it]->{BlockSize_KB},$MemoryDeviceList[$it]->{PageSize_B},$MemoryDeviceList[$it]->{SpareSize_B},$MemoryDeviceList[$it]->{Timing}, 0,0,0,$InFileChip{$ID}->{FEATURE},$InFileChip{$ID}->{SECTOR}, ";
            printf FD ("\"%.30s\",%d}, \n",$MemoryDeviceList[$it]->{Part_Number},$InFileChip{$ID}->{ADV_OPT});
        }
    }

    print FD "};\n\n";
    print FD "#endif\n";
    close FD;

}

#****************************************************************************
# subroutine:  check_PartNumber
# Description: check PartNumber in custom_MemoryDevice.h
#****************************************************************************
sub check_PartNumber()
{
    #1. check @PartNumbers do not have duplicate member
    my %hash;
    my $get = 0;
    my $input_part;
    foreach $i (@PartNumbers)
    {
        $hash{$i}++;
    }
    while(($key,$value) = each(%hash))
    {
        if($value >= 2)
        {&error_handler("Part Number: $key duplicates in custom_MemoryDevice.h\n", __FILE__, __LINE__);}
    }
    #2. check member of @PartNumber exists in MDL
    foreach $i (@PartNumbers)
    {
        $get =0;
        for($j=0;$j<@MemoryDeviceList;$j++){
            $input_part = $MemoryDeviceList[$j]->{Part_Number};
            $input_part =~ s/\s+//g;
            $get =1 if($input_part eq $i );
        }
        &error_handler("Part Number: $i not exist in MDL\n", __FILE__, __LINE__) if($get==0);
    }
    
}

#****************************************************************************
# subroutine:  search_PartNumber
# input:       part number
#****************************************************************************
sub search_PartNumber()
{
    my $get=0;
#print @_;
#print "\n";
    my ($input_part) = @_;
    $input_part =~ s/\s+//g;
    foreach $i (@PartNumbers)
    {
       chomp($i);
       if($i eq $input_part)
       { $get =1 };
    }
    return $get;
}


#****************************************************************************************
# subroutine:  xls_cell_value
# return:      Excel cell value no matter it's in merge area or not, and in windows or not
# input:       $Sheet:  Specified Excel Sheet
# input:       $row:    Specified row number
# input:       $col:    Specified column number
#****************************************************************************************
sub xls_cell_value()
{
    my($Sheet, $row, $col) = @_;
    my $cell = $Sheet->get_cell($row, $col);
    if (defined $cell)
    {
        return $cell->Value();
    } else
    {
        print "$Sheet: row=$row, col=$col undefined\n";
        return;
    }
}

#****************************************************************************
# subroutine:  error_handler
# input:       $error_msg:     error message
#****************************************************************************
sub error_handler()
{
    my($error_msg, $file, $line_no) = @_;
    my $final_error_msg = "NandGen ERROR: $error_msg at $file line $line_no\n";
    print $final_error_msg;
    die $final_error_msg;
}
#*******************************************************************************
#by_number: sort algorithm
#*******************************************************************************
sub by_length
{
	if(length($a)>length($b))
	{-1}
	elsif(length($a)<length($b))
	{1}
	elsif(length($a)==length($b))
	{$a<=>$b;}
}

sub by_number
{$a<=>$b}

sub struct_flashdev_info_define()
{
    my $template = <<"__TEMPLATE";
#define RAMDOM_READ\t\t(1<<0)
#define CACHE_READ\t\t(1<<1)

#define RAND_TYPE_SAMSUNG 0
#define RAND_TYPE_TOSHIBA 1
#define RAND_TYPE_NONE 2

#define READ_RETRY_MAX 10
struct gFeature
{
	u32 address;
	u32 feature;
};

struct gFeatureSet
{
	u8 sfeatureCmd;
	u8 gfeatureCmd;
	u8 readRetryCnt;
	u32 readRetryAddress;
	u32 readRetryDefault;
	u32 readRetryStart;
	struct gFeature Interface;
	struct gFeature Async_timing;
};

struct gRandConfig
{
	u8 type;
	u32 seed[6];
};

enum pptbl
{
	MICRON_8K,
};

struct MLC_feature_set
{
	enum pptbl ptbl_idx;
	struct gFeatureSet 	 FeatureSet;
	struct gRandConfig   randConfig;
};

enum flashdev_vendor
{
	VEND_SAMSUNG,
	VEND_MICRON,
	VEND_TOSHIBA,
	VEND_HYNIX,
	VEND_NONE,
};

enum flashdev_IOWidth
{
	IO_8BIT = 8,
	IO_16BIT = 16,
	IO_TOGGLEDDR = 9,
	IO_TOGGLESDR = 10,
	IO_ONFI = 12,
};

typedef struct
{
   u8 id[NAND_MAX_ID];
   u8 id_length;
   u8 addr_cycle;
   enum flashdev_IOWidth iowidth;
   u16 totalsize;
   u16 blocksize;
   u16 pagesize;
   u16 sparesize;
   u32 timmingsetting;
   u32 s_acccon;
   u32 s_acccon1;
   u32 freq;
   enum flashdev_vendor vendor;
   u16 sectorsize;
   u8 devciename[30];
   u32 advancedmode;
   struct MLC_feature_set feature_set;
}flashdev_info,*pflashdev_info;

__TEMPLATE

   return $template;
}

sub struct_snandflashdev_info_define()
{
    my $template = <<"__TEMPLATE";

typedef struct
{
    u8 id[SNAND_MAX_ID];
    u8 id_length;
    u16 totalsize;
    u16 blocksize;
    u16 pagesize;
    u16 sparesize;
    u32 SNF_DLY_CTL1;
    u32 SNF_DLY_CTL2;
    u32 SNF_DLY_CTL3;
    u32 SNF_DLY_CTL4;
    u32 SNF_MISC_CTL;
    u32 SNF_DRIVING;
    u8 devicename[30];
    u32 spareformat;
    u32 advancedmode;
}snand_flashdev_info,*psnandflashdev_info;

__TEMPLATE

   return $template;
}
