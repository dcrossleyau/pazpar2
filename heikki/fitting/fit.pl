#!/usr/bin/perl -w
# fit.c - experiments in curve fitting
# for pazpar'2 ranking normalizing

# We have a number of data points ( position, score) from
# different sources. The task is to normalize them so that
# they all fall near the curve y=1/p, where p is the position
# This is done by adjusting the ranks R so that Rn = aR+b
# We need to find parameters a,b so as to minimize the chi-
# squared difference from y=1/p


my $plotnr = 1; # number the tmp files for plotting
my $plotcmd = ""; # the plot commands for gnuplot

# Calculate the (squared) difference from the normalized rank to the 1/n function
# Params
#  p = position (x)
#  r = rank, not normalized
#  a,b normalizing params
sub diff {
    my ( $p, $r, $a, $b ) = @_;
    my $rn = $r * $a + $b;
    my $f = 1.0 / $p;  # target value
    my $d = $rn - $f;
    return $d * $d;
}

# Read and process one data file
# Just one float number per line, nothing else
sub onefile {
    my $fn = shift;
    my @d;
    open F, $fn or die "Could not open $fn: $!\n";
    my $n = 1; # number of data points
    my $first;
    my $last;
    my $title;
    while ( <F> ){
        chomp();
        $title = $_ unless defined($title);
        next unless /^[0-9]/; # skip comments etc
        my $v = 1.0 * $_ ;
        $first = $v unless defined($first);
        $last = $v;
        #print "Data $n is $v\n";
        $d[$n++] = $v;
    }
    $title =~ s/^[# ]+//; # clean the '#' and leading space
    print "$fn: '$title' $n points: $first - $last \n";
    # Initial guess Rn = a*R + b
    my $a = 1.0 / $first;
    my $b = - $last;
    # step sizes for a and b
    my $da = $a / 3;
    my $db = - $b / 3;
    my $iteration = 0;
    my $prev = 0.0;
    while (1) {
        $iteration++;
        # 5 sums: at (a,b) (a+,b), (a-,b), (a,b+), (a,b-)
        my $sab = 0.0; # at a,b
        my $sap = 0.0; # at a+da,b
        my $sam = 0.0; # at a-da,n
        my $sbp = 0.0; # at a, b+db
        my $sbm = 0.0; # at a, b-db
        for ( my $p = 1 ; $p < $n; $p++ ) {
            $sab += diff( $p, $d[$p], $a, $b );
            $sap += diff( $p, $d[$p], $a+$da, $b );
            $sam += diff( $p, $d[$p], $a-$da, $b );
            $sbp += diff( $p, $d[$p], $a, $b+$db );
            $sbm += diff( $p, $d[$p], $a, $b-$db );
        }
        my $dif = $sab - $prev;
        #print "iteration $iteration: a=$a +- $da   b=$b +- $db chisq=$sab dif=$dif\n";
        if ( (abs($da) < abs($a)/100.0 && abs($db) < abs($b)/100.0) ||
             ($iteration >= 100 ) ||
             (abs($dif) < 0.00001 ) ) {
            print "it-$iteration: a=$a +- $da   b=$b +- $db chisq=$sab dif=$dif\n";
            last;
        }
        $prev = $sab;
        # adjust a
        if ( $sap < $sab && $sap < $sam ) {
            $a += $da;
        } elsif ( $sam < $sab && $sam < $sap ) {
            $a -= $da;
        } else {
            $da = $da /2;
        }
        $da = $da * 0.99;
        # adjust b
        if ( $sbp < $sab && $sbp < $sbm ) {
            $b += $db;
        } elsif ( $sbm < $sab && $sbm < $sbp ) {
            $b -= $db;
        } else {
            $db = $db /2;
        }
        $db = $db * 0.99;
    }

    # plot the file
    my $pf = "/tmp/plot.$plotnr.data";
    $plotnr++;
    open PF, ">$pf" or die "Could not open plot file $pf: $!\n";
    for ( my $p = 1 ; $p < $n; $p++ ) {
        my $rn = $d[$p] * $a + $b;
        print PF "$p $rn\n";
    }
    close PF;
    $plotcmd .= "," if ($plotcmd);
    $plotcmd .= "\"$pf\" using 1:2 with points title \"$title\"";

    

}

# main

if ( !defined($ARGV[0]) ) {
    die "Need at least one file to plot\n";
}
while ($ARGV[0]) {
  onefile( $ARGV[0] );
  shift(@ARGV);
}
my $cmd =
    "set term png\n" .
    "set out \"plot.png\" \n" .
    "plot $plotcmd \n";

print "$cmd \n";

open GP, "| gnuplot" or die "Could not open a pipe to gnuplot: $!\n";
print GP $cmd;
close GP;