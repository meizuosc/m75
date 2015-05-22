#!/usr/bin/perl -w
use strict;
use Data::Dumper;
use Digest::MD5 qw(md5_hex);
use Getopt::Long;

my @to_drop;
my $start_regex      = qr/^\s* \/\*+ \s*/x;
my $start_regex_not  = qr/\/\* .* \*\//x;
my $end_regex        = qr/\*\/ \s*$/x;
my $end_regex_not    = $start_regex_not;
my $legal_regex   = qr/
                       (MEDIATEK\s+SOFTWARE\s+AT\s+ISSUE) | 
                       (MEDIATEK\s+EXPRESSLY\s+DISCLAIMS) |
                       (protected\s+by\s+Copyright.*mediatek) |
                       (proprietary\s+to\s+MediaTek) |
                       (permission\s+of\s+MediaTek\s+Inc) |
                       (Software\s+License\s+Agreement\s+with[\n\s\*]+Mediatek) |
                       (mediatek.*do\s+not\+modify) 
                    /six;
my $legal_regex_not = qr/ (\bGNU\b) |
                          (mediatek\/)
                      /six;
#(Copyright\s+\(c\)\s+\d+[\n\s\*]+MediaTek\s+Inc)
my ($OPT_DRYRUN, $OPT_VERBOSE) = ("") x 2;
GetOptions(
    "d" => \$OPT_DRYRUN,
    "v" => \$OPT_VERBOSE,
);

for my $src (@ARGV) {
    
    next unless -T $src;
    open my $fh, $src or die "Openning $src failed: $!";
    my $comment_start   = 0;
    my $comment_end     = 0;
    my @blocks;

    my $lns = "";
    my @flines = <$fh>;
    if ((join "", @flines) =~ /mediatek/si) {
        print "[hit] $src\n" if $OPT_VERBOSE;
    }
    else {
        print "[skip] $src\n" if $OPT_VERBOSE;
        next;
    }
    for (@flines) {
        if (/$start_regex/ and not /$start_regex_not/) {    # the line where comment string starts
            $comment_start = 1;
            $comment_end   = 0;
        }
        elsif (/$end_regex/ and not /$end_regex_not/) {    # the line where comment string ends
            $comment_start = 0;
            $comment_end   = 1;
        }
#        elsif (/$start_regex_not/) {
#            $comment_oneline = 1;
#        }
        else {
            $comment_end = 0;
        }

        # verbose
        if ($OPT_VERBOSE) {
            printf "$comment_start $comment_end\t";
            print "[$.]$_";
        }

        if ($comment_start) {
            $lns .= $_;
        }
        elsif ($comment_end) {
            $lns .= $_;
            if ( $lns =~ /$legal_regex/ and $lns !~ /$legal_regex_not/ ) {
                push @to_drop,
                  {
                    lns      => $lns,
                    type     => "legal",
                    digest   => md5_hex($lns),
                    checksum => length($lns),
                    file     => $src
                  };
            }
            else {
                push @blocks, { lns => $lns, type => "code" };
            }
            $lns = "";
        }
        else {
            push @blocks, { lns => $_, type => "code" };
        }
    }

    # if there is no comment_end line found at last
    if ($lns ne "") {
        push @blocks, { lns => $lns, type => "code" };
    }

    while ( my $ln = shift @blocks ) {
        if ( $ln->{lns} !~ /^\s*$/ ) {
            unshift @blocks, $ln;
            last;
        }
    }
    unless ($OPT_DRYRUN) {
        chmod 0755, $src;
        open $fh, ">", $src or die "Openning $src failed: $!";
        for (@blocks) {
            print $fh $_->{lns};
        }
    }
}

for (@to_drop) {
    printf "%s:%s:%s\n", $_->{checksum}, $_->{digest}, $_->{file};
}
