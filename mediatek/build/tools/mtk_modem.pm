package mtk_modem;
use strict;
use File::Basename;
use Exporter;
use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT		= qw(Get_MD_X_From_Bind_Sys_Id Get_MD_YY_From_Image_Type Parse_MD_Struct Check_MD_Info get_modem_file_mapping get_modem_name);
@EXPORT_OK	= qw(Get_MD_X_From_Bind_Sys_Id Get_MD_YY_From_Image_Type Parse_MD_Struct Check_MD_Info get_modem_file_mapping get_modem_name);


# availible yy for specified x, the yy value is not used at this moment
our %MTK_MODEM_MAP_X_TO_YY =
(
	1 => "2g 3g wg tg lwg ltg sglte",
	2 => "2g 3g wg tg lwg ltg sglte",
	5 => "2g wg tg lwg ltg sglte"
);

our %MTK_MODEM_MAP_YY_TO_IMAGE_TYPE =
(
	"2g" => 1,
	"3g" => 2,
	"wg" => 3,
	"tg" => 4,
	"lwg" => 5,
	"ltg" => 6,
	"sglte" => 7
);

# input: bind_sys_id
# output: x
sub Get_MD_X_From_Bind_Sys_Id
{
	my $input = shift @_;
	if (exists $MTK_MODEM_MAP_X_TO_YY{$input})
	{
		return $input;
	}
	elsif ($input == 0)
	{
		return 1;
	}
	else
	{
		die "Invalid modem bind_sys_id = $input";
	}
}

# input: image_type
# output: yy
sub Get_MD_YY_From_Image_Type
{
	my $input = shift @_;
	my $output = "";
	foreach my $key (keys %MTK_MODEM_MAP_YY_TO_IMAGE_TYPE)
	{
		if ($MTK_MODEM_MAP_YY_TO_IMAGE_TYPE{$key} == $input)
		{
			$output = $key;
		}
	}
	if ($output eq "")
	{
		die "Invalid modem image_type = $input";
	}
	return $output;
}

# input: aaa/bbb.c
# output: bbb.c
sub Get_Basename
{
	my ($tmpName) = @_;
	my $baseName;
	$baseName = $1 if ($tmpName =~ /.*\/(.*)/);
	return $baseName;
}

# get modem header struct from modem image content
sub Parse_MD_Struct
{
	my ($ref_hash, $parse_md_file) = @_;

	my $buffer;
	my $length1 = 188; # modem image rear
	my $length2 = 172;
	my $length3 = 128; # modem.img project name
	my $length4 = 36;  # modem.img flavor
	my $length5 = 64;  # modem.img verno
	my $whence = 0;
	my $md_file_size = -s $parse_md_file;

	open(MODEM, "< $parse_md_file") or die "Can NOT open file $parse_md_file\n";
	binmode(MODEM);
	seek(MODEM, $md_file_size - $length1-$length3-$length4-$length5, $whence) or die "Can NOT seek to the position of modem image rear in \"$parse_md_file\"!\n";
	read(MODEM, $buffer, $length2+$length3+$length4+$length5) or die "Failed to read the rear of the file \"$parse_md_file\"!\n";
	($ref_hash->{"project_name"}, $ref_hash->{"flavor"}, $ref_hash->{"verno"}, $ref_hash->{"check_header"}, $ref_hash->{"header_verno"}, $ref_hash->{"product_ver"}, $ref_hash->{"image_type"}, $ref_hash->{"platform"}, $ref_hash->{"build_time"}, $ref_hash->{"build_ver"}, $ref_hash->{"bind_sys_id"}) = unpack("A128 A36 A64 A12 L L L A16 A64 A64, L", $buffer);
	if ($ref_hash->{'check_header'} ne "CHECK_HEADER")
	{
		print "!!!" . $ref_hash->{'check_header'} . "!!!\n";
		warn "Reading from MODEM failed! No CHECK_HEADER info!\n";
		delete $ref_hash->{"project_name"};
		delete $ref_hash->{"flavor"};
		delete $ref_hash->{"verno"};
		delete $ref_hash->{"check_header"};
		delete $ref_hash->{"header_verno"};
		delete $ref_hash->{"product_ver"};
		delete $ref_hash->{"image_type"};
		delete $ref_hash->{"platform"};
		delete $ref_hash->{"build_time"};
		delete $ref_hash->{"build_ver"};
		delete $ref_hash->{"bind_sys_id"};
		return 0;
	}
	elsif ($ref_hash->{"header_verno"} < 2)
	{
		delete $ref_hash->{"project_name"};
		delete $ref_hash->{"flavor"};
		delete $ref_hash->{"verno"};
	}
	close(MODEM);
	return 1;
}

