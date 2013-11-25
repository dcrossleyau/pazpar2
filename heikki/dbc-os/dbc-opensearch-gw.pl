#!/usr/bin/perl -w

# DBC-OPENSEARCH gateway
# Z39.50 server (and SRU and...) and a client to DBC's opensearch
#
# Based on DBC's Primo gateway
#
# Supports sortby with one argument with or without /ascending/descending
# These are translated to parameters to the opensearch server. The rest of
# the query is passed on verbatim.
#
# See DBC-45 in our Jira
#
# Programmed by
#  Heikki Levanto, Index Data
#
# (C) Copyright 2012-2013 Index Data

# Example opensearch url (split for readability)

#http://opensearch.addi.dk/next_2.2/?
#  action=search&
#  query=hammer&
#  agency=100200&
#  profile=test&
#  start=1&
#  stepValue=3&
#  facets.numberOfTerms=5&
#  facets.facetName=facet.creator&
#  facets.facetName=facet.type

#http://opensearch.addi.dk/next_2.2/?action=search&query=hammer&agency=100200&profile=test&start=1&stepValue=3&facets.numberOfTerms=5&facets.facetName=facet.creator&facets.facetName=facet.type

# A simple way to test the script:
# ./dbc-opensearch-gw.pl -1 &
# zoomsh "open @:9999/default,agency=100200&profile=test" \
#   "find cql:hamlet sortby creator" "show 0" "quit")

use strict;
use warnings;
use utf8;
use Encode;
use URI::Escape;
use Net::Z3950::SimpleServer;
use Net::Z3950::OID;
use Data::Dumper;
use LWP::UserAgent;
use HTTP::Cookies;
use XML::LibXML;
use File::Basename;

my $gwversion = "1.5";

############# Configuration
my $configfilename = "dbc-opensearch-gw.cfg";


# The following can be overwritten in the config file
# It consists of namevalue pairs, separated by a colon
# The names are like the variable names below, without the $
# as in
#    chunksize: 10
# White space and #comments are ignored

my %baseurl;
my %constantparams;
my $fields = {};
my $objectformat = {};
$baseurl{'Default'} = "http://opensearch.addi.dk/2.2/";
$constantparams{'Default'} = "action=search&collectionType=manifestation";
my $chunksize = 10; # initial, grows to max present req.
my $prettyxml = 0;  # 1 for formatting XML a bit, 0 for not
my $debug = 0;
my $test_data = "";
my $op_and = "AND";
my $op_or = "OR";
my $op_not = "NOT";

# Magic value to tell that a term is not to be included in the query
# when it contained sort stuff that has been extracted in the session
my $magic_sort_indicator = " MAGIC SORT INDICATOR ";

############ Config file

