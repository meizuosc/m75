#!/usr/local/bin/perl
#
#
use strict;
use File::Path;
my $ORG_SCATTER = $ARGV[0];
my $NEW_SCATTER = $ARGV[1];
my $style = 0;
my $flag = 0;
my $Preloader = 0;
my $Combo_Emmc = 0;

if (-e $NEW_SCATTER)
{
        chmod(0777, $NEW_SCATTER) or &error_handler("chmod 0777 $NEW_SCATTER fail", __FILE__, __LINE__);
        if (!unlink $NEW_SCATTER)
        {
            print("remove $NEW_SCATTER fail ", __FILE__, __LINE__);
            die;
        }
    }
    else
    {
        my $dirpath = substr($NEW_SCATTER, 0, rindex($NEW_SCATTER, "/"));
        eval { mkpath($dirpath) };
        if ($@)
        {
            print("Can not make dir $dirpath", __FILE__, __LINE__, $@);
            die;
        }
}
    
open(READ_FP, "< $ORG_SCATTER") or die "Can not open $ORG_SCATTER";
open(WRITE_FP, "> $NEW_SCATTER") or die "Can not open $NEW_SCATTER";

while (<READ_FP>) {
    if ($_ =~ /^{/) {
        $style++;
        next;
    }
    if ($_ =~ /^}/) {
        $style++;
        next;
    }
    if ($_ =~ /^__NODL_BMTPOOL /) {
        next;
    }
    if ($_ =~ /^__NODL_RSV_BMTPOOL /) {
        next;
    }
    if ($_ =~ /^__NODL_RSV_OTP /) {
        next;
    }
    if ($_ =~ /^\n/) {
        next;
    }
    print WRITE_FP "$_";
}

close(READ_FP);
close(WRITE_FP);

#new scatter format
if ($style == 0){
    open(READ_FP, "< $ORG_SCATTER") or die "Can not open $ORG_SCATTER";
    open(WRITE_FP, "> $NEW_SCATTER") or die "Can not open $NEW_SCATTER";

    while (<READ_FP>) {
		chomp $_;
		if (($_ =~ m/config_version:(.*)/)){
				my $position = rindex($_, " ")+1;
				my $config_version = substr($_, $position);
				my $version_num = (hex(substr($config_version, 1,1)))*100+(hex(substr($config_version, 3,1)))*10+hex(substr($config_version, 5,1));
				if($version_num > 111){
					$Combo_Emmc++;
				}
				next;
		}
		if (($_ =~ m/BMTPOOL(.*)/) || ($_ =~ m/OTP(.*)/)){
		    $flag++;
			next;
		}
		else{
			if ($_ =~ m/partition_name:(.*)/) {
				my $position = rindex($_, " ")+1;
				my $p_partition =  substr($_, $position)." ";
				print WRITE_FP $p_partition;
				if($p_partition =~ m/PRELOADER(.*)/){
					$Preloader++;
				}
				next;
			}
			if($_ =~ m/partition_size:(.*)/ && ($Preloader==1)&&($Combo_Emmc==1)){
				my $position = rindex($_, " ")+1;
				my $p_size =  substr($_, $position)."\n";
				print WRITE_FP $p_size;
				$Preloader--;
				next;
			}
			if ($_ =~ m/physical_start_addr:(.*)/ && ($Preloader==0)&&($Combo_Emmc==1)) {
				if($flag==1) {
				    $flag--;
				next;
			}
				else{
					my $position = rindex($_, " ")+1;
					my $p_add =  substr($_, $position)."\n";
					print WRITE_FP $p_add;
					next;
				}
			}
			if ($_ =~ m/linear_start_addr:(.*)/&&($Combo_Emmc==0)) {
				if($flag==1) {
				    $flag--;
					next;
				}
				else{
					my $position = rindex($_, " ")+1;
					my $p_add =  substr($_, $position)."\n";
					print WRITE_FP $p_add;
					next;
				}
			}
		}
	}
    close(READ_FP);
    close(WRITE_FP);
}