sub Check_MD_Info
{
	my $ref_hash	= shift @_;
	my $md_id		= shift @_;
	my $MD_IMG		= shift @_;
	my $debug		= shift @_;
#######################################
# Check if MODEM file exists
#######################################
	print "\$MD_IMG = $MD_IMG\n";
	die "[MODEM CHECK FAILED]: The file \"$MD_IMG\" does NOT exist!\n" if (! -e $MD_IMG);

#######################################
# Read mode(2G/3G), debug/release flag, platform info, project info, serial number, etc. from modem.img
#######################################
	my $res = &Parse_MD_Struct($ref_hash, $MD_IMG);
	if ($res != 1)
	{
		die;
	}
	my $MD_IMG_DEBUG = $ref_hash->{"product_ver"};
	my $MD_IMG_MODE = $ref_hash->{"image_type"};
	my $MD_IMG_PLATFORM = $ref_hash->{"platform"};
	my $MD_IMG_PROJECT_ID = $ref_hash->{"build_ver"};
	my $MD_IMG_SERIAL_NO = $ref_hash->{"bind_sys_id"};

#######################################
# Output debug information
#######################################
	if ($debug)
	{
		print "*** Info from $md_id modem image ***\n\n";
		print "modem image is $MD_IMG\n";
		print "\$MD_IMG_DEBUG = $MD_IMG_DEBUG [" . sprintf("0x%08x",$MD_IMG_DEBUG) . "]\n";
		print "\$MD_IMG_MODE = $MD_IMG_MODE [" . sprintf("0x%08x",$MD_IMG_MODE) . "]\n";
		print "\$MD_IMG_PLATFORM = $MD_IMG_PLATFORM\n";
		print "\$MD_IMG_PROJECT_ID = $MD_IMG_PROJECT_ID\n";
		print "\$MD_IMG_SERIAL_NO = $MD_IMG_SERIAL_NO [" . sprintf("0x%08x",$MD_IMG_SERIAL_NO) . "]\n";
	}
	return 0;
}

# get modem image name for _x_yy_z part from modem image content
sub get_modem_suffix
{
	my $ref_header	= shift @_;
	my $flag_force	= shift @_;
	my $naming_string;
	my $MD_HEADER_VERNO		= $ref_header->{"header_verno"};
	my $MD_IMG_SERIAL_NO	= $ref_header->{"bind_sys_id"};
	my $MD_IMG_MODE			= $ref_header->{"image_type"};
	if (($flag_force == 2) || (($flag_force != 1) && ($MD_HEADER_VERNO == 2)))
	{
		# "*_x_yy_z"
		$naming_string = "_" . Get_MD_X_From_Bind_Sys_Id($MD_IMG_SERIAL_NO);
		$naming_string .= "_" . Get_MD_YY_From_Image_Type($MD_IMG_MODE);
		$naming_string .= "_n";	# Temp: all images are "n"
	}
	elsif (($flag_force == 1) || (($flag_force != 2) && ($MD_HEADER_VERNO == 1)))
	{
		if (($MD_IMG_SERIAL_NO == 1) || ($MD_IMG_SERIAL_NO == 0))
		{
			# as is
		}
		elsif ($MD_IMG_SERIAL_NO == 2)
		{
			$naming_string = "_sys2";
		}
		else
		{
			die "Invalid modem bind_sys_id = $MD_IMG_SERIAL_NO";
		}
	}
	else
	{
		die "Invalid modem header_verno = $MD_HEADER_VERNO";
	}
	return $naming_string;
}

