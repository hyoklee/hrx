#!/usr/bin/perl -w

# This program generates X.{dmr|dmrpp}.bescmd files for each X.h5 file
# in the input directory.
#
# Author: Hyo-Kyung Lee 
# Customize the following input and output directory.

use File::Basename;

my $input_dir = "./input";
my $output_dir = ".";
my $handle;

sub copy {
    @suffixes = ("dmr", "dmrpp");
    foreach $suffix (@suffixes) {
        $command = "cp ".$_[1]."/X.h5.".$suffix.".bescmd ".$_[1]."/".$_[0].".".$suffix.".bescmd";
        print $command."\n";
        system($command);
    }
}

sub replace {
    $command = "/usr/bin/perl -p -i -e \"s/X/".basename($_[0], ".h5")."/g\" ".$_[1]."/".$_[0].".*";
    print $command."\n";
    system($command);
}

# Open the directory where HDF5 files reside.
opendir($handle, $input_dir) or  die "Could not open $input_dir: $!";
while($filename = readdir($handle)){
# Filter out some files if necessary using the conditional statement
# like below.
    if($filename =~ /.h5/) {
        # Copy the template files with a new file name. 
        copy($filename, $output_dir);
        # Replace the contents of the new files.
	replace($filename, $output_dir);
    }
}