sub readconfig {
    my $options = shift;
    my $cfile = $options->{CONFIG};
    # Override config filename with command line value
    if ($cfile ne "default-config") {
	$configfilename = $cfile;
    }
    if (! -r $configfilename) {
	# die if we explicit gave a config file and it isn't present
	die "Error opening configuration file given by -c $configfilename: $!\n" if ($cfile ne "default-config");
        yazlog("WARN: Could not open config file $configfilename. Running with defaults.");
        return;
    }
    yazlog("Reading configuration file $configfilename");
    open(my $F,$configfilename)
        or die "Error opening config file $configfilename: $!\n";
    my $database = "Default";
    my $line = 1;
    while ( <$F> ) {
        chomp();
        s/\#.*$//; # remove comments
        s/\s*$//; # and trailing spaces
        if ( /^ *([^ :]+) *: *(.*)$/ ) {
            yazlog("Config setting $1 : $2") if $debug;
	    if ($1 eq "baseurl") {
		$baseurl{$database} = $2;
	    } elsif ($1 eq "urlpath") {
                die "$configfilename:$line: urlpath not supported anymore. Use baseurl";
            } elsif ($1 eq "constantparams") {
		$constantparams{$database} = $2;
	    } elsif ($1 eq "chunksize") {
		$chunksize = $2;
	    } elsif ($1 eq "prettyxml") {
		$prettyxml = $2;
	    } elsif ($1 eq "debug") {
		$debug = $2;
	    } elsif ($1 eq "test_data") {
		$test_data = $2;
	    } elsif ($1 eq "op_and") {
		$op_and =$2;
	    } elsif ($1 eq "op_or") {
		$op_or  =$2;
	    } elsif ($1 eq "op_not") {
		$op_not =$2;
	    } elsif ($1 eq "objectformat") {
		$objectformat->{$database} = $2;
	    } elsif ($1 eq "fields") {
		my $fname = $2;
		if ($fname !~ /^\// ) {
		    $fname = dirname($configfilename) . "/" . $fname;
		}
		$fields->{$database} = readfields($fname);
            } elsif ($1 eq "database") {
		$constantparams{$2} = $constantparams{$database};
		$baseurl{$2} = $baseurl{$database};
		$database =$2;
	    } else {
		die "$configfilename:$line: Bad directive: $1\n";
	    }
        } elsif (/^$/) {
	    ;
	} else {
	    die "$configfilename:$line: Bad syntax\n";
	}
	$line++;
    }
    # Only log if debugging, as these are displayed before
    # yaz processes the command line, and opens log file from -l
    yazlog("Opensearch gateway $gwversion starting") if ($debug);
    yazlog("Loaded config from $cfile") if ($debug);
}

####### fields
sub readfields {
    my $fname = shift;
    yazlog("Reading fields file $fname");
    open(my $F, $fname) or die "Error open fields file $fname\n";
    my $fr = {};
    while ( <$F> ) {
        chomp();
        s/\#.*$//; # remove comments
        s/\s*$//; # and trailing spaces
 	my @list = split(/\s+/,$_);
	my $cqlfield = $list[0];
	if (defined($cqlfield) && $cqlfield =~ /\./) {
	    if (defined($fr->{$cqlfield})) {
		print "$cqlfield already defined\n" if $debug;
	    } else {
		foreach (@list) {
		    if ( /^([^,]+),([^=]+)=(.*)$/ ) {
			$fr->{$cqlfield}->{$2} = $3;
			my $s = $1;
			if ($s =~ /^\d/ ) {
			    $fr->{$cqlfield}->{set} = $s;
			} elsif ($s =~ /^bib-?1$/i ) {
			    $fr->{$cqlfield}->{set} = '1.2.840.10003.3.1';
			} elsif ($s =~ /^dan-?1$/i ) {
			    $fr->{$cqlfield}->{set} = '1.2.840.10003.3.15';
			} elsif ($s =~ /^dbc-?1$/i ) {
			    $fr->{$cqlfield}->{set} = '1.2.840.10003.3.1000.105.1';
			} else {
			    die "Unknown attribute set $s\n";
			}
		    } elsif ( /^([^=]+)=(.*)$/ ) {
			$fr->{$cqlfield}->{$1} = $2;
		    }
		}
	    }
	}
    }

    if ($debug) {
	print Dumper($fr);
	foreach my $f (keys %{$fr}) {
	    print "f=$f\n";
	    foreach my $s (keys %{$fr->{$f}}) {
		my $x = $fr->{$f}->{$s};
		print " $s=$x\n";
	    }
	}
    }
    return $fr;
}

sub read_test_data {
    my $filename = shift;
    my $start = shift;
    my $chunksize = shift;
    $filename = $filename . "_" . $start . "_" . $chunksize . ".xml";
    yazlog("WARN: fetching test data only: $filename");
    open(F,$filename)
	or die "Error opening test data file $filename: $!\n";
    my $content;
    while ( <F> ) {
	$content .= $_;
    }
    yazlog("Loaded test data $filename");
    return $content;
}

