#!/usr/bin/perl
use lib 'mediatek/build/tools';
use pack_dep_gen;
usage() if ($#ARGV < 0);

my $modem_type = lc($ARGV[0]);
my $project_config = "$ENV{'MTK_ROOT_CONFIG_OUT'}/ProjectConfig.mk";
my $modem_path = "$ENV{'MTK_ROOT_CUSTOM'}/common/modem/";
PrintDependModule($0);
unless (-e $project_config) {
    print "Cannot find ProjectConfig.mk from " . $project_config . "\n";
    exit -1;
}

my $temp = `cat $project_config | grep -P '^CUSTOM_MODEM\\s*=' | head -1 | cut -d '#' -f1 | cut -d '=' -f2 | tr -d [:cntrl:]`;
$temp =~ m/^\s*(.*)\s*$/;
$temp = $1;

unless (defined $temp) {
    print "No modem image is defined in ProjectConfig.mk\n";
    exit -1;
}

my @projects = split(' ', $temp);
my $size = "0x00000000";
foreach my $i (@projects) {
    my $search = $modem_path . "$i";
    unless (-e $search) {
        print "$search NOT existed\n";
        #exit -1;
        last;
    }

    my $modem = `find $search -name '*.img' | head -1 | tr -d [:cntrl:]`;
    unless (defined $modem) {
        print "No modem image is found under $search\n";
        #exit -1;
        last;
    }

    my $result = `xxd -ps -l4 -s-20 $modem |  tr -d [:cntrl:]`;
    PrintDependency($modem);
    my $type = "md1";
    $type = "md2" if ($result eq "02000000");
   
    if ($modem_type eq $type) {
        $result = `xxd -ps -l4 -s-16 $modem |  tr -d [:cntrl:]`;
        $result = "0x". substr($result,6,2) . substr($result,4,2) . substr($result,2,2) . substr($result,0,2);
        $size = $result if ($result gt $size);
    } 
}

print "$size\n";
exit 0;

sub usage
{
	print <<"__EOFUSAGE";

Usage:
$0 Modem_Type

Modem Type           Type of modem image
__EOFUSAGE
	exit 1;
}
