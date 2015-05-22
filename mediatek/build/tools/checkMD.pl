#!/usr/bin/perl
use strict;
use lib "mediatek/build/tools";
use mtk_modem;
use pack_dep_gen;
PrintDependModule($0);

#######################################
# Description
## The script is for checking the consistency between modem.img (multiple) and MTK_MD1_SUPPORT/MTK_MD2_SUPPORT/MTK_PLATFORM option setting.
## Check item: mode(2G/3G/WG/TG), platform, project ID(branch/week), serial number
#######################################
#######################################
# NOTICE
## This script is specific for MT6572 with JB
## NOT backward compatible!!!!
#######################################

usage() if ($#ARGV < 1);

my ($AP_Project_Name, $AP_SW_Version, $modem_path, $MTK_PLATFORM, $mtk_modem_support, $mtk_modem_2nd_support, $bin_info);

$AP_SW_Version = $ENV{"MTK_BUILD_VERNO"}; # $AP_SW_Version --> modem-info

if($ARGV[0]=~/PROJECT=(.*)/){
	$AP_Project_Name = $1;
}

if($ARGV[1]=~/PRIVATE_MODEM_PATH=(.*)/){
	$modem_path = $1;
}

if($ARGV[2]=~/MTK_PLATFORM=(.*)/){
	$MTK_PLATFORM = $1;
}

if($ARGV[3]=~/MTK_MD1_SUPPORT=(.*)/){
	$mtk_modem_support = $1;
}

if($ARGV[4]=~/MTK_MD2_SUPPORT=(.*)/){
	$mtk_modem_2nd_support = $1;
}

if($ARGV[5]=~/MTK_GET_BIN_INFO=(.*)/){
	$bin_info = $1;
	if ($bin_info =~ /info/)
	{
		$bin_info = 1;
	} else {
		$bin_info = '';
	}
}

#######################################
# Initialization
#######################################
my %MD_INDEX;
my $errCnt = 0;
my $debug = 1; # output debug info.


if ($debug)
{
	print "\n==========================\n";
	print "Modem path = $modem_path\n";
	print "\n==========================\n";
}

if (1)
{
	my $MAX_MODEM_NUMBER = 5;
	my $md_id;
	for ($md_id = 1; $md_id <= $MAX_MODEM_NUMBER; $md_id++)
	{
		#print "md_id = $md_id\n";
		if (! exists $ENV{"MTK_ENABLE_MD" . $md_id})
		{
			print "\$MTK_ENABLE_MD" . $md_id . " is not defined\n";
		}
		elsif ($ENV{"MTK_ENABLE_MD" . $md_id} ne "yes")
		{
			print "\$MTK_ENABLE_MD" . $md_id . " = " . $ENV{"MTK_ENABLE_MD" . $md_id} . "\n";
			$MD_INDEX{$md_id} = undef;
		}
		else
		{
			print "\$MTK_ENABLE_MD" . $md_id . " = " . $ENV{"MTK_ENABLE_MD" . $md_id} . "\n";
			if (($MTK_PLATFORM eq "MT6575") or ($MTK_PLATFORM eq "MT6577") or ($MTK_PLATFORM eq "MT6589"))
			{
				if ($ENV{"MTK_MD" . $md_id . "_SUPPORT"} > 2)
				{
					print "[ERROR] Please set MTK_MD" . $md_id . "_SUPPORT" . "=2 instead of " . $ENV{"MTK_MD" . $md_id . "_SUPPORT"} . " if you are using 3G modem\n";
				}
			}
			else
			{
				if ($ENV{"MTK_MD" . $md_id . "_SUPPORT"} == 2)
				{
					print "[ERROR] Please set MTK_MD" . $md_id . "_SUPPORT" . "=3 instead of " . $ENV{"MTK_MD" . $md_id . "_SUPPORT"} . " if you are using 3G FDD modem\n";
					print "[ERROR] Please set MTK_MD" . $md_id . "_SUPPORT" . "=4 instead of " . $ENV{"MTK_MD" . $md_id . "_SUPPORT"} . " if you are using 3G TDD modem\n";
				}
				if ($ENV{"MTK_MD" . $md_id . "_SUPPORT"} == 0)
				{
					print "Skip checking modem because " . "MTK_MD" . $md_id . "_SUPPORT" . "=" . $ENV{"MTK_MD" . $md_id . "_SUPPORT"} . "\n";
					$MD_INDEX{$md_id} = {};
					next;
				}
			}
			my $modem_name = get_modem_name(\%ENV, $md_id);
			$MD_INDEX{$md_id} = {"modem_name" => $modem_name};
			$errCnt += Check_MD_Info($MD_INDEX{$md_id}, $md_id, $modem_path . "/" . $modem_name, $debug);
		}
	}
}
else
{
	print "Skip checking modem\n";
	exit 0;
}

if ($debug)
{
	print "\n==========================\n";
	print "*** Info from feature option configuration ***\n\n";
	foreach my $md_id (sort keys %MD_INDEX)
	{
		print "\$MTK_MD" . $md_id . "_SUPPORT = " . $ENV{"MTK_MD" . $md_id . "_SUPPORT"} . "\n";
	}
	print "\$MTK_PLATFORM = $MTK_PLATFORM\n\n";
}




#######################################
# Output modem information
#######################################
if ($bin_info)
{
	print "\[AP Project Name\]: $AP_Project_Name\n";
	print "\[AP SW Version\]: $AP_SW_Version\n";
	foreach my $md_id (sort keys %MD_INDEX)
	{
		if (exists $MD_INDEX{$md_id}->{"modem_name"})
		{
			my $MD_IMG_PROJECT_NAME = $MD_INDEX{$md_id}->{"project_name"};
			my $MD_IMG_PROJECT_FLAVOR = $MD_INDEX{$md_id}->{"flavor"};
			my $MD_IMG_PROJECT_VERNO = $MD_INDEX{$md_id}->{"verno"};
			print "\[MD$md_id Project Name\]: $MD_IMG_PROJECT_NAME\n";
			print "\[MD$md_id SW Version\]: $MD_IMG_PROJECT_VERNO\n";
			print "\[MD$md_id Flavor\]: $MD_IMG_PROJECT_FLAVOR\n";
		}
	}
	print "\[Site\]: ALPS\n";
}

exit $errCnt;









sub usage
{
	print <<"__EOFUSAGE";

Usage:
$0 [Modem Path] [MTK_PLATFORM] [MTK_MD1_SUPPORT] [MTK_MD2_SUPPORT]

Modem Path           Path of modem folder
MTK_PLATFORM         current project platform
                     (ex. MT6516, MT6573)
MTK_MD1_SUPPORT    mode of 1st modem of current project
                     (ex. modem_3g, modem_2g)
MTK_MD2_SUPPORT    mode of 2nd modem of current project
                     (ex. modem_3g, modem_2g)                  
__EOFUSAGE
	exit 1;
}
