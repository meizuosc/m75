#!/usr/bin/perl

usage() if ($#ARGV < 1); # This means checking the last index of ARGV

my $genMode = lc($ARGV[0]);
my $outFilePath = $ARGV[1];
my $isLegacy = 0;
if ($ENV{LEGACY_DFO_GEN} eq "yes") {
    $isLegacy = 1;
}

my $DEBUG = 0; # global debug switch
my @dfoBootArray = split(/\s+/, $ENV{DFO_MISC});
my $dfoBootCount = scalar @dfoBootArray;
my $tmpOutput = "";

if ($genMode eq "boot") {
#####generate DFO Boot header#####
$tmpOutput .= "#ifndef DFO_BOOT_H\n";
$tmpOutput .= "#define DFO_BOOT_H\n\n";
$tmpOutput .= "#define ATAG_DFO_DATA 0x41000805\n";
$tmpOutput .= "#define DFO_BOOT_COUNT $dfoBootCount\n\n";

if ($dfoBootCount > 0) {
$tmpOutput .= "typedef struct\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    char name[DFO_BOOT_COUNT][32];   // kernel dfo name array\n";
$tmpOutput .= "    int value[DFO_BOOT_COUNT];       // kernel dfo value array\n";
$tmpOutput .= "} tag_dfo_boot;\n\n";
} # ($dfoBootCount > 0) end
else {
$tmpOutput .= "typedef struct\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    char name[1][32];   // kernel dfo name array\n";
$tmpOutput .= "    int value[1];       // kernel dfo value array\n";
$tmpOutput .= "} tag_dfo_boot;\n\n";
} # else end

$tmpOutput .= "#endif\n";
#####generate DFO Boot header end#####
}
elsif ($genMode eq "bootdft" && $isLegacy == 1) {
#####generate DFO Boot Default value header#####
$tmpOutput .= "#ifndef DFO_BOOT_DEFAULT_H\n";
$tmpOutput .= "#define DFO_BOOT_DEFAULT_H\n\n";

if ($dfoBootCount > 0) {
$tmpOutput .= "tag_dfo_boot dfo_boot_default =\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    // name array\n";
$tmpOutput .= "    {\n";

    my $tmpIndex = 0;
    foreach my $dfoBoot (@dfoBootArray) {
        if ($tmpIndex < ($dfoBootCount-1)) {
            die "$dfoBoot name length too long\n" if ($dfoBoot >= 32);
$tmpOutput .= "        \"$dfoBoot\",\n";
        }
        else {
            die "$dfoBoot name length too long\n" if ($dfoBoot >= 32);
$tmpOutput .= "        \"$dfoBoot\"\n";
        }
        $tmpIndex++;
    }

$tmpOutput .= "    },\n\n";
$tmpOutput .= "    // value array\n";
$tmpOutput .= "    {\n";

    $tmpIndex = 0;
    foreach my $dfoBoot (@dfoBootArray) {
        my $val = getValue($dfoBoot);
    
        if ($tmpIndex < ($dfoBootCount-1)) {
$tmpOutput .= "        $val,\n";
        }
        else {
$tmpOutput .= "        $val\n";
        }
        $tmpIndex++;
    }

$tmpOutput .= "    }\n";
$tmpOutput .= "};\n\n";
} # ($dfoBootCount > 0) end
else {
$tmpOutput .= "tag_dfo_boot dfo_boot_default =\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    // name array\n";
$tmpOutput .= "    {\n";
$tmpOutput .= "        \"NO_DFO\"\n";
$tmpOutput .= "    },\n\n";

$tmpOutput .= "    // value array\n";
$tmpOutput .= "    {\n";
$tmpOutput .= "        0\n";
$tmpOutput .= "    }\n";
$tmpOutput .= "};\n\n";
} # else end

$tmpOutput .= "#endif\n";
#####generate DFO Boot header Default value end#####
}
elsif ($genMode eq "bootdft" && $isLegacy == 0) {
#####generate DFO Boot Default value header#####
$tmpOutput .= "#ifndef DFO_BOOT_DEFAULT_H\n";
$tmpOutput .= "#define DFO_BOOT_DEFAULT_H\n\n";
$tmpOutput .= "#define DFO_BOOT_COUNT $dfoBootCount\n\n";
$tmpOutput .= "typedef struct\n";
$tmpOutput .= "{\n";
$tmpOutput .= "    char name[32];   // kernel dfo name\n";
$tmpOutput .= "    unsigned long value;       // kernel dfo value\n";
$tmpOutput .= "} dfo_boot_info;\n\n";

if ($dfoBootCount > 0) {
$tmpOutput .= "const dfo_boot_info dfo_boot_default[DFO_BOOT_COUNT] =\n";
$tmpOutput .= "{\n";
    my $tmpIndex = 0;
    foreach my $dfoBoot (@dfoBootArray) {
        die "$dfoBoot name length too long\n" if (length($dfoBoot) >= 31);
        my $val = getValue($dfoBoot);
$tmpOutput .= "    // boot dfo $tmpIndex\n";
$tmpOutput .= "    {\n";
$tmpOutput .= "        \"$dfoBoot\",\n";
$tmpOutput .= "        $val\n";
        if ($tmpIndex < ($dfoBootCount-1)) {
$tmpOutput .= "    },\n\n";
        }
        else {
$tmpOutput .= "    }\n";
        }
        $tmpIndex++;
    }
$tmpOutput .= "};\n\n";
} # ($dfoBootCount > 0) end

$tmpOutput .= "#endif\n";
#####generate DFO Boot header Default value end#####
}

my $updated = writeFileWhenUpdated($tmpOutput, $outFilePath);


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
            return 1;
        }
        else {
            print "No need update file: $outputFileName\n";
            return 0;
        }
    }
    else {
        print "Creat new file: $outputFileName\n";
        open OUTPUT_FILE, ">$outputFileName" or die "Can't open the file $outputFileName";
        print OUTPUT_FILE "$tmpOutput";
        close OUTPUT_FILE;
        return 1;
    }
}

sub usage
{
	die "Usage: gendfoboot.pl [mode] [output file path] (mode=boot|bootdft)";
}