############## Helpers

# Simple logger
sub yazlog {
    my $msg = shift;
    if ($msg) {
        Net::Z3950::SimpleServer::yazlog($msg);
    }
}

# Set the error items in the handle, and return an empty string
# to signal error
sub err {
    my $href = shift;
    my $errno = shift;
    my $errtxt = shift;
    my $logmsg = shift; # optional
    if ( $href ) { # defensive coding
        $href->{ERR_CODE}=$errno;
        $href->{ERR_STR}=$errtxt;
    }
    yazlog("ERROR $errno: $errtxt");
    yazlog($logmsg);
    return "";
}

# Dump a handle, without the full record store
sub dumphandle {
    return unless $debug;
    my $href = shift;
    my $msg = shift;
    yazlog("Dumphandle: " . $msg);
    my $session = $href->{HANDLE};
    my $recs = $session->{records};
    $session->{records} = "<<< records omitted>>>";
    yazlog(Dumper($href));
    $session->{records} = $recs;
}

############## http client

# Fetch a page from the given URL
sub fetchpage {
    my $href = shift;
    my $url = shift;
    my $session = $href->{HANDLE};
    my $ua = new LWP::UserAgent;
    if ( ! $session->{cookies} ) {
      $session->{cookies} = HTTP::Cookies->new( );
      yazlog("Initialized a new cookie jar") if ($debug);
    }
    $ua->cookie_jar( $session->{cookies} );
    my $req = new HTTP::Request GET => $url;
    my $res = $ua->request($req);
    if ( ! $res->is_success ) {
        return err($href, 2, #temporary system error
            "HTTP error from opensearch: ".$res->code. ": " .$res->message,
            "fetching " . $url );
    }
    my $content = $res->content;
    yazlog( "Received " . length($content). " bytes from $url");
    if ( !utf8::valid($content) ) {
        yazlog("The data is NOT VALID utf-8!!");
        # Could return an error here, but probably better to limp along
    }
    # Force Perl to think the content as being utf-8
    # If we get bad utf-8 data, things may fail in strange ways
    # But without this, Perl assumes byte data, and helpfully
    # converts it into utf-8, resulting in double-encoding.
    # See bug 4669.
    Encode::_utf8_on($content);
    # TODO - Check the http content-type header to see if we really got utf-8!

    return $content;
}

# Get number of records from opensearch.
# Detects some simple error codes
# Returns a XPathContext that has the actual document, and some namespaces
# defined. It can be used for finding nodes.
# Or an empty string to indicate errors
sub opensearchclient {
    my $href = shift;
    my $startrec = shift;

    my $session = $href->{HANDLE};
    my $query = $session->{query};
    my $numrecs = $session->{chunksize};
    my $dbname = $session->{dbbase};
    my $extraargs = $session->{dbargs};
    my $sort = $session->{sort};

    my $urlparams =  "?" . $constantparams{$dbname} .  #all after '?'
	"&start=$startrec". "&stepValue=$numrecs";
    if (defined($session->{comp})) {
	$urlparams .= "&objectFormat=" . $session->{comp};
    }
    if ( $sort ) {
	$urlparams .= "&sort=$sort";
    }
    my $burl = $baseurl{$dbname};
    yazlog("initial url parts: $burl $urlparams $query")
        if $debug;
    while ( $extraargs =~ /([^ &=]+)=([^ &]+)&?/g ) {
        my $k = uri_unescape($1);
        my $v = uri_unescape($2);
        yazlog("Looking at extra parameter '$k' = '$v'") if $debug;
        if ( $k eq "host" ) {
            $burl = "http://" . $v. "/";
            yazlog("Replaced host, got baseurl '$burl' ") if $debug;
        } elsif ( $k eq "gwdebug" ) {
            yazlog("Setting debug to $v because of a databasename parameter")
              if ($debug || $v);
            $debug = $v;
        } elsif ( $urlparams =~ s/([?&])($k)=([^ &]+)/$1$k=$v/ ) {
            yazlog("Replaced '$k': was '$3' is now '$v' ") if $debug;
        } else {
            $urlparams .= "&$k=$v";
            yazlog("Appended '$k' = '$v'") if $debug;
        }
    }
    yazlog("dbname: $dbname");
    yazlog("final url parts: $burl $urlparams $query")
        if $debug;
    my $url = $burl . $urlparams . $query;
    yazlog("final url: $url")
        if $debug;

    my $page;
    if (!$test_data) {
	$page = fetchpage($href, $url);
    }
    else {
	$page = read_test_data($test_data, $startrec, $numrecs);
    }

    if (!$page) {
        return;
    }
    my $xmldom;
    eval { $xmldom = XML::LibXML->load_xml(string => $page); };
    if ( $@ ) {
        return err( $href,100, #unspecified error
          "Received bad XML from Opensearch: $@ ",
          substr( $page,0,200 )."...");
    }
    my $xml = XML::LibXML::XPathContext->new($xmldom);
    $xml->registerNs('os', 'http://oss.dbc.dk/ns/opensearch');

    # check error
    my $err = $xml->findvalue('//os:searchResponse/os:error');
    if ($err) {
        return err( $href, 2, #temporary system error
            "Error from Opensearch: " . $err,
             substr( $page,0,400 )."...");
    }
    return $xml;
}