sub get_modem_name
{
	my $ref_option	= shift @_;
	my $bind_sys_id	= shift @_;
	my $flag_force;
	my $image_type;
	if (exists $ref_option->{"MTK_MD" . $bind_sys_id . "_SUPPORT"})
	{
		if ($ref_option->{"MTK_MD" . $bind_sys_id . "_SUPPORT"} eq "modem_2g")
		{
			$image_type = 1;
			$flag_force = 1;
		}
		elsif ($ref_option->{"MTK_MD" . $bind_sys_id . "_SUPPORT"} eq "modem_3g")
		{
			$image_type = 2;
			$flag_force = 1;
		}
		else
		{
			$image_type = $ref_option->{"MTK_MD" . $bind_sys_id . "_SUPPORT"};
			$flag_force = 2;
		}
	}
	elsif ($bind_sys_id == 1)
	{
		if ($ref_option->{"MTK_MODEM_SUPPORT"} eq "modem_2g")
		{
			$image_type = 1;
			$flag_force = 1;
		}
		elsif ($ref_option->{"MTK_MODEM_SUPPORT"} eq "modem_3g")
		{
			$image_type = 2;
			$flag_force = 1;
		}
	}
	my %temp_feature = ("bind_sys_id" => $bind_sys_id, "image_type" => $image_type, "header_verno" => $flag_force);
	my $naming_string = "modem" . &get_modem_suffix(\%temp_feature, $flag_force) . ".img";
	return $naming_string;
}

sub set_modem_name
{
	my $ref_hash	= shift @_;
	my $ref_array	= shift @_;
	my $over_method	= shift @_;
	my $over_base	= shift @_;
	my $over_suffix	= shift @_;
	my $over_ext	= shift @_;
	foreach my $file (@$ref_array)
	{
		my $basename;
		my $dirname;
		my $extname;
		($basename, $dirname, $extname) = fileparse($file, qr/\.[^.]*/);
		if ($over_method == 0)
		{
			# add suffix in basename
			$basename = $over_base if ($over_base ne "*");
			$extname = $over_ext if ($over_ext ne "*");
		}
		elsif ($over_method == 1)
		{
			# add suffix in filename
			$basename .= $extname;
			$extname = "";
		}
		elsif ($over_method == 2)
		{
			# no change
			$over_suffix = "";
		}
		$ref_hash->{$file} = $basename . $over_suffix . $extname;
	}
}

sub find_modem_bin
{
	my $ref_hash_filelist	= shift @_;
	my $ref_hash_feature	= shift @_;
	my $path_of_bin			= shift @_;
	my $modem_bin_pattern = "*_PCB01_*.bin";
	my $modem_bin_file;
	my $modem_bin_link;
	$ref_hash_feature->{"TYPE_OF_BIN"} = 0;
	my @wildcard_list = &find_modem_glob($path_of_bin . $modem_bin_pattern);
	if (scalar @wildcard_list == 1)
	{
		my $wildcard_file = $wildcard_list[0];
		if ((! -d $wildcard_file) && (-e $wildcard_file))
		{
			$modem_bin_link = $wildcard_file;
			$ref_hash_feature->{"TYPE_OF_BIN"} |= 0b00000001;
		}
		if ((-d $wildcard_file) && (-e $wildcard_file . "/ROM"))
		{
			$modem_bin_link = $wildcard_file . "/ROM";
			$ref_hash_feature->{"TYPE_OF_BIN"} |= 0b00000010;
		}
	}
	elsif (scalar @wildcard_list == 0)
	{
		if (-e $path_of_bin . "modem_sys2.img")
		{
			$modem_bin_link = $path_of_bin . "modem_sys2.img";
		}
		if (-e $path_of_bin . "modem.img")
		{
			$modem_bin_link = $path_of_bin . "modem.img";
		}
	}
	else
	{
		print "[ERROR] More than one modem images are found: " . join(" ", @wildcard_list) . "\n";
	}
	if (-e $path_of_bin . "modem.img")
	{
		$modem_bin_file = $path_of_bin . "modem.img";
		$ref_hash_feature->{"TYPE_OF_BIN"} |= 0b00000100;
	}
	else
	{
		$modem_bin_file = $modem_bin_link;
	}
	return ($modem_bin_file, $modem_bin_link);
}

