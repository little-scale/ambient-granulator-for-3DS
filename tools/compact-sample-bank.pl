#!/usr/bin/env perl
use strict;
use warnings;

my ($input, $output) = @ARGV;
die "usage: $0 input-sample-bank.bin output-sample-bank.bin\n"
    unless defined $input && defined $output;

open my $source, '<:raw', $input or die "open $input: $!\n";
my $header = '';
read($source, $header, 64) == 64 or die "short sample-bank header\n";
my ($magic, $version, $capacity, $count, $used, $sample_rate,
    $entry_size, $max_entries, $data_offset) = unpack('a8 V8', $header);
die "not an NDSGRN01 sample bank\n" unless $magic eq 'NDSGRN01';
die "unsupported sample-bank version\n" unless $version == 1;
die "invalid sample-bank bounds\n"
    unless $used >= $data_offset && $used <= $capacity;

seek($source, 0, 0) or die "seek $input: $!\n";
my $data = '';
my $remaining = $used;
while ($remaining > 0) {
    my $chunk = '';
    my $wanted = $remaining > 1024 * 1024 ? 1024 * 1024 : $remaining;
    my $read = read($source, $chunk, $wanted);
    die "short sample-bank data\n" unless defined $read && $read > 0;
    $data .= $chunk;
    $remaining -= $read;
}
close $source;

# The capacity is the only field tied to the DS ROM's fixed reservation.
# Make it equal the used length; entry offsets, PCM, names and CRCs stay intact.
substr($data, 12, 4) = pack('V', $used);
open my $destination, '>:raw', $output or die "open $output: $!\n";
print {$destination} $data;
close $destination;

printf "Compact bank: %d samples, %d Hz, %.1f KiB (removed %.1f MiB padding)\n",
    $count, $sample_rate, $used / 1024, ($capacity - $used) / 1024 / 1024;