# Extract the hits into the cache in the session
sub get_results {
    my $href = shift;
    my $xml = shift;
    my $session = $href->{HANDLE};
    my $i = 0;
    my $first = 0;
    my $last = 0;
    foreach my $rec ( $xml->findnodes('//os:searchResult') ) {
        my $recno = $xml->findvalue('os:collection/os:resultPosition',$rec) ;
        if ( $recno <= 0 ) {
            return err( $href, 2, #temporary system error
            "Got a bad record from opensearch (no resultPosition)" );
        }
        $first = $recno unless ($first);
        $last = $recno;
        # Clone the node, so we get namespace definitions too
        my $clone = $rec->cloneNode(1);
	my $comp = $session->{comp};
        $session->{records}->{$comp}->[$recno] = $clone->toString($prettyxml);
	yazlog("Doc $recno: " .
            length($session->{records}->{$comp}->[$recno]) . " bytes"  )
          if $debug;
    };
    yazlog("Extracted records $first - $last") if $debug;
}


# extract facets from the xml into the session, in a form that can
# be returned directly in the searchresponse.
sub facets {
    my $href = shift;
    my $xml = shift;
    my $session = $href->{HANDLE};
    my $zfacetlist = [];
    bless $zfacetlist, 'Net::Z3950::FacetList';

    my $i = 0;

    foreach my $facetnode ( $xml->findnodes('//os:facetResult/os:facet') ) {
        #yazlog("Got facet " . $facetnode );
        my $facetname = $xml->findvalue('os:facetName', $facetnode);
        my $zfacetfield = {};
        bless $zfacetfield, 'Net::Z3950::FacetField';
        $zfacetlist->[$i++] = $zfacetfield;
        my $zattributes = [];
        bless $zattributes, 'Net::Z3950::RPN::Attributes';
        $zfacetfield->{'attributes'} = $zattributes;
        my $zattribute = {};
        bless $zattribute, 'Net::Z3950::RPN::Attribute';
        $zattribute->{'attributeType'} = 1;
        $zattribute->{'attributeValue'} = $facetname;
        $zattributes->[0]=$zattribute;
        my $zfacetterms = [];
        bless $zfacetterms, 'Net::Z3950::FacetTerms';
        $zfacetfield->{'terms'} = $zfacetterms;
        my $debugfacets = $facetname . " :";
        my $j = 0;
        foreach my $facetterm ( $xml->findnodes('os:facetTerm',$facetnode) ) {
             # They seem to misspell frequency. Check both, for the case they
             # get around to fixing it.
            my $freq = $xml->findvalue('os:frequence', $facetterm) ||
                       $xml->findvalue('os:frequency', $facetterm);
            my $term = $xml->findvalue('os:term', $facetterm);
            $debugfacets .= " '" . $term . "'=" . $freq;
            my $zfacetterm = {};
            bless $zfacetterm, 'Net::Z3950::FacetTerm';
            $zfacetterm->{'term'} = $term;
            $zfacetterm->{'count'} = $freq;
            $zfacetterms->[$j++] = $zfacetterm;
        }
        yazlog($debugfacets) if ($debug);
    } # facet loop
    if ( $i ) {
        $session->{facets} = $zfacetlist;
    }
    return;
}

