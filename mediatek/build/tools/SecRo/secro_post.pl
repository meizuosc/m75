#!/usr/bin/perl

use lib "mediatek/build/tools";
use pack_dep_gen;
PrintDependModule($0);

##########################################################
# Initialize Variables
##########################################################
my $secro_cfg = $ARGV[0];
my $prj = $ARGV[1];
my $custom_dir = $ARGV[2];
my $secro_ac = $ARGV[3];
my $secro_out = $ARGV[4];
my $sml_dir = "mediatek/custom/$custom_dir/security/sml_auth";
my $secro_tool = "mediatek/build/tools/SecRo/SECRO_POST";
my $secro_tool_legacy = "mediatek/build/tools/SecRo/SECRO_POST_LEGACY";
my $MTK_ENABLE_MD1 = $ENV{"MTK_ENABLE_MD1"};
my $MTK_ENABLE_MD2 = $ENV{"MTK_ENABLE_MD2"};
my $MTK_PLATFORM = $ENV{"MTK_PLATFORM"};
my $MTK_ROOT_CUSTOM_OUT = $ENV{"MTK_ROOT_CUSTOM_OUT"};
my $secro_ini = "$MTK_ROOT_CUSTOM_OUT/SECRO_WP.ini";
if (${MTK_PLATFORM} eq "MT6575" || ${MTK_PLATFORM} eq "MT6577")
{
   $legacy_mode = "1";
}

print "******************************************************\n";
print "*********************** SETTINGS *********************\n";
print "******************************************************\n";
print " Project           =  $prj\n";
print " Custom Dir  =  $custom_dir\n";
print " MTK_ENABLE_MD1 = $MTK_ENABLE_MD1\n";
print " MTK_ENABLE_MD2 = $MTK_ENABLE_MD2\n";
print " MTK_PLATFORM = $MTK_PLATFORM\n";
print " MTK_ROOT_CUSTOM_OUT = $MTK_ROOT_CUSTOM_OUT\n";

##########################################################
# SecRo Post Processing
##########################################################

my $ac_region = "mediatek/custom/$custom_dir/secro/AC_REGION";
#my $ac_region = "out/target/product/$prj/secro/AC_REGION";
my $and_secro = "mediatek/custom/$custom_dir/secro/AND_SECURE_RO";
my $md_secro = "$MTK_ROOT_CUSTOM_OUT/modem/SECURE_RO";
my $md2_secro = "$MTK_ROOT_CUSTOM_OUT/modem/SECURE_RO_sys2";

if (${secro_ac} eq "yes")
{
	$md_secro = "$MTK_ROOT_CUSTOM_OUT/modem/SECURE_RO";
	if ( ! -e $md_secro )
	{
		print "this modem does not has modem specific SECRO image, use prj SECRO\n";
		$md_secro = "mediatek/custom/$custom_dir/secro/SECURE_RO";
	}

        $md2_secro = "$MTK_ROOT_CUSTOM_OUT/modem/SECURE_RO_sys2";
        if ( ! -e $md2_secro )
        {
                print "this modem2 does not has modem2 specific SECRO image, use prj SECRO\n";
                $md2_secro = "mediatek/custom/$custom_dir/secro/SECURE_RO_sys2";
        }
}
else
{
	$md_secro = "mediatek/custom/$custom_dir/secro/SECURE_RO";
	$md2_secro = "mediatek/custom/$custom_dir/secro/SECURE_RO_sys2";
}

##########################################################
# Check the Existence of each Region
##########################################################
if ( ! -e $ac_region )
{
	$ac_region = "mediatek/custom/common/secro/AC_REGION";
	print "does not has aggregate specific AC_REGION image, use common AC_REGION\n";
}
print " ac_region = $ac_region\n";

if ( ! -e $and_secro )
{
        $and_secro = "mediatek/custom/common/secro/AND_SECURE_RO";
	print "does not has AP specific AC_REGION_RO image, use common AC_REGION_RO\n";
}
print " and_secro = $and_secro\n";

if ( ! -e $md_secro )
{
        $md_secro = "mediatek/custom/common/secro/SECURE_RO";
	print "does not has MODEM specific SECURE_RO image, use common SECURE_RO\n";
}
print " md_secro = $md_secro\n";

if ( ! -e $md2_secro )
{
        $md2_secro = "mediatek/custom/common/secro/SECURE_RO_sys2";
	print "does not has MODEM specific SECURE_RO image, use common SECURE_RO_sys2\n";
}
print " md2_secro = $md2_secro\n";

open(SECRO_FH, ">$secro_ini") or die "open file error $secro_ini\n";

print SECRO_FH "SECRO_CFG = $secro_cfg\n";
print SECRO_FH "AND_SECRO = $and_secro\n";
print SECRO_FH "AC_REGION = $ac_region\n";
PrintDependency($secro_cfg);
PrintDependency($and_secro);
PrintDependency($ac_region);

opendir(DIR, "$MTK_ROOT_CUSTOM_OUT/modem");
@files = grep(/^SECURE_RO*/,readdir(DIR));

my $count = 0;
foreach my $file (@files)
{
	PrintDependency("$MTK_ROOT_CUSTOM_OUT/modem/$file");
	print SECRO_FH "SECRO[$count] = $MTK_ROOT_CUSTOM_OUT/modem/$file\n";
	$count++;
}

if($count>=10)
{
	die "Maximum support of SECRO for world phone is 10, but current is $count\n";
}

while($count<=9)
{
	PrintDependency("mediatek/custom/common/secro/SECURE_RO");
	print SECRO_FH "SECRO[$count] = mediatek/custom/common/secro/SECURE_RO\n";
	$count++;
}

closedir(DIR);
close(SECRO_FH);

system("chmod 777 $ac_region") == 0 or die "can't configure $ac_region as writable";
print "MTK_SEC_SECRO_AC_SUPPORT = $secro_ac\n";
if (${secro_ac} eq "yes")
{		
	PrintDependency("$sml_dir/SML_ENCODE_KEY.ini");
	PrintDependency($secro_tool);
	if (${legacy_mode} eq "1")
	{
	    system("./$secro_tool_legacy $secro_cfg $sml_dir/SML_ENCODE_KEY.ini $and_secro $md_secro $ac_region $secro_out") == 0 or die "SECRO POST Tool return error\n";
	}
	else
	{
	    system("./$secro_tool $secro_ini $sml_dir/SML_ENCODE_KEY.ini $secro_out") == 0 or die "SECRO POST Tool return error\n";
	}
}
unlink($secro_ini);

