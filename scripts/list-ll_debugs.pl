#!/usr/bin/env perl

# list_ll_debugs.pl v1.10 (c)2012-2023 Henri Beauchamp.
# Released under the GPL (v2 or later, at your convenience) License:
# http://www.gnu.org/copyleft/gpl.html
#
# This script constructs an XML file containing the LLSD representation of the
# contents of a LLScrollListCtrl with two columns ("tag" and "references"),
# based on the usage of the LL_DEBUGS macro in the viewer source tree.
# The XML file is stored in the application settings directory of the viewer,
# ready to be loaded in the debug tags floater.

$basesourcedir	= './indra/';
$settingsdir	= $basesourcedir . 'newview/app_settings/';
$debugtagsfile	= $settingsdir . 'debug_tags.xml';
$filespattern	= '^.*\.cpp\z';
$searchstring1	= '^.*LL_DEBUGS\(\"([^\"]*)\"\).*$';
$searchstring2	= '^.*LL_DEBUGS_ONCE\(\"([^\"]*)\"\).*$';
$searchstring3	= '^.*LL_DEBUGS_SPARSE\(\"([^\"]*)\"\).*$';

unless (-d $settingsdir) {
	die("I cannot find directory \"$settingsdir\". This script must be ran from the root of the source tree.\n");
}

# Search for all *.cpp files in the sources tree
use File::Find();
@files = ();
sub wanted {
	/$filespattern/s && push(@files, $File::Find::name);
}
File::Find::find({wanted => \&wanted}, $basesourcedir);

# Construct an associative array using the LL_DEBUGS(_ONCE) tags as keys and
# the CSV list of the file names containing each tag as values
%tags = ();
foreach $file (@files) {
	open(FILE, $file);
	@lines = <FILE>;
	close(FILE);
    $file =~ s/$basesourcedir//;
	foreach $line (@lines) {
		chomp($line);
       	if ($line =~ /$searchstring1/) {
        	$line =~ s/$searchstring1/\1/;
            unless ($tags{$line} =~ /$file/) {
            	$value = $tags{$line};
                if ($value ne '') {
                	$value = $value . ', ';
                }
            	$tags{$line} = $value . $file;
            }
        }
       	if ($line =~ /$searchstring2/) {
        	$line =~ s/$searchstring2/\1/;
            unless ($tags{$line} =~ /$file/) {
            	$value = $tags{$line};
                if ($value ne '') {
                	$value = $value . ', ';
                }
            	$tags{$line} = $value . $file;
            }
        }
       	if ($line =~ /$searchstring3/) {
        	$line =~ s/$searchstring3/\1/;
            unless ($tags{$line} =~ /$file/) {
            	$value = $tags{$line};
                if ($value ne '') {
                	$value = $value . ', ';
                }
            	$tags{$line} = $value . $file;
            }
        }
	}
}

# Create an XML file holding the LLSD representation of the contents of a
# LLScrollListCtrl with two columns ("tag" and "references"), sorted by tag
# name
open(FILE, ">$debugtagsfile");
print FILE "<llsd>\n";
print FILE "	<array>\n";

$i = 1;
foreach $tag (sort((keys(%tags)))) {
	print FILE "		<map>\n";
	print FILE "		<key>columns</key>\n";
	print FILE "	    	<array>\n";
	print FILE "				<map>\n";
	print FILE "				<key>column</key>\n";
	print FILE "					<string>tag</string>\n";
	print FILE "				<key>value</key>\n";
	print FILE "					<string>$tag</string>\n";
	print FILE "				</map>\n";
	print FILE "				<map>\n";
	print FILE "				<key>column</key>\n";
	print FILE "					<string>references</string>\n";
	print FILE "				<key>value</key>\n";
	print FILE "					<string>$tags{$tag}</string>\n";
	print FILE "				</map>\n";
	print FILE "	    	</array>\n";
	print FILE "		<key>id</key>\n";
	print FILE "			<integer>$i</integer>\n";
	print FILE "		</map>\n";
    $i++;
}

print FILE "	</array>\n";
print FILE "</llsd>\n";
close(FILE);