# Check that we have the needed records in the cache, fetch if need be
sub getrecords {
    my $href = shift;
    my $start = shift;
    my $num = shift;
    my $session = $href->{HANDLE};

    if (defined($href->{COMP})) {
	$session->{comp} = $href->{COMP};
    } else {
	$session->{comp} = $session->{def_comp};
    }
    yazlog("Checking start=$start, num=$num") if ($debug);
    if ( $num > $session->{chunksize} ) {
        $session->{chunksize} = $num;
    }
    # Skip the records we already have
    my $comp = $session->{comp};
    while ( $num && $session->{records}->{$comp}->[$start] ) {
        $start++;
        $num--;
    }
    if ( $num == 0 && $session->{hits} ) { # we have a hit count and have them all
        yazlog("no need to get more records") if ($debug);
        return; # no need to fetch anything
    }
    my ($xml,$page) = opensearchclient($href, $start);
    if (!$xml) {
        return; # error has been set already
    }
    if ( ! $session->{hits} ){
        my $hits = $xml->findvalue('//os:searchResponse/os:result/os:hitCount');
        if ( length($hits) == 0 ) {  # can't just say !$hits, triggers on "0"
          return err($href, 100, "No hitcount in response");
        }
        $session->{hits} = $hits;
        # Do not attempt to extract facets on zero hits
        if ($hits > 0) {
          facets($href,$xml);
        }
    }
    get_results($href,$xml);
}

# Remove the sortby clause from the CQL query, translate to
# opensearch sort parameter, and put it in the session.
# Handles only one sort key
sub fixsortquery {
    my $href = shift;
    my $qry = shift;
    my $session = $href->{HANDLE};
    my $sortclause = "";
    if ( $qry =~ /^(.*?) +sortby *(\w+)(\/(\w+))?(.*) *$/ ) {
      yazlog("Separated query '$1' from sort clause '$2' '$3' leaving '$5' ") if $debug;
      $qry = $1;
      $sortclause= $2;
      my $direction = $4 || "ascending";
      if ( $5 ) {
        return err($href, 211, "Only one sort key supported" );
      }
      if ( $sortclause ne "random" ) {
        $sortclause .=  "_" . $direction;
      }
    }
    return ( $qry, $sortclause );
}

