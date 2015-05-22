#!/usr/bin/perl
use strict;
use File::Basename;
use File::Copy;
use File::Path;

my $LOCAL_PATH;
BEGIN
{
	$LOCAL_PATH = dirname($0);
}
use lib "mediatek/build/tools";
use lib $LOCAL_PATH;
use mtk_modem;


my ($input_path, $project) = @ARGV;

usage() if ($#ARGV < 1);

die "Cannot find modem path $input_path\n" if (!-d $input_path);

my $dest_folder = $input_path . "/temp_modem";

print "[LOG] Delete and re-make directory $dest_folder ...\n\n";

rmtree($dest_folder) or die "Cannot delete folder $dest_folder\n" if (-d $dest_folder);
mkpath($dest_folder) or die "Cannot create folder $dest_folder\n" if (!-d $dest_folder);

# Check whether "build" folder exists or not
my $project_path;
my $type_of_load = 0;
if (-d "$input_path/build")
{
	# MOLY
	my $flavorUC = "DEFAULT"; # default flavor is "DEFAULT"
	my $projectNoFlavorUC;
	if ($project =~ /(.*)\((.*)\)/)
	{
		$flavorUC = uc($2);
		$projectNoFlavorUC = uc($1);
	}
	else
	{
		$projectNoFlavorUC = uc($project);
	}
	$project_path = "$input_path/build/$projectNoFlavorUC/$flavorUC";
	# MAUI DSDA
	if ((! -d $project_path) && (! -e "$input_path/make/build.mak") && (-d "$input_path/build/$projectNoFlavorUC"))
	{
		$project_path = "$input_path/build/$projectNoFlavorUC";
		$type_of_load = 2;
	}
}
elsif (-d "$input_path/out_$project")
{
	# BACH new
	$project_path = "$input_path/out_$project";
}
elsif (-d "$input_path/out")
{
	# BACH old
	$project_path = "$input_path/out";
}

die "[FAIL] $project_path doesn't exist!" if (! -e $project_path);
my %mapping_table;
get_modem_file_mapping(\%mapping_table, undef, undef, $project_path, $type_of_load);

foreach my $curFile (keys %mapping_table)
{
	print "[Info] Copy $curFile as $dest_folder/$mapping_table{$curFile}\n";
	if (copy($curFile, "$dest_folder/$mapping_table{$curFile}"))
	{
	}
	else
	{
		warn "[Warning] Cannot copy $input_path/$curFile\n";
	}
}


print "\n\n[NOTICE]\n";
print "The modem files were renamed and copied to folder [\"$dest_folder\"].\n";
print "You can start using them in your Smartphone developement.\n\n";

exit;



sub usage
{
	print <<"__EOFUSAGE";

Usage:
	$0 [Modem Codebase Path] [Modem Project Makefile Name]

Description:

	The script is to automatically copy modem files which are needed in Smartphone development (including modem image, database, makefile, etc.) with defined renaming rule.
	The files will be copied to a designated folder for you to use. 

Example:
	$0 ~/currUser/Modem/mcu MT6572_DEMO_HSPA(DEFAULT)

__EOFUSAGE
	exit 1;
}

