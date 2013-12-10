#!/usr/bin/perl -w
# Analyzing DBC's example queries
# Step 3: Eliminate search terms
# Assumes x3 is the result of process2.pl
# Result should be sorted and passed to the next step


open F, "x3" or die "could not open x3: $!\n";

my $linecount = 0;
my %counts;
while ( <F> ) {
    next if /^#/;
    chomp();
    $linecount ++;
    #last if ($linecount >10);
    my ( $hits, $query) = split (';');
    $query =~ s/^ +//;
    $query =~ s/ +$//;
    #print "$_ : '$query'\n";
    my $nq = "";
    for my $t ( split(' ',$query) ) {
        #print "  '$t'\n";
        if ( $t ne "og" && $t ne "eller" && $t ne "ikke" ) {
            $t =~ s/[^ =]+/x/g;
        }
        $nq .= " " if ($nq);
        $nq .= $t;
    }
    $counts{$nq} = 0 unless defined($counts{$nq});
    $counts{$nq} += $hits;
    print "$nq: $hits $counts{$nq}\n";
}
close F;

open OUT, ">x5" or die "could not open sort>x5 for writing: $!\n";
my $thisq = "";
my $sum = 0;
for my $q ( sort { $counts{$b} <=> $counts{$a} } keys(%counts) ) {
    print "q='$q'  n=$counts{$q} \n";
    print OUT "$counts{$q}; $q\n";
}
close OUT;