################# Query translation
sub map_use_attr {
    my $href = shift;
    my $t = shift;
    my $session = $href->{HANDLE};
    my $fr = shift;

    my $dbbase = $session->{dbbase};
    if (!defined($fields->{$dbbase})) {
	return err($href, 3, "No mapping defined for numeric attribtues");
    }
    $fr = $fields->{$dbbase};
    my $a_set = '1.2.840.10003.3.1';
    my $i = 0;
    my $a_u = 1016; # use, type 1
    my $a_r = 3;  # relation, type 2
    my $a_p = -1;  # position, type 3
    my $a_s = -1; # structure, type 4
    my $a_t = 100; # truncation, type 5
    my $a_c = -1; # completeness, type 6
    while (my $attr = $t->{attributes}->[$i++]) {
	my $t = $attr->{attributeType};
        my $v = $attr->{attributeValue};
        if ($t == 1) {
	    $a_u = $v;
	    if (defined($attr->{attributeSet})) {
		$a_set = $attr->{attributeSet};
	    }
	} elsif ($t == 2) {
	    $a_r = $v;
	} elsif ($t == 3) {
	    $a_p = $v;
	} elsif ($t == 4) {
	    $a_s = $v;
	} elsif ($t == 5) {
	    $a_t = $v;
	} elsif ($t == 6) {
	    $a_c = $v;
	} else {
	    return err($href, 113, $t);
	}
    }
    my $best = undef;
    my $use_ok = 0;
    my $relation_ok = 0;
    my $position_ok = 0;
    my $structure_ok = 0;
    my $completeness_ok = 0;
    foreach my $f (keys %{$fr}) {
	my $accept = $f;
	foreach my $s (keys %{$fr->{$f}}) {
	    my $v = $fr->{$f}->{$s};
	    if ($s eq "u") {
		if ($a_u != $v) {
		    $accept = undef;
		} elsif (defined($fr->{$f}->{set}) && $a_set ne $fr->{$f}->{set}) {
		    $accept = undef;
		} else {
		    $use_ok = 1;
		}
	    }
	    if ($s eq "r") {
		if ($v =~ /^\d+?$/) {
		    if ($v != $a_r) {
			$accept = undef;
		    } else {
			$relation_ok = 1;
		    }
		} else {
		    $relation_ok = 1;
		}
	    }
	    if ($s eq "p") {
		if ($a_p != -1 && $v != $a_p) {
		    $accept = undef;
		} else {
		    $position_ok = 1;
		}
	    }
	    if ($s eq "s") {
		if ($a_s == -1) {
		    $structure_ok = 1;
		} elsif ($v =~ /^\d+?$/) {
		    if ($v != $a_s) {
			$accept = undef;
		    } else {
			$structure_ok = 1;
		    }
		} elsif ($v eq "pw") {
		    if ($a_s != 1 && $a_s != 2) {
			$accept = undef;
		    } else {
			$structure_ok = 1;
		    }
		}
	    }
	    if ($s eq "t") {
		if ($v =~ /^\d+?$/) {
		    $accept = undef unless $v == $a_t;
		} else {
		    if ($a_t == 1) {
			$accept = undef unless $v =~ /l/;
		    } elsif ($a_t == 2) {
			$accept = undef unless $v =~ /r/;
		    } else {
			$accept = undef unless $a_t == 100;
		    }
		}
	    }
	    if ($s eq "c" ) {
		if ($a_c != -1 && $v =~ /^\d+?$/ && $v != $a_c) {
		    $accept = undef;
		} else {
		    $completeness_ok = 1;
		}
	    }
	}
	$best = $accept if $accept;
    }
    if (!defined($best)) {
	return err($href, 114, $a_u) unless ($use_ok);
	return err($href, 117, $a_r) unless ($relation_ok);
	return err($href, 119, $a_p) unless ($position_ok);
	return err($href, 118, $a_s) unless ($structure_ok);
	return err($href, 122, $a_c) unless ($completeness_ok);
	return err($href, 123, "");
    }
    return $best;
}

