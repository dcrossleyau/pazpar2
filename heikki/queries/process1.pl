#!/usr/bin/perl -w
# Analyzing DBC's example queries
# Step 1: Split the file, merge nexpage queries


open F, "searches.csv" or die "could not open searches.csv: $!\n";

open OUT, ">x1" or die "could not open x1 for writing: $!\n";
print OUT "#query ; hits; pages\n";

my $thisline = "";
my $pagecount = 1; # how many next-page searches (same query)
while ( <F> ) {
    next unless /^[0-9]/;
    my ( $lineno, $query, $ccl, $hits, $timing ) = split (';');
    if ( $thisline eq $lineno ){
        $pagecount ++;
    } else {
        print OUT "$query ; $hits ; $pagecount\n";
        #print "$query ; $hits ; $pagecount\n";
        $pagecount = 1;
        $thisline = $lineno;
    }

    #die "STOPPING EARLY " if $lineno > 5000;
}
