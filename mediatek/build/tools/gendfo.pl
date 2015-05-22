#!/usr/bin/perl

usage() if ($#ARGV < 1); # This means checking the last index of ARGV

my $genMode = lc($ARGV[0]);
my $outFilePath = $ARGV[1];

my $DEBUG = 0; # global debug switch
my $ProjectVersion = $ENV{"MTK_PROJECT"};

######pre-parse dfo array start#####
my @dfoAll = ();
my @dfoSupport = split(/\s+/, $ENV{"DFO_NVRAM_SET"});
foreach my $dfoSet (@dfoSupport) {
    my $dfoSetName = $dfoSet."_VALUE";
    my @dfoValues = split(/\s+/, $ENV{"$dfoSetName"});
    foreach my $dfoValue (@dfoValues) {
        push(@dfoAll, $dfoValue);
    }
}

my @dfoArray = ();
my @dfoDisableArray = ();
foreach my $tempDfo (@dfoAll) {
    my $isFind = 0;
    #only eng load will enable dfo
    if ($ENV{"TARGET_BUILD_VARIANT"} ne "user" && $ENV{"TARGET_BUILD_VARIANT"} ne "userdebug") {
        foreach my $isDfoSupport (@dfoSupport) {
            if ($ENV{$isDfoSupport} eq "yes") {
                my $dfoSupportName = $isDfoSupport."_VALUE";
                my @dfoValues = split(/\s+/, $ENV{"$dfoSupportName"});
                foreach my $dfoValue (@dfoValues) {
                    if ($tempDfo eq $dfoValue) {
                        $isFind = 1;
                        break;
                    }
                }
                
                if ($isFind == 1) {
                    break;
                }
            }
        }
    }
    
    if ($isFind == 1) {
        push(@dfoArray, $tempDfo);
    }
    else {
        push(@dfoDisableArray, $tempDfo);
    }
}
#print "Enable: @dfoArray\n";
#print "Disable: @dfoDisableArray\n";
my $dfoCount = scalar @dfoArray;
my $dfoDisableCount = scalar @dfoDisableArray;
#print "Enable Count: $dfoCount Disable Count: $dfoDisableCount\n";
#####pre-parse dfo array end#####

my $tmpOutput = "";

if ($genMode eq "nvhdr") {
#####generate DFO struct for NVRAM#####
$tmpOutput .= "#ifndef _CFG_DFO_FILE_H\n";
$tmpOutput .= "#define _CFG_DFO_FILE_H\n";
$tmpOutput .= "\n";
$tmpOutput .= "// the record structure define of dfo nvram file\n";
$tmpOutput .= "typedef struct\n";
$tmpOutput .= "{\n";

if ($dfoCount > 0) {
$tmpOutput .= "    int count;\n";
$tmpOutput .= "    char name[$dfoCount][32];\n";
$tmpOutput .= "    int value[$dfoCount];\n";
}
else {
$tmpOutput .= "    int count;\n";
$tmpOutput .= "    char name[1][32];\n";
$tmpOutput .= "    int value[1];\n";
}

$tmpOutput .= "} ap_nvram_dfo_config_struct;\n";
$tmpOutput .= "\n";
$tmpOutput .= "//the record size and number of dfo nvram file\n";
$tmpOutput .= "#define CFG_FILE_DFO_CONFIG_SIZE    sizeof(ap_nvram_dfo_config_struct)\n";
$tmpOutput .= "#define CFG_FILE_DFO_CONFIG_TOTAL   1\n";
$tmpOutput .= "\n";
$tmpOutput .= "#endif /* _CFG_DFO_FILE_H */\n";
#####generate DFO struct for NVRAM end#####
}
elsif ($genMode eq "nvdft") {
#####generate DFO default value for NVRAM#####
$tmpOutput .= "#ifndef _CFG_DFO_D_H\n";
$tmpOutput .= "#define _CFG_DFO_D_H\n";
$tmpOutput .= "\n";
$tmpOutput .= "ap_nvram_dfo_config_struct stDfoConfigDefault =\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    // count\n";
$tmpOutput .= "    $dfoCount,\n";
$tmpOutput .= "\n";

if ($dfoCount > 0) {
$tmpOutput .= "    // name array\n";
$tmpOutput .= "    {\n";

    my $tmpIndex = 0;
    foreach my $dfo (@dfoArray) {
        if ($tmpIndex < ($dfoCount-1)) {
            die "$dfo name length too long\n" if ($dfo >= 32);
$tmpOutput .= "        \"$dfo\",\n";
        }
        else {
            die "$dfo name length too long\n" if ($dfo >= 32);
$tmpOutput .= "        \"$dfo\"\n";
        }
        $tmpIndex++;
    }

$tmpOutput .= "    },\n";
$tmpOutput .= "\n";
}   # ($dfoCount > 0) end
else {
$tmpOutput .= "    // name array\n";
$tmpOutput .= "    {\n";
$tmpOutput .= "        \"NODFO\"\n";
$tmpOutput .= "    },\n";
$tmpOutput .= "\n";
}

if ($dfoCount > 0) {
$tmpOutput .= "    // value array\n";
$tmpOutput .= "    {\n";

    $tmpIndex = 0;
    foreach my $dfo (@dfoArray) {
        my $val = getValue($dfo);
    
        if ($tmpIndex < ($dfoCount-1)) {
$tmpOutput .= "        $val,\n";
        }
        else {
$tmpOutput .= "        $val\n";
        }
        $tmpIndex++;
    }

$tmpOutput .= "    }\n";
}   # ($dfoCount > 0) end
else {
$tmpOutput .= "    // value array\n";
$tmpOutput .= "    {\n";
$tmpOutput .= "        0\n";
$tmpOutput .= "    }\n";
}

$tmpOutput .= "};\n";
$tmpOutput .= "\n";
$tmpOutput .= "#endif /* _CFG_DFO_D_H */\n";
#####generate DFO default value for NVRAM end#####
}
elsif ($genMode eq "def") {
#####generate DFO defines#####
$tmpOutput .= "#ifndef __DFO_DEFINES_H__\n";
$tmpOutput .= "#define __DFO_DEFINES_H__\n\n";

if ($dfoCount > 0) {
$tmpOutput .= "#include \"IDynFeatureOption.h\"\n\n";
    foreach my $dfo (@dfoArray) {
        if (isInteger($ENV{$dfo}) == 0) {
$tmpOutput .= "#define $dfo DfoGetBoolean(\"$dfo\")\n";
        }
        else {
$tmpOutput .= "#define $dfo DfoGetInt(\"$dfo\")\n";
        }
    }
}

if ($dfoDisableCount > 0) {
    foreach my $dfo (@dfoDisableArray) {
        my $val = getValue($dfo);
$tmpOutput .= "#define $dfo $val\n";
    }
}

$tmpOutput .= "\n#endif\n";
#####generate DFO defines end#####
}

writeFileWhenUpdated($tmpOutput, $outFilePath);

exit 0;

sub isInteger {
    my $val = shift;
    return ($val =~ m/^\d+$/) || ($val =~ m/^0x[0-9|a-f|A-F]+$/);
}

sub getValue {
    my $dfo = @_[0];
    my $dfoVal = $ENV{$dfo};
    my $val = -1;
    #print "get $dfo $dfoVal\n";
    
    if (isInteger($dfoVal)) {
        $val = $dfoVal;
    }
    elsif ($dfoVal eq "yes") {
        $val = 1;
    }
    elsif ($dfoVal eq "no") {
        $val = 0;
    }
    else {
        my @refCmd = split(/:/, $dfoVal);
        if ($refCmd[0] eq "ref") {
            my $cmd = "$ENV{'MTK_ROOT_BUILD'}/tools/$refCmd[1]";
            $val = `/usr/bin/perl $cmd`;
            if ($? eq 0) {
                chomp($val);
            }
            else {
                die "$cmd error $dfo = $dfoVal\n";
            }
            #print "Test $refCmd[0] $refCmd[1] $val\n";
        }
        else {
            die "WRONG format $dfo = $dfoVal\n";
        }
    }
    
    return $val;
}

sub writeFileWhenUpdated {
    my $tmpOutput = @_[0];
    my $outputFileName = @_[1];
    
    if (open(OUTPUT_FILE, "<$outputFileName")) {
        my @lines = <OUTPUT_FILE>;
        close OUTPUT_FILE;
        my $readFile = "";
        foreach my $line (@lines) {
            $readFile .= "$line";
        }
        
        if ($tmpOutput ne $readFile) {
            print "Update file: $outputFileName\n";
            open OUTPUT_FILE, ">$outputFileName" or die "Can't open the file $outputFileName";
            print OUTPUT_FILE "$tmpOutput";
            close OUTPUT_FILE;
        }
        else {
            print "No need update file: $outputFileName\n";
        }
    }
    else {
        print "Creat new file: $outputFileName\n";
        open OUTPUT_FILE, ">$outputFileName" or die "Can't open the file $outputFileName";
        print OUTPUT_FILE "$tmpOutput";
        close OUTPUT_FILE;
    }
}

sub usage
{
	die "Usage: gendfo.pl [mode] [output file path] (mode=def|nvhdr:nvdft)";
}