sub q_term {
    my $href = shift;
    my $t = shift;
    my $session = $href->{HANDLE};
    my $field = "";
    my $operator = "=";
    my $sort = "";
    my $quote = "";
    my $rtrunc = "";
    my $ltrunc = "";
    my $term =  $t->{term};
    if ($term eq "") {
	# ### Can not test, simpleServer gets such a bad handle
	return err($href, 108, # malformed query
		   "Empty term not supported" );
    }
    my $i = 0;
    while (my $attr = $t->{attributes}->[$i++])
    {
        #print "Attr: " . Dumper($attr) ;
        my $aval = $attr->{attributeValue};
	my $type = $attr->{attributeType};
        if ($type == 1) {
	    if ($aval =~ /^\d+?$/) { # numeric use
		$field = map_use_attr($href, $t);
		return if ($href->{ERR_CODE});
	    } else {
		$field = $aval;
	    }
        } elsif ($type == 2) {  # Relation
            if ($aval == 1) {
                $operator = "<";
            } elsif ($aval == 2) {
                $operator = "<=";
            } elsif ($aval == 3) {
                $operator = "=";
            } elsif ($aval == 4) {
                $operator = ">=";
            } elsif ($aval == 5) {
                $operator = ">";
            } else {
                return err($href, 117, # unsupp relation
			   $aval, "Unsupported relation $aval");
            }
        } elsif ($type == 3) { # position
            if ($aval < 1 || $aval > 3) {
                return err ($href, 119, # unsupp position
			    $aval, "Unsupported position $aval");
            }
        } elsif ($type == 4) { # structure
            if ($aval == 1) { # phrase
		# Not working, DBC-112
		# $operator = "adj"; 
		$quote = '"';
            } elsif ($aval == 2 || $aval == 4) {  # word / year
		# nothing special to do
            } else {
                return err($href, 118, # unsupp structure
			   $aval, "Unsupported structure $aval");
            }
        } elsif ($type == 5) {  # truncation
            if ($aval == 1) {  # right trunc
                $rtrunc = '*';
	    } elsif ($aval == 2) {
		$ltrunc = '*';
	    } elsif ($aval == 3) {
		$ltrunc = '*';
		$rtrunc = '*';
            } elsif ($aval == 100) {  # none
                ;
            } else {
                return err($href, 120, # unsupp relation
			   $aval, "Unsupported truncation $aval");
            }
        } elsif ($type == 6) {  # completeness
	    ;
        } elsif ($type == 7) { # sort
            if ($aval != 1 && $aval != 2) {
                return err($href, 237, # illegal sort
			   $aval, "Illegal sort (attr 7): $aval");
            }
            $sort = $aval;
        } else {
            return err($href, 113, # unupported attribute type
		       $type,
		       "Unsupported attribute type= " . $type.
		       " val='" . $aval ."'");
        }
    } # attr loop
    if ($sort) {
	if ($session->{sort}) {
	    return err($href, 237, # illegal sort
		       "Only one sort supported");
	}
	my $direction = "_ascending";
	if ($sort == 2) { $direction = "_descending"; }
	if ($field eq "random" ) { $direction = ""; }
	$session->{sort} = $field.$direction;
	return $magic_sort_indicator;
    }
    if (($rtrunc || $ltrunc) && $quote) { # We can not do truncation on phrases
        return err($href, 120, # unsupp trunc
		   "", "Can not do truncation on phrases");
    }
    # Escape characters that would be taken as wildcards
    $term =~ s/([*?^"])/\\$1/g;
    $term = $quote.$ltrunc.$term.$rtrunc.$quote;
    my $clause = $term;
    if ($field) {
	$clause = $field . " " . $operator . " " . $term;
    }
    yazlog("q_term: $clause" ) if ($debug);
    return $clause;
}

sub q_node {
    my $href = shift;
    my $n = shift;
    my $class = ref($n);
    if ( $class eq "Net::Z3950::RPN::Term" ) {
        return q_term($href, $n);
    }
    my %ops = ( "Net::Z3950::RPN::And" => $op_and,
                "Net::Z3950::RPN::Or" => $op_or,
                "Net::Z3950::RPN::AndNot" => $op_not );
    my $op = $ops{$class} ;
    if ( $op ) {
        my $left = q_node($href,$n->[0]);
        return "" unless $left;
        my $right = q_node($href,$n->[1]);
        return "" unless $right;
        return $left if ( $right eq $magic_sort_indicator );
        return $right if ( $left eq $magic_sort_indicator );
        my $clause = "( $left $op $right )";
        yazlog("q_node: $clause") if ($debug);
        return $clause;
    }
    my $opname = $class;
    $opname =~ s/^.*:+//; # Remove the Net::... for error msg
    return err($href,110,  # operator not supported
        $opname,
        "Operator '$class' not supported. Only 'And'");
}


sub q_query {
    my $href = shift;
    my $qry = $href->{RPN};
    my $class = ref($qry);
    yazlog("Translating query") if ($debug);
    if ( $class ne "Net::Z3950::APDU::Query" ) {
        return err($href,100,  # unspecified error
            "Programming error, no query found",
            "Class of query is '$class', not Net::Z3950::APDU::Query" );
    }
    # TODO - check attributeSet
    my $query = q_node($href,$qry->{query});
    yazlog("Translated query: $query" ) if ($debug);
    return $query;
}


################# Request callbacks

sub init_handler {
    my $href = shift;
    my $session = {};
    $session->{chunksize} = $chunksize; # to start with
    $session->{records} = {};
    $href->{HANDLE} = $session;
    dumphandle( $href, "Init:");
}

sub search_handler {
    my $href = shift;
    my $session = $href->{HANDLE};
    dumphandle( $href, "Search:");
    $session->{hits} = 0;
    $session->{facets} = [];
    $session->{records} = {};
    my $db = $href->{DATABASES}[0];
    $session->{dbbase} = $db;
    $session->{dbbase} =~ s/,.*$//;  # without extraargs
    if (! exists $constantparams{$session->{dbbase}}) {
        return err( $href, 235, #Database does not exist
		    $session->{dbbase});
    }
    if (defined($objectformat->{$session->{dbbase}})) {
	$session->{def_comp} = $objectformat->{$session->{dbbase}};
    } else {
	$session->{def_comp} = "dkabm";
    }
    if ($db =~ /.*,(.*)$/ ) {
	$session->{dbargs} = $1;
    } else {
	$session->{dbargs} = "";
    }
    $session->{sort} = '';
    my $qry = $href->{CQL};
    if ( $qry ) {
      my $sortby = "";
      ( $qry, $sortby ) = fixsortquery($href,$qry) ; # Remove CQL sortby clause
      if ( !$qry ) {
        return; # error already set
      $session->{sort} = $sortby;
      }
    } else {
      $qry = q_query($href);
    }
    if ( !$qry ) {
      return; # err is already set
    }
    $session->{query} = "&query=" . uri_escape($qry);
    my $number = $href->{PRESENT_NUMBER};
    #my $number = $session->{chunk_size};
    getrecords($href, 1, $number);
    $href->{HITS} = $session->{hits};
    if ( $session->{facets} ) {
        $href->{OUTPUTFACETS} = $session->{facets};
    }
}

sub present_handler {
}

sub fetch_handler {
    my $href = shift;
    dumphandle( $href, "Fetch:");
    my $offset = $href->{OFFSET};
    my $session = $href->{HANDLE};
    getrecords($href,$offset,1);
    my $comp = $session->{comp};
    my $record = $session->{records}->{$comp}->[$offset];
    if ( !$record ) {
        return err( $href, 13, # present out of range,
            "".$offset );
    }
    $href->{REP_FORM} = Net::Z3950::OID::xml;
    $href->{RECORD} = $record;
    $href->{LEN} = length($record);
    $href->{NUMBER} = $offset;
    $href->{BASENAME} = $session->{dbbase};
}

sub close_handler {
    my $href = shift;
    dumphandle( $href, "Close:");
}


########### Main program

#
my $handler = new Net::Z3950::SimpleServer(START => \&readconfig,
					   INIT => \&init_handler,
                                           CLOSE => \&close_handler,
                                           SEARCH => \&search_handler,
                                           FETCH => \&fetch_handler,
                                           PRESENT => \&present_handler);

$handler->launch_server("opensearch-gw.pl", @ARGV);
