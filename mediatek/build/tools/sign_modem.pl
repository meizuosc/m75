#!/usr/bin/perl

use lib "mediatek/build/tools";
use pack_dep_gen;
PrintDependModule($0);

##########################################################
# Initialize Variables
##########################################################

my $prj = $ARGV[0];
my $modem_encode = $ARGV[1];
#my $modem_encode = "no";
my $modem_auth = $ARGV[2];
my $custom_dir = $ARGV[3];
my $secro_ac = $ARGV[4];
my $MTK_SECURITY_SW_SUPPORT = $ENV{"MTK_SECURITY_SW_SUPPORT"};
my $MTK_ROOT_CUSTOM_OUT = $ENV{"MTK_ROOT_CUSTOM_OUT"};
my $OUT_DIR = $ENV{OUT_DIR};
my $MTK_PLATFORM =$ENV{MTK_PLATFORM};
if (${MTK_PLATFORM} eq "MT6575" || ${MTK_PLATFORM} eq "MT6577")
{
    $legacy_mode = "1";
}
my $modem_cipher = "yes";

my $sml_dir = "mediatek/custom/$custom_dir/security/sml_auth";

my $cipher_tool = "mediatek/build/tools/CipherTool/CipherTool";
my $sign_tool = "mediatek/build/tools/SignTool/SignTool.sh";
my $secro_tool = "mediatek/build/tools/SecRo/SECRO_POST";
if (${legacy_mode} eq "1")
{
    $secro_tool = "mediatek/build/tools/SecRo/SECRO_POST_LEGACY";
}

##########################################################
# Check Parameter
##########################################################

print "\n\n";
print "********************************************\n";
print " CHECK PARAMETER \n";
print "********************************************\n";

if (${modem_auth} eq "yes")
{
	if (${modem_encode} eq "no")
	{
		die "Error! MTK_SEC_MODEM_AUTH is 'yes' but MTK_SEC_MODEM_ENCODE is 'no'\n";
	}
}

if (${MTK_SECURITY_SW_SUPPORT} ne "yes")
{
	$modem_cipher = "no"
}

print "parameter check pass (2 MDs)\n";
print "MTK_SEC_MODEM_AUTH    =  $modem_auth\n";
print "MTK_SEC_MODEM_ENCODE  =  $modem_encode\n";
print "MTK_SECURITY_SW_SUPPORT  =  $MTK_SECURITY_SW_SUPPORT\n";
print "MTK_ROOT_CUSTOM_OUT  =  $MTK_ROOT_CUSTOM_OUT\n";
print "modem_cipher  =  $modem_cipher\n";

##########################################################
# Process Modem Image
##########################################################

my $md_load = "$MTK_ROOT_CUSTOM_OUT/modem/modem.img";
my $b_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/modem.img.bak";
my $c_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/cipher_modem.img";
my $s_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/signed_modem.img";

opendir(DIR, "$MTK_ROOT_CUSTOM_OUT/modem");
@files = grep(/\.img/,readdir(DIR));
foreach my $file (@files)
{
	$md_load = "$MTK_ROOT_CUSTOM_OUT/modem/$file";
	$b_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/$file.bak";
	$c_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/cipher_$file";
	$s_md_load = "$MTK_ROOT_CUSTOM_OUT/modem/signed_$file";
	&process_modem_image;
}
closedir(DIR);

sub process_modem_image
{
	print "\n\n";
	print "********************************************\n";
	print " PROCESS MODEM IMAGE ($md_load)\n";
	print "********************************************\n";	
	
	if (-e "$b_md_load")
	{
		print "$md_load already processed ... \n";
	}
	else
	{
		if (-e "$md_load")
		{
			system("cp -f $md_load $b_md_load") == 0 or die "can't backup modem image";

			########################################		
			# Encrypt and Sign Modem Image
			########################################		
			if (${modem_encode} eq "yes")
			{
				if (${modem_cipher} eq "yes")
				{
					PrintDependency("$sml_dir/SML_ENCODE_KEY.ini");
					PrintDependency("$sml_dir/SML_ENCODE_CFG.ini");
					PrintDependency($md_load);
					PrintDependency($cipher_tool);
					system("./$cipher_tool ENC $sml_dir/SML_ENCODE_KEY.ini $sml_dir/SML_ENCODE_CFG.ini $md_load $c_md_load") == 0 or die "Cipher Tool return error\n";
				
					if(-e "$c_md_load")
					{
						system("rm -f $md_load") == 0 or die "can't remove original modem binary\n";
						system("mv -f $c_md_load $md_load") == 0 or die "can't generate cipher modem binary\n";
					}
				}
				PrintDependency("$sml_dir/SML_AUTH_KEY.ini");
				PrintDependency("$sml_dir/SML_AUTH_CFG.ini");
				PrintDependency("$md_load");
				PrintDependency($sign_tool);
				system("./$sign_tool $sml_dir/SML_AUTH_KEY.ini $sml_dir/SML_AUTH_CFG.ini $md_load $s_md_load");
	
				if(-e "$s_md_load")
				{
					system("rm -f $md_load") == 0 or die "can't remove original modem binary\n";
					system("mv -f $s_md_load $md_load") == 0 or die "can't generate signed modem binary\n";
				}
			}
			else
			{
				print "doesn't execute Cipher Tool and Sign Tool ... \n";
			}
		}
		else
		{
			print "$md_load is not existed\n";			
		}
	}
}

##########################################################
# Fill AC_REGION
##########################################################

print "\n\n";
print "********************************************\n";
print " Fill AC_REGION \n";
print "********************************************\n";

my $secro_def_cfg = "mediatek/custom/common/secro/SECRO_DEFAULT_LOCK_CFG.ini";
if (${legacy_mode} eq "1")
{
    $secro_def_cfg = "mediatek/custom/common/secro/SECRO_DEFAULT_LOCK_CFG_LEGACY.ini";
}
# use $custom_dir to specify project only , not including flavor project part. ie:mt6582_evb , not mt6582_evb[tee]
my $secro_out_dir = "$OUT_DIR/target/product/$custom_dir/secro";
my $secro_out = "$secro_out_dir/AC_REGION";

my $secro_script = "mediatek/build/tools/SecRo/secro_post.pl";
PrintDependency($secro_def_cfg);

if (! -d "$secro_out_dir"){
    system("mkdir -p $secro_out_dir");
}

system("./$secro_script $secro_def_cfg $prj $custom_dir $secro_ac $secro_out") == 0 or die "SECRO post process return error\n";

##########################################################
# Process SECFL.ini
##########################################################

print "\n\n";
print "********************************************\n";
print " PROCESS SECFL.ini \n";
print "********************************************\n";

my $secfl_pl = "mediatek/build/tools/sign_sec_file_list.pl";
system("./$secfl_pl $custom_dir") == 0 or die "SECFL Perl return error\n";
