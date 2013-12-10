#!/usr/bin/perl -w
# Analyzing DBC's example queries
# Step 2: Count identical queries, and query types
# Assumes x2 is the result of process1.pl, sorted alphabetically
# 


open F, "x2" or die "could not open x2: $!\n";

open OUT, "| sort -n -r > x3" or die "could not open sort>x3 for writing: $!\n";
print OUT "#count ; query \n";

my $thisquery = "";
my $count = 1; # how many times seen the same query
my $totalqueries = 0;
my $booleans = 0; # queries that contain a boolean operator
my $fields = 0; # queries that start with field=, f.ex. title or author
my $multifields = 0; # queries that contain more than one field
my $simplequeries = 0; # queries without booleans or fields
my $uniquequeries = 0;
my $singlehits = 0;
my $singleterms = 0;
my $doubleterms = 0;
my $tripleterms = 0;
my $manyterms = 0;
my %field;
while ( <F> ) {
    next if /^#/;
    my ( $query, $hits, $pages ) = split (';');
    next if ( $hits =~ /["a-z]/); # semicolons in original query, ignore
    if ( $thisquery eq $query ){
        $count ++;
    } else {
        print OUT "$count ; $thisquery \n";
        $totalqueries += $count;
        $uniquequeries += 1;
        $singlehits += 1 if ($hits <= 1 );
        my $is_simple = 1;
        my $is_boolean = 0;
        if ( / og /i || / eller /i || / ikke /i ) {
            $booleans++;
            $is_simple = 0;
            $is_boolean = 1;
        }
        my $fieldcount = 0;
        while ( $query =~ /([^ (=%'"]+)=\s*([^ ]+)/g ) {
            if (++$fieldcount >1 ) {
                $is_simple = 0;
                print "OOPS: $fieldcount $query \n" unless $is_boolean;
                $multifields += $count;
            }
            my $fld = $1;
            $fields += $count;
            #print "Field loop: '$fld' $query \n";
            $fld = lc($fld);
            $field{$fld} = 0 unless defined($field{$fld});
            $field{$fld} += $count;
        }
        if ($is_simple) {
            $simplequeries += $count;
        }
        my $q2 = $query;
        $q2 =~ s/\S+=//; # remove fields
        $q2 =~ s/ og //;
        $q2 =~ s/ eller //;
        $q2 =~ s/ ikke //;
        $singleterms += $count if ($q2 =~ /^\s*\S+\s*$/ );
        $doubleterms += $count if ($q2 =~ /^\s*\S+\s+\S+\s*$/ );
        $tripleterms += $count if ($q2 =~ /^\s*\S+\s+\S+\s+\S+\s*$/ );
        $manyterms   += $count if ($q2 =~ /^\s*\S+\s+\S+\s+\S+\s+\S+/ );
        #print "$query ; $hits ; $pagecount\n";
        $count = 1;
        $thisquery = $query;
    }
}
close OUT;

open OUT, ">x4" or die "could not open x4 for writing summary: $!\n";

sub line {
    my ($capt, $number, $dopercent ) = @_;
    my $percents = "";
    if ( $dopercent ) {
        $percents = "". int( $number*1000 / $totalqueries ) / 10 . "%" ;
    }
    while (length($percents) < 6 ) { $percents = " $percents"; }
    print OUT sprintf("%-20s %7d %s\n", $capt, $number, $percents);
}

line "Total queries", $totalqueries, 0;
line "Unique queries", $uniquequeries, 1;

line "Boolean queries",  $booleans, 1;
line "Fielded queries",  $fields, 1;
line "Multiple fields",  $multifields, 1;
line "Simple queries",  $simplequeries, 1;
line "One-hit queries", $singlehits, 1;
print OUT "\n";
line "Single term",  $singleterms, 1;
line "Double terms",  $doubleterms, 1;
line "Triple terms",  $tripleterms, 1;
line "Many terms",  $manyterms, 1;

print OUT "\nFields\n";
for my $k (sort{ $field{$b} <=> $field{$a} } keys(%field) ) {
    line "  $k", $field{$k}, 1 if ($field{$k} > 100);
}