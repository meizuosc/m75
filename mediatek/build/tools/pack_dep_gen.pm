package pack_dep_gen;
use strict;
use Cwd qw(abs_path);
use Exporter;
use vars qw(@ISA @EXPORT @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT = qw(PrintDependency PrintDependModule);
@EXPORT_OK = qw(PrintDependency PrintDependModule);

return 1;

sub PrintDependency
{
	foreach my $file (@_)
	{
		if (-e $file)
		{
			my $file_full = abs_path($file);
			print STDERR "[Dependency] $file_full\n";
		}
	}
}

sub PrintDependModule
{
	if (scalar @_ > 0)
	{
		PrintDependency(@_);
	}
	else
	{
		PrintDependency($0);
	}
	foreach my $value (values %INC)
	{
		if ($value =~ /\/usr\/(.+)\/perl\//i)
		{
			# skip system module
		}
		else
		{
			PrintDependency($value);
		}
	}
}