sub find_modem_mak
{
	my $ref_hash_filelist	= shift @_;
	my $ref_hash_feature	= shift @_;
	my $path_of_mak			= shift @_;
	my $modem_mak_pattern = "*.mak";
	my $modem_mak_file;
	my @wildcard_list = &find_modem_glob($path_of_mak . $modem_mak_pattern);
	foreach my $wildcard_file (@wildcard_list)
	{
		if ($wildcard_file =~ /\bMMI_DRV_DEFS\.mak/)
		{
		}
		elsif ($wildcard_file =~ /~/)
		{
			$modem_mak_file = $wildcard_file;
		}
		elsif ($modem_mak_file eq "")
		{
			$modem_mak_file = $wildcard_file;
		}
		else
		{
			print "[WARNING] Unknown project makefile: " . $wildcard_file . " or " . $modem_mak_file . "\n";
		}
	}
	return $modem_mak_file;
}

sub get_modem_file_mapping
{
	my $ref_hash_filelist	= shift @_;
	my $ref_hash_feature	= shift @_;
	my $ref_hash_option		= shift @_;
	my $project_root		= shift @_;
	my $type_of_load		= shift @_;
	my $branch_of_ap		= shift @_;

	my $path_of_bin;
	my $path_of_database;
	my $modem_bin_file;
	my $modem_bin_link;
	my $modem_bin_suffix;
	my $modem_mak_file;
	my @checklist_of_general;
	my @checklist_of_dsp_bin;
	my @checklist_of_append;
	my @checklist_of_remain;

	my $rule_of_rename = 2;
	if (($branch_of_ap eq "") or ($branch_of_ap =~ /KK/))
	{
		$rule_of_rename = 2;
	}
	elsif ($branch_of_ap =~ /ALPS\.(JB|JB2)\./)
	{
		$rule_of_rename = 1;
	}
	elsif ($branch_of_ap =~ /ALPS\.(ICS|GB)\d*\./)
	{
		$rule_of_rename = 1;
	}
	else
	{
		$rule_of_rename = 2;
	}

	$project_root =~ s/\/$//;
	if ($type_of_load == 0)
	{
		$path_of_bin = $project_root . "/" . "bin/";
	}
	else
	{
		$path_of_bin = $project_root . "/";
	}
	($modem_bin_file, $modem_bin_link) = &find_modem_bin($ref_hash_filelist, $ref_hash_feature, $path_of_bin);
	$modem_mak_file = &find_modem_mak($ref_hash_filelist, $ref_hash_feature, $path_of_bin);
	if (($modem_bin_file eq "") or ($modem_bin_link eq ""))
	{
		die "The modem bin is not found";
	}
	elsif (! -e $modem_bin_file)
	{
		die "The file " . $modem_bin_file . " does NOT exist!";
	}
	elsif ($ref_hash_feature->{"TYPE_OF_BIN"} == 0b00000100)
	{
		# BACH
		$modem_bin_suffix = "";
		# hard code for PMS
		$ref_hash_feature->{"bind_sys_id"} = 1;
		if ($ref_hash_option)
		{
			$ref_hash_option->{"PURE_AP_USE_EXTERNAL_MODEM"} = "yes";
			if ($modem_bin_link =~ /modem_sys2\.img/)
			{
				$ref_hash_option->{"MT6280_SUPER_DONGLE"} = "no";
			}
			else
			{
				$ref_hash_option->{"MT6280_SUPER_DONGLE"} = "yes";
			}
		}
	}
	else
	{
		my $res = &Parse_MD_Struct($ref_hash_feature, $modem_bin_file);
		if ($res == 1)
		{
			$modem_bin_suffix = &get_modem_suffix($ref_hash_feature, $rule_of_rename);
			$ref_hash_feature->{"suffix"} = $modem_bin_suffix;
			print "Modem bin file is " . $modem_bin_file . "\n";
			&set_modem_name($ref_hash_filelist, [$modem_bin_file], 0, "modem", $modem_bin_suffix, ".img");
			if ($ref_hash_option)
			{
				$ref_hash_option->{"MTK_ENABLE_MD" . $ref_hash_feature->{"bind_sys_id"}} = "yes";
				$ref_hash_option->{"MTK_MD" . $ref_hash_feature->{"bind_sys_id"} . "_SUPPORT"} = $ref_hash_feature->{"image_type"};
			}
		}
		else
		{
			$ref_hash_feature->{"TYPE_OF_BIN"} = 0b00001000;
		}
	}
	if ($ref_hash_filelist)
	{
		if ($ref_hash_feature->{"TYPE_OF_BIN"} == 0b00000100)
		{
			# BACH
			push(@checklist_of_remain, $path_of_bin . "modem*.database");
			push(@checklist_of_remain, $path_of_bin . "boot.img");
			push(@checklist_of_remain, $path_of_bin . "configpack.bin");
			push(@checklist_of_remain, $path_of_bin . "fc-*.bin");
			push(@checklist_of_remain, $path_of_bin . "ful.bin");
			push(@checklist_of_remain, $path_of_bin . "md_bl.bin");
			push(@checklist_of_remain, $path_of_bin . "modem*.img");
			push(@checklist_of_remain, $path_of_bin . "preloader.bin");
			push(@checklist_of_remain, $path_of_bin . "SECURE_RO");
			push(@checklist_of_remain, $path_of_bin . "system.img");
			push(@checklist_of_remain, $path_of_bin . "uboot.bin");
			push(@checklist_of_remain, $path_of_bin . "userdata.img");
			push(@checklist_of_remain, $path_of_bin . "*.cfg");
			if (-e $path_of_bin . "catcher_filter.bin")
			{
				&set_modem_name($ref_hash_filelist, [$path_of_bin . "catcher_filter.bin"], 0, "*", "_ext", "*");
			}
			else
			{
				push(@checklist_of_remain, $path_of_bin . "catcher_filter_ext.bin");
			}
		}
		elsif ($ref_hash_feature->{"TYPE_OF_BIN"} == 0b00001000)
		{
			# MAUI DSDA
			if ($type_of_load == 2)
			{
				$path_of_database = $project_root . "/" . "../../" . "tst/database_classb/";
			}
			else
			{
				$path_of_database = $project_root . "/";
			}
			push(@checklist_of_remain, $path_of_database . "BPLGUInfoCustom*");
			my @wildcard_list = &find_modem_cfg(\@checklist_of_remain, dirname($modem_bin_link) . "/");
			if (scalar @wildcard_list == 1)
			{
				&set_modem_name($ref_hash_filelist, \@wildcard_list, 0, "EXT_MODEM_BB", "", "*");
			}
			else
			{
				die scalar @wildcard_list . " unexpected cfg: " . join(" ", @wildcard_list);
			}
			if (-e $path_of_database . "catcher_filter.bin")
			{
				&set_modem_name($ref_hash_filelist, [$path_of_database . "catcher_filter.bin"], 0, "*", "_ext", "*");
			}
			else
			{
				push(@checklist_of_remain, $path_of_bin . "catcher_filter_ext.bin");
			}
		}
		elsif (($ref_hash_feature->{"image_type"} >= 5) and ($ref_hash_feature->{"image_type"} <= 7))
		{
			# LTE
			if ($type_of_load == 0)
			{
				$path_of_database = $project_root . "/" . "dhl/database/";
			}
			else
			{
				$path_of_database = $project_root . "/";
			}
			push(@checklist_of_append, dirname($modem_bin_link) . "/" . "SECURE_RO") if (-e dirname($modem_bin_link) . "/" . "SECURE_RO");
			push(@checklist_of_append, $path_of_database . "BPLGUInfoCustom*");
			push(@checklist_of_append, $path_of_database . "DbgInfo*");
			push(@checklist_of_general, $path_of_database . "catcher_filter.bin");
			push(@checklist_of_general, $path_of_database . "mcddll.dll");
			# MT6575/MT6577
			push(@checklist_of_general, dirname($modem_bin_link) . "/" . "DSP_BL") if (-e dirname($modem_bin_link) . "/" . "DSP_BL");
			push(@checklist_of_general, dirname($modem_bin_link) . "/" . "DSP_ROM") if (-e dirname($modem_bin_link) . "/" . "DSP_ROM");
			# MT6582/MT6592/MT6595
			push(@checklist_of_dsp_bin, $path_of_bin . "*DSP*.bin");
		}
		elsif (($ref_hash_feature->{"image_type"} >= 1) and ($ref_hash_feature->{"image_type"} <= 4))
		{
			# WR8
			if ($type_of_load == 0)
			{
				$path_of_database = $project_root . "/" . "tst/database/";
			}
			else
			{
				$path_of_database = $project_root . "/";
			}
			push(@checklist_of_append, dirname($modem_bin_link) . "/" . "SECURE_RO") if (-e dirname($modem_bin_link) . "/" . "SECURE_RO");
			push(@checklist_of_append, $path_of_database . "BPLGUInfoCustom*");
			push(@checklist_of_append, $path_of_database . "DbgInfo*");
			push(@checklist_of_general, $path_of_database . "catcher_filter.bin");
			push(@checklist_of_general, $path_of_database . "mcddll.dll");
			# MT6575/MT6577
			push(@checklist_of_general, dirname($modem_bin_link) . "/" . "DSP_BL") if (-e dirname($modem_bin_link) . "/" . "DSP_BL");
			push(@checklist_of_general, dirname($modem_bin_link) . "/" . "DSP_ROM") if (-e dirname($modem_bin_link) . "/" . "DSP_ROM");
			# MT6582/MT6592
			push(@checklist_of_dsp_bin, $path_of_bin . "*DSP*.bin") if (&find_modem_glob($path_of_bin . "*DSP*.bin"));
		}
		else
		{
			die "Unknown modem spec";
		}
		foreach my $file (@checklist_of_general)
		{
			my @wildcard_list = &find_modem_glob($file);
			&set_modem_name($ref_hash_filelist, \@wildcard_list, 0, "*", $modem_bin_suffix, "*");
		}
		foreach my $file (@checklist_of_append)
		{
			my @wildcard_list = &find_modem_glob($file);
			&set_modem_name($ref_hash_filelist, \@wildcard_list, 1, "*", $modem_bin_suffix, "*");
		}
		foreach my $file (@checklist_of_remain)
		{
			my @wildcard_list = &find_modem_glob($file);
			&set_modem_name($ref_hash_filelist, \@wildcard_list, 2, "*", "", "*");
		}
		foreach my $file (@checklist_of_dsp_bin)
		{
			my @wildcard_list = &find_modem_glob($file);
			if (scalar @wildcard_list == 1)
			{
				&set_modem_name($ref_hash_filelist, \@wildcard_list, 0, "dsp", $modem_bin_suffix, "*");
			}
			else
			{
				die scalar @wildcard_list . " unexpected dsp bin: " . join(" ", @wildcard_list);
			}
		}
		if ($modem_mak_file ne "")
		{
			my @wildcard_list = &find_modem_glob($modem_mak_file);
			&set_modem_name($ref_hash_filelist, \@wildcard_list, 0, "modem", $modem_bin_suffix, "*");
		}
	}
}

sub find_modem_cfg
{
	my $ref_hash_filelist	= shift @_;
	my $path_of_bin			= shift @_;
	my $READ_HANDLE;
	my @files = &find_modem_glob($path_of_bin . "*.cfg");
	foreach my $file (@files)
	{
		if (open($READ_HANDLE, "<$file"))
		{
			my @lines = <$READ_HANDLE>;
			close $READ_HANDLE;
			foreach my $line (@lines)
			{
				if ($line =~ /^\s*-\s*file:\s*(\S+)/)
				{
					push(@$ref_hash_filelist, $path_of_bin . $1) if ($ref_hash_filelist);
				}
			}
		}
	}
	return @files;
}

sub find_modem_glob
{
	my @dirs = @_;
	my @result;
	foreach my $dir (@dirs)
	{
		$dir =~ s/([\[\]\(\)\{\}\^])/\\$1/g;
		push(@result, glob $dir);
	}
	return @result;
}

return 1;

