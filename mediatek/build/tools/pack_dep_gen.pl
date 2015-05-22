#!/usr/bin/env perl

use strict;
use Cwd qw(abs_path getcwd);
use File::Basename;
use File::Path;

my $flag_print = 0;
my $flag_create = 2;
my $flag_modis = 0;
my $flag_overwrite = 0;
if ($ARGV[0] =~ /^--append$/i)
{
	$flag_create = 1;
	shift @ARGV;
}
elsif ($ARGV[0] =~ /^--extract$/i)
{
	$flag_create = 0;
	shift @ARGV;
}
elsif ($ARGV[0] =~ /^--modis$/i)
{
	$flag_modis = 2;
	shift @ARGV;
}
elsif ($ARGV[0] =~ /^--overwrite$/i)
{
	$flag_overwrite = 1;
	shift @ARGV;
}


if ($#ARGV < 3)
{
	die "Usage: pack_dep_gen.pl output.dep target.obj input/parse/dir regular_expression [file_prefix]\n";
}

my $depFile = shift @ARGV;
my $objFile = shift @ARGV;
my $depDirs = shift @ARGV;
my $filters = shift @ARGV;
my $preDirs = shift @ARGV;

my $current = getcwd();
#$current =~ s/\\/\//g;
$current =~ s/\/$//g;
$current = quotemeta($current);
$preDirs .= "/" if ($preDirs ne "");
$preDirs =~ s/\/\//\//g;

if ($flag_create > 0)
{
	my @cgenDep;
	&CollectDepend(\@cgenDep, $depFile, $depDirs, $filters);
	#print "\n" if ($flag_modis == 0);
	&WriteDepend(\@cgenDep, $depFile, $objFile, $preDirs);
}
else
{
	&ExtractDepend($depFile, $depDirs);
}

sub CollectDepend
{
	my $refArray = shift;
	my $outputDep = shift;
	my $inputDir = shift;
	my $exprRule = shift;

	my $DIRHANDLE;
	my $FILEHANDLE;
	if (opendir($DIRHANDLE, $inputDir))
	{
	}
	else
	{
		#print "Skip for opendir " . $inputDir . "\n";
		return 0;
	}
	my @fileDep = readdir $DIRHANDLE;
	closedir $DIRHANDLE;
	#print "Processing" if ($flag_modis == 0);
	foreach my $dep (@fileDep)
	{
		next if ($dep !~ /$exprRule\b/i);
		if ($outputDep =~ /\b${dep}$/)
		{
			next if ($flag_create != 1);
		}
		#print " " . $dep if ($flag_modis == 0);
		my $line;
		open $FILEHANDLE, "<$inputDir/$dep";
		if ($dep =~ /\.lis$/i)
		{
			# for cgen.exe -src or xcopy
			while ($line = <$FILEHANDLE>)
			{
				if ($line =~ /^(\S+\.\w+)\s*$/i)
				{
					my $src = $1;
					push @$refArray, $src;
				}
			}
		}
		elsif ($dep =~ /\.log$/i)
		{
			# for MoDIS cl.exe or pregen log
			while ($line = <$FILEHANDLE>)
			{
				my $line2;
				$line =~ s/[\r\n]//g;
				if ($line =~ /\[Dependency\]\s*(.*?)\s*$/i)
				{
					$line2 = $1;
					# workaround for filename with space
					$line2 =~ s/\\ /\?/g;
				}
				elsif ($line =~ /Note: including file:\s*(.*?)\s*$/i)
				{
					$line2 = $1;
				}
				foreach my $src (split(/\s+/, $line2))
				{
					#if (($src ne "") && ($src !~ /\.obj$/i))
					if ($src =~ /\.(c|cpp)$/i)
					{
						unshift(@$refArray, $src);
					}
					elsif ($src ne "")
					{
						push(@$refArray, $src);
					}
				}
			}
		}
		else
		{
			# for Target armcc.exe
			while ($line = <$FILEHANDLE>)
			{
				next if ($line =~ /^\#/i);
				$line =~ s/[\r\n]//g;
				$line =~ s/^\s*(\S+\:)?\s*(.*?)\s*(\\)?$/$2/;
				foreach my $src (split(/\s+/, $line))
				{
					if (($src ne "") && ($src !~ /[\\\/]~/) && ($src !~ /^~/))
					{
						if ($src =~ /\.(c|cpp)$/i)
						{
							unshift(@$refArray, $src);
						}
						else
						{
							push(@$refArray, $src);
						}
					}
				}
			}
		}
		close $FILEHANDLE;
	}
}

sub WriteDepend
{
	my $refArray = shift;
	my $outputDep = shift;
	my $targetDep = shift;
	my $prefixDir = shift;
	my $objFirst = 1;
	my $outText;
	my $outTail = "\n";
	my %saw;
	my @fileDep = grep (!$saw{$_}++, @$refArray);
	my %fileSrc;
	my %fileObj;
	foreach my $dep (@fileDep)
	{
		if ($dep ne "")
		{
			$dep = $prefixDir . $dep if ($dep !~ /^[a-zA-Z]:\\/);
			$dep =~ s/[\\\/]+/\//g;
			$dep =~ s/\/\.\//\//g;
			$dep =~ s/^\.\///;
			while ($dep =~ /\w+\/\.\.\//)
			{
				$dep =~ s/\w+\/\.\.\///;
			}
			while ($dep =~ /^(\/)?\.\.\//)
			{
				$dep =~ s/^(\/)?\.\.\///;# remove "..\"
			}
			while ($dep =~ /^\w\:\/\.\.\//)
			{
				$dep =~ s/^(\w\:\/)\.\.\//$1/;# remove "k:\..\"
			}
			$dep =~ s/^$current\///i;
			if ($flag_modis)
			{
				$dep =~ s/^[k-z]\:[\/]+//i;# for subst
				if ($dep =~ /.+\/(\S+?)\.(c|cpp)$/i)
				{
					$fileSrc{lc($1)} = $dep;
					if ($^O eq "MSWin32")
					{
						$fileSrc{lc($1)} =~ s/\//\\/g;
					}
					next;
				}
				elsif ($dep =~ /.+\/(\S+?)\.obj$/i)
				{
					$fileObj{lc($1)} = $fileSrc{lc($1)};
					next;
				}
				elsif ($dep =~ /\/auto_header\.h/i)
				{
					next;
				}
			}
			if ($dep =~ /\?/)
			{
				my $dep_tmp = $dep;
				$dep_tmp =~ s/\?/ /g;
				next if (! -e $dep_tmp);
				$dep =~ s/\?/\\ /g;
			}
			else
			{
				next if (! -e $dep);
			}
			# skip temp file
			if ($objFirst)
			{
				#$outText .= "	$dep";
				$outText .=  " \\\n		$dep";
				$objFirst = 0;
			}
			else
			{
				$outText .=  " \\\n		$dep";
			}
			$outTail .= "$dep:\n";
		}
	}
	my $FILEHANDLE;
	my $outputDir = dirname($outputDep);
	mkpath($outputDir) if (! -d $outputDir);
	if ($flag_overwrite)
	{
		open $FILEHANDLE, ">>$outputDep" or die "Fail to append $outputDep\n";
	}
	else
	{
		open $FILEHANDLE, ">$outputDep" or die "Fail to write $outputDep\n";
	}
	if ($flag_modis)
	{
		foreach my $key (keys %fileSrc)
		{
			next if (exists $fileObj{$key});
			my $value = $fileSrc{$key};
			$outText .=  " \\\n		$value";
			print "include " . $value . " for " . $key ."\n" if ($flag_print);
		}
		foreach my $key (sort keys %fileObj)
		{
			print $FILEHANDLE "#UPDATE#\n";
			print $FILEHANDLE $key . ".obj: " . $fileObj{$key} . " \\\n	";
			print $FILEHANDLE $outText . "\n";
			print $FILEHANDLE "#ENDUPDATE#\n";
		}
	}
	else
	{
		print $FILEHANDLE $targetDep . ":";
		print $FILEHANDLE $outText . "\n";
		print $FILEHANDLE $outTail . "\n";
	}
	close $FILEHANDLE;
}

sub ExtractDepend
{
	my $inputDep = shift;
	my $outputDir = shift;
	my $FILEHANDLE;
	open $FILEHANDLE, "<$inputDep" or die "Fail to open $inputDep\n";
	my @fileDep = <$FILEHANDLE>;
	close $FILEHANDLE;
	my $targetDep;
	foreach my $line (@fileDep)
	{
		if ($line =~ /\#UPDATE\#/i)
		{
		}
		elsif ($line =~ /^(\S+?):\s*(\S+)\s*\\?/)
		{
			if ($targetDep ne "")
			{
				close $FILEHANDLE;
				$targetDep = "";
			}
			$targetDep = $1;
			$targetDep =~ s/^.*\\//;
			$targetDep =~ s/\.\w+$/.det/;
			open $FILEHANDLE, ">$outputDir/$targetDep" or die "Fail to write $outputDir/$targetDep\n";
			print $FILEHANDLE "\#UPDATE\#\n";
			print $FILEHANDLE $line;
		}
		elsif ($line =~ /^\s*(\S+)\s*\\?/)
		{
			print $FILEHANDLE $line;
		}
	}
	if ($targetDep ne "")
	{
		close $FILEHANDLE;
		$targetDep = "";
	}
}

